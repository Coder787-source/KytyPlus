// SPDX-License-Identifier: MIT
// Format references: https://github.com/videobitva/orbis (PFS/PFSC),
// https://github.com/PSBrew/MkPFS (wrapper), Microsoft exFAT specification.
using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;
using static GameImage;

internal abstract class GameImage
{
    public abstract long Length { get; }
    public abstract void Read(long offset, Span<byte> data);
    public byte[] Bytes(long offset, int length) { var b = new byte[length]; Read(offset, b); return b; }
    public static void Range(long total, long offset, long length)
    {
        if (offset < 0 || length < 0 || offset > total || length > total - offset)
            throw new InvalidDataException("Image range is outside its backing data");
    }
    public static long L64(ReadOnlySpan<byte> b, int p)
    {
        ulong value = BinaryPrimitives.ReadUInt64LittleEndian(b[p..]);
        if (value > long.MaxValue) throw new InvalidDataException("Image size or offset exceeds signed 64-bit limits");
        return (long)value;
    }
    public static uint U32(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt32LittleEndian(b[p..]);
    public static ushort U16(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt16LittleEndian(b[p..]);
    public GameImage Slice(long start, long length) => new SliceImage(this, start, length);
    sealed class SliceImage : GameImage
    {
        readonly GameImage source; readonly long start;
        public override long Length { get; }
        public SliceImage(GameImage source, long start, long length)
        { GameImage.Range(source.Length, start, length); this.source = source; this.start = start; Length = length; }
        public override void Read(long offset, Span<byte> data)
        { GameImage.Range(Length, offset, data.Length); source.Read(checked(start + offset), data); }
    }
}

internal sealed class FileGameImage : GameImage, IDisposable
{
    readonly FileStream file;
    public FileGameImage(string path) => file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
    public override long Length => file.Length;
    public override void Read(long offset, Span<byte> data)
    { GameImage.Range(Length, offset, data.Length); file.Position = offset; file.ReadExactly(data); }
    public void Dispose() => file.Dispose();
}

internal sealed class PfscGameImage : GameImage
{
    readonly GameImage source;
    readonly long[] offsets;
    readonly int blockSize;
    readonly Dictionary<int, byte[]> cache = new();
    readonly Queue<int> order = new();
    public override long Length { get; }
    public PfscGameImage(GameImage source)
    {
        this.source = source;
        var h = source.Bytes(0, 0x30);
        if (!h.AsSpan(0, 4).SequenceEqual("PFSC"u8)) throw new InvalidDataException("Missing PFSC magic");
        blockSize = checked((int)U32(h, 0x0c));
        Length = L64(h, 0x28);
        if (blockSize < 0x1000 || blockSize > 0x100000 || (blockSize & (blockSize - 1)) != 0 ||
            L64(h, 0x10) != blockSize || Length <= 0)
            throw new InvalidDataException("Unsupported PFSC block geometry");
        long count = checked((Length + blockSize - 1) / blockSize);
        if (count > 16000000) throw new InvalidDataException("PFSC offset table exceeds 128 MiB limit");
        long tableStart = L64(h, 0x18), dataStart = L64(h, 0x20);
        GameImage.Range(source.Length, tableStart, checked((count + 1) * 8));
        if (tableStart < 0x30 || dataStart < tableStart + (count + 1) * 8)
            throw new InvalidDataException("PFSC table overlaps data");
        var table = source.Bytes(tableStart, checked((int)((count + 1) * 8)));
        offsets = new long[count + 1];
        for (int i = 0; i < offsets.Length; ++i)
        {
            offsets[i] = L64(table, i * 8);
            if (offsets[i] > source.Length || (i == 0 && offsets[i] != dataStart) ||
                (i > 0 && (offsets[i] <= offsets[i - 1] || offsets[i] - offsets[i - 1] > blockSize)))
                throw new InvalidDataException("Invalid PFSC block offsets");
        }
    }
    byte[] Block(int index)
    {
        if (cache.TryGetValue(index, out var hit)) return hit;
        int span = checked((int)(offsets[index + 1] - offsets[index]));
        var stored = source.Bytes(offsets[index], span);
        byte[] decoded;
        if (span == blockSize) decoded = stored;
        else
        {
            decoded = new byte[blockSize];
            using var input = new MemoryStream(stored, false);
            using var z = new ZLibStream(input, CompressionMode.Decompress);
            z.ReadExactly(decoded);
            if (z.ReadByte() != -1) throw new InvalidDataException("PFSC block inflates past its declared size");
        }
        cache[index] = decoded; order.Enqueue(index);
        if (order.Count > 16) cache.Remove(order.Dequeue());
        return decoded;
    }
    public override void Read(long offset, Span<byte> data)
    {
        GameImage.Range(Length, offset, data.Length);
        while (!data.IsEmpty)
        {
            int block = checked((int)(offset / blockSize)), inside = (int)(offset % blockSize);
            int take = Math.Min(data.Length, blockSize - inside);
            Block(block).AsSpan(inside, take).CopyTo(data); offset += take; data = data[take..];
        }
    }
}

internal sealed record GameImageEntry(string Path, GameImage Data);

internal sealed class PfsGameTree
{
    readonly GameImage image;
    readonly int blockSize, stride, pointerSize, pointerOffset, pointerStride;
    readonly bool signed, decompressFiles;
    readonly Node[] nodes;
    readonly Dictionary<int, List<long>> chains = new();
    readonly record struct Node(ushort Mode, uint Flags, long Size, long LogicalSize, long Blocks, byte[] Record);
    public List<GameImageEntry> Files { get; } = new();
    public PfsGameTree(GameImage image, long superblock = 0, bool decompressFiles = true)
    {
        this.image = image; this.decompressFiles = decompressFiles;
        var h = image.Bytes(superblock, 0x380);
        long version = L64(h, 0);
        if ((version != 1 && version != 2) || L64(h, 8) != 20130315)
            throw new InvalidDataException("Unsupported PFS header");
        int mode = U16(h, 0x1c);
        signed = (mode & 1) != 0;
        bool wide = (mode & 2) != 0;
        if (wide && !signed) throw new InvalidDataException("Unsigned 64-bit PFS inodes are not supported");
        stride = signed ? wide ? 0x310 : 0x2c8 : 0xa8;
        pointerSize = wide ? 8 : 4;
        pointerOffset = wide ? 0x68 : 0x64;
        pointerStride = pointerSize + (signed ? 32 : 0);
        blockSize = checked((int)U32(h, 0x20));
        long count = L64(h, 0x30), tableBlocks = L64(h, 0x40);
        if (blockSize < 0x1000 || blockSize > 0x100000 || (blockSize & (blockSize - 1)) != 0 ||
            count <= 0 || count > 1000000)
            throw new InvalidDataException("Invalid PFS inode geometry");
        int perBlock = blockSize / stride;
        long needed = (count + perBlock - 1) / perBlock;
        if (tableBlocks < needed || checked(needed * blockSize) > 128 * 1024 * 1024)
            throw new InvalidDataException("Invalid or oversized PFS inode table");
        var table = image.Bytes(checked(superblock + blockSize), checked((int)(needed * blockSize)));
        nodes = new Node[count];
        for (int i = 0; i < nodes.Length; ++i)
        {
            var r = table.AsSpan(i / perBlock * blockSize + i % perBlock * stride, stride).ToArray();
            nodes[i] = new Node(U16(r, 0), U32(r, 4), L64(r, 8), L64(r, 0x10),
                wide ? L64(r, 0x60) : U32(r, 0x60), r);
        }
        var root = DirectoryEntries(0);
        int uroot = root.FindIndex(e => e.Name == "uroot" && e.Type == 3);
        var visited = new HashSet<int>();
        Walk(uroot >= 0 ? root[uroot].Inode : 0, "", 0, visited);
    }
    long Pointer(byte[] record, int index) => pointerSize == 8 ? L64(record, index) : U32(record, index);
    List<long> Chain(int inode)
    {
        if (chains.TryGetValue(inode, out var cached)) return cached;
        Node n = nodes[inode];
        if (n.Blocks > image.Length / blockSize || n.Blocks > 16000000)
            throw new InvalidDataException("PFS block chain is too large");
        var result = new List<long>();
        int ptr = pointerOffset + (signed ? 32 : 0);
        long first = Pointer(n.Record, ptr);
        bool sentinel = pointerSize == 8 ? n.Record.AsSpan(ptr + pointerStride, 8).SequenceEqual(new byte[] {255,255,255,255,255,255,255,255}) : Pointer(n.Record, ptr + pointerStride) == uint.MaxValue;
        bool contiguous = n.Blocks <= 1 || sentinel;
        // Signed 64-bit -1 has all bits set and cannot be read as a signed offset.
        if (pointerSize == 8 && n.Record.AsSpan(ptr + pointerStride, 8).SequenceEqual(new byte[] {255,255,255,255,255,255,255,255})) contiguous = true;
        void Add(long block)
        {
            if (block <= 0) throw new InvalidDataException("Invalid PFS data block");
            GameImage.Range(image.Length, checked(block * blockSize), blockSize); result.Add(block);
        }
        if (contiguous)
        {
            GameImage.Range(image.Length, checked(first * blockSize), checked(n.Blocks * blockSize));
            // Avoid allocating a block index per block for huge contiguous files.
            chains[inode] = result; return result;
        }
        for (int i = 0; i < 12 && result.Count < n.Blocks; ++i) Add(Pointer(n.Record, ptr + i * pointerStride));
        var seen = new HashSet<long>();
        void Indirect(long block, int depth)
        {
            if (depth > 5 || !seen.Add(block)) throw new InvalidDataException("Cyclic PFS indirect blocks");
            GameImage.Range(image.Length, checked(block * blockSize), blockSize);
            var data = image.Bytes(block * blockSize, blockSize);
            for (int p = 0; p + pointerStride <= data.Length && result.Count < n.Blocks; p += pointerStride)
            {
                long next = Pointer(data, p + (signed ? 32 : 0));
                if (depth == 1) Add(next); else Indirect(next, depth - 1);
            }
        }
        for (int i = 0; i < 5 && result.Count < n.Blocks; ++i)
            Indirect(Pointer(n.Record, ptr + (12 + i) * pointerStride), i + 1);
        if (result.Count != n.Blocks) throw new InvalidDataException("Truncated PFS block chain");
        chains[inode] = result; return result;
    }
    GameImage NodeData(int inode, bool decompress = true)
    {
        Node n = nodes[inode];
        if (n.Size == 0) return image.Slice(0, 0);
        var blocks = Chain(inode);
        int ptr = pointerOffset + (signed ? 32 : 0);
        GameImage stored = blocks.Count == 0 ? image.Slice(checked(Pointer(n.Record, ptr) * blockSize), n.Size) :
            new BlockChainImage(image, blocks, blockSize, n.Size);
        if ((n.Flags & 1) == 0 || !decompress) return stored;
        var decoded = new PfscGameImage(stored);
        if (n.LogicalSize <= 0 || n.LogicalSize > decoded.Length)
            throw new InvalidDataException("PFS compressed file size is invalid");
        return decoded.Slice(0, n.LogicalSize);
    }
    List<(string Name, int Inode, int Type)> DirectoryEntries(int inode)
    {
        if (inode < 0 || inode >= nodes.Length || (nodes[inode].Mode & 0xf000) != 0x4000 || nodes[inode].Size > 16 * 1024 * 1024)
            throw new InvalidDataException("Invalid PFS directory inode");
        var data = NodeData(inode).Bytes(0, checked((int)nodes[inode].Size));
        var result = new List<(string, int, int)>();
        for (int p = 0; p + 16 <= data.Length;)
        {
            int child = checked((int)U32(data, p)), type = checked((int)U32(data, p + 4));
            int names = checked((int)U32(data, p + 8)), size = checked((int)U32(data, p + 12));
            if (size == 0) break;
            if (size < 16 || size > data.Length - p || names <= 0 || names > size - 16 || child >= nodes.Length)
                throw new InvalidDataException("Invalid PFS directory entry");
            string name = new UTF8Encoding(false, true).GetString(data, p + 16, names).TrimEnd('\0');
            if (type is 2 or 3) { ImageExtraction.CheckName(name); result.Add((name, child, type)); }
            else if (type is not (4 or 5)) throw new InvalidDataException("Unknown PFS directory entry type");
            p += size;
        }
        return result;
    }
    void Walk(int inode, string prefix, int depth, HashSet<int> visited)
    {
        if (depth > 64 || !visited.Add(inode)) throw new InvalidDataException("Cyclic or deep PFS directory tree");
        foreach (var e in DirectoryEntries(inode))
        {
            string path = prefix + e.Name;
            if (e.Type == 3) Walk(e.Inode, path + "/", depth + 1, visited);
            else
            {
                if ((nodes[e.Inode].Mode & 0xf000) != 0x8000) throw new InvalidDataException("PFS file entry references a non-file inode");
                Files.Add(new GameImageEntry(path, decompressFiles ? NodeData(e.Inode) : image.Slice(0, 0)));
                if (Files.Count > 1000000) throw new InvalidDataException("Too many PFS files");
            }
        }
    }
    sealed class BlockChainImage : GameImage
    {
        readonly GameImage source; readonly List<long> blocks; readonly int blockSize;
        public override long Length { get; }
        public BlockChainImage(GameImage source, List<long> blocks, int blockSize, long size)
        { GameImage.Range(checked((long)blocks.Count * blockSize), 0, size); this.source = source; this.blocks = blocks; this.blockSize = blockSize; Length = size; }
        public override void Read(long offset, Span<byte> data)
        {
            GameImage.Range(Length, offset, data.Length);
            while (!data.IsEmpty)
            {
                int inside = (int)(offset % blockSize), take = Math.Min(data.Length, blockSize - inside);
                source.Read(checked(blocks[checked((int)(offset / blockSize))] * blockSize + inside), data[..take]);
                offset += take; data = data[take..];
            }
        }
    }
}

internal sealed class ExfatGameTree
{
    readonly GameImage image;
    readonly long fat, heap, clusterSize;
    readonly uint clusters;
    public List<GameImageEntry> Files { get; } = new();
    public ExfatGameTree(GameImage image)
    {
        this.image = image;
        var h = image.Bytes(0, 512);
        if (!h.AsSpan(3, 8).SequenceEqual("EXFAT   "u8) || h[510] != 0x55 || h[511] != 0xaa)
            throw new InvalidDataException("Missing exFAT boot signature");
        int sectorShift = h[108], clusterShift = h[109];
        if (sectorShift < 9 || sectorShift > 12 || clusterShift + sectorShift > 25 || h[110] != 1)
            throw new InvalidDataException("Unsupported exFAT geometry");
        long sector = 1L << sectorShift;
        fat = checked(U32(h, 80) * sector); heap = checked(U32(h, 88) * sector);
        clusterSize = 1L << (sectorShift + clusterShift); clusters = U32(h, 92);
        GameImage.Range(image.Length, 0, checked(L64(h, 72) * sector));
        GameImage.Range(image.Length, fat, checked(U32(h, 84) * sector));
        GameImage.Range(image.Length, heap, checked(clusters * clusterSize));
        if (clusters == 0 || (long)(clusters + 2UL) * 4 > U32(h, 84) * sector)
            throw new InvalidDataException("Invalid exFAT FAT extent");
        Walk(U32(h, 96), 0, false, "", 0, new HashSet<uint>());
    }
    long ClusterOffset(uint cluster)
    {
        if (cluster < 2 || cluster - 2 >= clusters) throw new InvalidDataException("Invalid exFAT cluster");
        return checked(heap + (cluster - 2) * clusterSize);
    }
    uint Next(uint cluster) => U32(image.Bytes(checked(fat + cluster * 4L), 4), 0);
    List<uint> Chain(uint first, long size, bool contiguous, bool root = false)
    {
        if (size == 0 && !root) return new();
        long needed = root ? clusters : checked((size + clusterSize - 1) / clusterSize);
        if (needed > clusters || (!root && needed > 16000000)) throw new InvalidDataException("exFAT chain too large");
        var result = new List<uint>(); var seen = new HashSet<uint>(); uint c = first;
        while (result.Count < needed)
        {
            ClusterOffset(c);
            if (!seen.Add(c)) throw new InvalidDataException("Cyclic exFAT cluster chain");
            result.Add(c);
            if (root && result.Count * clusterSize > 16 * 1024 * 1024) throw new InvalidDataException("exFAT root directory too large");
            if (contiguous) ++c;
            else
            {
                c = Next(c);
                if (c >= 0xfffffff8)
                {
                    if (root || result.Count == needed) break;
                    throw new InvalidDataException("Truncated exFAT cluster chain");
                }
            }
        }
        return result;
    }
    GameImage Data(uint first, long size, bool contiguous, long valid)
    {
        GameImage.Range(size, 0, valid);
        if (size == 0) return image.Slice(0, 0);
        if (contiguous)
        {
            long needed = checked((size + clusterSize - 1) / clusterSize);
            if (needed > clusters || first < 2 || needed > clusters - (first - 2L)) throw new InvalidDataException("exFAT contiguous extent leaves cluster heap");
            return new ExfatFileImage(image.Slice(ClusterOffset(first), size), valid);
        }
        var chain = Chain(first, size, false);
        return new ExfatFileImage(new ClusterImage(this, chain, size), valid);
    }
    void Walk(uint first, long size, bool contiguous, string prefix, int depth, HashSet<uint> visited)
    {
        if (depth > 64 || !visited.Add(first)) throw new InvalidDataException("Cyclic or deep exFAT directory tree");
        bool root = depth == 0;
        if (size > 16 * 1024 * 1024) throw new InvalidDataException("exFAT directory too large");
        GameImage dir;
        if (root) { var chain = Chain(first, 0, false, true); dir = new ClusterImage(this, chain, chain.Count * clusterSize); }
        else dir = Data(first, size, contiguous, size);
        var bytes = dir.Bytes(0, checked((int)dir.Length));
        for (int p = 0; p + 32 <= bytes.Length;)
        {
            int type = bytes[p]; if (type == 0) break;
            if (type != 0x85)
            {
                if ((type & 0x80) != 0 && type is not (0x81 or 0x82 or 0x83) && (type & 0x20) == 0)
                    throw new InvalidDataException("Unknown critical exFAT directory entry");
                p += 32; continue;
            }
            int secondary = bytes[p + 1], total = checked((secondary + 1) * 32);
            if (secondary < 2 || total > bytes.Length - p || bytes[p + 32] != 0xc0)
                throw new InvalidDataException("Invalid exFAT file entry set");
            ushort checksum = 0;
            for (int i = 0; i < total; ++i) if (i is not (2 or 3)) checksum = (ushort)(((checksum << 15) | (checksum >> 1)) + bytes[p + i]);
            if (checksum != U16(bytes, p + 2)) throw new InvalidDataException("exFAT entry checksum mismatch");
            int length = bytes[p + 35]; var nameBytes = new List<byte>();
            for (int i = 2; i <= secondary; ++i)
            {
                int q = p + i * 32;
                if (bytes[q] != 0xc1) throw new InvalidDataException("Unsupported exFAT secondary entry");
                nameBytes.AddRange(bytes.AsSpan(q + 2, 30).ToArray());
            }
            if (length == 0 || length * 2 > nameBytes.Count) throw new InvalidDataException("Invalid exFAT filename length");
            string name = new UnicodeEncoding(false, false, true).GetString(nameBytes.ToArray(), 0, length * 2);
            ImageExtraction.CheckName(name);
            long dataSize = L64(bytes, p + 56), valid = L64(bytes, p + 40); uint cluster = U32(bytes, p + 52);
            bool noFat = (bytes[p + 33] & 2) != 0;
            if ((U16(bytes, p + 4) & 0x10) != 0) Walk(cluster, dataSize, noFat, prefix + name + "/", depth + 1, visited);
            else Files.Add(new GameImageEntry(prefix + name, Data(cluster, dataSize, noFat, valid)));
            if (Files.Count > 1000000) throw new InvalidDataException("Too many exFAT files");
            p += total;
        }
    }
    sealed class ClusterImage : GameImage
    {
        readonly ExfatGameTree tree; readonly List<uint> chain;
        public override long Length { get; }
        public ClusterImage(ExfatGameTree tree, List<uint> chain, long length) { this.tree = tree; this.chain = chain; Length = length; }
        public override void Read(long offset, Span<byte> data)
        {
            GameImage.Range(Length, offset, data.Length);
            while (!data.IsEmpty)
            {
                long inside = offset % tree.clusterSize; int take = (int)Math.Min(data.Length, tree.clusterSize - inside);
                tree.image.Read(tree.ClusterOffset(chain[checked((int)(offset / tree.clusterSize))]) + inside, data[..take]);
                offset += take; data = data[take..];
            }
        }
    }
    sealed class ExfatFileImage : GameImage
    {
        readonly GameImage source; readonly long valid;
        public override long Length => source.Length;
        public ExfatFileImage(GameImage source, long valid) { this.source = source; this.valid = valid; }
        public override void Read(long offset, Span<byte> data)
        {
            GameImage.Range(Length, offset, data.Length); int take = (int)Math.Clamp(valid - offset, 0, data.Length);
            source.Read(offset, data[..take]); data[take..].Clear();
        }
    }
}

internal static class ImageExtraction
{
    public static void CheckName(string name)
    {
        if (string.IsNullOrEmpty(name) || name is "." or ".." || name.EndsWith('.') || name.EndsWith(' ') ||
            name.IndexOfAny(new[] {'/', '\\', ':', '\0', '<', '>', '"', '|', '?', '*'}) >= 0 || name.Any(c => c < 32))
            throw new InvalidDataException("Unsafe image filename");
        string stem = name.Split('.')[0].ToUpperInvariant();
        if (stem is "CON" or "PRN" or "AUX" or "NUL" ||
            (stem.Length == 4 && (stem.StartsWith("COM") || stem.StartsWith("LPT")) && stem[3] is >= '1' and <= '9'))
            throw new InvalidDataException("Reserved image filename");
    }
    public static List<GameImageEntry> Tree(GameImage image, int depth = 0)
    {
        if (depth > 4) throw new InvalidDataException("Too many nested image layers");
        var h = image.Bytes(0, 16);
        if (h.AsSpan(0, 4).SequenceEqual("PFSC"u8)) return Tree(new PfscGameImage(image), depth + 1);
        if (h.AsSpan(3, 8).SequenceEqual("EXFAT   "u8)) return new ExfatGameTree(image).Files;
        if (GameImage.L64(h, 8) != 20130315) throw new InvalidDataException("Unsupported game image (expected PFS, PFSC or exFAT)");
        var sb = image.Bytes(0, 0x380);
        if ((GameImage.U16(sb, 0x1c) & 4) != 0)
            throw new InvalidDataException("Encrypted standalone image rejected; fake-package keys must come from a verified FPKG");
        var files = new PfsGameTree(image).Files;
        var nested = files.SingleOrDefault(e => e.Path == "pfs_image.dat" || e.Path.EndsWith(".exfat", StringComparison.OrdinalIgnoreCase));
        return nested == null ? files : Tree(nested.Data, depth + 1);
    }
    public static int Extract(List<GameImageEntry> entries, string output, string? onlyFile = null, bool list = false, bool metadataOnly = false)
    {
        if (list) { foreach (var e in entries) Console.WriteLine($"{e.Path}\t{e.Data.Length}"); return 0; }
        var selected = metadataOnly
            ? entries.Where(e => e.Path is "sce_sys/param.json" or "sce_sys/icon0.png" or "sce_sys/pic0.png").ToList()
            : onlyFile == null ? entries : entries.Where(e => e.Path == onlyFile).ToList();
        if (selected.Count == 0) throw new InvalidDataException("Requested file is not present in the image");
        if (metadataOnly)
        {
            if (!selected.Any(e => e.Path == "sce_sys/param.json")) throw new InvalidDataException("Game image lacks param.json");
            if (selected.Any(e => e.Data.Length > (e.Path == "sce_sys/param.json" ? 1024 * 1024 : 16 * 1024 * 1024)))
                throw new InvalidDataException("Game image metadata is too large");
        }
        else if (onlyFile == null && !entries.Any(e => e.Path == "eboot.bin")) throw new InvalidDataException("Game image lacks eboot.bin");
        string root = Path.GetFullPath(output);
        Directory.CreateDirectory(root);
        if ((File.GetAttributes(root) & FileAttributes.ReparsePoint) != 0 || Directory.EnumerateFileSystemEntries(root).Any())
            throw new IOException("Extraction directory must be empty and must not be a link");
        long total = selected.Sum(e => e.Data.Length);
        string? volume = Path.GetPathRoot(root);
        if (volume != null && new DriveInfo(volume).AvailableFreeSpace < total)
            throw new IOException($"Not enough free disk space: extraction needs {total} bytes");
        var progress = new ExtractionProgress(total);
        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase); var buffer = new byte[1024 * 1024]; int count = 0;
        foreach (var e in selected)
        {
            foreach (string component in e.Path.Split('/')) CheckName(component);
            string destination = Path.GetFullPath(Path.Combine(root, e.Path));
            if (!destination.StartsWith(root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) || !paths.Add(destination))
                throw new InvalidDataException("Duplicate or escaping image path");
            for (string? p = Path.GetDirectoryName(destination); p != null; p = Path.GetDirectoryName(p))
                if (Directory.Exists(p) && (File.GetAttributes(p) & FileAttributes.ReparsePoint) != 0)
                    throw new IOException("Extraction destination contains a link");
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            using var file = new FileStream(destination, FileMode.CreateNew, FileAccess.Write);
            for (long offset = 0; offset < e.Data.Length;)
            {
                int take = (int)Math.Min(buffer.Length, e.Data.Length - offset);
                e.Data.Read(offset, buffer.AsSpan(0, take)); file.Write(buffer, 0, take); offset += take; progress.Add(take);
            }
            ++count;
            if (count <= 5 || count % 100 == 0) Console.WriteLine($"IMAGE: extracted {count}/{selected.Count}: {e.Path}");
        }
        progress.Finish();
        Console.WriteLine($"PKG_IMAGE_EXTRACTED={count}");
        return count;
    }
}
