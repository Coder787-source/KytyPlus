// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KYTY_LIBS_RPF_ARCHIVE_READER_H_
#define KYTY_LIBS_RPF_ARCHIVE_READER_H_

#include "common/common.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::RpfArchive {

// ─── RPF magic / version constants ───────────────────────────────────────────

constexpr uint32_t kRpfMagic_RPF3 = 0x33465052; // "RPF3" — GTA IV era
constexpr uint32_t kRpfMagic_RPF6 = 0x36465052; // "RPF6" — Max Payne 3
constexpr uint32_t kRpfMagic_RPF7 = 0x37465052; // "RPF7" — GTA V (PS4/PS5)
constexpr uint32_t kRpfMagic_RPF8 = 0x38465052; // "RPF8" — RDR2

// ─── TOC entry types ─────────────────────────────────────────────────────────

enum class RpfEntryType : uint32_t {
	Binary   = 0x7FFFFF01, // plain or zlib-compressed data
	Resource = 0x7FFFFF02, // typed resource with flags
	Dir      = 0x7FFFFF03, // subdirectory
};

// ─── TOC entry ───────────────────────────────────────────────────────────────

struct RpfEntry {
	RpfEntryType type          = RpfEntryType::Binary;
	std::string  name;
	uint32_t     file_offset   = 0;  // offset into the .rpf file
	uint32_t     decompressed_size = 0;
	uint32_t     compressed_size   = 0; // if < decompressed_size → zlib-compressed
	uint32_t     resource_flags    = 0;
	uint32_t     dir_entry_count   = 0; // for directory entries
	uint32_t     dir_first_index   = 0; // first child index in TOC
};

// ─── File data returned from ReadFile ────────────────────────────────────────

struct RpfFileData {
	std::vector<uint8_t> data;
	bool                 ok = false;
};

// ─── Archive handle ──────────────────────────────────────────────────────────

struct RpfArchive {
	uint32_t               magic        = 0;
	uint32_t               version      = 0;
	uint32_t               entry_count  = 0;
	uint32_t               flags        = 0;
	std::string            file_path;
	std::vector<RpfEntry>  entries;

	// Name → entry index lookup (lowercase for case-insensitive search).
	std::unordered_map<std::string, uint32_t> name_index;
};

// ─── Public API ──────────────────────────────────────────────────────────────

// Open and parse an RPF archive file. Returns nullptr on failure.
// The returned pointer is heap-allocated; caller owns it.
RpfArchive* OpenArchive(const char* path);

// Close and free an archive handle.
void CloseArchive(RpfArchive* archive);

// Look up an entry by name (case-insensitive). Returns -1 if not found.
int32_t FindEntry(const RpfArchive* archive, const char* name);

// Read and decompress a file entry by index. Returns the raw bytes.
RpfFileData ReadFile(RpfArchive* archive, uint32_t entry_index);

// Read a file by name. Convenience wrapper.
RpfFileData ReadFile(RpfArchive* archive, const char* name);

// List all file entries (names) in the archive.
std::vector<std::string> ListEntries(const RpfArchive* archive);

// Check if a file exists in the archive.
bool FileExists(const RpfArchive* archive, const char* name);

// Get the entry count.
uint32_t GetEntryCount(const RpfArchive* archive);

// Get entry info by index.
const RpfEntry* GetEntry(const RpfArchive* archive, uint32_t index);

} // namespace Libs::RpfArchive

#endif // KYTY_LIBS_RPF_ARCHIVE_READER_H_
