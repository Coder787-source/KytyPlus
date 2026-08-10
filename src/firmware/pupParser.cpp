#include "firmware/pupParser.h"
#include "firmware/pupKeyStore.h"
#include "common/logging/log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Libs::Firmware {

// Known PS5 firmware file IDs (partial list from reverse engineering)
// NOTE: These are educated guesses based on PS4 patterns. Real PS5 IDs may differ.
struct KnownFileId {
    uint64_t id;
    const char* name;
    bool is_prx;
};

static constexpr KnownFileId kKnownFiles[] = {
    // System modules
    {0x100, "ps5firmware.pkg", false},
    {0x200, "ps5recovery.dat", false},
    {0x300, "version.txt", false},

    // Kernel and core libraries
    {0x1000, "kernel.prx", true},
    {0x1001, "libkernel.prx", true},
    {0x1002, "libkernel_sys.prx", true},

    // System libraries
    {0x2000, "libSceBase.prx", true},
    {0x2001, "libSceLibcInternal.prx", true},
    {0x2002, "libSceFile.prx", true},
    {0x2003, "libSceNet.prx", true},
    {0x2004, "libSceNp.prx", true},
    {0x2005, "libScePad.prx", true},
    {0x2006, "libSceAudio.prx", true},
    {0x2007, "libSceVideoOut.prx", true},
    {0x2008, "libSceGnmDriver.prx", true},
    {0x2009, "libSceSaveData.prx", true},
    {0x200A, "libSceSystemService.prx", true},
    {0x200B, "libSceUserService.prx", true},
    {0x200C, "libSceCommonDialog.prx", true},

    // Graphics and compute
    {0x3000, "libSceGpu.prx", true},
    {0x3001, "libSceGpuAsync.prx", true},
    {0x3002, "libSceGnmCompute.prx", true},

    // Audio
    {0x4000, "libSceAudio2.prx", true},
    {0x4001, "libSceAudioOut.prx", true},
    {0x4002, "libSceAudioIn.prx", true},
    {0x4003, "libSceAudio3d.prx", true},

    // Network
    {0x5000, "libSceHttp.prx", true},
    {0x5001, "libSceSsl.prx", true},
    {0x5002, "libSceNetCtl.prx", true},

    // Input
    {0x6000, "libScePadTrigger.prx", true},
    {0x6001, "libSceTouch.prx", true},
    {0x6002, "libSceMotion.prx", true},

    // Storage
    {0x7000, "libSceSaveData.prx", true},
    {0x7001, "libSceAppInst.prx", true},
};

PupParseResult PupParser::Parse(const std::string& pup_path) {
    PupParseResult result {};

    // Open file
    std::ifstream file(pup_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.ok = false;
        result.error = "Failed to open PUP file: " + pup_path;
        return result;
    }

    const auto file_size = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    LOGF("[Firmware] INFO: Parsing PUP file (%.2f MB) \
",
         static_cast<double>(file_size) / (1024.0 * 1024.0));

    // Read SLB2 header (PS5 format)
    SLB2Header slb2_hdr {};
    file.read(reinterpret_cast<char*>(&slb2_hdr), sizeof(slb2_hdr));
    if (!file.good()) {
        result.ok = false;
        result.error = "Failed to read SLB2 header";
        return result;
    }

    // Debug: print raw header
    LOGF("[Firmware] DEBUG: SLB2 magic=0x%02X%02X%02X%02X \
",
         slb2_hdr.magic[0], slb2_hdr.magic[1], slb2_hdr.magic[2], slb2_hdr.magic[3]);
    LOGF("[Firmware] DEBUG: SLB2 version=%u \
", slb2_hdr.version);
    LOGF("[Firmware] DEBUG: SLB2 entries=%u \
", slb2_hdr.entries);
    LOGF("[Firmware] DEBUG: SLB2 blocks=%u \
", slb2_hdr.blocks);

    if (!ValidateSLB2Header(slb2_hdr, file_size)) {
        result.ok = false;
        result.error = "Invalid SLB2 header (magic not 'SLB2')";
        return result;
    }

    result.total_entries = slb2_hdr.entries;
    result.total_blocks = slb2_hdr.blocks;

    LOGF("[Firmware] INFO: SLB2 version=%u, entries=%u, blocks=%u \
",
         slb2_hdr.version, slb2_hdr.entries, slb2_hdr.blocks);

    // Read SLB2 entry table
    std::vector<SLB2Entry> slb2_entries(slb2_hdr.entries);
    file.read(reinterpret_cast<char*>(slb2_entries.data()),
              static_cast<std::streamsize>(slb2_hdr.entries * sizeof(SLB2Entry)));
    if (!file.good()) {
        result.ok = false;
        result.error = "Failed to read SLB2 entry table";
        return result;
    }

    uint32_t prx_count = 0;
    uint32_t elf_count = 0;

    for (uint32_t i = 0; i < slb2_hdr.entries; i++) {
        const SLB2Entry& slb2_entry = slb2_entries[i];

        // Debug: print first 20 entries
        if (i < 20) {
            LOGF("[Firmware] DEBUG: SLB2 Entry %u: start=%u, size=%u, name='%s' \
",
                 i, slb2_entry.start, slb2_entry.size, slb2_entry.name);
        }

        // Calculate file offset (start block * 512)
        const uint64_t file_offset = static_cast<uint64_t>(slb2_entry.start) * SLB2_BLOCK_SIZE;
        if (file_offset + slb2_entry.size > file_size) {
            LOGF("[Firmware] WARN: SLB2 entry %u has invalid offset/size (skipped) \
", i);
            continue;
        }

        // Read file data
        file.seekg(file_offset, std::ios::beg);
        std::vector<uint8_t> data(static_cast<size_t>(slb2_entry.size));
        file.read(reinterpret_cast<char*>(data.data()), data.size());
        if (!file.good()) {
            LOGF("[Firmware] WARN: Failed to read data for SLB2 entry %u (skipped) \
", i);
            continue;
        }

        // Check ELF magic
        const bool has_elf_magic = HasElfMagic(data);
        const bool is_prx = IsPrxModule(data);

        if (has_elf_magic && !is_prx) {
            LOGF("[Firmware] WARN: Entry '%s' has ELF magic but failed PRX validation \
",
                 slb2_entry.name);
        }

        // Map SLB2 entry to FirmwareModule
        FirmwareModule module {};
        module.file_id = 0;
        for (char c : slb2_entry.name) {
            module.file_id = (module.file_id * 31) + static_cast<uint8_t>(c);
        }
        module.data = std::move(data);
        module.is_encrypted = !has_elf_magic;
        module.is_prx = is_prx;
        module.is_valid = is_prx || has_elf_magic;
        module.name = ReadFixedString(slb2_entry.name, sizeof(slb2_entry.name));
        module.path = "";
        module.offset = file_offset;
        module.size = slb2_entry.size;

        if (is_prx) {
            prx_count++;
            LOGF("[Firmware] INFO: Found PRX module: %s (file_id=0x%08llX, %llu bytes) \
",
                 module.name.c_str(),
                 static_cast<unsigned long long>(module.file_id),
                 static_cast<unsigned long long>(module.data.size()));
        } else if (has_elf_magic) {
            elf_count++;
            LOGF("[Firmware] INFO: Found ELF file (non-PRX): %s (file_id=0x%08llX, %llu bytes) \
",
                 module.name.c_str(),
                 static_cast<unsigned long long>(module.file_id),
                 static_cast<unsigned long long>(module.data.size()));
        }

        result.modules.push_back(std::move(module));
    }

    // Summary log (uses prx_count/elf_count to avoid unused-variable warnings)
    LOGF("[Firmware] INFO: Parse complete: %u PRX modules, %u ELF files, %u total entries \
",
         prx_count, elf_count, result.total_entries);

    result.ok = true;
    return result;
}


bool PupParser::HasElfMagic(const std::vector<uint8_t>& data) {
    // ELF magic: 0x7F 'E' 'L' 'F'
    if (data.size() < 4) {
        return false;
    }
    return (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F');
}

bool PupParser::IsPrxModule(const std::vector<uint8_t>& data) {
    // Check minimum size for ELF header
    if (data.size() < sizeof(Elf64Header)) {
        return false;
    }

    // Check ELF magic
    if (!HasElfMagic(data)) {
        return false;
    }

    // Parse the ELF header
    const auto* elf_hdr = reinterpret_cast<const Elf64Header*>(data.data());

    // PS5 PRX modules use SCE-specific types
    const bool is_sce_type = (elf_hdr->type == ET_SCE_EXEC ||
                             elf_hdr->type == ET_SCE_RELEXEC ||
                             elf_hdr->type == ET_SCE_DYNEXEC ||
                             elf_hdr->type == ET_SCE_DYNAMIC ||
                             elf_hdr->type == ET_SCE_STUBLIB);

    // Must be x86_64
    const bool is_x86_64 = (elf_hdr->machine == 0x3E);

    return is_sce_type && is_x86_64;
}

bool PupParser::ValidateSLB2Header(const SLB2Header& hdr, uint64_t file_size) {
    // Check magic number ("SLB2")
    if (hdr.magic[0] != 'S' || hdr.magic[1] != 'L' || hdr.magic[2] != 'B' || hdr.magic[3] != '2') {
        return false;
    }

    // Sanity checks
    if (hdr.version != 1) {
        LOGF("[Firmware] WARN: SLB2 version %u is unsupported (expected 1) \
", hdr.version);
    }

    if (hdr.entries == 0 || hdr.entries > 10000) {
        return false;
    }

    // Check total size (blocks * 512)
    const uint64_t total_size = static_cast<uint64_t>(hdr.blocks) * SLB2_BLOCK_SIZE;
    if (total_size > file_size) {
        LOGF("[Firmware] WARN: SLB2 total size (%llu) > file size (%llu) \
",
             static_cast<unsigned long long>(total_size),
             static_cast<unsigned long long>(file_size));
    }

    return true;
}

bool PupParser::IsEncrypted(const std::vector<uint8_t>& data) {
    // Encrypted data lacks ELF magic
    return !HasElfMagic(data);
}

std::string PupParser::ReadFixedString(const char* buffer, size_t max_len) {
    // Read null-terminated string from fixed-size char array
    std::string s;
    for (size_t i = 0; i < max_len; ++i) {
        if (buffer[i] == ' ') {
            break;
        }
        s += buffer[i];
    }
    return s;
}// ---------------------------------------------------------------------------
// AES-128-CTR decryption
// ---------------------------------------------------------------------------
//
// Standard AES-128-CTR (no Sony-specific logic; pure mathematics on the
// user-supplied key + IV).  The counter block is the IV incremented as a
// 128-bit big-endian integer, one block (16 bytes) at a time.
//
// This is self-contained to avoid pulling in OpenSSL/mbedTLS just for one
// cipher mode; real PS5 SELF entries use AES-128-CBC or AES-128-CTR
// depending on the SELF segment flags.  The user's keys.bin must match.
// ---------------------------------------------------------------------------

namespace {

// AES substitution box (S-box)
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

// AES round constant
static constexpr uint8_t kAesRcon[11] = {
    0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

using AesBlock = std::array<uint8_t, 16>;
// AES-128 key schedule: 11 round keys of 16 bytes = 176 bytes
using AesRoundKeys = std::array<uint8_t, 176>;

static uint8_t GfMul2(uint8_t b) {
    return static_cast<uint8_t>((b << 1) ^ ((b & 0x80) ? 0x1b : 0x00));
}

static AesRoundKeys AesKeyExpand(const std::array<uint8_t, 16>& key) {
    AesRoundKeys rk {};
    std::memcpy(rk.data(), key.data(), 16);

    for (int i = 4; i < 44; ++i) {
        uint8_t tmp[4];
        std::memcpy(tmp, rk.data() + (i - 1) * 4, 4);

        if (i % 4 == 0) {
            // RotWord + SubWord + Rcon
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
    for (int i = 0; i < 16; ++i) {
        state[i] = kAesSbox[state[i]];
    }
}

static void AesShiftRows(uint8_t state[16]) {
    // Row 1: left-rotate by 1
    uint8_t t = state[1]; state[1]=state[5]; state[5]=state[9]; state[9]=state[13]; state[13]=t;
    // Row 2: left-rotate by 2
    std::swap(state[2], state[10]); std::swap(state[6], state[14]);
    // Row 3: left-rotate by 3
    t = state[15]; state[15]=state[11]; state[11]=state[7]; state[7]=state[3]; state[3]=t;
}

static void AesMixColumns(uint8_t state[16]) {
    for (int c = 0; c < 4; ++c) {
        uint8_t* col = state + c * 4;
        uint8_t s0 = col[0], s1 = col[1], s2 = col[2], s3 = col[3];
        col[0] = GfMul2(s0) ^ (GfMul2(s1) ^ s1) ^ s2 ^ s3;
        col[1] = s0 ^ GfMul2(s1) ^ (GfMul2(s2) ^ s2) ^ s3;
        col[2] = s0 ^ s1 ^ GfMul2(s2) ^ (GfMul2(s3) ^ s3);
        col[3] = (GfMul2(s0) ^ s0) ^ s1 ^ s2 ^ GfMul2(s3);
    }
}

static void AesAddRoundKey(uint8_t state[16], const uint8_t* rk) {
    for (int i = 0; i < 16; ++i) {
        state[i] ^= rk[i];
    }
}

// Encrypt one 16-byte block in-place (ECB, for CTR keystream generation)
static AesBlock AesEncryptBlock(const AesBlock& in, const AesRoundKeys& rk) {
    uint8_t state[16];
    std::memcpy(state, in.data(), 16);

    AesAddRoundKey(state, rk.data()); // Initial round key

    for (int round = 1; round < 10; ++round) {
        AesSubBytes(state);
        AesShiftRows(state);
        AesMixColumns(state);
        AesAddRoundKey(state, rk.data() + round * 16);
    }
    // Final round (no MixColumns)
    AesSubBytes(state);
    AesShiftRows(state);
    AesAddRoundKey(state, rk.data() + 10 * 16);

    AesBlock out;
    std::memcpy(out.data(), state, 16);
    return out;
}

// Increment a 16-byte counter block as a big-endian 128-bit integer
static void AesCtrIncrement(AesBlock& ctr) {
    for (int i = 15; i >= 0; --i) {
        if (++ctr[i] != 0) {
            break;
        }
    }
}

// AES-128-CTR: encrypt or decrypt (same operation — XOR with keystream)
static std::vector<uint8_t> Aes128CtrCrypt(
    const std::vector<uint8_t>&     data,
    const std::array<uint8_t, 16>&  key,
    const std::array<uint8_t, 16>&  iv)
{
    const AesRoundKeys rk = AesKeyExpand(key);
    AesBlock counter;
    std::memcpy(counter.data(), iv.data(), 16);

    std::vector<uint8_t> out(data.size());
    size_t offset = 0;

    while (offset < data.size()) {
        // Generate one keystream block
        const AesBlock keystream = AesEncryptBlock(counter, rk);
        AesCtrIncrement(counter);

        // XOR up to 16 bytes
        const size_t chunk = std::min<size_t>(16, data.size() - offset);
        for (size_t i = 0; i < chunk; ++i) {
            out[offset + i] = data[offset + i] ^ keystream[i];
        }
        offset += chunk;
    }
    return out;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// PupParser::ParseWithKeys
// ---------------------------------------------------------------------------

PupParseResult PupParser::ParseWithKeys(const std::string& pup_path,
                                         const PupKeyStore&  key_store) {
    // First do a normal parse (reads SLB2 container — always unencrypted)
    PupParseResult result = Parse(pup_path);
    if (!result.ok) {
        return result; // Propagate parse-level errors (bad magic, truncated file, etc.)
    }

    // Walk every module and handle encrypted ones
    bool any_encrypted = false;
    bool any_missing_key = false;

    for (auto& module : result.modules) {
        if (!module.is_encrypted) {
            // Already plaintext — nothing to do
            continue;
        }

        any_encrypted = true;
        result.had_encrypted_entries = true;

        // Determine the entry_id from the module's file_id field
        // (FirmwareModule carries file_id as set in Parse())
        const uint64_t entry_id = module.file_id;

        if (key_store.IsEmpty()) {
            // No keys at all — refuse immediately
            result.ok = false;
            result.keys_required_and_missing = true;
            result.error =
                "Official encrypted .pup detected but no keys.bin was found alongside it.\n"
                "Place your own keys.bin (dumped from your PS5 console) in the same\n"
                "directory as the .pup file. The emulator never provides keys.";
            LOGF("[Firmware] ERROR: %s\n", result.error.c_str());
            return result;
        }

        if (!key_store.HasKey(entry_id)) {
            // keys.bin exists but is missing this specific entry
            any_missing_key = true;
            result.keys_required_and_missing = true;
            result.skipped_encrypted++;
            LOGF("[Firmware] ERROR: Encrypted entry 0x%016llX has no key in keys.bin — "
                 "cannot decrypt.\n",
                 static_cast<unsigned long long>(entry_id));
            continue;
        }

        // We have a key — decrypt via AES-128-CTR
        const auto seg_key = key_store.GetKey(entry_id).value();

        LOGF("[Firmware] INFO: Decrypting entry 0x%016llX (%zu bytes) with user-supplied key\n",
             static_cast<unsigned long long>(entry_id), module.data.size());

        module.data = Aes128CtrCrypt(module.data, seg_key.key, seg_key.iv);

        // Re-validate after decryption
        const bool has_elf = HasElfMagic(module.data);
        module.is_encrypted = false; // now plaintext (or decryption failed)
        module.is_prx = IsPrxModule(module.data);

        if (!has_elf) {
            LOGF("[Firmware] WARN: Entry 0x%016llX still lacks ELF magic after decryption — "
                 "key may be wrong.\n",
                 static_cast<unsigned long long>(entry_id));
        } else {
            result.decrypted_count++;
            LOGF("[Firmware] INFO: Entry 0x%016llX decrypted OK (PRX=%s)\n",
                 static_cast<unsigned long long>(entry_id),
                 module.is_prx ? "yes" : "no");
        }
    }

    // If we found encrypted entries but any were missing a key, fail.
    if (any_missing_key) {
        result.ok = false;
        if (result.error.empty()) {
            result.error =
                "keys.bin was found but is missing keys for one or more encrypted entries.\n"
                "Ensure your keys.bin contains all required entry keys.";
        }
        LOGF("[Firmware] ERROR: %u encrypted entries could not be decrypted (missing keys).\n",
             result.skipped_encrypted);
        return result;
    }

    if (any_encrypted) {
        LOGF("[Firmware] INFO: Decrypted %u encrypted entries using user-supplied keys.\n",
             result.decrypted_count);
    } else {
        LOGF("[Firmware] INFO: No encrypted entries found — PUP was already plaintext.\n");
    }

    return result;
}

} // namespace Libs::Firmware
