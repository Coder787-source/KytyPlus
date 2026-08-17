#ifndef KYTY_FIRMWARE_PFS_PARSER_H_
#define KYTY_FIRMWARE_PFS_PARSER_H_

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace Libs::Firmware {

// PFS (PlayStation File System) parser.
//
// Parses the PS4/PS5 PFS image format used inside .pkg package bodies.
// Based on the publicly documented PFS format (psdevwiki, MkPFS project).
//
// The PFS superblock (header) is UNENCRYPTED and can be parsed without keys.
// File data blocks may be encrypted (AES-XTS) requiring user-supplied EKPFS keys,
// or plaintext (debug/decrypted images). This parser handles the plaintext case
// fully and detects encryption without attempting decryption.
//
// Format constants verified against MkPFS (github.com/PSBrew/MkPFS) consts.py.

#pragma pack(push, 1)

// PFS superblock (at offset 0 of the PFS image)
// Magic: 20130315 (0x013926B3) as uint32 little-endian
struct PfsSuperblock {
    uint32_t magic;           // 0x00: 20130315 (0x013926B3)
    uint32_t version;         // 0x04: 1=PS4, 2=PS5
    uint32_t mode;            // 0x08: flags (0x4 = encrypted, 0x2 = 64-bit inodes)
    uint32_t block_size;      // 0x0C: filesystem block size (e.g. 0x10000 = 64KB)
    uint32_t num_blocks;      // 0x10: total blocks in image
    uint32_t num_inodes;      // 0x14: total inodes
    uint32_t unk_18;           // 0x18: unknown
    uint32_t unk_1c;           // 0x1C: unknown
    // ... more fields follow (the full superblock is larger, but these
    //     are the fields we need for parsing)
};

// PFS inode (D32 variant, 0xA8 bytes = 168 bytes)
// Used when PFS_MODE_64BIT_INODES is NOT set
struct PfsInodeD32 {
    uint32_t number;          // 0x00: inode number
    uint16_t mode;             // 0x04: file mode (0x4000=dir, 0x8000=file)
    uint16_t nlink;            // 0x06: link count
    uint32_t uid;              // 0x08: owner UID
    uint32_t gid;              // 0x0C: group GID
    uint32_t flags;            // 0x10: inode flags (0x1=compressed, 0x10=readonly)
    uint32_t blocks;           // 0x14: block count
    uint32_t size;             // 0x18: file size in bytes
    uint32_t unk_1c;           // 0x1C: unknown
    int32_t  db[12];           // 0x20: direct block pointers (12 entries)
    int32_t  ib[5];            // 0x50: indirect block pointers (5 entries)
    uint8_t  pad[0xA8 - 0x50 - 20]; // padding to 0xA8
};

// PFS directory entry
struct PfsDirent {
    uint32_t type;            // 2=file, 3=directory, 4=dot, 5=dotdot
    uint32_t inode;           // inode number this entry points to
    uint32_t name_size;       // length of the name string (including null)
    uint32_t pad;              // padding
    // Followed by name_size bytes of name (null-terminated)
};

#pragma pack(pop)

// PFS format constants (verified against MkPFS consts.py)
static constexpr uint32_t PFS_MAGIC              = 20130315;  // 0x013926B3
static constexpr uint32_t PFS_VERSION_PS4        = 1;
static constexpr uint32_t PFS_VERSION_PS5        = 2;
static constexpr uint32_t PFS_MODE_SIGNED        = 0x1;
static constexpr uint32_t PFS_MODE_64BIT_INODES   = 0x2;
static constexpr uint32_t PFS_MODE_ENCRYPTED      = 0x4;
static constexpr uint32_t PFS_MODE_CASE_INSENSITIVE = 0x8;

static constexpr uint32_t INODE_MODE_DIR          = 0x4000;
static constexpr uint32_t INODE_MODE_FILE         = 0x8000;

static constexpr uint32_t INODE_FLAG_COMPRESSED   = 0x1;
static constexpr uint32_t INODE_FLAG_READONLY     = 0x10;

static constexpr uint32_t DIRENT_TYPE_FILE        = 2;
static constexpr uint32_t DIRENT_TYPE_DIRECTORY   = 3;
static constexpr uint32_t DIRENT_TYPE_DOT         = 4;
static constexpr uint32_t DIRENT_TYPE_DOTDOT      = 5;

static constexpr size_t   INODE_D32_SIZE          = 0xA8;  // 168 bytes
static constexpr size_t   MAX_DIRECT_BLOCKS        = 12;
static constexpr size_t   MAX_INDIRECT_BLOCKS      = 5;

// PFSC (compressed PFS) constants
static constexpr uint32_t PFSC_MAGIC              = 0x43534650; // "PFSC" (little-endian)
static constexpr uint32_t PFSC_LOGICAL_BLOCK_SIZE = 0x10000;    // 64KB
static constexpr uint32_t PFSC_HEADER_SIZE        = 0x30;
static constexpr uint32_t PFSC_OFFSET_ENTRY_SIZE  = 0x8;
static constexpr uint32_t PFSC_BLOCK_OFFSETS_OFFSET = 0x400;
static constexpr uint32_t PFSC_INITIAL_DATA_OFFSET  = 0x10000;

// Extracted file from PFS
struct PfsFile {
    std::string name;        // file name
    uint32_t inode;          // inode number
    uint32_t size;            // file size in bytes
    uint32_t block_number;   // first data block
    bool is_compressed;       // PFSC compressed
    std::vector<uint8_t> data; // extracted file data (for small files)
};

// PFS parse result
struct PfsParseResult {
    bool ok;
    std::string error;
    uint32_t version;           // 1=PS4, 2=PS5
    uint32_t mode;              // mode flags
    uint32_t block_size;        // block size
    uint32_t num_blocks;        // total blocks
    uint32_t num_inodes;        // total inodes
    bool is_encrypted;          // true if PFS_MODE_ENCRYPTED set
    bool is_compressed;         // true if PFSC format detected
    std::vector<PfsFile> files; // extracted file entries
};

class PfsParser {
public:
    // Parse a PFS image file.
    // Reads the superblock, validates magic, detects encryption/compression,
    // and enumerates the root directory to list files.
    // For encrypted images, only the superblock + directory structure is parsed
    // (file data cannot be read without EKPFS keys).
    static PfsParseResult Parse(const std::string& pfs_path);

    // Extract all files from a decrypted (plaintext) PFS image to output_dir.
    // Returns the number of files extracted. Returns 0 for encrypted images.
    static uint32_t ExtractAll(const PfsParseResult& result,
                                 const std::string& pfs_path,
                                 const std::string& output_dir);

    // Check if data starts with PFS magic
    static bool HasPfsMagic(const std::vector<uint8_t>& data);

    // Check if data starts with PFSC (compressed PFS) magic
    static bool HasPfscMagic(const std::vector<uint8_t>& data);

private:
    // Read an inode at a given block number
    static bool ReadInode(std::ifstream& f, uint32_t block_number,
                           uint32_t block_size, bool is_64bit,
                           PfsInodeD32& out_inode);

    // Read directory entries from a directory inode's data blocks
    static std::vector<std::pair<std::string, uint32_t>> ReadDirectory(
        std::ifstream& f, const PfsInodeD32& dir_inode,
        uint32_t block_size, uint32_t num_blocks);

    // Decompress a PFSC block (zlib)
    static std::vector<uint8_t> DecompressPfscBlock(
        std::ifstream& f, uint32_t block_offset, uint32_t block_size);

    // Read file data from inode's direct blocks (for small files)
    static std::vector<uint8_t> ReadFileData(
        std::ifstream& f, const PfsInodeD32& inode,
        uint32_t block_size, uint32_t num_blocks,
        bool is_compressed);
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_PFS_PARSER_H_ */