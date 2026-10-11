using System.Buffers.Binary;

internal sealed class NapsExtractor : IDisposable
{
    readonly FileStream pkg;
    readonly XtsGameImage? decrypted;
    readonly FileGameImage backing;
    readonly long imageBase, imageSize, superblock, cntBase;
    readonly byte[] naps;
    readonly int files, ublocks, cblocks, fidxStart, u2cStart, cblockStart;
    readonly long mountSize, metadataBase;
    readonly List<long> boundaries = new();
    readonly Dictionary<int, byte[]> cache = new();
    readonly Queue<int> cacheOrder = new();
    readonly long innerSize;

    static ushort U16(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt16LittleEndian(b[p..]);
    static uint U32(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt32LittleEndian(b[p..]);
    static ulong U64(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt64LittleEndian(b[p..]);
    static uint B32(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt32BigEndian(b[p..]);
    static long CheckedOffset(ulong v) => checked((long)v);
    static void Range(long total, long offset, long length)
    {
        if (offset < 0 || length < 0 || offset > total || length > total - offset)
            throw new InvalidDataException($"Invalid package range: offset={offset:X}, length={length:X}, total={total:X}");
    }
    byte[] Read(long offset, int length)
    {
        Range(pkg.Length, offset, length);
        var b = new byte[length];
        if (decrypted != null && offset >= imageBase && offset - imageBase <= imageSize && length <= imageSize - (offset - imageBase))
            decrypted.Read(offset - imageBase, b);
        else { pkg.Position = offset; pkg.ReadExactly(b); }
        return b;
    }
    byte[] Image(long offset, int length) { Range(imageSize, offset, length); return Read(imageBase + offset, length); }
    byte[] Inner(long offset, int length) { Range(innerSize, offset, length); return Image(offset, length); }

    record struct Block(ulong Bits)
    {
        public bool Run => (Bits & (1UL << 18)) != 0;
        public int COffset => (int)(Bits & 0x3ffff);
        public int UOffset => (int)((Bits >> 19) & 0x7ffff);
        public int EvenLength => (int)((Bits >> 37) & 0x3ffff) / 2 + 1;
        public int Predictor => (int)((Bits >> 56) & 7);
        public long Tweak => (long)((Bits >> 19) & 0xfffffff);
    }
    Block C(int index)
    {
        if (index < 0 || index >= cblocks) throw new InvalidDataException("NAPS block index out of range");
        return new Block(U64(naps, cblockStart + index * 9));
    }
    int U2C(int block)
    {
        if (block < 0 || block >= ublocks) throw new InvalidDataException("NAPS logical block out of range");
        int p = u2cStart + block / 8 * 10;
        int v = naps[p] | naps[p + 1] << 8 | naps[p + 2] << 16;
        if (block % 8 != 0) v += naps[p + 2 + block % 8];
        if (v >= cblocks) throw new InvalidDataException("Invalid NAPS ublock map");
        return v;
    }
    long DiskOffset(int index)
    {
        int r = index;
        while (r >= 0 && !C(r).Run) --r;
        if (r < 0 || r + 1 >= cblocks) throw new InvalidDataException("Missing NAPS run base");
        Block run = C(r), first = C(r + 1), current = C(index);
        long relative = 0;
        for (int i = r + 1; i < index; ++i)
        {
            int advance = (C(i + 1).COffset - C(i).COffset) & 0x3ffff;
            relative += advance == 0 && C(i).Predictor == 4 ? 0x40000 : advance;
        }
        return checked((run.Tweak << 15) + (first.COffset & 0xffff) + relative);
    }

    public NapsExtractor(string path, string? debugPasscode = null)
    {
        backing = new FileGameImage(path);
        pkg = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        byte[] fih = Read(0, 0x100);
        if (!fih.AsSpan(0, 4).SequenceEqual(new byte[] { 0x7f, 70, 73, 72 })) throw new InvalidDataException("Missing FIH header");
        imageBase = CheckedOffset(U64(fih, 0x10)); imageSize = CheckedOffset(U64(fih, 0x18));
        superblock = CheckedOffset(U64(fih, 0x20)); cntBase = CheckedOffset(U64(fih, 0x58));
        Range(pkg.Length, imageBase, imageSize); Range(imageSize, superblock - imageBase, 0x380);
        byte[] sb = Read(superblock, 0x380);
        if (U64(sb, 8) != 20130315 || U32(sb, 0x20) != 0x10000 || U64(sb, 0x30) > 4096)
            throw new InvalidDataException("Unsupported outer PFS geometry");
        if ((U16(sb, 0x1c) & 4) != 0 && !sb.AsSpan(0x370, 16).SequenceEqual("PPRPLAIN-NOAUTH!"u8))
            decrypted = FakePackage.OpenPs5Debug(backing, debugPasscode ?? new string('0', 32));
        // Signed 32-bit outer dinodes are packed in the block after the data-first superblock.
        if ((U16(sb, 0x1c) & 3) != 1) throw new InvalidDataException("Unsupported outer inode variant");
        int count = checked((int)U64(sb, 0x30));
        byte[] table = Read(superblock + 0x10000, checked(count * 0x2c8));
        long nestedLength = 0, layoutOffset = -1; int layoutLength = 0;
        // Locate the outer uroot and read its directory records.
        for (int i = 0; i < count; ++i)
        {
            int p = i * 0x2c8;
            if ((U16(table, p) & 0xf000) != 0x4000) continue;
            long dirSize = CheckedOffset(U64(table, p + 8));
            if (dirSize > 0x100000) throw new InvalidDataException("Outer directory too large");
            byte[] dir = Image((long)U32(table, p + 0x84) * 0x10000, (int)dirSize);
            for (int d = 0; d + 16 <= dir.Length;)
            {
                int inode = checked((int)U32(dir, d)), type = checked((int)U32(dir, d + 4));
                int names = checked((int)U32(dir, d + 8)), size = checked((int)U32(dir, d + 12));
                if (size == 0) break;
                if (size < 16 || names <= 0 || names > size - 16 || d + size > dir.Length || inode >= count)
                    throw new InvalidDataException("Invalid outer directory entry");
                string name = System.Text.Encoding.UTF8.GetString(dir, d + 16, names).TrimEnd('\0');
                int ip = inode * 0x2c8;
                if (type == 2 && name == "pfs_image.dat")
                {
                    if (U32(table, ip + 0x84) != 0) throw new InvalidDataException("Nonzero nested image start is unsupported");
                    nestedLength = CheckedOffset(U64(table, ip + 8));
                }
                if (type == 2 && name == "naps_pkg_layout.dat")
                {
                    layoutOffset = (long)U32(table, ip + 0x84) * 0x10000;
                    layoutLength = checked((int)U64(table, ip + 8));
                }
                d += size;
            }
        }
        if (nestedLength <= 0 || layoutOffset < 0 || layoutLength < 16 || layoutLength > 64 * 1024 * 1024)
            throw new InvalidDataException("Missing nested image or NAPS layout");
        innerSize = nestedLength;
        naps = Image(layoutOffset, layoutLength);
        ulong a = U64(naps, 0), b = U64(naps, 8);
        files = checked((int)(a & 0xffffff) + 1); ublocks = (int)((a >> 32) & 0xffffff);
        cblocks = checked((int)((b >> 24) & 0xffffff) + 2);
        fidxStart = checked(16 + (int)(b & 0xffffff) * 8 + (int)((a >> 28) & 15) * 8);
        // Native NAPS uses numFiles fidx entries and an extra terminal cblock.
        u2cStart = checked(fidxStart + files * 6);
        cblockStart = checked(u2cStart + ((ublocks + 8) >> 3) * 10);
        Range(naps.Length, cblockStart, (long)cblocks * 9);
        for (int i = 0; i < files; ++i)
        {
            int p = fidxStart + i * 6; long v = 0;
            for (int j = 0; j < 5; ++j) v |= (long)naps[p + j] << (j * 8);
            if (v > 0 && v <= (long)ublocks * 0x40000) boundaries.Add(v);
        }
        boundaries = boundaries.Distinct().Order().ToList();
        mountSize = (long)ublocks * 0x40000;
        metadataBase = boundaries.LastOrDefault(v => v < mountSize && (v & 0xffff) == 0);
        if (metadataBase <= 0) throw new InvalidDataException("Missing metadata boundary");
        Console.WriteLine($"NAPS: files={files}, blocks={ublocks}, cblocks={cblocks}, metadata=0x{metadataBase:X}");
    }

    byte[] Decode(int index, int length)
    {
        Block e = C(index), next = C(index + 1);
        int compressed = (next.COffset - e.COffset) & 0x3ffff;
        if (compressed <= 0 || compressed > 0x40000) throw new InvalidDataException($"Invalid NAPS compressed length at entry {index}");
        // The caller knows the exact uncompressed bytes for this file block.
        // UOffset is a doubled logical position, and its delta can also include
        // alignment padding before the next file. It is not a decoded length.
        long diskOffset = DiskOffset(index);
        var source = Inner(diskOffset, compressed);
        if (Environment.GetEnvironmentVariable("KYTY_NAPS_RAWDUMP") is string rd && rd.Length > 0)
            File.WriteAllBytes(System.IO.Path.Combine(rd, $"entry-{index}-raw.bin"), source);
        var full = new byte[length];
        // Each 128-KiB half has its own literal predictor: bit 56 for
        // the even half, bit 59 for the odd half. They are not one mode.
        int trace = Environment.GetEnvironmentVariable("KYTY_NAPS_TRACE") == "1" ? 1 : 0;
        if (trace != 0)
            Console.Error.WriteLine($"entry={index} disk=0x{diskOffset:X} bits=0x{e.Bits:X16} even=0x{e.EvenLength:X} pred={e.Predictor} nextBits=0x{C(index + 1).Bits:X16} comp=0x{compressed:X} head={Convert.ToHexString(source.AsSpan(0, Math.Min(source.Length, 32)))}");

        // The two three-bit predictor groups encode literal mode, newLZ,
        // and restart independently. Do not guess/invert modes: the wrong
        // literal predictor can return Success with corrupted output.
        int flag = e.Predictor | ((int)((e.Bits >> 59) & 7) << 4);
        var status = LibProsperoPkg.PFS.Compression.Oodle.KrakenDecoder.DecodeBlock(source, flag, e.EvenLength, full);
        if (trace != 0) Console.Error.WriteLine($"  flag=0x{flag:X2} result={status}");
        if (status == LibProsperoPkg.PFS.Compression.Oodle.KrakenDecodeStatus.Success)
            return full;
        // A zero-filled block can pass the filesystem probe or silently corrupt
        // an installed game. Reject the package at the first undecodable block.
        throw new InvalidDataException($"NAPS block {index} at image offset 0x{diskOffset:X} could not be decoded (stored=0x{compressed:X}, raw=0x{length:X})");
    }

    byte[] MetadataBlock(int block)
    {
        if (cache.TryGetValue(block, out var found)) return found;
        byte[] decoded = Decode(U2C(block), 0x40000);
        if (Environment.GetEnvironmentVariable("KYTY_NAPS_DUMP") is string dump && dump.Length > 0)
            File.WriteAllBytes(System.IO.Path.Combine(dump, $"naps-{block:X}.bin"), decoded);
        cache[block] = decoded; cacheOrder.Enqueue(block);
        if (cacheOrder.Count > 16) cache.Remove(cacheOrder.Dequeue());
        return decoded;
    }
    byte[] Logical(long offset, int length)
    {
        Range(mountSize, offset, length);
        var output = new byte[length]; int done = 0;
        while (done < length)
        {
            int block = checked((int)(offset / 0x40000)), inside = (int)(offset % 0x40000);
            int take = Math.Min(length - done, 0x40000 - inside);
            MetadataBlock(block).AsSpan(inside, take).CopyTo(output.AsSpan(done));
            offset += take; done += take;
        }
        return output;
    }
    public void Probe()
    {
        byte[] sb = Logical(metadataBase, 0x100);
        Console.WriteLine($"Decoded metadata: {Convert.ToHexString(sb.AsSpan(0, 64))}");
        if (U64(sb, 0) != 2 || U64(sb, 8) != 20130315)
            throw new InvalidDataException("Decoded metadata lacks the PS5 PFS superblock");
    }
    record Node(ushort Mode, long Size, long Offset);
    List<(string Path, long Offset, long Size)> ReadTree()
    {
        byte[] sb = Logical(metadataBase, 0x100);
        int blockSize = checked((int)U32(sb, 0x20));
        int count = checked((int)U64(sb, 0x30));
        if (blockSize != 0x10000 || count <= 0 || count > 1000000)
            throw new InvalidDataException("Invalid inner inode geometry");
        int perBlock = blockSize / 0xa8;
        byte[] table = Logical(metadataBase + blockSize, checked(((count + perBlock - 1) / perBlock) * blockSize));
        var nodes = new Node[count];
        for (int i = 0; i < count; ++i)
        {
            int p = i / perBlock * blockSize + i % perBlock * 0xa8;
            nodes[i] = new Node(U16(table, p), CheckedOffset(U64(table, p + 8)), CheckedOffset(U64(table, p + 0x60)));
        }
        var result = new List<(string, long, long)>(); var seen = new HashSet<int>();
        void Walk(int inode, string prefix, bool user, int depth)
        {
            if (depth > 64 || inode < 0 || inode >= count || !seen.Add(inode))
                throw new InvalidDataException("Invalid or cyclic inner directory tree");
            Node node = nodes[inode];
            if ((node.Mode & 0xf000) != 0x4000 || node.Size < 0 || node.Size > 16 * 1024 * 1024)
                throw new InvalidDataException($"Invalid inner directory inode {inode}: mode={node.Mode:X} size={node.Size} offset={node.Offset:X}");
            byte[] directory = Logical(node.Offset, checked((int)node.Size));
            for (int d = 0; d + 16 <= directory.Length;)
            {
                int child = checked((int)U32(directory, d)); int type = checked((int)U32(directory, d + 4));
                int names = checked((int)U32(directory, d + 8)), size = checked((int)U32(directory, d + 12));
                if (size == 0) break;
                if (size < 16 || names <= 0 || names > size - 16 || size > directory.Length - d || child >= count)
                    throw new InvalidDataException($"Invalid inner directory record: inode={inode} offset={node.Offset:X} record={d:X} child={child} type={type} names={names} size={size}");
                string name = System.Text.Encoding.UTF8.GetString(directory, d + 16, names).TrimEnd('\0');
                if (name != "." && name != "..")
                {
                    if (name.Length == 0 || name.IndexOfAny(new[] { '/', '\\', ':', '\0' }) >= 0)
                        throw new InvalidDataException($"Unsafe package filename in inode {inode}, directory offset 0x{node.Offset:X}, record 0x{d:X}: {Convert.ToHexString(directory.AsSpan(d, Math.Min(size, 128)))}");
                    bool root = !user && name == "uroot";
                    string path = root ? "" : prefix.Length == 0 ? name : prefix + "/" + name;
                    if (type == 3) Walk(child, path, user || root, depth + 1);
                    else if (type == 2 && user) result.Add((path, nodes[child].Offset, nodes[child].Size));
                }
                d += size;
            }
        }
        Walk(0, "", false, 0);
        if (result.Count == 0) throw new InvalidDataException("Inner filesystem contains no application files");
        return result;
    }
    static string SafeDestination(string root, string path)
    {
        string full = System.IO.Path.GetFullPath(System.IO.Path.Combine(root, path));
        string prefix = System.IO.Path.GetFullPath(root).TrimEnd(System.IO.Path.DirectorySeparatorChar) + System.IO.Path.DirectorySeparatorChar;
        if (!full.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Package path escapes output directory");
        // Existing links must not redirect writes outside the extraction tree.
        for (string? p = full; p != null && p.Length >= prefix.Length; p = System.IO.Path.GetDirectoryName(p))
            if ((File.Exists(p) || Directory.Exists(p)) && (File.GetAttributes(p) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Extraction destination contains a symbolic link");
        return full;
    }
    int FindFileBlock(long offset)
    {
        int index = U2C(checked((int)(offset / 0x40000)));
        // UOffset stores a doubled position in a 256-KiB logical block:
        // all 19 bits are required. Dropping bit 18 aliases files 128 KiB
        // apart and selects an earlier file's compressed payload.
        int selector = (int)((offset * 2) & 0x7ffff);
        // u2c can point to an earlier file crossing the logical block boundary.
        for (int i = index; i < cblocks - 1 && i < index + 4096; ++i)
            if (!C(i).Run && C(i).UOffset == selector) return i;
        throw new InvalidDataException($"No NAPS record for logical file offset 0x{offset:X}");
    }
    void WriteFile(string destination, long offset, long size, ExtractionProgress progress)
    {
        Range(mountSize, offset, size);
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(destination)!);
        if (File.Exists(destination)) throw new IOException("Refusing to overwrite existing extraction file: " + destination);
        using var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write);
        if (size == 0) return;
        int index = FindFileBlock(offset); long done = 0;
        while (done < size)
        {
            while (C(index).Run) ++index;
            Block block = C(index); int amount = (int)Math.Min(0x40000, size - done);
            byte[] data;
            // Stored blocks keep the file bytes verbatim. The cblock's COffset
            // span may include alignment padding before the next record, so
            // a span at least as large as the requested file tail is not a
            // Kraken payload even when its predictor bits are set.
            int storedSpan = (C(index + 1).COffset - block.COffset) & 0x3ffff;
            if (storedSpan >= amount || (storedSpan == 0 && amount == 0x40000))
                data = Inner(DiskOffset(index), amount);
            else data = Decode(index, amount);
            output.Write(data); done += amount; ++index; progress.Add(amount);
        }
    }
    int ExtractMetadata(string output)
    {
        byte[] header = Read(cntBase, 0x40);
        int count = checked((int)B32(header, 0x10)); long tableOffset = B32(header, 0x18);
        if (count <= 0 || count > 4096) throw new InvalidDataException("Invalid CNT entry count");
        byte[] table = Read(cntBase + tableOffset, count * 32), names = Array.Empty<byte>();
        for (int i = 0; i < count; ++i)
        {
            int p = i * 32;
            if (B32(table, p) == 0x200)
            {
                int size = checked((int)B32(table, p + 20));
                if (size > 1024 * 1024) throw new InvalidDataException("CNT name table too large");
                names = Read(cntBase + B32(table, p + 16), size);
            }
        }
        int written = 0;
        for (int i = 0; i < count; ++i)
        {
            int p = i * 32; uint id = B32(table, p), nameOffset = B32(table, p + 4);
            if (id < 0x1000 || nameOffset == 0) continue;
            if ((B32(table, p + 8) & 0x80000000) != 0) continue; // encrypted auxiliary entry, not plaintext
            if (nameOffset >= names.Length) throw new InvalidDataException("CNT name offset out of bounds");
            int end = Array.IndexOf(names, (byte)0, (int)nameOffset);
            if (end < 0) throw new InvalidDataException("Unterminated CNT entry name");
            string name = System.Text.Encoding.UTF8.GetString(names, (int)nameOffset, end - (int)nameOffset);
            if (name.Split('/').Any(c => c.Length == 0 || c is "." or ".." || c.IndexOfAny(new[] { ':', '\\' }) >= 0))
                throw new InvalidDataException("Unsafe CNT metadata filename");
            string dest = SafeDestination(output, "sce_sys/" + name);
            if (File.Exists(dest)) continue;
            long size = B32(table, p + 20), offset = cntBase + B32(table, p + 16);
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(dest)!);
            using var target = new FileStream(dest, FileMode.CreateNew, FileAccess.Write);
            while (size > 0) { int amount = (int)Math.Min(size, 0x100000); target.Write(Read(offset, amount)); offset += amount; size -= amount; }
            ++written;
        }
        return written;
    }
    public int Extract(string output, bool listOnly, string? onlyFile = null)
    {
        var entries = ReadTree();
        Console.WriteLine($"NAPS: application tree contains {entries.Count} files");
        if (listOnly)
        {
            foreach (var e in entries) Console.WriteLine($"{e.Path}\t0x{e.Offset:X}\t{e.Size}");
            return 0;
        }
        if (onlyFile != null) entries = entries.Where(e => e.Path == onlyFile).ToList();
        Directory.CreateDirectory(output);
        if (Directory.EnumerateFileSystemEntries(output).Any()) throw new IOException("Extraction directory must be empty");
        if (Environment.GetEnvironmentVariable("KYTY_NAPS_TRACE") == "1")
        {
            Console.WriteLine("NAPS_TREE_DUMP");
            foreach (var e in entries) Console.WriteLine($"{e.Offset}\t{e.Size}\t{e.Path}");
        }
        var progress = new ExtractionProgress(entries.Sum(e => e.Size));
        int extracted = 0;
        foreach (var e in entries)
        {
            try { WriteFile(SafeDestination(output, e.Path), e.Offset, e.Size, progress); }
            catch (Exception ex) { throw new InvalidDataException($"Could not extract '{e.Path}': {ex.Message}", ex); }
            ++extracted;
            if (extracted <= 5 || extracted % 500 == 0) Console.WriteLine($"NAPS: extracted {extracted}/{entries.Count}: {e.Path}");
        }
        progress.Finish();
        if (onlyFile != null) return extracted;
        extracted += ExtractMetadata(output);
        if (!File.Exists(System.IO.Path.Combine(output, "eboot.bin")) || !File.Exists(System.IO.Path.Combine(output, "sce_sys", "param.json")))
            throw new InvalidDataException("Extraction lacks eboot.bin or param.json");
        Console.WriteLine($"PKG_NAPS_EXTRACTED={extracted}");
        return extracted;
    }
    public void Dispose() { pkg.Dispose(); backing.Dispose(); }
    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 2 && args[0] == "--serve-image") return ImageServer.Serve(args[1]);
            if (args.Length == 3 && args[0] == "--image-metadata")
            {
                using var metadataImage = new FileGameImage(args[1]);
                ImageExtraction.Extract(ImageExtraction.Tree(metadataImage), args[2], metadataOnly: true);
                return 0;
            }
            if (args.Length < 2) throw new ArgumentException("Usage: kyty_naps_extractor <pkg|ffpfsc> <output> [--image] [--probe|--list|--file <path>] [--passcode-file <path>]");
            string? only = args.Length > 3 && args[2] == "--file" ? args[3] : null;
            string? passcode = null;
            int passIndex = Array.IndexOf(args, "--passcode-file");
            if (passIndex >= 0)
            {
                if (passIndex + 1 >= args.Length) throw new ArgumentException("Missing passcode file");
                passcode = File.ReadAllText(args[passIndex + 1]).TrimEnd('\r', '\n');
                if (passcode.Length != 32) throw new ArgumentException("Fake-package passcode must be 32 characters");
            }
            using var image = new FileGameImage(args[0]);
            if (args.Contains("--image") || Path.GetExtension(args[0]).Equals(".ffpfsc", StringComparison.OrdinalIgnoreCase))
            {
                var files = ImageExtraction.Tree(image);
                if (args.Contains("--probe")) { Console.WriteLine($"IMAGE: files={files.Count}, bytes={files.Sum(e => e.Data.Length)}"); return 0; }
                ImageExtraction.Extract(files, args[1], only, args.Contains("--list")); return 0;
            }
            var magic = image.Bytes(0, 4);
            if (magic.AsSpan().SequenceEqual(new byte[] {0x7f,67,78,84}))
            {
                var files = new FakePackage(image).Ps4Tree();
                if (!args.Contains("--probe")) ImageExtraction.Extract(files, args[1], only, args.Contains("--list"));
                return 0;
            }
            if (magic.AsSpan().SequenceEqual(new byte[] {0x7f,70,73,72}))
            {
                var fih = image.Bytes(0, 0x100);
                long start = GameImage.L64(fih, 0x10), size = GameImage.L64(fih, 0x18), sbOffset = GameImage.L64(fih, 0x20);
                if (sbOffset == 0 || sbOffset == start)
                {
                    GameImage raw = image.Slice(start, size);
                    var sb = raw.Bytes(0, 0x380);
                    bool encrypted = (GameImage.U16(sb, 0x1c) & 4) != 0 && !sb.AsSpan(0x370, 16).SequenceEqual("PPRPLAIN-NOAUTH!"u8);
                    GameImage plain = encrypted ? FakePackage.OpenPs5Debug(image, passcode ?? new string('0', 32)) : raw;
                    var outer = new PfsGameTree(plain).Files;
                    var nested = outer.SingleOrDefault(e => e.Path == "pfs_image.dat");
                    var files = nested == null ? outer : ImageExtraction.Tree(nested.Data);
                    var metadata = new FakePackage(image, GameImage.L64(fih, 0x58));
                    metadata.AddMetadata(files, ps5: true);
                    Console.WriteLine("PKG_CONTENT_ID=" + metadata.ContentId);
                    if (!args.Contains("--probe")) ImageExtraction.Extract(files, args[1], only, args.Contains("--list"));
                    return 0;
                }
            }
            using var reader = new NapsExtractor(args[0], passcode);
            reader.Probe();
            if (args.Contains("--probe")) return 0;
            reader.Extract(args[1], args.Contains("--list"), args.Length > 3 && args[2] == "--file" ? args[3] : null);
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("PKG_ERROR_REASON=" + ex.Message.Replace('\n', ' ').Replace('\r', ' '));
            if (Environment.GetEnvironmentVariable("KYTY_NAPS_TRACE") == "1") Console.Error.WriteLine(ex);
            return 1;
        }
    }
}
