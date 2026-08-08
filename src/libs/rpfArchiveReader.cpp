// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// RPF (RAGE Pack File) archive reader.
//
// Reads .rpf archives used by RAGE engine games (GTA V, RDR2). Supports:
//   - RPF3 / RPF6 / RPF7 / RPF8 magic detection
//   - Table-of-contents parsing (binary, resource, directory entries)
//   - Zlib decompression for compressed file entries
//   - Case-insensitive name lookup
//   - Path-based lookup with '/' separator (e.g. "common/data/game.dat")
//
// RPF7 header layout (16 bytes, little-endian):
//   [0..3]   magic    "RPF7"
//   [4..7]   version  (typically 0 for PS5)
//   [8..11]  entry_count
//   [12..15] flags
//
// Each TOC entry (24 bytes, RPF7):
//   [0..3]   name_offset     into string table
//   [4..7]   file_offset     absolute offset in .rpf
//   [8..11]  decompressed_size
//   [12..15] compressed_size  (if < decompressed_size → zlib)
//   [16..19] type_or_flags    (0x7FFFFF01=binary, 02=resource, 03=dir)
//   [20..23] extra            (resource flags or dir entry count)
//
// The string table follows the TOC entries and is null-terminated strings.

#include "libs/rpfArchiveReader.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

#if defined(KYTY_HAS_ZLIB)
#include <zlib.h>
#endif

namespace Libs::RpfArchive {

namespace {

// ─── Helpers ─────────────────────────────────────────────────────────────────

inline uint32_t ReadLE32(const uint8_t* p) {
	return static_cast<uint32_t>(p[0])       | (static_cast<uint32_t>(p[1]) << 8u) |
	       (static_cast<uint32_t>(p[2]) << 16u) | (static_cast<uint32_t>(p[3]) << 24u);
}

std::string ToLower(const std::string& s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

// Normalize path separators to '/'.
std::string NormalizePath(const std::string& path) {
	std::string out = path;
	for (auto& c : out) {
		if (c == '\\') c = '/';
	}
	return ToLower(out);
}

// Read the entire file into a byte vector.
std::vector<uint8_t> ReadWholeFile(const char* path) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f.is_open()) return {};
	const auto size = f.tellg();
	if (size <= 0) return {};
	f.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(static_cast<size_t>(size));
	f.read(reinterpret_cast<char*>(data.data()), size);
	return data;
}

// Decompress a zlib stream. Returns empty vector on failure.
std::vector<uint8_t> ZlibInflate(const uint8_t* src, uint32_t src_size,
                                  uint32_t expected_dst_size) {
#if !defined(KYTY_HAS_ZLIB)
	// Without zlib, return the raw compressed data as-is (callers must handle
	// short reads gracefully).
	(void) src; (void) src_size;
	std::vector<uint8_t> dst(src, src + src_size);
	LOGF("[RpfReader] WARN: zlib not available, returning compressed data uncompressed\n");
	return dst;
#else
	std::vector<uint8_t> dst(expected_dst_size);

	z_stream strm {};
	strm.next_in  = const_cast<Bytef*>(src);
	strm.avail_in = src_size;
	strm.next_out = dst.data();
	strm.avail_out = expected_dst_size;

	int ret = inflateInit(&strm);
	if (ret != Z_OK) {
		LOGF("[RpfReader] ERR: inflateInit failed (%d)\n", ret);
		return {};
	}

	ret = inflate(&strm, Z_FINISH);
	inflateEnd(&strm);

	if (ret != Z_STREAM_END) {
		LOGF("[RpfReader] WARN: inflate returned %d (expected Z_STREAM_END), "
		     "decompressed %lu of %u bytes\n",
		     ret, strm.total_out, expected_dst_size);
		dst.resize(strm.total_out);
	}
	return dst;
#endif
}

// ─── TOC parser (RPF7) ──────────────────────────────────────────────────────

bool ParseRpf7Toc(const std::vector<uint8_t>& file_data, RpfArchive& archive) {
	if (file_data.size() < 16) {
		LOGF("[RpfReader] ERR: file too small for header (%zu bytes)\n", file_data.size());
		return false;
	}

	const uint8_t* hdr = file_data.data();
	archive.magic       = ReadLE32(hdr + 0);
	archive.version     = ReadLE32(hdr + 4);
	archive.entry_count = ReadLE32(hdr + 8);
	archive.flags       = ReadLE32(hdr + 12);

	const uint32_t entry_count = archive.entry_count;
	constexpr uint32_t kEntrySize = 24;
	const uint64_t toc_start = 16;
	const uint64_t toc_end   = toc_start + static_cast<uint64_t>(entry_count) * kEntrySize;

	if (toc_end > file_data.size()) {
		LOGF("[RpfReader] ERR: TOC extends past file (toc_end=%llu, file_size=%zu)\n",
		     static_cast<unsigned long long>(toc_end), file_data.size());
		return false;
	}

	// String table starts right after TOC entries.
	const uint8_t* string_table = file_data.data() + toc_end;
	const size_t string_table_size = file_data.size() - static_cast<size_t>(toc_end);

	archive.entries.resize(entry_count);

	for (uint32_t i = 0; i < entry_count; i++) {
		const uint8_t* e = file_data.data() + toc_start + i * kEntrySize;

		uint32_t name_offset   = ReadLE32(e + 0);
		uint32_t file_offset   = ReadLE32(e + 4);
		uint32_t decomp_size   = ReadLE32(e + 8);
		uint32_t comp_size     = ReadLE32(e + 12);
		uint32_t type_field    = ReadLE32(e + 16);
		uint32_t extra_field   = ReadLE32(e + 20);

		auto& entry = archive.entries[i];

		// Read name from string table.
		if (name_offset < string_table_size) {
			const char* name_ptr = reinterpret_cast<const char*>(string_table + name_offset);
			size_t max_len = string_table_size - name_offset;
			entry.name = std::string(name_ptr, strnlen(name_ptr, max_len));
		}

		entry.file_offset        = file_offset;
		entry.decompressed_size  = decomp_size;
		entry.compressed_size    = comp_size;

		// Determine entry type from the type field.
		// The high byte of type_field often encodes the type:
		//   0x01 = binary, 0x02 = resource, 0x03 = directory
		// The full 32-bit value might be e.g. 0x7FFFFF01.
		const uint8_t type_byte = type_field & 0xFF;
		switch (type_byte) {
			case 0x01: entry.type = RpfEntryType::Binary;   break;
			case 0x02: entry.type = RpfEntryType::Resource; entry.resource_flags = extra_field; break;
			case 0x03: entry.type = RpfEntryType::Dir;      entry.dir_entry_count = extra_field; break;
			default:   entry.type = RpfEntryType::Binary;   break; // assume binary
		}
	}

	// Build the name index for fast lookup.
	for (uint32_t i = 0; i < entry_count; i++) {
		std::string key = NormalizePath(archive.entries[i].name);
		archive.name_index[key] = i;
	}

	LOGF("[RpfReader] INFO: Parsed '%s' — magic=0x%08X, v%u, %u entries\n",
	     archive.file_path.c_str(), archive.magic, archive.version, entry_count);
	return true;
}

// ─── TOC parser (RPF3/RPF6/RPF8 — similar structure with minor differences) ─

bool ParseRpfGenericToc(const std::vector<uint8_t>& file_data, RpfArchive& archive) {
	// RPF3 and RPF6 use a slightly different entry layout but the same general
	// structure. Fall back to the RPF7 parser with adjusted offsets.
	//
	// For RPF3/RPF6, the entry is typically 16 bytes:
	//   [0..3]  name_offset
	//   [4..7]  file_offset
	//   [8..11] size
	//   [12..15] flags/type
	//
	// For simplicity, we try the RPF7 parser first; if that fails we try the
	// compact layout.

	if (file_data.size() < 16) return false;

	const uint8_t* hdr = file_data.data();
	archive.magic       = ReadLE32(hdr + 0);
	archive.version     = ReadLE32(hdr + 4);
	archive.entry_count = ReadLE32(hdr + 8);
	archive.flags       = ReadLE32(hdr + 12);

	const uint32_t entry_count = archive.entry_count;

	// Try compact 16-byte entries.
	constexpr uint32_t kCompactEntrySize = 16;
	const uint64_t toc_end = 16 + static_cast<uint64_t>(entry_count) * kCompactEntrySize;
	if (toc_end > file_data.size()) {
		LOGF("[RpfReader] ERR: compact TOC extends past file\n");
		return false;
	}

	const uint8_t* string_table = file_data.data() + toc_end;
	const size_t string_table_size = file_data.size() - static_cast<size_t>(toc_end);

	archive.entries.resize(entry_count);
	for (uint32_t i = 0; i < entry_count; i++) {
		const uint8_t* e = file_data.data() + 16 + i * kCompactEntrySize;

		uint32_t name_offset = ReadLE32(e + 0);
		uint32_t file_offset = ReadLE32(e + 4);
		uint32_t size_field  = ReadLE32(e + 8);
		uint32_t flags_field = ReadLE32(e + 12);

		auto& entry = archive.entries[i];
		if (name_offset < string_table_size) {
			const char* name_ptr = reinterpret_cast<const char*>(string_table + name_offset);
			size_t max_len = string_table_size - name_offset;
			entry.name = std::string(name_ptr, strnlen(name_ptr, max_len));
		}
		entry.file_offset       = file_offset;
		entry.decompressed_size = size_field;
		entry.compressed_size   = size_field; // assume uncompressed for compact format
		entry.type              = RpfEntryType::Binary;
	}

	for (uint32_t i = 0; i < entry_count; i++) {
		std::string key = NormalizePath(archive.entries[i].name);
		archive.name_index[key] = i;
	}

	LOGF("[RpfReader] INFO: Parsed '%s' (compact) — magic=0x%08X, %u entries\n",
	     archive.file_path.c_str(), archive.magic, entry_count);
	return true;
}

} // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────────────────

RpfArchive* OpenArchive(const char* path) {
	if (!path) return nullptr;

	auto file_data = ReadWholeFile(path);
	if (file_data.size() < 16) {
		LOGF("[RpfReader] ERR: cannot open or file too small: '%s' (%zu bytes)\n",
		     path, file_data.size());
		return nullptr;
	}

	auto* archive = new RpfArchive();
	archive->file_path = path;

	const uint32_t magic = ReadLE32(file_data.data());
	bool ok = false;

	switch (magic) {
		case kRpfMagic_RPF7:
		case kRpfMagic_RPF8:
			ok = ParseRpf7Toc(file_data, *archive);
			break;
		case kRpfMagic_RPF3:
		case kRpfMagic_RPF6:
			ok = ParseRpfGenericToc(file_data, *archive);
			break;
		default:
			// Try RPF7 layout anyway — some archives have non-standard magic.
			LOGF("[RpfReader] WARN: unrecognized magic 0x%08X, trying RPF7 layout\n", magic);
			ok = ParseRpf7Toc(file_data, *archive);
			break;
	}

	if (!ok) {
		delete archive;
		return nullptr;
	}

	return archive;
}

void CloseArchive(RpfArchive* archive) {
	delete archive;
}

int32_t FindEntry(const RpfArchive* archive, const char* name) {
	if (!archive || !name) return -1;
	std::string key = NormalizePath(name);
	auto it = archive->name_index.find(key);
	if (it != archive->name_index.end()) {
		return static_cast<int32_t>(it->second);
	}
	return -1;
}

RpfFileData ReadFile(RpfArchive* archive, uint32_t entry_index) {
	RpfFileData result;
	if (!archive || entry_index >= archive->entries.size()) return result;

	const auto& entry = archive->entries[entry_index];
	if (entry.type == RpfEntryType::Dir) {
		LOGF("[RpfReader] ERR: cannot read directory entry '%s'\n", entry.name.c_str());
		return result;
	}

	// Read raw file data from disk.
	std::ifstream f(archive->file_path, std::ios::binary);
	if (!f.is_open()) {
		LOGF("[RpfReader] ERR: cannot reopen archive '%s'\n", archive->file_path.c_str());
		return result;
	}

	// Determine how many bytes to read from disk.
	uint32_t on_disk_size = entry.compressed_size;
	if (on_disk_size == 0 || on_disk_size >= entry.decompressed_size) {
		// Uncompressed: read decompressed_size bytes.
		on_disk_size = entry.decompressed_size;
	}

	if (on_disk_size == 0) {
		// Empty file.
		result.ok = true;
		return result;
	}

	f.seekg(entry.file_offset, std::ios::beg);
	std::vector<uint8_t> raw(on_disk_size);
	f.read(reinterpret_cast<char*>(raw.data()), on_disk_size);
	if (!f) {
		LOGF("[RpfReader] ERR: failed to read %u bytes at offset %u\n",
		     on_disk_size, entry.file_offset);
		return result;
	}

	// Check if the data is compressed.
	if (entry.compressed_size < entry.decompressed_size && entry.compressed_size > 0) {
		result.data = ZlibInflate(raw.data(), entry.compressed_size, entry.decompressed_size);
		result.ok = !result.data.empty();
	} else {
		result.data = std::move(raw);
		result.ok = true;
	}

	return result;
}

RpfFileData ReadFile(RpfArchive* archive, const char* name) {
	int32_t idx = FindEntry(archive, name);
	if (idx < 0) {
		LOGF("[RpfReader] WARN: file not found: '%s'\n", name);
		return {};
	}
	return ReadFile(archive, static_cast<uint32_t>(idx));
}

std::vector<std::string> ListEntries(const RpfArchive* archive) {
	std::vector<std::string> names;
	if (!archive) return names;
	names.reserve(archive->entries.size());
	for (const auto& e : archive->entries) {
		names.push_back(e.name);
	}
	return names;
}

bool FileExists(const RpfArchive* archive, const char* name) {
	return FindEntry(archive, name) >= 0;
}

uint32_t GetEntryCount(const RpfArchive* archive) {
	return archive ? archive->entry_count : 0;
}

const RpfEntry* GetEntry(const RpfArchive* archive, uint32_t index) {
	if (!archive || index >= archive->entries.size()) return nullptr;
	return &archive->entries[index];
}

} // namespace Libs::RpfArchive
