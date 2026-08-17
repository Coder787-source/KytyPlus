#include "firmware/pkgParser.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>

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

bool PkgParser::IsEncrypted(const std::string& pkg_path, uint32_t body_offset) {
    std::ifstream f(pkg_path, std::ios::binary);
    if (!f) return true; // assume encrypted if unreadable

    f.seekg(static_cast<std::streamoff>(body_offset), std::ios::beg);
    uint8_t magic[4] = {0};
    f.read(reinterpret_cast<char*>(magic), 4);
    if (f.gcount() < 4) return true;

    // PFS magic "PFS\0" indicates decrypted/plaintext body
    return !(magic[0] == 0x50 && magic[1] == 0x46 && magic[2] == 0x53 && magic[3] == 0x00);
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

// ---- Main parse ----

PkgParseResult PkgParser::Parse(const std::string& pkg_path) {
    PkgParseResult result;
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

    if (!ValidatePkgHeader(hdr, file_size)) {
        // Log the raw magic for debugging
        char magic_str[5] = {};
        std::memcpy(magic_str, &hdr.magic, 4);
        result.error = std::string("Invalid PKG magic: 0x") +
                       (magic_str[0] < 16 ? "0" : "") +
                       std::to_string(static_cast<int>(static_cast<unsigned char>(magic_str[0])));
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

    if (table_offset > 0 && table_offset < file_size && table_entries > 0 && table_entries < 4096) {
        f.seekg(static_cast<std::streamoff>(table_offset), std::ios::beg);

        // The file table format varies; the common PS4 layout is:
        // - A 32-bit offset to a name table (at fixed offset 0x2B30 for CNT-type)
        // - File entries with name, offset, size
        // For robustness, we read the name table pointer and parse names from there.

        // Read the name table pointer (common at offset 0x2B30 for CNT PKGs)
        f.seekg(0x2B30, std::ios::beg);
        uint32_t name_table_off = 0;
        f.read(reinterpret_cast<char*>(&name_table_off), 4);
        name_table_off = Be32(name_table_off);

        if (name_table_off > 0 && name_table_off < file_size) {
            // Read name table (null-separated names)
            f.seekg(static_cast<std::streamoff>(name_table_off), std::ios::beg);
            std::vector<char> names(8192, 0);
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
        }
    }

    // Detect encryption (check for PFS magic at body_offset)
    result.is_encrypted = IsEncrypted(pkg_path, result.body_offset);

    if (result.is_encrypted) {
        LOGF("PKG: body is encrypted (no PFS magic at offset 0x%X) - requires user keys to extract",
             result.body_offset);
    } else {
        LOGF("PKG: body is decrypted (PFS magic found) - can extract without keys");
    }

    result.ok = true;
    return result;
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
        LOGF("PKG: cannot extract - body is encrypted, requires user-supplied keys.bin");
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

    // For a decrypted PKG, the body is a PFS image. A full PFS parser would
    // walk the filesystem. For now, we extract the known manifest files by
    // scanning the body for file signatures. This is a best-effort extraction
    // that handles the common case (param.sfo, pic0.png, etc.).
    //
    // NOTE: Full PFS extraction requires implementing the PFS block format
    // (see PSDevWiki PFS). This stub extracts the raw body to a .pfs file
    // so external tools or a future PFS parser can process it.

    if (result.body_size == 0) {
        // Fallback: use file_size - body_offset
        const uint64_t fsize = std::filesystem::file_size(pkg_path);
        const uint32_t bsize = static_cast<uint32_t>(fsize - result.body_offset);
        const_cast<PkgParseResult&>(result).body_size = bsize;
    }

    f.seekg(static_cast<std::streamoff>(result.body_offset), std::ios::beg);

    const std::filesystem::path out_pfs = std::filesystem::path(output_dir) / "body.pfs";
    std::ofstream out(out_pfs, std::ios::binary);
    if (!out) {
        LOGF("PKG: cannot create %s", out_pfs.string().c_str());
        return 0;
    }

    // Copy body to output file
    constexpr size_t kBufSize = 1 << 20; // 1 MB
    std::vector<uint8_t> buf(kBufSize);
    uint64_t remaining = result.body_size;
    while (remaining > 0) {
        const size_t to_read = static_cast<size_t>(std::min<uint64_t>(remaining, kBufSize));
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(to_read));
        const auto got = static_cast<size_t>(f.gcount());
        if (got == 0) break;
        out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(got));
        remaining -= got;
    }

    out.close();
    extracted = 1; // the body.pfs file

    LOGF("PKG: extracted body PFS image to %s (%u bytes)",
         out_pfs.string().c_str(), result.body_size);

    return extracted;
}

} // namespace Libs::Firmware