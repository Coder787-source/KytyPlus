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

        // Run bases name a 256-KiB block (bits 49..63 plus the ninth byte, doubled) and the next
        // record gives the offset inside it. These nine-byte records are the tail of a 134 GB PS5
        // package: a run, then three data records (metadata block, then its neighbours).
        {
            var records = new List<byte>();
            // run: start=0xF93F0 (low 15 bits 0x73F0 in bits 49.., high bits 0x1F in byte 9), COffset 0
            ulong runBits = (1UL << 18) | ((ulong)(0xF93F0 & 0x7fff) << 49);
            Span<byte> r = stackalloc byte[9];
            BinaryPrimitives.WriteUInt64LittleEndian(r, runBits); r[8] = (byte)(0xF93F0 >> 15);
            records.AddRange(r.ToArray());
            foreach (ulong coff in new ulong[] { 0x10000, 0x105F5, 0x13D43 })
            {
                BinaryPrimitives.WriteUInt64LittleEndian(r, coff); r[8] = 0x01;
                records.AddRange(r.ToArray());
            }
            var big = (NapsExtractor)RuntimeHelpers.GetUninitializedObject(typeof(NapsExtractor));
            typeof(NapsExtractor).GetField("naps", fields)!.SetValue(big, records.ToArray());
            typeof(NapsExtractor).GetField("cblocks", fields)!.SetValue(big, 4);
            typeof(NapsExtractor).GetField("cblockStart", fields)!.SetValue(big, 0);
            typeof(NapsExtractor).GetField("innerSize", fields)!.SetValue(big, 0x1F281B0000L);
            var disk = typeof(NapsExtractor).GetMethod("DiskOffset", fields)!;
            Check((long)disk.Invoke(big, new object[] { 1 })! == 0x1F27E10000L, "First block after a run starts at the run's 256-KiB block");
            Check((long)disk.Invoke(big, new object[] { 2 })! == 0x1F27E105F5L, "Next block follows the previous block's stored length");
            Check((long)disk.Invoke(big, new object[] { 3 })! == 0x1F27E13D43L, "Offsets accumulate across data records");
            // An unchanged offset between two data records is a whole 256-KiB block stored verbatim.
            BinaryPrimitives.WriteUInt64LittleEndian(r, 0x1000); r[8] = 1;
            var same = new List<byte>(records.Take(9)); same.AddRange(r.ToArray()); same.AddRange(r.ToArray()); same.AddRange(r.ToArray());
            typeof(NapsExtractor).GetField("naps", fields)!.SetValue(big, same.ToArray());
            typeof(NapsExtractor).GetField("cblocks", fields)!.SetValue(big, 4);
            Check((long)disk.Invoke(big, new object[] { 3 })! == 0x1F27E00000L + 0x1000 + 2 * 0x40000, "Zero advance between data records is a stored 256-KiB block");
        }

        GameImageTests.Run();
        Console.WriteLine("PASS: 19-bit file selectors, bare entropy, stored chunks, independent second chunks, truncated input");
        return 0;
    }
}
