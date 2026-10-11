// SPDX-License-Identifier: MIT
// PS4 key-wrap/format reference: https://github.com/videobitva/orbis.
// PS5 debug key schedule and signed-sector format documented by LibProsperoPKG.
using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;

internal sealed class XtsGameImage : GameImage
{
    readonly GameImage source;
    readonly byte[] tweakKey, dataKey;
    readonly int unit;
    readonly long plaintextStart, plaintextSize;
    readonly bool outerPs5;
    readonly HashSet<long> dataBlocks = new();
    readonly Dictionary<long, byte[]> cache = new();
    readonly Queue<long> order = new();
    public override long Length => source.Length;
    public XtsGameImage(GameImage source, byte[] ekpfs, byte[] seed, bool newCrypt, int unit,
                        long plaintextStart, long plaintextSize, bool outerPs5 = false)
    {
        this.source = source; this.unit = unit; this.plaintextStart = plaintextStart;
        this.plaintextSize = plaintextSize; this.outerPs5 = outerPs5;
        byte[] key = newCrypt ? HMACSHA256.HashData(ekpfs, seed) : ekpfs;
        byte[] material = new byte[20]; BinaryPrimitives.WriteUInt32LittleEndian(material, 1); seed.CopyTo(material, 4);
        byte[] pair = HMACSHA256.HashData(key, material);
        tweakKey = pair[..16]; dataKey = pair[16..];
    }
    public void SetDataRange(long start, long end)
    {
        Range(Length, start, end - start);
        for (long b = start / unit; b < (end + unit - 1) / unit; ++b) dataBlocks.Add(b);
        cache.Clear(); order.Clear();
    }
    public static void Transform(Span<byte> data, ReadOnlySpan<byte> tweakKey, ReadOnlySpan<byte> dataKey, ulong sector, bool encrypt)
    {
        if (data.Length == 0 || data.Length % 16 != 0) throw new InvalidDataException("XTS unit must contain full AES blocks");
        using var tweakAes = Aes.Create(); tweakAes.Key = tweakKey.ToArray();
        using var dataAes = Aes.Create(); dataAes.Key = dataKey.ToArray();
        Span<byte> tweak = stackalloc byte[16]; tweak.Clear(); BinaryPrimitives.WriteUInt64LittleEndian(tweak, sector);
        tweakAes.EncryptEcb(tweak, tweak, PaddingMode.None);
        var masks = new byte[data.Length];
        for (int p = 0; p < masks.Length; p += 16)
        {
            tweak.CopyTo(masks.AsSpan(p));
            int carry = 0;
            for (int j = 0; j < 16; ++j) { int next = tweak[j] >> 7; tweak[j] = (byte)((tweak[j] << 1) | carry); carry = next; }
            if (carry != 0) tweak[0] ^= 0x87;
        }
        for (int i = 0; i < data.Length; ++i) data[i] ^= masks[i];
        if (encrypt) dataAes.EncryptEcb(data, data, PaddingMode.None); else dataAes.DecryptEcb(data, data, PaddingMode.None);
        for (int i = 0; i < data.Length; ++i) data[i] ^= masks[i];
    }
    byte[] Block(long index)
    {
        if (cache.TryGetValue(index, out var hit)) return hit;
        long start = checked(index * unit);
        byte[] data = source.Bytes(start, unit);
        if (start < plaintextStart || start >= plaintextStart + plaintextSize)
        {
            ulong sector = (ulong)index;
            if (outerPs5 && !dataBlocks.Contains(index)) sector |= 0x800000000000UL;
            Transform(data, tweakKey, dataKey, sector, false);
        }
        cache[index] = data; order.Enqueue(index);
        if (order.Count > 16) cache.Remove(order.Dequeue());
        return data;
    }
    public override void Read(long offset, Span<byte> data)
    {
        Range(Length, offset, data.Length);
        while (!data.IsEmpty)
        {
            int inside = (int)(offset % unit), take = Math.Min(data.Length, unit - inside);
            Block(offset / unit).AsSpan(inside, take).CopyTo(data); offset += take; data = data[take..];
        }
    }
}

internal sealed class FakePackage
{
    readonly GameImage package;
    readonly long cnt;
    readonly List<byte[]> entries = new();
    byte[]? dk3;
    public string ContentId { get; }
    public FakePackage(GameImage package, long cnt = 0)
    {
        this.package = package; this.cnt = cnt;
        var h = package.Bytes(cnt, 0x100);
        if (!h.AsSpan(0, 4).SequenceEqual(new byte[] {0x7f, 67, 78, 84})) throw new InvalidDataException("Missing CNT header");
        ContentId = Encoding.ASCII.GetString(h, 0x40, 36).TrimEnd('\0');
        int count = checked((int)B32(h, 0x10)); long offset = B32(h, 0x18);
        if (count <= 0 || count > 4096) throw new InvalidDataException("Invalid CNT entry count");
        var table = package.Bytes(checked(cnt + offset), checked(count * 32));
        for (int i = 0; i < count; ++i)
        {
            byte[] e = table.AsSpan(i * 32, 32).ToArray();
            GameImage.Range(package.Length, checked(cnt + B32(e, 16)), B32(e, 20));
            entries.Add(e);
        }
    }
    static uint B32(ReadOnlySpan<byte> b, int p) => BinaryPrimitives.ReadUInt32BigEndian(b[p..]);
    static long B64(ReadOnlySpan<byte> b, int p) => checked((long)BinaryPrimitives.ReadUInt64BigEndian(b[p..]));
    byte[] Find(uint id) => entries.SingleOrDefault(e => B32(e, 0) == id) ?? throw new InvalidDataException("Missing fake-package key entry");
    byte[] EntryData(byte[] e, bool decrypt = true)
    {
        uint size = B32(e, 20); bool encrypted = (B32(e, 8) & 0x80000000) != 0;
        long padded = decrypt && encrypted ? checked((size + 15L) & ~15L) : size;
        if (padded > 64 * 1024 * 1024) throw new InvalidDataException("CNT entry exceeds 64 MiB limit");
        byte[] data = package.Bytes(checked(cnt + B32(e, 16)), checked((int)padded));
        if (!encrypted || !decrypt) return data;
        if (dk3 == null || ((B32(e, 12) >> 12) & 15) != 3)
            throw new InvalidDataException("Unsupported encrypted metadata key index (fake key 3 only)");
        byte[] material = SHA256.HashData(e.Concat(dk3).ToArray());
        using var aes = Aes.Create(); aes.Key = material[16..];
        return aes.DecryptCbc(data, material.AsSpan(0, 16), PaddingMode.None)[..checked((int)size)];
    }
    public byte[] UnwrapPs4FakeKey()
    {
        byte[] keys = EntryData(Find(0x10), false);
        if (keys.Length != 2048) throw new InvalidDataException("Invalid PS4 entry-key table");
        try
        {
            using var key3 = FakePkgKeys.EntryKey3();
            dk3 = key3.Decrypt(keys.AsSpan(256 + 3 * 256, 256).ToArray(), RSAEncryptionPadding.Pkcs1);
            if (dk3.Length != 32) throw new CryptographicException("Invalid DK3 size");
            using var fake = FakePkgKeys.ImageKey();
            byte[] result = fake.Decrypt(EntryData(Find(0x20)), RSAEncryptionPadding.Pkcs1);
            if (result.Length != 32) throw new CryptographicException("Invalid fake EKPFS size");
            // The fake-key RSA unwrap is the gate; never accept caller-supplied retail keys.
            return result;
        }
        catch (CryptographicException ex) { throw new InvalidDataException("Not a supported PS4 FPKG: fake image-key unwrap failed; retail decryption is disabled", ex); }
    }
    public List<GameImageEntry> Ps4Tree()
    {
        byte[] key = UnwrapPs4FakeKey();
        var h = package.Bytes(0, 0x440);
        long start = B64(h, 0x410), size = B64(h, 0x418);
        GameImage raw = package.Slice(start, size);
        byte[] sb = raw.Bytes(0, 0x380);
        if (GameImage.L64(sb, 0) != 1) throw new InvalidDataException("PS4 package contains a non-PS4 filesystem");
        int block = checked((int)GameImage.U32(sb, 0x20));
        if (block < 0x1000 || block > 0x100000) throw new InvalidDataException("Invalid PS4 block size");
        GameImage plain = (GameImage.U16(sb, 0x1c) & 4) != 0 ?
            new XtsGameImage(raw, key, sb[0x370..0x380], (BinaryPrimitives.ReadUInt64BigEndian(h.AsSpan(0x408)) & 0x2000000000000000UL) != 0, 0x1000, 0, block) : raw;
        var outer = new PfsGameTree(plain).Files;
        var nested = outer.SingleOrDefault(e => e.Path == "pfs_image.dat");
        var files = nested == null ? outer : ImageExtraction.Tree(nested.Data);
        AddMetadata(files);
        Console.WriteLine("PKG_CONTENT_ID=" + ContentId);
        Console.WriteLine("FPKG: PS4 fake image key verified");
        return files;
    }
    public void AddMetadata(List<GameImageEntry> files, bool ps5 = false)
    {
        var names = EntryData(Find(0x200));
        foreach (var e in entries)
        {
            uint id = B32(e, 0), name = B32(e, 4);
            if (id < 0x1000) continue;
            string path;
            if (id == 0x1000) path = ps5 ? "sce_sys/param.json" : "sce_sys/param.sfo";
            else
            {
                if (name == 0 || name >= names.Length) continue;
                int end = Array.IndexOf(names, (byte)0, checked((int)name));
                if (end < 0) throw new InvalidDataException("Unterminated CNT filename");
                path = "sce_sys/" + new UTF8Encoding(false, true).GetString(names, (int)name, end - (int)name);
            }
            foreach (var c in path.Split('/')) ImageExtraction.CheckName(c);
            if (files.Any(f => f.Path == path)) continue;
            files.Add(new GameImageEntry(path, new MemoryGameImage(EntryData(e))));
        }
    }
    public static byte[] DebugKey(string contentId, string passcode, bool sha3)
    {
        if (contentId.Length != 36 || passcode.Length != 32 || contentId.Any(c => c > 127) || passcode.Any(c => c > 127))
            throw new InvalidDataException("Debug key derivation requires a 36-character content ID and a 32-character ASCII passcode");
        byte[] Hash(byte[] data) => sha3 ? SHA3_256.HashData(data) : SHA256.HashData(data);
        byte[] material = new byte[96];
        Hash(new byte[] {0,0,0,1}).CopyTo(material, 0);
        Hash(Encoding.ASCII.GetBytes(contentId.PadRight(48, '\0'))).CopyTo(material, 32);
        Encoding.ASCII.GetBytes(passcode).CopyTo(material, 64);
        return Hash(material);
    }
    public static XtsGameImage OpenPs5Debug(GameImage package, string passcode)
    {
        var h = package.Bytes(0, 0x100);
        if (!h.AsSpan(0, 4).SequenceEqual(new byte[] {0x7f,70,73,72}) || h[5] != 0)
            throw new InvalidDataException("Encrypted retail/submitted PS5 packages are disabled; only debug FPKGs are supported");
        long start = GameImage.L64(h, 0x10), size = GameImage.L64(h, 0x18), sbAbs = GameImage.L64(h, 0x20);
        if (sbAbs == 0) sbAbs = start;
        long sbRel = checked(sbAbs - start);
        var raw = package.Slice(start, size); var sb = raw.Bytes(sbRel, 0x380);
        int block = checked((int)GameImage.U32(sb, 0x20));
        if (GameImage.L64(sb, 0) != 2 || GameImage.L64(sb, 8) != 20130315 || block != 0x10000 || sbRel % block != 0 || size % block != 0)
            throw new InvalidDataException("Unsupported PS5 debug PFS geometry");
        var metadata = new FakePackage(package, GameImage.L64(h, 0x58));
        var seed = sb[0x370..0x380];
        foreach (bool sha3 in new[] {false, true})
        {
            if (sha3 && !SHA3_256.IsSupported) continue;
            var key = DebugKey(metadata.ContentId, passcode, sha3);
            foreach (bool newCrypt in new[] {true, false})
            {
                var decoded = new XtsGameImage(raw, key, seed, newCrypt, sbRel == 0 ? 0x1000 : block, sbRel, block, sbRel != 0);
                try
                {
                    var tree = new PfsGameTree(decoded, sbRel, decompressFiles: false).Files;
                    if (tree.Count == 0) throw new InvalidDataException("Empty debug filesystem");
                    if (sbRel != 0)
                    {
                        // Data-first: pfs_image.dat starts at image block zero; the
                        // following signed NAPS file bounds the physical data extent.
                        var naps = tree.SingleOrDefault(e => e.Path == "naps_pkg_layout.dat");
                        if (naps == null) throw new InvalidDataException("Missing debug NAPS layout");
                        // Locate the NAPS bytes by inode block pointer, not logical mount size.
                        long dataEnd = LocateNapsOffset(decoded, sbRel, block);
                        decoded.SetDataRange(0, dataEnd);
                    }
                    Console.WriteLine("FPKG: PS5 debug key validated against the filesystem");
                    return decoded;
                }
                catch (InvalidDataException) { /* Try the other documented debug schedule. */ }
            }
        }
        throw new InvalidDataException("PS5 debug FPKG could not be decrypted with this passcode; retail keys are never tried");
    }
    static long LocateNapsOffset(GameImage image, long sb, int block)
    {
        var h = image.Bytes(sb, 0x380); int count = checked((int)GameImage.L64(h, 0x30));
        var table = image.Bytes(sb + block, checked(count * 0x2c8));
        long end = sb;
        for (int i = 0; i < count; ++i)
        {
            var r = table.AsSpan(i * 0x2c8, 0x2c8);
            if ((GameImage.U16(r, 0) & 0xf000) == 0x8000)
            { long start = GameImage.U32(r, 0x84) * (long)block; if (start > 0 && start < end) end = start; }
        }
        return end;
    }
    internal sealed class MemoryGameImage : GameImage
    {
        readonly byte[] data; public override long Length => data.Length;
        public MemoryGameImage(byte[] data) => this.data = data;
        public override void Read(long offset, Span<byte> target) { Range(Length, offset, target.Length); data.AsSpan(checked((int)offset), target.Length).CopyTo(target); }
    }
}
