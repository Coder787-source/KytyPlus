using System.Buffers.Binary;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;

internal static class GameImageTests
{
    const int B = 65536;
    static void U16(byte[] b, int p, ushort v) => BinaryPrimitives.WriteUInt16LittleEndian(b.AsSpan(p), v);
    static void U32(byte[] b, int p, uint v) => BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(p), v);
    static void U64(byte[] b, int p, ulong v) => BinaryPrimitives.WriteUInt64LittleEndian(b.AsSpan(p), v);
    static void BE32(byte[] b, int p, uint v) => BinaryPrimitives.WriteUInt32BigEndian(b.AsSpan(p), v);
    static void BE64(byte[] b, int p, ulong v) => BinaryPrimitives.WriteUInt64BigEndian(b.AsSpan(p), v);
    static GameImage Mem(byte[] b) => new FakePackage.MemoryGameImage(b);
    static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
    static void Reject(Action action, string message)
    { try { action(); } catch (InvalidDataException) { return; } throw new Exception("Should reject: " + message); }
    static int Dent(byte[] b, int p, uint inode, uint type, string name)
    {
        int n = Encoding.UTF8.GetByteCount(name) + 1, size = (16 + n + 7) & ~7;
        U32(b, p, inode); U32(b, p + 4, type); U32(b, p + 8, (uint)n); U32(b, p + 12, (uint)size);
        Encoding.UTF8.GetBytes(name).CopyTo(b, p + 16); return p + size;
    }
    public static byte[] Pfs(byte[] payload, string name = "eboot.bin", bool signed = false, uint version = 2)
    {
        int blocks = (payload.Length + B - 1) / B;
        var b = new byte[(5 + blocks) * B]; U64(b, 0, version); U64(b, 8, 20130315); U16(b, 28, (ushort)(signed ? 9 : 8));
        U32(b, 32, B); U64(b, 40, 1); U64(b, 48, 3); U64(b, 56, (ulong)(3 + blocks)); U64(b, 64, 1);
        void Inode(int i, ushort mode, long size, uint block, uint amount)
        {
            int p = B + i * (signed ? 0x2c8 : 0xa8); U16(b, p, mode); U16(b, p + 2, 1);
            U64(b, p + 8, (ulong)size); U64(b, p + 16, (ulong)size); U32(b, p + 0x60, amount);
            U32(b, p + (signed ? 0x84 : 0x64), block); U32(b, p + (signed ? 0xa8 : 0x68), uint.MaxValue);
        }
        Inode(0, 0x416d, B, 2, 1); Inode(1, 0x416d, B, 3, 1); Inode(2, 0x816d, payload.Length, 5, (uint)blocks);
        int q = Dent(b, 2 * B, 0, 4, "."); q = Dent(b, q, 0, 5, ".."); Dent(b, q, 1, 3, "uroot");
        q = Dent(b, 3 * B, 1, 4, "."); q = Dent(b, q, 0, 5, ".."); Dent(b, q, 2, 2, name);
        payload.CopyTo(b, 5 * B); return b;
    }
    public static byte[] Pfsc(byte[] raw, bool compressed)
    {
        int count = (raw.Length + B - 1) / B; int start = ((0x400 + (count + 1) * 8 + B - 1) / B) * B;
        using var stream = new MemoryStream(); stream.SetLength(start); stream.Position = start;
        var offsets = new long[count + 1];
        for (int i = 0; i < count; ++i)
        {
            var block = new byte[B]; raw.AsSpan(i * B, Math.Min(B, raw.Length - i * B)).CopyTo(block); offsets[i] = stream.Position;
            if (compressed)
            { using var encoded = new MemoryStream(); using (var z = new ZLibStream(encoded, CompressionLevel.SmallestSize, true)) z.Write(block);
                stream.Write(encoded.Length < B ? encoded.ToArray() : block); }
            else stream.Write(block);
        }
        offsets[count] = stream.Position; byte[] result = stream.ToArray(); "PFSC"u8.CopyTo(result);
        U32(result, 8, 6); U32(result, 12, B); U64(result, 16, B); U64(result, 24, 0x400); U64(result, 32, (ulong)start); U64(result, 40, (ulong)count * B);
        for (int i = 0; i <= count; ++i) U64(result, 0x400 + i * 8, (ulong)offsets[i]); return result;
    }
    static byte[] Exfat(byte[] payload)
    {
        var b = new byte[16 * 512]; Encoding.ASCII.GetBytes("EXFAT   ").CopyTo(b, 3); b[510] = 0x55; b[511] = 0xaa;
        U64(b, 72, 16); U32(b, 80, 1); U32(b, 84, 1); U32(b, 88, 2); U32(b, 92, 10); U32(b, 96, 2);
        b[108] = 9; b[109] = 0; b[110] = 1; U32(b, 512 + 2 * 4, uint.MaxValue);
        int p = 1024; b[p] = 0x85; b[p + 1] = 2; b[p + 32] = 0xc0; b[p + 33] = 3; b[p + 35] = 9;
        U64(b, p + 40, (ulong)payload.Length); U32(b, p + 52, 3); U64(b, p + 56, (ulong)payload.Length);
        b[p + 64] = 0xc1; Encoding.Unicode.GetBytes("eboot.bin").CopyTo(b, p + 66);
        ushort sum = 0; for (int i = 0; i < 96; ++i) if (i is not (2 or 3)) sum = (ushort)(((sum << 15) | (sum >> 1)) + b[p + i]); U16(b, p + 2, sum);
        payload.CopyTo(b, 1536); return b;
    }
    static byte[] Cnt(string id, byte[] entryKeys, byte[] imageKey, byte[] image)
    {
        var pkg = new byte[0x10000 + image.Length]; new byte[] {0x7f,67,78,84}.CopyTo(pkg, 0); Encoding.ASCII.GetBytes(id).CopyTo(pkg, 0x40);
        BE32(pkg, 0x10, 3); BE32(pkg, 0x18, 0x600); BE64(pkg, 0x410, 0x10000); BE64(pkg, 0x418, (ulong)image.Length);
        void Entry(int i, uint type, uint offset, uint size, bool crypt = false)
        { int p = 0x600 + i * 32; BE32(pkg, p, type); BE32(pkg, p + 8, crypt ? 0x80000000 : 0); BE32(pkg, p + 12, crypt ? 0x3000u : 0); BE32(pkg, p + 16, offset); BE32(pkg, p + 20, size); }
        Entry(0, 0x10, 0x2000, 2048); Entry(1, 0x20, 0x2800, 256, true); Entry(2, 0x200, 0x3000, 1);
        entryKeys.CopyTo(pkg, 0x2000); imageKey.CopyTo(pkg, 0x2800); image.CopyTo(pkg, 0x10000); return pkg;
    }
    public static void Run()
    {
        byte[] payload = "\x7fELF synthetic game"u8.ToArray();
        var encoded = Pfsc(Pfs(payload), true); var tree = ImageExtraction.Tree(Mem(encoded));
        Check(tree.Count == 1 && tree[0].Data.Bytes(0, payload.Length).SequenceEqual(payload), "PFSC compressed PFS exact payload");
        tree = ImageExtraction.Tree(Mem(Pfsc(Exfat(payload), false)));
        Check(tree[0].Data.Bytes(0, payload.Length).SequenceEqual(payload), "PFSC stored exFAT exact payload");
        var bad = encoded.ToArray(); U64(bad, 0x408, 1); Reject(() => new PfscGameImage(Mem(bad)), "descending PFSC offsets");
        Reject(() => ImageExtraction.Tree(Mem(Pfs(payload, "../bad"))), "PFS traversal");
        bad = Exfat(payload); bad[1024 + 4] ^= 1; Reject(() => ImageExtraction.Tree(Mem(bad)), "exFAT checksum");
        // Wrapped compressed exFAT, matching real .ffpfsc inode flags and sizes.
        var nested = Pfsc(Exfat(payload), true); var wrapped = Pfs(nested, "TEST.exfat");
        U32(wrapped, B + 2 * 0xa8 + 4, 1); U64(wrapped, B + 2 * 0xa8 + 16, B);
        tree = ImageExtraction.Tree(Mem(wrapped)); Check(tree[0].Data.Bytes(0, payload.Length).SequenceEqual(payload), "wrapped FFPFSC");
        byte[] ekpfs = Enumerable.Range(0, 32).Select(i => (byte)i).ToArray(), seed = new byte[16];
        var plain = Pfs(Pfsc(Pfs(payload, version: 1), true), "pfs_image.dat", true, 1); U16(plain, 28, 13);
        byte[] material = new byte[20]; material[0] = 1; seed.CopyTo(material, 4); byte[] pair = HMACSHA256.HashData(ekpfs, material);
        var encrypted = plain.ToArray();
        for (int p = B; p < encrypted.Length; p += 4096) XtsGameImage.Transform(encrypted.AsSpan(p, 4096), pair.AsSpan(0,16), pair.AsSpan(16), (ulong)p / 4096, true);
        using var rsa3 = FakePkgKeys.EntryKey3(); using var fake = FakePkgKeys.ImageKey();
        byte[] dk3 = RandomNumberGenerator.GetBytes(32), keys = new byte[2048]; rsa3.Encrypt(dk3, RSAEncryptionPadding.Pkcs1).CopyTo(keys, 256 + 3 * 256);
        var pkg = Cnt("UP0001-CUSA00000_00-TESTCONTENT00000", keys, new byte[256], encrypted);
        byte[] meta = pkg.AsSpan(0x620,32).ToArray(); byte[] ivKey = SHA256.HashData(meta.Concat(dk3).ToArray());
        using var aes = Aes.Create(); aes.Key = ivKey[16..];
        aes.EncryptCbc(fake.Encrypt(ekpfs, RSAEncryptionPadding.Pkcs1), ivKey.AsSpan(0,16), PaddingMode.None).CopyTo(pkg, 0x2800);
        tree = new FakePackage(Mem(pkg)).Ps4Tree(); Check(tree[0].Data.Bytes(0, payload.Length).SequenceEqual(payload), "encrypted PS4 FPKG round trip");
        pkg[0x2800] ^= 1; Reject(() => new FakePackage(Mem(pkg)).Ps4Tree(), "retail/non-fake image key");
        const string id = "UP0001-PPSA00000_00-TESTCONTENT00000"; Check(id.Length == 36, "test ID length");
        var ps5plain = Pfs(payload); U16(ps5plain, 28, 12);
        byte[] debug = FakePackage.DebugKey(id, new string('0',32), false);
        byte[] ladder = HMACSHA256.HashData(debug, seed); pair = HMACSHA256.HashData(ladder, material);
        encrypted = ps5plain.ToArray();
        for (int p = B; p < encrypted.Length; p += 4096) XtsGameImage.Transform(encrypted.AsSpan(p,4096), pair.AsSpan(0,16), pair.AsSpan(16), (ulong)p / 4096, true);
        var fih = new byte[0x10000 + encrypted.Length + B]; new byte[] {0x7f,70,73,72}.CopyTo(fih,0);
        U64(fih,16,0x10000); U64(fih,24,(ulong)encrypted.Length); U64(fih,32,0x10000); U64(fih,88,(ulong)(0x10000 + encrypted.Length)); encrypted.CopyTo(fih,0x10000);
        int cnt = 0x10000 + encrypted.Length; new byte[] {0x7f,67,78,84}.CopyTo(fih,cnt); Encoding.ASCII.GetBytes(id).CopyTo(fih,cnt+64); BE32(fih,cnt+16,1); BE32(fih,cnt+24,0x100);
        tree = new PfsGameTree(FakePackage.OpenPs5Debug(Mem(fih), new string('0',32))).Files;
        Check(tree[0].Data.Bytes(0,payload.Length).SequenceEqual(payload), "encrypted PS5 debug round trip");
        Reject(() => FakePackage.OpenPs5Debug(Mem(fih), new string('1',32)), "wrong PS5 passcode");
        fih[5] = 0x80; Reject(() => FakePackage.OpenPs5Debug(Mem(fih), new string('0',32)), "PS5 retail gate");
        string output = Path.Combine(Path.GetTempPath(), "kyty-image-test-" + Guid.NewGuid());
        try { ImageExtraction.Extract(ImageExtraction.Tree(Mem(wrapped)), output); Check(File.ReadAllBytes(Path.Combine(output,"eboot.bin")).SequenceEqual(payload), "extraction bytes"); }
        finally { if (Directory.Exists(output)) Directory.Delete(output,true); }
        Console.WriteLine("PASS: PFSC, PFS/exFAT wrappers, unsafe images, PS4 fake crypto, PS5 debug crypto, retail rejection");
    }
}
