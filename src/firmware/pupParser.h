#ifndef KYTY_FIRMWARE_PUP_PARSER_H_
#define KYTY_FIRMWARE_PUP_PARSER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace Libs::Firmware {

// PS5 PUP (PlayStation Update Package) file format parser.
//
// Parses official Sony firmware update files (.pup) distributed at:
// https://www.playstation.com/en-us/support/hardware/ps5/system-software/
//
// Legal notice: This parser does NOT distribute, include, or link to any
// Sony copyrighted material. Users must obtain the .pup file directly from
// Sony's official website. The parser only provides extraction capability
// for interoperability purposes, consistent with the approach used by
// RPCS3, Dolphin, Cemu, and other emulators.

#pragma pack(push, 1)

// PUP file header (32 bytes)
struct PupHeader {
    uint32_t magic;              // 0x70757000 ("pup")
    uint32_t version;            // Format version
    uint64_t file_size;          // Total file size in bytes
    uint32_t num_files;          // Number of contained files
    uint32_t flags;              // Update flags
    uint64_t header_hash_offset; // Offset to header hash
    uint64_t data_hash_offset;   // Offset to data hash
    uint8_t  reserved[8];        // Padding
};

// Individual file entry within the PUP
struct PupFileEntry {
    uint64_t file_id;            // Unique identifier for this file
    uint64_t offset;             // Offset within .pup file
    uint64_t size;               // Size in bytes
    uint64_t flags;              // Entry flags
    uint8_t  hash[64];           // SHA-256 hash of file content
    uint8_t  reserved[32];       // Padding
};

// PRX/SPRX module header (simplified ELF-based format)
struct PrxHeader {
    uint8_t  magic[4];           // 0x7F "ELF"
    uint8_t  class_type;         // 1=32-bit, 2=64-bit
    uint8_t  data_encoding;      // 1=LE, 2=BE
    uint8_t  version;            // ELF version
    uint8_t  os_abi;             // OS/ABI identification
    uint8_t  abi_version;        // ABI version
    uint8_t  reserved[7];        // Padding
    uint16_t type;               // Object file type (ET_SCE_DYNEXEC=0xFE00)
    uint16_t machine;            // Architecture (EM_X86_64=0x3E)
    uint32_t elf_version;        // ELF version
    uint64_t entry;              // Entry point address
    uint64_t ph_offset;          // Program header offset
    uint64_t sh_offset;          // Section header offset
    uint32_t flags;              // Processor flags
    uint16_t eh_size;            // ELF header size
    uint16_t ph_entry_size;      // Program header entry size
    uint16_t ph_num;             // Number of program headers
    uint16_t sh_entry_size;      // Section header entry size
    uint16_t sh_num;             // Number of section headers
    uint16_t sh_strndx;          // Section name string table index
};

#pragma pack(pop)

// Extracted firmware module
struct FirmwareModule {
    std::string name;            // Module name (e.g., "libkernel.prx")
    std::string path;            // Extracted file path
    uint64_t file_id;            // Original file_id from PUP
    std::vector<uint8_t> data;   // Raw module bytes
    bool is_prx;                 // True if this is a PRX/SPRX module
    bool is_valid;               // True if header validation passed
};

// PUP parser result
struct PupParseResult {
    bool ok;                     // True if parsing succeeded
    std::string error;           // Error message if failed
    PupHeader header;            // PUP file header
    std::vector<PupFileEntry> entries; // All file entries
    std::vector<FirmwareModule> modules; // Extracted PRX modules
};

class PupParser {
public:
    // Parse a .pup file and extract all modules
    static PupParseResult Parse(const std::string& pup_path);

    // Get human-readable name for a file_id
    static std::string GetFileName(uint64_t file_id);

    // Check if a file_id corresponds to a PRX/SPRX module
    static bool IsPrxModule(uint64_t file_id);

private:
    // Validate PUP header magic and size
    static bool ValidateHeader(const PupHeader& hdr, uint64_t file_size);

    // Parse ELF header to determine if it's a PRX module
    static bool IsPrxData(const std::vector<uint8_t>& data);

    // Extract module name from file path or file_id
    static std::string ExtractModuleName(const std::string& path, uint64_t file_id);
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_PUP_PARSER_H_ */
