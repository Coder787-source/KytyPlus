#ifndef KYTY_FIRMWARE_PUP_PARSER_H_
#define KYTY_FIRMWARE_PUP_PARSER_H_

#include "firmware/pupKeyStore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Libs::Firmware {

// PS5 PUP (PlayStation Update Package) file format parser.
//
// Parses Sony firmware update files (.pup and .pup.dec) using the SLB2 container format.
// The SLB2 container header and entry table are UNENCRYPTED and can be parsed without keys.
// Individual file contents may be encrypted (SELF format) or unencrypted (plain ELF).
//
// Format documentation based on public reverse engineering from psdevwiki and ps5-tools.
// Legal notice: This parser does NOT include, hardcode, or link to any decryption keys.
// It only parses the container structure which is publicly documented and unencrypted.

#pragma pack(push, 1)

// SLB2 container header (28 bytes)
// Magic "SLB2" followed by version, flags, entry count, and block count.
// All fields are little-endian. This header is UNENCRYPTED.
struct SLB2Header {
    char     magic[4];     // "SLB2" (0x53 0x4C 0x42 0x32)
    uint32_t version;      // Container version (typically 1)
    uint32_t flags;        // Container flags
    uint32_t entries;      // Number of entries in the entry table
    uint32_t blocks;       // Total number of 0x200-byte blocks
    uint32_t reserved[3];  // Padding/reserved (must be zero)
};

static constexpr uint32_t SLB2_BLOCK_SIZE = 0x200; // 512 bytes per block

// SLB2 entry descriptor (48 bytes)
// Describes one file within the container: start block, size, and filename.
// The entry table is UNENCRYPTED — all filenames and offsets are readable.
struct SLB2Entry {
    uint32_t start;        // Start block number (offset = start * 0x200)
    uint32_t size;         // File size in bytes
    uint8_t  reserved[8];  // Padding/reserved
    char     name[32];     // Filename (null-terminated ASCII)
};

// ELF header for validation (64-bit)
struct Elf64Header {
    uint8_t  magic[4];     // 0x7F 'E' 'L' 'F'
    uint8_t  elf_class;    // 1=32-bit, 2=64-bit
    uint8_t  data;         // 1=LE, 2=BE
    uint8_t  version;      // ELF version
    uint8_t  os_abi;       // OS/ABI (0x00=System V, 0x26=FreeBSD, 0xFF=SCE)
    uint8_t  abi_version;  // ABI version
    uint8_t  pad[7];       // Padding
    uint16_t type;         // Object file type
    uint16_t machine;      // Architecture (0x3E=x86_64)
    uint32_t elf_version;  // ELF version
    uint64_t entry;        // Entry point address
    uint64_t ph_offset;    // Program header table offset
    uint64_t sh_offset;    // Section header table offset
    uint32_t flags;        // Processor-specific flags
    uint16_t eh_size;      // ELF header size
    uint16_t ph_entry_size;// Program header entry size
    uint16_t ph_num;       // Number of program headers
    uint16_t sh_entry_size;// Section header entry size
    uint16_t sh_num;       // Number of section headers
    uint16_t sh_strndx;    // Section name string table index
};

// SCE-specific ELF types used by PS5
static constexpr uint16_t ET_SCE_EXEC      = 0xFE00; // SCE Executable (PRX)
static constexpr uint16_t ET_SCE_RELEXEC   = 0xFE04; // SCE Relocatable Executable
static constexpr uint16_t ET_SCE_STUBLIB   = 0xFE0C; // SCE SDK Stubs
static constexpr uint16_t ET_SCE_DYNEXEC   = 0xFE10; // SCE EXEC with ASLR
static constexpr uint16_t ET_SCE_DYNAMIC   = 0xFE18; // SCE Dynamic Library

#pragma pack(pop)

// Extracted firmware module from PUP container
struct FirmwareModule {
    std::string name;            // Module filename (from SLB2 entry)
    std::string path;            // Extracted file path (set during extraction)
    uint64_t    offset;          // Offset within PUP file
    uint64_t    size;            // Size in bytes
    uint64_t    file_id;         // PUP file entry ID (used for key store lookup)
    std::vector<uint8_t> data;   // Raw module bytes
    bool is_encrypted;           // True if content lacks ELF magic (likely encrypted)
    bool is_prx;                 // True if valid PS5 PRX/SPRX module
    bool is_valid;               // True if accepted as ELF/PRX (may be false for non-ELF entries)
};

// PUP parser result
struct PupParseResult {
    bool ok;                              // True if parsing succeeded
    std::string error;                    // Error message if failed
    std::string firmware_version;         // Detected firmware version (if found)
    uint32_t total_entries;               // Total entries in container
    uint32_t total_blocks;                // Total blocks in container
    std::vector<FirmwareModule> modules;  // Extracted modules

    // Encryption / key tracking (populated by ParseWithKeys)
    bool     had_encrypted_entries      = false; // >=1 entry was encrypted in the PUP
    bool     keys_required_and_missing  = false; // encrypted entry had no key in the store
    uint32_t decrypted_count            = 0;     // entries successfully decrypted
    uint32_t skipped_encrypted          = 0;     // encrypted entries skipped (no key)
};

class PupParser {
public:
    // Parse a .pup or .pup.dec file.
    // Parses the SLB2 container structure (unencrypted) and extracts all entries.
    // Encrypted entries are extracted as-is and marked as encrypted.
    // Unencrypted entries are validated as ELF/PRX modules.
    static PupParseResult Parse(const std::string& pup_path);

    // Parse an official encrypted .pup using a user-supplied key store.
    //
    // For each entry in the PUP:
    //   - If the data has ELF magic (already plaintext): accepted as-is.
    //   - If the data is encrypted AND a key is present in key_store:
    //       decrypts via AES-128-CTR, then validates as ELF/PRX.
    //   - If the data is encrypted AND no key is available:
    //       sets keys_required_and_missing = true and returns ok = false.
    //
    // If key_store.IsEmpty() and the PUP has any encrypted entries the call
    // returns immediately with ok = false and a descriptive error.
    static PupParseResult ParseWithKeys(const std::string& pup_path,
                                        const PupKeyStore&  key_store);

    // Extract all modules to the specified directory.
    // Creates the directory if it doesn't exist.
    // Returns the number of files successfully extracted.
    static uint32_t ExtractAll(const PupParseResult& result, const std::string& output_dir);

    // Check if data starts with valid ELF magic
    static bool HasElfMagic(const std::vector<uint8_t>& data);

    // Check if data is a valid PS5 PRX module (ELF with SCE type)
    static bool IsPrxModule(const std::vector<uint8_t>& data);

private:
    // Validate SLB2 header magic and basic sanity
    static bool ValidateSLB2Header(const SLB2Header& hdr, uint64_t file_size);

    // Check if data appears to be encrypted (no ELF magic) vs unencrypted
    static bool IsEncrypted(const std::vector<uint8_t>& data);

    // Read null-terminated string from fixed-size char array
    static std::string ReadFixedString(const char* buffer, size_t max_len);
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_PUP_PARSER_H_ */
