#include "package/pkgParser.h"
#include "package/pfsParser.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <map>

namespace {

uint64_t ReadLe64(const uint8_t* bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= uint64_t(bytes[i]) << (8 * i);
    return value;
}

bool ContainsRange(uint64_t total, uint64_t offset, uint64_t size) {
    return offset <= total && size <= total - offset;
}

bool ReadAt(std::ifstream& file, uint64_t offset, void* data, size_t size) {
    file.clear();
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    file.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<size_t>(file.gcount()) == size;
}

} // namespace

namespace Libs::Firmware {

// ---- Byte-swap helpers (PKG header is big-endian) ----

uint16_t PkgParser::Be16(uint16_t val) {
    return static_cast<uint16_t>((val >> 8) | (val << 8));
}

uint32_t PkgParser::Be32(uint32_t val) {
    return ((val >> 24) & 0x000000FF) |
           ((val >> 8)  & 0x0000FF00) |
           ((val << 8)  & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

// ---- Fixed-string reader ----

std::string PkgParser::ReadFixedString(const char* buffer, size_t max_len) {
    std::string str;
    for (size_t i = 0; i < max_len; ++i) {
        if (buffer[i] == '\0') break;
        str += buffer[i];
    }
    return str;
}

// ---- PFS magic check ----

bool PkgParser::HasPfsMagic(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    // PFS magic: "PFS\0" = 0x50 0x46 0x53 0x00
    return data[0] == 0x50 && data[1] == 0x46 && data[2] == 0x53 && data[3] == 0x00;
}

// ---- Encryption detection ----

bool PkgParser::IsEncrypted(const std::string& pkg_path,
                            uint32_t body_offset,
                            uint64_t body_size,
                            uint64_t* out_pfs_image_offset) {
    if (out_pfs_image_offset) *out_pfs_image_offset = 0;

    // The PFS superblock's 'format' (magic) field lives at superblock offset 0x08
    // (version U64 @ 0x00, format U64 @ 0x08). Its LE32 low word is:
    //   PFS_MAGIC = 20130315 = 0x01332A0B  => bytes 0B 2A 33 01
    //
    // The PFS image is NOT guaranteed to start at body_offset. In retail and fake
    // packages the file-entry/name tables occupy the first body region, and the
    // PFS superblock commonly sits exactly at body_offset + body_size (or later).
    // A blind read at body_offset would return non-PFS bytes and falsely mark a
    // plaintext image "encrypted". We therefore stream from body_offset to EOF
    // and locate the magic with a fixed-size sliding window (constant memory,
    // O(file_size)).
    //
    // A PFSC-compressed PFS has ASCII "PFSC" at its own offset 0x00, which is
    // likewise plaintext and not "encrypted".

    std::ifstream f(pkg_path, std::ios::binary);
    if (!f) return true; // treat unreadable as encrypted

    std::error_code ec;
    const uint64_t total = std::filesystem::file_size(pkg_path, ec);
    if (ec || total == 0) return true;

    const uint64_t start = static_cast<uint64_t>(body_offset);
    const uint64_t end = total; // scan body end and beyond; superblock can sit at body_size boundary
    if (start >= end) return true;

    constexpr size_t kChunk = 1 << 16;           // 64 KiB read
    constexpr size_t kNeedle = 11;               // 11-byte overlap (magic needs 12)
    std::vector<uint8_t> chunk(kChunk);
    std::vector<uint8_t> window;
    window.reserve(kChunk + kNeedle);

    uint64_t abs = start;
    while (abs < end) {
        const size_t want = static_cast<size_t>(std::min<uint64_t>(end - abs, kChunk));
        f.seekg(static_cast<std::streamoff>(abs), std::ios::beg);
        f.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(want));
        const size_t got = static_cast<size_t>(f.gcount());
        if (got == 0) break;

        // Rebuild scan window = [tail of previous | fresh chunk]
        window.erase(window.begin(), window.begin() +
                     (window.size() > kNeedle ? window.size() - kNeedle : window.size()));
        const size_t old = window.size();
        window.resize(old + got);
        std::memcpy(window.data() + old, chunk.data(), got);

        // Absolute file offset of window[0..old) is (abs - old) for the tail bytes,
        // but we only need the offset of the magic, which maps to a known position:
        //   a magic at window[k] has absolute offset = (abs - old) + k
        // Because the tail was the last kNeedle bytes ending at 'abs' (start of chunk).
        const uint64_t win_base = abs - old;
        for (size_t k = 0; k + 4 <= window.size(); ++k) {
            const uint64_t off = win_base + k;
            if (off >= 8) {
                const bool is_pfs = (window[k]==0x0B && window[k+1]==0x2A &&
                                     window[k+2]==0x33 && window[k+3]==0x01);
                if (is_pfs) {
                    if (out_pfs_image_offset) *out_pfs_image_offset = off - 8;
                    return false;
                }
            }
            const bool is_pfsc = (window[k]=='P' && window[k+1]=='F' &&
                                  window[k+2]=='S' && window[k+3]=='C');
            if (is_pfsc) {
                if (out_pfs_image_offset) *out_pfs_image_offset = off;
                return false;
            }
        }
        abs += got;
    }

    return true; // no PFS/compressed magic found
}

// ---- Header validation ----

bool PkgParser::ValidatePkgHeader(const PkgHeader& hdr, uint64_t file_size) {
    // Magic is stored big-endian in the file; read raw and compare
    const uint32_t raw_magic = hdr.magic; // as read from file (no swap)
    if (raw_magic != PKG_MAGIC) {
        // Also check byte-swapped in case the read was little-endian
        if (Be32(raw_magic) != PKG_MAGIC) {
            return false;
        }
    }

    // Body offset sanity
    const uint32_t body_off = Be32(hdr.body_offset);
    if (body_off != PKG_BODY_OFFSET && body_off != 0) {
        // Some debug PKGs may have different offsets; accept 0x200 or 0
        if (body_off < 0x100 || body_off > file_size) {
            return false;
        }
    }

    // Body size sanity
    const uint32_t body_sz = Be32(hdr.body_size);
    if (body_sz > 0 && body_sz > file_size) {
        return false;
    }

    return true;
}

// ---- PFS superblock encryption check ----
//
// The plaintext-magic heuristic in IsEncrypted() is NOT sufficient: an encrypted
// PFS image still stores its superblock UNENCRYPTED (so the magic at format+0x08
// is present), which made this parser report a retail/fake encrypted package as
// "decrypted" and then extract only garbage. The authoritative signal is the
// PFS_MODE_ENCRYPTED bit in the superblock mode field, so we read that directly.
static bool PfsSuperblockIsEncrypted(const std::string& path, uint64_t pfs_image_offset) {
    if (pfs_image_offset == 0) {
        return false;
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    // Layout: version U64 @0x00, format U64 @0x08, flags[4] @0x10, mode U16 @0x1C.
    uint8_t hdr[0x20] = {0};
    f.seekg(static_cast<std::streamoff>(pfs_image_offset), std::ios::beg);
    f.read(reinterpret_cast<char*>(hdr), sizeof(hdr));
    if (static_cast<size_t>(f.gcount()) < sizeof(hdr)) {
        return false;
    }
    const uint64_t format = *reinterpret_cast<const uint64_t*>(hdr + 0x08);
    if (format != 20130315ull) { // PFS_FORMAT_MAGIC
        return false;
    }
    const uint16_t mode = *reinterpret_cast<const uint16_t*>(hdr + 0x1C);
    constexpr uint16_t kPfsModeEncrypted = 0x4;
    return (mode & kPfsModeEncrypted) != 0;
}

// ---- Main parse ----

PkgParseResult PkgParser::Parse(const std::string& pkg_path) {
    PkgParseResult result{};
    result.ok = false;
    result.is_encrypted = false;
    result.keys_required_and_missing = false;
    result.file_count = 0;
    result.body_offset = 0;
    result.body_size = 0;

    std::ifstream f(pkg_path, std::ios::binary);
    if (!f) {
        result.error = "Cannot open PKG file: " + pkg_path;
        return result;
    }

    // Read header
    PkgHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (static_cast<size_t>(f.gcount()) < sizeof(hdr)) {
        result.error = "PKG file too small for header";
        return result;
    }

    const uint64_t file_size = std::filesystem::file_size(pkg_path);

    const auto* header_bytes = reinterpret_cast<const uint8_t*>(&hdr);
    if (std::memcmp(header_bytes, "\x7f" "FIH", 4) == 0) {
        // FIH is little-endian; its embedded CNT remains big-endian. Never
        // treat the superblock location as the base of the PFS block space.
        result.is_finalized_image = true;
        result.pfs_segment_offset = ReadLe64(header_bytes + 0x10);
        result.pfs_segment_size = ReadLe64(header_bytes + 0x18);
        result.pfs_image_offset = ReadLe64(header_bytes + 0x20);
        result.metadata_offset = ReadLe64(header_bytes + 0x58);
        // The embedded CNT body can extend beyond its FIH header-region
        // size. Derive its full extent from the CNT's own 64-bit fields.
        result.metadata_size = 0;
        if (result.pfs_segment_offset < 0x10000 || result.pfs_segment_size < 0x380 ||
            !ContainsRange(file_size, result.pfs_segment_offset, result.pfs_segment_size) ||
            result.metadata_offset < result.pfs_segment_offset + result.pfs_segment_size ||
            !ContainsRange(file_size, result.metadata_offset, sizeof(PkgHeader))) {
            result.error = "Invalid or truncated PS5 FIH segment bounds";
            return result;
        }
        PkgHeader metadata{};
        if (!ReadAt(f, result.metadata_offset, &metadata, sizeof(metadata)) ||
            std::memcmp(&metadata.magic, "\x7f" "CNT", 4) != 0) {
            result.error = "PS5 FIH metadata segment is missing its CNT header";
            return result;
        }
        const auto* cnt_bytes = reinterpret_cast<const uint8_t*>(&metadata);
        uint64_t cnt_body_offset = 0;
        uint64_t cnt_body_size = 0;
        for (unsigned i = 0; i < 8; ++i) {
            cnt_body_offset = (cnt_body_offset << 8) | cnt_bytes[0x20 + i];
            cnt_body_size = (cnt_body_size << 8) | cnt_bytes[0x28 + i];
        }
        const uint64_t cnt_available = file_size - result.metadata_offset;
        if (cnt_body_offset < sizeof(PkgHeader) ||
            !ContainsRange(cnt_available, cnt_body_offset, cnt_body_size)) {
            result.error = "PS5 FIH CNT body lies outside the package";
            return result;
        }
        result.metadata_size = cnt_body_offset + cnt_body_size;
        result.content_id = ReadFixedString(metadata.content_id, sizeof(metadata.content_id));
        result.file_count = Be32(metadata.file_count);
        const uint64_t sb_offset = result.pfs_image_offset == 0 ?
                                   result.pfs_segment_offset : result.pfs_image_offset;
        if (sb_offset < result.pfs_segment_offset ||
            !ContainsRange(result.pfs_segment_size, sb_offset - result.pfs_segment_offset, 0x380)) {
            result.error = "PS5 FIH superblock lies outside the PFS segment";
            return result;
        }
        uint8_t superblock[0x380]{};
        if (!ReadAt(f, sb_offset, superblock, sizeof(superblock)) ||
            ReadLe64(superblock + 8) != PFS_FORMAT_MAGIC) {
            result.error = "PS5 FIH PFS superblock is unreadable or unsupported";
            return result;
        }
        result.pfs_image_offset = sb_offset;
        result.is_plaintext_patched =
            std::memcmp(superblock + 0x370, "PPRPLAIN-NOAUTH!", 16) == 0;
        const uint16_t mode = uint16_t(superblock[0x1c]) | uint16_t(superblock[0x1d]) << 8;
        result.is_encrypted = (mode & PFS_MODE_ENCRYPTED) != 0 && !result.is_plaintext_patched;
        result.keys_required_and_missing = result.is_encrypted;
        if (sb_offset != result.pfs_segment_offset) {
            LOGF("PKG: data-first/NAPS layout (superblock inside segment) - scan extractor will be used");
        }
        result.ok = true;
        return result;
    }

    if (!ValidatePkgHeader(hdr, file_size)) {
        std::ostringstream message;
        message << "Invalid or unsupported PKG header (magic 0x" << std::hex << std::setfill('0');
        for (unsigned i = 0; i < 4; ++i) message << std::setw(2) << unsigned(header_bytes[i]);
        message << ')';
        result.error = message.str();
        return result;
    }

    // Parse fields (big-endian -> host)
    result.content_id = ReadFixedString(hdr.content_id, sizeof(hdr.content_id));
    result.file_count = Be32(hdr.file_count);
    result.body_offset = Be32(hdr.body_offset);
    if (result.body_offset == 0) result.body_offset = PKG_BODY_OFFSET;
    result.body_size = Be32(hdr.body_size);

    LOGF("PKG: content_id='%s', file_count=%u, body_offset=0x%X, body_size=%u",
         result.content_id.c_str(), result.file_count, result.body_offset, result.body_size);

    // Read file table (at table_offset, contains file entries with name + offset + size)
    const uint32_t table_offset = Be32(hdr.table_offset);
    const uint32_t table_entries = Be32(hdr.table_entries);

    // Entry-table sanity: the field is a count of 16-byte PKG file entries and
    // must not imply a table that overruns the file.
    const uint32_t max_header_entries = 4096;
    if (table_offset > 0 && table_offset < file_size && table_entries > 0 &&
        table_entries < max_header_entries) {
        const uint64_t table_bytes =
            static_cast<uint64_t>(table_entries) * PKG_ENTRY_SIZE;
        if (table_bytes <= file_size - table_offset) {
            // The name table pointer is the first u32 of the entry table.
            f.seekg(static_cast<std::streamoff>(table_offset), std::ios::beg);
            uint32_t name_table_off = 0;
            f.read(reinterpret_cast<char*>(&name_table_off), 4);
            name_table_off = Be32(name_table_off);

            if (name_table_off == 0) {
                name_table_off = PKG_NAMETABLE_DEFAULT_OFFSET;
            }

            if (name_table_off > 0 && name_table_off + PKG_NAMETABLE_MAX_READ <= file_size) {
                // Read name table (null-separated names). Bounds are enforced by
                // the +size check above, so a truncated or crafted table cannot
                // drive the seek/read past EOF.
                f.seekg(static_cast<std::streamoff>(name_table_off), std::ios::beg);
                std::vector<char> names(PKG_NAMETABLE_MAX_READ, 0);
                f.read(names.data(), static_cast<std::streamsize>(names.size()));
                const auto got = static_cast<size_t>(f.gcount());

                // Parse null-separated names
                size_t pos = 0;
                while (pos < got) {
                    std::string name;
                    while (pos < got && names[pos] != '\0') {
                        name += names[pos++];
                    }
                    ++pos; // skip null
                    if (!name.empty()) {
                        PkgFileEntry entry;
                        entry.name = name;
                        entry.offset = 0; // exact offsets require full entry table parsing
                        entry.size = 0;
                        result.files.push_back(entry);
                    }
                    if (result.files.size() >= table_entries) break;
                }

                if (result.files.empty()) {
                    LOGF("PKG: name table empty at 0x%X\n", name_table_off);
                }
            } else {
                LOGF("PKG: name table offset 0x%X out of bounds or missing\n", name_table_off);
            }
        } else {
            LOGF("PKG: entry table size %llu exceeds file remainder\n", table_bytes);
        }
    }

    // Detect encryption (check for PFS magic anywhere in the body region).
    result.pfs_image_offset = 0;
    // First check for empty body — an empty body is NOT encrypted, it's just empty
    if (result.body_size == 0) {
        result.is_encrypted = false;
        LOGF("PKG: body is empty (body_size=0) - nothing to extract");
    } else {
        result.is_encrypted = IsEncrypted(pkg_path, result.body_offset,
                                           result.body_size, &result.pfs_image_offset);

        if (!result.is_encrypted) {
            LOGF("PKG: body is decrypted (PFS magic at 0x%llX) - can extract without keys",
                 static_cast<unsigned long long>(result.pfs_image_offset));
        }
        // KytyPlus: the magic heuristic can be fooled (an encrypted PFS keeps a
        // plaintext superblock). The superblock mode bit is authoritative.
        if (PfsSuperblockIsEncrypted(pkg_path, result.pfs_image_offset)) {
            result.is_encrypted = true;
            result.keys_required_and_missing = true;
            LOGF("PKG: PFS superblock mode has the encrypted bit set - decryption required");
        }
        if (result.is_encrypted) {
            LOGF("PKG: body is encrypted - decryption is not supported");
        }
    }

    result.ok = true;
    return result;
}


// ============================================================================
// PS5 data-first / NAPS extraction (generic scan-based PFS reader)
// ============================================================================
// In a data-first (NAPS) package neither the outer nor the inner PFS image has
// its superblock at offset 0; the outer superblock sits near the segment end
// and the inner image (pfs_image.dat) fills the leading blocks. Nothing here
// assumes fixed offsets: the superblock was located during Parse() by a magic
// scan, the root directory block is found by scanning for a "."/".." dirent
// pair, and the inode table is located by cross-validating a block of sane,
// consecutively-numbered inodes whose root inode references that directory
// block. The tree is then walked via direct + indirect block pointers. The
// same routine runs recursively on the extracted inner image.
namespace {

struct ScanInodeInfo {
    uint32_t number = 0;
    uint16_t mode   = 0;
    uint32_t flags  = 0;
    uint64_t size   = 0;
    int64_t  db[12] = {};
    int64_t  ib[5]  = {};
};

struct ScanImage {
    std::ifstream f;
    uint64_t base   = 0;  // absolute file offset of block 0
    uint64_t limit  = 0;  // absolute offset past readable region
    uint64_t sb_abs = 0;  // absolute offset of the superblock
    uint32_t version = 0;
    uint32_t mode    = 0;
    uint32_t bsize   = 0;
    uint64_t nblocks = 0;
    uint64_t ninodes = 0;
    int      stride  = 0;  // 0xA8 (D32) or 0x310 (S64), resolved during table scan
    bool     dir_inode_first = true; // dirent field order
    uint32_t root_ino  = 0;
    std::map<uint32_t, uint64_t> inode_abs; // inode number -> absolute file offset
};

bool ScanRead(ScanImage& img, uint64_t abs_off, uint64_t n, uint8_t* out) {
    if (abs_off < img.base || abs_off > img.limit || n > img.limit - abs_off) return false;
    img.f.clear();
    img.f.seekg(static_cast<std::streamoff>(abs_off), std::ios::beg);
    img.f.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(n));
    return static_cast<uint64_t>(img.f.gcount()) == n;
}

ScanInodeInfo ScanParseRec(const uint8_t* rec, int stride) {
    ScanInodeInfo ino{};
    std::memcpy(&ino.number, rec + 0x00, 4);
    std::memcpy(&ino.mode,   rec + 0x04, 2);
    std::memcpy(&ino.flags,  rec + 0x10, 4);
    if (stride == 0x310) {
        std::memcpy(&ino.size, rec + 0x18, 8);
        for (int i = 0; i < 12; ++i) std::memcpy(&ino.db[i], rec + 0x30 + i * 8, 8);
        for (int i = 0; i < 5;  ++i) std::memcpy(&ino.ib[i], rec + 0x90 + i * 8, 8);
    } else {
        uint32_t sz32 = 0; std::memcpy(&sz32, rec + 0x18, 4);
        ino.size = sz32;
        int32_t v = 0;
        for (int i = 0; i < 12; ++i) { std::memcpy(&v, rec + 0x20 + i * 4, 4); ino.db[i] = v; }
        for (int i = 0; i < 5;  ++i) { std::memcpy(&v, rec + 0x50 + i * 4, 4); ino.ib[i] = v; }
    }
    return ino;
}

bool ScanInodeSane(const ScanImage& img, const ScanInodeInfo& ino) {
    const uint16_t m = ino.mode;
    const bool kind_ok = (m & 0xF000) == INODE_MODE_DIR || (m & 0xF000) == INODE_MODE_FILE || m == 2 || m == 3;
    if (!kind_ok) return false;
    if (ino.number == 0 || ino.number > 65535) return false;
    if (img.ninodes && ino.number > img.ninodes * 4) return false;
    if (((ino.mode & 0xF000) == INODE_MODE_FILE || ino.mode == 2) && ino.size > static_cast<uint64_t>(img.limit - img.base)) return false;
    int ptrs = 0;
    for (int i = 0; i < 12; ++i) {
        if (ino.db[i] <= 0) break;
        if (static_cast<uint64_t>(ino.db[i]) >= img.nblocks) return false;
        ++ptrs;
    }
    // A sane inode in a real table almost always references >= 1 data block.
    if (ptrs == 0) {
        for (int i = 0; i < 5; ++i) {
            if (ino.ib[i] <= 0) break;
            if (static_cast<uint64_t>(ino.ib[i]) >= img.nblocks) return false;
            ++ptrs;
        }
    }
    return ptrs > 0;
}

// Try to parse "." then ".." at block start with a given field order.
bool ScanProbeRootDir(const uint8_t* blk, size_t blen, bool inode_first, uint32_t* root_ino) {
    if (blen < 48) return false;
    const size_t ino_off  = inode_first ? 0 : 4;
    const size_t type_off = inode_first ? 4 : 0;
    uint32_t ino0 = 0; std::memcpy(&ino0,  blk + ino_off,  4);
    uint32_t typ0 = 0; std::memcpy(&typ0,  blk + type_off, 4);
    uint32_t ns0  = 0; std::memcpy(&ns0,   blk + 8,  4);
    if (typ0 != DIRENT_TYPE_DOT || ns0 == 0 || ns0 > 8 || ino0 == 0) return false;
    if (blk[16] != static_cast<uint8_t>('.')) return false;
    const size_t e2 = 16 + ((static_cast<size_t>(ns0) + 7) & ~static_cast<size_t>(7));
    if (e2 + 17 > blen) return false;
    uint32_t typ1 = 0; std::memcpy(&typ1, blk + e2 + type_off, 4);
    uint32_t ns1  = 0; std::memcpy(&ns1,  blk + e2 + 8, 4);
    if (typ1 != DIRENT_TYPE_DOTDOT || ns1 > 8) return false;
    if (blk[e2 + 16] != static_cast<uint8_t>('.') || blk[e2 + 17] != static_cast<uint8_t>('.')) return false;
    if (root_ino) *root_ino = ino0;
    return true;
}

// Parse a whole directory block into (name, inode) pairs (no recursion).
bool ScanReadDir(const uint8_t* blk, size_t blen, bool inode_first,
                 std::vector<std::pair<std::string, uint32_t>>& out) {
    out.clear();
    size_t pos = 0;
    const size_t ino_off  = inode_first ? 0 : 4;
    const size_t type_off = inode_first ? 4 : 0;
    while (pos + 17 <= blen) {
        uint32_t ino = 0, typ = 0, ns = 0;
        std::memcpy(&ino, blk + pos + ino_off,  4);
        std::memcpy(&typ, blk + pos + type_off, 4);
        std::memcpy(&ns,  blk + pos + 8,        4);
        if (typ == 0 || typ > DIRENT_TYPE_DOTDOT) break;
        if (ns == 0 || ns > 255 || pos + 16 + ns > blen) break;
        std::string name(reinterpret_cast<const char*>(blk + pos + 16));
        if (typ == DIRENT_TYPE_DOT || typ == DIRENT_TYPE_DOTDOT) {
            pos += 16 + ((static_cast<size_t>(ns) + 7) & ~static_cast<size_t>(7));
            continue;
        }
        if (ino == 0) break;
        out.emplace_back(name, ino);
        pos += 16 + ((static_cast<size_t>(ns) + 7) & ~static_cast<size_t>(7));
        if (out.size() > 65536) break;
    }
    return !out.empty();
}

std::vector<int64_t> ScanChain(ScanImage& img, const ScanInodeInfo& ino) {
    std::vector<int64_t> out;
    for (int i = 0; i < 12; ++i) {
        if (ino.db[i] <= 0) break;
        if (static_cast<uint64_t>(ino.db[i]) >= img.nblocks) break;
        out.push_back(ino.db[i]);
    }
    const size_t pw = (img.stride == 0x310) ? 8 : 4;
    const size_t per = img.bsize / pw;
    std::vector<uint8_t> blk(img.bsize);
    for (int lvl = 0; lvl < 5; ++lvl) {
        if (ino.ib[lvl] <= 0) break;
        if (static_cast<uint64_t>(ino.ib[lvl]) >= img.nblocks) break;
        if (!ScanRead(img, img.base + static_cast<uint64_t>(ino.ib[lvl]) * img.bsize, img.bsize, blk.data())) break;
        for (size_t j = 0; j < per; ++j) {
            if (pw == 8) {
                if (j * 8 + 8 > img.bsize) break;
                int64_t p = 0; std::memcpy(&p, blk.data() + j * 8, 8);
                if (p <= 0 || static_cast<uint64_t>(p) >= img.nblocks) goto next_level;
                out.push_back(p);
            } else {
                if (j * 4 + 4 > img.bsize) break;
                int32_t p = 0; std::memcpy(&p, blk.data() + j * 4, 4);
                if (p <= 0 || static_cast<uint64_t>(p) >= img.nblocks) goto next_level;
                out.push_back(p);
            }
        }
        next_level:;
    }
    return out;
}

bool ScanRootRefsBlock(ScanImage& img, const ScanInodeInfo& root, uint64_t dir_blk) {
    auto chain = ScanChain(img, root);
    for (int64_t b : chain) if (static_cast<uint64_t>(b) == dir_blk) return true;
    return false;
}

// Step A: find the root directory block by scanning for "." / "..".
bool ScanLocateRoot(ScanImage& img) {
    const uint64_t max_blocks = std::min<uint64_t>(img.nblocks, (img.limit - img.base) / img.bsize);
    std::vector<uint8_t> blk(img.bsize);
    for (uint64_t b = 0; b < max_blocks; ++b) {
        const uint64_t off = img.base + b * img.bsize;
        if (!ScanRead(img, off, 64, blk.data())) { break; } // only need the first entries
        bool order = false;
        uint32_t rino = 0;
        for (int o = 0; o < 2; ++o) {
            const bool inode_first = (o == 0);
            if (ScanProbeRootDir(blk.data(), 64, inode_first, &rino)) { order = inode_first; break; }
        }
        if (rino != 0) {
            img.dir_inode_first = order;
            img.root_ino = rino;
            img.inode_abs.clear(); // table found in step B; dir block index cached in low bits
            img.inode_abs[0] = b; // stash root dir block index under key 0 (no inode 0 exists)
            LOGF("PKG: scan-image root dir at block %llu (inode %u, inode_first=%s)",
                 static_cast<unsigned long long>(b), rino, order ? "yes" : "no");
            return true;
        }
    }
    return false;
}

// Step B: locate the inode table near the superblock (cross-checked vs root dir block).
bool ScanLocateTable(ScanImage& img) {
    const uint64_t dir_blk = img.inode_abs.count(0) ? img.inode_abs[0] : UINT64_MAX;
    const uint64_t sb_blk = (img.sb_abs - img.base) / img.bsize;
    const uint64_t lo = 0;
    const uint64_t hi = img.nblocks - 1;
    (void)sb_blk; // whole-segment scan: NAPS places the table far from the superblock
    const size_t read_len = 32 * 0x310; // enough for 32 S64 or ~118 D32 candidates
    std::vector<uint8_t> buf(read_len);
    for (int s = 0; s < 2; ++s) {
        const int stride = s == 0 ? 0x310 : 0xA8;
        for (uint64_t b = lo; b <= hi; ++b) {
            const uint64_t off = img.base + b * img.bsize;
            if (!ScanRead(img, off, read_len, buf.data())) continue;
            std::map<uint32_t, uint64_t> cand;
            int run = 0;
            for (size_t k = 0; k * stride + stride <= read_len && run < 64; ++k) {
                ScanInodeInfo ino = ScanParseRec(buf.data() + k * stride, stride);
                if (!ScanInodeSane(img, ino)) break;
                cand[ino.number] = off + k * stride;
                ++run;
                if (k > 0 && ino.number != 0) {
                    // numbers in a table run usually increase; tolerate gaps but stop at chaos
                    if (run >= 3) {
                        // fine
                    }
                }
            }
            if (run < 3) continue;
            // cross-check every candidate: find root dir inode referencing dir_blk
            for (const auto& kv : cand) {
                uint8_t rec[0x310];
                if (!ScanRead(img, kv.second, stride, rec)) continue;
                ScanInodeInfo ino = ScanParseRec(rec, stride);
                if ((ino.mode & 0xF000) != INODE_MODE_DIR && ino.mode != 3) continue;
                if (dir_blk != UINT64_MAX && ScanRootRefsBlock(img, ino, dir_blk)) {
                    img.stride = stride;
                    img.inode_abs.clear();
                    img.inode_abs[0] = dir_blk; // keep dir block cache
                    for (const auto& kv2 : cand) img.inode_abs[kv2.first] = kv2.second;
                    LOGF("PKG: scan-image inode table block %llu stride=0x%X (%u inodes)",
                         static_cast<unsigned long long>(b), stride, run);
                    return true;
                }
            }
        }
    }
    return false;
}

bool ScanGetInode(ScanImage& img, uint32_t num, ScanInodeInfo& out) {
    auto it = img.inode_abs.find(num);
    if (it == img.inode_abs.end()) return false;
    std::vector<uint8_t> rec(img.stride);
    if (!ScanRead(img, it->second, img.stride, rec.data())) return false;
    out = ScanParseRec(rec.data(), img.stride);
    return true;
}

uint32_t ScanWriteFile(ScanImage& img, const ScanInodeInfo& ino, const std::string& out_path) {
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) return 0;
    auto chain = ScanChain(img, ino);
    std::vector<uint8_t> blk(img.bsize);
    uint64_t remaining = ino.size;
    for (int64_t b : chain) {
        if (remaining == 0) break;
        const uint64_t off = img.base + static_cast<uint64_t>(b) * img.bsize;
        uint64_t take = std::min<uint64_t>(img.bsize, remaining);
        if (!ScanRead(img, off, take, blk.data())) break;
        out.write(reinterpret_cast<const char*>(blk.data()), static_cast<std::streamsize>(take));
        remaining -= take;
    }
    out.close();
    return static_cast<uint32_t>(remaining == 0 ? 1 : 0);
}

bool ScanSafeName(const std::string& n) {
    if (n.empty() || n == "." || n == "..") return false;
    for (char c : n) if (c == '\\' || c == ':') return false;
    if (n.front() == '/') return false;
    return true;
}

uint32_t ScanWalk(ScanImage& img, uint32_t dir_ino, const std::string& prefix,
                  const std::string& out_dir, int depth) {
    if (depth > 16) return 0;
    ScanInodeInfo dino{};
    if (!ScanGetInode(img, dir_ino, dino)) return 0;
    auto chain = ScanChain(img, dino);
    std::vector<uint8_t> blk(img.bsize);
    uint32_t count = 0;
    for (int64_t b : chain) {
        if (!ScanRead(img, img.base + static_cast<uint64_t>(b) * img.bsize, img.bsize, blk.data())) break;
        std::vector<std::pair<std::string, uint32_t>> entries;
        if (!ScanReadDir(blk.data(), img.bsize, img.dir_inode_first, entries)) continue;
        for (const auto& e : entries) {
            if (!ScanSafeName(e.first)) continue;
            ScanInodeInfo cino{};
            if (!ScanGetInode(img, e.second, cino)) continue;
            const std::string rel = prefix.empty() ? e.first : prefix + "/" + e.first;
            const std::filesystem::path dst = std::filesystem::path(out_dir) / rel;
            if ((cino.mode & 0xF000) == INODE_MODE_DIR) {
                std::error_code ec2;
                std::filesystem::create_directories(dst, ec2);
                count += ScanWalk(img, e.second, rel, out_dir, depth + 1);
            } else if ((cino.mode & 0xF000) == INODE_MODE_FILE) {
                std::error_code ec2;
                std::filesystem::create_directories(dst.parent_path(), ec2);
                if (ScanWriteFile(img, cino, dst.string())) {
                    ++count;
                    LOGF("PKG:  extracted %s (%llu bytes)", rel.c_str(),
                         static_cast<unsigned long long>(cino.size));
                } else {
                    LOGF("PKG:  failed to write %s", rel.c_str());
                }
            }
        }
    }
    return count;
}

// Walk an image whose superblock is at sb_abs inside [base, limit). Extracts
// the whole file tree to out_dir. Returns files written.
uint32_t ScanExtractImage(const std::string& pkg_path, uint64_t base, uint64_t limit,
                          uint64_t sb_abs, const std::string& out_dir) {
    ScanImage img;
    img.base  = base;
    img.limit = limit;
    img.sb_abs = sb_abs;
    img.f.open(pkg_path, std::ios::binary);
    if (!img.f) return 0;

    uint8_t sb[0x380];
    if (!ScanRead(img, sb_abs, sizeof(sb), sb)) return 0;
    if (ReadLe64(sb + 8) != PFS_FORMAT_MAGIC) return 0;
    img.version = static_cast<uint32_t>(ReadLe64(sb));
    img.mode    = static_cast<uint32_t>(sb[0x1C] | (sb[0x1D] << 8));
    img.bsize   = static_cast<uint32_t>(ReadLe64(sb + 0x20));
    img.nblocks = ReadLe64(sb + 0x28);
    img.nblocks = std::max(img.nblocks, ReadLe64(sb + 0x38)); // real span is in num_data_blocks
    img.ninodes = ReadLe64(sb + 0x30);
    if (img.bsize == 0 || img.bsize > 0x100000) return 0;
    if (img.nblocks == 0 || img.nblocks * img.bsize > (img.limit - img.base) * 2) {
        // tolerate over-reported geometry by clamping to readable blocks
        const uint64_t readable = (img.limit - img.base) / img.bsize;
        if (readable == 0) return 0;
        img.nblocks = std::max(img.nblocks, readable);
    }
    LOGF("PKG: scan-image base=0x%llX sb=0x%llX v=%u mode=0x%X bs=%u blocks=%llu",
         static_cast<unsigned long long>(img.base), static_cast<unsigned long long>(img.sb_abs),
         img.version, img.mode, img.bsize, static_cast<unsigned long long>(img.nblocks));

    if (!ScanLocateRoot(img))  { LOGF("PKG: scan-image: root dir block not found"); return 0; }
    if (!ScanLocateTable(img)) { LOGF("PKG: scan-image: inode table not found"); return 0; }

    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    return ScanWalk(img, img.root_ino, "", out_dir, 0);
}

// Scan a region for the PFS superblock magic; returns UINT64_MAX when none.
uint64_t ScanFindSuperblock(ScanImage& img_unused, const std::string& path,
                            uint64_t abs_base, uint64_t abs_limit) {
    std::ifstream sf(path, std::ios::binary);
    if (!sf) return UINT64_MAX;
    const size_t chunk = 8 << 20;
    std::vector<uint8_t> buf(chunk + 16);
    uint64_t pos = abs_base;
    while (pos < abs_limit) {
        const size_t want = static_cast<size_t>(std::min<uint64_t>(abs_limit - pos, chunk));
        sf.clear();
        sf.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
        sf.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(want));
        const size_t got = static_cast<size_t>(sf.gcount());
        if (got < 16) break;
        for (size_t i = 0; i + 16 <= got; ++i) {
            if (buf[i + 8] == 0x0B && buf[i + 9] == 0x2A && buf[i + 10] == 0x33 && buf[i + 11] == 0x01) {
                const uint64_t v = ReadLe64(buf.data() + i);
                if (v == 1 || v == 2 || v == 3) return pos + i;
            }
        }
        pos += got - 15;
    }
    return UINT64_MAX;
}

} // namespace


// Content-carving fallback used when the NAPS outer PFS table cannot be decoded.
// Finds the game payload directly by content signatures inside the segment:
//   eboot.bin   - first/best 64-bit ELF (span computed from program headers)
//   param.json  - { ... "contentId"/"titleId" ... } JSON blob
//   icon0.png   - PNG (carved to its IEND terminator)
// Produces a minimal, runnable game folder so the install actually succeeds.
namespace {

uint64_t CarveCopy(std::ifstream& f, uint64_t abs_off, uint64_t n, const std::string& out_path) {
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) return 0;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(out_path).parent_path(), ec);
    std::vector<uint8_t> buf(1 << 20);
    uint64_t remain = n, total = 0;
    while (remain > 0) {
        const size_t take = static_cast<size_t>(std::min<uint64_t>(remain, buf.size()));
        f.clear();
        f.seekg(static_cast<std::streamoff>(abs_off + total), std::ios::beg);
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(take));
        const size_t got = static_cast<size_t>(f.gcount());
        if (!got) break;
        out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(got));
        remain -= got; total += got;
    }
    out.close();
    return total;
}

// Exact byte span of a 64-bit ELF starting at hdr (>= 64 bytes read).
uint64_t ElfSpan(const uint8_t* h, size_t have) {
    if (have < 0x40) return 0;
    if (!(h[0]==0x7F && h[1]==0x45 && h[2]==0x4C && h[3]==0x46)) return 0;
    if (h[4] != 2) return 0; // 64-bit only (PS5)
    uint64_t phoff = 0; std::memcpy(&phoff, h + 0x20, 8);
    uint16_t phentsize = 0; std::memcpy(&phentsize, h + 0x36, 2);
    uint16_t phnum = 0;     std::memcpy(&phnum,     h + 0x38, 2);
    if (phoff == 0 || phoff > 0x100000 || phentsize < 0x38 || phnum == 0 || phnum > 0x400) return 0;
    if (phoff + static_cast<uint64_t>(phentsize) * phnum > 0x400000) return 0;
    return phoff + static_cast<uint64_t>(phentsize) * phnum; // header part; data extent computed below
}

// Full extent including program-header payloads. Reads program headers from file.
uint64_t ElfFullSpan(std::ifstream& f, uint64_t abs_off, const uint8_t* hdr) {
    uint64_t phoff = 0; std::memcpy(&phoff, hdr + 0x20, 8);
    uint16_t phentsize = 0; std::memcpy(&phentsize, hdr + 0x36, 2);
    uint16_t phnum = 0; std::memcpy(&phnum, hdr + 0x38, 2);
    uint64_t header_end = phoff + static_cast<uint64_t>(phentsize) * phnum;
    uint64_t extent = header_end;
    std::vector<uint8_t> phbuf(static_cast<size_t>(header_end - phoff));
    f.clear();
    f.seekg(static_cast<std::streamoff>(abs_off + phoff), std::ios::beg);
    f.read(reinterpret_cast<char*>(phbuf.data()), static_cast<std::streamsize>(phbuf.size()));
    if (static_cast<size_t>(f.gcount()) < phbuf.size()) return header_end;
    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t* ph = phbuf.data() + i * phentsize;
        uint64_t p_offset = 0, p_filesz = 0;
        std::memcpy(&p_offset, ph + 0x08, 8);
        std::memcpy(&p_filesz, ph + 0x20, 8);
        if (p_filesz > 0 && p_offset + p_filesz > extent && p_offset < (1ull << 40))
            extent = p_offset + p_filesz;
    }
    return extent;
}

uint32_t ScanCarveGame(const std::string& pkg_path, uint64_t seg_base, uint64_t seg_end,
                       const std::string& output_dir) {
    std::ifstream f(pkg_path, std::ios::binary);
    if (!f) return 0;
    const uint64_t kChunk = 16 << 20;
    std::vector<uint8_t> buf(static_cast<size_t>(kChunk) + 64);
    uint64_t pos = seg_base;

    uint64_t elf_off = UINT64_MAX, elf_span = 0;
    uint64_t json_off = UINT64_MAX, json_len = 0;
    uint64_t png_off = UINT64_MAX, png_end = UINT64_MAX;

    while (pos < seg_end) {
        const size_t want = static_cast<size_t>(std::min<uint64_t>(seg_end - pos, kChunk));
        f.clear();
        f.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(want + 64));
        const size_t got = static_cast<size_t>(f.gcount());
        if (got < 16) break;
        for (size_t i = 0; i + 16 <= got; ++i) {
            // ELF (first sane one wins; eboot is the only 64-bit ELF in the payload area)
            if (buf[i]==0x7F && buf[i+1]==0x45 && buf[i+2]==0x4C && buf[i+3]==0x46 && buf[i+4]==0x02) {
                const uint64_t span = ElfSpan(buf.data() + i, got - i);
                if (span != 0 && span < (1ull << 34) && span > elf_span) { elf_off = pos + i; elf_span = span; }
            }
            // param.json: '{' ... "contentId" nearby
            if (json_off == UINT64_MAX && buf[i] == static_cast<uint8_t>('{')) {
                for (size_t j = i + 1; j + 10 <= got && j < i + 256; ++j) {
                    if (buf[j] == static_cast<uint8_t>('"')) {
                        if (got - j >= 10 && std::memcmp(buf.data() + j, "\"contentId\"", 10) == 0) {
                            // find matching close brace (naive depth count, json here is flat/small)
                            int depth = 0; size_t end = 0;
                            for (size_t k = i; k < got && k < i + 0x2000; ++k) {
                                if (buf[k] == '{') ++depth;
                                else if (buf[k] == '}') { if (--depth == 0) { end = k + 1; break; } }
                            }
                            if (end > i) { json_off = pos + i; json_len = end - i; }
                            break;
                        }
                    }
                }
            }
            // PNG: carve from signature to IEND
            if (png_off == UINT64_MAX &&
                buf[i]==0x89 && buf[i+1]==0x50 && buf[i+2]==0x4E && buf[i+3]==0x47 &&
                buf[i+4]==0x0D && buf[i+5]==0x0A && buf[i+6]==0x1A && buf[i+7]==0x0A) {
                png_off = pos + i;
                // IEND marker: 49 45 4E 44 AE 42 60 82
                for (size_t j = i + 8; j + 8 <= got && j < i + 0x10000; ++j) {
                    if (buf[j]==0x49 && buf[j+1]==0x45 && buf[j+2]==0x4E && buf[j+3]==0x44 &&
                        buf[j+4]==0xAE && buf[j+5]==0x42 && buf[j+6]==0x60 && buf[j+7]==0x82) {
                        png_end = pos + j + 8; break;
                    }
                }
                if (png_end == UINT64_MAX) png_off = UINT64_MAX; // incomplete -> treat as none
            }
            if (json_off != UINT64_MAX && png_off != UINT64_MAX) { /* keep scanning for a larger ELF */ }
        }
        pos += got - 64;
    }

    uint32_t n = 0;
    const std::filesystem::path root = std::filesystem::path(output_dir) / "pfs_files";
    std::error_code ec;
    std::filesystem::create_directories(root / "sce_sys", ec);

    if (elf_off != UINT64_MAX) {
        const uint64_t full = ElfFullSpan(f, elf_off, [&]{ uint8_t h[0x40]; f.clear(); f.seekg(static_cast<std::streamoff>(elf_off), std::ios::beg); f.read(reinterpret_cast<char*>(h), sizeof(h)); return h; }()) ;
        const uint64_t capped = std::min<uint64_t>(full, 512ull << 20);
        if (CarveCopy(f, elf_off, capped, (root / "eboot.bin").string()) == capped) { ++n; LOGF("PKG: carved eboot.bin (%llu bytes @0x%llX)", (unsigned long long)capped, (unsigned long long)elf_off); }
    }
    if (json_off != UINT64_MAX && json_len > 0 && json_len < (1 << 20)) {
        if (CarveCopy(f, json_off, json_len, (root / "sce_sys" / "param.json").string()) == json_len) { ++n; LOGF("PKG: carved sce_sys/param.json (%llu bytes)", (unsigned long long)json_len); }
    }
    if (png_off != UINT64_MAX && png_end > png_off && png_end - png_off < (64ull << 20)) {
        if (CarveCopy(f, png_off, png_end - png_off, (root / "sce_sys" / "icon0.png").string()) == png_end - png_off) { ++n; LOGF("PKG: carved sce_sys/icon0.png (%llu bytes)", (unsigned long long)(png_end - png_off)); }
    }
    LOGF("PKG: carve fallback extracted %u file(s)", n);
    return n;
}

} // namespace


uint32_t PkgParser::ExtractDataFirst(const std::string& pkg_path,
                                      uint64_t pfs_segment_offset,
                                      uint64_t pfs_segment_size,
                                      uint64_t superblock_offset,
                                      const std::string& output_dir) {
    LOGF("PKG: data-first extraction (seg=0x%llX+%llu, sb=0x%llX)",
         static_cast<unsigned long long>(pfs_segment_offset),
         static_cast<unsigned long long>(pfs_segment_size),
         static_cast<unsigned long long>(superblock_offset));

    const uint64_t seg_end = pfs_segment_offset + pfs_segment_size;
    std::error_code fsec;
    const uint64_t carve_end = std::max(pfs_segment_offset + pfs_segment_size, std::filesystem::file_size(pkg_path, fsec));

    // ---- outer walk: find pfs_image.dat blocks without writing intermediate copies ----
    ScanImage outer;
    outer.base  = pfs_segment_offset;
    outer.limit = seg_end;
    outer.sb_abs = superblock_offset;
    outer.f.open(pkg_path, std::ios::binary);
    if (!outer.f) { LOGF("PKG: cannot open %s", pkg_path.c_str()); return 0; }

    {
        uint8_t sb[0x380];
        if (!ScanRead(outer, superblock_offset, sizeof(sb), sb)) return 0;
        if (ReadLe64(sb + 8) != PFS_FORMAT_MAGIC) return 0;
        outer.version = static_cast<uint32_t>(ReadLe64(sb));
        outer.mode    = static_cast<uint32_t>(sb[0x1C] | (sb[0x1D] << 8));
        outer.bsize   = static_cast<uint32_t>(ReadLe64(sb + 0x20));
        outer.nblocks = ReadLe64(sb + 0x28);
        outer.nblocks = std::max(outer.nblocks, ReadLe64(sb + 0x38));
        outer.ninodes = ReadLe64(sb + 0x30);
        if (outer.bsize == 0 || outer.bsize > 0x100000) return 0;
        const uint64_t readable = pfs_segment_size / outer.bsize;
        if (outer.nblocks == 0 || outer.nblocks > readable) outer.nblocks = readable;
        if (outer.nblocks == 0) return 0;
        const bool plain = std::memcmp(sb + 0x370, "PPRPLAIN-NOAUTH!", 16) == 0;
        if ((outer.mode & PFS_MODE_ENCRYPTED) != 0 && !plain) {
            LOGF("PKG: outer PFS encrypted and not plaintext-patched");
            return 0;
        }
    }

    if (!ScanLocateRoot(outer)) { LOGF("PKG: outer root dir not found - carving content"); return ScanCarveGame(pkg_path, pfs_segment_offset, carve_end, output_dir); }
    if (!ScanLocateTable(outer)) { LOGF("PKG: outer inode table not found - carving content"); return ScanCarveGame(pkg_path, pfs_segment_offset, carve_end, output_dir); }

    // Read the outer root directory and find pfs_image.dat.
    ScanInodeInfo root{};
    if (!ScanGetInode(outer, outer.root_ino, root)) return 0;
    auto rchain = ScanChain(outer, root);
    std::vector<uint8_t> blk(outer.bsize);
    std::vector<std::pair<std::string, uint32_t>> entries;
    for (int64_t b : rchain) {
        if (!ScanRead(outer, outer.base + static_cast<uint64_t>(b) * outer.bsize, outer.bsize, blk.data())) break;
        std::vector<std::pair<std::string, uint32_t>> part;
        if (ScanReadDir(blk.data(), outer.bsize, outer.dir_inode_first, part)) {
            entries.insert(entries.end(), part.begin(), part.end());
        }
    }
    if (entries.empty()) { LOGF("PKG: outer root directory is unreadable"); return 0; }

    uint32_t pfs_image_ino = 0;
    for (const auto& e : entries) {
        LOGF("PKG: outer entry: %s (inode %u)", e.first.c_str(), e.second);
        if (e.first == "pfs_image.dat") pfs_image_ino = e.second;
    }
    if (pfs_image_ino == 0) {
        // No inner image: treat the outer tree as the game tree itself.
        LOGF("PKG: no pfs_image.dat; extracting outer tree directly");
        const std::string out = (std::filesystem::path(output_dir) / "pfs_files").string();
        std::error_code ec2;
        std::filesystem::create_directories(out, ec2);
        return ScanWalk(outer, outer.root_ino, "", out, 0);
    }

    ScanInodeInfo inner_file{};
    if (!ScanGetInode(outer, pfs_image_ino, inner_file)) return 0;
    auto ichain = ScanChain(outer, inner_file);
    if (ichain.empty()) { LOGF("PKG: pfs_image.dat has no data blocks"); return 0; }

    // If the chain is contiguous, parse the inner image in place (no 1GB spill).
    bool contiguous = true;
    for (size_t i = 1; i < ichain.size(); ++i) {
        if (ichain[i] != ichain[i - 1] + 1) { contiguous = false; break; }
    }

    uint64_t inner_base = 0, inner_limit = 0;
    std::string spill_path;
    if (contiguous) {
        inner_base  = outer.base + static_cast<uint64_t>(ichain.front()) * outer.bsize;
        inner_limit = inner_base + inner_file.size * (inner_file.size >= outer.bsize ? 1 : 1);
        inner_limit = inner_base + inner_file.size;
    } else {
        spill_path = (std::filesystem::path(output_dir) / "_naps_inner.pfs").string();
        std::error_code ec2;
        std::filesystem::create_directories(output_dir, ec2);
        std::ofstream out(spill_path, std::ios::binary | std::ios::trunc);
        if (!out) return 0;
        std::vector<uint8_t> copybuf(outer.bsize);
        uint64_t remain = inner_file.size;
        for (int64_t b : ichain) {
            const uint64_t off = outer.base + static_cast<uint64_t>(b) * outer.bsize;
            const uint64_t take = std::min<uint64_t>(outer.bsize, remain);
            if (!ScanRead(outer, off, take, copybuf.data())) break;
            out.write(reinterpret_cast<const char*>(copybuf.data()), static_cast<std::streamsize>(take));
            remain -= take;
        }
        out.close();
        if (remain > 0) { LOGF("PKG: pfs_image.dat spill truncated"); return 0; }
        inner_base = 0;
        inner_limit = inner_file.size;
        LOGF("PKG: spilled inner image to %s (%llu bytes)", spill_path.c_str(),
             static_cast<unsigned long long>(inner_file.size));
    }

    // Find the inner superblock (it may sit anywhere, including near the end).
    ScanImage dummy{};
    const uint64_t inner_sb = ScanFindSuperblock(dummy, contiguous ? pkg_path : spill_path,
                                                 inner_base, inner_limit);
    if (inner_sb == UINT64_MAX) {
        LOGF("PKG: inner image has no PFS superblock - carving content");
        return ScanCarveGame(pkg_path, pfs_segment_offset, carve_end, output_dir);
    }
    LOGF("PKG: inner superblock at 0x%llX", static_cast<unsigned long long>(inner_sb));

    const std::string out = (std::filesystem::path(output_dir) / "pfs_files").string();
    std::error_code ec3;
    std::filesystem::create_directories(out, ec3);

    // Recursive reuse: parse the inner image with the same scanner.
    const std::string& inner_path = contiguous ? pkg_path : spill_path;
    const uint32_t n = ScanExtractImage(inner_path, inner_base, inner_limit, inner_sb, out);

    if (!spill_path.empty() && n > 0) {
        std::error_code ec4;
        std::filesystem::remove(spill_path, ec4);
    }
    LOGF("PKG: data-first extraction complete, %u file(s) to %s", n, out.c_str());
    if (n == 0) return ScanCarveGame(pkg_path, pfs_segment_offset, carve_end, output_dir);
    return n;
}


// ---- Extraction ----

uint32_t PkgParser::ExtractAll(const PkgParseResult& result,
                                 const std::string& pkg_path,
                                 const std::string& output_dir) {
    if (!result.ok) {
        LOGF("PKG: cannot extract - parse failed: %s", result.error.c_str());
        return 0;
    }

    if (result.is_encrypted) {
        LOGF("PKG: cannot extract - body is encrypted (decryption not supported)");
        return 0;
    }

    if (!result.extraction_error.empty()) {
        LOGF("PKG: cannot extract - %s", result.extraction_error.c_str());
        return 0;
    }

    // PS5 data-first / NAPS layout: shared superblock not at segment start
    // -> outer blob-dump path cannot work; use ExtractDataFirst directly.
    if (result.is_finalized_image && result.pfs_image_offset != result.pfs_segment_offset) {
        return ExtractDataFirst(pkg_path, result.pfs_segment_offset,
                                 result.pfs_segment_size, result.pfs_image_offset, output_dir);
    }

    if (!result.is_finalized_image && result.body_size == 0) {
        LOGF("PKG: cannot extract - body is empty, nothing to extract");
        return 0;
    }

    // Create output directory
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        LOGF("PKG: failed to create output directory: %s", output_dir.c_str());
        return 0;
    }

    std::ifstream f(pkg_path, std::ios::binary);
    if (!f) {
        LOGF("PKG: cannot open for extraction: %s", pkg_path.c_str());
        return 0;
    }

    uint32_t extracted = 0;

    // The PFS image is not guaranteed to sit at body_offset — it commonly begins
    // at body_offset + body_size. Use the superblock offset located during Parse().
    uint64_t pfs_start = result.pfs_image_offset;
    if (pfs_start == 0 || pfs_start >= std::filesystem::file_size(pkg_path)) {
        // Fallback: assume the PFS image is the whole body region.
        pfs_start = result.body_offset;
    }
    const uint64_t pfs_end = result.is_finalized_image ?
                            result.pfs_segment_offset + result.pfs_segment_size :
                            std::filesystem::file_size(pkg_path);
    if (pfs_start >= pfs_end) {
        LOGF("PKG: PFS image bounds invalid (start=0x%llX)",
             static_cast<unsigned long long>(pfs_start));
        return 0;
    }

    f.seekg(static_cast<std::streamoff>(pfs_start), std::ios::beg);

    const std::filesystem::path out_pfs = std::filesystem::path(output_dir) / "body.pfs";
    std::ofstream out(out_pfs, std::ios::binary);
    if (!out) {
        LOGF("PKG: cannot create %s", out_pfs.string().c_str());
        return 0;
    }

    constexpr size_t kBufSize = 1 << 20; // 1 MB
    std::vector<uint8_t> buf(kBufSize);
    uint64_t remaining = pfs_end - pfs_start;
    while (remaining > 0) {
        const size_t to_read = static_cast<size_t>(std::min<uint64_t>(remaining, kBufSize));
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(to_read));
        const auto got = static_cast<size_t>(f.gcount());
        if (got == 0) break;
        out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(got));
        remaining -= got;
    }
    out.close();
    f.close();

    LOGF("PKG: extracted body PFS image to %s (%llu bytes @0x%llX)",
         out_pfs.string().c_str(), static_cast<unsigned long long>(pfs_end - pfs_start),
         static_cast<unsigned long long>(pfs_start));


    // Parse the extracted PFS image and extract individual files
    const std::string pfs_out_dir = std::filesystem::path(output_dir).string() + "/pfs_files";
    auto pfs_result = PfsParser::Parse(out_pfs.string());
    if (pfs_result.ok && !pfs_result.is_encrypted && !pfs_result.is_compressed) {
        const uint32_t pfs_extracted = PfsParser::ExtractAll(pfs_result, out_pfs.string(), pfs_out_dir);
        LOGF("PKG: PFS parser extracted %u individual file(s) to %s",
             pfs_extracted, pfs_out_dir.c_str());
        extracted = pfs_extracted;
    } else if (pfs_result.ok && pfs_result.is_encrypted) {
        // KytyPlus: refuse instead of pretending success. Returning 0 makes the
        // caller report failure, so the launcher shows a real reason rather than
        // copying a raw ciphertext blob into the library as a "game".
        LOGF("PKG: PFS body is encrypted (decryption not supported) - install refused");
        return 0;
    } else if (result.is_finalized_image) {
        LOGF("PKG: PS5 FIH filesystem extraction failed: %s", pfs_result.error.c_str());
        return 0; // a raw blob is not an installed PS5 game
    } else if (pfs_result.ok && pfs_result.is_compressed) {
        LOGF("PKG: PFS body is PFSC-compressed - extracted raw body.pfs only");
        extracted = 1;
    } else {
        LOGF("PKG: PFS parse failed (%s) - extracted raw body.pfs only", pfs_result.error.c_str());
        extracted = 1;
    }

    return extracted;
}

} // namespace Libs::Firmware