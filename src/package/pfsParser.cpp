#include "package/pfsParser.h"
#include <functional>
#include "common/logging/log.h"
#include "IO/Decompressor.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <span>
#include <sstream>

#ifdef KYTY_HAS_ZLIB
#include <zlib.h>
#define KYTY_PFS_HAS_ZLIB 1
#else
#define KYTY_PFS_HAS_ZLIB 0
#endif

namespace Libs::Firmware {

// ============================================================================
// Self-contained AES-128 implementation (for AES-XTS decryption)
// Self-contained AES-XTS implementation (FIPS 197 tables).
// ============================================================================

namespace {

// Little-endian reads with explicit bounds checks (no reinterpret_cast on
// unaligned/truncated buffers).
static uint32_t ReadU32LE(const uint8_t* p) {
	uint32_t v = 0;
	std::memcpy(&v, p, sizeof(v));
	return v;
}
static uint64_t ReadU64LE(const uint8_t* p) {
	uint64_t v = 0;
	std::memcpy(&v, p, sizeof(v));
	return v;
}

// Returns true if a PFS file path is safe to extract under the output
// directory. File names come from the package and are untrusted: reject
// absolute paths, "."/".." components, empty components, backslashes and
// Windows drive letters so a hostile package cannot write outside output_dir.
bool IsSafeRelativePath(const std::string& name) {
	if (name.empty() || name.front() == '/') {
		return false;
	}
	size_t pos = 0;
	while (true) {
		const size_t next = name.find('/', pos);
		const size_t end  = (next == std::string::npos) ? name.size() : next;
		if (end == pos) {
			return false; // empty component (leading/double/trailing slash)
		}
		const std::string comp = name.substr(pos, end - pos);
		if (comp == "." || comp == "..") {
			return false;
		}
		if (comp.find('\\') != std::string::npos || comp.find(':') != std::string::npos) {
			return false;
		}
		if (next == std::string::npos) {
			break;
		}
		pos = next + 1;
	}
	return true;
}

static constexpr uint8_t kAesSbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static constexpr uint8_t kAesRcon[11] = {
    0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

using AesBlock = std::array<uint8_t, 16>;
using AesRoundKeys = std::array<uint8_t, 176>;

static uint8_t GfMul2(uint8_t b) {
    return static_cast<uint8_t>((b << 1) ^ ((b & 0x80) ? 0x1b : 0x00));
}

static uint8_t GfMul3(uint8_t b) {
    return GfMul2(b) ^ b;
}

static AesRoundKeys AesKeyExpand(const std::array<uint8_t, 16>& key) {
    AesRoundKeys rk {};
    std::memcpy(rk.data(), key.data(), 16);
    for (int i = 4; i < 44; ++i) {
        uint8_t tmp[4];
        std::memcpy(tmp, rk.data() + (i - 1) * 4, 4);
        if (i % 4 == 0) {
            const uint8_t t = tmp[0];
            tmp[0] = kAesSbox[tmp[1]] ^ kAesRcon[i / 4];
            tmp[1] = kAesSbox[tmp[2]];
            tmp[2] = kAesSbox[tmp[3]];
            tmp[3] = kAesSbox[t];
        }
        for (int j = 0; j < 4; ++j) {
            rk[i * 4 + j] = rk[(i - 4) * 4 + j] ^ tmp[j];
        }
    }
    return rk;
}

static void AesSubBytes(uint8_t state[16]) {
    for (int i = 0; i < 16; ++i) state[i] = kAesSbox[state[i]];
}

static void AesShiftRows(uint8_t state[16]) {
    uint8_t t = state[1]; state[1]=state[5]; state[5]=state[9]; state[9]=state[13]; state[13]=t;
    std::swap(state[2], state[10]); std::swap(state[6], state[14]);
    t = state[15]; state[15]=state[11]; state[11]=state[7]; state[7]=state[3]; state[3]=t;
}

static void AesMixColumns(uint8_t state[16]) {
    for (int c = 0; c < 4; ++c) {
        uint8_t* col = state + c * 4;
        uint8_t s0 = col[0], s1 = col[1], s2 = col[2], s3 = col[3];
        col[0] = GfMul2(s0) ^ GfMul3(s1) ^ s2 ^ s3;
        col[1] = s0 ^ GfMul2(s1) ^ GfMul3(s2) ^ s3;
        col[2] = s0 ^ s1 ^ GfMul2(s2) ^ GfMul3(s3);
        col[3] = GfMul3(s0) ^ s1 ^ s2 ^ GfMul2(s3);
    }
}

static void AesAddRoundKey(uint8_t state[16], const uint8_t* rk) {
    for (int i = 0; i < 16; ++i) state[i] ^= rk[i];
}

static AesBlock AesEncryptBlock(const AesBlock& in, const AesRoundKeys& rk) {
    uint8_t state[16];
    std::memcpy(state, in.data(), 16);
    AesAddRoundKey(state, rk.data());
    for (int round = 1; round < 10; ++round) {
        AesSubBytes(state);
        AesShiftRows(state);
        AesMixColumns(state);
        AesAddRoundKey(state, rk.data() + round * 16);
    }
    AesSubBytes(state);
    AesShiftRows(state);
    AesAddRoundKey(state, rk.data() + 10 * 16);
    AesBlock out;
    std::memcpy(out.data(), state, 16);
    return out;
}

// AES-XTS: tweak-based encryption. For PFS, we only need decryption.
// XTS processes data in 16-byte blocks, using a tweak that's encrypted
// and then multiplied by GF(2^128) powers.
static void GfMul128(uint8_t* out, const uint8_t* in) {
    // Multiply in GF(2^128) with polynomial x^7 + x^4 + x^3 + x + 1 (0x87)
    uint8_t carry = 0;
    for (int i = 0; i < 16; ++i) {
        uint8_t next_carry = (in[i] >> 7) & 1;
        out[i] = (in[i] << 1) | carry;
        carry = next_carry;
    }
    if (carry) {
        out[0] ^= 0x87;
    }
}

} // anonymous namespace

// ============================================================================
// PfsParser implementation
// ============================================================================

bool PfsParser::HasPfsMagic(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    return data[0] == 0xB3 && data[1] == 0x26 && data[2] == 0x39 && data[3] == 0x01;
}

bool PfsParser::HasPfscMagic(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    return data[0] == 0x50 && data[1] == 0x46 && data[2] == 0x53 && data[3] == 0x43;
}

// ---- AES-XTS decryption ----

std::vector<uint8_t> PfsParser::AesXtsDecryptSector(
    const uint8_t* sector_data, size_t sector_size,
    const PfsEkpfsKey& key, uint64_t sector_number) {

    const AesRoundKeys data_rk = AesKeyExpand(key.data_key);
    const AesRoundKeys tweak_rk = AesKeyExpand(key.tweak_key);

    // Encrypt the tweak (sector number as little-endian 16 bytes)
    AesBlock tweak_input;
    std::memset(tweak_input.data(), 0, 16);
    for (int i = 0; i < 8; ++i) {
        tweak_input[i] = static_cast<uint8_t>((sector_number >> (i * 8)) & 0xFF);
    }
    AesBlock tweak = AesEncryptBlock(tweak_input, tweak_rk);

    // Inverse S-box for AES decryption
    static constexpr uint8_t kInvSbox[256] = {
        0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
        0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
        0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
        0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
        0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
        0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
        0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
        0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
        0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
        0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
        0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
        0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
        0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
        0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
        0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
        0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
    };

    // GF(2^8) multiply helpers for InvMixColumns
    auto gf_mul = [](uint8_t a, uint8_t b) -> uint8_t {
        uint8_t p = 0;
        for (int i = 0; i < 8; ++i) {
            if (b & 1) p ^= a;
            uint8_t hi = a & 0x80;
            a <<= 1;
            if (hi) a ^= 0x1b;
            b >>= 1;
        }
        return p;
    };

    std::vector<uint8_t> out(sector_size);
    AesBlock current_tweak = tweak;
    size_t offset = 0;

    while (offset + 16 <= sector_size) {
        uint8_t block[16];
        std::memcpy(block, sector_data + offset, 16);

        // XOR with tweak
        for (int i = 0; i < 16; ++i) block[i] ^= current_tweak[i];

        // AES-ECB Decrypt (inverse cipher)
        // AddRoundKey (last round key first)
        for (int i = 0; i < 16; ++i) block[i] ^= data_rk[10 * 16 + i];

        // Inverse rounds (9 down to 1)
        for (int round = 9; round >= 1; --round) {
            // InvShiftRows
            uint8_t t = block[13]; block[13]=block[9]; block[9]=block[5]; block[5]=block[1]; block[1]=t;
            std::swap(block[2], block[10]); std::swap(block[6], block[14]);
            t = block[3]; block[3]=block[7]; block[7]=block[11]; block[11]=block[15]; block[15]=t;

            // InvSubBytes
            for (int i = 0; i < 16; ++i) block[i] = kInvSbox[block[i]];

            // AddRoundKey
            for (int i = 0; i < 16; ++i) block[i] ^= data_rk[round * 16 + i];

            // InvMixColumns
            for (int c = 0; c < 4; ++c) {
                uint8_t* col = block + c * 4;
                uint8_t s0 = col[0], s1 = col[1], s2 = col[2], s3 = col[3];
                col[0] = gf_mul(0x0e, s0) ^ gf_mul(0x0b, s1) ^ gf_mul(0x0d, s2) ^ gf_mul(0x09, s3);
                col[1] = gf_mul(0x09, s0) ^ gf_mul(0x0e, s1) ^ gf_mul(0x0b, s2) ^ gf_mul(0x0d, s3);
                col[2] = gf_mul(0x0d, s0) ^ gf_mul(0x09, s1) ^ gf_mul(0x0e, s2) ^ gf_mul(0x0b, s3);
                col[3] = gf_mul(0x0b, s0) ^ gf_mul(0x0d, s1) ^ gf_mul(0x09, s2) ^ gf_mul(0x0e, s3);
            }
        }

        // Final InvShiftRows + InvSubBytes + AddRoundKey
        {
            uint8_t t = block[13]; block[13]=block[9]; block[9]=block[5]; block[5]=block[1]; block[1]=t;
            std::swap(block[2], block[10]); std::swap(block[6], block[14]);
            t = block[3]; block[3]=block[7]; block[7]=block[11]; block[11]=block[15]; block[15]=t;
            for (int i = 0; i < 16; ++i) block[i] = kInvSbox[block[i]];
            for (int i = 0; i < 16; ++i) block[i] ^= data_rk[i];
        }

        // XOR with tweak again
        for (int i = 0; i < 16; ++i) out[offset + i] = block[i] ^ current_tweak[i];

        // Advance tweak: T = T * alpha in GF(2^128)
        GfMul128(current_tweak.data(), current_tweak.data());

        offset += 16;
    }

    // Handle remaining bytes (partial block — XTS ciphertext stealing not implemented)
    if (offset < sector_size) {
        for (size_t i = offset; i < sector_size; ++i) {
            out[i] = sector_data[i];
        }
    }

    return out;
}
// ---- Block reading with optional decryption ----

std::vector<uint8_t> PfsParser::ReadBlock(
    std::ifstream& f, int64_t block_num, uint32_t block_size,
    uint32_t num_blocks, const PfsEkpfsKey* ekpfs_key) {

    if (block_num < 0 || static_cast<uint32_t>(block_num) >= num_blocks) {
        return {};
    }

    const uint64_t offset = static_cast<uint64_t>(block_num) * block_size;
    f.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    std::vector<uint8_t> raw(block_size);
    f.read(reinterpret_cast<char*>(raw.data()), block_size);
    const auto got = static_cast<size_t>(f.gcount());
    raw.resize(got);

    if (ekpfs_key && !raw.empty()) {
        // Decrypt each XTS sector within the block
        const uint32_t sectors_per_block = block_size / PFS_XTS_SECTOR_SIZE;
        const uint64_t base_sector = offset / PFS_XTS_SECTOR_SIZE;

        for (uint32_t s = 0; s < sectors_per_block; ++s) {
            const size_t sec_offset = s * PFS_XTS_SECTOR_SIZE;
            if (sec_offset + PFS_XTS_SECTOR_SIZE > raw.size()) break;

            auto decrypted = AesXtsDecryptSector(
                raw.data() + sec_offset, PFS_XTS_SECTOR_SIZE,
                *ekpfs_key, base_sector + s);

            std::copy(decrypted.begin(), decrypted.end(), raw.begin() + sec_offset);
        }
    }

    return raw;
}

// ---- PFSC decompression ----

std::vector<uint8_t> PfsParser::DecompressPfscBlock(const std::vector<uint8_t>& raw_block) {
#if KYTY_PFS_HAS_ZLIB
    // Check for PFSC header
    if (raw_block.size() < PFSC_HEADER_SIZE) return raw_block;

    // PFSC magic check
    const uint32_t magic = *reinterpret_cast<const uint32_t*>(raw_block.data());
    if (magic != PFSC_MAGIC) return raw_block;

    // PFSC format: 0x30-byte header, then block offset table at 0x400,
    // then compressed data blocks.
    // The header contains: magic, block_count, uncompressed_size, etc.
    // For a full implementation, we'd parse the offset table and decompress
    // each block. For now, attempt zlib decompression of data after header.

    // Read the offset table (starts at PFSC_BLOCK_OFFSETS_OFFSET = 0x400)
    if (raw_block.size() < PFSC_BLOCK_OFFSETS_OFFSET + 8) return raw_block;

    // Simple approach: try decompressing from the data section
    // The first compressed block starts at PFSC_INITIAL_DATA_OFFSET (0x10000)
    // but in a single-block context, it may be right after the header.

    // Attempt zlib decompression of everything after the header
    std::vector<uint8_t> out(PFSC_LOGICAL_BLOCK_SIZE * 4); // generous output
    uLongf out_len = out.size();

    const int rc = uncompress(out.data(), &out_len,
                               raw_block.data() + PFSC_HEADER_SIZE,
                               raw_block.size() - PFSC_HEADER_SIZE);

    if (rc == Z_OK) {
        out.resize(out_len);
        return out;
    }

    // If simple decompression failed, try from the block offsets area
    out_len = out.size();
    const int rc2 = uncompress(out.data(), &out_len,
                                raw_block.data() + PFSC_BLOCK_OFFSETS_OFFSET,
                                raw_block.size() - PFSC_BLOCK_OFFSETS_OFFSET);
    if (rc2 == Z_OK) {
        out.resize(out_len);
        return out;
    }

    LOGF("PFS: PFSC decompression failed (zlib rc=%d, rc2=%d), returning raw", rc, rc2);
    return raw_block;
#else
    // No zlib — return raw data, log warning
    if (raw_block.size() >= 4) {
        const uint32_t magic = *reinterpret_cast<const uint32_t*>(raw_block.data());
        if (magic == PFSC_MAGIC) {
            LOGF("PFS: PFSC compressed block detected but zlib not linked — returning raw");
        }
    }
    return raw_block;
#endif
}

// ---- PFSC stream decompression (full header + offset table) ----

std::vector<uint8_t> PfsParser::DecompressPfscStream(const std::vector<uint8_t>& stream) {
    // PFSC stream layout (verified against MkPFS consts.py and pfsVolume.h):
    //   +0x00  'PFSC' magic
    //   +0x0C  u32 block_count
    //   +0x10  u64 logical_size (total uncompressed size of this stream)
    //   +0x400 offset table: (block_count + 1) u64 entries; entry[i] is the
    //          byte offset of compressed block i within the stream, and the
    //          +1 terminator gives the end of the last block.
    //   +0x10000 first compressed block.
    if (stream.size() < PFSC_HEADER_SIZE) {
        return {};
    }
    if (ReadU32LE(stream.data()) != PFSC_MAGIC) {
        return {};
    }

    const uint32_t block_count  = ReadU32LE(stream.data() + 0x0C);
    const uint64_t logical_size = ReadU64LE(stream.data() + 0x10);

    if (block_count == 0 || logical_size == 0) {
        return {};
    }
    // Sanity: block count must cover the logical size.
    const uint64_t max_blocks =
        (logical_size + PFSC_LOGICAL_BLOCK_SIZE - 1) / PFSC_LOGICAL_BLOCK_SIZE;
    if (block_count != max_blocks) {
        LOGF("PFS: PFSC block_count %u != expected %llu", block_count,
             static_cast<unsigned long long>(max_blocks));
        return {};
    }
    if (block_count > (1u << 26)) {
        return {};
    }

    // Offset table: (block_count + 1) u64 entries at 0x400.
    const size_t table_len = static_cast<size_t>(block_count + 1) * PFSC_OFFSET_ENTRY_SIZE;
    if (PFSC_BLOCK_OFFSETS_OFFSET + table_len > stream.size()) {
        return {};
    }

    std::vector<uint64_t> offsets(block_count + 1);
    for (uint32_t i = 0; i <= block_count; ++i) {
        offsets[i] = ReadU64LE(stream.data() + PFSC_BLOCK_OFFSETS_OFFSET
                               + static_cast<size_t>(i) * PFSC_OFFSET_ENTRY_SIZE);
    }

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(logical_size));

    for (uint32_t i = 0; i < block_count; ++i) {
        const uint64_t cur  = offsets[i];
        const uint64_t next = offsets[i + 1];
        if (next < cur || next > stream.size()) {
            LOGF("PFS: PFSC offset table entry %u out of range (0x%llx -> 0x%llx)", i,
                 static_cast<unsigned long long>(cur), static_cast<unsigned long long>(next));
            return {};
        }
        const size_t comp_len = static_cast<size_t>(next - cur);
        if (comp_len == 0) {
            return {};
        }

        // Logical size of this block: full 64KiB except the last block.
        const uint64_t base = static_cast<uint64_t>(i) * PFSC_LOGICAL_BLOCK_SIZE;
        const size_t   raw_len =
            (base + PFSC_LOGICAL_BLOCK_SIZE <= logical_size)
                ? PFSC_LOGICAL_BLOCK_SIZE
                : static_cast<size_t>(logical_size - base);

        // Stored-uncompressed fast path: comp_len == raw_len means the block
        // was not compressed (offset delta equals the raw length).
        if (comp_len == raw_len) {
            out.insert(out.end(), stream.begin() + static_cast<std::ptrdiff_t>(cur),
                       stream.begin() + static_cast<std::ptrdiff_t>(next));
            continue;
        }

        // Route through the decompression provider (zlib stock, Oodle if a
        // user-supplied core is loaded). algo 0 = zlib/deflate.
        auto result = KytyPS5::IO::DecompressionProvider::Instance().ProcessAsset(
            0, std::span<const uint8_t>(stream.data() + cur, comp_len), raw_len);
        if (!result || result->size() != raw_len) {
            LOGF("PFS: PFSC block %u decode failed", i);
            return {};
        }
        out.insert(out.end(), result->begin(), result->end());
    }

    if (out.size() != logical_size) {
        LOGF("PFS: PFSC decoded %zu bytes, expected %llu", out.size(),
             static_cast<unsigned long long>(logical_size));
        return {};
    }
    return out;
}

// ---- Inode reading (D32/S32/S64) ----

bool PfsParser::ReadInode(std::ifstream& f, uint32_t block_number,
                           uint32_t block_size, uint32_t version, uint32_t mode,
                           PfsInodeD32& out_d32, PfsInodeS32& out_s32,
                           PfsInodeS64& out_s64, int& out_variant) {

    if (block_number == 0) return false;

    const uint64_t offset = static_cast<uint64_t>(block_number) * block_size;
    f.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    const bool is_64bit = (mode & PFS_MODE_64BIT_INODES) != 0;

    if (is_64bit) {
        if (version == PFS_VERSION_PS5) {
            // S64 variant (0x310 bytes)
            out_variant = 3;
            f.read(reinterpret_cast<char*>(&out_s64), INODE_S64_SIZE);
            return static_cast<size_t>(f.gcount()) >= INODE_S64_SIZE;
        } else {
            // S32 variant (0x2C8 bytes)
            out_variant = 2;
            f.read(reinterpret_cast<char*>(&out_s32), INODE_S32_SIZE);
            return static_cast<size_t>(f.gcount()) >= INODE_S32_SIZE;
        }
    } else {
        // D32 variant (0xA8 bytes)
        out_variant = 1;
        f.read(reinterpret_cast<char*>(&out_d32), INODE_D32_SIZE);
        return static_cast<size_t>(f.gcount()) >= INODE_D32_SIZE;
    }
}

PfsParser::InodeInfo PfsParser::ExtractInodeInfo(const PfsInodeD32& d32,
                                                    const PfsInodeS32& s32,
                                                    const PfsInodeS64& s64,
                                                    int variant) {
    InodeInfo info {};
    info.size = 0;

    switch (variant) {
        case 1: // D32
            info.number = d32.number;
            info.mode = d32.mode;
            info.flags = d32.flags;
            info.size = d32.size;
            for (size_t i = 0; i < MAX_DIRECT_BLOCKS; ++i) info.db[i] = d32.db[i];
            for (size_t i = 0; i < MAX_INDIRECT_BLOCKS; ++i) info.ib[i] = d32.ib[i];
            break;
        case 2: // S32
            info.number = s32.number;
            info.mode = s32.mode;
            info.flags = s32.flags;
            info.size = s32.size;
            for (size_t i = 0; i < MAX_DIRECT_BLOCKS; ++i) info.db[i] = s32.db[i];
            for (size_t i = 0; i < MAX_INDIRECT_BLOCKS; ++i) info.ib[i] = s32.ib[i];
            break;
        case 3: // S64
            info.number = s64.number;
            info.mode = s64.mode;
            info.flags = s64.flags;
            info.size = s64.size;
            for (size_t i = 0; i < MAX_DIRECT_BLOCKS; ++i) info.db[i] = s64.db[i];
            for (size_t i = 0; i < MAX_INDIRECT_BLOCKS; ++i) info.ib[i] = s64.ib[i];
            break;
    }

    return info;
}

// ---- Indirect block traversal ----

std::vector<int64_t> PfsParser::GetIndirectBlocks(
    std::ifstream& f, const InodeInfo& inode,
    uint32_t block_size, uint32_t num_blocks, uint32_t mode,
    const PfsEkpfsKey* ekpfs_key) {

    std::vector<int64_t> all_blocks;

    // Direct blocks (0-11)
    for (size_t i = 0; i < MAX_DIRECT_BLOCKS; ++i) {
        if (inode.db[i] < 0) break;
        if (static_cast<uint64_t>(inode.db[i]) < num_blocks) {
            all_blocks.push_back(inode.db[i]);
        }
    }

    // Indirect blocks (ib[0-4])
    // Each indirect block contains block_size / sizeof(int32_t or int64_t) block pointers
    // The pointer width is an image-wide property set by the superblock mode flag
    // (PFS_MODE_64BIT_INODES), *not* a per-inode flag.
    const bool   is_64bit       = (mode & PFS_MODE_64BIT_INODES) != 0;
    const size_t ptr_size       = is_64bit ? 8 : 4;
    const size_t ptrs_per_block = block_size / ptr_size;

    for (size_t level = 0; level < MAX_INDIRECT_BLOCKS; ++level) {
        if (inode.ib[level] < 0) break;
        if (static_cast<uint32_t>(inode.ib[level]) >= num_blocks) break;

        auto block_data = ReadBlock(f, inode.ib[level], block_size, num_blocks, ekpfs_key);
        if (block_data.empty()) break;

        // Read pointers from the indirect block
        for (size_t j = 0; j < ptrs_per_block; ++j) {
            int64_t ptr;
            if (is_64bit) {
                if (j * 8 + 8 > block_data.size()) break;
                std::memcpy(&ptr, block_data.data() + j * 8, 8);
            } else {
                if (j * 4 + 4 > block_data.size()) break;
                int32_t ptr32;
                std::memcpy(&ptr32, block_data.data() + j * 4, 4);
                ptr = ptr32;
            }

            if (ptr < 0) break;
            if (static_cast<uint32_t>(ptr) >= num_blocks) break;
            all_blocks.push_back(ptr);
        }
    }

    return all_blocks;
}

// ---- Directory reading ----

std::vector<std::pair<std::string, uint32_t>> PfsParser::ReadDirectory(
    std::ifstream& f, const InodeInfo& dir_inode,
    uint32_t block_size, uint32_t num_blocks, uint32_t mode,
    const PfsEkpfsKey* ekpfs_key) {

    std::vector<std::pair<std::string, uint32_t>> entries;

    // Get all data blocks (direct + indirect) for this directory
    auto all_blocks = GetIndirectBlocks(f, dir_inode, block_size, num_blocks, mode, ekpfs_key);

    for (int64_t blk : all_blocks) {
        auto block_data = ReadBlock(f, blk, block_size, num_blocks, ekpfs_key);
        if (block_data.empty()) continue;

        // Parse dirent entries from the block
        size_t pos = 0;
        while (pos + sizeof(PfsDirent) <= block_data.size()) {
            PfsDirent dirent;
            std::memcpy(&dirent, block_data.data() + pos, sizeof(PfsDirent));

            if (dirent.type == 0 || dirent.type > DIRENT_TYPE_DOTDOT) {
                break; // end of entries
            }

            pos += sizeof(PfsDirent);
            if (pos + dirent.name_size > block_data.size()) break;

            std::string name;
            for (uint32_t j = 0; j < dirent.name_size && pos < block_data.size(); ++j) {
                if (block_data[pos] == '\0') { pos++; break; }
                name += static_cast<char>(block_data[pos++]);
            }

            // Entries are 8-byte aligned. The name parser has already consumed
            // the null terminator, so only the remaining inter-entry padding
            // (if any) is skipped here — never valid name bytes.
            while (pos < block_data.size() && (pos % 8) != 0) {
                ++pos;
            }

            if (dirent.type != DIRENT_TYPE_DOT && dirent.type != DIRENT_TYPE_DOTDOT) {
                entries.emplace_back(name, dirent.inode);
            }
        }
    }

    return entries;
}

// ---- File data reading (direct + indirect blocks) ----

std::vector<uint8_t> PfsParser::ReadFileData(
    std::ifstream& f, const InodeInfo& inode,
    uint32_t block_size, uint32_t num_blocks, uint32_t mode,
    const PfsEkpfsKey* ekpfs_key) {

    // Get all data blocks (direct + indirect)
    auto all_blocks = GetIndirectBlocks(f, inode, block_size, num_blocks, mode, ekpfs_key);

    // PFSC-compressed inode: the data blocks form a single PFSC stream
    // (header + offset table + compressed blocks). Decode the whole stream,
    // not per-block — the old per-block zlib hack ignored the offset table.
    if ((inode.flags & INODE_FLAG_COMPRESSED) != 0) {
        std::vector<uint8_t> stream;
        for (int64_t blk : all_blocks) {
            auto block_data = ReadBlock(f, blk, block_size, num_blocks, ekpfs_key);
            if (block_data.empty()) break;
            stream.insert(stream.end(), block_data.begin(), block_data.end());
        }
        if (stream.empty()) {
            return {};
        }
        auto decoded = DecompressPfscStream(stream);
        if (decoded.empty()) {
            LOGF("PFS: PFSC stream decode failed for inode %u", inode.number);
            return {};
        }
        if (decoded.size() > inode.size) {
            decoded.resize(static_cast<size_t>(inode.size));
        }
        return decoded;
    }

    std::vector<uint8_t> data;
    data.reserve(static_cast<size_t>(inode.size));

    uint64_t remaining = inode.size;
    for (int64_t blk : all_blocks) {
        if (remaining == 0) break;

        auto block_data = ReadBlock(f, blk, block_size, num_blocks, ekpfs_key);
        if (block_data.empty()) break;

        const size_t to_copy = std::min<size_t>(block_data.size(), remaining);
        data.insert(data.end(), block_data.begin(), block_data.begin() + to_copy);
        remaining -= to_copy;
    }

    return data;
}

// ---- Main parse ----

PfsParseResult PfsParser::Parse(const std::string& pfs_path) {
    PfsParseResult result;
    result.ok = false;
    result.is_encrypted = false;
    result.is_compressed = false;

    std::ifstream f(pfs_path, std::ios::binary);
    if (!f) {
        result.error = "Cannot open PFS file: " + pfs_path;
        return result;
    }

    // Read superblock
    PfsSuperblock sb{};
    f.read(reinterpret_cast<char*>(&sb), sizeof(sb));
    if (static_cast<size_t>(f.gcount()) < sizeof(sb)) {
        result.error = "PFS file too small for superblock";
        return result;
    }

    // Check for PFSC (compressed PFS)
    if (sb.format != PFS_FORMAT_MAGIC) {
        f.seekg(0, std::ios::beg);
        uint8_t magic4[4] = {0};
        f.read(reinterpret_cast<char*>(magic4), 4);
        if (magic4[0] == 0x50 && magic4[1] == 0x46 && magic4[2] == 0x53 && magic4[3] == 0x43) {
            result.is_compressed = true;
            LOGF("PFS: whole-image PFSC container detected");
            // A whole-image PFSC is a PFSC stream whose decompressed payload is
            // a PFS image. It is a distinct container from per-inode PFSC
            // streams (which are handled by DecompressPfscStream during
            // extraction). Decompressing the entire image to memory and
            // re-parsing the inner PFS is not supported here.
            result.error = "Whole-image PFSC container (decompress the image first, then parse the inner PFS)";
            result.ok = false;
            return result;
        }

        result.error = "Invalid PFS magic (expected 20130315 at format field, got " +
                        std::to_string(sb.format) + ")";
        return result;
    }

    result.version = static_cast<uint32_t>(sb.version);
    result.mode = sb.mode;
    result.block_size = sb.block_size;
    result.num_blocks = static_cast<uint32_t>(sb.num_blocks);
    result.num_inodes = static_cast<uint32_t>(sb.num_inodes);
    result.is_encrypted = (sb.mode & PFS_MODE_ENCRYPTED) != 0;

    LOGF("PFS: version=%u (%s), mode=0x%X, block_size=%u, num_blocks=%u, num_inodes=%u",
         result.version, result.version == PFS_VERSION_PS5 ? "PS5" : "PS4",
         result.mode, result.block_size, result.num_blocks, result.num_inodes);

    if (result.is_encrypted) {
        LOGF("PFS: image is encrypted — file data requires EKPFS keys (not provided by emulator)");
    }

    if (result.block_size == 0 || result.block_size > 0x100000) {
        result.error = "Invalid block size: " + std::to_string(result.block_size);
        return result;
    }

    // Find root inode (typically inode 2, at block 2 in the inode table)
    PfsInodeD32 d32{};
    PfsInodeS32 s32{};
    PfsInodeS64 s64{};
    int variant = 0;

    InodeInfo root_info{};
    bool found_root = false;

    for (uint32_t try_block = 1; try_block <= 4 && !found_root; ++try_block) {
        if (ReadInode(f, try_block, result.block_size, result.version, result.mode,
                      d32, s32, s64, variant)) {
            root_info = ExtractInodeInfo(d32, s32, s64, variant);
            if ((root_info.mode & INODE_MODE_DIR) != 0 && root_info.number >= 1) {
                found_root = true;
                LOGF("PFS: root directory inode found at block %u (inode %u, variant %d)",
                     try_block, root_info.number, variant);
            }
        }
    }

    if (!found_root) {
        LOGF("PFS: root directory inode not found, listing skipped");
        result.ok = true;
        return result;
    }

    // Recursively walk the directory tree starting from root
    std::function<void(const InodeInfo&, const std::string&)> walkDir =
        [&](const InodeInfo& dir_info, const std::string& path_prefix) {
        auto dir_entries = ReadDirectory(f, dir_info, result.block_size, result.num_blocks,
                                          result.mode, nullptr);
        if (path_prefix.empty()) {
            LOGF("PFS: root directory has %zu entries", dir_entries.size());
        }

        for (const auto& [name, inode_num] : dir_entries) {
            if (inode_num == 0 || inode_num > result.num_inodes) continue;
            if (name == "." || name == "..") continue;

            PfsInodeD32 ed32{};
            PfsInodeS32 es32{};
            PfsInodeS64 es64{};
            int evar = 0;

            if (ReadInode(f, inode_num, result.block_size, result.version, result.mode,
                          ed32, es32, es64, evar)) {
                auto info = ExtractInodeInfo(ed32, es32, es64, evar);

                PfsFile file;
                file.name = path_prefix.empty() ? name : (path_prefix + "/" + name);
                file.inode = info.number;
                file.size = info.size;
                file.is_compressed = (info.flags & INODE_FLAG_COMPRESSED) != 0;
                file.is_directory = (info.mode & INODE_MODE_DIR) != 0;
                file.block_number = (info.db[0] > 0) ? static_cast<uint32_t>(info.db[0]) : 0;
                result.files.push_back(file);

                LOGF("PFS:  %s%s (inode=%u, size=%llu, compressed=%s)",
                     file.name.c_str(),
                     file.is_directory ? "/" : "",
                     file.inode,
                     static_cast<unsigned long long>(file.size),
                     file.is_compressed ? "yes" : "no");

                // Recurse into subdirectories
                if (file.is_directory) {
                    walkDir(info, file.name);
                }
            }
        }
    };

    walkDir(root_info, "");

    result.ok = true;
    return result;
}

// ---- Extraction ----

uint32_t PfsParser::ExtractAll(const PfsParseResult& result,
                                 const std::string& pfs_path,
                                 const std::string& output_dir,
                                 const PfsEkpfsKey* ekpfs_key) {

    if (!result.ok) {
        LOGF("PFS: cannot extract - parse failed: %s", result.error.c_str());
        return 0;
    }

    if (result.is_encrypted && !ekpfs_key) {
        LOGF("PFS: cannot extract - image is encrypted and no EKPFS key provided");
        return 0;
    }

    if (result.is_compressed) {
#if !KYTY_PFS_HAS_ZLIB
        LOGF("PFS: cannot extract - PFSC compressed and zlib not linked");
        return 0;
#endif
    }

    if (result.files.empty()) {
        LOGF("PFS: no files to extract");
        return 0;
    }

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        LOGF("PFS: failed to create output directory: %s", output_dir.c_str());
        return 0;
    }

    std::ifstream f(pfs_path, std::ios::binary);
    if (!f) {
        LOGF("PFS: cannot open for extraction: %s", pfs_path.c_str());
        return 0;
    }

    uint32_t extracted = 0;

    for (const auto& file : result.files) {
        if (file.name.empty() || file.name == "." || file.name == "..") continue;
        if (!IsSafeRelativePath(file.name)) {
            LOGF("PFS: skipping unsafe path in package: %s", file.name.c_str());
            continue;
        }
        if (file.is_directory) continue; // skip directories, only extract files

        const std::filesystem::path out_path = std::filesystem::path(output_dir) / file.name;
        // Create parent directories for nested files (e.g. sce_sys/param.json)
        std::filesystem::create_directories(out_path.parent_path(), ec);
        std::ofstream out(out_path, std::ios::binary);
        if (!out) {
            LOGF("PFS: cannot create %s", out_path.string().c_str());
            continue;
        }

        // Re-read inode for this file
        PfsInodeD32 d32{};
        PfsInodeS32 s32{};
        PfsInodeS64 s64{};
        int variant = 0;

        if (ReadInode(f, file.inode, result.block_size, result.version, result.mode,
                      d32, s32, s64, variant)) {
            auto info = ExtractInodeInfo(d32, s32, s64, variant);

            auto data = ReadFileData(f, info, result.block_size,
                                     result.num_blocks, result.mode, ekpfs_key);

            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));
            extracted++;
            LOGF("PFS: extracted %s (%zu bytes)", file.name.c_str(), data.size());
        }

        out.close();
    }

    LOGF("PFS: extracted %u file(s) to %s", extracted, output_dir.c_str());
    return extracted;
}

} // namespace Libs::Firmware