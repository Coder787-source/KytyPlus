// this code is fully vibe ported/coded from the oodle c++ sdk
// huge shoutouts to claude opus 4.5

// ReSharper disable InconsistentNaming
// ReSharper disable NotAccessedVariable
// ReSharper disable RedundantStringInterpolation
// ReSharper disable RedundantAssignment
// ReSharper disable UnusedMember.Local
#pragma warning disable IDE0090
#pragma warning disable IDE0004

using System.Buffers.Binary;
using System.Diagnostics;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Intrinsics.X86;

namespace OodleSharp;

file static unsafe class H // static Helpers class
{
    [Conditional("DEBUG_OODLE_LOGGING")]
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void LogOodle(string message)
    {
        Console.WriteLine($"[Oodle] {message}");
    }

    // Fast copy helpers - use Span.CopyTo which is SIMD optimized internally
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void CopyBytes_SIMD(byte* dst, byte* src, int count)
    {
        // For small counts, use direct 64-bit copies to avoid Span overhead
        if (count <= 32)
        {
            if (count >= 16)
            {
                *(ulong*)dst = *(ulong*)src;
                *(ulong*)(dst + 8) = *(ulong*)(src + 8);
                if (count > 16)
                {
                    *(ulong*)(dst + count - 16) = *(ulong*)(src + count - 16);
                    *(ulong*)(dst + count - 8) = *(ulong*)(src + count - 8);
                }
            }
            else if (count >= 8)
            {
                *(ulong*)dst = *(ulong*)src;
                *(ulong*)(dst + count - 8) = *(ulong*)(src + count - 8);
            }
            else if (count >= 4)
            {
                *(uint*)dst = *(uint*)src;
                *(uint*)(dst + count - 4) = *(uint*)(src + count - 4);
            }
            else if (count > 0)
            {
                dst[0] = src[0];
                if (count >= 2) *(ushort*)(dst + count - 2) = *(ushort*)(src + count - 2);
            }
        }
        else
        {
            // Large non-overlapping copy; let the runtime use its optimized memcpy
            Buffer.MemoryCopy(src, dst, count, count);
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void CopyMatch_SIMD(byte* dst, byte* src, int count, int offset)
    {
        // If no overlap, defer to the raw copy path
        if (offset >= count)
        {
            CopyBytes_SIMD(dst, src, count);
            return;
        }

        // Fast path: large offsets can use 16-byte SIMD copies safely
        if (offset >= 16 && Sse2.IsSupported)
        {
            while (count >= 16)
            {
                var v = Sse2.LoadVector128((byte*)src);
                Sse2.Store((byte*)dst, v);
                dst += 16; src += 16; count -= 16;
            }
            while (count >= 8)
            {
                *(ulong*)dst = *(ulong*)src;
                dst += 8; src += 8; count -= 8;
            }
            if (count > 0)
            {
                if (count >= 4)
                {
                    *(uint*)dst = *(uint*)src;
                    *(uint*)(dst + count - 4) = *(uint*)(src + count - 4);
                }
                else
                {
                    *dst = *src;
                    if (count >= 2) *(ushort*)(dst + count - 2) = *(ushort*)(src + count - 2);
                }
            }
            return;
        }

        // Hot path: offset >= 8 (minimum Oodle offset) - most common case
        if (offset >= 8)
        {
            // Unroll first 32 bytes (4 iterations) without branch
            if (count >= 32)
            {
                *(ulong*)dst = *(ulong*)src;
                *(ulong*)(dst + 8) = *(ulong*)(src + 8);
                *(ulong*)(dst + 16) = *(ulong*)(src + 16);
                *(ulong*)(dst + 24) = *(ulong*)(src + 24);
                dst += 32; src += 32; count -= 32;
            }
            
            // 8-byte copies for remaining
            while (count >= 8)
            {
                *(ulong*)dst = *(ulong*)src;
                dst += 8; src += 8; count -= 8;
            }
            // Tail - overlapping writes for 1-7 bytes
            if (count > 0)
            {
                if (count >= 4)
                {
                    *(uint*)dst = *(uint*)src;
                    *(uint*)(dst + count - 4) = *(uint*)(src + count - 4);
                }
                else
                {
                    *dst = *src;
                    if (count >= 2) *(ushort*)(dst + count - 2) = *(ushort*)(src + count - 2);
                }
            }
        }
        else if (offset == 1)
        {
            // RLE pattern: fill with single byte value
            byte val = *src;
            // Broadcast to 8-byte value for fast fill
            ulong val8 = val * 0x0101010101010101UL;
            while (count >= 8)
            {
                *(ulong*)dst = val8;
                dst += 8; count -= 8;
            }
            // Tail
            while (count-- > 0) *dst++ = val;
        }
        else
        {
            // Small offset 2-7 - chunked copy with proper overlap handling
            // Copy 'offset' bytes at a time to handle overlap
            while (count >= offset)
            {
                // Safe to copy 'offset' bytes - they've all been written
                if (offset >= 4)
                {
                    *(uint*)dst = *(uint*)src;
                    if (offset > 4) { dst[4] = src[4]; if (offset > 5) { dst[5] = src[5]; if (offset > 6) dst[6] = src[6]; } }
                }
                else if (offset == 2)
                {
                    *(ushort*)dst = *(ushort*)src;
                }
                else // offset == 3
                {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                }
                dst += offset; src += offset; count -= offset;
            }
            // Remaining bytes (0 to offset-1)
            while (count-- > 0) *dst++ = *src++;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void CopySub_SIMD(byte* dst, byte* literals, byte* match, int count)
    {
        // SUB mode: dst[i] = literals[i] + match[i] (byte addition with wrap)
        // IMPORTANT: match may point to dst + neg_offset (where neg_offset is negative)
        // This creates an overlap situation. The minimum offset is 8, so we can safely
        // copy 8 bytes at a time without reading bytes we just wrote.
        const ulong mask = 0x7F7F7F7F7F7F7F7FUL;

        // If SSE2 is available and match is far enough, use 16-byte vector adds
        if (Sse2.IsSupported)
        {
            int offset = (int)(dst - match);
            if (offset >= 16)
            {
                while (count >= 16)
                {
                    var litv = Sse2.LoadVector128((byte*)literals);
                    var matv = Sse2.LoadVector128((byte*)match);
                    var sum = Sse2.Add(litv, matv); // wrap-around byte add
                    Sse2.Store((byte*)dst, sum);
                    literals += 16; match += 16; dst += 16; count -= 16;
                }
            }
        }

        // Process 8 bytes at a time using SWAR parallel byte addition
        while (count >= 8)
        {
            ulong lit8 = *(ulong*)literals;
            ulong match8 = *(ulong*)match;
            // SWAR byte addition: add bytes in parallel without carry propagation
            *(ulong*)dst = ((lit8 & mask) + (match8 & mask)) ^ ((lit8 ^ match8) & ~mask);
            literals += 8; match += 8; dst += 8; count -= 8;
        }

        // Handle remaining bytes (0-7) - unrolled
        if (count >= 4)
        {
            dst[0] = (byte)(literals[0] + match[0]);
            dst[1] = (byte)(literals[1] + match[1]);
            dst[2] = (byte)(literals[2] + match[2]);
            dst[3] = (byte)(literals[3] + match[3]);
            literals += 4; match += 4; dst += 4; count -= 4;
        }
        while (count-- > 0)
        {
            *dst++ = (byte)(*literals++ + *match++);
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void FillBytes(byte* dst, byte value, int count)
    {
        Unsafe.InitBlockUnaligned(dst, value, (uint)count);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static uint Bswap32(uint x) => BinaryPrimitives.ReverseEndianness(x);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong Bswap64(ulong x) => BinaryPrimitives.ReverseEndianness(x);

    // Read big-endian uint32 from unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static uint ReadU32BE(byte* p) => Bswap32(*(uint*)p);

    // Read little-endian uint32 from unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static uint ReadU32LE(byte* p) => *(uint*)p;

    // Read big-endian uint64 from unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong ReadU64BE(byte* p) => Bswap64(*(ulong*)p);

    // Read little-endian uint64 from unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong ReadU64LE(byte* p) => *(ulong*)p;

    // Read little-endian uint16 from unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ushort ReadU16LE(byte* p) => *(ushort*)p;

    // Read big-endian uint16 from unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ushort ReadU16BE(byte* p) => BinaryPrimitives.ReverseEndianness(*(ushort*)p);

    // Read big-endian 24-bit value from unaligned pointer (returns uint32)
    // Read 32 bits, swap endianness, shift right 8 to get upper 24 bits
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static uint ReadU24BE(byte* p) => Bswap32(*(uint*)p) >> 8;

    // Read little-endian 24-bit value from unaligned pointer (returns uint32)
    // Just mask off the upper byte after reading 32 bits
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static uint ReadU24LE(byte* p) => *(uint*)p & 0xFFFFFF;

    // Write little-endian uint32 to unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void WriteU32LE(byte* p, uint value) => *(uint*)p = value;

    // Write little-endian uint64 to unaligned pointer
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void WriteU64LE(byte* p, ulong value) => *(ulong*)p = value;

    // Write 5 bytes from separate uint values (packed as little-endian)
    // Writes: b0, b1, b2, b3, b4 to p[0..4]
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Write5BytesLE(byte* p, uint b0, uint b1, uint b2, uint b3, uint b4)
    {
        // Pack 4 bytes into one uint write
        uint packed4 = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        *(uint*)p = packed4;
        p[4] = (byte)b4;
    }
}

[SkipLocalsInit]
public static unsafe partial class OodleDecompressor
{
    // Thread-local scratch memory pool to avoid allocation overhead per decompression
    [ThreadStatic]
    private static byte* _scratchPool;

    private const int SCRATCH_POOL_SIZE = 3 * 1024 * 1024; // 3MB

    static OodleDecompressor()
    {
        for (int i = 0; i < 256; i++)
        {
            int numraw = i >> OFFSET_ALT_NUM_UNDER_BITS;
            if (numraw > 26) // 0xd8..0xff are illegal
            {
                newlz_offsetsalt_bias_tab[i] = -NEWLZ_MAX_OFFSET;
            }
            else
            {
                int val = OFFSET_ALT_BIAS - (((i & ((1 << OFFSET_ALT_NUM_UNDER_BITS) - 1)) | (1 << OFFSET_ALT_NUM_UNDER_BITS)) << numraw);
                newlz_offsetsalt_bias_tab[i] = val;
            }
        }

        for (int i = 0; i < 256; i++)
        {
            int numraw = i >> OFFSET_ALT_NUM_UNDER_BITS;
            if (numraw > 26) // 0xd8..0xff are illegal
            {
                newlz_offsetsalt_bias_tab[i] = -NEWLZ_MAX_OFFSET;
            }
            else
            {
                int val = OFFSET_ALT_BIAS - (((i & ((1 << OFFSET_ALT_NUM_UNDER_BITS) - 1)) | (1 << OFFSET_ALT_NUM_UNDER_BITS)) << numraw);
                newlz_offsetsalt_bias_tab[i] = val;
            }
        }

        for (int i = 0; i < 256; i++)
        {
            if (i < 0xF0)
            {
                int numRaw = (i >> 4) + OFFSET_RAW_BITS;
                newlz_offsets44_count_tab[i] = (byte)numRaw;
                newlz_offsets44_bias_tab[i] = -((i & 0xF) + (1 << (numRaw + 4)) - (1 << (OFFSET_RAW_BITS + 4)) + NEWLZ_MIN_OFFSET);
            }
            else
            {
                int numRaw = i - 0xF0 + 16;
                newlz_offsets44_count_tab[i] = (byte)numRaw;
                newlz_offsets44_bias_tab[i] = -(ESCAPE_OFFSET_BIAS + (1 << numRaw));
            }
        }
        newlz_offsets44_bias_tab[0xFE] = -NEWLZ_MIN_OFFSET;
        newlz_offsets44_bias_tab[0xFF] = -NEWLZ_MIN_OFFSET;
    }

    // Enums
    private enum OodleLZ_Compressor
    {
        OodleLZ_Compressor_Invalid = -1,
        OodleLZ_Compressor_None = 3,
        OodleLZ_Compressor_Kraken = 8,
        OodleLZ_Compressor_Leviathan = 13,
        OodleLZ_Compressor_Mermaid = 9,
        OodleLZ_Compressor_Selkie = 11,
        OodleLZ_Compressor_Hydra = 12,
        OodleLZ_Compressor_BitKnit = 10,
        OodleLZ_Compressor_LZB16 = 4,
        OodleLZ_Compressor_LZNA = 7,
        OodleLZ_Compressor_LZH = 0,
        OodleLZ_Compressor_LZHLW = 1,
        OodleLZ_Compressor_LZNIB = 2,
        OodleLZ_Compressor_LZBLW = 5,
        OodleLZ_Compressor_LZA = 6,
        OodleLZ_Compressor_Count = 14,
        OodleLZ_Compressor_Force32 = 0x40000000
    }

    // Constants
    private const int OODLELZ_BLOCK_LEN = 1 << 18; // 256KB
    private const int OODLELZ_QUANTUM_LEN = 1 << 14; // 16KB
    private const int OODLELZ_BLOCK_HEADER_BYTES_MAX = 2 + 1;
    private const int OODLELZ_QUANTUM_HEADER_MAX_SIZE = 4;
    private const int RAD_LZ_HEADER_VERSION = 4;

    private const int NEWLZ_ARRAY_INTERNAL_MAX_SCRATCH = 48 * 1024;
    private const int NEWLZ_MULTIARRAYS_FORMAT_MAX_NUM_ARRAYS = 63;

    // Decode Types
    private const int RAD_LZ_DECODE_LZHLW = 0;
    private const int RAD_LZ_DECODE_LZNIB = 1;
    private const int RAD_LZ_DECODE_LZB16 = 2;
    private const int RAD_LZ_DECODE_LZBLW = 3;
    private const int RAD_LZ_DECODE_LZA = 4;
    private const int RAD_LZ_DECODE_LZNA = 5;
    private const int RAD_LZ_DECODE_KRAKEN = 6;
    private const int RAD_LZ_DECODE_LZH = 7;
    private const int RAD_LZ_DECODE_MERMAID = 10;
    private const int RAD_LZ_DECODE_BITKNIT = 11;
    private const int RAD_LZ_DECODE_LEVIATHAN = 12;
    private const int RAD_LZ_DECODE_COUNT = 13;

    private const int OFFSET_RAW_BITS = 4;
    private const int NEWLZ_MIN_OFFSET = 8;
    private const int NEWLZ_MAX_OFFSET = (1 << 30);
    private const int ESCAPE_OFFSET_BIAS = (((1 << 23) - 256) - (1 << 16));
    private const int OFFSET_ALT_NUM_UNDER_BITS = 3;
    private const int OFFSET_ALT_BIAS = (1 << OFFSET_ALT_NUM_UNDER_BITS);

    private static readonly int[] newlz_offsets44_bias_tab = new int[256];
    private static readonly int[] newlz_offsetsalt_bias_tab = new int[256];
    private static readonly byte[] newlz_offsets44_count_tab = new byte[256];

    // Structs
    [StructLayout(LayoutKind.Sequential)]
    private struct LZBlockHeader
    {
        public int version;
        public int decodeType;
        public int offsetShift;
        public bool chunkIsMemcpy;
        public bool chunkIsReset;
        public bool chunkHasQuantumCRCs;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LZQuantumHeader
    {
        public int compLen;
        public uint crc;
        public bool wholeMatchFlag;
        public long wholeMatchOffset;
        public bool huffFlag;
        public bool extraFlag;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct newLZ_chunk_arrays
    {
        public byte* chunk_ptr;
        public byte* scratch_ptr;
        public int* offsets;
        public long offsets_count;
        public uint* excesses;
        public long excesses_count;
        public byte* packets;
        public long packets_count;
        public byte* literals_ptr;
        public long literals_count;
    }

    private const int NEWLZ_HUFF_CODELEN_LIMIT = 11;
    private const int NEWLZ_HUFF_DECODE_TABLE_SIZE = 2048;

    private const int NEWLZF_CHUNK_LEN = 128 * 1024;
    private const int NEWLZF_MIN_CHUNK_LEN = 128;
    private const int NEWLZF_OFFSET_FOURBYTE_SHIFT = 22;
    private const int NEWLZF_OFFSET_FOURBYTE_THRESHOLD = (1 << 24) - (1 << NEWLZF_OFFSET_FOURBYTE_SHIFT);
    private const int NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT = 8;
    private const int NEWLZF_LRL_EXCESS = 64;
    private const int NEWLZF_ML_EXCESS = 91;
    private const int NEWLZF_OFF24_MML_DECODE = 8;

    [StructLayout(LayoutKind.Sequential)]
    private struct newLZF_chunk_arrays
    {
        public byte* chunk_ptr;
        public byte* scratch_ptr;
        public byte* excesses_ptr;
        public byte* excesses_end;
        public byte* packets_ptr;
        public byte* packets_end;
        public long packets_count;
        public long packets_count1;
        public byte* literals_ptr;
        public byte* literals_end;
        public byte* off16_ptr;
        public byte* off16_end;
        public uint* escape_offsets1;
        public uint* escape_offsets2;
        public long escape_offsets_count1;
        public long escape_offsets_count2;
        public uint* escape_offsets_ptr;
        public uint* escape_offsets_end;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KrakenMSBHuffTab
    {
        public fixed byte len[NEWLZ_HUFF_DECODE_TABLE_SIZE + 16];
        public fixed byte sym[NEWLZ_HUFF_DECODE_TABLE_SIZE + 16];
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KrakenHuffTab
    {
        public fixed ushort e[NEWLZ_HUFF_DECODE_TABLE_SIZE];
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct rrVarBits
    {
        public byte* m_cur;
        public byte* m_end;
        public ulong m_bits;
        public int m_inv_bitlen;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BlockBitReader
    {
        public byte* ptr;
        public byte* end;
        public uint pos_in_byte;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KrakenHuffState
    {
        public fixed ulong decodeptr[2];
        public fixed ulong decodeend[2];
        public fixed ulong strm0_end[2];
        public fixed ulong bitp[6];
        public fixed uint bits[6];
        public fixed uint bitc[6];
        public KrakenHuffTab table;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KrakenOffsetState
    {
        public byte* offs_u8;
        public byte* offs_u8_end;
        public int* neg_offs_s32;
        public fixed ulong bitp[2];
        public fixed uint bitc[2];
    }

    // rrVarBits helpers
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void rrVarBits_GetOpen(ref rrVarBits vb, byte* ptr, byte* end)
    {
        vb.m_cur = ptr;
        vb.m_end = end;
        vb.m_bits = 0;
        vb.m_inv_bitlen = 63;
        rrVarBits_Refill_Safe(ref vb);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void rrVarBits_GetOpenBack(ref rrVarBits vb, byte* ptr, byte* limit)
    {
        vb.m_cur = ptr;
        vb.m_end = limit;
        vb.m_bits = 0;
        vb.m_inv_bitlen = 63;
        rrVarBits_RefillBack_Safe(ref vb);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void rrVarBits_Refill_Safe(ref rrVarBits vb)
    {
        int bitlen = 63 - vb.m_inv_bitlen;
        while (bitlen <= 56 && vb.m_cur < vb.m_end)
        {
            vb.m_bits |= (ulong)(*vb.m_cur++) << (56 - bitlen);
            bitlen += 8;
            vb.m_inv_bitlen -= 8;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_Get1(ref rrVarBits vb)
    {
        if (vb.m_inv_bitlen > 63) rrVarBits_Refill_Safe(ref vb);
        uint ret = (uint)(vb.m_bits >> 63);
        vb.m_bits <<= 1;
        vb.m_inv_bitlen += 1;
        return ret;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_Get_C(ref rrVarBits vb, int count)
    {
        if (vb.m_inv_bitlen > (63 - count)) rrVarBits_Refill_Safe(ref vb);
        uint ret = (uint)(vb.m_bits >> (64 - count));
        vb.m_bits <<= count;
        vb.m_inv_bitlen += count;
        return ret;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void rrVarBits_Use(ref rrVarBits vb, int count)
    {
        vb.m_bits <<= count;
        vb.m_inv_bitlen += count;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_Get_V(ref rrVarBits vb, uint count)
    {
        return rrVarBits_Get_C(ref vb, (int)count);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_Get_0Ok(ref rrVarBits vb, int count)
    {
        if (count == 0) return 0;
        return rrVarBits_Get_C(ref vb, count);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_CountLeadingZeros(ref rrVarBits vb)
    {
        return (uint)BitOperations.LeadingZeroCount(vb.m_bits);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_Peek(ref rrVarBits vb, int count)
    {
        return (uint)(vb.m_bits >> (64 - count));
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static ulong rrVarBits_GetSizeBytes(ref rrVarBits vb, byte* start)
    {
        int bitlen = 63 - vb.m_inv_bitlen;
        return (ulong)(vb.m_cur - start - (bitlen >> 3));
    }

    private static ReadOnlySpan<byte> tab_data => [
        0x00, 0x20, 0x10, 0x30, 0x08, 0x28, 0x18, 0x38,
        0x04, 0x24, 0x14, 0x34, 0x0c, 0x2c, 0x1c, 0x3c,
        0x02, 0x22, 0x12, 0x32, 0x0a, 0x2a, 0x1a, 0x3a,
        0x06, 0x26, 0x16, 0x36, 0x0e, 0x2e, 0x1e, 0x3e,
        0x01, 0x21, 0x11, 0x31, 0x09, 0x29, 0x19, 0x39,
        0x05, 0x25, 0x15, 0x35, 0x0d, 0x2d, 0x1d, 0x3d,
        0x03, 0x23, 0x13, 0x33, 0x0b, 0x2b, 0x1b, 0x3b,
        0x07, 0x27, 0x17, 0x37, 0x0f, 0x2f, 0x1f, 0x3f
    ];

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint newlz_huff_bitreverse(uint x)
    {
        return (uint)((tab_data[(int)(x & 0x3f)] << 5) | tab_data[(int)(x >> 5)]);
    }

    private static uint newlz_decode_packhuff8_runlen(ref rrVarBits vb)
    {
        if (rrVarBits_Peek(ref vb, 8) == 0) return 511;
        uint clz = rrVarBits_CountLeadingZeros(ref vb);
        uint nbits = clz * 2 + 1 + 1;
        return rrVarBits_Get_V(ref vb, nbits) - 1;
    }

    private static int newlz_decode_hufflens(ref rrVarBits vb, byte* symLists, uint* lastSymOfLen)
    {
        int gotNumSyms = -1;

        if (rrVarBits_Get1(ref vb) != 0)
        {
            const uint numSymbols = 256;
            int riceBits = (int)rrVarBits_Get_C(ref vb, 2);

            uint maxCodelenDeltaFolded = (NEWLZ_HUFF_CODELEN_LIMIT - 1) * 2;
            uint maxUnaryPrefixVal = maxCodelenDeltaFolded >> riceBits;
            uint riceLenBias = (uint)riceBits + 1;

            int prevCodeLen4 = 8 * 4;
            uint i = 0;

            rrVarBits_Refill_Safe(ref vb);
            gotNumSyms = 0;

            if (rrVarBits_Get1(ref vb) == 0)
                i = newlz_decode_packhuff8_runlen(ref vb);

            while (i < numSymbols)
            {
                rrVarBits_Refill_Safe(ref vb);
                uint nzRunLen = newlz_decode_packhuff8_runlen(ref vb);
                if (i + nzRunLen > numSymbols) return -1;

                rrVarBits_Refill_Safe(ref vb);
                gotNumSyms += (int)nzRunLen;

                do
                {
                    uint clz = rrVarBits_CountLeadingZeros(ref vb);
                    if (clz > maxUnaryPrefixVal) return -1;

                    uint nbits = clz + riceLenBias;
                    uint riceTail = rrVarBits_Get_V(ref vb, nbits);
                    uint code = ((clz - 1) << riceBits) + riceTail;

                    int delta;
                    if ((code & 1) != 0) delta = -((int)(code + 1) >> 1);
                    else delta = (int)(code >> 1);

                    int pred = (prevCodeLen4 + 2) >> 2;
                    int curCodeLen = delta + pred;
                    if (curCodeLen < 1 || curCodeLen > NEWLZ_HUFF_CODELEN_LIMIT) return -1;

                    prevCodeLen4 = (((prevCodeLen4) * 3 + 2) >> 2) + curCodeLen;

                    uint count = lastSymOfLen[curCodeLen];
                    rrVarBits_Refill_Safe(ref vb);

                    symLists[count] = (byte)i;
                    lastSymOfLen[curCodeLen] = count + 1;

                    i++;
                } while (--nzRunLen > 0);

                if (i >= numSymbols) break;

                uint zRunLen = newlz_decode_packhuff8_runlen(ref vb);
                i += zRunLen;
            }

            if (i != numSymbols || gotNumSyms < 2) gotNumSyms = -1;
        }
        else
        {
            gotNumSyms = (int)rrVarBits_Get_C(ref vb, 8);
            if (gotNumSyms == 0) return -1;
            else if (gotNumSyms == 1)
            {
                byte sym = (byte)rrVarBits_Get_C(ref vb, 8);
                symLists[0] = sym;
            }
            else
            {
                int log2CodeLen = (int)rrVarBits_Get_C(ref vb, 3);
                if (log2CodeLen > 4) return -1;

                for (int i = 0; i < gotNumSyms; i++)
                {
                    rrVarBits_Refill_Safe(ref vb);
                    int sym = (int)rrVarBits_Get_V(ref vb, 8);
                    int len = (int)rrVarBits_Get_0Ok(ref vb, log2CodeLen);
                    int curCodeLen = len + 1;
                    if (curCodeLen > NEWLZ_HUFF_CODELEN_LIMIT) return -1;

                    uint count = lastSymOfLen[curCodeLen];
                    symLists[count] = (byte)sym;
                    lastSymOfLen[curCodeLen] = count + 1;
                }
            }
        }

        return gotNumSyms;
    }

    private static int newLZ_decode_alphabet_shape_num_EG(int num_syms, ref rrVarBits vb)
    {
        int num_eg = 0;
        if (num_syms != 256)
        {
            int num_eg_bound = Math.Min(num_syms, 257 - num_syms) * 2;

            int nbits = 32 - BitOperations.LeadingZeroCount((uint)(num_eg_bound - 1));
            int large_thresh = (1 << nbits) - num_eg_bound;
            int peek = (int)rrVarBits_Peek(ref vb, nbits);
            if ((peek >> 1) < large_thresh)
            {
                num_eg = peek >> 1;
                rrVarBits_Get_V(ref vb, (uint)(nbits - 1));
            }
            else
            {
                num_eg = peek - large_thresh;
                rrVarBits_Get_V(ref vb, (uint)nbits);
            }
        }
        return num_eg;
    }

    private static int newLZ_decode_unary_block(byte* unary, int count, ref BlockBitReader br)
    {
        if (count <= 0) return count;

        byte* bits_ptr = br.ptr;
        byte* bits_end = br.end;
        uint bits_pos = br.pos_in_byte;

        byte* unary_cur = unary;
        byte* unary_end = unary + count;

        *unary_cur = 0;
        while (unary_cur < unary_end)
        {
            if (bits_ptr >= bits_end) return -1;

            int bit = (*bits_ptr >> (int)(7 - bits_pos)) & 1;
            bits_pos++;
            if (bits_pos == 8)
            {
                bits_pos = 0;
                bits_ptr++;
            }

            if (bit == 0)
            {
                (*unary_cur)++;
            }
            else
            {
                unary_cur++;
                if (unary_cur < unary_end) *unary_cur = 0;
            }
        }

        br.ptr = bits_ptr;
        br.pos_in_byte = bits_pos;
        return count;
    }

    private static int newLZ_decode_rice_U8_bottom_block(byte* codes, int count, int riceK, ref BlockBitReader br)
    {
        if (riceK < 0 || riceK > 3) return -1;
        if (riceK == 0) return count;

        byte* bits_ptr = br.ptr;
        byte* bits_end = br.end;
        uint bits_pos = br.pos_in_byte;

        int num_uni_bits = count * riceK;
        int num_uni_bytes = (int)((bits_pos + num_uni_bits + 7) >> 3);
        if (bits_ptr + num_uni_bytes > bits_end) return -1;

        for (int i = 0; i < count; i++)
        {
            uint val = 0;
            for (int k = 0; k < riceK; k++)
            {
                int bit = (*bits_ptr >> (int)(7 - bits_pos)) & 1;
                val = (val << 1) | (uint)bit;
                bits_pos++;
                if (bits_pos == 8)
                {
                    bits_pos = 0;
                    bits_ptr++;
                }
            }

            codes[i] = (byte)(((uint)codes[i] << riceK) | val);
        }

        br.ptr = bits_ptr;
        br.pos_in_byte = bits_pos;
        return count;
    }

    private static int newlz_decode_hufflens2(ref rrVarBits vb, byte* symLists, uint* lastSymOfLen)
    {
        byte* vb_start = vb.m_cur;
        int riceBits = (int)rrVarBits_Get_C(ref vb, 2);
        int gotNumSyms = (int)rrVarBits_Get_C(ref vb, 8) + 1;
        if (gotNumSyms < 2) return -1;

        int numEG = newLZ_decode_alphabet_shape_num_EG(gotNumSyms, ref vb);

        BlockBitReader bbr;
        int len_in = 63 - vb.m_inv_bitlen;
        bbr.ptr = vb.m_cur - ((len_in + 7) >> 3);
        bbr.end = vb.m_end;
        bbr.pos_in_byte = (uint)((0 - len_in) & 7);

        int numUnary = gotNumSyms + numEG;
        byte* unary = stackalloc byte[527];

        if (newLZ_decode_unary_block(unary, numUnary, ref bbr) != numUnary) return -1;

        Unsafe.InitBlockUnaligned(unary + numUnary, 0, 16);

        if (newLZ_decode_rice_U8_bottom_block(unary, gotNumSyms, riceBits, ref bbr) != gotNumSyms) return -1;

        rrVarBits_GetOpen(ref vb, bbr.ptr, bbr.end);
        if (bbr.pos_in_byte > 0) rrVarBits_Use(ref vb, (int)bbr.pos_in_byte);

        // Store codelens in temp array (in-place conversion from delta to codelen)
        byte* codelens = stackalloc byte[256];
        int prevCodeLen4 = 8 * 4 + 2;
        for (int i = 0; i < gotNumSyms; i++)
        {
            uint code = unary[i];
            int delta;
            if ((code & 1) != 0) delta = -((int)(code + 1) >> 1);
            else delta = (int)(code >> 1);

            int curCodeLen = (prevCodeLen4 >> 2) + delta;
            if (curCodeLen <= 0 || curCodeLen > NEWLZ_HUFF_CODELEN_LIMIT) return -1;

            prevCodeLen4 += delta;
            codelens[i] = (byte)curCodeLen;
        }

        // Decode alphabet shape run lengths
        ushort* runLens = stackalloc ushort[128 * 2 + 8 + 1];
        int numRunPairs = newLZ_decode_alphabet_shape_runlens(runLens, (uint)gotNumSyms, (uint)numEG, unary + gotNumSyms, ref vb);
        if (numRunPairs < 0) return -1;

        // Process runs to build symbol list
        int lenIdx = 0;
        for (int pair = 0; pair < numRunPairs; pair++)
        {
            uint curSymbol = runLens[pair * 2 + 0];
            uint runLen = runLens[pair * 2 + 1];

            do
            {
                if (lenIdx >= gotNumSyms) return -1;
                byte curCodeLen = codelens[lenIdx++];

                uint count = lastSymOfLen[curCodeLen];
                symLists[count] = (byte)curSymbol;
                lastSymOfLen[curCodeLen] = count + 1;
                curSymbol++;
            } while (--runLen > 0);
        }

        return gotNumSyms;
    }

    private static uint decode_alphabet_shape_zerorun(uint prefix, ref rrVarBits vb)
    {
        if (prefix > 7)
            return 511;
        else
        {
            uint nextra = prefix + 1;
            uint code = (1u << (int)nextra) + rrVarBits_Get_V(ref vb, nextra);
            return (code - 2) + 1;
        }
    }

    private static uint decode_alphabet_shape_nonzerorun(uint prefix, ref rrVarBits vb)
    {
        if (prefix > 8)
            return 511;
        else
        {
            uint code = (1u << (int)prefix) + rrVarBits_Get_0Ok(ref vb, (int)prefix);
            return (code - 1) + 1;
        }
    }

    private static int newLZ_decode_alphabet_shape_runlens(ushort* runLens, uint numNonzeroSyms, uint numEG, byte* runPrefix, ref rrVarBits vb)
    {
        if (numNonzeroSyms == 0 || numNonzeroSyms > 256) return -1;
        if (numEG > 255) return -1;

        int numRunPairs = (int)(numEG >> 1);

        uint curSymbol = 0;
        if ((numEG & 1) != 0) // initial zero run present
        {
            rrVarBits_Refill_Safe(ref vb);
            curSymbol = (ushort)decode_alphabet_shape_zerorun(runPrefix[0], ref vb);
            runPrefix++;
        }

        // Decode run len codes
        for (int pair = 0; pair < numRunPairs; pair++)
        {
            rrVarBits_Refill_Safe(ref vb);
            runLens[pair * 2 + 1] = (ushort)decode_alphabet_shape_nonzerorun(runPrefix[0], ref vb);
            runLens[pair * 2 + 2] = (ushort)decode_alphabet_shape_zerorun(runPrefix[1], ref vb);
            runPrefix += 2;
        }

        // Perform summing of zero counts and nonzero counts
        uint totalNonzero = 0;
        for (int pair = 0; pair < numRunPairs; pair++)
        {
            uint nzlen = runLens[pair * 2 + 1];
            uint zlen = runLens[pair * 2 + 2];

            runLens[pair * 2 + 0] = (ushort)curSymbol;

            curSymbol += nzlen + zlen;
            totalNonzero += nzlen;
        }

        // Final NZ run is implied and has nonzero length; check that the counts work out.
        if (curSymbol >= 256 || totalNonzero >= numNonzeroSyms)
            return -1;

        uint finalRunLen = numNonzeroSyms - totalNonzero;
        if (curSymbol + finalRunLen > 256)
            return -1;

        runLens[numRunPairs * 2 + 0] = (ushort)curSymbol;
        runLens[numRunPairs * 2 + 1] = (ushort)finalRunLen;

        if (curSymbol > 256) return -1;

        return numRunPairs + 1;
    }

    private static bool newlz_build_msbfirst_table(uint* firstSymOfLen, uint* lastSymOfLen, KrakenMSBHuffTab* msbHuff, byte* symLists)
    {
        byte* lens = msbHuff->len;
        byte* syms = msbHuff->sym;

        uint curCode = 0;
        for (uint codeLen = 1; codeLen < NEWLZ_HUFF_CODELEN_LIMIT; ++codeLen)
        {
            uint num = lastSymOfLen[codeLen] - firstSymOfLen[codeLen];
            if (num == 0) continue;

            uint numEntriesShift = (uint)(NEWLZ_HUFF_CODELEN_LIMIT - codeLen);
            uint numEntriesPerSym = 1u << (int)numEntriesShift;
            uint numTableEntriesThisLen = num << (int)numEntriesShift;

            if (curCode + numTableEntriesThisLen > NEWLZ_HUFF_DECODE_TABLE_SIZE) return false;

            for (uint k = 0; k < numTableEntriesThisLen; k++) lens[curCode + k] = (byte)codeLen;

            byte* symList = symLists + firstSymOfLen[codeLen];
            for (uint i = 0; i < num; i++)
            {
                byte s = symList[i];
                for (uint k = 0; k < numEntriesPerSym; k++) syms[curCode + k] = s;
                curCode += numEntriesPerSym;
            }
        }

        uint numLast = lastSymOfLen[NEWLZ_HUFF_CODELEN_LIMIT] - firstSymOfLen[NEWLZ_HUFF_CODELEN_LIMIT];
        if (numLast != 0)
        {
            if (curCode + numLast > NEWLZ_HUFF_DECODE_TABLE_SIZE) return false;

            for (uint k = 0; k < numLast; k++) lens[curCode + k] = (byte)NEWLZ_HUFF_CODELEN_LIMIT;

            byte* symList = symLists + firstSymOfLen[NEWLZ_HUFF_CODELEN_LIMIT];
            for (uint k = 0; k < numLast; k++) syms[curCode + k] = symList[k];

            curCode += numLast;
        }

        return curCode == NEWLZ_HUFF_DECODE_TABLE_SIZE;
    }

    private static void newLZ_bit_reverse_hufftab(KrakenHuffTab* to_table, KrakenMSBHuffTab* msbHuff)
    {
        for (uint i = 0; i < NEWLZ_HUFF_DECODE_TABLE_SIZE; i++)
        {
            uint rev = newlz_huff_bitreverse(i);
            to_table->e[rev] = (ushort)(msbHuff->len[i] | (msbHuff->sym[i] << 8));
        }
    }

    private static KrakenHuffTab* newLZ_prep_hufftab(KrakenHuffTab* to_table, KrakenMSBHuffTab* msbHuff)
    {
        newLZ_bit_reverse_hufftab(to_table, msbHuff);
        return to_table;
    }

    private static bool newlz_huff64(KrakenHuffState* s, KrakenMSBHuffTab* huff, bool is_huff6)
    {
        KrakenHuffTab* phufftab = newLZ_prep_hufftab(&s->table, huff);
        ushort* table = phufftab->e;
        int half = 0;

        while (true)
        {
            byte* in0 = (byte*)s->bitp[0];
            byte* in1 = (byte*)s->bitp[1];
            byte* in2 = (byte*)s->bitp[2];

            byte* decodeptr = (byte*)s->decodeptr[0];
            byte* decodeend = (byte*)s->decodeend[0];
            byte* strm0_end = (byte*)s->strm0_end[0];

            H.LogOodle($"newlz_huff64: half={half} decodeend-decodeptr={decodeend - decodeptr}");

            // Constants for 64-bit bulk decoding (matching native implementation)
            const int N_DECS_PER_REFILL = 5;  // With 56 bits and max 11-bit codes, we can decode 5 symbols
            const int TRIPLE_DECS_PER_REFILL = 3 * N_DECS_PER_REFILL;  // 15 symbols per iteration
            const int nIterCheck = 8;
            const int nBytesCheck = nIterCheck * TRIPLE_DECS_PER_REFILL;  // 120 bytes between checks
            const int nBytesDecOverRead = 7;  // 8-byte access gives 7B overread
            const int nBytesDecMax = (nBytesCheck * 11 + 7) / 8 + nBytesDecOverRead;  // Max bytes accessed

            bool usedBulk = false;
            ulong cbits0 = 0, cbits1 = 0, cbits2 = 0;
            int cbitc0 = 0, cbitc1 = 0, cbitc2 = 0;

            // Bulk loop using 64-bit reads
            if (decodeend - decodeptr > TRIPLE_DECS_PER_REFILL - 1)
            {
                byte* decodeend_bulk = decodeend - (TRIPLE_DECS_PER_REFILL - 1);

                // Bit buffers start empty for bulk loop
                ulong bits0 = 0, bits1 = 0, bits2 = 0;
                int bitcount0 = 0, bitcount1 = 0, bitcount2 = 0;

                while (true)
                {
                    long nBytesDecLeft = decodeend_bulk - decodeptr;
                    if (nBytesDecLeft <= 0)
                        break;

                    byte* decodemark = decodeptr + nBytesCheck;
                    if (nBytesDecLeft < nBytesCheck)
                        decodemark = decodeend_bulk;

                    // Go to careful loop once stream pointers crossed or at risk
                    if (in0 > in2 || (in1 - in2) < nBytesDecMax)
                        break;

                    // Main loop
                    while (decodeptr < decodemark)
                    {
                        // Refill using 64-bit reads
                        bits0 |= *(ulong*)in0 << bitcount0;
                        in0 += (63 - bitcount0) >> 3;
                        bitcount0 |= 56;

                        bits1 |= H.Bswap64(*(ulong*)(in1 - 8)) << bitcount1;
                        in1 -= (63 - bitcount1) >> 3;
                        bitcount1 |= 56;

                        bits2 |= *(ulong*)in2 << bitcount2;
                        in2 += (63 - bitcount2) >> 3;
                        bitcount2 |= 56;

                        // Decode 15 symbols (5 per stream x 3 streams)
                        uint e; int cl;

                        // Round 1
                        e = table[bits0 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits0 >>= cl; bitcount0 -= cl;
                        e = table[bits1 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits1 >>= cl; bitcount1 -= cl;
                        e = table[bits2 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits2 >>= cl; bitcount2 -= cl;

                        // Round 2
                        e = table[bits0 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits0 >>= cl; bitcount0 -= cl;
                        e = table[bits1 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits1 >>= cl; bitcount1 -= cl;
                        e = table[bits2 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits2 >>= cl; bitcount2 -= cl;

                        // Round 3
                        e = table[bits0 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits0 >>= cl; bitcount0 -= cl;
                        e = table[bits1 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits1 >>= cl; bitcount1 -= cl;
                        e = table[bits2 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits2 >>= cl; bitcount2 -= cl;

                        // Round 4
                        e = table[bits0 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits0 >>= cl; bitcount0 -= cl;
                        e = table[bits1 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits1 >>= cl; bitcount1 -= cl;
                        e = table[bits2 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits2 >>= cl; bitcount2 -= cl;

                        // Round 5
                        e = table[bits0 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits0 >>= cl; bitcount0 -= cl;
                        e = table[bits1 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits1 >>= cl; bitcount1 -= cl;
                        e = table[bits2 & 2047]; cl = (int)(e & 0xFF); *decodeptr++ = (byte)(e >> 8); bits2 >>= cl; bitcount2 -= cl;
                    }
                }

                // Transition to careful loop - convert 64-bit state back to careful state
                usedBulk = true;
                in0 -= (bitcount0 >> 3);
                in1 += (bitcount1 >> 3);
                in2 -= (bitcount2 >> 3);
                cbits0 = bits0 & 0xFF; cbitc0 = bitcount0 & 7;
                cbits1 = bits1 & 0xFF; cbitc1 = bitcount1 & 7;
                cbits2 = bits2 & 0xFF; cbitc2 = bitcount2 & 7;
            }

            if (!usedBulk)
            {
                cbits0 = s->bits[0]; cbitc0 = (int)s->bitc[0];
                cbits1 = s->bits[1]; cbitc1 = (int)s->bitc[1];
                cbits2 = s->bits[2]; cbitc2 = (int)s->bitc[2];
            }

            if (in0 > in2)
            {
                H.LogOodle($"newlz_huff64: half={half} in0 > in2");
                return false;
            }

            // Final/careful loop - handles byte-by-byte refill carefully
            while (decodeptr < decodeend)
            {
                int cl;
                uint sym;

                // Refill bits0 - forward stream from in0
                long avail = in2 - in0;
                if (avail > 1)
                    cbits0 |= (ulong)(*(ushort*)in0) << cbitc0;
                else if (avail == 1)
                    cbits0 |= (ulong)in0[0] << cbitc0;

                // Decode from stream 0
                uint entry0 = table[cbits0 & 2047];
                cl = (int)(entry0 & 0xFF);
                sym = entry0 >> 8;

                int availBits = cbitc0 + (int)(avail < 2 ? avail : 2) * 8;
                if (availBits < cl)
                {
                    H.LogOodle($"newlz_huff64: half={half} stream0 not enough bits: bitc0={cbitc0} available={availBits} cl={cl}");
                    return false;
                }

                cbits0 >>= cl;
                cbitc0 -= cl;
                in0 += (7 - cbitc0) >> 3;
                cbitc0 &= 7;
                *decodeptr++ = (byte)sym;

                if (decodeptr >= decodeend) break;

                // Refill bits1 and bits2 - they share the middle region
                avail = in1 - in2;
                if (avail > 1)
                {
                    cbits1 |= (ulong)H.ReadU16BE(in1 - 2) << cbitc1;
                    cbits2 |= (ulong)(*(ushort*)in2) << cbitc2;
                }
                else if (avail == 1)
                {
                    ulong shared = in2[0];
                    cbits1 |= shared << cbitc1;
                    cbits2 |= shared << cbitc2;
                }

                // Decode from stream 1 (backward)
                uint entry1 = table[cbits1 & 2047];
                cl = (int)(entry1 & 0xFF);
                sym = entry1 >> 8;

                availBits = cbitc1 + (int)(avail < 2 ? avail : 2) * 8;
                if (availBits < cl)
                {
                    H.LogOodle($"newlz_huff64: half={half} stream1 not enough bits: bitc1={cbitc1} available={availBits} cl={cl}");
                    return false;
                }

                cbits1 >>= cl;
                cbitc1 -= cl;
                in1 -= (7 - cbitc1) >> 3;
                cbitc1 &= 7;
                *decodeptr++ = (byte)sym;

                if (decodeptr >= decodeend) break;

                // Decode from stream 2 (forward from middle)
                uint entry2 = table[cbits2 & 2047];
                cl = (int)(entry2 & 0xFF);
                sym = entry2 >> 8;

                if (cbitc2 + (int)(avail < 2 ? avail : 2) * 8 < cl)
                {
                    H.LogOodle($"newlz_huff64: half={half} stream2 not enough bits: bitc2={cbitc2} available={(int)(avail < 2 ? avail : 2) * 8} cl={cl}");
                    return false;
                }

                cbits2 >>= cl;
                cbitc2 -= cl;
                in2 += (7 - cbitc2) >> 3;
                cbitc2 &= 7;
                *decodeptr++ = (byte)sym;

                // Corruption check
                if (in0 > in2 || in2 > in1)
                {
                    H.LogOodle($"newlz_huff64: half={half} stream crossing: in0={((long)in0):X} in2={((long)in2):X} in1={((long)in1):X}");
                    return false;
                }
            }

            s->bitp[0] = (ulong)in0; s->bits[0] = (uint)cbits0; s->bitc[0] = (uint)cbitc0;
            s->bitp[1] = (ulong)in1; s->bits[1] = (uint)cbits1; s->bitc[1] = (uint)cbitc1;
            s->bitp[2] = (ulong)in2; s->bits[2] = (uint)cbits2; s->bitc[2] = (uint)cbitc2;
            s->decodeptr[0] = (ulong)decodeptr;

            if (decodeptr != decodeend)
            {
                H.LogOodle($"newlz_huff64: half={half} decodeptr != decodeend");
                return false;
            }

            if (in0 != (byte*)s->strm0_end[0] || in1 != in2)
            {
                H.LogOodle($"newlz_huff64: half={half} stream end mismatch in0={((long)in0):X} strm0_end={s->strm0_end[0]:X} in1={((long)in1):X} in2={((long)in2):X}");
                return false;
            }

            if (!is_huff6) break;

            s->decodeptr[0] = s->decodeptr[1];
            s->decodeend[0] = s->decodeend[1];
            s->strm0_end[0] = s->strm0_end[1];

            s->bitp[0] = s->bitp[3]; s->bits[0] = s->bits[3]; s->bitc[0] = s->bitc[3];
            s->bitp[1] = s->bitp[4]; s->bits[1] = s->bits[4]; s->bitc[1] = s->bitc[4];
            s->bitp[2] = s->bitp[5]; s->bits[2] = s->bits[5]; s->bitc[2] = s->bitc[5];

            is_huff6 = false;
            half++;
        }

        return true;
    }

    private static long newlz_get_array_huff(byte* comp, long comp_len, byte* to, long to_len, bool is_huff6)
    {
        H.LogOodle($"newlz_get_array_huff: comp_len={comp_len} to_len={to_len} is_huff6={is_huff6}");
        byte* comp_end = comp + comp_len;
        KrakenMSBHuffTab msbHuff;

        rrVarBits vb = new rrVarBits();
        rrVarBits_GetOpen(ref vb, comp, comp_end);

        byte* symListsBuf = stackalloc byte[256 + 256 * (NEWLZ_HUFF_CODELEN_LIMIT - 7)];
        uint* firstSymOfLen = stackalloc uint[NEWLZ_HUFF_CODELEN_LIMIT + 1];
        uint* lastSymOfLen = stackalloc uint[NEWLZ_HUFF_CODELEN_LIMIT + 1];

        firstSymOfLen[0] = 0; lastSymOfLen[0] = 0;

        uint cur = 0;
        for (uint i = 1; i <= 7; i++)
        {
            firstSymOfLen[i] = cur; lastSymOfLen[i] = cur;
            cur += 1u << (int)i;
        }
        for (uint i = 8; i <= NEWLZ_HUFF_CODELEN_LIMIT; i++)
        {
            firstSymOfLen[i] = cur; lastSymOfLen[i] = cur;
            cur += 256;
        }

        int gotNumSymbols;
        uint huff_type_flag1 = rrVarBits_Get1(ref vb);
        H.LogOodle($"newlz_get_array_huff: huff_type_flag1={huff_type_flag1}");
        if (huff_type_flag1 != 0)
        {
            uint huff_type_flag2 = rrVarBits_Get1(ref vb);
            H.LogOodle($"newlz_get_array_huff: huff_type_flag2={huff_type_flag2}");
            if (huff_type_flag2 == 0)
            {
                gotNumSymbols = newlz_decode_hufflens2(ref vb, symListsBuf, lastSymOfLen);
            }
            else
            {
                H.LogOodle($"newlz_get_array_huff: unsupported huff type (flag2=1)");
                return -1;
            }
        }
        else
        {
            gotNumSymbols = newlz_decode_hufflens(ref vb, symListsBuf, lastSymOfLen);
        }

        H.LogOodle($"newlz_get_array_huff: gotNumSymbols={gotNumSymbols}");
        if (gotNumSymbols < 1) { H.LogOodle($"newlz_get_array_huff: gotNumSymbols < 1 returning -1"); return -1; }
        else if (gotNumSymbols == 1)
        {
            byte val = symListsBuf[0];
            H.FillBytes(to, val, (int)to_len);

            return (long)rrVarBits_GetSizeBytes(ref vb, comp);
        }

        if (!newlz_build_msbfirst_table(firstSymOfLen, lastSymOfLen, &msbHuff, symListsBuf)) { H.LogOodle($"newlz_get_array_huff: build_msbfirst_table failed"); return -1; }

        int bitlen_at_end = 63 - vb.m_inv_bitlen;
        long header_size = (long)(vb.m_cur - comp - (bitlen_at_end >> 3));
        byte* comp_start = comp + header_size;
        H.LogOodle($"newlz_get_array_huff: header_size={header_size} comp_end-comp_start={comp_end - comp_start}");


        KrakenHuffState s = new KrakenHuffState();

        if (!is_huff6)
        {
            if (comp_end - comp_start < 3) { H.LogOodle($"newlz_get_array_huff: !huff6 comp_end-comp_start < 3"); return -1; }

            int comp_len1 = H.ReadU16LE(comp_start);
            comp_start += 2;

            if (comp_end - comp_start < comp_len1 + 2) { H.LogOodle($"newlz_get_array_huff: !huff6 comp_end-comp_start < comp_len1 + 2"); return -1; }

            s.decodeptr[0] = (ulong)to;
            s.decodeend[0] = (ulong)(to + to_len);
            s.strm0_end[0] = (ulong)(comp_start + comp_len1);

            s.bitp[0] = (ulong)comp_start; s.bits[0] = 0; s.bitc[0] = 0;
            s.bitp[1] = (ulong)comp_end; s.bits[1] = 0; s.bitc[1] = 0;
            s.bitp[2] = (ulong)(comp_start + comp_len1); s.bits[2] = 0; s.bitc[2] = 0;
        }
        else
        {
            if (comp_end - comp_start < 6)
            {
                H.LogOodle($"newlz_get_array_huff: huff6 comp_end-comp_start < 6");
                return -1;
            }

            long first_half_to_len = (to_len + 1) >> 1;

            uint first_half_len = H.ReadU24LE(comp_start);
            comp_start += 3;
            H.LogOodle($"newlz_get_array_huff: huff6 first_half_len={first_half_len} first_half_to_len={first_half_to_len} comp_end-comp_start={comp_end - comp_start}");

            if (comp_end - comp_start < first_half_len)
            {
                H.LogOodle($"newlz_get_array_huff: huff6 comp_end-comp_start < first_half_len");
                return -1;
            }

            byte* first_half_end = comp_start + first_half_len;

            uint comp_str0len = H.ReadU16LE(comp_start);
            comp_start += 2;
            H.LogOodle($"newlz_get_array_huff: huff6 comp_str0len={comp_str0len} first_half_end-comp_start={first_half_end - comp_start}");

            if (first_half_end - comp_start < comp_str0len + 2)
            {
                H.LogOodle($"newlz_get_array_huff: huff6 first_half_end-comp_start < comp_str0len + 2");
                return -1;
            }

            byte* comp_mid = first_half_end;
            if (comp_end - comp_mid < 3)
            {
                H.LogOodle($"newlz_get_array_huff: huff6 comp_end-comp_mid < 3");
                return -1;
            }

            uint comp_str3len = H.ReadU16LE(comp_mid);
            comp_mid += 2;
            H.LogOodle($"newlz_get_array_huff: huff6 comp_str3len={comp_str3len} comp_end-comp_mid={comp_end - comp_mid}");

            if (comp_end - comp_mid < comp_str3len + 2)
            {
                H.LogOodle($"newlz_get_array_huff: huff6 comp_end-comp_mid < comp_str3len + 2");
                return -1;
            }

            s.decodeptr[0] = (ulong)to;
            s.decodeend[0] = (ulong)(to + first_half_to_len);
            s.decodeptr[1] = (ulong)(to + first_half_to_len);
            s.decodeend[1] = (ulong)(to + to_len);
            s.strm0_end[0] = (ulong)(comp_start + comp_str0len);
            s.strm0_end[1] = (ulong)(comp_mid + comp_str3len);

            s.bitp[0] = (ulong)comp_start; s.bits[0] = 0; s.bitc[0] = 0;
            s.bitp[1] = (ulong)first_half_end; s.bits[1] = 0; s.bitc[1] = 0;
            s.bitp[2] = (ulong)(comp_start + comp_str0len); s.bits[2] = 0; s.bitc[2] = 0;
            s.bitp[3] = (ulong)comp_mid; s.bits[3] = 0; s.bitc[3] = 0;
            s.bitp[4] = (ulong)comp_end; s.bits[4] = 0; s.bitc[4] = 0;
            s.bitp[5] = (ulong)(comp_mid + comp_str3len); s.bits[5] = 0; s.bitc[5] = 0;
        }

        H.LogOodle($"newlz_get_array_huff: calling newlz_huff64");
        if (newlz_huff64(&s, &msbHuff, is_huff6)) { H.LogOodle($"newlz_get_array_huff: huff64 succeeded"); return comp_len; }
        H.LogOodle($"newlz_get_array_huff: huff64 failed");
        return -1;
    }

    // Array Types
    private const int NEWLZ_ARRAY_TYPE_UNCOMPRESSED = 0;
    private const int NEWLZ_ARRAY_TYPE_HUFF = 2;
    private const int NEWLZ_ARRAY_TYPE_HUFF6 = 4;
    private const int NEWLZ_ARRAY_TYPE_TANS = 1;
    private const int NEWLZ_ARRAY_TYPE_RLE = 3;
    private const int NEWLZ_ARRAY_TYPE_SPLIT = 5;
    private const int NEWLZ_ARRAY_TYPE_COUNT = 6;

    private const int NEWLZ_ARRAY_SIZE_BITS = 18;
    private const int NEWLZ_ARRAY_SIZE_MASK = 0x3FFFF;

    private const int NEWLZ_TANS_L_BITS_MAX = 11;
    private const int NEWLZ_TANS_L_BITS_MIN = 8;
    private const int NEWLZ_TANS_L_BITS_BITS = 2;

    [StructLayout(LayoutKind.Sequential)]
    private struct newlz_tans_UnpackedCounts
    {
        public int num_singles;
        public int num_larger;
        public fixed byte singles[256];
        public fixed uint larger[256];
    }

    [StructLayout(LayoutKind.Explicit, Size = 8)]
    private struct tans_decode_entry_U8
    {
        [FieldOffset(0)] public uint mask;
        [FieldOffset(4)] public byte len;
        [FieldOffset(5)] public byte sym;
        [FieldOffset(6)] public ushort nextst;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct newlz_tans_Decoder
    {
        public int L;
        public int L_bits;
        public tans_decode_entry_U8* decode_table_U8;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KrakenTansState
    {
        public byte* decodeptr;
        public byte* decodeend;
        public byte** bitp; // bitp[2]
        public uint* bits; // bits[2]
        public uint* bitc; // bitc[2]
        public tans_decode_entry_U8* table;
        public fixed uint tans_state[5];
    }
    private const int NEWLZ_ARRAY_SMALL_SIZE_BITS = 10;
    private const int NEWLZ_ARRAY_SMALL_SIZE_MASK = 1023;

    // Get array lengths without decoding data - used to determine sizes before actual decode
    private static long newLZ_get_arraylens(byte* from, byte* from_end, long* pto_len, long to_len_max)
    {
        byte* from_ptr = from;

        if (from_end - from_ptr < 2) return -1;

        byte first_byte = *from_ptr;
        uint array_type = (uint)(first_byte >> 4);
        H.LogOodle($"newLZ_get_arraylens: first_byte={first_byte:X2} array_type={array_type}");

        if (first_byte >= 0x80) // small flag
        {
            array_type &= 7;

            if (array_type == NEWLZ_ARRAY_TYPE_UNCOMPRESSED)
            {
                // 2 byte header
                uint header = H.ReadU16BE(from_ptr);
                from_ptr += 2;

                long to_len = (long)(header & 0xFFF);
                if (to_len > to_len_max) return -1;
                if (from_ptr + to_len > from_end) return -1;

                from_ptr += to_len;
                *pto_len = to_len;
                return from_ptr - from;
            }
            else
            {
                // 3 byte header, must have >= 1 payload byte
                if (from_end - from_ptr < 4) return -1;

                uint header = H.ReadU24BE(from_ptr);
                from_ptr += 3;

                if (array_type >= NEWLZ_ARRAY_TYPE_COUNT) return -1;

                // 10 bits comp, 10 bits raw + 4 bits header
                long comp_len = (long)(header & NEWLZ_ARRAY_SMALL_SIZE_MASK);
                if (from_ptr + comp_len > from_end) return -1;

                long to_len = (long)((header >> NEWLZ_ARRAY_SMALL_SIZE_BITS) & NEWLZ_ARRAY_SMALL_SIZE_MASK);
                to_len += comp_len + 1;

                if (to_len > to_len_max) return -1;

                from_ptr += comp_len;
                *pto_len = to_len;
                return from_ptr - from;
            }
        }
        else
        {
            // Large header
            if (from_end - from_ptr < 4)
            {
                // Could be a 3-byte header at end of stream
                if (from_end - from_ptr == 3)
                {
                    uint header = H.ReadU24BE(from_ptr);
                    if (header == 0)
                    {
                        *pto_len = 0;
                        return 3;
                    }
                }
                return -1;
            }

            if (array_type == NEWLZ_ARRAY_TYPE_UNCOMPRESSED)
            {
                // 3-byte header
                uint header = H.ReadU24BE(from_ptr);
                from_ptr += 3;

                long to_len = (long)(header & NEWLZ_ARRAY_SIZE_MASK);
                if ((header >> NEWLZ_ARRAY_SIZE_BITS) != 0) return -1;

                if (to_len > to_len_max) return -1;
                if (to_len > from_end - from_ptr) return -1;

                from_ptr += to_len;
                *pto_len = to_len;
                return from_ptr - from;
            }
            else
            {
                if (array_type >= NEWLZ_ARRAY_TYPE_COUNT) return -1;

                if (from_end - from_ptr < 5) return -1;

                // 5-byte header
                ulong header64 = (ulong)*from_ptr++;
                header64 <<= 32;
                header64 += H.ReadU32BE(from_ptr);
                from_ptr += 4;

                long comp_len = (long)(header64 & NEWLZ_ARRAY_SIZE_MASK);
                if (comp_len > from_end - from_ptr) return -1;

                long to_len = (long)((header64 >> NEWLZ_ARRAY_SIZE_BITS) & NEWLZ_ARRAY_SIZE_MASK);
                to_len++;

                if (to_len > to_len_max) return -1;
                if (comp_len >= to_len) return -1;

                from_ptr += comp_len;
                *pto_len = to_len;
                return from_ptr - from;
            }
        }
    }

    private static long newLZ_get_array_rle(byte* comp, long comp_len, byte* to, long to_len, byte* scratch_ptr, byte* scratch_end)
    {
        if (comp_len <= 1 || to_len <= 0)
        {
            if (comp_len == 1)
            {
                byte val = comp[0];
                H.FillBytes(to, val, (int)to_len);
                return comp_len;
            }
            else
            {
                return -1;
            }
        }

        byte* lits = comp + 1;
        byte* pkts = comp + comp_len;

        if (comp[0] != 0)
        {
            byte* temp_to = scratch_ptr;
            long temp_to_len = 0;
            long from_len = newLZ_get_array(&temp_to, comp, comp + comp_len, &temp_to_len, (long)(scratch_end - scratch_ptr), true, scratch_ptr, scratch_end);
            if (from_len <= 0) return -1;

            long rest_size = comp_len - from_len;
            if (temp_to_len + rest_size > (scratch_end - scratch_ptr)) return -1;

            Buffer.MemoryCopy(comp + from_len, temp_to + temp_to_len, rest_size, rest_size);
            lits = temp_to;
            pkts = temp_to + temp_to_len + rest_size;

            scratch_ptr += temp_to_len + rest_size;
        }

        byte* outp = to;
        byte* out_end = to + to_len;
        byte run_val = 0;

        while (pkts > lits && outp < out_end)
        {
            byte packet = *(--pkts);

            if ((byte)(packet - 1) >= 0x2F) // Simple packet
            {
                int lrl = (15 - packet) & 0xF;
                int rl = packet >> 4;

                if (lrl > 0)
                {
                    H.CopyBytes_SIMD(outp, lits, lrl);
                    lits += lrl;
                    outp += lrl;
                }

                if (rl > 0)
                {
                    H.FillBytes(outp, run_val, rl);
                    outp += rl;
                }
            }
            else if (packet >= 0x10) // Medium LRL + RL
            {
                pkts--;
                ushort v_raw = H.ReadU16LE(pkts);
                int v = v_raw - 0x1000;

                int lrl = v & 0x3f;
                int rl = v >> 6;

                if (lrl > 0)
                {
                    H.CopyBytes_SIMD(outp, lits, lrl);
                    lits += lrl;
                    outp += lrl;
                }

                if (rl > 0)
                {
                    H.FillBytes(outp, run_val, rl);
                    outp += rl;
                }
            }
            else if (packet == 1) // Change run value
            {
                run_val = *lits++;
            }
            else if (packet >= 0x09) // Long RL
            {
                pkts--;
                ushort v_raw = H.ReadU16LE(pkts);
                int v = v_raw - 0x900 + 1;

                int rl = v << 7;

                H.FillBytes(outp, run_val, rl);
                outp += rl;
            }
            else // Long LRL (packet >= 0x02)
            {
                pkts--;
                ushort v_raw = H.ReadU16LE(pkts);
                int v = v_raw - 0x200 + 1;

                int lrl = v << 6;

                H.CopyBytes_SIMD(outp, lits, lrl);
                lits += lrl;
                outp += lrl;
            }
        }

        return comp_len;
    }

    private static long newLZ_get_array(byte** ptr_to, byte* from, byte* from_end, long* pto_len, long to_len_max, bool force_copy_uncompressed, byte* scratch_ptr, byte* scratch_end)
    {
        byte* from_ptr = from;
        if (from_end - from_ptr < 2) { H.LogOodle("newLZ_get_array: from_end - from_ptr < 2"); return -1; }

        byte first_byte = *from_ptr;
        uint array_type = (uint)(first_byte >> 4);
        H.LogOodle($"newLZ_get_array: first_byte={first_byte:X2} array_type={array_type} from_end-from_ptr={from_end - from_ptr} to_len_max={to_len_max}");

        if (first_byte >= 0x80)
        {
            array_type &= 7;
            H.LogOodle($"newLZ_get_array: short header, array_type={array_type} from_ptr[0]={from_ptr[0]:X2} from_ptr[1]={from_ptr[1]:X2}");
            if (array_type == 0)
            {
                uint header = H.ReadU16BE(from_ptr);
                from_ptr += 2;

                long to_len = (long)(header & 0xFFF);
                H.LogOodle($"newLZ_get_array: short uncompressed header={header:X4} to_len={to_len} from_end-from_ptr={from_end - from_ptr}");
                if (to_len > to_len_max) { H.LogOodle($"newLZ_get_array: to_len={to_len} > to_len_max={to_len_max}"); return -1; }
                if (from_ptr + to_len > from_end) { H.LogOodle($"newLZ_get_array: from_ptr + to_len > from_end"); return -1; }

                if (force_copy_uncompressed)
                {
                    Buffer.MemoryCopy(from_ptr, *ptr_to, to_len, to_len);
                }
                else
                {
                    *ptr_to = from_ptr;
                }

                from_ptr += to_len;
                *pto_len = to_len;
                return from_ptr - from;
            }
        }
        else
        {
            if (from_end - from_ptr < 4)
            {
                if (from_end - from_ptr == 3)
                {
                    uint header = H.ReadU24BE(from_ptr);
                    if (header == 0)
                    {
                        *ptr_to = null;
                        *pto_len = 0;
                        return 3;
                    }
                }
                return -1;
            }

            if (array_type == NEWLZ_ARRAY_TYPE_UNCOMPRESSED)
            {
                uint header = H.ReadU24BE(from_ptr);
                from_ptr += 3;

                long to_len = (long)(header & NEWLZ_ARRAY_SIZE_MASK);
                if ((header >> NEWLZ_ARRAY_SIZE_BITS) != 0) return -1;

                if (to_len > to_len_max) return -1;
                if (to_len > (from_end - from_ptr)) return -1;

                if (force_copy_uncompressed)
                {
                    Buffer.MemoryCopy(from_ptr, *ptr_to, to_len, to_len);
                }
                else
                {
                    *ptr_to = from_ptr;
                }

                from_ptr += to_len;
                *pto_len = to_len;
                return from_ptr - from;
            }
        }

        return newLZ_get_array_comp(array_type, ptr_to, from, from_end, pto_len, to_len_max, scratch_ptr, scratch_end);
    }

    private static long newLZ_get_array_comp(uint array_type, byte** ptr_to, byte* from, byte* from_end, long* pto_len, long to_len_max, byte* scratch_ptr, byte* scratch_end)
    {
        if (array_type == NEWLZ_ARRAY_TYPE_UNCOMPRESSED || array_type >= 6) return -1;

        byte* from_ptr = from;
        long comp_len;
        long to_len;

        if (*from_ptr >= 0x80)
        {
            if (from_end - from_ptr < 4) return -1;

            uint header = H.ReadU24BE(from_ptr);
            from_ptr += 3;

            comp_len = (long)(header & NEWLZ_ARRAY_SMALL_SIZE_MASK);
            if (comp_len > (from_end - from_ptr)) return -1;

            to_len = (long)((header >> NEWLZ_ARRAY_SMALL_SIZE_BITS) & NEWLZ_ARRAY_SMALL_SIZE_MASK);
            to_len += comp_len + 1;

            if (to_len > to_len_max) { H.LogOodle($"newLZ_get_array_comp short: to_len={to_len} > to_len_max={to_len_max}"); return -1; }
        }
        else
        {
            if (from_end - from_ptr < 5)
            {
                return -1;
            }

            ulong h1 = *from_ptr++;
            uint h2 = H.ReadU32BE(from_ptr);
            from_ptr += 4;
            ulong headerVal = (h1 << 32) | h2;

            if ((headerVal >> 36) != array_type)
            {
                return -1;
            }

            comp_len = (long)(headerVal & NEWLZ_ARRAY_SIZE_MASK);
            if (comp_len > (from_end - from_ptr))
            {
                return -1;
            }

            to_len = (long)((headerVal >> NEWLZ_ARRAY_SIZE_BITS) & NEWLZ_ARRAY_SIZE_MASK);
            to_len++;

            if (to_len > to_len_max)
            {
                return -1;
            }
            if (comp_len >= to_len)
            {
                return -1;
            }
        }

        if (*ptr_to == scratch_ptr)
        {
            if ((scratch_end - scratch_ptr) < to_len)
            {
                return -1;
            }
            scratch_ptr += to_len;
        }

        long comp_used = -1;

        if (array_type == NEWLZ_ARRAY_TYPE_SPLIT)
        {
            comp_used = newLZ_get_array_split(from_ptr, comp_len, *ptr_to, to_len, scratch_ptr, scratch_end);
        }
        else if (array_type == NEWLZ_ARRAY_TYPE_RLE)
        {
            comp_used = newLZ_get_array_rle(from_ptr, comp_len, *ptr_to, to_len, scratch_ptr, scratch_end);
        }
        else if (array_type == NEWLZ_ARRAY_TYPE_TANS)
        {
            comp_used = newlz_get_array_tans(from_ptr, comp_len, *ptr_to, to_len, scratch_ptr, scratch_end);
        }
        else
        {
            comp_used = newlz_get_array_huff(from_ptr, comp_len, *ptr_to, to_len, array_type == NEWLZ_ARRAY_TYPE_HUFF6);
        }

        if (comp_len != comp_used) { H.LogOodle($"newLZ_get_array_comp: comp_len={comp_len} != comp_used={comp_used}"); return -1; }

        *pto_len = to_len;
        return (from_ptr - from) + comp_len;
    }

    private static int newLZ_offset44_decode64_tab(ref KrakenOffsetState s)
    {
        byte* offs_u8 = s.offs_u8;
        byte* offs_u8_end = s.offs_u8_end;
        byte* offs_u8_start = offs_u8;

        if (offs_u8_end - offs_u8 >= 8 && s.bitp[1] - s.bitp[0] >= 8)
        {
            offs_u8_end -= 7;

            int* neg_offs_s32 = s.neg_offs_s32;
            byte* bitp0 = (byte*)s.bitp[0];
            byte* bitp1 = (byte*)s.bitp[1] - 8;
            uint bitc0 = s.bitc[0];
            uint bitc1 = s.bitc[1];

            while (offs_u8 < offs_u8_end)
            {
                bitp0 += (int)(bitc0 >> 3); bitc0 &= 7;
                bitp1 -= (int)(bitc1 >> 3); bitc1 &= 7;

                if (bitp0 > bitp1) break;

                // Read 64 bits big-endian for forward stream
                ulong bits0 = H.ReadU64BE(bitp0);
                // Read 64 bits little-endian for backward stream
                ulong bits1 = H.ReadU64LE(bitp1);

                ulong next_offsets = H.ReadU64LE(offs_u8);
                ulong next_offsets_hi = next_offsets & 0x8080808080808080;
                ulong splat8 = 0x0101010101010101;
                ulong has_large = (0xef * splat8 - next_offsets) & next_offsets_hi;

                if (has_large == 0)
                {
                    for (int k = 0; k < 6; k++)
                    {
                        byte packed = offs_u8[k];
                        int num_raw = (packed >> 4) + OFFSET_RAW_BITS;

                        uint offs_raw_bits;
                        if ((k & 1) == 0)
                        {
                            offs_raw_bits = (uint)((bits0 << (int)bitc0) >> 32) >> (32 - num_raw);
                            bitc0 += (uint)num_raw;
                        }
                        else
                        {
                            offs_raw_bits = (uint)((bits1 << (int)bitc1) >> 32) >> (32 - num_raw);
                            bitc1 += (uint)num_raw;
                        }

                        int neg_offs = newlz_offsets44_bias_tab[packed] - ((int)offs_raw_bits << 4);
                        neg_offs_s32[k] = neg_offs;
                    }

                    offs_u8 += 6;
                    neg_offs_s32 += 6;
                }
                else
                {
                    ulong has_bad_codes = (0xfd * splat8 - next_offsets) & next_offsets_hi;
                    if (has_bad_codes != 0) return 0;

                    for (int k = 0; k < 6; k++)
                    {
                        byte packed = offs_u8[k];
                        int num_raw = newlz_offsets44_count_tab[packed];

                        uint offs_raw_bits;
                        if ((k & 1) == 0)
                        {
                            offs_raw_bits = (uint)((bits0 << (int)bitc0) >> 32) >> (32 - num_raw);
                            bitc0 += (uint)num_raw;
                        }
                        else
                        {
                            offs_raw_bits = (uint)((bits1 << (int)bitc1) >> 32) >> (32 - num_raw);
                            bitc1 += (uint)num_raw;
                        }

                        int shifted_offs = (int)offs_raw_bits << 4;
                        int neg_offs = newlz_offsets44_bias_tab[packed] - (packed >= 0xf0 ? (int)offs_raw_bits : shifted_offs);
                        neg_offs_s32[k] = neg_offs;

                        if (k == 1 || k == 3)
                        {
                            if (bitc0 >= 34 || bitc1 >= 34)
                            {
                                offs_u8 += k + 1;
                                neg_offs_s32 += k + 1;
                                goto continue_outer;
                            }
                        }
                    }

                    offs_u8 += 6;
                    neg_offs_s32 += 6;
                }
                continue_outer:;
            }

            // Final advance (normalize bitc before saving)
            bitp0 += (int)(bitc0 >> 3); bitc0 &= 7;
            bitp1 -= (int)(bitc1 >> 3); bitc1 &= 7;

            s.offs_u8 = offs_u8;
            s.neg_offs_s32 = neg_offs_s32;
            s.bitp[0] = (ulong)bitp0;
            s.bitp[1] = (ulong)(bitp1 + 8);
            s.bitc[0] = bitc0;
            s.bitc[1] = bitc1;
        }

        return newLZ_offset44_decode_finish(ref s);
    }

    private static int newLZ_offset44_decode_finish(ref KrakenOffsetState s)
    {
        byte* offs_u8 = s.offs_u8;
        byte* offs_u8_end = s.offs_u8_end;
        int* neg_offs_s32 = s.neg_offs_s32;

        byte* bitp0 = (byte*)s.bitp[0];
        byte* bitp1 = (byte*)s.bitp[1];
        uint bitc0 = s.bitc[0];
        uint bitc1 = s.bitc[1];

        if (bitp0 > bitp1) return 0;

        while (offs_u8 != offs_u8_end)
        {
            // Checkpoint input
            long bytes_left = bitp1 - bitp0;
            if (bytes_left < 8)
            {
                if (bytes_left < 0) return 0;
            }

            ulong bits0 = 0;
            ulong bits1 = 0;

            // Safe read for bits0 (big endian)
            for (int i = 0; i < 8; i++)
            {
                if (bitp0 + i < bitp1) bits0 |= (ulong)bitp0[i] << (56 - i * 8);
            }

            // Safe read for bits1 (little endian)
            for (int i = 0; i < 8; i++)
            {
                if (bitp1 - 8 + i >= bitp0) bits1 |= (ulong)(bitp1 - 8)[i] << (i * 8);
            }

            // Decode one
            byte packed = *offs_u8++;
            int offs;

            if (packed < 0xf0)
            {
                int num_raw = (packed >> 4) + OFFSET_RAW_BITS;
                bitc0 += (uint)num_raw;
                int mask = (1 << num_raw) - 1;
                int offs_raw_bits = (int)((bits0 >> (int)(64 - bitc0)) & (ulong)mask);
                offs = ((offs_raw_bits + mask) << 4) + (1 << 4) + (packed & 0xf) - (1 << (OFFSET_RAW_BITS + 4)) + NEWLZ_MIN_OFFSET;
            }
            else
            {
                int num_raw = (packed - 0xf0) + 16;
                if (num_raw >= 30) return 0;
                bitc0 += (uint)num_raw;
                int mask = (1 << num_raw) - 1;
                int offs_raw_bits = (int)((bits0 >> (int)(64 - bitc0)) & (ulong)mask);
                offs = offs_raw_bits + mask + 1 + ESCAPE_OFFSET_BIAS;
                if (offs >= NEWLZ_MAX_OFFSET) return 0;
            }
            *neg_offs_s32++ = -offs;

            if (offs_u8 != offs_u8_end)
            {
                packed = *offs_u8++;
                if (packed < 0xf0)
                {
                    int num_raw = (packed >> 4) + OFFSET_RAW_BITS;
                    bitc1 += (uint)num_raw;
                    int mask = (1 << num_raw) - 1;
                    int offs_raw_bits = (int)((bits1 >> (int)(64 - bitc1)) & (ulong)mask);
                    offs = ((offs_raw_bits + mask) << 4) + (1 << 4) + (packed & 0xf) - (1 << (OFFSET_RAW_BITS + 4)) + NEWLZ_MIN_OFFSET;
                }
                else
                {
                    int num_raw = (packed - 0xf0) + 16;
                    if (num_raw >= 30) return 0;
                    bitc1 += (uint)num_raw;
                    int mask = (1 << num_raw) - 1;
                    int offs_raw_bits = (int)((bits1 >> (int)(64 - bitc1)) & (ulong)mask);
                    offs = offs_raw_bits + mask + 1 + ESCAPE_OFFSET_BIAS;
                    if (offs >= NEWLZ_MAX_OFFSET) return 0;
                }
                *neg_offs_s32++ = -offs;
            }

            bitp0 += (int)(bitc0 >> 3); bitc0 &= 7;
            bitp1 -= (int)(bitc1 >> 3); bitc1 &= 7;
        }

        s.bitp[0] = (ulong)bitp0;
        s.bitp[1] = (ulong)bitp1;
        s.bitc[0] = bitc0;
        s.bitc[1] = bitc1;

        return 1;
    }

    private static int newLZ_offsetalt_decode64_tab(ref KrakenOffsetState s, uint offset_alt_modulo)
    {
        H.LogOodle("newLZ_offsetalt_decode64_tab called");
        long offs_count = s.offs_u8_end - s.offs_u8;
        H.LogOodle($"  offs_u8_count={offs_count} bitp0={(long)(byte*)s.bitp[0]:X} bitp1={(long)(byte*)s.bitp[1]:X} bitc0={s.bitc[0]} bitc1={s.bitc[1]}");
        if (offs_count == 10498)
        {
            // Dump bytes around 378 and 391
            H.LogOodle($"  offs_u8[385..395]={s.offs_u8[385]:X2} {s.offs_u8[386]:X2} {s.offs_u8[387]:X2} {s.offs_u8[388]:X2} {s.offs_u8[389]:X2} {s.offs_u8[390]:X2} {s.offs_u8[391]:X2} {s.offs_u8[392]:X2} {s.offs_u8[393]:X2} {s.offs_u8[394]:X2} {s.offs_u8[395]:X2}");
        }

        // TEST: Skip fast path entirely to use finish path only
        // return newLZ_offsetalt_decode_finish(ref s);

        byte* offs_u8 = s.offs_u8;
        byte* offs_u8_end = s.offs_u8_end;
        byte* offs_u8_start = offs_u8; // Track starting position for debug
        // SIMPLEPROFILE_SCOPE_N(offsetsalt_dec64_tab,offs_u8_end-offs_u8);

        if (offs_u8_end - offs_u8 >= 8 && (byte*)s.bitp[1] - (byte*)s.bitp[0] >= 8)
        {
            offs_u8_end -= 7;

            int* neg_offs_s32 = s.neg_offs_s32;
            byte* bitp0 = (byte*)s.bitp[0];
            byte* bitp1 = (byte*)s.bitp[1] - 8;
            uint bitc0 = s.bitc[0];
            uint bitc1 = s.bitc[1];

            while (offs_u8 < offs_u8_end)
            {
                // Advance
                bitp0 += bitc0 >> 3; bitc0 &= 7;
                bitp1 -= bitc1 >> 3; bitc1 &= 7;

                if (bitp0 > bitp1)
                    break;

                // Bit buffer refill
                ulong bits0 = H.ReadU64BE(bitp0);
                ulong bits1 = H.ReadU64LE(bitp1);

                // Check whether the next 6 offsets have <=18 extra bits
                const ulong splat8 = 0x0101010101010101UL; // ~0ull / 255
                const byte first_large_code = 19 << OFFSET_ALT_NUM_UNDER_BITS;

                ulong next_offsets = H.ReadU64LE(offs_u8);
                ulong next_offsets_hi = next_offsets & 0x808080808080UL; // MSB of the first 6 bytes

                ulong has_large_codes = ((first_large_code - 1) * splat8 - next_offsets) & next_offsets_hi;

                long current_idx = offs_u8 - offs_u8_start; // Current offset index for debugging
                bool log_detail = (offs_count == 10498 && current_idx >= 384 && current_idx < 396);

                if (has_large_codes == 0)
                {
                    long neg_offs_idx = neg_offs_s32 - s.neg_offs_s32;
                    bool log_chunk1_833 = (current_idx >= 828 && current_idx <= 840 && offs_count > 15000);
                    if (log_detail || log_chunk1_833) H.LogOodle($"decode offset batch [{current_idx}..{current_idx+5}]: next_offsets={next_offsets:X16} bitp0={(long)bitp0:X} bitp1={(long)bitp1:X} bitc0={bitc0} bitc1={bitc1} neg_offs_idx={neg_offs_idx}");
                    // All short (<=18 bits), can do 3 decodes without refill.

                    // 0
                    byte packed = (byte)(next_offsets);
                    uint num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc0 += num_raw;
                    uint offs_raw_bits = (uint)((bits0 >> (int)(64 - bitc0)) & ((1UL << (int)num_raw) - 1));
                    int neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    if (neg_offs < -3000000 || log_detail || log_chunk1_833) H.LogOodle($"  offset[{current_idx}]: packed=0x{packed:X2} num_raw={num_raw} offs_raw_bits={offs_raw_bits} bits0={bits0:X16} bitc0={bitc0} neg_offs={neg_offs}");
                    neg_offs_s32[0] = neg_offs;

                    // 1
                    packed = (byte)(next_offsets >> 8);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    uint bitc1_before = bitc1;
                    bitc1 += num_raw;
                    offs_raw_bits = (uint)((bits1 >> (int)(64 - bitc1)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    if (neg_offs < -3000000 || log_detail || log_chunk1_833) H.LogOodle($"  offset[{current_idx+1}]: packed=0x{packed:X2} num_raw={num_raw} offs_raw_bits={offs_raw_bits} bits1={bits1:X16} bitc1_before={bitc1_before} bitc1={bitc1} neg_offs={neg_offs}");
                    neg_offs_s32[1] = neg_offs;

                    // 2
                    packed = (byte)(next_offsets >> 16);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc0 += num_raw;
                    offs_raw_bits = (uint)((bits0 >> (int)(64 - bitc0)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    if (neg_offs < -3000000 || log_detail || log_chunk1_833) H.LogOodle($"  offset[{current_idx+2}]: packed=0x{packed:X2} num_raw={num_raw} offs_raw_bits={offs_raw_bits} bits0={bits0:X16} bitc0={bitc0} neg_offs={neg_offs}");
                    neg_offs_s32[2] = neg_offs;

                    // 3
                    packed = (byte)(next_offsets >> 24);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc1 += num_raw;
                    offs_raw_bits = (uint)((bits1 >> (int)(64 - bitc1)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    if (neg_offs < -3000000 || log_detail || log_chunk1_833) H.LogOodle($"  offset[{current_idx+3}]: packed=0x{packed:X2} num_raw={num_raw} offs_raw_bits={offs_raw_bits} bits1={bits1:X16} bitc1={bitc1} neg_offs={neg_offs}");
                    neg_offs_s32[3] = neg_offs;

                    // 4
                    packed = (byte)(next_offsets >> 32);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc0 += num_raw;
                    offs_raw_bits = (uint)((bits0 >> (int)(64 - bitc0)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    if (neg_offs < -3000000 || log_detail || log_chunk1_833) H.LogOodle($"  offset[{current_idx+4}]: packed=0x{packed:X2} num_raw={num_raw} offs_raw_bits={offs_raw_bits} bits0={bits0:X16} bitc0={bitc0} neg_offs={neg_offs}");
                    neg_offs_s32[4] = neg_offs;

                    // 5
                    packed = (byte)(next_offsets >> 40);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc1 += num_raw;
                    offs_raw_bits = (uint)((bits1 >> (int)(64 - bitc1)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    if (neg_offs < -3000000 || log_detail || log_chunk1_833) H.LogOodle($"  offset[{current_idx+5}]: packed=0x{packed:X2} num_raw={num_raw} offs_raw_bits={offs_raw_bits} bits1={bits1:X16} bitc1={bitc1} neg_offs={neg_offs}");
                    neg_offs_s32[5] = neg_offs;

                    offs_u8 += 6;
                    neg_offs_s32 += 6;
                }
                else
                {
                    // Max legal number of bits to consume is 26.
                    const byte first_illegal_code = 27 << OFFSET_ALT_NUM_UNDER_BITS;
                    ulong has_bad_codes = ((first_illegal_code - 1) * splat8 - next_offsets) & next_offsets_hi;
                    if (has_bad_codes != 0)
                    {
                        H.LogOodle($"newLZ_offsetalt_decode64_tab: has_bad_codes={has_bad_codes:X} next_offsets={next_offsets:X}");
                        return 0;
                    }

                    // DECONE 0
                    byte packed = (byte)(next_offsets);
                    uint num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc0 += num_raw;
                    uint offs_raw_bits = (uint)((bits0 >> (int)(64 - bitc0)) & ((1UL << (int)num_raw) - 1));
                    int neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    neg_offs_s32[0] = neg_offs;

                    // DECONE 1
                    packed = (byte)(next_offsets >> 8);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc1 += num_raw;
                    offs_raw_bits = (uint)((bits1 >> (int)(64 - bitc1)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    neg_offs_s32[1] = neg_offs;

                    if (bitc0 >= 64 - 26 || bitc1 >= 64 - 26)
                    {
                        offs_u8 += 2;
                        neg_offs_s32 += 2;
                        continue;
                    }

                    // DECONE 2
                    packed = (byte)(next_offsets >> 16);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc0 += num_raw;
                    offs_raw_bits = (uint)((bits0 >> (int)(64 - bitc0)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    neg_offs_s32[2] = neg_offs;

                    // DECONE 3
                    packed = (byte)(next_offsets >> 24);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc1 += num_raw;
                    offs_raw_bits = (uint)((bits1 >> (int)(64 - bitc1)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    neg_offs_s32[3] = neg_offs;

                    if (bitc0 >= 64 - 26 || bitc1 >= 64 - 26)
                    {
                        offs_u8 += 4;
                        neg_offs_s32 += 4;
                        continue;
                    }

                    // DECONE 4
                    packed = (byte)(next_offsets >> 32);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc0 += num_raw;
                    offs_raw_bits = (uint)((bits0 >> (int)(64 - bitc0)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    neg_offs_s32[4] = neg_offs;

                    // DECONE 5
                    packed = (byte)(next_offsets >> 40);
                    num_raw = (uint)(packed >> OFFSET_ALT_NUM_UNDER_BITS);
                    bitc1 += num_raw;
                    offs_raw_bits = (uint)((bits1 >> (int)(64 - bitc1)) & ((1UL << (int)num_raw) - 1));
                    neg_offs = newlz_offsetsalt_bias_tab[packed] - (int)offs_raw_bits;
                    neg_offs_s32[5] = neg_offs;

                    offs_u8 += 6;
                    neg_offs_s32 += 6;
                }
            }

            s.offs_u8 = offs_u8;
            s.neg_offs_s32 = neg_offs_s32;

            // Final advance
            bitp0 += bitc0 >> 3; bitc0 &= 7;
            bitp1 -= bitc1 >> 3; bitc1 &= 7;

            s.bitp[0] = (ulong)bitp0;
            s.bitp[1] = (ulong)(bitp1 + 8);
            s.bitc[0] = bitc0;
            s.bitc[1] = bitc1;
        }

        return newLZ_offsetalt_decode_finish(ref s);
    }

    private static int newLZ_offsetalt_decode_finish(ref KrakenOffsetState s)
    {
        byte* offs_u8 = s.offs_u8;
        byte* offs_u8_end = s.offs_u8_end;
        int* neg_offs_s32 = s.neg_offs_s32;
        long total_count = offs_u8_end - offs_u8;
        ulong total_bits1_consumed = 0;  // Track total bits consumed from stream 1

        byte* bitp0 = (byte*)s.bitp[0];
        byte* bitp1 = (byte*)s.bitp[1];
        uint bitc0 = s.bitc[0];
        uint bitc1 = s.bitc[1];

        H.LogOodle($"newLZ_offsetalt_decode_finish: offs_u8={(long)offs_u8:X} offs_u8_end={(long)offs_u8_end:X} diff={offs_u8_end - offs_u8}");

        // Dump first 10 offs_u8 values for the 10498 case
        if (total_count == 10498) {
            H.LogOodle($"  offs_u8[0..9]={offs_u8[0]:X2} {offs_u8[1]:X2} {offs_u8[2]:X2} {offs_u8[3]:X2} {offs_u8[4]:X2} {offs_u8[5]:X2} {offs_u8[6]:X2} {offs_u8[7]:X2} {offs_u8[8]:X2} {offs_u8[9]:X2}");
        }

        if (bitp0 > bitp1)
        {
            H.LogOodle($"newLZ_offsetalt_decode_finish: bitp0 > bitp1 ({((long)bitp0):X} > {((long)bitp1):X})");
            return 0;
        }

        while (offs_u8 != offs_u8_end)
        {
            // Checkpoint input
            long bytes_left = bitp1 - bitp0;
            if (bytes_left < 8)
            {
                // Tail handling not fully implemented, but should be rare/handled by caller padding
                // For now, fail if we run out of buffer space
                // In C++ code, it copies to a temp buffer.
                // Let's try to read carefully.
                if (bytes_left < 0)
                {
                    H.LogOodle($"newLZ_offsetalt_decode_finish: bytes_left < 0 ({bytes_left})");
                    return 0;
                }
            }

            ulong bits0 = 0;
            ulong bits1 = 0;

            // Safe read for bits0 (big endian)
            for (int i = 0; i < 8; i++)
            {
                if (bitp0 + i < bitp1) bits0 |= (ulong)bitp0[i] << (56 - i * 8);
            }

            // Safe read for bits1 (little endian)
            for (int i = 0; i < 8; i++)
            {
                if (bitp1 - 8 + i >= bitp0) bits1 |= (ulong)(bitp1 - 8)[i] << (i * 8);
            }

            // Decode one
            byte packed = *offs_u8++;
            int num_raw = packed >> OFFSET_ALT_NUM_UNDER_BITS;
            if (num_raw > 26)
            {
                H.LogOodle($"newLZ_offsetalt_decode_finish: num_raw > 26 ({num_raw})");
                return 0;
            }

            bitc0 += (uint)num_raw;
            int offs_lo_bits = (int)((bits0 >> (int)(64 - bitc0)) & ((1UL << num_raw) - 1));
            // Use bias table like the fast path - table already encapsulates high bits
            int neg_offs0 = newlz_offsetsalt_bias_tab[packed] - offs_lo_bits;
            // Log even offsets (from bits0) for the chunk with 10498 offsets
            long offs_idx0 = neg_offs_s32 - s.neg_offs_s32;
            if (total_count == 15805 && (offs_idx0 >= 830 && offs_idx0 <= 836)) {
                H.LogOodle($"FINISH15805 offset[{offs_idx0}]: packed=0x{packed:X2} num_raw={num_raw} bits0={bits0:X16} bitc0={bitc0} offs_lo={offs_lo_bits} neg_offs={neg_offs0}");
            }
            *neg_offs_s32++ = neg_offs0;

            if (offs_u8 != offs_u8_end)
            {
                long offs_idx = neg_offs_s32 - s.neg_offs_s32;
                packed = *offs_u8++;
                num_raw = packed >> OFFSET_ALT_NUM_UNDER_BITS;
                if (num_raw > 26)
                {
                    H.LogOodle($"newLZ_offsetalt_decode_finish: num_raw (2) > 26 ({num_raw})");
                    return 0;
                }

                uint bitc1_before = bitc1;
                bitc1 += (uint)num_raw;
                total_bits1_consumed += (uint)num_raw;
                offs_lo_bits = (int)((bits1 >> (int)(64 - bitc1)) & ((1UL << num_raw) - 1));
                // Use bias table like the fast path - table already encapsulates high bits
                int neg_offs = newlz_offsetsalt_bias_tab[packed] - offs_lo_bits;
                // Log offsets 831-835 for the chunk with 15805 offsets
                if (total_count == 15805 && (offs_idx >= 831 && offs_idx <= 837)) {
                    H.LogOodle($"FINISH15805 offset[{offs_idx}]: packed=0x{packed:X2} num_raw={num_raw} bits1={bits1:X16} bitc1_before={bitc1_before} bitc1={bitc1} offs_lo={offs_lo_bits} neg_offs={neg_offs}");
                }
                if (neg_offs < -3000000) {
                    byte* raw = bitp1 - 8;
                    H.LogOodle($"FINISH offset[{offs_idx}]: packed=0x{packed:X2} num_raw={num_raw} bits1={bits1:X16} bitc1_before={bitc1_before} bitc1={bitc1} offs_lo={offs_lo_bits} neg_offs={neg_offs} bitp1={(long)bitp1:X} raw_bytes={raw[0]:X2}{raw[1]:X2}{raw[2]:X2}{raw[3]:X2}{raw[4]:X2}{raw[5]:X2}{raw[6]:X2}{raw[7]:X2}");
                }
                *neg_offs_s32++ = neg_offs;
            }

            bitp0 += (int)(bitc0 >> 3); bitc0 &= 7;
            bitp1 -= (int)(bitc1 >> 3); bitc1 &= 7;
        }

        s.bitp[0] = (ulong)bitp0;
        s.bitp[1] = (ulong)bitp1;
        s.bitc[0] = bitc0;
        s.bitc[1] = bitc1;

        return 1;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void rrVarBits_RefillBack_Safe(ref rrVarBits vb)
    {
        int bitlen = 63 - vb.m_inv_bitlen;
        int bytes_consumed = (63 - bitlen) >> 3;
        
        // Read 8 bytes BEFORE current position (not from current position)
        if (vb.m_cur - 8 >= vb.m_end)
        {
            ulong next = *(ulong*)(vb.m_cur - 8);
            vb.m_bits |= next >> bitlen;
            vb.m_cur -= bytes_consumed;
            vb.m_inv_bitlen -= bytes_consumed << 3;
        }
        else
        {
            // Slow path: byte by byte when we're too close to the start
            while (bitlen <= 56)
            {
                if (vb.m_cur <= vb.m_end) break;
                vb.m_cur--;
                vb.m_bits |= (ulong)(*vb.m_cur) << (56 - bitlen);
                bitlen += 8;
                vb.m_inv_bitlen -= 8;
            }
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static uint rrVarBits_Get_V_NoRefill(ref rrVarBits vb, int count)
    {
        uint ret = (uint)(vb.m_bits >> (64 - count));
        vb.m_bits <<= count;
        vb.m_inv_bitlen += count;
        return ret;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static bool VARBITS_GET_EXPGOLOMB(ref rrVarBits vb, int rshift, out uint val)
    {
        ulong minval = (1UL << (63 - (18 - rshift)));
        if (vb.m_bits < minval)
        {
            val = 0;
            return false;
        }

        int cntLZ = BitOperations.LeadingZeroCount(vb.m_bits);
        int nBits = 2 * cntLZ + 1 + rshift;

        if ((63 - vb.m_inv_bitlen) < nBits)
        {
             val = 0;
             return false;
        }

        uint v = rrVarBits_Get_V_NoRefill(ref vb, nBits);
        val = v - (1U << rshift);
        return true;
    }

    private static long newLZ_get_excesses(ref rrVarBits vb1, ref rrVarBits vb2, uint* excesses_base, uint* excesses_end, long expected_count, byte* ptr_start, byte* ptr_end)
    {
        byte* vb1_init = ptr_start;
        byte* vb2_init = ptr_end;

        uint* excesses_ptr = excesses_base;

        while (vb2.m_cur > vb1.m_cur + 32)
        {
            if (excesses_ptr > excesses_end)
            {
                H.LogOodle("newLZ_get_excesses: excesses_ptr > excesses_end");
                return -1;
            }

            rrVarBits_Refill_Safe(ref vb1);
            rrVarBits_RefillBack_Safe(ref vb2);

            uint excess1, excess2;
            if (!VARBITS_GET_EXPGOLOMB(ref vb1, EXCESS_EXPGOLOMB_SHIFT, out excess1))
            {
                H.LogOodle("newLZ_get_excesses: VARBITS_GET_EXPGOLOMB 1 failed");
                return -1;
            }
            if (!VARBITS_GET_EXPGOLOMB(ref vb2, EXCESS_EXPGOLOMB_SHIFT, out excess2))
            {
                H.LogOodle("newLZ_get_excesses: VARBITS_GET_EXPGOLOMB 2 failed");
                return -1;
            }

            *excesses_ptr++ = excess1;
            *excesses_ptr++ = excess2;

            rrVarBits_Refill_Safe(ref vb1);
            rrVarBits_RefillBack_Safe(ref vb2);

            if (!VARBITS_GET_EXPGOLOMB(ref vb1, EXCESS_EXPGOLOMB_SHIFT, out excess1))
            {
                H.LogOodle("newLZ_get_excesses: VARBITS_GET_EXPGOLOMB 3 failed");
                return -1;
            }
            if (!VARBITS_GET_EXPGOLOMB(ref vb2, EXCESS_EXPGOLOMB_SHIFT, out excess2))
            {
                H.LogOodle("newLZ_get_excesses: VARBITS_GET_EXPGOLOMB 4 failed");
                return -1;
            }

            *excesses_ptr++ = excess1;
            *excesses_ptr++ = excess2;
        }

        // Tail handling
        // NOTE: rem_bytes can be negative when lvb1/lvb2 have crossed - this is valid!
        long rem_bytes = vb2.m_cur - vb1.m_cur;

        byte* tail_buf = stackalloc byte[128];

        byte* newf = tail_buf + 24; // 16 + 8

        long head_pad = Math.Min(vb1.m_cur - vb1_init, 8);
        long tail_pad = Math.Min(vb2_init - vb2.m_cur, 8);

        long copy_len = rem_bytes + head_pad + tail_pad;
        if (copy_len > 0)
        {
            Buffer.MemoryCopy(vb1.m_cur - head_pad, newf - head_pad, 128, copy_len);
        }

        vb1.m_cur = newf;
        vb2.m_cur = newf + rem_bytes; // rem_bytes can be negative!
        vb1.m_end = tail_buf + 128;
        vb2.m_end = tail_buf;

        while (excesses_ptr < excesses_base + expected_count)
        {
             if (excesses_ptr >= excesses_end) return -1;

             byte* vb1_cons = vb1.m_cur - ((63 - vb1.m_inv_bitlen + 7) >> 3);
             byte* vb2_cons = vb2.m_cur + ((63 - vb2.m_inv_bitlen + 7) >> 3);

             if (vb1_cons > vb2_cons) break;

             rrVarBits_Refill_Safe(ref vb1);

             uint excess1;
             if (!VARBITS_GET_EXPGOLOMB(ref vb1, EXCESS_EXPGOLOMB_SHIFT, out excess1)) break;

             vb1_cons = vb1.m_cur - ((63 - vb1.m_inv_bitlen + 7) >> 3);
             if (vb1_cons > vb2_cons) break;

             *excesses_ptr++ = excess1;
             if (excesses_ptr >= excesses_base + expected_count) break;

             if (vb1_cons > vb2_cons) break;

             rrVarBits_RefillBack_Safe(ref vb2);

             uint excess2;
             if (!VARBITS_GET_EXPGOLOMB(ref vb2, EXCESS_EXPGOLOMB_SHIFT, out excess2)) break;

             vb2_cons = vb2.m_cur + ((63 - vb2.m_inv_bitlen + 7) >> 3);
             if (vb1_cons > vb2_cons) break;

             *excesses_ptr++ = excess2;
        }

        return excesses_ptr - excesses_base;
    }

    private const int EXCESS_EXPGOLOMB_SHIFT = 6;

    private static int newLZ_get_offsets_excesses(byte* comp_ptr, byte* chunk_comp_end, byte* offsets_u8, byte* offsets_u8_2, long offsets_count, uint offset_alt_modulo, byte* excesses_u8, long excesses_count, int* offsets, uint* excesses, long end_of_chunk_pos, byte excess_hdr_byte, int excess_stream_bytes)
    {
        // H.LogOodle($"newLZ_get_offsets_excesses: comp_ptr={(long)comp_ptr:X} chunk_comp_end={(long)chunk_comp_end:X} diff={chunk_comp_end - comp_ptr} offsets_count={offsets_count} excesses_count={excesses_count}");
        if (comp_ptr >= chunk_comp_end && excess_hdr_byte == 0)
        {
             H.LogOodle($"newLZ_get_offsets_excesses: comp_ptr >= chunk_comp_end ({chunk_comp_end - comp_ptr}) and excess_hdr_byte == 0");
             return -1;
        }
        if (excess_stream_bytes > (chunk_comp_end - comp_ptr))
        {
             H.LogOodle($"newLZ_get_offsets_excesses: excess_stream_bytes ({excess_stream_bytes}) > diff ({chunk_comp_end - comp_ptr})");
             return -1;
        }

        byte* ptr_start = comp_ptr;
        byte* ptr_end = chunk_comp_end - excess_stream_bytes;

        rrVarBits lvb1 = new rrVarBits();
        rrVarBits lvb2 = new rrVarBits();

        rrVarBits_GetOpen(ref lvb1, ptr_start, ptr_end);

        // Setup lvb2 as backward reader
        lvb2.m_cur = ptr_end;
        lvb2.m_end = ptr_start;
        lvb2.m_bits = 0;
        lvb2.m_inv_bitlen = 63;

        rrVarBits_Refill_Safe(ref lvb1);
        rrVarBits_RefillBack_Safe(ref lvb2);

        long excess_u32_count = 0;

        if (excess_hdr_byte == 0)
        {
            // Old encoding - use EXCESS_U32_COUNT_EXPGOLOMB_SHIFT = 0 (Elias Gamma)
            uint val;
            if (!VARBITS_GET_EXPGOLOMB(ref lvb2, 0, out val))
            {
                H.LogOodle("newLZ_get_offsets_excesses: VARBITS_GET_EXPGOLOMB failed for excess_u32_count");
                return -1;
            }
            excess_u32_count = val;

            rrVarBits_RefillBack_Safe(ref lvb2);
        }

        if (offsets_count > 0)
        {
            KrakenOffsetState s = new KrakenOffsetState();
            s.offs_u8 = offsets_u8;
            s.offs_u8_end = offsets_u8 + offsets_count;
            s.neg_offs_s32 = offsets;

            // Debug: print u8 values around 833 if enough offsets
            if (offsets_count >= 835)
            {
                H.LogOodle($"Before offset decode, offs_u8[832..834]=0x{offsets_u8[832]:X2} 0x{offsets_u8[833]:X2} 0x{offsets_u8[834]:X2}");
            }

            long len1 = 63 - lvb1.m_inv_bitlen;
            long len2 = 63 - lvb2.m_inv_bitlen;
            s.bitp[0] = (ulong)(lvb1.m_cur - ((len1 + 7) >> 3));
            s.bitp[1] = (ulong)(lvb2.m_cur + ((len2 + 7) >> 3));
            s.bitc[0] = (uint)((0 - len1) & 7);
            s.bitc[1] = (uint)((0 - len2) & 7);

            if (offsets_count == 10498) {
                H.LogOodle($"Offset init (10498): len1={len1} len2={len2} lvb1.m_cur={((long)lvb1.m_cur):X} lvb2.m_cur={((long)lvb2.m_cur):X} bitp0={s.bitp[0]:X} bitp1={s.bitp[1]:X} bitc0={s.bitc[0]} bitc1={s.bitc[1]}");
                // Dump first bytes of bitstream
                byte* bp0 = (byte*)s.bitp[0];
                byte* bp1 = (byte*)s.bitp[1];
                H.LogOodle($"  bitp0 bytes: {bp0[0]:X2} {bp0[1]:X2} {bp0[2]:X2} {bp0[3]:X2} {bp0[4]:X2} {bp0[5]:X2} {bp0[6]:X2} {bp0[7]:X2}");
                H.LogOodle($"  bitp1-8 bytes: {(bp1-8)[0]:X2} {(bp1-8)[1]:X2} {(bp1-8)[2]:X2} {(bp1-8)[3]:X2} {(bp1-8)[4]:X2} {(bp1-8)[5]:X2} {(bp1-8)[6]:X2} {(bp1-8)[7]:X2}");
            }

            int decode_ok = 0;
            if (offset_alt_modulo == 0)
            {
                decode_ok = newLZ_offset44_decode64_tab(ref s);
            }
            else
            {
                decode_ok = newLZ_offsetalt_decode64_tab(ref s, offset_alt_modulo);
            }

            // After offset decoding, apply modulo multiplication if offset_alt_modulo > 1
            // This is: offset[i] = offset[i] * modulo - offsets_u8_2[i]
            if (offset_alt_modulo > 1 && decode_ok != 0)
            {
                for (long i = 0; i < offsets_count; i++)
                {
                    offsets[i] = offsets[i] * (int)offset_alt_modulo - (int)offsets_u8_2[i];
                }
            }

            // Verify offsets after decoding - log offset around index 126
            if (offsets_count > 126)
            {
                H.LogOodle($"After offset decode, offsets[125..128]={offsets[125]},{offsets[126]},{offsets[127]},{(offsets_count > 128 ? offsets[128].ToString() : "N/A")} count={offsets_count}");
                H.LogOodle($"After offset decode, offs_u8[125..128]=0x{offsets_u8[125]:X2},0x{offsets_u8[126]:X2},0x{offsets_u8[127]:X2}");
            }

            if (decode_ok == 0)
            {
                H.LogOodle("newLZ_get_offsets_excesses: newLZ_offset44_decode64_tab failed");
                return -1;
            }

            if (excess_hdr_byte == 0)
            {
                rrVarBits_GetOpen(ref lvb1, (byte*)s.bitp[0], ptr_end);
                if (s.bitc[0] > 0)
                {
                    rrVarBits_Get_V(ref lvb1, (uint)s.bitc[0]);
                }

                // Restore lvb2
                lvb2.m_cur = (byte*)s.bitp[1];
                lvb2.m_bits = 0;
                lvb2.m_inv_bitlen = 63;

                rrVarBits_RefillBack_Safe(ref lvb2);
                if (s.bitc[1] > 0)
                {
                    rrVarBits_Get_V_NoRefill(ref lvb2, (int)s.bitc[1]);
                }
            }
        }

        // Excesses
        if (excess_hdr_byte != 0)
        {
            if (excess_stream_bytes != 0)
            {
                ptr_start = chunk_comp_end - excess_stream_bytes;
                ptr_end = chunk_comp_end;

                rrVarBits_GetOpen(ref lvb1, ptr_start, ptr_end);

                lvb2.m_cur = ptr_end;
                lvb2.m_end = ptr_start;
                lvb2.m_bits = 0;
                lvb2.m_inv_bitlen = 63;
                rrVarBits_RefillBack_Safe(ref lvb2);
                // New framing stores the escaped excess count in the byte
                // array (0xff entries), not the old tail gamma prefix.
                for (long i = 0; i < excesses_count; ++i)
                    if (excesses_u8[i] == 255) ++excess_u32_count;
            }
        }

        if (excess_u32_count > 0 || excess_stream_bytes > 0)
        {
            H.LogOodle($"newLZ_get_offsets_excesses: excess_u32_count={excess_u32_count} excess_stream_bytes={excess_stream_bytes}");
            uint* excesses_u32_arr = stackalloc uint[512 + 8];
            uint* excesses_u32_base = excesses_u32_arr;

            long decoded_count = newLZ_get_excesses(ref lvb1, ref lvb2, excesses_u32_base, excesses_u32_base + 512, excess_u32_count, ptr_start, ptr_end);
            if (decoded_count < 0 || decoded_count != excess_u32_count)
            {
                H.LogOodle($"newLZ_get_offsets_excesses: newLZ_get_excesses failed decoded={decoded_count} expected={excess_u32_count}");
                return -1;
            }

            // Merge
            byte* excesses_u8_ptr = excesses_u8;
            uint* excesses_ptr = excesses;
            uint* excesses_ptr_end = excesses + excesses_count;
            uint* excesses_u32_ptr = excesses_u32_base;

            while (excesses_ptr < excesses_ptr_end)
            {
                byte val = *excesses_u8_ptr++;
                uint exc = (uint)val + 3; // NEWLZ_PACKET_LRL_MAX

                if (val == 255)
                {
                    if (excesses_u32_ptr >= excesses_u32_base + decoded_count) return -1;
                    exc += *excesses_u32_ptr++;
                }

                *excesses_ptr++ = exc;
            }
        }
        else
        {
            byte* excesses_u8_ptr = excesses_u8;
            uint* excesses_ptr = excesses;
            uint* excesses_ptr_end = excesses + excesses_count;

            while (excesses_ptr < excesses_ptr_end)
            {
                *excesses_ptr++ = (uint)(*excesses_u8_ptr++) + 3;
            }
        }

        return 1;
    }


    [StructLayout(LayoutKind.Sequential)]
    private struct OodleLZDecoder
    {
        public long decPos;
        public long decLen;
        public long resetPos;
        public LZBlockHeader header;
        public long gotHeaderPos;
        public int callsWithoutProgress;
        public int memorySize;
        public byte* memory; // scratch memory (native allocated)
    }

    // Main Function
    public static int Decompress(ReadOnlySpan<byte> source, Span<byte> destination)
    {
        fixed (byte* srcPtr = source)
        fixed (byte* dstPtr = destination)
        {
            return (int)OodleLZ_Decompress(srcPtr, source.Length, dstPtr, destination.Length);
        }
    }

    public static bool TryDecompress(ReadOnlySpan<byte> source, Span<byte> destination, out int bytesWritten)
    {
        fixed (byte* srcPtr = source)
        fixed (byte* dstPtr = destination)
        {
            bytesWritten = (int)OodleLZ_Decompress(srcPtr, source.Length, dstPtr, destination.Length);
            return bytesWritten > 0;
        }
    }

    private static long OodleLZ_Decompress(byte* compBuf, long compBufSize, byte* rawBuf, long rawLen)
    {
        if (rawLen <= 0) return 0;

        // OodleLZ_GetAllChunksCompressor logic (simplified)
        OodleLZ_Compressor compressor = OodleLZ_GetFirstChunkCompressor(compBuf, compBufSize);
        if (compressor == OodleLZ_Compressor.OodleLZ_Compressor_Invalid) return 0;

        // Use thread-local scratch pool to avoid allocation overhead
        if (_scratchPool is null)
        {
            _scratchPool = (byte*)NativeMemory.AlignedAlloc(SCRATCH_POOL_SIZE, (nuint)nint.Size);
        }

        // Decoder setup
        OodleLZDecoder decoder = new OodleLZDecoder();
        decoder.decLen = rawLen;
        decoder.decPos = 0;
        decoder.resetPos = 0;
        decoder.gotHeaderPos = -1; // Invalid initially
        decoder.callsWithoutProgress = 0;
        decoder.memorySize = SCRATCH_POOL_SIZE;
        decoder.memory = _scratchPool;

        long rawDecoded = 0;
        long compUsed = 0;

        while (rawDecoded < rawLen)
        {
            if (compUsed >= compBufSize) break;

            bool result = OodleLZDecoder_DecodeSome(
                ref decoder,
                rawBuf,
                rawDecoded,
                rawLen, // decBufferSize
                rawLen - rawDecoded, // decBufAvail
                compBuf + compUsed,
                compBufSize - compUsed,
                out long decodedCount,
                out long compConsumed
            );

            if (!result) break;
            if (decodedCount == 0 && compConsumed == 0) break; // No progress

            rawDecoded += decodedCount;
            compUsed += compConsumed;
        }

        return rawDecoded == rawLen ? rawDecoded : 0;
    }

    private static bool OodleLZDecoder_DecodeSome(
        ref OodleLZDecoder decoder,
        byte* decBuf,
        long decBufPos,
        long decBufferSize,
        long decBufAvail,
        byte* compBuf,
        long compBufAvail,
        out long decodedCount,
        out long compBufUsed)
    {
        decodedCount = 0;
        compBufUsed = 0;

        if (++decoder.callsWithoutProgress >= 1000) return false;

        long chunkPos = decoder.decPos & (OODLELZ_BLOCK_LEN - 1);
        long rawBytesToGo = Math.Min(OODLELZ_BLOCK_LEN, Math.Min(decBufferSize - decBufPos, decBufAvail));

        rawBytesToGo = Math.Min(rawBytesToGo, OODLELZ_BLOCK_LEN - chunkPos);
        rawBytesToGo = Math.Min(rawBytesToGo, decoder.decLen - decoder.decPos);

        if (rawBytesToGo == 0) return true;
        if (compBufAvail == 0) return true;

        byte* compPtr = compBuf;
        byte* compPtrEnd = compBuf + compBufAvail;

        // Block Header
        if (chunkPos == 0 && decoder.gotHeaderPos != decoder.decPos)
        {
            if (compBufAvail < OODLELZ_BLOCK_HEADER_BYTES_MAX) return true; // Wait for more data

            decoder.gotHeaderPos = decoder.decPos;
            compPtr = LZBlockHeader_Get(ref decoder.header, compPtr, compBufAvail);
            if (compPtr == null)
            {
                return false;
            }

            if (decoder.header.chunkIsReset)
            {
                decoder.resetPos = decoder.decPos;
            }
        }

        byte* decPtrStart = decBuf + decBufPos;
        byte* decPtr = decPtrStart;

        if (decoder.header.chunkIsMemcpy)
        {
            long compAvail = compPtrEnd - compPtr;
            if (rawBytesToGo > compAvail)
            {
                compBufUsed = compPtr - compBuf;
                return true; // Wait for more data
            }

            Buffer.MemoryCopy(compPtr, decPtr, rawBytesToGo, rawBytesToGo);

            decPtr += rawBytesToGo;
            compPtr += rawBytesToGo;
            decoder.decPos += rawBytesToGo;
            decoder.callsWithoutProgress = 0;

            decodedCount = rawBytesToGo;
            compBufUsed = compPtr - compBuf;
            return true;
        }
        else
        {
            // For large quantum types (Kraken, Mermaid, Leviathan), the quantum can be the entire block
            // For small quantum types, cap to 16KB
            long qh_rawLen;
            if (LZ_DecodeType_IsLargeQuantum(decoder.header.decodeType))
            {
                qh_rawLen = rawBytesToGo; // Can be up to OODLELZ_BLOCK_LEN (256KB)
            }
            else
            {
                qh_rawLen = Math.Min(rawBytesToGo, OODLELZ_QUANTUM_LEN); // Cap to 16KB
            }

            LZQuantumHeader qh = new LZQuantumHeader();

            int lzqhLen;
            if (LZ_DecodeType_IsLargeQuantum(decoder.header.decodeType))
            {
                lzqhLen = LZLargeQuantumHeader_Get(compPtr, compPtrEnd, ref qh, decoder.header.chunkHasQuantumCRCs, (int)qh_rawLen);
            }
            else
            {
                lzqhLen = LZQuantumHeader_Get(compPtr, compPtrEnd, ref qh, decoder.header.chunkHasQuantumCRCs, (int)qh_rawLen);
            }

            if (lzqhLen <= 0)
            {
                if ((compPtrEnd - compPtr) < OODLELZ_QUANTUM_HEADER_MAX_SIZE)
                {
                    compBufUsed = compPtr - compBuf;
                    return true; // Wait for more data
                }
                return false; // Corrupt
            }

            compPtr += lzqhLen;

            if (qh.compLen > (compPtrEnd - compPtr))
            {
                compBufUsed = compPtr - compBuf - lzqhLen; // Backtrack
                return true; // Wait for more data
            }

            byte* compPtrQuantumEnd = compPtr + qh.compLen;

            // Decompression Logic
            if (qh.compLen == qh_rawLen)
            {
                // Memcpy quantum
                Buffer.MemoryCopy(compPtr, decPtr, qh_rawLen, qh_rawLen);
                decPtr += qh_rawLen;
                compPtr += qh_rawLen;
                decoder.decPos += qh_rawLen;
                decoder.callsWithoutProgress = 0;
                decodedCount = qh_rawLen;
                compBufUsed = compPtr - compBuf;
                return true;
            }
            else if (qh.compLen == 0) // Memset
            {
                 // Simplified memset (using crc as value for special case)
                 // Real implementation handles more cases
                 int memsetVal = (int)qh.crc; // Simplified
                 // TODO: Handle legacy memset val if needed

                 H.FillBytes(decPtr, (byte)memsetVal, (int)qh_rawLen);

                 decPtr += qh_rawLen;
                 decoder.decPos += qh_rawLen;
                 decoder.callsWithoutProgress = 0;
                 decodedCount = qh_rawLen;
                 compBufUsed = compPtr - compBuf;
                 return true;
            }
            else
            {
                // Actual Decompression
                byte* scratch = decoder.memory;
                int ret = 0;
                if (decoder.header.decodeType == RAD_LZ_DECODE_MERMAID)
                {
                    // Mermaid also handles Selkie
                    ret = Mermaid_DecodeOneQuantum(decPtr, decPtr + qh_rawLen, compPtr, qh.compLen, compPtrEnd, decoder.decPos, scratch, decoder.memorySize);
                }
                else if (decoder.header.decodeType == RAD_LZ_DECODE_KRAKEN)
                {
                    ret = Kraken_DecodeOneQuantum(decPtr, decPtr + qh_rawLen, compPtr, qh.compLen, compPtrEnd, decoder.decPos, scratch, decoder.memorySize);
                }
                else if (decoder.header.decodeType == RAD_LZ_DECODE_LEVIATHAN)
                {
                    ret = Leviathan_DecodeOneQuantum(decPtr, decPtr + qh_rawLen, compPtr, qh.compLen, compPtrEnd, decoder.decPos, scratch, decoder.memorySize);
                }
                else if (decoder.header.decodeType == RAD_LZ_DECODE_LZB16)
                {
                    ret = LZB16_DecodeOneQuantum(decPtr, decPtr + qh_rawLen, compPtr, compPtrEnd, decBuf);
                }
                else
                {
                    // Unsupported decode type (legacy decoders: LZHLW, LZNIB, LZBLW, LZA, LZNA, LZH, BitKnit)
                    return false;
                }

                if (ret != qh_rawLen)
                {
                    return false;
                }

                decPtr += qh_rawLen;
                compPtr += qh.compLen;
                decoder.decPos += qh_rawLen;
                decoder.callsWithoutProgress = 0;
                decodedCount = qh_rawLen;
                compBufUsed = compPtr - compBuf;
                return true;
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct newLZ_LOs
    {
        public fixed int lasts[8]; // 4 padding + 4 actual values at indices 4-7

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Reset()
        {
            lasts[4] = NEWLZ_MIN_OFFSET;
            lasts[5] = NEWLZ_MIN_OFFSET;
            lasts[6] = NEWLZ_MIN_OFFSET;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Reset_Neg()
        {
            lasts[4] = -NEWLZ_MIN_OFFSET;
            lasts[5] = -NEWLZ_MIN_OFFSET;
            lasts[6] = -NEWLZ_MIN_OFFSET;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public int MTF4(int index)
        {
            // Direct indexed access - JIT optimizes fixed arrays well
            int top = lasts[4 + index];
            // Unrolled shift - compiler can optimize based on index
            if (index >= 3) lasts[7] = lasts[6];
            if (index >= 2) lasts[6] = lasts[5];
            if (index >= 1) lasts[5] = lasts[4];
            lasts[4] = top;
            return top;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Add(int offset)
        {
            lasts[7] = offset;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public int LastOffset()
        {
            return lasts[4];
        }
    }

    private static int Mermaid_DecodeOneQuantum(byte* decomp, byte* decomp_end, byte* comp, int quantumCompLen, byte* compBufEnd, long pos_since_reset, byte* scratch, int scratch_size)
    {
        H.LogOodle($"DecodeOneQuantum: decomp_len={decomp_end - decomp} comp_len={quantumCompLen}");
        long decomp_len = decomp_end - decomp;
        byte* scratch_ptr = scratch;
        byte* scratch_end = scratch + scratch_size;

        byte* rawPtr = decomp;
        byte* rawEnd = decomp_end;
        byte* compPtr = comp;
        byte* compEnd = comp + quantumCompLen;

        while (rawPtr < rawEnd)
        {
            long chunk_len = Math.Min(NEWLZF_CHUNK_LEN, rawEnd - rawPtr);
            long chunk_pos = (rawPtr - decomp) + pos_since_reset;

            if (compEnd - compPtr < 4) return -1;

            int chunk_comp_len = (int)H.ReadU24BE(compPtr);

            H.LogOodle($"Chunk header bytes: {compPtr[0]:X2} {compPtr[1]:X2} {compPtr[2]:X2} raw_header={chunk_comp_len:X6}");

            if (chunk_comp_len >= (1 << 23))
            {
                int chunk_type = (chunk_comp_len >> 19) & 0xF;
                chunk_comp_len &= (1 << 19) - 1;
                compPtr += 3;

                H.LogOodle($"Chunk: type={chunk_type} len={chunk_len} comp_len={chunk_comp_len} pos={chunk_pos}");

                if (chunk_comp_len > compEnd - compPtr) return -1;

                byte* chunk_comp_end = compPtr + chunk_comp_len;

                if (chunk_comp_len >= chunk_len)
                {
                    if (chunk_comp_len > chunk_len) return -1;
                    if (chunk_type != 0) return -1;

                    Buffer.MemoryCopy(compPtr, rawPtr, chunk_len, chunk_len);
                }
                else
                {
                    if (chunk_len < NEWLZF_MIN_CHUNK_LEN) return -1;

                    if (scratch_size < chunk_len + 256 * 1024) return -1;

                    newLZF_chunk_arrays arrays = new newLZF_chunk_arrays();
                    arrays.chunk_ptr = rawPtr;
                    arrays.scratch_ptr = scratch_ptr;

                    if (newLZF_decode_chunk_phase1(chunk_type, compPtr, chunk_comp_end, rawPtr, chunk_len, chunk_pos, scratch_ptr, scratch_end, ref arrays) < 0)
                    {
                        H.LogOodle("newLZF_decode_chunk_phase1 failed");
                        return -1;
                    }

                    if (newLZF_decode_chunk_phase2(chunk_type, compPtr, chunk_comp_end, rawPtr, chunk_len, chunk_pos, decomp_end, ref arrays) < 0)
                    {
                        H.LogOodle("newLZF_decode_chunk_phase2 failed");
                        return -1;
                    }

                    // Debug: check what's at byte 936 after decoding this chunk
                    if (chunk_pos == 0 && chunk_len > 940)
                    {
                        H.LogOodle($"DEBUG: After first chunk, bytes at 936: {rawPtr[936]:X2} {rawPtr[937]:X2} {rawPtr[938]:X2} {rawPtr[939]:X2} {rawPtr[940]:X2} {rawPtr[941]:X2} {rawPtr[942]:X2} {rawPtr[943]:X2}");
                    }
                }

                compPtr += chunk_comp_len;
                rawPtr += chunk_len;
            }
            else
            {
                H.LogOodle($"Chunk: HUFF (not compressed with packet format) len={chunk_len}");
                H.LogOodle($"Chunk header bytes: {compPtr[0]:X2} {compPtr[1]:X2} {compPtr[2]:X2} {compPtr[3]:X2}");
                long literals_len = 0;

                bool inplace_comp_raw_overlap = (compPtr <= (rawPtr + chunk_len) && rawPtr <= compEnd);

                if (inplace_comp_raw_overlap)
                {
                    // decode into scratch and then copy
                    byte* literals = scratch_ptr;
                    long literals_comp_len = newLZ_get_array(&literals, compPtr, compEnd, &literals_len, chunk_len, false, scratch_ptr, scratch_end);
                    H.LogOodle($"newLZ_get_array returned: comp_len={literals_comp_len} literals_len={literals_len}");
                    if (literals_comp_len < 0 || literals_len != chunk_len)
                    {
                        H.LogOodle($"FAILED: literals_comp_len={literals_comp_len} literals_len={literals_len} chunk_len={chunk_len}");
                        return -1;
                    }
                    if (literals != scratch_ptr) // got uncompressed data when expecting compressed
                        return -1;
                    Buffer.MemoryCopy(scratch_ptr, rawPtr, chunk_len, chunk_len);
                    compPtr += literals_comp_len;
                }
                else
                {
                    byte* literals = rawPtr;
                    long literals_comp_len = newLZ_get_array(&literals, compPtr, compEnd, &literals_len, chunk_len, false, scratch_ptr, scratch_end);
                    H.LogOodle($"newLZ_get_array returned: comp_len={literals_comp_len} literals_len={literals_len}");
                    if (literals_comp_len < 0 || literals_len != chunk_len)
                    {
                        H.LogOodle($"FAILED: literals_comp_len={literals_comp_len} literals_len={literals_len} chunk_len={chunk_len}");
                        return -1;
                    }
                    if (literals != rawPtr) // got uncompressed data when expecting compressed
                        return -1;
                    compPtr += literals_comp_len;
                }

                rawPtr += chunk_len;
            }
        }

        return (int)decomp_len;
    }

    private static int newLZF_decode_chunk_phase1(int chunk_type, byte* comp, byte* comp_end, byte* chunk_ptr, long chunk_len, long chunk_pos, byte* scratch_space, byte* scratch_end, ref newLZF_chunk_arrays arrays)
    {
        if (chunk_type > 1) { Console.Error.WriteLine("P1FAIL:type"); return -1; }

        bool inplace_comp_raw_overlap = (comp <= (chunk_ptr + chunk_len) && chunk_ptr <= comp_end);
        byte* comp_ptr = comp;

        if (comp_end - comp_ptr < 10) return -1;

        byte* to_ptr = chunk_ptr;
        if (chunk_pos == 0)
        {
            *(ulong*)to_ptr = *(ulong*)comp_ptr;
            to_ptr += 8;
            comp_ptr += 8;
        }

        byte* scratch_ptr = scratch_space;

        H.LogOodle($"Phase1: comp_ptr offset from comp_base = {comp_ptr - comp}");

        byte* literals = scratch_ptr;
        long literals_count;
        long literals_comp_len = newLZ_get_array(&literals, comp_ptr, comp_end, &literals_count, Math.Min(chunk_len, scratch_end - scratch_ptr), inplace_comp_raw_overlap, scratch_ptr, scratch_end);
        if (literals_comp_len < 0) return -1;
        comp_ptr += literals_comp_len;
        scratch_ptr += literals_count;

        H.LogOodle($"Phase1: literals_count={literals_count} literals_comp_len={literals_comp_len} first8={literals[0]:X2} {literals[1]:X2} {literals[2]:X2} {literals[3]:X2} {literals[4]:X2} {literals[5]:X2} {literals[6]:X2} {literals[7]:X2}");

        arrays.literals_ptr = literals;
        arrays.literals_end = literals + literals_count;

        byte* packets_base = scratch_ptr;
        long packets_count;
        long packets_comp_len = newLZ_get_array(&packets_base, comp_ptr, comp_end, &packets_count, Math.Min(chunk_len, scratch_end - scratch_ptr), inplace_comp_raw_overlap, scratch_ptr, scratch_end);
        if (packets_comp_len < 0) return -1;
        comp_ptr += packets_comp_len;
        scratch_ptr += packets_count;

        arrays.packets_ptr = packets_base;
        arrays.packets_count = packets_count;

        if (chunk_len > (1 << 16))
        {
            if (comp_ptr + 2 > comp_end) return -1;
            arrays.packets_count1 = H.ReadU16LE(comp_ptr);
            comp_ptr += 2;
        }
        else
        {
            arrays.packets_count1 = packets_count;
        }

        if (arrays.packets_count1 > packets_count) return -1;

        if (comp_end - comp_ptr < 2) return -1;
        uint num_off16s = H.ReadU16LE(comp_ptr);
        comp_ptr += 2;

        if (num_off16s == 0xFFFF)
        {
            byte* off16s_hi = scratch_ptr;
            long off16s_hi_count;
            long off16s_hi_comp_len = newLZ_get_array(&off16s_hi, comp_ptr, comp_end, &off16s_hi_count, Math.Min(chunk_len / 2, scratch_end - scratch_ptr), false, scratch_ptr, scratch_end);
            if (off16s_hi_comp_len < 0) return -1;
            comp_ptr += off16s_hi_comp_len;
            scratch_ptr += off16s_hi_count;

            byte* off16s_lo = scratch_ptr;
            long off16s_lo_count;
            long off16s_lo_comp_len = newLZ_get_array(&off16s_lo, comp_ptr, comp_end, &off16s_lo_count, Math.Min(chunk_len / 2, scratch_end - scratch_ptr), false, scratch_ptr, scratch_end);
            if (off16s_lo_comp_len < 0) return -1;
            comp_ptr += off16s_lo_comp_len;
            scratch_ptr += off16s_lo_count;

            if (off16s_hi_count != off16s_lo_count) return -1;
            num_off16s = (uint)off16s_hi_count;

            if (((long)scratch_ptr & 1) != 0) scratch_ptr++;
            if (2 * num_off16s > scratch_end - scratch_ptr) return -1;

            arrays.off16_ptr = scratch_ptr;
            ushort* off16s = (ushort*)arrays.off16_ptr;

            for (int i = 0; i < num_off16s; i++)
            {
                off16s[i] = (ushort)(off16s_lo[i] | (off16s_hi[i] << 8));
            }
            scratch_ptr += 2 * num_off16s;
        }
        else
        {
            if (2 * num_off16s > comp_end - comp_ptr) return -1;

            if (inplace_comp_raw_overlap)
            {
                if (scratch_end - scratch_ptr < 2 * num_off16s) return -1;
                Buffer.MemoryCopy(comp_ptr, scratch_ptr, 2 * num_off16s, 2 * num_off16s);
                arrays.off16_ptr = scratch_ptr;
                scratch_ptr += 2 * num_off16s;
            }
            else
            {
                arrays.off16_ptr = comp_ptr;
            }
            comp_ptr += 2 * num_off16s;
        }
        arrays.off16_end = arrays.off16_ptr + 2 * num_off16s;

        if (comp_end - comp_ptr < 3) return -1;
        uint off24_header = H.ReadU24LE(comp_ptr);
        comp_ptr += 3;

        if (off24_header == 0)
        {
            if (((long)scratch_ptr & 3) != 0) scratch_ptr += 4 - ((long)scratch_ptr & 3);
            arrays.escape_offsets1 = (uint*)scratch_ptr;
            arrays.escape_offsets2 = arrays.escape_offsets1;
            scratch_ptr += 4 * NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT;
            Unsafe.InitBlockUnaligned((byte*)arrays.escape_offsets1, 0, (uint)(4 * NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT));
        }
        else
        {
            uint off24_ptr_chunk_count1 = off24_header >> 12;
            uint off24_ptr_chunk_count2 = off24_header & 0xFFF;

            if (off24_ptr_chunk_count1 == 0xFFF)
            {
                if (comp_end - comp_ptr < 2) return -1;
                off24_ptr_chunk_count1 = H.ReadU16LE(comp_ptr);
                comp_ptr += 2;
            }
            if (off24_ptr_chunk_count2 == 0xFFF)
            {
                if (comp_end - comp_ptr < 2) return -1;
                off24_ptr_chunk_count2 = H.ReadU16LE(comp_ptr);
                comp_ptr += 2;
            }

            arrays.escape_offsets_count1 = off24_ptr_chunk_count1;
            arrays.escape_offsets_count2 = off24_ptr_chunk_count2;

            if (((long)scratch_ptr & 3) != 0) scratch_ptr += 4 - ((long)scratch_ptr & 3);

            arrays.escape_offsets1 = (uint*)scratch_ptr;
            scratch_ptr += 4 * off24_ptr_chunk_count1;
            Unsafe.InitBlockUnaligned(scratch_ptr, 0, (uint)(4 * NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT));
            scratch_ptr += 4 * NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT;

            arrays.escape_offsets2 = (uint*)scratch_ptr;
            scratch_ptr += 4 * off24_ptr_chunk_count2;
            Unsafe.InitBlockUnaligned(scratch_ptr, 0, (uint)(4 * NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT));
            scratch_ptr += 4 * NEWLZF_ESCAPE_OFFSET_PAD_ZERO_COUNT;

            int off24_data_len1 = newlzf_unpack_escape_offsets(comp_ptr, comp_end, arrays.escape_offsets1, off24_ptr_chunk_count1, chunk_pos);
            if (off24_data_len1 < 0) return -1;
            comp_ptr += off24_data_len1;

            int off24_data_len2 = newlzf_unpack_escape_offsets(comp_ptr, comp_end, arrays.escape_offsets2, off24_ptr_chunk_count2, chunk_pos + (1 << 16));
            if (off24_data_len2 < 0) return -1;
            comp_ptr += off24_data_len2;
        }

        arrays.excesses_ptr = comp_ptr;
        arrays.excesses_end = comp_end;

        return 0;
    }

    private static int newlzf_unpack_escape_offsets(byte* comp_base, byte* comp_end, uint* offsets, long offset_count, long max_offset)
    {
        if (offset_count == 0) return 0;

        byte* comp_ptr = comp_base;

        for (int i = 0; i < offset_count; i++)
        {
            if (comp_ptr + 3 > comp_end) return -1;

            uint off24 = H.ReadU24LE(comp_ptr);
            H.LogOodle($"  unpack_escape_offset[{i}]: raw bytes={comp_ptr[0]:X2} {comp_ptr[1]:X2} {comp_ptr[2]:X2} raw24={off24}");
            comp_ptr += 3;

            if (off24 >= NEWLZF_OFFSET_FOURBYTE_THRESHOLD)
            {
                if (comp_ptr >= comp_end) return -1;
                uint hi = *comp_ptr++;
                H.LogOodle($"    4-byte offset: hi={hi:X2} final={off24 + (hi << NEWLZF_OFFSET_FOURBYTE_SHIFT)}");
                off24 += (hi << NEWLZF_OFFSET_FOURBYTE_SHIFT);
            }

            H.LogOodle($"    offset={off24} max_offset={max_offset}");

            if (off24 > max_offset) return -1;
            offsets[i] = off24;
        }

        return (int)(comp_ptr - comp_base);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static int newlzf_getv(ref byte* from, byte* end)
    {
        if (from >= end) return 0;
        int b = *from++;
        if (b > 251)
        {
            if (from + 2 > end) return 0;
            int up = *(ushort*)from;
            from += 2;
            b += (up << 2);
        }
        return b;
    }

    private static int newLZF_decode_chunk_phase2(int chunk_type, byte* comp_base, byte* comp_end, byte* whole_chunk_ptr, long whole_chunk_len, long chunk_pos, byte* block_end, ref newLZF_chunk_arrays arrays)
    {
        long chunk_len1 = Math.Min(1 << 16, whole_chunk_len);
        long chunk_len2 = whole_chunk_len - chunk_len1;

        long packets_count1 = arrays.packets_count1;
        long packets_count2 = arrays.packets_count - packets_count1;

        byte* window_base = whole_chunk_ptr - chunk_pos;
        int neg_offset = -8; // NEWLZF_MIN_OFFSET
        bool isSub = (chunk_type == 0); // NEWLZ_LITERALS_TYPE_SUB=0, NEWLZ_LITERALS_TYPE_RAW=1

        // Save original packets_ptr since we use offsets from it
        byte* packets_base = arrays.packets_ptr;

        for (int twice = 0; twice < 2; twice++)
        {
            byte* chunk_ptr;
            long chunk_len;

            // Local pointers for this parse chunk
            byte* packets_ptr;
            byte* packets_end;

            if (twice == 0)
            {
                chunk_ptr = whole_chunk_ptr;
                chunk_len = chunk_len1;

                arrays.escape_offsets_ptr = arrays.escape_offsets1;
                arrays.escape_offsets_end = arrays.escape_offsets1 + arrays.escape_offsets_count1;

                packets_ptr = packets_base;
                packets_end = packets_base + packets_count1;
            }
            else
            {
                if (chunk_len2 == 0) break;

                chunk_ptr = whole_chunk_ptr + chunk_len1;
                chunk_len = chunk_len2;

                arrays.escape_offsets_ptr = arrays.escape_offsets2;
                arrays.escape_offsets_end = arrays.escape_offsets2 + arrays.escape_offsets_count2;

                packets_ptr = packets_base + packets_count1;
                packets_end = packets_ptr + packets_count2;
            }

            byte* to_ptr = chunk_ptr;
            if (chunk_pos == 0 && twice == 0) to_ptr += 8;

            byte* parse_chunk_end = chunk_ptr + chunk_len;

            // Split the main packet loop based on isSub to eliminate the branch in the hot path
            if (isSub)
            {
                // Hoist SWAR mask constant
                const ulong mask = 0x7F7F7F7F7F7F7F7FUL;
                
                // Local copies of frequently accessed struct fields for better codegen
                byte* literals_ptr_local = arrays.literals_ptr;
                byte* off16_ptr_local = arrays.off16_ptr;
                
                while (packets_ptr < packets_end)
                {
                    uint packet = *packets_ptr++;

                    if (packet >= 128)
                    {
                        // Repeat Match (Hot)
                        byte* match_base = to_ptr + neg_offset;
                        
                        ulong lit8 = *(ulong*)literals_ptr_local;
                        ulong match8 = *(ulong*)match_base;
                        *(ulong*)to_ptr = ((lit8 & mask) + (match8 & mask)) ^ ((lit8 ^ match8) & ~mask);
                        
                        uint lrl = packet & 7;
                        to_ptr += lrl;
                        literals_ptr_local += lrl;
                        
                        uint ml = (packet >> 3) & 0xF;
                        
                        byte* match_ptr = to_ptr + neg_offset;
                        if (match_ptr < window_base) return -1;
                        *(ulong*)to_ptr = *(ulong*)match_ptr;
                        *(ulong*)(to_ptr + 8) = *(ulong*)(match_ptr + 8);
                        to_ptr += ml;
                    }
                    else if (packet >= 24)
                    {
                        // Normal Match (New Offset)
                        byte* match_base = to_ptr + neg_offset;
                        
                        ulong lit8 = *(ulong*)literals_ptr_local;
                        ulong match8 = *(ulong*)match_base;
                        *(ulong*)to_ptr = ((lit8 & mask) + (match8 & mask)) ^ ((lit8 ^ match8) & ~mask);
                        
                        uint lrl = packet & 7;
                        to_ptr += lrl;
                        literals_ptr_local += lrl;

                        int offset = *(ushort*)off16_ptr_local;
                        neg_offset = -offset;
                        off16_ptr_local += 2;
                        
                        uint ml = (packet >> 3) & 0xF;
                        
                        byte* match_ptr = to_ptr + neg_offset;
                        if (match_ptr < window_base) return -1;
                        *(ulong*)to_ptr = *(ulong*)match_ptr;
                        *(ulong*)(to_ptr + 8) = *(ulong*)(match_ptr + 8);
                        to_ptr += ml;
                    }
                    else
                    {
                        // Escape packet - SUB mode - sync locals back
                        arrays.literals_ptr = literals_ptr_local;
                        arrays.off16_ptr = off16_ptr_local;
                        
                        if (packet <= 2)
                        {
                            if (packet == 0) // Long LRL
                            {
                                int lrl = newlzf_getv(ref arrays.excesses_ptr, arrays.excesses_end);
                                lrl += NEWLZF_LRL_EXCESS;

                                if (to_ptr + lrl > parse_chunk_end) return -1;

                                H.CopySub_SIMD(to_ptr, arrays.literals_ptr, to_ptr + neg_offset, lrl);
                                to_ptr += lrl;
                                arrays.literals_ptr += lrl;
                            }
                            else if (packet == 1) // Long ML, OFF16
                            {
                                int excess_val = newlzf_getv(ref arrays.excesses_ptr, arrays.excesses_end);
                                int ml = NEWLZF_ML_EXCESS + excess_val;

                                H.LogOodle($"  packet=1 Long ML OFF16: excess={excess_val} ml={ml}");

                                int offset = H.ReadU16LE(arrays.off16_ptr);
                                arrays.off16_ptr += 2;
                                neg_offset = -offset;

                                H.LogOodle($"  offset={offset} neg_offset={neg_offset}");

                                byte* match_ptr = to_ptr + neg_offset;
                                if (match_ptr < window_base) return -1;

                                H.LogOodle($"  to_ptr-chunk={to_ptr - whole_chunk_ptr} match_ptr-window_base={match_ptr - window_base}");

                                H.CopyMatch_SIMD(to_ptr, match_ptr, ml, -neg_offset);
                                to_ptr += ml;
                            }
                            else // packet == 2, Long ML + New Offset
                            {
                                int ml = 21 + NEWLZF_OFF24_MML_DECODE + newlzf_getv(ref arrays.excesses_ptr, arrays.excesses_end);

                                if (arrays.escape_offsets_ptr >= arrays.escape_offsets_end) return -1;
                                int offset = (int)(*arrays.escape_offsets_ptr++);

                                byte* match_ptr = chunk_ptr - offset;
                                neg_offset = (int)(match_ptr - to_ptr);

                                if (match_ptr < window_base) return -1;

                                H.CopyMatch_SIMD(to_ptr, match_ptr, ml, -neg_offset);
                                to_ptr += ml;
                            }
                        }
                        else // Short escape (packet 3-23)
                        {
                            int ml = (int)(packet - 3) + NEWLZF_OFF24_MML_DECODE;

                            if (arrays.escape_offsets_ptr >= arrays.escape_offsets_end) return -1;
                            int offset = (int)(*arrays.escape_offsets_ptr++);

                            byte* match_ptr = chunk_ptr - offset;
                            neg_offset = (int)(match_ptr - to_ptr);

                            if (match_ptr < window_base) return -1;

                            H.CopyMatch_SIMD(to_ptr, match_ptr, ml, -neg_offset);
                            to_ptr += ml;
                        }
                        
                        // Reload locals after escape packet
                        literals_ptr_local = arrays.literals_ptr;
                        off16_ptr_local = arrays.off16_ptr;
                    }
                }
                
                // Sync locals back to struct
                arrays.literals_ptr = literals_ptr_local;
                arrays.off16_ptr = off16_ptr_local;
            }
            else
            {
                // RAW mode loop - no SUB operations
                // Local copies for better codegen
                byte* literals_ptr_local = arrays.literals_ptr;
                byte* off16_ptr_local = arrays.off16_ptr;
                
                while (packets_ptr < packets_end)
                {
                    int packet = *packets_ptr++;

                    if (packet >= 24)
                    {
                        // Simple packet - RAW mode
                        int lrl = packet & 7;
                        int ml = (packet >> 3) & 0xF;
                        int offset_mask = (packet >> 7) - 1;

                        int next_offset = -(int)*(ushort*)off16_ptr_local;

                        // RAW mode - direct 64-bit copy
                        *(ulong*)to_ptr = *(ulong*)literals_ptr_local;
                        to_ptr += lrl;
                        literals_ptr_local += lrl;

                        // Branchless offset update and pointer advance
                        neg_offset ^= (next_offset ^ neg_offset) & offset_mask;
                        off16_ptr_local += (offset_mask & 2);

                        byte* match_ptr = to_ptr + neg_offset;
                        if (match_ptr < window_base) return -1;

                        // Copy match using 64-bit ops (16 bytes safe, ml <= 15)
                        *(ulong*)to_ptr = *(ulong*)match_ptr;
                        *(ulong*)(to_ptr + 8) = *(ulong*)(match_ptr + 8);
                        to_ptr += ml;
                    }
                    else
                    {
                        // Escape packet - RAW mode - sync locals back
                        arrays.literals_ptr = literals_ptr_local;
                        arrays.off16_ptr = off16_ptr_local;
                        
                        if (packet <= 2)
                        {
                            if (packet == 0) // Long LRL
                            {
                                int lrl = newlzf_getv(ref arrays.excesses_ptr, arrays.excesses_end);
                                lrl += NEWLZF_LRL_EXCESS;

                                if (to_ptr + lrl > parse_chunk_end) return -1;

                                H.CopyBytes_SIMD(to_ptr, arrays.literals_ptr, lrl);
                                to_ptr += lrl;
                                arrays.literals_ptr += lrl;
                            }
                            else if (packet == 1) // Long ML, OFF16
                            {
                                int excess_val = newlzf_getv(ref arrays.excesses_ptr, arrays.excesses_end);
                                int ml = NEWLZF_ML_EXCESS + excess_val;

                                H.LogOodle($"  packet=1 Long ML OFF16: excess={excess_val} ml={ml}");

                                int offset = H.ReadU16LE(arrays.off16_ptr);
                                arrays.off16_ptr += 2;
                                neg_offset = -offset;

                                H.LogOodle($"  offset={offset} neg_offset={neg_offset}");

                                byte* match_ptr = to_ptr + neg_offset;
                                if (match_ptr < window_base) return -1;

                                H.LogOodle($"  to_ptr-chunk={to_ptr - whole_chunk_ptr} match_ptr-window_base={match_ptr - window_base}");

                                H.CopyMatch_SIMD(to_ptr, match_ptr, ml, -neg_offset);
                                to_ptr += ml;
                            }
                            else // packet == 2, Long ML + New Offset
                            {
                                int ml = 21 + NEWLZF_OFF24_MML_DECODE + newlzf_getv(ref arrays.excesses_ptr, arrays.excesses_end);

                                if (arrays.escape_offsets_ptr >= arrays.escape_offsets_end) return -1;
                                int offset = (int)(*arrays.escape_offsets_ptr++);

                                byte* match_ptr = chunk_ptr - offset;
                                neg_offset = (int)(match_ptr - to_ptr);

                                if (match_ptr < window_base) return -1;

                                H.CopyMatch_SIMD(to_ptr, match_ptr, ml, -neg_offset);
                                to_ptr += ml;
                            }
                        }
                        else // Short escape (packet 3-23)
                        {
                            int ml = packet - 3 + NEWLZF_OFF24_MML_DECODE;

                            if (arrays.escape_offsets_ptr >= arrays.escape_offsets_end) return -1;
                            int offset = (int)(*arrays.escape_offsets_ptr++);

                            byte* match_ptr = chunk_ptr - offset;
                            neg_offset = (int)(match_ptr - to_ptr);

                            if (match_ptr < window_base) return -1;

                            H.CopyMatch_SIMD(to_ptr, match_ptr, ml, -neg_offset);
                            to_ptr += ml;
                        }
                        
                        // Reload locals after escape packet
                        literals_ptr_local = arrays.literals_ptr;
                        off16_ptr_local = arrays.off16_ptr;
                    }
                }
                
                // Sync locals back to struct
                arrays.literals_ptr = literals_ptr_local;
                arrays.off16_ptr = off16_ptr_local;
            }

            // Final literals
            if (to_ptr < parse_chunk_end)
            {
                int lrl = (int)(parse_chunk_end - to_ptr);
                if (isSub)
                {
                    H.CopySub_SIMD(to_ptr, arrays.literals_ptr, to_ptr + neg_offset, lrl);
                    to_ptr += lrl;
                    arrays.literals_ptr += lrl;
                }
                else
                {
                    H.CopyBytes_SIMD(to_ptr, arrays.literals_ptr, lrl);
                    to_ptr += lrl;
                    arrays.literals_ptr += lrl;
                }
            }
        }

        return 0;
    }

    private const int NEWLZHC_LITERAL_ARRAY_COUNT_MAX = 16;
    private const int NEWLZHC_PACKET_POS_COUNT = 8;

    [StructLayout(LayoutKind.Sequential)]
    private struct newLZHC_chunk_arrays
    {
        public byte* chunk_ptr;
        public byte* scratch_ptr;
        public byte* scratch_end;

        public int* offsets;
        public long offsets_count;

        public uint* excesses;
        public long excesses_count;

        public fixed long literals_ptrs_long[NEWLZHC_LITERAL_ARRAY_COUNT_MAX];
        public fixed long literals_counts[NEWLZHC_LITERAL_ARRAY_COUNT_MAX];
        public long tot_literals_count;

        public fixed long packet_pos_ptrs_long[NEWLZHC_PACKET_POS_COUNT];
        public fixed long packet_pos_ends_long[NEWLZHC_PACKET_POS_COUNT];

        public byte* packets;
        public long packets_count;

        public byte* GetLiteralsPtr(int index)
        {
            return (byte*)literals_ptrs_long[index];
        }
        public void SetLiteralsPtr(int index, byte* ptr)
        {
            literals_ptrs_long[index] = (long)ptr;
        }

        public byte* GetPacketPosPtr(int index)
        {
            return (byte*)packet_pos_ptrs_long[index];
        }
        public void SetPacketPosPtr(int index, byte* ptr)
        {
            packet_pos_ptrs_long[index] = (long)ptr;
        }

        public byte* GetPacketPosEnd(int index)
        {
            return (byte*)packet_pos_ends_long[index];
        }
        public void SetPacketPosEnd(int index, byte* ptr)
        {
            packet_pos_ends_long[index] = (long)ptr;
        }
    }

    // Leviathan constants
    private const int NEWLZHC_CHUNK_LEN = 128 * 1024;
    private const int NEWLZHC_NUM_LAST_OFFSETS = 7;
    private const int NEWLZHC_PACKET_LRL_BITS = 2;
    private const int NEWLZHC_PACKET_ML_BITS = 3;
    private const int NEWLZHC_PACKET_OFFSET_SHIFT = (NEWLZHC_PACKET_LRL_BITS + NEWLZHC_PACKET_ML_BITS);
    private const int NEWLZHC_PACKET_LRL_MAX = (1 << NEWLZHC_PACKET_LRL_BITS) - 1; // 3
    private const int NEWLZHC_PACKET_ML_MAX = (1 << NEWLZHC_PACKET_ML_BITS) - 1; // 7
    private const int NEWLZHC_LOMML = 2;  // Last offset minimum match length
    private const int NEWLZHC_CHUNK_NO_MATCH_ZONE = 16;
    private const int NEWLZHC_MATCH_END_PAD = 8;
    private const int NEWLZHC_PACKET_POS_MASK = 7;

    private static int Leviathan_DecodeOneQuantum(byte* decomp, byte* decomp_end, byte* comp, int quantumCompLen, byte* compBufEnd, long pos_since_reset, byte* scratch, int scratch_size)
    {
        long decomp_len = decomp_end - decomp;
        byte* scratch_ptr = scratch;
        byte* scratch_end = scratch_ptr + scratch_size;

        byte* rawPtr = decomp;
        byte* rawEnd = decomp_end;
        byte* compPtr = comp;
        byte* compEnd = compPtr + quantumCompLen;

        while (rawPtr < rawEnd)
        {
            long chunk_len = Math.Min(NEWLZHC_CHUNK_LEN, rawEnd - rawPtr);
            long chunk_pos = (rawPtr - decomp) + pos_since_reset;

            if (compEnd - compPtr < 4)
            {
                H.LogOodle("Not enough data for chunk header");
                return -1;
            }

            int chunk_comp_len = (int)H.ReadU24BE(compPtr);

            if (chunk_comp_len >= (1 << 23))
            {
                int chunk_type = (chunk_comp_len >> 19) & 0xF;
                chunk_comp_len &= (1 << 19) - 1;
                compPtr += 3;

                H.LogOodle($"Leviathan_DecodeOneQuantum: chunk_type={chunk_type} chunk_comp_len={chunk_comp_len}");

                if (chunk_comp_len > compEnd - compPtr)
                {
                    H.LogOodle($"chunk_comp_len {chunk_comp_len} > avail {compEnd - compPtr}");
                    return -1;
                }

                byte* chunk_comp_end = compPtr + chunk_comp_len;

                if (chunk_comp_len >= chunk_len)
                {
                    // Raw chunk
                    if (chunk_comp_len > chunk_len) return -1;
                    if (chunk_type != 0) return -1;

                    Buffer.MemoryCopy(compPtr, rawPtr, chunk_len, chunk_len);
                }
                else
                {
                    if (chunk_len < 128) return -1; // NEWLZ_MIN_CHUNK_LEN

                    // Check scratch size
                    if (scratch_size < chunk_len * 3 + 256 * 1024) return -1;

                    newLZHC_chunk_arrays arrays = new newLZHC_chunk_arrays();

                    if (newLZHC_decode_chunk_phase1(chunk_type, compPtr, chunk_comp_end, rawPtr, chunk_len, chunk_pos, scratch_ptr, scratch_end, ref arrays) < 0)
                    {
                        H.LogOodle("phase1 failed");
                        return -1;
                    }

                    if (newLZHC_decode_chunk_phase2(chunk_type, rawPtr, chunk_len, chunk_pos, ref arrays) < 0)
                    {
                        H.LogOodle("phase2 failed");
                        return -1;
                    }
                }

                compPtr += chunk_comp_len;
                rawPtr += chunk_len;
            }
            else
            {
                // Huff-only chunk
                long literals_len = 0;
                byte* literals = rawPtr;

                bool inplace_comp_raw_overlap = (compPtr <= (rawPtr + chunk_len) && rawPtr <= compEnd);

                if (inplace_comp_raw_overlap)
                {
                    literals = scratch_ptr;
                }

                long ret = newLZ_get_array(&literals, compPtr, compEnd, &literals_len, chunk_len, false, scratch_ptr, scratch_end);

                if (ret < 0)
                {
                    H.LogOodle($"newLZ_get_array failed in huff-only chunk. Ret={ret}");
                    return -1;
                }

                chunk_comp_len = (int)ret;

                if (inplace_comp_raw_overlap)
                {
                    if (literals != scratch_ptr) return -1;
                    Buffer.MemoryCopy(scratch_ptr, rawPtr, chunk_len, chunk_len);
                }
                else
                {
                    if (literals != rawPtr) return -1;
                }

                if (literals_len != chunk_len)
                {
                    H.LogOodle($"huff-only chunk length mismatch. Got {literals_len}, expected {chunk_len}");
                    return -1;
                }

                compPtr += chunk_comp_len;
                rawPtr += chunk_len;
            }
        }

        return (int)decomp_len;
    }

    private static int newLZHC_decode_chunk_phase1(int chunk_type, byte* comp, byte* chunk_comp_end, byte* chunk_ptr, long chunk_len, long chunk_pos, byte* scratch_space, byte* scratch_end, ref newLZHC_chunk_arrays arrays)
    {
        // SUB(0), RAW(1), LAMSUB(2), SUBAND3(3), O1(4), SUBANDF(5) supported
        if (chunk_type < 0 || chunk_type > 5)
        {
            H.LogOodle($"newLZHC_decode_chunk_phase1: chunk_type {chunk_type} not supported");
            return -1;
        }

        const bool force_copy_uncompressed = true;

        arrays.chunk_ptr = chunk_ptr;
        arrays.scratch_ptr = scratch_space;
        arrays.scratch_end = scratch_end;

        byte* comp_ptr = comp;
        byte* scratch_ptr = scratch_space;

        // Minimum 13 bytes needed
        if (chunk_comp_end - comp_ptr < 13)
        {
            H.LogOodle($"newLZHC_decode_chunk_phase1: not enough data. avail={chunk_comp_end - comp_ptr} needed=13");
            return -1;
        }

        // First chunk needs to copy first 8 bytes
        byte* to_ptr = chunk_ptr;
        if (chunk_pos == 0)
        {
            *(ulong*)to_ptr = *(ulong*)comp_ptr;
            to_ptr += 8;
            comp_ptr += 8;
        }

        // Max counts
        long max_num_offsets = chunk_len / 3;
        long max_num_excesses = chunk_len / 5;

        // Get offsets u8 array - need to read the header byte first to check for alt offset
        if (comp_ptr >= chunk_comp_end)
        {
            H.LogOodle("newLZHC_decode_chunk_phase1: comp_ptr >= chunk_comp_end before offsets");
            return -1;
        }

        byte* offsets_u8 = null;
        byte* offsets_u8_2 = null;
        long offsets_count = 0;
        int offset_alt_modulo = 0;

        // Keep track of scratch_end for u8 arrays at the tail
        byte* scratch_tail = scratch_end;

        if (*comp_ptr >= 0x80)
        {
            // Alt offset encoding
            offset_alt_modulo = *comp_ptr - 0x80 + 1;
            H.LogOodle($"Alt offset encoding: modulo={offset_alt_modulo} chunk_pos={chunk_pos}");
            comp_ptr++;

            long offsets_count1;
            if (newLZ_get_arraylens(comp_ptr, chunk_comp_end, &offsets_count, max_num_offsets) < 0) return -1;

            scratch_tail -= offsets_count;
            offsets_u8 = scratch_tail;

            long offsets_u8_comp_len1 = newLZ_get_array(&offsets_u8, comp_ptr, chunk_comp_end, &offsets_count1, offsets_count, false, scratch_ptr, scratch_tail);
            if (offsets_u8_comp_len1 < 0) return -1;

            comp_ptr += offsets_u8_comp_len1;

            if (offset_alt_modulo > 1)
            {
                scratch_tail -= offsets_count;
                offsets_u8_2 = scratch_tail;

                long offsets_count2;
                long offsets_u8_comp_len2 = newLZ_get_array(&offsets_u8_2, comp_ptr, chunk_comp_end, &offsets_count2, offsets_count, false, scratch_ptr, scratch_tail);
                if (offsets_u8_comp_len2 < 0) return -1;
                if (offsets_count != offsets_count2) return -1;

                comp_ptr += offsets_u8_comp_len2;
            }
        }
        else
        {
            // Standard offset44 encoding
            long offsets_count1;
            if (newLZ_get_arraylens(comp_ptr, chunk_comp_end, &offsets_count, max_num_offsets) < 0) return -1;

            scratch_tail -= offsets_count;
            offsets_u8 = scratch_tail;

            long offsets_u8_comp_len = newLZ_get_array(&offsets_u8, comp_ptr, chunk_comp_end, &offsets_count1, offsets_count, false, scratch_ptr, scratch_tail);
            if (offsets_u8_comp_len < 0) return -1;

            comp_ptr += offsets_u8_comp_len;
        }

        // Get excesses u8 array
        long excesses_count;
        if (newLZ_get_arraylens(comp_ptr, chunk_comp_end, &excesses_count, max_num_excesses) < 0) return -1;

        H.LogOodle($"newLZHC_decode_chunk_phase1: excesses_count={excesses_count}");

        scratch_tail -= excesses_count;
        byte* excesses_u8 = scratch_tail;

        long excesses_count1;
        long excesses_u8_comp_len = newLZ_get_array(&excesses_u8, comp_ptr, chunk_comp_end, &excesses_count1, excesses_count, false, scratch_ptr, scratch_tail);
        if (excesses_u8_comp_len < 0) return -1;

        comp_ptr += excesses_u8_comp_len;

        // Allocate space for offsets and excesses u32 arrays at head of scratch
        scratch_ptr = (byte*)((((long)scratch_ptr) + 15) & ~15L); // Align to 16

        int* offsets = (int*)scratch_ptr;
        uint* excesses = (uint*)(offsets + offsets_count);
        excesses = (uint*)(((long)excesses + 15) & ~15L); // Align to 16
        uint* u32_space_end = excesses + excesses_count;
        scratch_ptr = (byte*)u32_space_end;

        arrays.offsets = offsets;
        arrays.offsets_count = offsets_count;
        arrays.excesses = excesses;
        arrays.excesses_count = excesses_count;

        // Get literals array
        {
            int num_literals_arrays = 1;
            if (chunk_type == 2) num_literals_arrays = 2; // NEWLZ_LITERALS_TYPE_LAMSUB
            if (chunk_type == 3) num_literals_arrays = 4; // NEWLZ_LITERALS_TYPE_SUBAND3
            if (chunk_type == 4) num_literals_arrays = 16; // NEWLZ_LITERALS_TYPE_O1
            if (chunk_type == 5) num_literals_arrays = 16; // NEWLZ_LITERALS_TYPE_SUBANDF

            long tot_literals_count;

            if (num_literals_arrays == 1)
            {
                byte* literals = scratch_ptr;
                long literals_count;
                long literals_comp_len = newLZ_get_array(&literals, comp_ptr, chunk_comp_end, &literals_count, scratch_tail - scratch_ptr, force_copy_uncompressed, scratch_ptr, scratch_tail);
                if (literals_comp_len < 0) { Console.Error.WriteLine($"P1FAIL:lit pos={chunk_pos:X}"); return -1; }

                comp_ptr += literals_comp_len;
                scratch_ptr += literals_count;

                arrays.SetLiteralsPtr(0, literals);
                arrays.literals_counts[0] = literals_count;
                tot_literals_count = literals_count;
            }
            else
            {
                byte** literals_ptrs = stackalloc byte*[num_literals_arrays];
                long* literals_counts = stackalloc long[num_literals_arrays];

                long literals_comp_len = newLZ_get_multiarray(comp_ptr, chunk_comp_end, scratch_ptr, scratch_tail, literals_ptrs, literals_counts, num_literals_arrays, &tot_literals_count, force_copy_uncompressed, scratch_ptr, scratch_tail);
                if (literals_comp_len < 0) return -1;
                if (tot_literals_count > chunk_len) return -1;

                comp_ptr += literals_comp_len;
                scratch_ptr += tot_literals_count;

                for(int i=0;i<num_literals_arrays;i++)
                {
                    arrays.SetLiteralsPtr(i, literals_ptrs[i]);
                    arrays.literals_counts[i] = literals_counts[i];
                }
            }
            arrays.tot_literals_count = tot_literals_count;
        }

        // Get packets array
        if (comp_ptr >= chunk_comp_end)
        {
            H.LogOodle($"newLZHC_decode_chunk_phase1: comp_ptr >= chunk_comp_end before packets. avail={chunk_comp_end - comp_ptr}");
            return -1;
        }

        long tot_packets_count = 0;
        long max_packets_count = chunk_len / 2;

        H.LogOodle($"newLZHC_decode_chunk_phase1: getting packets. *comp_ptr={*comp_ptr:X2}");

        if (*comp_ptr >= 0x80)
        {
            // Multi-packet mode
            byte header = *comp_ptr++;
            int packet_pos_bits = header & 0x7F;
            int num_packets_arrays = 1 << packet_pos_bits;

            if (num_packets_arrays != NEWLZHC_PACKET_POS_COUNT) return -1;

            long* packet_pos_counts = stackalloc long[NEWLZHC_PACKET_POS_COUNT];
            byte** packet_ptrs = stackalloc byte*[NEWLZHC_PACKET_POS_COUNT];

            long packets_comp_len = newLZ_get_multiarray(comp_ptr, chunk_comp_end, scratch_ptr, scratch_tail, packet_ptrs, packet_pos_counts, NEWLZHC_PACKET_POS_COUNT, &tot_packets_count, force_copy_uncompressed, scratch_ptr, scratch_tail);
            if (packets_comp_len < 0) { Console.Error.WriteLine($"P1FAIL:pkt pos={chunk_pos:X}"); return -1; }
            if (tot_packets_count > max_packets_count) return -1;

            for (int i = 0; i < NEWLZHC_PACKET_POS_COUNT; i++)
            {
                arrays.SetPacketPosPtr(i, packet_ptrs[i]);
                arrays.SetPacketPosEnd(i, packet_ptrs[i] + packet_pos_counts[i]);
            }

            comp_ptr += packets_comp_len;
            scratch_ptr += tot_packets_count;

            arrays.packets = null;  // null means multipacket mode
            arrays.packets_count = tot_packets_count;
        }
        else
        {
            // Single packet stream
            byte* packets = scratch_ptr;
            long packets_comp_len = newLZ_get_array(&packets, comp_ptr, chunk_comp_end, &tot_packets_count, Math.Min(max_packets_count, scratch_tail - scratch_ptr), force_copy_uncompressed, scratch_ptr, scratch_tail);
            if (packets_comp_len < 0) return -1;

            H.LogOodle($"newLZHC_decode_chunk_phase1: packets_count={tot_packets_count}");

            comp_ptr += packets_comp_len;
            scratch_ptr += tot_packets_count;

            arrays.packets = packets;
            arrays.packets_count = tot_packets_count;
        }

        // Check scratch bounds
        if (scratch_ptr - scratch_space > 2 * chunk_len)
        {
            H.LogOodle($"scratch bounds check failed: used={scratch_ptr - scratch_space} limit={2 * chunk_len}");
            return -1;
        }

        // Decode offsets and excesses from varbits
        byte excess_hdr_byte = 0;
        int excess_stream_size = 0;

        if (offsets_count == 10498) {
            H.LogOodle($"Before varbits decode: comp_ptr={((long)comp_ptr):X} chunk_comp_end={((long)chunk_comp_end):X} varbits_len={chunk_comp_end - comp_ptr}");
        }

        if (newLZ_get_offsets_excesses(comp_ptr, chunk_comp_end, offsets_u8, offsets_u8_2, offsets_count, (uint)offset_alt_modulo, excesses_u8, excesses_count, offsets, excesses, chunk_pos + chunk_len, excess_hdr_byte, excess_stream_size) < 0)
        {
            H.LogOodle("newLZ_get_offsets_excesses failed");
            return -1;
        }

        // Debug: dump some offset values after decoding
        if (offsets_count > 10 && chunk_len == 131072)
        {
            H.LogOodle($"After offset decode: [0]={offsets[0]} [1]={offsets[1]} [2]={offsets[2]} [3]={offsets[3]} [4]={offsets[4]} chunk_pos={chunk_pos}");
            H.LogOodle($"u8 bytes: [0]={offsets_u8[0]:X2} [1]={offsets_u8[1]:X2} [2]={offsets_u8[2]:X2} [3]={offsets_u8[3]:X2} [4]={offsets_u8[4]:X2}");
            // Dump offsets around index 830-840 for chunk 1
            if (chunk_pos == 131072 && offsets_count > 840)
            {
                H.LogOodle($"Offsets around 830: [830]={offsets[830]} [831]={offsets[831]} [832]={offsets[832]} [833]={offsets[833]} [834]={offsets[834]}");
                H.LogOodle($"u8 around 830: [830]={offsets_u8[830]:X2} [831]={offsets_u8[831]:X2} [832]={offsets_u8[832]:X2} [833]={offsets_u8[833]:X2} [834]={offsets_u8[834]:X2}");
            }
        }

        H.LogOodle($"newLZHC_decode_chunk_phase1 success. consumed={comp_ptr - comp}");
        return (int)(comp_ptr - comp);
    }

    private static long newLZ_get_multiarray(byte* comp_start, byte* comp_end, byte* to_mem, byte* to_mem_end, byte** to_ptrs, long* to_lens, int num_arrays, long* ptot_to_len, bool force_copy_uncompressed, byte* scratch_ptr, byte* scratch_end)
    {
        if (comp_end - comp_start < 3) return -1;

        byte* comp_ptr = comp_start;

        int control_byte = *comp_ptr++;
        if (control_byte < 0x80) return -1;

        int num_entropy_arrays = control_byte & 63;
        bool is_identity = (num_entropy_arrays == 0);

        if (is_identity)
            num_entropy_arrays = num_arrays;

        H.LogOodle($"newLZ_get_multiarray: num_entropy_arrays={num_entropy_arrays} is_identity={is_identity}");

        if (to_mem == scratch_ptr)
        {
            long scratch_space = scratch_end - scratch_ptr;
            scratch_space -= NEWLZ_ARRAY_INTERNAL_MAX_SCRATCH;
            if (scratch_space <= 0) return -1;
            scratch_space /= 2;

            scratch_ptr += scratch_space;
            to_mem_end = scratch_ptr;
        }

        byte* to_ptr;
        byte* to_ptr_end;
        bool to_ptr_is_scratch;

        if (is_identity)
        {
            to_ptr = to_mem;
            to_ptr_end = to_mem_end;
            to_ptr_is_scratch = false;
        }
        else
        {
            to_ptr = scratch_ptr;
            to_ptr_end = scratch_end;
            to_ptr_is_scratch = true;
        }

        long tot_to_len = 0;

        byte** entropy_array_ptrs = stackalloc byte*[NEWLZ_MULTIARRAYS_FORMAT_MAX_NUM_ARRAYS];
        long* entropy_array_lens = stackalloc long[NEWLZ_MULTIARRAYS_FORMAT_MAX_NUM_ARRAYS];

        for (int i = 0; i < num_entropy_arrays; i++)
        {
            bool sub_force_copy_uncompressed = is_identity && force_copy_uncompressed;

            long cur_to_len;
            byte* cur_lit_ptr = to_ptr;
            long cur_comp_len = newLZ_get_array(&cur_lit_ptr, comp_ptr, comp_end, &cur_to_len, to_ptr_end - to_ptr, sub_force_copy_uncompressed, scratch_ptr, scratch_end);
            if (cur_comp_len < 0) { H.LogOodle($"newLZ_get_multiarray: entropy array {i} failed"); return -1; }

            entropy_array_ptrs[i] = cur_lit_ptr;
            entropy_array_lens[i] = cur_to_len;

            to_ptr += cur_to_len;
            if (to_ptr_is_scratch) scratch_ptr += cur_to_len;
            tot_to_len += cur_to_len;
            comp_ptr += cur_comp_len;
        }

        *ptot_to_len = tot_to_len;

        if (is_identity)
        {
            for (int i = 0; i < num_arrays; i++)
            {
                to_ptrs[i] = entropy_array_ptrs[i];
                to_lens[i] = entropy_array_lens[i];
            }
            return comp_ptr - comp_start;
        }

        // Indexed mode
        H.LogOodle("newLZ_get_multiarray: starting indexed logic");
        if (comp_end - comp_ptr < 3) return -1;

        int varbits_complen = H.ReadU16LE(comp_ptr);
        comp_ptr += 2;

        bool varbits_complen_flag = (varbits_complen & 0x8000) != 0;
        varbits_complen &= 0x3FFF;

        long num_indices;
        long len_bytes = newLZ_get_arraylens(comp_ptr, comp_end, &num_indices, tot_to_len + 1024);
        H.LogOodle($"newLZ_get_multiarray: num_indices={num_indices} varbits_complen_flag={varbits_complen_flag} varbits_complen={varbits_complen}");
        if (len_bytes <= 0) return -1;

        long num_intervals = num_indices - num_arrays;
        if (num_intervals < 1) return -1;

        byte* interval_lenlog2 = scratch_ptr;
        if (scratch_end - scratch_ptr < num_indices) return -1;
        scratch_ptr += num_indices;

        byte* interval_indices = scratch_ptr;
        if (scratch_end - scratch_ptr < num_indices) return -1;
        scratch_ptr += num_indices;

        long num_lens;

        if (!varbits_complen_flag)
        {
            // Indices - newLZ_get_array may update the pointer to point to uncompressed data in stream
            long to_len;
            long array_len = newLZ_get_array(&interval_indices, comp_ptr, comp_end, &to_len, num_indices, false, scratch_ptr, scratch_end);
            H.LogOodle($"newLZ_get_multiarray: indices array_len={array_len} to_len={to_len} expected={num_indices}");
            if (array_len < 0 || to_len != num_indices) return -1;
            comp_ptr += array_len;

            // Len log2 - newLZ_get_array may update the pointer to point to uncompressed data in stream
            array_len = newLZ_get_array(&interval_lenlog2, comp_ptr, comp_end, &to_len, num_intervals, false, scratch_ptr, scratch_end);
            H.LogOodle($"newLZ_get_multiarray: lenlog2 array_len={array_len} to_len={to_len} expected={num_intervals}");
            if (array_len < 0 || to_len != num_intervals) return -1;
            comp_ptr += array_len;

            num_lens = num_intervals;

            for (int i = 0; i < num_lens; i++) if (interval_lenlog2[i] > 16) return -1;
        }
        else
        {
            // Packed
            byte* lens_and_indices = interval_indices;
            long to_len;
            long array_len = newLZ_get_array(&lens_and_indices, comp_ptr, comp_end, &to_len, num_indices, false, scratch_ptr, scratch_end);
            if (array_len < 0 || to_len != num_indices) return -1;
            comp_ptr += array_len;

            H.LogOodle($"newLZ_get_multiarray: lens_and_indices==interval_indices: {(lens_and_indices == interval_indices)}");

            // Unpack
            for (int i = 0; i < num_indices; i++)
            {
                byte packed = lens_and_indices[i];
                interval_lenlog2[i] = (byte)(packed >> 4);
                interval_indices[i] = (byte)(packed & 0xF);
            }

            // Log first few and last few indices
            if (num_indices >= 5)
            {
                H.LogOodle($"newLZ_get_multiarray: first indices: {interval_indices[0]}, {interval_indices[1]}, {interval_indices[2]}, {interval_indices[3]}, {interval_indices[4]}");
                H.LogOodle($"newLZ_get_multiarray: last indices: {interval_indices[num_indices-5]}, {interval_indices[num_indices-4]}, {interval_indices[num_indices-3]}, {interval_indices[num_indices-2]}, {interval_indices[num_indices-1]}");
            }

            num_lens = num_indices;
        }

        // Interval lens array
        if (scratch_end - scratch_ptr < 4) return -1;
        long align = (long)scratch_ptr & 3;
        if (align != 0) scratch_ptr += 4 - align;

        uint* interval_lens = (uint*)scratch_ptr;
        if (scratch_end - scratch_ptr < num_lens * 4) return -1;
        scratch_ptr += num_lens * 4;

        // Varbits for lengths
        byte* varbits_end = comp_ptr + varbits_complen;
        if (varbits_end > comp_end) return -1;

        rrVarBits lvb1 = new rrVarBits();
        rrVarBits_GetOpen(ref lvb1, comp_ptr, comp_end);

        rrVarBits lvb2 = new rrVarBits();
        rrVarBits_GetOpenBack(ref lvb2, varbits_end, comp_ptr);

        int leni = 0;
        while (leni + 2 <= num_lens)
        {
            rrVarBits_Refill_Safe(ref lvb1);
            rrVarBits_RefillBack_Safe(ref lvb2);

            int len1_log2 = interval_lenlog2[leni];
            int len2_log2 = interval_lenlog2[leni+1];

            uint len1 = (1u << len1_log2) | rrVarBits_Get_0Ok(ref lvb1, len1_log2);
            uint len2 = (1u << len2_log2) | rrVarBits_Get_0Ok(ref lvb2, len2_log2);

            interval_lens[leni] = len1;
            interval_lens[leni+1] = len2;
            leni += 2;
        }
        if (leni < num_lens)
        {
            rrVarBits_Refill_Safe(ref lvb1);
            int len1_log2 = interval_lenlog2[leni];
            uint len1 = (1u << len1_log2) | rrVarBits_Get_0Ok(ref lvb1, len1_log2);
            interval_lens[leni] = len1;
            leni++;
        }

        comp_ptr = varbits_end;

        // Process commands
        to_ptr = to_mem;
        to_ptr_end = to_mem_end;

        int indi = 0;
        leni = 0;
        int eof_len_increment = varbits_complen_flag ? 1 : 0;

        for (int dest_array_i = 0; dest_array_i < num_arrays; dest_array_i++)
        {
            to_ptrs[dest_array_i] = to_ptr;

            if (indi >= num_indices) return -1;

            H.LogOodle($"newLZ_get_multiarray: starting dest_array {dest_array_i}, indi={indi}, leni={leni}");

            for (;;)
            {
                int source = interval_indices[indi++];
                H.LogOodle($"newLZ_get_multiarray: indi={indi-1}, source={source}");
                if (source == 0)
                {
                    leni += eof_len_increment;
                    break;
                }
                if (source > num_entropy_arrays) { H.LogOodle($"newLZ_get_multiarray: source {source} > num_entropy_arrays {num_entropy_arrays}"); return -1; }
                source--;

                if (leni >= num_lens) { H.LogOodle($"newLZ_get_multiarray: leni {leni} >= num_lens {num_lens}"); return -1; }
                long cur_len = interval_lens[leni++];

                H.LogOodle($"newLZ_get_multiarray: copying from entropy array {source}: cur_len={cur_len}, remaining={entropy_array_lens[source]}, to_space={to_ptr_end - to_ptr}");

                if (cur_len > entropy_array_lens[source] || cur_len > to_ptr_end - to_ptr) { H.LogOodle($"newLZ_get_multiarray: copy would overflow"); return -1; }

                H.CopyBytes_SIMD(to_ptr, entropy_array_ptrs[source], (int)cur_len);

                to_ptr += cur_len;
                entropy_array_ptrs[source] += cur_len;
                entropy_array_lens[source] -= cur_len;
            }

            to_lens[dest_array_i] = to_ptr - to_ptrs[dest_array_i];
            H.LogOodle($"newLZ_get_multiarray: dest_array_i={dest_array_i} len={to_lens[dest_array_i]}");
        }

        return comp_ptr - comp_start;
    }

    private static int newLZHC_decode_chunk_phase2(int chunk_type, byte* chunk_ptr, long chunk_len, long chunk_pos, ref newLZHC_chunk_arrays arrays)
    {
        H.LogOodle($"newLZHC_decode_chunk_phase2 start: chunk_type={chunk_type} chunk_len={chunk_len} chunk_pos={chunk_pos} offsets_count={arrays.offsets_count} packets_count={arrays.packets_count} packets={(arrays.packets != null ? "single" : "multi")}");
        // SUB(0), RAW(1), LAMSUB(2), SUBAND3(3), O1(4), SUBANDF(5) supported
        if (chunk_type < 0 || chunk_type > 5) { H.LogOodle($"phase2: invalid chunk_type {chunk_type}"); return -1; }

        int start_pos = 0;
        if (chunk_pos == 0) start_pos = NEWLZ_MIN_OFFSET;

        byte* to_ptr = chunk_ptr + start_pos;
        byte* chunk_end = chunk_ptr + chunk_len;
        byte* window_base = chunk_ptr - chunk_pos;

        // Debug: for first chunk, dump first 20 and offsets around 545-550
        if (chunk_pos == 0 && arrays.offsets_count > 0)
        {
            H.LogOodle($"First chunk offsets sample: [0]={arrays.offsets[0]} [1]={arrays.offsets[1]} [2]={arrays.offsets[2]} [3]={arrays.offsets[3]} [4]={arrays.offsets[4]}");
            if (arrays.offsets_count > 550)
            {
                H.LogOodle($"Offsets around 545: [545]={arrays.offsets[545]} [546]={arrays.offsets[546]} [547]={arrays.offsets[547]} [548]={arrays.offsets[548]} [549]={arrays.offsets[549]}");
                H.LogOodle($"Offsets around 540: [540]={arrays.offsets[540]} [541]={arrays.offsets[541]} [542]={arrays.offsets[542]} [543]={arrays.offsets[543]} [544]={arrays.offsets[544]}");
            }
        }

        int* offsets_ptr = arrays.offsets;
        int* offsets_end = offsets_ptr + arrays.offsets_count;
        uint* excesses_ptr = arrays.excesses;  // LRL excesses read FORWARD from here
        uint* excesses_end_ptr = arrays.excesses + arrays.excesses_count;  // ML excesses read BACKWARD from here

        byte* literals_ptr = arrays.GetLiteralsPtr(0);
        byte* literals_ptr_lam = (chunk_type == 2) ? arrays.GetLiteralsPtr(1) : null; // LAMSUB second array

        // SUBAND3 uses 4 literal arrays indexed by (position & 3)
        byte** literals_ptrs_suband3 = stackalloc byte*[4];
        // SUBANDF uses 16 literal arrays indexed by (position & 15)
        byte** literals_ptrs_subandf = stackalloc byte*[16];

        byte** literals_ptrs_o1 = stackalloc byte*[16];
        byte* next_literals_o1 = stackalloc byte[16];

        if (chunk_type == 3) // SUBAND3
        {
            for (int i = 0; i < 4; i++)
                literals_ptrs_suband3[i] = arrays.GetLiteralsPtr(i);
        }

        if (chunk_type == 5) // SUBANDF
        {
            for (int i = 0; i < 16; i++)
                literals_ptrs_subandf[i] = arrays.GetLiteralsPtr(i);
        }

        if (chunk_type == 4)
        {
            for(int i=0;i<16;i++)
            {
                literals_ptrs_o1[i] = arrays.GetLiteralsPtr(i);
                next_literals_o1[i] = *literals_ptrs_o1[i]++;
            }
        }

        // Initialize 8 last offsets for Leviathan (0-6 for history, 7 for preloaded new offset)
        int* lastoffsets = stackalloc int[NEWLZHC_NUM_LAST_OFFSETS + 1];
        for (int i = 0; i < NEWLZHC_NUM_LAST_OFFSETS; i++)
            lastoffsets[i] = -NEWLZ_MIN_OFFSET;
        lastoffsets[NEWLZHC_NUM_LAST_OFFSETS] = 0; // Slot 7 for preloaded offset

        int neg_offset = -NEWLZ_MIN_OFFSET;

        byte* match_zone_end = chunk_end - NEWLZHC_CHUNK_NO_MATCH_ZONE;
        if (match_zone_end < chunk_ptr) match_zone_end = chunk_ptr;

        if (arrays.packets != null)
        {
            // Single packet stream mode
            byte* packets = arrays.packets;
            byte* packets_end = packets + arrays.packets_count;

            while (packets < packets_end)
            {
                // PRELOAD: stuff *offsets_ptr into lastoffsets[7] early (like C++ newLZHC_dec_LOs_Add)
                // Note: C++ does this unconditionally - bounds check happens later
                lastoffsets[NEWLZHC_NUM_LAST_OFFSETS] = *offsets_ptr;

                uint packet = *packets++;

                // Leviathan packet: 3 bits offset | 2 bits lrl | 3 bits ml
                uint lrl_part = (packet >> NEWLZHC_PACKET_ML_BITS) & NEWLZHC_PACKET_LRL_MAX;
                uint packet_offset = packet >> NEWLZHC_PACKET_OFFSET_SHIFT;
                uint ml = (packet & NEWLZHC_PACKET_ML_MAX) + NEWLZHC_LOMML;

                // Get LRL
                long lrl;
                if (lrl_part == NEWLZHC_PACKET_LRL_MAX)
                {
                    // Excess LRL - read FORWARD from excesses_ptr
                    if (excesses_ptr >= excesses_end_ptr)
                    {
                        H.LogOodle("phase2: excess LRL overflow (ignored)");
                        lrl = NEWLZHC_PACKET_LRL_MAX;
                    }
                    else
                    {
                        lrl = *excesses_ptr++;
                    }
                }
                else
                {
                    lrl = lrl_part;
                }

                // Check bounds - in careful mode, we need to_ptr + lrl <= match_zone_end
                // This ensures we don't start a match in the no-match zone
                if (to_ptr + lrl > match_zone_end) {
                    // We've gone past match_zone_end. Break out of the packet loop
                    // and let the final literal copy handle the remaining bytes.
                    // Put the packet back since we didn't process it
                    packets--;
                    break;
                }

                // Copy literals based on chunk_type (0=SUB, 1=RAW, 2=LAMSUB per SDK)
                if (chunk_type == 1) // RAW
                {
                    // Use SIMD for RAW mode
                    H.CopyBytes_SIMD(to_ptr, literals_ptr, (int)lrl);
                    to_ptr += lrl;
                    literals_ptr += lrl;
                }
                else if (chunk_type == 4) // O1
                {
                    byte val = to_ptr[-1];
                    for (long i = 0; i < lrl; i++)
                    {
                        int c = (val >> 4);
                        byte* from_ptr = literals_ptrs_o1[c];
                        val = next_literals_o1[c];
                        *to_ptr++ = val;
                        next_literals_o1[c] = *from_ptr++;
                        literals_ptrs_o1[c] = from_ptr;
                    }
                }
                else if (chunk_type == 2) // LAMSUB
                {
                    // First byte comes from LAM array (Last Actual Match)
                    if (lrl > 0)
                    {
                        byte sub = *literals_ptr_lam++;
                        byte predicted = to_ptr[neg_offset];
                        *to_ptr++ = (byte)(sub + predicted);
                        // Rest comes from main literals array like SUB - use SIMD for the rest
                        long remaining = lrl - 1;
                        if (remaining > 0)
                        {
                            H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, (int)remaining);
                            to_ptr += remaining;
                            literals_ptr += remaining;
                        }
                    }
                }
                else if (chunk_type == 3) // SUBAND3
                {
                    // SUBAND3: literal array indexed by (position & 3), SUB mode
                    for (long i = 0; i < lrl; i++)
                    {
                        int idx = ((int)(to_ptr - chunk_ptr)) & 3;
                        byte sub = *literals_ptrs_suband3[idx]++;
                        byte predicted = to_ptr[neg_offset];
                        *to_ptr++ = (byte)(sub + predicted);
                    }
                }
                else if (chunk_type == 5) // SUBANDF
                {
                    // SUBANDF: literal array indexed by (position & 15), SUB mode
                    for (long i = 0; i < lrl; i++)
                    {
                        int idx = ((int)(to_ptr - chunk_ptr)) & 15;
                        byte sub = *literals_ptrs_subandf[idx]++;
                        byte predicted = to_ptr[neg_offset];
                        *to_ptr++ = (byte)(sub + predicted);
                    }
                }
                else // SUB (type 0)
                {
                    // Use SIMD SUB copy
                    H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, (int)lrl);
                    to_ptr += lrl;
                    literals_ptr += lrl;
                }

                // Get match offset using MTF  (single packet mode)
                // packet_offset 0-6: use last offset at that index
                // packet_offset 7: use preloaded new offset from slot 7
                int lo_index = (int)packet_offset;
                neg_offset = lastoffsets[lo_index];

                if (to_ptr + neg_offset < window_base)
                {
                    H.LogOodle($"phase2: window bounds check failed. to_ptr offset < window_base");
                    return -1;
                }

                // MTF: unrolled shift - much faster than loop for 8 elements
                // Select value at lo_index, shift [0..lo_index-1] right by 1, put selected at [0]
                switch (lo_index)
                {
                    case 0: break; // No shift needed
                    case 1: lastoffsets[1] = lastoffsets[0]; break;
                    case 2: lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 3: lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 4: lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 5: lastoffsets[5] = lastoffsets[4]; lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 6: lastoffsets[6] = lastoffsets[5]; lastoffsets[5] = lastoffsets[4]; lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    default: // case 7
                        lastoffsets[7] = lastoffsets[6]; lastoffsets[6] = lastoffsets[5]; lastoffsets[5] = lastoffsets[4]; 
                        lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0];
                        offsets_ptr++; // Advance for new offset case
                        break;
                }
                lastoffsets[0] = neg_offset;

                // Get ML
                if (ml == NEWLZHC_PACKET_ML_MAX + NEWLZHC_LOMML)
                {
                    // Excess ML - read BACKWARD from excesses_end_ptr
                    if (excesses_end_ptr <= excesses_ptr)
                    {
                        H.LogOodle($"phase2: excess ML overflow (ignored). ptr={(long)excesses_ptr:X} end_ptr={(long)excesses_end_ptr:X} count={arrays.excesses_count}");
                        ml = (NEWLZHC_PACKET_ML_MAX + NEWLZHC_LOMML);
                    }
                    else
                    {
                        excesses_end_ptr--;
                        ml = *excesses_end_ptr;
                        ml += (NEWLZHC_PACKET_ML_MAX + NEWLZHC_LOMML) - NEWLZHC_PACKET_LRL_MAX;
                    }
                }


                // Check match bounds
                if (to_ptr + ml > chunk_end)
                {
                    // Clamp to end of chunk
                    long remaining = chunk_end - to_ptr;
                    if (remaining < 0) { H.LogOodle($"phase2: match bounds check failed (negative remaining)"); return -1; }
                    ml = (uint)remaining;
                }
                if (to_ptr + neg_offset < window_base)
                {
                    H.LogOodle($"phase2: window bounds check failed");
                    return -1;
                }

                // Copy match using SIMD
                byte* match_src = to_ptr + neg_offset;
                H.CopyMatch_SIMD(to_ptr, match_src, (int)ml, -neg_offset);
                to_ptr += ml;
            }
        }
        else
        {
            // Multi-packet mode
            long packets_count = arrays.packets_count;

            // Copy packet pointers, adjusting for chunk_base offset
            byte** packet_ptrs = stackalloc byte*[NEWLZHC_PACKET_POS_COUNT];
            for (int i = 0; i < NEWLZHC_PACKET_POS_COUNT; i++)
            {
                int adjusted_i = (i - (int)((long)chunk_ptr)) & NEWLZHC_PACKET_POS_MASK;
                packet_ptrs[i] = arrays.GetPacketPosPtr(adjusted_i);
            }

            while (packets_count > 0)
            {
                // PRELOAD: stuff *offsets_ptr into lastoffsets[7] early (like C++ newLZHC_dec_LOs_Add)
                // Note: C++ does this unconditionally - bounds check happens later
                lastoffsets[NEWLZHC_NUM_LAST_OFFSETS] = *offsets_ptr;

                packets_count--;

                // Get packet from appropriate stream based on position
                int packet_ptr_index = ((int)(long)to_ptr) & NEWLZHC_PACKET_POS_MASK;
                byte* packet_ptr = packet_ptrs[packet_ptr_index];
                uint packet = *packet_ptr++;
                packet_ptrs[packet_ptr_index] = packet_ptr;

                // Leviathan packet: 3 bits offset | 2 bits lrl | 3 bits ml
                uint lrl_part = (packet >> NEWLZHC_PACKET_ML_BITS) & NEWLZHC_PACKET_LRL_MAX;
                uint packet_offset = packet >> NEWLZHC_PACKET_OFFSET_SHIFT;
                uint ml = (packet & NEWLZHC_PACKET_ML_MAX) + NEWLZHC_LOMML;

                // Get LRL
                long lrl;
                if (lrl_part == NEWLZHC_PACKET_LRL_MAX)
                {
                    // Excess LRL - read FORWARD
                    if (excesses_ptr >= excesses_end_ptr) { H.LogOodle("phase2: excess LRL overflow (multi)"); return -1; }
                    lrl = *excesses_ptr++;
                }
                else
                {
                    lrl = lrl_part;
                }

                // Check bounds
                if (to_ptr + lrl > chunk_end) { H.LogOodle($"phase2: LRL bounds check failed (multi). to_ptr={to_ptr - chunk_ptr} lrl={lrl} chunk_end={chunk_end - chunk_ptr}"); return -1; }

                // Copy literals based on chunk_type (0=SUB, 1=RAW, 2=LAMSUB per SDK)
                if (chunk_type == 1) // RAW
                {
                    // Use SIMD for RAW mode
                    H.CopyBytes_SIMD(to_ptr, literals_ptr, (int)lrl);
                    to_ptr += lrl;
                    literals_ptr += lrl;
                }
                else if (chunk_type == 4) // O1
                {
                    byte val = to_ptr[-1];
                    for (long i = 0; i < lrl; i++)
                    {
                        int c = (val >> 4);
                        byte* from_ptr = literals_ptrs_o1[c];
                        val = next_literals_o1[c];
                        *to_ptr++ = val;
                        next_literals_o1[c] = *from_ptr++;
                        literals_ptrs_o1[c] = from_ptr;
                    }
                }
                else if (chunk_type == 2) // LAMSUB
                {
                    // First byte comes from LAM array (Last Actual Match)
                    if (lrl > 0)
                    {
                        byte sub = *literals_ptr_lam++;
                        *to_ptr = (byte)(sub + to_ptr[neg_offset]);
                        to_ptr++;
                        // Rest comes from main literals array like SUB - use SIMD for the rest
                        long remaining = lrl - 1;
                        if (remaining > 0)
                        {
                            H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, (int)remaining);
                            to_ptr += remaining;
                            literals_ptr += remaining;
                        }
                    }
                }
                else if (chunk_type == 3) // SUBAND3
                {
                    // SUBAND3: literal array indexed by (position & 3), SUB mode
                    for (long i = 0; i < lrl; i++)
                    {
                        int idx = ((int)(to_ptr - chunk_ptr)) & 3;
                        byte sub = *literals_ptrs_suband3[idx]++;
                        byte predicted = to_ptr[neg_offset];
                        *to_ptr++ = (byte)(sub + predicted);
                    }
                }
                else if (chunk_type == 5) // SUBANDF
                {
                    // SUBANDF: literal array indexed by (position & 15), SUB mode
                    for (long i = 0; i < lrl; i++)
                    {
                        int idx = ((int)(to_ptr - chunk_ptr)) & 15;
                        byte sub = *literals_ptrs_subandf[idx]++;
                        byte predicted = to_ptr[neg_offset];
                        *to_ptr++ = (byte)(sub + predicted);
                    }
                }
                else // SUB (type 0)
                {
                    // Use SIMD SUB copy
                    H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, (int)lrl);
                    to_ptr += lrl;
                    literals_ptr += lrl;
                }

                // Get match offset using MTF
                // packet_offset 0-6: use last offset at that index
                // packet_offset 7: use preloaded new offset from slot 7
                int lo_index = (int)packet_offset;
                neg_offset = lastoffsets[lo_index];

                // MTF: unrolled shift - much faster than loop for 8 elements
                // Select value at lo_index, shift [0..lo_index-1] right by 1, put selected at [0]
                switch (lo_index)
                {
                    case 0: break; // No shift needed
                    case 1: lastoffsets[1] = lastoffsets[0]; break;
                    case 2: lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 3: lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 4: lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 5: lastoffsets[5] = lastoffsets[4]; lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    case 6: lastoffsets[6] = lastoffsets[5]; lastoffsets[5] = lastoffsets[4]; lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0]; break;
                    default: // case 7
                        lastoffsets[7] = lastoffsets[6]; lastoffsets[6] = lastoffsets[5]; lastoffsets[5] = lastoffsets[4]; 
                        lastoffsets[4] = lastoffsets[3]; lastoffsets[3] = lastoffsets[2]; lastoffsets[2] = lastoffsets[1]; lastoffsets[1] = lastoffsets[0];
                        offsets_ptr++; // Advance for new offset case
                        break;
                }
                lastoffsets[0] = neg_offset;

                // Get ML
                if (ml == NEWLZHC_PACKET_ML_MAX + NEWLZHC_LOMML)
                {
                    // Excess ML - read BACKWARD from excesses_end_ptr
                    if (excesses_end_ptr <= excesses_ptr) { H.LogOodle("phase2: excess ML overflow (multi)"); return -1; }
                    excesses_end_ptr--;
                    ml = *excesses_end_ptr;
                    ml += (NEWLZHC_PACKET_ML_MAX + NEWLZHC_LOMML) - NEWLZHC_PACKET_LRL_MAX;
                }

                // Check match bounds
                if (to_ptr + ml > chunk_end) { H.LogOodle($"phase2: match bounds check failed (multi). to_ptr={to_ptr - chunk_ptr} ml={ml} limit={chunk_end - chunk_ptr}"); return -1; }
                if (to_ptr + neg_offset < window_base)
                {
                    H.LogOodle($"phase2: window bounds check failed (multi). to_ptr={to_ptr - chunk_ptr} neg_offset={neg_offset} window_base={window_base - chunk_ptr}");
                    H.LogOodle($"phase2: packet_offset={packet_offset} lastoffsets={lastoffsets[0]},{lastoffsets[1]},{lastoffsets[2]}...");
                    return -1;
                }

                // Copy match
                byte* match_src = to_ptr + neg_offset;
                H.CopyMatch_SIMD(to_ptr, match_src, (int)ml, -neg_offset);
                to_ptr += ml;
            }
        }

        // Final LRL - copy remaining literals
        if (to_ptr < chunk_end)
        {
            int lrl = (int)(chunk_end - to_ptr);

            if (chunk_type == 1) // RAW (type 0=SUB, type 1=RAW, type 2=LAMSUB per SDK)
            {
                // Use SIMD for RAW mode
                H.CopyBytes_SIMD(to_ptr, literals_ptr, lrl);
                to_ptr += lrl;
            }
            else if (chunk_type == 4) // O1
            {
                byte val = to_ptr[-1];
                for (int i = 0; i < lrl; i++)
                {
                    int c = (val >> 4);
                    byte* from_ptr = literals_ptrs_o1[c];
                    val = next_literals_o1[c];
                    *to_ptr++ = val;
                    next_literals_o1[c] = *from_ptr++;
                    literals_ptrs_o1[c] = from_ptr;
                }
            }
            else if (chunk_type == 2) // LAMSUB
            {
                // First byte comes from LAM array (Last Actual Match)
                if (lrl > 0)
                {
                    byte sub = *literals_ptr_lam++;
                    *to_ptr = (byte)(sub + to_ptr[neg_offset]);
                    to_ptr++;
                    lrl--;
                    // Rest comes from main literals array like SUB
                    for (int i = 0; i < lrl; i++)
                    {
                        *to_ptr = (byte)(*literals_ptr++ + to_ptr[neg_offset]);
                        to_ptr++;
                    }
                }
            }
            else if (chunk_type == 3) // SUBAND3
            {
                // SUBAND3: literal array indexed by (position & 3), SUB mode
                for (int i = 0; i < lrl; i++)
                {
                    int idx = ((int)(to_ptr - chunk_ptr)) & 3;
                    byte sub = *literals_ptrs_suband3[idx]++;
                    byte predicted = to_ptr[neg_offset];
                    *to_ptr++ = (byte)(sub + predicted);
                }
            }
            else if (chunk_type == 5) // SUBANDF
            {
                // SUBANDF: literal array indexed by (position & 15), SUB mode
                for (int i = 0; i < lrl; i++)
                {
                    int idx = ((int)(to_ptr - chunk_ptr)) & 15;
                    byte sub = *literals_ptrs_subandf[idx]++;
                    byte predicted = to_ptr[neg_offset];
                    *to_ptr++ = (byte)(sub + predicted);
                }
            }
            else // SUB (type 0)
            {
                // Use SIMD SUB copy
                H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, lrl);
                to_ptr += lrl;
            }
        }

        if (to_ptr != chunk_end) { H.LogOodle($"phase2: final check failed. to_ptr != chunk_end. diff={chunk_end - to_ptr}"); return -1; }

        return 1;
    }

    // LZB16 Constants
    private const int LZB_MML = 4;                // Minimum Match Length
    private const int LZB_LRL_ESCAPE = 15;        // LRL escape code
    private const int LZB_MLCONTROL_ESCAPE = 15;  // ML control escape code

    private static int LZB16_DecodeOneQuantum(byte* decPtr, byte* decPtrEnd, byte* comp, byte* compEnd, byte* dictionaryBase)
    {
        byte* rp = decPtr;
        byte* rpEnd = decPtrEnd;
        byte* cp = comp;

        while (rp < rpEnd)
        {
            if (cp >= compEnd) return -1;

            // Read control byte
            uint control = *cp++;
            int lrl = (int)(control & 0xF);
            int ml_control = (int)(control >> 4);

            // Handle LRL (literal run length)
            if (lrl >= LZB_LRL_ESCAPE)
            {
                // Read excess LRL using BW encoding
                if (cp >= compEnd) return -1;
                uint b = *cp++;
                if (b < 192)
                {
                    lrl += (int)b;
                }
                else
                {
                    if (cp >= compEnd) return -1;
                    lrl += (int)((b - 192) << 8);
                    lrl += *cp++;
                }
            }

            // Copy literals
            if (lrl > 0)
            {
                // NOTE: the encoder intentionally produces over-long lrls sometimes, so clamp
                if (lrl > rpEnd - rp)
                {
                    lrl = (int)(rpEnd - rp);
                }
                if (cp + lrl > compEnd) return -1;
                // Use SIMD for literal copy
                H.CopyBytes_SIMD(rp, cp, lrl);
                rp += lrl;
                cp += lrl;

                if (rp >= rpEnd) break;
            }

            // Handle match
            if (ml_control <= 8)
            {
                // Short match: read 16-bit offset
                if (cp + 2 > compEnd) return -1;
                uint off = H.ReadU16LE(cp);
                cp += 2;

                if (off == 0 || off > (uint)(rp - dictionaryBase)) return -1;

                int ml = ml_control + LZB_MML;
                if (rp + ml > rpEnd) return -1;

                byte* match = rp - off;
                // Use SIMD match copy
                H.CopyMatch_SIMD(rp, match, ml, (int)off);
                rp += ml;
            }
            else if (ml_control < LZB_MLCONTROL_ESCAPE)
            {
                // Medium match: read 16-bit offset
                if (cp + 2 > compEnd) return -1;
                uint off = H.ReadU16LE(cp);
                cp += 2;

                if (off == 0 || off > (uint)(rp - dictionaryBase)) return -1;

                int ml = ml_control + LZB_MML;
                if (rp + ml > rpEnd) return -1;

                byte* match = rp - off;
                // Use SIMD match copy
                H.CopyMatch_SIMD(rp, match, ml, (int)off);
                rp += ml;
            }
            else
            {
                // ml_control == 15: escape - read excess byte
                if (cp >= compEnd) return -1;
                uint excess_byte = *cp++;

                if ((excess_byte & 128) != 0)
                {
                    // Low offset (3-bit) + extended ML
                    ml_control = (int)((excess_byte >> 3) & 0xF);
                    int ml = ml_control + LZB_MML;

                    if (ml_control == 0xF)
                    {
                        // Read more ML using BW encoding
                        if (cp >= compEnd) return -1;
                        uint b = *cp++;
                        if (b < 192)
                        {
                            ml += (int)b;
                        }
                        else
                        {
                            if (cp >= compEnd) return -1;
                            ml += (int)((b - 192) << 8);
                            ml += *cp++;
                        }
                    }

                    uint off = excess_byte & 7;
                    if (off == 0 || off > (uint)(rp - dictionaryBase)) return -1;
                    if (rp + ml > rpEnd) return -1;

                    byte* match = rp - off;
                    // Low offset is 1-7, use CopyMatch_SIMD which handles small offsets
                    H.CopyMatch_SIMD(rp, match, ml, (int)off);
                    rp += ml;
                }
                else
                {
                    // Long offset (16-bit) + extended ML
                    if (cp + 2 > compEnd) return -1;
                    uint off = H.ReadU16LE(cp);
                    cp += 2;

                    if (off == 0 || off > (uint)(rp - dictionaryBase)) return -1;

                    int ml = ml_control + LZB_MML;
                    ml += (int)(excess_byte & 127);

                    if ((excess_byte & 127) == 127)
                    {
                        // Read more ML using BW encoding
                        if (cp >= compEnd) return -1;
                        uint b = *cp++;
                        if (b < 192)
                        {
                            ml += (int)b;
                        }
                        else
                        {
                            if (cp >= compEnd) return -1;
                            ml += (int)((b - 192) << 8);
                            ml += *cp++;
                        }
                    }

                    if (rp + ml > rpEnd) return -1;

                    byte* match = rp - off;
                    // Use SIMD match copy
                    H.CopyMatch_SIMD(rp, match, ml, (int)off);
                    rp += ml;
                }
            }
        }

        return (int)(rp - decPtr);
    }

    private static int Kraken_DecodeOneQuantum(byte* decomp, byte* decomp_end, byte* comp, int quantumCompLen, byte* compBufEnd, long pos_since_reset, byte* scratch, int scratch_size)
    {
        long decomp_len = decomp_end - decomp;
        byte* scratch_ptr = scratch;
        byte* scratch_end = scratch_ptr + scratch_size;

        byte* rawPtr = decomp;
        byte* rawEnd = decomp_end;
        byte* compPtr = comp;
        byte* compEnd = compPtr + quantumCompLen;

        const int newlz_chunk_len = 128 * 1024;

        while (rawPtr < rawEnd)
        {
            long chunk_len = Math.Min(newlz_chunk_len, rawEnd - rawPtr);
            long chunk_pos = (rawPtr - decomp) + pos_since_reset;

            if (compEnd - compPtr < 4)
            {
                return -1;
            }

            int chunk_comp_len = (int)H.ReadU24BE(compPtr);

            if (chunk_comp_len >= (1 << 23))
            {
                int chunk_type = (chunk_comp_len >> 19) & 0xF;
                chunk_comp_len &= (1 << 19) - 1;
                compPtr += 3;

                H.LogOodle($"Kraken chunk: type={chunk_type} comp_len={chunk_comp_len} chunk_len={chunk_len} chunk_pos={chunk_pos}");

                if (chunk_comp_len > compEnd - compPtr)
                {
                    return -1;
                }

                byte* chunk_comp_end = compPtr + chunk_comp_len;

                if (chunk_comp_len >= chunk_len)
                {
                    if (chunk_comp_len > chunk_len) return -1;
                    if (chunk_type != 0) return -1;

                    Buffer.MemoryCopy(compPtr, rawPtr, chunk_len, chunk_len);
                }
                else
                {
                    if (chunk_len < 128) return -1; // NEWLZ_MIN_CHUNK_LEN

                    // Check scratch size (simplified)
                    if (scratch_size < chunk_len + 256 * 1024) return -1; // Rough estimate

                    newLZ_chunk_arrays arrays = new newLZ_chunk_arrays();

                    if (newLZ_decode_chunk_phase1(chunk_type, compPtr, chunk_comp_end, rawPtr, chunk_len, chunk_pos, scratch_ptr, scratch_end, ref arrays) < 0)
                    {
                        return -1;
                    }

                    if (newLZ_decode_chunk_phase2(chunk_type, rawPtr, chunk_len, chunk_pos, ref arrays) < 0)
                    {
                        return -1;
                    }
                }

                compPtr += chunk_comp_len;
                rawPtr += chunk_len;
            }
            else
            {
                // Huff-only chunk
                long literals_len = 0;
                byte* literals = rawPtr;

                // Check for in-place overlap
                bool inplace_comp_raw_overlap = (compPtr <= (rawPtr + chunk_len) && rawPtr <= compEnd);

                if (inplace_comp_raw_overlap)
                {
                    literals = scratch_ptr;
                }

                long ret = newLZ_get_array(&literals, compPtr, compEnd, &literals_len, chunk_len, false, scratch_ptr, scratch_end);

                if (ret < 0)
                {
                    return -1;
                }

                chunk_comp_len = (int)ret;

                if (inplace_comp_raw_overlap)
                {
                    if (literals != scratch_ptr) return -1; // Should have decoded to scratch
                    Buffer.MemoryCopy(scratch_ptr, rawPtr, chunk_len, chunk_len);
                }
                else
                {
                    if (literals != rawPtr) return -1; // Should have decoded to rawPtr
                }

                if (literals_len != chunk_len)
                {
                    return -1;
                }

                compPtr += chunk_comp_len;
                rawPtr += chunk_len;
            }
        }

        return (int)decomp_len;
    }

    private static int newLZ_decode_chunk_phase1(int chunk_type, byte* comp, byte* chunk_comp_end, byte* chunk_ptr, long chunk_len, long chunk_pos, byte* scratch_space, byte* scratch_end, ref newLZ_chunk_arrays arrays)
    {
        if (chunk_type > 1) return -1;

        bool inplace_comp_raw_overlap = (comp <= (chunk_ptr + chunk_len) && chunk_ptr <= chunk_comp_end);

        arrays.chunk_ptr = chunk_ptr;
        arrays.scratch_ptr = scratch_space;

        byte* comp_ptr = comp;
        if (chunk_comp_end - comp_ptr < 13) { Console.Error.WriteLine($"P1FAIL:len13 pos={chunk_pos:X} avail={chunk_comp_end - comp_ptr:X}"); return -1; }

        byte* to_ptr = chunk_ptr;
        if (System.Environment.GetEnvironmentVariable("KYTY_NAPS_BYTELOG") == "1" &&
            System.Environment.GetEnvironmentVariable("KYTY_NAPS_BYTELOG_POS") is string want &&
            want == chunk_pos.ToString("X"))
        {
            var buf = new byte[Math.Min(48, chunk_comp_end - comp_ptr)];
            System.Runtime.InteropServices.Marshal.Copy((System.IntPtr)comp_ptr, buf, 0, buf.Length);
            Console.Error.WriteLine($"BYTELOG pos={chunk_pos:X} clen={chunk_len:X} {System.Convert.ToHexString(buf)}");
        }
        if (chunk_pos == 0)
        {
            *(ulong*)to_ptr = *(ulong*)comp_ptr;
            to_ptr += 8;
            comp_ptr += 8;
        }

        byte excess_hdr_byte = 0;
        int excess_stream_size = 0;

        if (comp_ptr < chunk_comp_end && *comp_ptr >= 0x80)
        {
            excess_hdr_byte = *comp_ptr++;
            if ((excess_hdr_byte & 0xc0) != 0x80) return -1;

            excess_stream_size = excess_hdr_byte & 0x3f;
            if (excess_stream_size >= 32)
            {
                if (comp_ptr == chunk_comp_end) { Console.Error.WriteLine($"P1FAIL:excessend pos={chunk_pos:X}"); return -1; }
                excess_stream_size += *comp_ptr++ << 5;
            }
        }

        byte* scratch_ptr = scratch_space;

        // Literals
        {
            byte* literals = scratch_ptr;
            long literals_count = 0;
            long literals_comp_len = newLZ_get_array(&literals, comp_ptr, chunk_comp_end, &literals_count, (long)(scratch_end - scratch_ptr), inplace_comp_raw_overlap, scratch_ptr, scratch_end);
            if (literals_comp_len < 0) return -1;

            comp_ptr += literals_comp_len;
            scratch_ptr += literals_count;

            arrays.literals_ptr = literals;
            arrays.literals_count = literals_count;
        }

        // Packets
        {
            byte* packets = scratch_ptr;
            long packets_count = 0;
            long packets_comp_len = newLZ_get_array(&packets, comp_ptr, chunk_comp_end, &packets_count, (long)(scratch_end - scratch_ptr), inplace_comp_raw_overlap, scratch_ptr, scratch_end);
            if (packets_comp_len < 0) return -1;

            comp_ptr += packets_comp_len;
            scratch_ptr += packets_count;

            arrays.packets = packets;
            arrays.packets_count = packets_count;
        }

        // Offsets
        byte* offsets_u8 = scratch_ptr;
        byte* offsets_u8_2 = null;
        long offsets_count = 0;
        uint offset_alt_modulo = 0;

        if (chunk_comp_end - comp_ptr < 3) return -1;

        if (*comp_ptr >= 0x80)
        {
            byte offset_alt_header_byte = *comp_ptr++;
            offset_alt_modulo = (uint)(offset_alt_header_byte - 0x80 + 1);

            long offsets_u8_comp_len = newLZ_get_array(&offsets_u8, comp_ptr, chunk_comp_end, &offsets_count, (long)(scratch_end - scratch_ptr), false, scratch_ptr, scratch_end);
            if (offsets_u8_comp_len < 0) return -1;

            comp_ptr += offsets_u8_comp_len;
            scratch_ptr += offsets_count;

            if (offset_alt_modulo != 1)
            {
                offsets_u8_2 = scratch_ptr;
                long offsets_count_2 = 0;
                long offsets_u8_comp_len_2 = newLZ_get_array(&offsets_u8_2, comp_ptr, chunk_comp_end, &offsets_count_2, (long)(scratch_end - scratch_ptr), false, scratch_ptr, scratch_end);
                if (offsets_u8_comp_len_2 < 0) return -1;

                comp_ptr += offsets_u8_comp_len_2;
                scratch_ptr += offsets_count;

                if (offsets_count != offsets_count_2) return -1;
            }
        }
        else
        {
            long offsets_u8_comp_len = newLZ_get_array(&offsets_u8, comp_ptr, chunk_comp_end, &offsets_count, (long)(scratch_end - scratch_ptr), false, scratch_ptr, scratch_end);
            if (offsets_u8_comp_len < 0) return -1;

            comp_ptr += offsets_u8_comp_len;
            scratch_ptr += offsets_count;
        }

        // Excesses
        byte* excesses_u8 = scratch_ptr;
        long excesses_count = 0;

        {
            long excesses_u8_comp_len = newLZ_get_array(&excesses_u8, comp_ptr, chunk_comp_end, &excesses_count, (long)(scratch_end - scratch_ptr), false, scratch_ptr, scratch_end);
            if (excesses_u8_comp_len < 0) return -1;

            comp_ptr += excesses_u8_comp_len;
            scratch_ptr += excesses_count;
        }

        // Align scratch for U32 arrays
        long scratch_addr = (long)scratch_ptr;
        long aligned_addr = (scratch_addr + 15) & ~15;
        scratch_ptr = (byte*)aligned_addr;

        int* offsets = (int*)scratch_ptr;
        uint* excesses = (uint*)(offsets + offsets_count);

        // Align excesses
        long excesses_addr = (long)excesses;
        long excesses_aligned = (excesses_addr + 15) & ~15;
        excesses = (uint*)excesses_aligned;

        byte* scratch_used_ptr = (byte*)(excesses + excesses_count);

        if (scratch_used_ptr + 64 > scratch_end) return -1; // NEWLZ_EXTRA_SCRATCH_MEM_FOR_FUZZ = 64

        // Zero fuzz area
        Unsafe.InitBlockUnaligned(scratch_used_ptr, 0, 64);

        if (newLZ_get_offsets_excesses(comp_ptr, chunk_comp_end, offsets_u8, offsets_u8_2, offsets_count, offset_alt_modulo, excesses_u8, excesses_count, offsets, excesses, chunk_pos + chunk_len, excess_hdr_byte, excess_stream_size) < 0)
        {
            return -1;
        }

        arrays.offsets = offsets;
        arrays.offsets_count = offsets_count;
        arrays.excesses = excesses;
        arrays.excesses_count = excesses_count;

        return 1;
    }

    private static int newLZ_decode_chunk_phase2(int chunk_type, byte* chunk_ptr, long chunk_len, long chunk_pos, ref newLZ_chunk_arrays arrays)
    {
        if (chunk_type > 1) return -1;

        int start_pos = 0;
        if (chunk_pos == 0) start_pos = NEWLZ_MIN_OFFSET;

        byte* to_ptr = chunk_ptr + start_pos;
        byte* chunk_end = chunk_ptr + chunk_len;
        byte* window_base = chunk_ptr - chunk_pos;

        H.LogOodle($"newLZ_decode_chunk_phase2: start_pos={start_pos} chunk_len={chunk_len} bytes_to_write={chunk_end - to_ptr}");

        if (!newLZ_decode_parse(ref arrays, to_ptr, chunk_ptr, chunk_end, window_base, chunk_type == 0)) // type 0 = SUB
        {
            return -1;
        }

        return 1;
    }

    private static bool newLZ_decode_parse(ref newLZ_chunk_arrays arrays, byte* to_ptr, byte* chunk_base, byte* chunk_end, byte* window_base, bool isSub)
    {
        if (chunk_end - chunk_base < 16) return false; // NEWLZ_CHUNK_NO_MATCH_ZONE

        int* offsets = arrays.offsets;
        long offsets_count = arrays.offsets_count;
        uint* excesses = arrays.excesses;
        long excesses_count = arrays.excesses_count;
        byte* packets = arrays.packets;
        long packets_count = arrays.packets_count;

        int* offsets_ptr = offsets;
        uint* excesses_ptr = excesses;
        byte* literals_ptr = arrays.literals_ptr;
        byte* literals_start = literals_ptr;
        long literals_count = arrays.literals_count;

        // Localized offsets history
        int lo0 = -8, lo1 = -8, lo2 = -8; // -NEWLZ_MIN_OFFSET
        int neg_offset = -8; // -NEWLZ_MIN_OFFSET

        byte* packets_end = packets + packets_count;
        byte* match_end = chunk_end - 8; // NEWLZ_MATCH_END_PAD

        // Padding excesses
        Unsafe.InitBlockUnaligned((byte*)(excesses + excesses_count), 3, 16); // NEWLZ_PACKET_LRL_MAX

        while (packets < packets_end)
        {
            byte packet = *packets++;

            int lrl = packet & 3;
            int packet_ml = (packet >> 2) & 0xF;
            int packet_offset = packet >> 6;

            int next_off = 0;
            if (packet_offset == 3)
            {
                if (offsets_ptr >= offsets + offsets_count) { H.LogOodle($"NAPS offsets exhausted: out={to_ptr-chunk_base}"); return false; }
                next_off = *offsets_ptr;
            }

            // LRL
            if (lrl == 3)
            {
                if (excesses_ptr >= excesses + excesses_count) { H.LogOodle($"NAPS excesses exhausted: out={to_ptr-chunk_base}"); return false; }
                uint excess_val = *excesses_ptr++;
                if (excess_val > int.MaxValue) { H.LogOodle($"NAPS excess overflow {excess_val}"); return false; }
                lrl = (int)excess_val;
            }

            if (lrl < 0 || lrl > chunk_end - to_ptr ||
                lrl > literals_count - (literals_ptr - literals_start)) {
                H.LogOodle($"NAPS literal bound: out={to_ptr-chunk_base} lrl={lrl} litused={literals_ptr-literals_start} total={literals_count}"); return false;
            }
            if (isSub && (neg_offset >= 0 || neg_offset < window_base - to_ptr)) return false;
            if (lrl > 0)
            {
                if (isSub)
                {
                    // Inline small SUB copy (common case)
                    if (lrl <= 16)
                    {
                        byte* m = to_ptr + neg_offset;
                        for (int i = 0; i < lrl; i++) to_ptr[i] = (byte)(literals_ptr[i] + m[i]);
                    }
                    else
                    {
                        H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, lrl);
                    }
                }
                else
                {
                    // Inline small RAW copy
                    if (lrl <= 16)
                    {
                         for (int i = 0; i < lrl; i++) to_ptr[i] = literals_ptr[i];
                    }
                    else
                    {
                        H.CopyBytes_SIMD(to_ptr, literals_ptr, lrl);
                    }
                }
                to_ptr += lrl;
                literals_ptr += lrl;
            }

            // Offset MTF logic scalarized
            if (packet_offset == 0)
            {
                neg_offset = lo0;
            }
            else if (packet_offset == 1)
            {
                neg_offset = lo1;
                lo1 = lo0;
                lo0 = neg_offset;
            }
            else if (packet_offset == 2)
            {
                neg_offset = lo2;
                lo2 = lo1;
                lo1 = lo0;
                lo0 = neg_offset;
            }
            else // packet_offset == 3
            {
                neg_offset = next_off;
                offsets_ptr++;
                lo2 = lo1;
                lo1 = lo0;
                lo0 = neg_offset;
            }

            if (-neg_offset < 8 || (ulong)neg_offset < (ulong)(window_base - to_ptr)) 
            {
                    H.LogOodle($"  FAIL: offset valid check failed, neg_offset={neg_offset}");
                    return false;
            }

            // Match
            int ml = packet_ml + 2; // NEWLZ_LOMML
            
            if (packet_ml == 15)
            {
                // Excess ML
                if (excesses_ptr >= excesses + excesses_count) { H.LogOodle($"NAPS excesses exhausted: out={to_ptr-chunk_base}"); return false; }
                uint excess_val = *excesses_ptr++;
                if (excess_val > int.MaxValue - 14) { H.LogOodle($"NAPS match excess overflow {excess_val}"); return false; }
                ml = 14 + (int)excess_val;
                if (to_ptr + ml > match_end) { H.LogOodle($"NAPS excess match overrun: out={to_ptr-chunk_base} ml={ml} excess={excess_val}"); return false; }
            }

            if (ml < 0 || ml > chunk_end - to_ptr) { H.LogOodle($"NAPS match bound: out={to_ptr-chunk_base} ml={ml}"); return false; }
            H.CopyMatch_SIMD(to_ptr, to_ptr + neg_offset, ml, -neg_offset);
            to_ptr += ml;
        }

        // Check whether we consumed all offsets and excesses
        if (offsets_ptr != offsets + offsets_count)
        {
            H.LogOodle($"  FAIL: offsets mismatch, consumed={(offsets_ptr - offsets)} expected={offsets_count}");
            return false;
        }
        if (excesses_ptr != excesses + excesses_count)
        {
            H.LogOodle($"  FAIL: excesses mismatch, consumed={(excesses_ptr - excesses)} expected={excesses_count}");
            return false;
        }
        // Final literals
        if (to_ptr < chunk_end)
        {
            int lrl = (int)(chunk_end - to_ptr);
            if (lrl > literals_count - (literals_ptr - literals_start)) return false;
            if (isSub && (neg_offset >= 0 || neg_offset < window_base - to_ptr)) return false;
            // Just copy what we need for the final LRL
            if (isSub)
            {
                // Use SIMD SUB copy
                H.CopySub_SIMD(to_ptr, literals_ptr, to_ptr + neg_offset, lrl);
                to_ptr += lrl;
                literals_ptr += lrl;
            }
            else
            {
                // Use SIMD for RAW mode
                H.CopyBytes_SIMD(to_ptr, literals_ptr, lrl);
                to_ptr += lrl;
                literals_ptr += lrl;
            }
        }

        return true;
    }

    // Helpers
    private static OodleLZ_Compressor OodleLZ_GetFirstChunkCompressor(byte* compBuf, long compBufSize)
    {
        LZBlockHeader header = new LZBlockHeader();
        if (LZBlockHeader_Get(ref header, compBuf, compBufSize) != null)
        {
             return OodleLZ_DecodeType_to_Compressor(header.decodeType);
        }
        return OodleLZ_Compressor.OodleLZ_Compressor_Invalid;
    }

    private static OodleLZ_Compressor OodleLZ_DecodeType_to_Compressor(int decodeType)
    {
        switch (decodeType)
        {
            case RAD_LZ_DECODE_LZHLW: return OodleLZ_Compressor.OodleLZ_Compressor_LZHLW;
            case RAD_LZ_DECODE_LZNIB: return OodleLZ_Compressor.OodleLZ_Compressor_LZNIB;
            case RAD_LZ_DECODE_LZB16: return OodleLZ_Compressor.OodleLZ_Compressor_LZB16;
            case RAD_LZ_DECODE_LZBLW: return OodleLZ_Compressor.OodleLZ_Compressor_LZBLW;
            case RAD_LZ_DECODE_LZA: return OodleLZ_Compressor.OodleLZ_Compressor_LZA;
            case RAD_LZ_DECODE_LZNA: return OodleLZ_Compressor.OodleLZ_Compressor_LZNA;
            case RAD_LZ_DECODE_KRAKEN: return OodleLZ_Compressor.OodleLZ_Compressor_Kraken;
            case RAD_LZ_DECODE_LZH: return OodleLZ_Compressor.OodleLZ_Compressor_LZH;
            case RAD_LZ_DECODE_MERMAID: return OodleLZ_Compressor.OodleLZ_Compressor_Mermaid;
            case RAD_LZ_DECODE_BITKNIT: return OodleLZ_Compressor.OodleLZ_Compressor_BitKnit;
            case RAD_LZ_DECODE_LEVIATHAN: return OodleLZ_Compressor.OodleLZ_Compressor_Leviathan;
            default: return OodleLZ_Compressor.OodleLZ_Compressor_Invalid;
        }
    }

    private static byte* LZBlockHeader_Get(ref LZBlockHeader pHeader, byte* compPtr, long compAvail)
    {
        if (compAvail < 1) return null;
        byte packedHeader = *compPtr++;

        // LZ_HEADER_BOTTOM4_VERSION4 is 12 (0xC)
        if ((packedHeader & 0xF) == 12)
        {
            pHeader.version = 4 + ((packedHeader >> 4) & 3);
            if (pHeader.version > RAD_LZ_HEADER_VERSION) return null;

            pHeader.chunkIsMemcpy = ((packedHeader >> 6) & 1) != 0;
            pHeader.chunkIsReset = ((packedHeader >> 7) & 1) != 0;

            if (compAvail < 2) return null;
            byte byte2 = *compPtr++;

            pHeader.chunkHasQuantumCRCs = ((byte2 >> 7) & 1) != 0;
            pHeader.decodeType = byte2 & 0x7F;
            pHeader.offsetShift = 0;

            if (pHeader.decodeType >= 7 && pHeader.decodeType <= 9) // RAD_LZ_DECODE_V4_LZH range
            {
                pHeader.offsetShift = pHeader.decodeType - 7;
                pHeader.decodeType = RAD_LZ_DECODE_LZH;
            }

            if (pHeader.decodeType >= RAD_LZ_DECODE_COUNT) return null;
            return compPtr;
        }
        else
        {
            // Legacy header support omitted for brevity, assuming modern Oodle
            return null;
        }
    }

    private static int LZLargeQuantumHeader_Get(byte* ptrStart, byte* ptrEnd, ref LZQuantumHeader pQH, bool doCRC, int quantumRawLen)
    {
        byte* ptr = ptrStart;
        if (ptr + 3 > ptrEnd) return -1;

        // rrGetBytes_U24 is Big Endian
        uint header = H.ReadU24BE(ptr);
        ptr += 3;

        uint packedCompLen = header & 0x3FFFF; // PACKED_LQH_FLAG_COMPLEN

        if (packedCompLen == 0x3FFFF)
        {
            // Special case
            int special = (int)(header >> 18); // PACKED_LQH_FLAG_SHIFT

            if (special == 0) // PACKED_LQH_FLAG_WHOLEMATCH
            {
                pQH.wholeMatchFlag = true;
                pQH.compLen = 0;

                // TODO: Implement whole match parsing
                return -1;
            }
            else if (special == 1) // PACKED_LQH_FLAG_MEMSET
            {
                if (ptr + 1 > ptrEnd) return -1;
                pQH.crc = *ptr++;
                pQH.compLen = 0;
            }
            else if (special == 2) // PACKED_LQH_FLAG_MEMCPY
            {
                pQH.compLen = quantumRawLen;

                if (doCRC)
                {
                    if (ptr + 3 > ptrEnd) return -1;
                    pQH.crc = H.ReadU24BE(ptr);
                    ptr += 3;
                }
            }
            else
            {
                return -1;
            }
        }
        else
        {
            pQH.compLen = (int)packedCompLen + 1;
            pQH.huffFlag = ((header >> 18) & 1) != 0;
            pQH.extraFlag = ((header >> 19) & 1) != 0;

            if (doCRC)
            {
                if (ptr + 3 > ptrEnd) return -1;
                pQH.crc = H.ReadU24BE(ptr);
                ptr += 3;
            }
        }

        return (int)(ptr - ptrStart);
    }

    private static bool LZ_DecodeType_IsLargeQuantum(int decodeType)
    {
        return decodeType == RAD_LZ_DECODE_KRAKEN ||
               decodeType == RAD_LZ_DECODE_LEVIATHAN ||
               decodeType == RAD_LZ_DECODE_MERMAID;
    }

    private static int LZQuantumHeader_Get(byte* ptrStart, byte* ptrEnd, ref LZQuantumHeader pQH, bool doCRC, int quantumRawLen)
    {
        byte* ptr = ptrStart;
        if (ptr + 2 > ptrEnd) return -1;

        // rrGetBytes_U16 is Big Endian
        ushort packedCompLen16 = H.ReadU16BE(ptr);
        ptr += 2;

        ushort packedCompLen14 = (ushort)(packedCompLen16 & 0x3FFF);

        if (packedCompLen14 == 0x3FFF)
        {
            // Special case
            int special = packedCompLen16 >> 14;

            if (special == 0) // PACKEDQH_FLAG_WHOLEMATCH (0)
            {
                pQH.wholeMatchFlag = true;
                pQH.compLen = 0;

                // rrGetVariableModPow2SeriesWBA logic needed here
                // For now, assume it's not hit in this test case
                // Or implement simplified version

                // TODO: Implement full whole match parsing
                return -1;
            }
            else if (special == 1) // PACKEDQH_FLAG_MEMSET (1)
            {
                if (ptr + 1 > ptrEnd) return -1;
                pQH.crc = *ptr++; // Use crc field to store memset value
                pQH.compLen = 0;
            }
            else if (special == 2) // PACKEDQH_FLAG_MEMCPY (2)
            {
                pQH.compLen = quantumRawLen;

                if (doCRC)
                {
                    if (ptr + 3 > ptrEnd) return -1;
                    // rrGetBytes_U24 is Big Endian
                    pQH.crc = H.ReadU24BE(ptr);
                    ptr += 3;
                }
            }
            else
            {
                return -1;
            }
        }
        else
        {
            pQH.compLen = packedCompLen14 + 1;
            pQH.huffFlag = ((packedCompLen16 >> 14) & 1) != 0;
            pQH.extraFlag = ((packedCompLen16 >> 15) & 1) != 0;

            if (doCRC)
            {
                if (ptr + 3 > ptrEnd) return -1;
                // rrGetBytes_U24 is Big Endian
                pQH.crc = H.ReadU24BE(ptr);
                ptr += 3;
            }
        }

        return (int)(ptr - ptrStart);
    }

    private static void rrVarBits_Copy(ref rrVarBits dest, ref rrVarBits src)
    {
        dest = src;
    }

    private static void rrVarBits_To_BlockBitReader(ref rrVarBits vb, ref BlockBitReader bbr)
    {
        long len_in = 63 - vb.m_inv_bitlen;
        bbr.ptr = vb.m_cur - ((len_in + 7) >> 3);
        bbr.end = vb.m_end;
        bbr.pos_in_byte = (uint)((0 - len_in) & 7);
    }

    private static void rrVarBits_From_BlockBitReader(ref rrVarBits vb, ref BlockBitReader bbr)
    {
        rrVarBits_GetOpen(ref vb, bbr.ptr, bbr.end);
        rrVarBits_Use(ref vb, bbr.pos_in_byte);
    }

    private static void rrVarBits_Use(ref rrVarBits vb, uint count)
    {
        vb.m_bits <<= (int)count;
        vb.m_inv_bitlen += (int)count;
    }

    private static int rrUnfoldNegatives(uint i)
    {
        uint x = i >> 1;
        int y = -(int)(i & 1);
        return (int)(x ^ (uint)y);
    }

    private static int rrUnfoldDeltaClamped(uint delta, int pred)
    {
        if ((int)delta <= 2 * pred)
        {
            return rrUnfoldNegatives(delta) + pred;
        }
        else
        {
            return (int)delta;
        }
    }

    private static uint rrVarBits_Get_0Ok(ref rrVarBits vb, uint count)
    {
        if (count == 0) return 0;
        return rrVarBits_Get_V(ref vb, count);
    }

    private static void small_insertion_sort(byte* ptr, byte* end)
    {
        for (byte* p = ptr + 1; p < end; p++)
        {
            byte val = *p;
            byte* q = p - 1;
            while (q >= ptr && *q > val)
            {
                *(q + 1) = *q;
                q--;
            }
            *(q + 1) = val;
        }
    }

    private static void small_insertion_sort(uint* ptr, uint* end)
    {
        for (uint* p = ptr + 1; p < end; p++)
        {
            uint val = *p;
            uint* q = p - 1;
            while (q >= ptr && (*q >> 16) > (val >> 16))
            {
                *(q + 1) = *q;
                q--;
            }
            *(q + 1) = val;
        }
    }

    private static bool newlz_tans_UnPackCounts4(int L_bits, ref newlz_tans_UnpackedCounts counts, ref rrVarBits vbl)
    {
        byte* seen = stackalloc byte[256];
        Unsafe.InitBlockUnaligned(seen, 0, 256);

        uint L = 1u << L_bits;

        int c_num_non_zero_bits = 3;
        int num_non_zero = (int)rrVarBits_Get_V(ref vbl, (uint)c_num_non_zero_bits) + 2;

        uint max_delta_bits_bits = (uint)(32 - BitOperations.LeadingZeroCount((uint)L_bits));
        int max_delta_bits = (int)rrVarBits_Get_V(ref vbl, max_delta_bits_bits);

        if (max_delta_bits == 0) { H.LogOodle("UnPackCounts4: max_delta_bits == 0"); return false; }
        if (max_delta_bits > L_bits) { H.LogOodle($"UnPackCounts4: max_delta_bits {max_delta_bits} > L_bits {L_bits}"); return false; }

        uint cur = 0;
        uint sum = 0;

        fixed (byte* pSinglesBase = counts.singles)
        fixed (uint* pLargerBase = counts.larger)
        {
            byte* cur_single = pSinglesBase;
            uint* cur_larger = pLargerBase;

            for(int i=0; i < (num_non_zero-1); i++)
            {
                int sym = (int)rrVarBits_Get_V(ref vbl, 8);
                if (seen[sym] != 0) { H.LogOodle($"UnPackCounts4: seen[{sym}] != 0"); return false; }

                uint delta = (uint)rrVarBits_Get_V(ref vbl, (uint)max_delta_bits);
                cur += delta;

                if (cur == 0) { H.LogOodle("UnPackCounts4: cur == 0"); return false; }

                seen[sym] = 1;
                if (cur == 1)
                    *cur_single++ = (byte)sym;
                else
                    *cur_larger++ = ((uint)sym << 16) | cur;

                sum += cur;
                if (sum >= L) { H.LogOodle($"UnPackCounts4: sum {sum} >= L {L}"); return false; }
            }

            int mps = (int)rrVarBits_Get_V(ref vbl, 8);
            if (seen[mps] != 0) { H.LogOodle($"UnPackCounts4: seen[{mps}] != 0 (mps)"); return false; }

            uint last = L - sum;
            if (last < cur || last <= 1) { H.LogOodle($"UnPackCounts4: last {last} < cur {cur} or <= 1"); return false; }

            *cur_larger++ = ((uint)mps << 16) | last;

            small_insertion_sort(pSinglesBase, cur_single);
            small_insertion_sort(pLargerBase, cur_larger);

            counts.num_singles = (int)(cur_single - pSinglesBase);
            counts.num_larger = (int)(cur_larger - pLargerBase);
        }

        return true;
    }

    private static bool newlz_tans_UnPackCounts_CountDelta(int L_bits, ref newlz_tans_UnpackedCounts counts, ref rrVarBits vbl)
    {
        int expGolombK = (int)rrVarBits_Get_V(ref vbl, 3);
        uint gotNumSyms = (uint)rrVarBits_Get_V(ref vbl, 8) + 1;
        if (gotNumSyms < 2) { H.LogOodle("newlz_tans_UnPackCounts_CountDelta: gotNumSyms < 2"); return false; }

        int numEG = newLZ_decode_alphabet_shape_num_EG((int)gotNumSyms, ref vbl);

        BlockBitReader bbr = new BlockBitReader();
        rrVarBits_To_BlockBitReader(ref vbl, ref bbr);

        int numUnary = (int)(gotNumSyms + numEG);
        byte* unary = stackalloc byte[numUnary + 16];

        if (newLZ_decode_unary_block(unary, numUnary, ref bbr) != numUnary) { H.LogOodle("newlz_tans_UnPackCounts_CountDelta: newLZ_decode_unary_block failed"); return false; }

        *(ulong*)(unary + numUnary) = 0;
        *(ulong*)(unary + numUnary + 8) = 0;

        rrVarBits vb = new rrVarBits();
        rrVarBits_From_BlockBitReader(ref vb, ref bbr);

        ushort* runLens = stackalloc ushort[128*2 + 1 + 8];
        int numRunPairs = newLZ_decode_alphabet_shape_runlens(runLens, gotNumSyms, (uint)numEG, unary + gotNumSyms, ref vb);
        if (numRunPairs < 0) { H.LogOodle("newlz_tans_UnPackCounts_CountDelta: newLZ_decode_alphabet_shape_runlens failed"); return false; }

        rrVarBits_Copy(ref vbl, ref vb);

        uint count_sum = 0;
        uint L = 1u << L_bits;
        int predState = (1 * 4) + 2; // COUNT_PRED_INIT(1)

        uint golombBias = 1u << expGolombK;

        fixed (byte* pSinglesBase = counts.singles)
        fixed (uint* pLargerBase = counts.larger)
        {
            byte* cur_single = pSinglesBase;
            uint* cur_larger = pLargerBase;

            byte* cur_unary = unary;
            for (int pair = 0; pair < numRunPairs; pair++)
            {
                uint i = runLens[pair*2 + 0];
                uint runLen = runLens[pair*2 + 1];

                do
                {
                    int pred = predState >> 2; // COUNT_PRED_GET

                    uint nextra = (uint)(*(cur_unary++) + expGolombK);
                    if (nextra > 15) { H.LogOodle($"newlz_tans_UnPackCounts_CountDelta: nextra > 15 ({nextra})"); return false; }

                    uint delta = (1u << (int)nextra) - golombBias;
                    delta += (uint)rrVarBits_Get_0Ok(ref vbl, (uint)nextra);

                    int count = rrUnfoldDeltaClamped(delta, pred);

                    // COUNT_PRED_UPDATE
                    predState = predState - pred + Math.Min(count, pred * 2);

                    count++;
                    if (count <= 0) { H.LogOodle($"newlz_tans_UnPackCounts_CountDelta: count <= 0 ({count})"); return false; }

                    *cur_single = (byte)i;
                    *cur_larger = (i << 16) + (uint)count;

                    if (count == 1) cur_single++;
                    else cur_larger++;

                    count_sum += (uint)count;
                    i++;
                } while (--runLen > 0);
            }

            counts.num_singles = (int)(cur_single - pSinglesBase);
            counts.num_larger = (int)(cur_larger - pLargerBase);
        }

        if (count_sum != L) { H.LogOodle($"newlz_tans_UnPackCounts_CountDelta: count_sum != L ({count_sum} != {L})"); return false; }

        return true;
    }

    private static bool newlz_tans_UnPackCounts(ref rrVarBits vb, int L_bits, ref newlz_tans_UnpackedCounts counts)
    {
        rrVarBits vbl = new rrVarBits();
        rrVarBits_Copy(ref vbl, ref vb);
        rrVarBits_Refill_Safe(ref vbl);

        bool ret = false;
        uint type = rrVarBits_Get1(ref vbl);
        if (type != 0)
        {
            H.LogOodle("newlz_tans_UnPackCounts: calling CountDelta");
            ret = newlz_tans_UnPackCounts_CountDelta(L_bits, ref counts, ref vbl);
        }
        else
        {
            H.LogOodle("newlz_tans_UnPackCounts: calling UnPackCounts4");
            ret = newlz_tans_UnPackCounts4(L_bits, ref counts, ref vbl);
        }

        if (!ret) { H.LogOodle("newlz_tans_UnPackCounts: sub-function failed"); return false; }

        rrVarBits_Copy(ref vb, ref vbl);
        return true;
    }

    private static void newlz_tans_Decoder_Fill(ref newlz_tans_Decoder tables, ref newlz_tans_UnpackedCounts counts)
    {
        int L = tables.L;
        int L_bits = tables.L_bits;

        tans_decode_entry_U8* detable = tables.decode_table_U8;
        ulong* detable64 = (ulong*)detable;

        ulong** bucket_ptr = stackalloc ulong*[1 << 2]; // BUCKET_BITS = 2
        int BUCKET_SIZE = 4;
        int BUCKET_MASK = 3;

        uint bucket_total = (uint)(L - counts.num_singles);
        uint bucket_even = bucket_total >> 2;
        uint bucket_remainder = bucket_total & 3;

        uint bucket_cum = 0;
        for (int r = 0; r < BUCKET_SIZE; r++)
        {
            bucket_ptr[r] = detable64 + bucket_cum;
            bucket_cum += bucket_even;
            if (r < bucket_remainder) bucket_cum++;
        }

        // do singletons
        {
            tans_decode_entry_U8 count_1_entry = new tans_decode_entry_U8();
            count_1_entry.nextst = 0;
            count_1_entry.len = (byte)L_bits;
            count_1_entry.mask = (uint)((1 << L_bits) - 1);
            ulong count_1_entry_64 = *(ulong*)&count_1_entry;

            ulong* pdest = detable64 + bucket_total;

            fixed (byte* pSingles = counts.singles)
            {
                byte* singleton = pSingles;
                byte* singleton_end = pSingles + counts.num_singles;

                while (singleton < singleton_end)
                {
                    *pdest = count_1_entry_64;
                    ((tans_decode_entry_U8*)pdest)->sym = *singleton;
                    pdest++;
                    singleton++;
                }
            }
        }

        // count ranks to histo
        int cumulative_count = 0;
        fixed (uint* pLarger = counts.larger)
        {
            uint* cur_count = pLarger;
            uint* larger_end = pLarger + counts.num_larger;

            while (cur_count < larger_end)
            {
                int sym = (int)(*cur_count >> 16);
                uint count = *cur_count & 0xFFFF;

                if (count <= (uint)BUCKET_SIZE)
                {
                    uint mask = (1u << (int)count) - 1;
                    int base_idx = cumulative_count & BUCKET_MASK;

                    mask <<= base_idx;
                    mask |= mask >> BUCKET_SIZE;

                    for (int c = 0; c < count; c++)
                    {
                        int r = BitOperations.TrailingZeroCount(mask);
                        mask &= mask - 1;

                        ulong* pdest = bucket_ptr[r];
                        bucket_ptr[r]++;

                        int from_state = (int)count + c;
                        int num_bits = L_bits - (31 - BitOperations.LeadingZeroCount((uint)from_state));

                        tans_decode_entry_U8* entry = (tans_decode_entry_U8*)pdest;
                        entry->sym = (byte)sym;
                        entry->len = (byte)num_bits;
                        entry->mask = (uint)((1 << num_bits) - 1);
                        entry->nextst = (ushort)((from_state << num_bits) & (L - 1));
                    }
                    cumulative_count += (int)count;
                }
                else
                {
                    int bsr_start = 31 - BitOperations.LeadingZeroCount(count);
                    int num_bits = L_bits - bsr_start;
                    int count_threshold = (1 << (bsr_start + 1)) - (int)count;

                    uint one_shifted_num_bits = 1u << num_bits;
                    uint num_bits_mask = one_shifted_num_bits - 1;

                    tans_decode_entry_U8 entry = new tans_decode_entry_U8();
                    entry.sym = (byte)sym;
                    entry.len = (byte)num_bits;
                    entry.mask = (uint)num_bits_mask;
                    entry.nextst = (ushort)((count << num_bits) & (L - 1));

                    tans_decode_entry_U8 entry_inc = new tans_decode_entry_U8();
                    entry_inc.nextst = (ushort)one_shifted_num_bits;

                    ulong entry64 = *(ulong*)&entry;
                    ulong entry64inc = *(ulong*)&entry_inc;

                    int countdown = count_threshold;
                    for (int r = 0; r < BUCKET_SIZE; r++)
                    {
                        int run_len = ((int)count + ((cumulative_count + BUCKET_MASK - r) & BUCKET_MASK)) >> 2; // BUCKET_BITS=2

                        ulong* pdest = bucket_ptr[r];

                        if (countdown < run_len)
                        {
                            for (int i = 0; i < countdown; i++)
                            {
                                *pdest++ = entry64;
                                entry64 += entry64inc;
                            }

                            num_bits--;
                            entry.len = (byte)num_bits;
                            entry.mask = (ushort)(num_bits_mask >> 1);
                            entry.nextst = 0;

                            entry64 = *(ulong*)&entry;
                            entry64inc >>= 1;

                            for (int i = 0; i < (run_len - countdown); i++)
                            {
                                *pdest++ = entry64;
                                entry64 += entry64inc;
                            }

                            countdown = (int)count;
                        }
                        else
                        {
                            for (int i = 0; i < run_len; i++)
                            {
                                *pdest++ = entry64;
                                entry64 += entry64inc;
                            }
                            countdown -= run_len;
                        }
                        bucket_ptr[r] = pdest;
                    }
                    cumulative_count += (int)count;
                }
                cur_count++;
            }
        }
    }

    private static bool tansx2_finish(ref KrakenTansState s)
    {
        long kLookaheadBytes = 2 * 3;
        long kMaxReadPerIter = 2 * 3 + 4;
        long kTailBufSize = 2 * kMaxReadPerIter + 2 * kLookaheadBytes;

        byte* tailbuf = stackalloc byte[(int)kTailBufSize];
        long final_nbytes = 0;

        byte* in0 = s.bitp[0];
        byte* in1 = s.bitp[1];

        uint bits0 = s.bits[0]; int bitc0 = (int)s.bitc[0];
        uint bits1 = s.bits[1]; int bitc1 = (int)s.bitc[1];

        byte* decodeptr = s.decodeptr;
        byte* decodeend = s.decodeend;

        if (in1 < in0)
        {
            H.LogOodle($"tansx2_finish: in1 < in0. in_left={in1-in0} missing_symbols={decodeend-decodeptr}");
            return false;
        }

        uint tans0 = s.tans_state[0];
        uint tans1 = s.tans_state[1];
        uint tans2 = s.tans_state[2];
        uint tans3 = s.tans_state[3];
        uint tans4 = s.tans_state[4];
        tans_decode_entry_U8* table = s.table;

        while (decodeptr < decodeend)
        {
            long in_left = in1 - in0;

            if (in_left < kMaxReadPerIter)
            {
                if (final_nbytes != 0)
                {
                    H.LogOodle($"tansx2_finish: final_nbytes!=0. in_left={in_left} missing_symbols={decodeend-decodeptr}");
                    return false;
                }

                in0 -= bitc0 >> 3; bitc0 &= 7;
                in1 += bitc1 >> 3; bitc1 &= 7;
                in_left = in1 - in0;
                if (in_left < 0)
                {
                    H.LogOodle($"tansx2_finish: in_left < 0. in_left={in_left}");
                    return false;
                }

                if (in_left >= kMaxReadPerIter + kLookaheadBytes) return false;
                if (in_left * 2 >= kTailBufSize) return false;

                Buffer.MemoryCopy(in0, tailbuf, kTailBufSize, in_left);
                Buffer.MemoryCopy(in1 - in_left, tailbuf + kTailBufSize - in_left, kTailBufSize, in_left);

                in0 = tailbuf;
                in1 = tailbuf + kTailBufSize;

                final_nbytes = kTailBufSize - in_left;
                if (final_nbytes == 0) return false;
            }

            int len;
            tans_decode_entry_U8 entry;

            // REFILL0
            bits0 |= H.ReadU32LE(in0) << bitc0;
            in0 += (31 - bitc0) >> 3;
            bitc0 |= 24;

            // DECONE tans0 - cache table entry
            entry = table[tans0];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans0 = (uint)((bits0 & entry.mask) + entry.nextst);
            bits0 >>= len;
            bitc0 -= len;
            if (decodeptr >= decodeend) break;

            // DECONE tans1
            entry = table[tans1];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans1 = (uint)((bits0 & entry.mask) + entry.nextst);
            bits0 >>= len;
            bitc0 -= len;
            if (decodeptr >= decodeend) break;

            // REFILL0
            bits0 |= H.ReadU32LE(in0) << bitc0;
            in0 += (31 - bitc0) >> 3;
            bitc0 |= 24;

            // DECONE tans2
            entry = table[tans2];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans2 = (uint)((bits0 & entry.mask) + entry.nextst);
            bits0 >>= len;
            bitc0 -= len;
            if (decodeptr >= decodeend) break;

            // DECONE tans3
            entry = table[tans3];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans3 = (uint)((bits0 & entry.mask) + entry.nextst);
            bits0 >>= len;
            bitc0 -= len;
            if (decodeptr >= decodeend) break;

            // REFILL0
            bits0 |= H.ReadU32LE(in0) << bitc0;
            in0 += (31 - bitc0) >> 3;
            bitc0 |= 24;

            // DECONE tans4
            entry = table[tans4];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans4 = (uint)((bits0 & entry.mask) + entry.nextst);
            bits0 >>= len;
            bitc0 -= len;
            if (decodeptr >= decodeend) break;

            // REFILL1
            bits1 |= H.ReadU32BE(in1 - 4) << bitc1;
            in1 -= (31 - bitc1) >> 3;
            bitc1 |= 24;

            // DECONE tans0
            entry = table[tans0];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans0 = (uint)((bits1 & entry.mask) + entry.nextst);
            bits1 >>= len;
            bitc1 -= len;
            if (decodeptr >= decodeend) break;

            // DECONE tans1
            entry = table[tans1];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans1 = (uint)((bits1 & entry.mask) + entry.nextst);
            bits1 >>= len;
            bitc1 -= len;
            if (decodeptr >= decodeend) break;

            // REFILL1
            bits1 |= H.ReadU32BE(in1 - 4) << bitc1;
            in1 -= (31 - bitc1) >> 3;
            bitc1 |= 24;

            // DECONE tans2
            entry = table[tans2];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans2 = (uint)((bits1 & entry.mask) + entry.nextst);
            bits1 >>= len;
            bitc1 -= len;
            if (decodeptr >= decodeend) break;

            // DECONE tans3
            entry = table[tans3];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans3 = (uint)((bits1 & entry.mask) + entry.nextst);
            bits1 >>= len;
            bitc1 -= len;
            if (decodeptr >= decodeend) break;

            // REFILL1
            bits1 |= H.ReadU32BE(in1 - 4) << bitc1;
            in1 -= (31 - bitc1) >> 3;
            bitc1 |= 24;

            // DECONE tans4
            entry = table[tans4];
            *decodeptr++ = entry.sym;
            len = (int)entry.len;
            tans4 = (uint)((bits1 & entry.mask) + entry.nextst);
            bits1 >>= len;
            bitc1 -= len;
        }

        in0 -= bitc0 >> 3;
        in1 += bitc1 >> 3;

        if ((in1 - in0) != final_nbytes) return false;
        if ((tans0 | tans1 | tans2 | tans3 | tans4) > 255) return false;

        // Final 5 states have 5 more bytes - write as 1 uint + 1 byte instead of 5 individual bytes
        H.Write5BytesLE(decodeend, tans0, tans1, tans2, tans3, tans4);

        return true;
    }

    private static bool tansx2_64(ref KrakenTansState s)
    {
        byte* decodeptr = s.decodeptr;
        byte* decodeend = s.decodeend;
        byte* in0 = s.bitp[0];
        byte* in1 = s.bitp[1];

        if (in1 - in0 >= 8 && decodeend - decodeptr >= 10)
        {
            ulong bits0 = s.bits[0]; uint bitc0 = s.bitc[0];
            ulong bits1 = s.bits[1]; uint bitc1 = s.bitc[1];

            uint tans0 = s.tans_state[0];
            uint tans1 = s.tans_state[1];
            uint tans2 = s.tans_state[2];
            uint tans3 = s.tans_state[3];
            uint tans4 = s.tans_state[4];
            tans_decode_entry_U8* table = s.table;

            in1 -= 8;
            decodeend -= 10;

            while (in0 <= in1 && decodeptr <= decodeend)
            {
                uint len;
                tans_decode_entry_U8 entry;

                bits0 |= H.ReadU64LE(in0) << (int)bitc0;
                in0 += (63 - (int)bitc0) >> 3;
                bitc0 |= 56;

                bits1 |= H.ReadU64BE(in1) << (int)bitc1;
                in1 -= (63 - (int)bitc1) >> 3;
                bitc1 |= 56;

                // DECONE 0 - cache table entry to avoid 4 reads per decode
                entry = table[tans0];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans0 = (uint)((bits0 & entry.mask) + entry.nextst);
                bits0 >>= (int)len;
                bitc0 -= len;

                entry = table[tans1];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans1 = (uint)((bits0 & entry.mask) + entry.nextst);
                bits0 >>= (int)len;
                bitc0 -= len;

                entry = table[tans2];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans2 = (uint)((bits0 & entry.mask) + entry.nextst);
                bits0 >>= (int)len;
                bitc0 -= len;

                entry = table[tans3];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans3 = (uint)((bits0 & entry.mask) + entry.nextst);
                bits0 >>= (int)len;
                bitc0 -= len;

                entry = table[tans4];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans4 = (uint)((bits0 & entry.mask) + entry.nextst);
                bits0 >>= (int)len;
                bitc0 -= len;

                // DECONE 1 - cache table entry
                entry = table[tans0];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans0 = (uint)((bits1 & entry.mask) + entry.nextst);
                bits1 >>= (int)len;
                bitc1 -= len;

                entry = table[tans1];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans1 = (uint)((bits1 & entry.mask) + entry.nextst);
                bits1 >>= (int)len;
                bitc1 -= len;

                entry = table[tans2];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans2 = (uint)((bits1 & entry.mask) + entry.nextst);
                bits1 >>= (int)len;
                bitc1 -= len;

                entry = table[tans3];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans3 = (uint)((bits1 & entry.mask) + entry.nextst);
                bits1 >>= (int)len;
                bitc1 -= len;

                entry = table[tans4];
                *decodeptr++ = entry.sym;
                len = entry.len;
                tans4 = (uint)((bits1 & entry.mask) + entry.nextst);
                bits1 >>= (int)len;
                bitc1 -= len;
            }

            in1 += 8;
            decodeend += 10; // Restore decodeend

            s.decodeptr = decodeptr;
            s.decodeend = decodeend;  // FIX: Also restore decodeend to state
            s.bitp[0] = in0 - (bitc0 >> 3);
            s.bitp[1] = in1 + (bitc1 >> 3);
            s.bits[0] = (uint)bits0;
            s.bits[1] = (uint)bits1;
            s.bitc[0] = bitc0 & 7;
            s.bitc[1] = bitc1 & 7;
            s.tans_state[0] = tans0;
            s.tans_state[1] = tans1;
            s.tans_state[2] = tans2;
            s.tans_state[3] = tans3;
            s.tans_state[4] = tans4;
        }

        H.LogOodle($"tansx2_64: loop ended. in0={ (long)s.bitp[0] } in1={ (long)s.bitp[1] } decodeptr={ (long)s.decodeptr } decodeend={ (long)s.decodeend }");

        H.LogOodle($"tansx2_64: calling tansx2_finish, remaining={s.decodeend - s.decodeptr}");
        if (!tansx2_finish(ref s)) { H.LogOodle("tansx2_64: tansx2_finish failed"); return false; }
        return true;
    }

    private static long newlz_get_array_tans(byte* comp, long comp_len, byte* to, long to_len, byte* scratch_ptr, byte* scratch_end)
    {
        byte* comp_ptr = comp;
        byte* comp_end = comp + comp_len;

        if (comp_len < 8) { H.LogOodle("newlz_get_array_tans: comp_len < 8"); return -1; }
        if (to_len < 5) { H.LogOodle("newlz_get_array_tans: to_len < 5"); return -1; }

        int L_bits;
        newlz_tans_UnpackedCounts unpacked_counts = new newlz_tans_UnpackedCounts();

        {
            rrVarBits header_vb = new rrVarBits();
            rrVarBits_GetOpen(ref header_vb, comp_ptr, comp_end);

            uint flag = rrVarBits_Get1(ref header_vb);
            if (flag != 0) { H.LogOodle("newlz_get_array_tans: flag != 0"); return -1; }

            L_bits = (int)rrVarBits_Get_C(ref header_vb, NEWLZ_TANS_L_BITS_BITS);
            L_bits += NEWLZ_TANS_L_BITS_MIN;
            if (L_bits > NEWLZ_TANS_L_BITS_MAX) { H.LogOodle($"newlz_get_array_tans: L_bits invalid ({L_bits})"); return -1; }
            H.LogOodle($"newlz_get_array_tans: L_bits={L_bits}");

            if (!newlz_tans_UnPackCounts(ref header_vb, L_bits, ref unpacked_counts)) { H.LogOodle("newlz_get_array_tans: newlz_tans_UnPackCounts failed"); return -1; }

            comp_ptr = header_vb.m_cur - ((63 - header_vb.m_inv_bitlen) >> 3); // rrVarBits_GetEndPtr
        }

        long tans_compLen = comp_end - comp_ptr;
        H.LogOodle($"newlz_get_array_tans: tans_compLen={tans_compLen} header_size={comp_ptr - comp}");

        byte* comp_scratch = stackalloc byte[2 * 8 + 8];
        if (tans_compLen < 8)
        {
            if (tans_compLen <= 0) { H.LogOodle("newlz_get_array_tans: tans_compLen <= 0"); return -1; }
            Unsafe.InitBlockUnaligned(comp_scratch, 0, (uint)(2 * 8 + 8));
            Buffer.MemoryCopy(comp_ptr, comp_scratch + 8, tans_compLen, tans_compLen);
            comp_ptr = comp_scratch + 8;
            comp_end = comp_ptr + tans_compLen;
        }

        long decmemsize = (1L << L_bits) * sizeof(tans_decode_entry_U8);
        decmemsize = (decmemsize + 15) & ~15; // Align up

        if ((scratch_end - scratch_ptr) < 16) { H.LogOodle("newlz_get_array_tans: scratch too small (1)"); return -1; }
        scratch_ptr = (byte*)(((long)scratch_ptr + 15) & ~15);
        if ((scratch_end - scratch_ptr) < decmemsize) { H.LogOodle($"newlz_get_array_tans: scratch too small (2) needed={decmemsize} avail={scratch_end - scratch_ptr}"); return -1; }

        newlz_tans_Decoder dec = new newlz_tans_Decoder();
        dec.L = 1 << L_bits;
        dec.L_bits = L_bits;
        dec.decode_table_U8 = (tans_decode_entry_U8*)scratch_ptr;

        newlz_tans_Decoder_Fill(ref dec, ref unpacked_counts);

        {
            byte* lvb1_ptr = comp_ptr;
            ulong lvb1_bits = H.ReadU64LE(lvb1_ptr);
            int lvb1_bitcnt = 64;
            lvb1_ptr += 8;

            byte* lvb2_ptr = comp_end - 8;
            ulong lvb2_bits = H.ReadU64BE(lvb2_ptr);
            int lvb2_bitcnt = 64;
            lvb2_ptr -= 8;

            uint x1, x2, x3, x4, x5;

            // GetBits_LSB(x1, lvb1, L_bits)
            x1 = (uint)(lvb1_bits & ((1ul << L_bits) - 1));
            lvb1_bits >>= L_bits; lvb1_bitcnt -= L_bits;

            x2 = (uint)(lvb2_bits & ((1ul << L_bits) - 1));
            lvb2_bits >>= L_bits; lvb2_bitcnt -= L_bits;

            x3 = (uint)(lvb1_bits & ((1ul << L_bits) - 1));
            lvb1_bits >>= L_bits; lvb1_bitcnt -= L_bits;

            x4 = (uint)(lvb2_bits & ((1ul << L_bits) - 1));
            lvb2_bits >>= L_bits; lvb2_bitcnt -= L_bits;

            // Refill32_LSB_LE_Forward(lvb1) - not needed for 64-bit regs if we have enough bits?
            // We consumed 2*L_bits <= 22 bits. 64-22 = 42 bits left. Enough for another L_bits.

            x5 = (uint)(lvb1_bits & ((1ul << L_bits) - 1));
            lvb1_bits >>= L_bits; lvb1_bitcnt -= L_bits;

            to_len -= 5;

            KrakenTansState s = new KrakenTansState();
            s.decodeptr = to;
            s.decodeend = to + to_len;

            byte** bitp_arr = stackalloc byte*[2];
            uint* bits_arr = stackalloc uint[2];
            uint* bitc_arr = stackalloc uint[2];

            s.bitp = bitp_arr;
            s.bits = bits_arr;
            s.bitc = bitc_arr;

            s.bitc[0] = (uint)(lvb1_bitcnt & 7);
            s.bitc[1] = (uint)(lvb2_bitcnt & 7);

            s.bitp[0] = lvb1_ptr - (lvb1_bitcnt >> 3);
            s.bitp[1] = lvb2_ptr + 8 + (lvb2_bitcnt >> 3);

            H.LogOodle($"newlz_get_array_tans: lvb1_bitcnt={lvb1_bitcnt} lvb2_bitcnt={lvb2_bitcnt}");
            H.LogOodle($"newlz_get_array_tans: s.bitp[0]={(long)s.bitp[0]} s.bitp[1]={(long)s.bitp[1]}");

            // Store full 32 bits of the remaining bit buffer (C++ just casts to U32)
            s.bits[0] = (uint)lvb1_bits;
            s.bits[1] = (uint)lvb2_bits;

            s.table = dec.decode_table_U8;
            s.tans_state[0] = x1;
            s.tans_state[1] = x2;
            s.tans_state[2] = x3;
            s.tans_state[3] = x4;
            s.tans_state[4] = x5;

            if (true) // Use tansx2_64 fast path
            {
                if (!tansx2_64(ref s)) { H.LogOodle("newlz_get_array_tans: tansx2_64 failed"); return -1; }
            }
            /*else
            {
                //H.LogOodle($"newlz_get_array_tans: L_bits={L_bits}, skipping tansx2_64");
                if (!tansx2_finish(ref s)) { H.LogOodle("newlz_get_array_tans: tansx2_finish failed"); return -1; }
            }*/
        }

        return comp_len;
    }

    private static long newLZ_get_array_split(byte* comp_start, long comp_len, byte* to, long to_len, byte* scratch_ptr, byte* scratch_end)
    {
        if (comp_len < 6) return -1;

        byte* comp_ptr = comp_start;
        byte* comp_end = comp_ptr + comp_len;

        int num_splits = *comp_ptr;
        int is_indexed = num_splits & 0x80;
        num_splits &= 0x7F;
        
        H.LogOodle($"newLZ_get_array_split: num_splits={num_splits} is_indexed={is_indexed} to_len={to_len}");

        if (num_splits < 2) return -1;

        if (is_indexed != 0)
        {
            H.LogOodle($"newLZ_get_array_split: using indexed path (multiarray)");
            byte** to_ptrs = stackalloc byte*[1];
            long* to_lens = stackalloc long[1];
            long tot_to_len;

            long used_comp_len = newLZ_get_multiarray(comp_start, comp_end, to, to + to_len, to_ptrs, to_lens, 1, &tot_to_len, true, scratch_ptr, scratch_end);

            if (used_comp_len < 0) return -1;
            if (to_lens[0] != to_len) return -1;

            return used_comp_len;
        }

        comp_ptr++;

        byte* to_ptr = to;
        byte* to_ptr_end = to + to_len;

        H.LogOodle($"newLZ_get_array_split: num_splits={num_splits} to_len={to_len}");

        for (int i = 0; i < num_splits; i++)
        {
            long cur_to_len;
            long cur_comp_len = newLZ_get_array(&to_ptr, comp_ptr, comp_end, &cur_to_len, to_ptr_end - to_ptr, true, scratch_ptr, scratch_end);
            H.LogOodle($"newLZ_get_array_split: split {i} cur_to_len={cur_to_len} cur_comp_len={cur_comp_len} to_ptr offset={(long)(to_ptr - to)}");
            if (cur_comp_len < 0) return -1;

            to_ptr += cur_to_len;
            comp_ptr += cur_comp_len;
        }

        H.LogOodle($"newLZ_get_array_split: after loop to_ptr offset={(long)(to_ptr - to)} expected={to_len}");
        if (to_ptr != to_ptr_end) return -1;

        return comp_ptr - comp_start;
    }
}
