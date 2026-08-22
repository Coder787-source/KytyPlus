#ifndef KYTY_FIRMWARE_PFS_PARSER_H_
#define KYTY_FIRMWARE_PFS_PARSER_H_

#include <array>
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
// or plaintext (debug/decrypted images). This parser handles both cases:
//   - Plaintext: full enumeration + extraction
//   - Encrypted: superblock parse + enumeration (data needs EKPFS keys)
//   - PFSC compressed: full decompression via zlib
//
// Format constants verified against MkPFS (github.com/PSBrew/MkPFS) consts.py.

#pragma pack(push, 1)

// PFS header/superblock (at offset 0 of the PFS image)
// Verified layout from orbis-pfs (videobitva/orbis) and PSDevWiki.
// All multi-byte fields are LITTLE-ENDIAN on disk.
// The 'magic' is actually the 'format' field at 0x08 (U64 = 20130315).
// Full header size is 0x380 bytes (includes key seed at 0x370).
struct PfsSuperblock {
    uint64_t version;         // 0x00: header version (always 1)
    uint64_t format;          // 0x08: format magic = 20130315 (0x01332A0B)
    uint64_t id;              // 0x10: filesystem id
    uint8_t  flags[4];        // 0x18: fmode/clean/ronly/rsv
    uint16_t mode;            // 0x1C: mode flags
    uint16_t unk_1e;          // 0x1E: unknown
    uint32_t block_size;      // 0x20: filesystem block size (e.g. 0x10000 = 64KB)
    uint32_t nbackup;         // 0x24: backup block count
    uint64_t num_blocks;      // 0x28: total blocks in image
    uint64_t num_inodes;      // 0x30: number of inodes
    uint64_t num_data_blocks; // 0x38: number of data blocks
    uint64_t num_inode_blocks;// 0x40: number of inode blocks
    uint64_t root_inode;     // 0x48: root inode number
    // ... more fields follow up to 0x380 (includes key seed at 0x370)
};

// Convenience: the real PFS 'magic' is the format field at offset 0x08
static constexpr uint64_t PFS_FORMAT_MAGIC = 20130315;  // format field value

// PFS inode — D32 variant (32-bit block pointers, 0xA8 = 168 bytes)
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

// PFS inode — S32 variant (32-bit block pointers, extended, 0x2C8 = 712 bytes)
// Used when PFS_MODE_64BIT_INODES is set AND version is PS4
struct PfsInodeS32 {
    uint32_t number;          // 0x00: inode number
    uint16_t mode;             // 0x04: file mode
    uint16_t nlink;            // 0x06: link count
    uint32_t uid;              // 0x08: owner UID
    uint32_t gid;              // 0x0C: group GID
    uint32_t flags;            // 0x10: inode flags
    uint32_t blocks;           // 0x14: block count
    uint64_t size;             // 0x18: file size in bytes (64-bit)
    uint32_t unk_20;           // 0x20: unknown
    uint32_t unk_24;           // 0x24: unknown
    uint32_t unk_28;           // 0x28: unknown
    uint32_t unk_2c;           // 0x2C: unknown
    int32_t  db[12];           // 0x30: direct block pointers (12 entries)
    int32_t  ib[5];            // 0x60: indirect block pointers (5 entries)
    uint8_t  pad[0x2C8 - 0x60 - 20]; // padding to 0x2C8
};

// PFS inode — S64 variant (64-bit block pointers, 0x310 = 784 bytes)
// Used when PFS_MODE_64BIT_INODES is set AND version is PS5
struct PfsInodeS64 {
    uint32_t number;          // 0x00: inode number
    uint16_t mode;             // 0x04: file mode
    uint16_t nlink;            // 0x06: link count
    uint32_t uid;              // 0x08: owner UID
    uint32_t gid;              // 0x0C: group GID
    uint32_t flags;            // 0x10: inode flags
    uint32_t blocks;           // 0x14: block count
    uint64_t size;             // 0x18: file size in bytes (64-bit)
    uint32_t unk_20;           // 0x20: unknown
    uint32_t unk_24;           // 0x24: unknown
    uint32_t unk_28;           // 0x28: unknown
    uint32_t unk_2c;           // 0x2C: unknown
    int64_t  db[12];           // 0x30: direct block pointers (12 entries, 64-bit)
    int64_t  ib[5];            // 0x90: indirect block pointers (5 entries, 64-bit)
    uint8_t  pad[0x310 - 0x90 - 40]; // padding to 0x310
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
static constexpr size_t   INODE_S32_SIZE          = 0x2C8; // 712 bytes
static constexpr size_t   INODE_S64_SIZE          = 0x310; // 784 bytes
static constexpr size_t   MAX_DIRECT_BLOCKS        = 12;
static constexpr size_t   MAX_INDIRECT_BLOCKS      = 5;

// PFSC (compressed PFS) constants
static constexpr uint32_t PFSC_MAGIC              = 0x43534650; // "PFSC" (little-endian)
static constexpr uint32_t PFSC_LOGICAL_BLOCK_SIZE = 0x10000;    // 64KB
static constexpr uint32_t PFSC_HEADER_SIZE        = 0x30;
static constexpr uint32_t PFSC_OFFSET_ENTRY_SIZE  = 0x8;
static constexpr uint32_t PFSC_BLOCK_OFFSETS_OFFSET = 0x400;
static constexpr uint32_t PFSC_INITIAL_DATA_OFFSET  = 0x10000;

// AES-XTS sector size (for encrypted PFS)
static constexpr uint32_t PFS_XTS_SECTOR_SIZE      = 0x1000; // 4KB

// EKPFS key size (32 bytes = 16-byte data key + 16-byte tweak key)
static constexpr size_t   EKPFS_KEY_SIZE           = 32;

// Extracted file from PFS
struct PfsFile {
    std::string name;        // file name
    uint32_t inode;          // inode number
    uint64_t size;            // file size in bytes
    uint32_t block_number;   // first data block
    bool is_compressed;       // PFSC compressed
    bool is_directory;        // true if this is a directory
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

// EKPFS key material for encrypted PFS decryption
struct PfsEkpfsKey {
    std::array<uint8_t, 16> tweak_key;  // AES-XTS tweak key
    std::array<uint8_t, 16> data_key;   // AES-XTS data key
};

class PfsParser {
public:
    // Parse a PFS image file.
    // Reads the superblock, validates magic, detects encryption/compression,
    // and enumerates the root directory to list files.
    // For encrypted images, the superblock + directory structure is parsed.
    // File data extraction requires EKPFS keys (use ExtractWithKeys).
    static PfsParseResult Parse(const std::string& pfs_path);

    // Extract all files from a decrypted (plaintext) PFS image to output_dir.
    // Returns the number of files extracted. Returns 0 for encrypted images
    // without keys.
    static uint32_t ExtractAll(const PfsParseResult& result,
                                 const std::string& pfs_path,
                                 const std::string& output_dir,
                                 const PfsEkpfsKey* ekpfs_key = nullptr);

    // Check if data starts with PFS magic
    static bool HasPfsMagic(const std::vector<uint8_t>& data);

    // Check if data starts with PFSC (compressed PFS) magic
    static bool HasPfscMagic(const std::vector<uint8_t>& data);

private:
    // Read an inode at a given block number (handles D32/S32/S64 variants)
    static bool ReadInode(std::ifstream& f, uint32_t block_number,
                           uint32_t block_size, uint32_t version, uint32_t mode,
                           PfsInodeD32& out_d32, PfsInodeS32& out_s32,
                           PfsInodeS64& out_s64, int& out_variant);

    // Unified inode info extracted from any variant
    struct InodeInfo {
        uint32_t number;
        uint16_t mode;
        uint32_t flags;
        uint64_t size;
        int64_t  db[MAX_DIRECT_BLOCKS];
        int64_t  ib[MAX_INDIRECT_BLOCKS];
    };

    // Extract unified inode info from any variant
    static InodeInfo ExtractInodeInfo(const PfsInodeD32& d32,
                                       const PfsInodeS32& s32,
                                       const PfsInodeS64& s64,
                                       int variant);

    // Read directory entries from a directory inode's data blocks
    // Handles both direct and indirect blocks
    static std::vector<std::pair<std::string, uint32_t>> ReadDirectory(
        std::ifstream& f, const InodeInfo& dir_inode,
        uint32_t block_size, uint32_t num_blocks,
        const PfsEkpfsKey* ekpfs_key);

    // Read file data from inode's direct + indirect blocks
    // Handles both direct and indirect block pointers for files > 12 blocks
    static std::vector<uint8_t> ReadFileData(
        std::ifstream& f, const InodeInfo& inode,
        uint32_t block_size, uint32_t num_blocks,
        bool is_compressed, const PfsEkpfsKey* ekpfs_key);

    // Read a data block, decrypting if EKPFS key is provided
    static std::vector<uint8_t> ReadBlock(
        std::ifstream& f, int64_t block_num, uint32_t block_size,
        uint32_t num_blocks, const PfsEkpfsKey* ekpfs_key);

    // Decompress a PFSC block (requires zlib)
    static std::vector<uint8_t> DecompressPfscBlock(
        const std::vector<uint8_t>& raw_block);

    // Follow indirect block chain and collect all block numbers
    static std::vector<int64_t> GetIndirectBlocks(
        std::ifstream& f, const InodeInfo& inode,
        uint32_t block_size, uint32_t num_blocks,
        const PfsEkpfsKey* ekpfs_key);

    // AES-XTS decrypt a sector (self-contained, no OpenSSL)
    static std::vector<uint8_t> AesXtsDecryptSector(
        const uint8_t* sector_data, size_t sector_size,
        const PfsEkpfsKey& key, uint64_t sector_number);
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_PFS_PARSER_H_ */