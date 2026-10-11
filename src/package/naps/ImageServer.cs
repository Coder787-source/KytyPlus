// SPDX-License-Identifier: MIT
using System.Text;

internal static class ImageServer
{
    // Protocol: KYTYIMG1, u32 file count, then {u32 UTF-8 path length,
    // path bytes, u64 size}. Requests: u32 file index, u64 offset, u32 length.
    // Response: i32 byte length + bytes, or -1 on failure. Little-endian.
    public static int Serve(string path)
    {
        using var image = new FileGameImage(path);
        var files = ImageExtraction.Tree(image);
        if (files.Count == 0 || files.Count > 1000000 || !files.Any(e => e.Path == "eboot.bin"))
            throw new InvalidDataException("Game image lacks eboot.bin or has an invalid file count");
        var unique = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var e in files)
        {
            foreach (var component in e.Path.Split('/')) ImageExtraction.CheckName(component);
            if (!unique.Add(e.Path)) throw new InvalidDataException("Duplicate case-insensitive image path");
        }
        using var input = new BinaryReader(Console.OpenStandardInput(), Encoding.UTF8, false);
        using var output = new BinaryWriter(Console.OpenStandardOutput(), Encoding.UTF8, false);
        output.Write("KYTYIMG1"u8); output.Write((uint)files.Count);
        foreach (var e in files)
        {
            byte[] name = Encoding.UTF8.GetBytes(e.Path);
            if (name.Length > 16384) throw new InvalidDataException("Image path is too long");
            output.Write((uint)name.Length); output.Write(name); output.Write((ulong)e.Data.Length);
        }
        output.Flush();
        var buffer = new byte[1024 * 1024];
        while (true)
        {
            uint index;
            try { index = input.ReadUInt32(); } catch (EndOfStreamException) { break; }
            ulong offset = input.ReadUInt64(); uint length = input.ReadUInt32();
            if (index >= files.Count || length > buffer.Length || offset > long.MaxValue)
            { output.Write(-1); output.Flush(); return 1; }
            var file = files[(int)index];
            try
            {
                GameImage.Range(file.Data.Length, (long)offset, length);
                file.Data.Read((long)offset, buffer.AsSpan(0, (int)length));
                output.Write((int)length); output.Write(buffer, 0, (int)length); output.Flush();
            }
            catch (Exception ex) when (ex is IOException or InvalidDataException or OverflowException)
            { Console.Error.WriteLine("IMAGE_READ_ERROR=" + ex.Message); output.Write(-1); output.Flush(); return 1; }
        }
        return 0;
    }
}

internal sealed class ExtractionProgress
{
    readonly long total;
    readonly System.Diagnostics.Stopwatch clock = System.Diagnostics.Stopwatch.StartNew();
    long bytes, last;
    public ExtractionProgress(long total) { this.total = total; Report(true); }
    public void Add(long amount) { bytes = checked(bytes + amount); Report(false); }
    public void Finish() { bytes = total; Report(true); }
    void Report(bool force)
    {
        if (!force && clock.ElapsedMilliseconds - last < 200) return;
        last = clock.ElapsedMilliseconds;
        Console.WriteLine($"KYTY_PROGRESS={bytes}/{total}");
        Console.Out.Flush();
    }
}
