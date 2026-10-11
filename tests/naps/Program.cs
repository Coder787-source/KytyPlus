using System.Buffers.Binary;
using System.Reflection;
using System.Runtime.CompilerServices;
using LibProsperoPkg.PFS.Compression.Oodle;

internal static class NapsRegressionTests
{
    static void Check(bool value, string reason)
    {
        if (!value) throw new Exception(reason);
    }

    public static int Main()
    {
        const BindingFlags fields = BindingFlags.Instance | BindingFlags.NonPublic;
        var reader = (NapsExtractor)RuntimeHelpers.GetUninitializedObject(typeof(NapsExtractor));
        var layout = new byte[28];
        // u2c points at the first of two records whose selectors differ only
        // by bit 18. The requested file must select the second record.
        BinaryPrimitives.WriteUInt64LittleEndian(layout.AsSpan(10), 0x10fe8UL << 19);
        BinaryPrimitives.WriteUInt64LittleEndian(layout.AsSpan(19), 0x50fe8UL << 19);
        typeof(NapsExtractor).GetField("naps", fields)!.SetValue(reader, layout);
        typeof(NapsExtractor).GetField("ublocks", fields)!.SetValue(reader, 1);
        typeof(NapsExtractor).GetField("cblocks", fields)!.SetValue(reader, 3);
        typeof(NapsExtractor).GetField("cblockStart", fields)!.SetValue(reader, 10);
        int index = (int)typeof(NapsExtractor).GetMethod("FindFileBlock", fields)!
            .Invoke(reader, new object[] { 0x287f4L })!;
        Check(index == 1, "Files 128 KiB apart must not alias the same NAPS record");

        byte[] expected = "entropy-only"u8.ToArray();
        byte[] entropy = new byte[expected.Length + 3];
        entropy[2] = (byte)expected.Length;
        expected.CopyTo(entropy, 3);
        byte[] output = new byte[expected.Length];
        Check(KrakenDecoder.DecodeBlock(entropy, 0, entropy.Length, output) == KrakenDecodeStatus.Success,
            "Predictor 0 must support bare entropy instead of copying compressed framing");
        Check(output.SequenceEqual(expected), "Entropy output must equal the original bytes");

        Check(KrakenDecoder.DecodeBlock(expected, 0, expected.Length, output) == KrakenDecodeStatus.Success,
            "Equal-size verbatim chunks must still decode");
        Check(output.SequenceEqual(expected), "Stored output must remain unchanged");
        byte[] twoChunks = Enumerable.Repeat((byte)0x41, 0x20000).Concat(entropy).ToArray();
        output = new byte[0x20000 + expected.Length];
        Check(KrakenDecoder.DecodeBlock(twoChunks, 0, 0x20000, output) == KrakenDecodeStatus.Success,
            "Independent bare-entropy second chunk must decode");
        Check(output.AsSpan(0x20000).SequenceEqual(expected), "Second chunk output must be exact");
        Check(KrakenDecoder.DecodeBlock(entropy.AsSpan(0, entropy.Length - 1), 0, entropy.Length - 1,
            new byte[expected.Length]) != KrakenDecodeStatus.Success, "Truncated entropy must be rejected");

        GameImageTests.Run();
        Console.WriteLine("PASS: 19-bit file selectors, bare entropy, stored chunks, independent second chunks, truncated input");
        return 0;
    }
}
