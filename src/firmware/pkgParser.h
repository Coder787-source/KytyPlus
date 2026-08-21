#ifndef KYTY_FIRMWARE_PKG_PARSER_H_
#define KYTY_FIRMWARE_PKG_PARSER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace Libs::Firmware {

// PS4/PS5 PKG (Package) file format parser.
//
// Parses Sony digital game/update packages (.pkg) using the publicly documented
// PKG container format. The PKG header is UNENCRYPTED and can be parsed without keys.
// The body may contain encrypted PFS data (retail) or plaintext PFS (debug/decrypted).
//
// Format documentation based on public reverse engineering from psdevwiki (SpecterDev).
// Legal notice: This parser does NOT include, hardcode, or link to any decryption keys.
// It only parses the container structure which is publicly documented and unencrypted.

#pragma pack(push, 1)

// PKG container header (big-endian, based on PS3/PS4 package format)
// The magic is 0x7F504B47 = "\x7FPKG"
struct PkgHeader {
    uint32_t magic;             // 0x000: 0x7F504B47 ("\x7FPKG"), big-endian
    uint16_t revision;          // 0x004: package revision
    uint16_t type;              // 0x006: package type
    uint32_t unknown0;          // 0x008: unknown field
    uint32_t file_count;        // 0x00C: number of files
    uint32_t table_entries;     // 0x010: table entry count
    uint16_t sys_ents;          // 0x014: system entries
    uint16_t unknown1;          // 0x016: unknown
    uint32_t table_offset;      // 0x018: file table offset
    uint32_t entry_data_size;   // 0x01C: entry data size
    uint32_t unknown2;          // 0x020: unknown
    uint32_t body_offset;       // 0x024: body offset (typically 0x200)
    uint32_t body_unknown;      // 0x028: unknown
    uint32_t body_size;         // 0x02C: body size in bytes
    uint8_t  padding0[0x10];    // 0x030: 16 bytes padding
    char     content_id[0x24]; // 0x040: content ID (36-byte string, e.g. "EP0001-CUSA01234_00-...")
    uint8_t  padding1[0x10];    // 0x064: 16 bytes padding
    uint8_t  unknown_data[0x8C]; // 0x074: unknown data
    // Digest table follows at 0x100...
};

// PFS (PlayStation File System) magic for detecting decrypted body content
static constexpr uint32_t PKG_MAGIC       = 0x7F504B47; // "\x7FPKG" (big-endian)
static constexpr uint32_t PKG_PFS_BODY_MAGIC = 0x00534650; // "PFS\0" (little-endian read)
static constexpr uint32_t PKG_BODY_OFFSET  = 0x200;       // Standard body offset

#pragma pack(pop)

// Extracted file entry from PKG
struct PkgFileEntry {
    std::string name;            // File name (from file table)
    uint64_t    offset;          // Offset within PKG body
    uint64_t    size;            // File size in bytes
};

// PKG parser result
struct PkgParseResult {
    bool ok;                              // True if parsing succeeded
    std::string error;                    // Error message if failed
    std::string content_id;               // Content ID from header
    uint32_t file_count;                  // File count from header
    uint32_t body_offset;                 // Body offset
    uint32_t body_size;                   // Body size
    std::vector<PkgFileEntry> files;      // Extracted file entries

    // Encryption status
    bool is_encrypted;                    // True if body appears encrypted (no PFS magic)
    bool keys_required_and_missing;       // True if encrypted but no keys available
};

class PkgParser {
public:
    // Parse a .pkg file.
    // Parses the container header (unencrypted) and reads the file table.
    // Detects whether the body is encrypted (no PFS magic) or decrypted.
    // Does NOT extract file contents — use ExtractAll() for that.
    static PkgParseResult Parse(const std::string& pkg_path);

    // Extract all files from a pre-decrypted (unencrypted) PKG to the output directory.
    // Creates the directory if it doesn't exist.
    // Returns the number of files successfully extracted.
    // If the PKG is encrypted, returns 0 and sets result.keys_required_and_missing.
    static uint32_t ExtractAll(const PkgParseResult& result,
                                const std::string& pkg_path,
                                const std::string& output_dir);

    // Check if data starts with PFS magic (decrypted body indicator)
    static bool HasPfsMagic(const std::vector<uint8_t>& data);

    // Check if the PKG body is encrypted (no PFS magic at body_offset)
    static bool IsEncrypted(const std::string& pkg_path, uint32_t body_offset);

private:
    // Validate PKG header magic and basic sanity
    static bool ValidatePkgHeader(const PkgHeader& hdr, uint64_t file_size);

    // Read null-terminated string from fixed-size char array
    static std::string ReadFixedString(const char* buffer, size_t max_len);

    // Convert big-endian fields to host byte order
    static uint16_t Be16(uint16_t val);
    static uint32_t Be32(uint32_t val);
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_PKG_PARSER_H_ */