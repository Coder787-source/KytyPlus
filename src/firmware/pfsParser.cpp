#include "firmware/pfsParser.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace Libs::Firmware {

// ---- Magic checks ----

bool PfsParser::HasPfsMagic(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    // PFS magic = 20130315 = 0x013926B3 (little-endian: B3 26 39 01)
    return data[0] == 0xB3 && data[1] == 0x26 && data[2] == 0x39 && data[3] == 0x01;
}

bool PfsParser::HasPfscMagic(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    // PFSC magic = 0x43534650 = "PFSC" (little-endian: 50 46 53 43)
    return data[0] == 0x50 && data[1] == 0x46 && data[2] == 0x53 && data[3] == 0x43;
}

// ---- Inode reading ----

bool PfsParser::ReadInode(std::ifstream& f, uint32_t block_number,
                            uint32_t block_size, bool is_64bit,
                            PfsInodeD32& out_inode) {
    if (block_number == 0 || block_number > 0xFFFFFF) {
        return false;
    }

    const uint64_t offset = static_cast<uint64_t>(block_number) * block_size;
    f.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

    if (is_64bit) {
        // 64-bit inodes use a larger structure (S32/S64); we read the D32 portion
        // and skip the rest. Full 64-bit support would need the larger struct.
        // For now, read the first INODE_D32_SIZE bytes which overlap the common fields.
        f.read(reinterpret_cast<char*>(&out_inode), INODE_D32_SIZE);
        if (static_cast<size_t>(f.gcount()) < INODE_D32_SIZE) {
            return false;
        }
    } else {
        f.read(reinterpret_cast<char*>(&out_inode), INODE_D32_SIZE);
        if (static_cast<size_t>(f.gcount()) < INODE_D32_SIZE) {
            return false;
        }
    }

    return true;
}

// ---- Directory reading ----

std::vector<std::pair<std::string, uint32_t>> PfsParser::ReadDirectory(
    std::ifstream& f, const PfsInodeD32& dir_inode,
    uint32_t block_size, uint32_t num_blocks) {

    std::vector<std::pair<std::string, uint32_t>> entries;

    // Read directory data from direct blocks
    for (size_t i = 0; i < MAX_DIRECT_BLOCKS; ++i) {
        const int32_t blk = dir_inode.db[i];
        if (blk < 0 || static_cast<uint32_t>(blk) >= num_blocks) {
            break; // no more direct blocks
        }

        const uint64_t offset = static_cast<uint64_t>(blk) * block_size;
        f.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

        // Read the block
        std::vector<uint8_t> block_data(block_size);
        f.read(reinterpret_cast<char*>(block_data.data()), block_size);
        const auto got = static_cast<size_t>(f.gcount());
        if (got == 0) break;
        block_data.resize(got);

        // Parse dirent entries from the block
        size_t pos = 0;
        while (pos + sizeof(PfsDirent) <= block_data.size()) {
            PfsDirent dirent;
            std::memcpy(&dirent, block_data.data() + pos, sizeof(PfsDirent));

            // Validate type
            if (dirent.type == 0 || dirent.type > DIRENT_TYPE_DOTDOT) {
                break; // end of entries in this block
            }

            // Read the name
            pos += sizeof(PfsDirent);
            if (pos + dirent.name_size > block_data.size()) {
                break;
            }

            std::string name;
            for (uint32_t j = 0; j < dirent.name_size && pos < block_data.size(); ++j) {
                if (block_data[pos] == '\0') {
                    pos++;
                    break;
                }
                name += static_cast<char>(block_data[pos++]);
            }

            // Skip dot/dotdot entries
            if (dirent.type != DIRENT_TYPE_DOT && dirent.type != DIRENT_TYPE_DOTDOT) {
                entries.emplace_back(name, dirent.inode);
            }
        }
    }

    return entries;
}

// ---- PFSC decompression ----

std::vector<uint8_t> PfsParser::DecompressPfscBlock(
    std::ifstream& f, uint32_t block_offset, uint32_t block_size) {

    // PFSC compressed block: header + compressed data
    // NOTE: Full PFSC decompression requires zlib. Since the firmware library
    // does not link zlib, this returns the raw block data. PFSC-compressed
    // files will not decompress correctly — the parser logs this and the
    // caller falls back to raw body.pfs extraction. Linking zlib to the
    // firmware target would enable full PFSC support.

    f.seekg(static_cast<std::streamoff>(block_offset), std::ios::beg);
    std::vector<uint8_t> raw(block_size);
    f.read(reinterpret_cast<char*>(raw.data()), block_size);
    const auto got = static_cast<size_t>(f.gcount());
    raw.resize(got);

    // Check if this is a PFSC-compressed block
    if (raw.size() >= 4 && raw[0] == 0x50 && raw[1] == 0x46 && raw[2] == 0x53 && raw[3] == 0x43) {
        LOGF("PFS: PFSC compressed block detected at 0x%X (decompression requires zlib - not linked)", block_offset);
        // Return raw data; the caller should detect the compression flag and handle accordingly
    }

    return raw; // return raw (uncompressed if not PFSC, raw compressed if PFSC)
}

// ---- File data reading ----

std::vector<uint8_t> PfsParser::ReadFileData(
    std::ifstream& f, const PfsInodeD32& inode,
    uint32_t block_size, uint32_t num_blocks,
    bool is_compressed) {

    std::vector<uint8_t> data;
    data.reserve(inode.size);

    uint32_t remaining = inode.size;
    for (size_t i = 0; i < MAX_DIRECT_BLOCKS && remaining > 0; ++i) {
        const int32_t blk = inode.db[i];
        if (blk < 0 || static_cast<uint32_t>(blk) >= num_blocks) {
            break;
        }

        const uint64_t offset = static_cast<uint64_t>(blk) * block_size;
        f.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

        const uint32_t to_read = std::min(remaining, block_size);
        std::vector<uint8_t> block_data(to_read);
        f.read(reinterpret_cast<char*>(block_data.data()), to_read);
        const auto got = static_cast<size_t>(f.gcount());
        block_data.resize(got);

        if (is_compressed && inode.flags & INODE_FLAG_COMPRESSED) {
            block_data = DecompressPfscBlock(f, static_cast<uint32_t>(offset), block_size);
        }

        data.insert(data.end(), block_data.begin(),
                    block_data.begin() + std::min<size_t>(got, remaining));
        remaining -= static_cast<uint32_t>(got);
    }

    return data;
}

// ---- Main parse ----

PfsParseResult PfsParser::Parse(const std::string& pfs_path) {
    PfsParseResult result;
    result.ok = false;
    result.is_encrypted = false;
    result.is_compressed = false;
    result.version = 0;
    result.mode = 0;
    result.block_size = 0;
    result.num_blocks = 0;
    result.num_inodes = 0;

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

    // Validate magic
    if (sb.magic != PFS_MAGIC) {
        // Check if it's PFSC (compressed PFS)
        f.seekg(0, std::ios::beg);
        uint8_t magic4[4] = {0};
        f.read(reinterpret_cast<char*>(magic4), 4);
        if (magic4[0] == 0x50 && magic4[1] == 0x46 && magic4[2] == 0x53 && magic4[3] == 0x43) {
            // PFSC compressed — the real PFS superblock is inside the decompressed data
            result.is_compressed = true;
            LOGF("PFS: PFSC compressed image detected (full decompression not yet implemented)");
            result.error = "PFS image is PFSC-compressed (full decompression not yet implemented)";
            result.ok = false;
            return result;
        }

        result.error = "Invalid PFS magic (expected 20130315, got " +
                        std::to_string(sb.magic) + ")";
        return result;
    }

    result.version = sb.version;
    result.mode = sb.mode;
    result.block_size = sb.block_size;
    result.num_blocks = sb.num_blocks;
    result.num_inodes = sb.num_inodes;
    result.is_encrypted = (sb.mode & PFS_MODE_ENCRYPTED) != 0;
    result.is_compressed = false;

    LOGF("PFS: version=%u (%s), mode=0x%X, block_size=%u, num_blocks=%u, num_inodes=%u",
         result.version, result.version == PFS_VERSION_PS5 ? "PS5" : "PS4",
         result.mode, result.block_size, result.num_blocks, result.num_inodes);

    if (result.is_encrypted) {
        LOGF("PFS: image is encrypted — file data requires EKPFS keys (not provided by emulator)");
    }

    // Validate basic sanity
    if (result.block_size == 0 || result.block_size > 0x100000) {
        result.error = "Invalid block size: " + std::to_string(result.block_size);
        return result;
    }
    if (result.num_blocks == 0 || result.num_blocks > 0xFFFFFF) {
        result.error = "Invalid block count: " + std::to_string(result.num_blocks);
        return result;
    }

    // If encrypted, we can only parse the superblock (file data needs keys)
    if (result.is_encrypted) {
        result.ok = true; // superblock parsed OK
        return result;
    }

    // For plaintext images, enumerate the root directory
    // The root inode is typically inode 2 (following Unix convention)
    // or inode 3 in some PFS variants. We try both.
    const bool is_64bit = (sb.mode & PFS_MODE_64BIT_INODES) != 0;

    // Root inode is typically at block 1 (the inode table starts at block 1)
    // Inode 2 = root directory (standard Unix-like layout)
    PfsInodeD32 root_inode{};
    bool found_root = false;

    // Try block 1, 2, 3 for the root inode
    for (uint32_t try_block = 1; try_block <= 3 && !found_root; ++try_block) {
        if (ReadInode(f, try_block, result.block_size, is_64bit, root_inode)) {
            if ((root_inode.mode & INODE_MODE_DIR) != 0 && root_inode.number >= 1) {
                found_root = true;
                LOGF("PFS: root directory inode found at block %u (inode %u)",
                     try_block, root_inode.number);
            }
        }
    }

    if (!found_root) {
        LOGF("PFS: root directory inode not found, listing skipped");
        result.ok = true; // superblock parsed OK, just couldn't enumerate
        return result;
    }

    // Read root directory entries
    auto dir_entries = ReadDirectory(f, root_inode, result.block_size, result.num_blocks);

    LOGF("PFS: root directory has %zu entries", dir_entries.size());

    // For each entry, read its inode to get file info
    for (const auto& [name, inode_num] : dir_entries) {
        if (inode_num == 0 || inode_num > result.num_inodes) continue;

        PfsInodeD32 entry_inode{};
        // Inode block = inode_num (inodes are 1-indexed, inode N is at block N)
        // This is approximate — the real mapping depends on the inode table layout.
        if (ReadInode(f, inode_num, result.block_size, is_64bit, entry_inode)) {
            PfsFile file;
            file.name = name;
            file.inode = entry_inode.number;
            file.size = entry_inode.size;
            file.is_compressed = (entry_inode.flags & INODE_FLAG_COMPRESSED) != 0;
            file.block_number = (entry_inode.db[0] > 0) ? static_cast<uint32_t>(entry_inode.db[0]) : 0;
            result.files.push_back(file);

            LOGF("PFS:  %s%s (inode=%u, size=%u, block=%u)",
                 name.c_str(),
                 (entry_inode.mode & INODE_MODE_DIR) ? "/" : "",
                 file.inode, file.size, file.block_number);
        }
    }

    result.ok = true;
    return result;
}

// ---- Extraction ----

uint32_t PfsParser::ExtractAll(const PfsParseResult& result,
                                 const std::string& pfs_path,
                                 const std::string& output_dir) {
    if (!result.ok) {
        LOGF("PFS: cannot extract - parse failed: %s", result.error.c_str());
        return 0;
    }

    if (result.is_encrypted) {
        LOGF("PFS: cannot extract - image is encrypted, requires EKPFS keys");
        return 0;
    }

    if (result.is_compressed) {
        LOGF("PFS: cannot extract - image is PFSC-compressed (not yet implemented)");
        return 0;
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

        const std::filesystem::path out_path = std::filesystem::path(output_dir) / file.name;
        std::ofstream out(out_path, std::ios::binary);
        if (!out) {
            LOGF("PFS: cannot create %s", out_path.string().c_str());
            continue;
        }

        // Read file data from inode's direct blocks
        // We need to re-read the inode to get block pointers
        PfsInodeD32 inode{};
        const bool is_64bit = (result.mode & PFS_MODE_64BIT_INODES) != 0;
        if (ReadInode(f, file.inode, result.block_size, is_64bit, inode)) {
            auto data = ReadFileData(f, inode, result.block_size,
                                     result.num_blocks, result.is_compressed);
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