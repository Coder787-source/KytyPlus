// Regression tests for the PKG/PFS package parsers.
//
// Everything here is synthesized in-memory from publicly documented formats
// (psdevwiki / SpecterDev). No Sony data, keys, or copyrighted material.
//
// These tests pin the v3.1 parser fixes:
//   - PKG name-table offset read from the header entry table (not hardcoded
//     0x2B30) and bounds-checked against the file size
//   - PFS dirent padding handled once (name parser consumes the terminator,
//     only inter-entry padding is skipped)
//   - PFS 64-bit inode variant selected by superblock mode
//   - PFSC stream decode: offset table + stored blocks fast path
//   - extraction path-traversal guard (../ names cannot escape the output dir)

#include "package/pkgParser.h"
#include "package/pfsParser.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "PackageParserTests: FAILED: %s\n", text);
		++g_failures;
	} else {
		std::fprintf(stderr, "PackageParserTests: ok: %s\n", text);
	}
}

// Size-aware little/big-endian writers: grow the vector as needed.
void PutU16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
	if (at + 2 > v.size()) {
		v.resize(at + 2);
	}
	v[at]     = static_cast<uint8_t>(x & 0xFF);
	v[at + 1] = static_cast<uint8_t>(x >> 8);
}

void PutU32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
	if (at + 4 > v.size()) {
		v.resize(at + 4);
	}
	for (int i = 0; i < 4; ++i) {
		v[at + i] = static_cast<uint8_t>((x >> (8 * i)) & 0xFF);
	}
}

void PutU64(std::vector<uint8_t>& v, size_t at, uint64_t x) {
	if (at + 8 > v.size()) {
		v.resize(at + 8);
	}
	for (int i = 0; i < 8; ++i) {
		v[at + i] = static_cast<uint8_t>((x >> (8 * i)) & 0xFF);
	}
}

void PutBe32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
	if (at + 4 > v.size()) {
		v.resize(at + 4);
	}
	for (int i = 0; i < 4; ++i) {
		v[at + i] = static_cast<uint8_t>((x >> (8 * (3 - i))) & 0xFF);
	}
}

void WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::string TempPath(const char* name) {
	return (std::filesystem::temp_directory_path() / name).string();
}

// ---------------------------------------------------------------------------
// PKG synthesis (little-endian host; multi-byte header fields stored big-endian)
// ---------------------------------------------------------------------------

constexpr uint32_t PKG_NAME_TABLE_OFF  = 0x400;
constexpr uint32_t PKG_ENTRY_TABLE_OFF = 0x300;

std::vector<uint8_t> MakePkgHeader(uint32_t body_size) {
	// 0x2400: the parser bounds-checks name_off + 8192 <= file_size
	std::vector<uint8_t> v(0x2400, 0);

	// magic "\x7FPKG" (bytes as stored in the file)
	v[0] = 0x7F;
	v[1] = 'P';
	v[2] = 'K';
	v[3] = 'G';
	PutBe32(v, 0x0C, 2);                    // file_count
	PutBe32(v, 0x10, 2);                    // table_entries (16-byte entries)
	PutBe32(v, 0x18, PKG_ENTRY_TABLE_OFF);  // table_offset
	PutBe32(v, 0x24, 0x200);                // body_offset
	PutBe32(v, 0x2C, body_size);            // body_size

	const char* content_id = "UP0001-TEST00000_00-TESTCONTENTID000";
	std::memcpy(v.data() + 0x40, content_id, std::strlen(content_id));

	// entry table: first u32 is the name-table pointer (big-endian in file)
	PutBe32(v, PKG_ENTRY_TABLE_OFF + 0, PKG_NAME_TABLE_OFF);
	PutBe32(v, PKG_ENTRY_TABLE_OFF + 4, 0);

	// name table: null-separated names
	const char* names = "eboot.bin\0sce_sys/param.json\0";
	std::memcpy(v.data() + PKG_NAME_TABLE_OFF, names, 30);

	// body: PFS format magic 20130315 (LE) at body_offset + 0x08 — what
	// IsEncrypted() checks to classify the body as decrypted
	PutU32(v, 0x208, 20130315);

	return v;
}

// ---------------------------------------------------------------------------
// PFS synthesis (PS5-style: 64-bit inodes, 64 KiB blocks)
// ---------------------------------------------------------------------------

struct DirentSpec {
	uint32_t    type;
	uint32_t    inode;
	std::string name;
};

void AppendDirent(std::vector<uint8_t>& block, const DirentSpec& d) {
	while ((block.size() % 8) != 0) {
		block.push_back(0); // inter-entry padding
	}
	PutU32(block, block.size(), d.type);
	PutU32(block, block.size(), d.inode);
	PutU32(block, block.size(), static_cast<uint32_t>(d.name.size() + 1));
	PutU32(block, block.size(), 0);
	for (char c: d.name) {
		block.push_back(static_cast<uint8_t>(c));
	}
	block.push_back(0); // name terminator
}

// Builds a 5-block image: 0 superblock, 1 root inode, 2 dirents, 3 file inode,
// 4 file data. `with_evil_entry` adds a "../evil.txt" dirent so the extraction
// test can prove the path-traversal guard.
std::vector<uint8_t> MakePfsImage(const char* file_name, bool with_evil_entry) {
	constexpr uint32_t   BLOCK = 0x10000;
	std::vector<uint8_t> img(5 * BLOCK, 0);

	auto in_block = [&](uint32_t b) { return img.begin() + static_cast<std::ptrdiff_t>(b) * BLOCK; };

	// superblock (block 0)
	{
		std::vector<uint8_t> sb(0x100, 0);
		PutU64(sb, 0x00, 2);        // version = PS5
		PutU64(sb, 0x08, 20130315); // format magic
		sb[0x18] = 0;               // fmode
		sb[0x19] = 1;               // clean
		PutU16(sb, 0x1C, 0x2);      // mode: PFS_MODE_64BIT_INODES
		PutU32(sb, 0x20, BLOCK);    // block_size
		PutU64(sb, 0x28, 5);        // num_blocks
		PutU64(sb, 0x30, 3);        // num_inodes
		PutU64(sb, 0x38, 1);        // num_data_blocks
		PutU64(sb, 0x40, 1);        // num_inode_blocks
		PutU64(sb, 0x48, 2);        // root inode number (informational)
		std::copy(sb.begin(), sb.end(), in_block(0));
	}

	// inode writer (S64 layout)
	auto put_inode = [&](uint32_t block, uint32_t number, uint16_t mode, uint32_t flags,
	                     uint64_t size, int64_t db0) {
		const auto base = static_cast<std::ptrdiff_t>(block) * BLOCK;
		PutU32(img, base + 0x00, number);
		PutU16(img, base + 0x04, mode);
		PutU16(img, base + 0x06, 1); // nlink
		PutU32(img, base + 0x10, flags);
		PutU32(img, base + 0x14, 1); // blocks
		PutU64(img, base + 0x18, size);
		PutU64(img, base + 0x30, db0); // db[0]
	};

	put_inode(1, 1, 0x4000, 0, 0x1000, 2); // root directory, dirent block 2
	put_inode(3, 3, 0x8000, 0, 11, 4);     // regular file, data block 4

	// dirent block (block 2)
	{
		std::vector<uint8_t> dents;
		AppendDirent(dents, {4, 1, "."});
		AppendDirent(dents, {5, 1, ".."});
		AppendDirent(dents, {2, 3, file_name});
		if (with_evil_entry) {
			AppendDirent(dents, {2, 3, "../evil.txt"});
		}
		dents.resize(dents.size() + 8, 0); // type=0 end marker (8-aligned)
		std::copy(dents.begin(), dents.end(), in_block(2));
	}

	// file data (block 4)
	const char* content = "hello world";
	std::copy(content, content + 11, in_block(4));

	return img;
}

// ---------------------------------------------------------------------------
// PFSC stream synthesis (stored/uncompressed blocks; no codec dependency)
// ---------------------------------------------------------------------------

constexpr uint32_t PFSC_BLOCK = 0x10000;

std::vector<uint8_t> MakePfscStream(uint32_t block_count, const std::vector<uint8_t>& payload) {
	// payload must be block_count * PFSC_BLOCK bytes of source data; every
	// block is stored verbatim (offset delta == raw length).
	const uint64_t logical = static_cast<uint64_t>(block_count) * PFSC_BLOCK;
	std::vector<uint8_t> stream(0x10000 + static_cast<size_t>(logical), 0);

	stream[0] = 'P';
	stream[1] = 'F';
	stream[2] = 'S';
	stream[3] = 'C';
	PutU32(stream, 0x0C, block_count);
	PutU64(stream, 0x10, logical);

	uint64_t off = 0x10000;
	for (uint32_t i = 0; i <= block_count; ++i) {
		PutU64(stream, 0x400 + static_cast<size_t>(i) * 8, off);
		if (i < block_count) {
			off += PFSC_BLOCK;
		}
	}
	std::copy(payload.begin(), payload.end(), stream.begin() + 0x10000);
	return stream;
}

} // namespace

int main() {
	// Unbuffered stdout so the emulator's LOGF trail survives a crash in the
	// code under test (which is exactly what these tests are hunting for).
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	// ---- PKG: valid header with header-driven name table ----
	{
		const auto path = TempPath("kyty_test_valid.pkg");
		WriteFile(path, MakePkgHeader(0x400));
		auto r = Libs::Firmware::PkgParser::Parse(path);
		Check(r.ok, "PKG: valid image parses ok");
		Check(r.file_count == 2, "PKG: file_count from header");
		Check(r.files.size() == 2, "PKG: two names from the header-driven name table");
		Check(r.files.size() == 2 && r.files[0].name == "eboot.bin" &&
		          r.files[1].name == "sce_sys/param.json",
		      "PKG: names parsed correctly");
		Check(!r.is_encrypted, "PKG: body with PFS magic reported decrypted");
		std::filesystem::remove(path);
	}

	// ---- PKG: out-of-bounds name-table pointer must not read past EOF ----
	{
		auto v = MakePkgHeader(0x400);
		PutBe32(v, PKG_ENTRY_TABLE_OFF + 0, 0xFFFF0000); // bogus pointer
		const auto path = TempPath("kyty_test_badtable.pkg");
		WriteFile(path, v);
		auto r = Libs::Firmware::PkgParser::Parse(path);
		Check(r.ok, "PKG: bogus name-table pointer still parses the container");
		Check(r.files.empty(), "PKG: bogus name-table pointer yields no names (bounds check)");
		std::filesystem::remove(path);
	}

	// ---- PKG: zero name-table pointer falls back to 0x2B30 (absent here) ----
	{
		auto v = MakePkgHeader(0x400);
		PutBe32(v, PKG_ENTRY_TABLE_OFF + 0, 0); // no header pointer
		const auto path = TempPath("kyty_test_fallback.pkg");
		WriteFile(path, v);
		auto r = Libs::Firmware::PkgParser::Parse(path);
		Check(r.ok && r.files.empty(),
		      "PKG: zero name-table pointer uses the 0x2B30 fallback (absent here)");
		std::filesystem::remove(path);
	}

	// ---- PKG: truncated file ----
	{
		const auto path = TempPath("kyty_test_trunc.pkg");
		WriteFile(path, std::vector<uint8_t>(16, 0x7F));
		auto r = Libs::Firmware::PkgParser::Parse(path);
		Check(!r.ok, "PKG: truncated file rejected");
		std::filesystem::remove(path);
	}

	// ---- PFS: dirent padding + 64-bit inodes + full walk ----
	{
		const auto path = TempPath("kyty_test.pfs");
		WriteFile(path, MakePfsImage("hello.txt", false));
		auto r = Libs::Firmware::PfsParser::Parse(path);
		Check(r.ok, "PFS: synthetic image parses ok");
		Check(r.version == 2 && r.mode == 0x2, "PFS: PS5 + 64-bit-inode mode detected");
		Check(r.files.size() == 1, "PFS: dirent padding handled once - entry not dropped");
		Check(r.files.size() == 1 && r.files[0].name == "hello.txt", "PFS: file name correct");
		Check(r.files.size() == 1 && r.files[0].size == 11 && !r.files[0].is_directory,
		      "PFS: inode 64-bit size and type correct");
		Check(r.files.size() == 1 && r.files[0].block_number == 4,
		      "PFS: direct block pointer correct");
		std::filesystem::remove(path);
	}

	// ---- PFS: extraction + path-traversal guard ----
	{
		const auto path = TempPath("kyty_test_evil.pfs");
		WriteFile(path, MakePfsImage("hello.txt", true));
		auto r = Libs::Firmware::PfsParser::Parse(path);
		Check(r.ok && r.files.size() == 2, "PFS: traversal entry listed during parse");

		const auto out_dir = TempPath("kyty_test_out") + "/";
		std::error_code ec;
		std::filesystem::remove_all(out_dir, ec);
		const auto extracted = Libs::Firmware::PfsParser::ExtractAll(r, path, out_dir);
		Check(extracted == 1, "PFS: exactly the safe file extracted");

		const bool hello_ok = std::filesystem::file_size(out_dir + "hello.txt", ec) == 11 && !ec;
		Check(hello_ok, "PFS: extracted content has the expected size");
		const bool escaped = std::filesystem::exists(TempPath("evil.txt"), ec) && !ec;
		Check(!escaped, "PFS: ../-style name did not escape the output directory");
		std::filesystem::remove_all(out_dir, ec);
		std::filesystem::remove(TempPath("evil.txt"), ec);
		std::filesystem::remove(path, ec);
	}

	// ---- PFSC: stored-block stream decode ----
	{
		std::vector<uint8_t> payload(2 * PFSC_BLOCK); // must match block_count * 64 KiB
		for (size_t i = 0; i < payload.size(); ++i) {
			payload[i] = static_cast<uint8_t>(i * 31 + (i >> 9));
		}
		auto stream = MakePfscStream(2, payload);
		auto out    = Libs::Firmware::PfsParser::DecompressPfscStream(stream);
		Check(out == payload, "PFSC: two-block stored stream decodes verbatim");

		// corrupt block_count -> rejected
		PutU32(stream, 0x0C, 3);
		out = Libs::Firmware::PfsParser::DecompressPfscStream(stream);
		Check(out.empty(), "PFSC: wrong block_count rejected");
		PutU32(stream, 0x0C, 2);

		// truncated offset table -> rejected
		stream.resize(0x410);
		out = Libs::Firmware::PfsParser::DecompressPfscStream(stream);
		Check(out.empty(), "PFSC: truncated offset table rejected");
	}

	// ---- PFSC: bad magic / tiny input ----
	{
		std::vector<uint8_t> junk(0x40, 0);
		Check(Libs::Firmware::PfsParser::DecompressPfscStream(junk).empty(),
		      "PFSC: non-PFSC buffer rejected");
		Check(Libs::Firmware::PfsParser::DecompressPfscStream({}).empty(),
		      "PFSC: empty buffer rejected");
	}

	if (g_failures != 0) {
		std::fprintf(stderr, "PackageParserTests: %d failure(s)\n", g_failures);
		return 1;
	}
	std::fprintf(stderr, "PackageParserTests: all checks passed\n");
	return 0;
}
