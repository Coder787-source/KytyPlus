// PfsVolume + PfscBlockReader implementation.
//
// Runtime PFS mount layer: parses superblock/inodes/dirents at mount time and
// serves on-demand reads through the (correct) PFSC block decompressor.
// Backends are pluggable via DecompressionProvider; the stock codec is zlib,
// and user-supplied Oodle cores (never redistributed) handle Kraken-family
// codecs when present.
//
// This file contains no Sony copyrighted material and no decryption keys.

#include "package/pfsVolume.h"

#include "IO/Decompressor.hpp"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace Libs::Firmware {

// ============================================================================
// PfscBlockReader
// ============================================================================

namespace {
uint32_t ReadU32LE(const uint8_t* p) {
	uint32_t v = 0;
	std::memcpy(&v, p, sizeof(v));
	return v;
}
uint64_t ReadU64LE(const uint8_t* p) {
	uint64_t v = 0;
	std::memcpy(&v, p, sizeof(v));
	return v;
}
} // namespace

bool PfscBlockReader::ReadFromBacking(uint64_t off, uint32_t len, void* dst) {
	if (!m_read_backing) {
		return false;
	}
	return m_read_backing(off, len, dst);
}

bool PfscBlockReader::Init(uint64_t stream_offset, uint32_t algo_id, ReadBacking read_backing) {
	m_stream_offset = stream_offset;
	m_algo_id       = algo_id;
	m_read_backing  = std::move(read_backing);

	// ---- header ----
	m_header.assign(PFSC_HEADER_SIZE, 0);
	if (!ReadFromBacking(m_stream_offset, PFSC_HEADER_SIZE, m_header.data())) {
		return false;
	}

	// 'PFSC' little-endian on disk => bytes 50 46 53 43
	if (m_header[0] != 0x50 || m_header[1] != 0x46 || m_header[2] != 0x53 || m_header[3] != 0x43) {
		LOGF("PFSC: bad magic at stream offset 0x%llx", static_cast<unsigned long long>(m_stream_offset));
		return false;
	}

	m_block_count  = ReadU32LE(m_header.data() + 0x0C);
	m_logical_size = ReadU64LE(m_header.data() + 0x10);

	// Sanity: block count must cover the logical size, and must be plausible.
	if (m_logical_size == 0 || m_block_count == 0) {
		return false;
	}
	const uint32_t max_blocks = static_cast<uint32_t>((m_logical_size + PFSC_LOGICAL_BLOCK_SIZE - 1)
	                                                   / PFSC_LOGICAL_BLOCK_SIZE);
	if (m_block_count != max_blocks) {
		LOGF("PFSC: block_count %u does not match size %llu (expected %u)", m_block_count,
		     static_cast<unsigned long long>(m_logical_size), max_blocks);
		return false;
	}
	if (m_block_count > (1u << 26)) {
		return false;
	}

	// ---- offset table: (block_count + 1) u64 entries at 0x400 ----
	const uint64_t table_off  = m_stream_offset + PFSC_BLOCK_OFFSETS_OFFSET;
	const size_t  table_len   = static_cast<size_t>(m_block_count + 1) * PFSC_OFFSET_ENTRY_SIZE;

	std::vector<uint8_t> table(table_len);
	if (!ReadFromBacking(table_off, static_cast<uint32_t>(table_len), table.data())) {
		return false;
	}

	m_block_offsets.assign(m_block_count + 1, 0);
	for (uint32_t i = 0; i <= m_block_count; ++i) {
		m_block_offsets[i] = ReadU64LE(table.data() + static_cast<size_t>(i) * PFSC_OFFSET_ENTRY_SIZE);
	}

	// ---- per-block info ----
	m_blocks.resize(m_block_count);
	for (uint32_t i = 0; i < m_block_count; ++i) {
		const uint64_t cur  = m_block_offsets[i];
		const uint64_t next = (i + 1 <= m_block_count) ? m_block_offsets[i + 1] : cur;
		if (next < cur || next > PFSC_INITIAL_DATA_OFFSET + (1ull << 40)) {
			LOGF("PFSC: implausible offset table entry %u (0x%llx -> 0x%llx)", i,
			     static_cast<unsigned long long>(cur), static_cast<unsigned long long>(next));
			return false;
		}
		m_blocks[i].stream_offset = cur;
		m_blocks[i].comp_len      = static_cast<uint32_t>(next - cur);
		m_blocks[i].raw_len       = LogicalBlockSizeFor(i);
	}

	m_initialized = true;
	LOGF("PFSC: stream at 0x%llx: %u blocks, logical size %llu, algo %u",
	     static_cast<unsigned long long>(m_stream_offset), m_block_count,
	     static_cast<unsigned long long>(m_logical_size), m_algo_id);
	return true;
}

uint32_t PfscBlockReader::LogicalBlockSizeFor(uint32_t block_index) const {
	const uint64_t base = static_cast<uint64_t>(block_index) * PFSC_LOGICAL_BLOCK_SIZE;
	if (base + PFSC_LOGICAL_BLOCK_SIZE <= m_logical_size) {
		return PFSC_LOGICAL_BLOCK_SIZE;
	}
	if (m_logical_size > base) {
		return static_cast<uint32_t>(m_logical_size - base);
	}
	return 0;
}

bool PfscBlockReader::DecodeBlockInto(size_t block_index, void* dst) {
	if (!m_initialized || block_index >= m_blocks.size()) {
		return false;
	}

	const BlockInfo& b = m_blocks[block_index];
	if (b.comp_len == 0 || b.raw_len == 0) {
		return false;
	}

	// Stored-uncompressed convention: when a block didn't shrink, the offset
	// entry's length encodes the raw length directly (offset delta equals raw
	// len, i.e. entry i+1 - entry i == raw_len). Fast-path the memcpy out.
	if (b.comp_len == b.raw_len) {
		return ReadFromBacking(m_stream_offset + b.stream_offset, b.comp_len, dst);
	}

	std::vector<uint8_t> comp(b.comp_len);
	if (!ReadFromBacking(m_stream_offset + b.stream_offset, b.comp_len, comp.data())) {
		LOGF("PFSC: block %zu backing read failed", block_index);
		return false;
	}

	// Route through the provider so backends stay swappable.
	auto result = KytyPS5::IO::DecompressionProvider::Instance().ProcessAsset(m_algo_id, comp, b.raw_len);
	if (!result) {
		LOGF("PFSC: block %zu decode failed (err=%d)", block_index, static_cast<int>(result.error()));
		return false;
	}
	if (result->size() != b.raw_len) {
		LOGF("PFSC: block %zu decoded %zu bytes, expected %u", block_index, result->size(), b.raw_len);
		return false;
	}

	std::memcpy(dst, result->data(), b.raw_len);
	return true;
}

int64_t PfscBlockReader::Read(uint64_t off, size_t size, void* dst) {
	if (!m_initialized) {
		return -1;
	}
	if (off >= m_logical_size || size == 0) {
		return 0;
	}

	size_t first = off / PFSC_LOGICAL_BLOCK_SIZE;
	size_t last  = (off + size - 1) / PFSC_LOGICAL_BLOCK_SIZE;
	if (last >= m_blocks.size()) {
		last = m_blocks.size() - 1;
	}

	uint8_t* out  = static_cast<uint8_t*>(dst);
	size_t   done = 0;

	for (size_t c = first; c <= last; ++c) {
		const size_t in_off = (c == first) ? (off % PFSC_LOGICAL_BLOCK_SIZE) : 0;
		const size_t want   = std::min<size_t>(m_blocks[c].raw_len - in_off, size - done);

		if (in_off == 0 && want == m_blocks[c].raw_len) {
			// Fast path: decode straight into the caller's buffer, no cache.
			if (!DecodeBlockInto(c, out + done)) {
				return (done > 0 ? static_cast<int64_t>(done) : -1);
			}
		} else {
			// Partial block: decode to cache, copy the slice out.
			std::vector<uint8_t> block(m_blocks[c].raw_len);
			if (!DecodeBlockInto(c, block.data())) {
				return (done > 0 ? static_cast<int64_t>(done) : -1);
			}
			CacheInsert(c, std::move(block));
			uint8_t* p = CacheGet(c);
			if (p == nullptr) {
				return (done > 0 ? static_cast<int64_t>(done) : -1);
			}
			std::memcpy(out + done, p + in_off, want);
		}

		done += want;
	}

	return static_cast<int64_t>(done);
}

uint8_t* PfscBlockReader::CacheGet(size_t block_index) {
	auto it = m_cache.find(CacheKey{m_stream_offset, block_index});
	if (it == m_cache.end()) {
		return nullptr;
	}

	// LRU bump
	auto key = it->first;
	std::erase_if(m_lru, [&key](const CacheKey& k) { return k == key; });
	m_lru.push_back(key);

	return it->second.data();
}

void PfscBlockReader::CacheInsert(size_t block_index, std::vector<uint8_t>&& data) {
	const CacheKey key{m_stream_offset, block_index};

	// Replace-or-insert + LRU bookkeeping.
	auto it = m_cache.find(key);
	if (it != m_cache.end()) {
		m_cache_bytes -= it->second.size();
		it->second = std::move(data);
	} else {
		m_cache.emplace(key, std::move(data));
	}
	std::erase_if(m_lru, [&key](const CacheKey& k) { return k == key; });
	m_lru.push_back(key);
	m_cache_bytes += m_cache[key].size();

	// Evict LRU until under budget.
	while (m_cache_bytes > kCacheBudgetBytes && !m_lru.empty()) {
		const CacheKey victim = m_lru.front();
		auto           vit    = m_cache.find(victim);
		if (vit != m_cache.end()) {
			m_cache_bytes -= vit->second.size();
			m_cache.erase(vit);
		}
		m_lru.erase(m_lru.begin());
	}
}

// ============================================================================
// PfsVolume
// ============================================================================

bool PfsVolume::Mount(ReadBacking read_backing, const Options& options) {
	m_read_backing = std::move(read_backing);
	m_options      = options;
	m_mounted      = false;

	// Ensure stock + user-supplied decoders are registered once.
	KytyPS5::IO::RegisterBuiltinDecompressors();

	// ---- superblock (0x380 bytes at image offset 0) ----
	std::vector<uint8_t> sb_buf(sizeof(PfsSuperblock));
	if (!m_read_backing(0, static_cast<uint32_t>(sb_buf.size()), sb_buf.data())) {
		return false;
	}

	PfsSuperblock sb {};
	std::memcpy(&sb, sb_buf.data(), sizeof(sb));

	if (sb.format != PFS_FORMAT_MAGIC) {
		LOGF("PfsVolume: bad format magic %llu (expected %llu)", static_cast<unsigned long long>(sb.format),
		     static_cast<unsigned long long>(PFS_FORMAT_MAGIC));
		return false;
	}

	m_version    = static_cast<uint32_t>(sb.version);
	m_mode       = sb.mode;
	m_block_size = sb.block_size;
	m_num_blocks = static_cast<uint32_t>(sb.num_blocks);
	m_num_inodes = static_cast<uint32_t>(sb.num_inodes);

	if (m_block_size == 0 || m_block_size > 0x100000) {
		LOGF("PfsVolume: invalid block size 0x%X", m_block_size);
		return false;
	}

	LOGF("PfsVolume: version=%u (%s), mode=0x%X, block_size=0x%X, blocks=%u, inodes=%u", m_version,
	     (m_version == PFS_VERSION_PS5 ? "PS5" : "PS4"), m_mode, m_block_size, m_num_blocks, m_num_inodes);

	// ---- root inode ----
	// PFS keeps inode N at block N of the inode region (root = 2).
	PfsParser::InodeInfo root {};
	bool                 found_root = false;
	for (uint32_t try_block = 1; try_block <= 4 && !found_root; ++try_block) {
		if (ReadInodeByNumber(try_block, root) && (root.mode & INODE_MODE_DIR) != 0 && root.number >= 1) {
			found_root = true;
		}
	}
	if (!found_root) {
		LOGF("PfsVolume: root inode not found in blocks 1..4");
		return false;
	}

	// ---- enumerate tree at mount time ----
	m_entries.clear();

	// Root entry (name "") so that opening the mount point itself works.
	EntryInfo root_entry;
	root_entry.name    = "";
	root_entry.inode   = root.number;
	root_entry.size    = 0;
	root_entry.is_dir  = true;
	root_entry.first_block = (root.db[0] > 0) ? root.db[0] : 0;
	m_entries.push_back(root_entry);

	BuildEntryListRecursive(root, "");

	m_mounted = true;
	LOGF("PfsVolume: mounted, %zu entries", m_entries.size());
	return true;
}

bool PfsVolume::ReadBlockBacking(uint64_t block_num, std::vector<uint8_t>& out) {
	if (block_num >= m_num_blocks) {
		return false;
	}
	out.assign(m_block_size, 0);
	return m_read_backing(block_num * static_cast<uint64_t>(m_block_size), m_block_size, out.data());
}

bool PfsVolume::ReadInodeByNumber(uint32_t inode_num, PfsParser::InodeInfo& out) {
	{
		std::lock_guard<std::mutex> lock(m_inode_cache_mutex);
		auto                       it = m_inode_cache.find(inode_num);
		if (it != m_inode_cache.end()) {
			out = it->second;
			return true;
		}
	}

	// inode N lives at block N (the inode table occupies the first blocks of
	// the image, root inode = 2 — same convention the offline parser uses).
	if (inode_num == 0 || inode_num >= m_num_blocks) {
		return false;
	}

	std::vector<uint8_t> buf;
	if (!ReadBlockBacking(inode_num, buf)) {
		return false;
	}

	// Variant selection mirrors PfsParser::ReadInode.
	const bool is_64bit = (m_mode & PFS_MODE_64BIT_INODES) != 0;
	const int  variant  = is_64bit ? (m_version == PFS_VERSION_PS5 ? 3 : 2) : 1;

	PfsInodeD32 d32 {};
	PfsInodeS32 s32 {};
	PfsInodeS64 s64 {};
	switch (variant) {
		case 1: std::memcpy(&d32, buf.data(), std::min(buf.size(), sizeof(d32))); break;
		case 2: std::memcpy(&s32, buf.data(), std::min(buf.size(), sizeof(s32))); break;
		default: std::memcpy(&s64, buf.data(), std::min(buf.size(), sizeof(s64))); break;
	}

	out = PfsParser::ExtractInodeInfo(d32, s32, s64, variant);

	{
		std::lock_guard<std::mutex> lock(m_inode_cache_mutex);
		m_inode_cache[inode_num] = out;
	}
	return true;
}

bool PfsVolume::ResolveIndirect(const PfsParser::InodeInfo& inode, std::vector<int64_t>& out_blocks) {
	out_blocks.clear();

	// Direct blocks (0-11).
	for (size_t i = 0; i < MAX_DIRECT_BLOCKS; ++i) {
		if (inode.db[i] < 0) {
			break;
		}
		out_blocks.push_back(inode.db[i]);
	}

	// Indirect blocks: ib[0..4], each holds block_size/ptr_size pointers.
	const bool   is_64bit        = (m_mode & PFS_MODE_64BIT_INODES) != 0;
	const size_t ptr_size       = is_64bit ? 8 : 4;
	const size_t ptrs_per_block = m_block_size / ptr_size;

	for (size_t level = 0; level < MAX_INDIRECT_BLOCKS; ++level) {
		if (inode.ib[level] < 0 || static_cast<uint32_t>(inode.ib[level]) >= m_num_blocks) {
			break;
		}

		std::vector<uint8_t> data;
		if (!ReadBlockBacking(static_cast<uint64_t>(inode.ib[level]), data)) {
			break;
		}

		for (size_t j = 0; j < ptrs_per_block; ++j) {
			int64_t ptr = 0;
			if (is_64bit) {
				if (j * 8 + 8 > data.size()) {
					break;
				}
				std::memcpy(&ptr, data.data() + j * 8, 8);
			} else {
				if (j * 4 + 4 > data.size()) {
					break;
				}
				int32_t ptr32 = 0;
				std::memcpy(&ptr32, data.data() + j * 4, 4);
				ptr = ptr32;
			}
			if (ptr < 0 || static_cast<uint32_t>(ptr) >= m_num_blocks) {
				break;
			}
			out_blocks.push_back(ptr);
		}
	}
	return true;
}

bool PfsVolume::ReadDirData(const PfsParser::InodeInfo& dir_inode,
                            std::vector<std::pair<std::string, uint32_t>>& out) {
	std::vector<int64_t> blocks;
	if (!ResolveIndirect(dir_inode, blocks)) {
		return false;
	}

	for (int64_t b: blocks) {
		std::vector<uint8_t> data;
		if (!ReadBlockBacking(static_cast<uint64_t>(b), data)) {
			continue;
		}
		ParseDirentsFromBlock(data.data(), data.size(), out);
	}
	return true;
}

bool PfsVolume::ParseDirentsFromBlock(const uint8_t* data, size_t size,
                                      std::vector<std::pair<std::string, uint32_t>>& out) {
	size_t pos = 0;
	while (pos + sizeof(PfsDirent) <= size) {
		PfsDirent dirent {};
		std::memcpy(&dirent, data + pos, sizeof(dirent));

		// 0 = end of entries; anything above DOTDOT is invalid.
		if (dirent.type == 0 || dirent.type > DIRENT_TYPE_DOTDOT) {
			break;
		}

		pos += sizeof(dirent);
		if (pos + dirent.name_size > size) {
			break;
		}

		std::string name;
		for (uint32_t j = 0; j < dirent.name_size && pos < size; ++j) {
			if (data[pos] == '\0') {
				++pos;
				break;
			}
			name += static_cast<char>(data[pos++]);
		}

		// Entries are 8-byte aligned.
		while (pos < size && (pos % 8) != 0) {
			++pos;
		}

		if (dirent.type != DIRENT_TYPE_DOT && dirent.type != DIRENT_TYPE_DOTDOT) {
			out.emplace_back(name, dirent.inode);
		}
	}
	return true;
}

bool PfsVolume::ReadFileBlocks(const PfsParser::InodeInfo& inode, uint64_t offset, size_t size, void* dst,
                               int64_t& bytes_read) {
	bytes_read = 0;
	if (size == 0) {
		return true;
	}

	std::vector<int64_t> blocks;
	if (!ResolveIndirect(inode, blocks)) {
		return false;
	}

	const size_t first = offset / m_block_size;
	if (first >= blocks.size()) {
		return true; // read at/after EOF
	}

	uint8_t* out     = static_cast<uint8_t*>(dst);
	size_t   done    = 0;
	size_t   logical = 0;

	for (size_t i = first; i < blocks.size() && done < size; ++i) {
		std::vector<uint8_t> data;
		if (!ReadBlockBacking(static_cast<uint64_t>(blocks[i]), data)) {
			return false;
		}

		const size_t in_off = (i == first) ? (offset % m_block_size) : 0;
		const size_t avail  = (data.size() > in_off) ? (data.size() - in_off) : 0;
		const size_t want   = std::min<size_t>(avail, size - done);

		if (want > 0) {
			std::memcpy(out + done, data.data() + in_off, want);
			done += want;
		}
		logical += data.size();
	}

	bytes_read = static_cast<int64_t>(done);
	return true;
}

bool PfsVolume::InitPfscStream(uint32_t inode_number, const PfsParser::InodeInfo& inode,
                               std::shared_ptr<PfscBlockReader>& out) {
	{
		std::lock_guard<std::mutex> lock(m_pfsc_readers_mutex);
		auto                       it = m_pfsc_readers.find(inode_number);
		if (it != m_pfsc_readers.end()) {
			out = it->second;
			return true;
		}
	}

	auto reader = std::make_shared<PfscBlockReader>();

	// The PFSC stream starts at the inode's first data block (db[0]), same
	// convention the offline ExtractAll path uses.
	const uint64_t stream_offset =
	    (inode.db[0] > 0 ? static_cast<uint64_t>(inode.db[0]) : 0) * m_block_size;

	if (!reader->Init(stream_offset, m_options.algo_id, m_read_backing)) {
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(m_pfsc_readers_mutex);
		m_pfsc_readers[inode_number] = reader;
		out                          = reader;
	}
	return true;
}

int64_t PfsVolume::ReadFile(const EntryInfo& entry, uint64_t offset, size_t size, void* dst) {
	if (size == 0) {
		return 0;
	}

	PfsParser::InodeInfo inode;
	if (!ReadInodeByNumber(entry.inode, inode)) {
		return -1;
	}

	// Clamp to EOF.
	if (offset >= inode.size) {
		return 0;
	}
	if (offset + size > inode.size) {
		size = static_cast<size_t>(inode.size - offset);
	}

	// PFSC-compressed inode -> read through the block reader.
	if (entry.is_compressed || (inode.flags & INODE_FLAG_COMPRESSED) != 0) {
		std::shared_ptr<PfscBlockReader> reader;
		if (!InitPfscStream(entry.inode, inode, reader)) {
			return -1;
		}
		return reader->Read(offset, size, dst);
	}

	int64_t done = 0;
	if (!ReadFileBlocks(inode, offset, size, dst, done)) {
		return -1;
	}
	return done;
}

void PfsVolume::BuildEntryListRecursive(const PfsParser::InodeInfo& dir_inode, const std::string& prefix) {
	std::vector<std::pair<std::string, uint32_t>> entries_vec;
	if (!ReadDirData(dir_inode, entries_vec)) {
		return;
	}

	for (const auto& [name, inode_num]: entries_vec) {
		if (inode_num == 0 || inode_num > m_num_inodes) {
			continue;
		}

		PfsParser::InodeInfo info;
		if (!ReadInodeByNumber(inode_num, info)) {
			continue;
		}

		EntryInfo e;
		e.name          = prefix.empty() ? name : (prefix + "/" + name);
		e.inode         = info.number;
		e.size          = info.size;
		e.is_dir        = (info.mode & INODE_MODE_DIR) != 0;
		e.is_compressed = (info.flags & INODE_FLAG_COMPRESSED) != 0;
		e.first_block   = (info.db[0] > 0) ? info.db[0] : 0;
		m_entries.push_back(e);

		if (e.is_dir) {
			BuildEntryListRecursive(info, e.name);
		}
	}
}

const PfsVolume::EntryInfo* PfsVolume::Lookup(const std::string& path) const {
	const auto norm = NormalizePath(path);

	// Empty path = volume root (the mount point itself).
	if (norm.empty()) {
		for (const auto& e: m_entries) {
			if (e.name.empty() && e.is_dir) {
				return &e;
			}
		}
		return nullptr;
	}

	for (const auto& e: m_entries) {
		if (e.name == norm) {
			return &e;
		}
	}
	return nullptr;
}

std::vector<PfsVolume::EntryInfo> PfsVolume::ListDir(const std::string& dir_path) const {
	std::vector<EntryInfo> out;
	const auto             norm = NormalizePath(dir_path);

	for (const auto& e: m_entries) {
		if (e.name.empty()) {
			continue; // the root marker itself
		}
		if (norm.empty()) {
			// Root children: top-level names only.
			if (e.name.find('/') == std::string::npos) {
				out.push_back(e);
			}
		} else if (e.name.find(norm + "/") == 0) {
			// Children of the directory: path starts with "<dir>/".
			const auto rest = e.name.substr(norm.size() + 1);
			if (rest.find('/') == std::string::npos) {
				out.push_back(e);
			}
		}
	}
	return out;
}

std::string PfsVolume::NormalizePath(const std::string& path) {
	std::string p = path;

	// Strip leading '/' (volume-relative internally).
	while (!p.empty() && p.front() == '/') {
		p.erase(0, 1);
	}
	// Strip trailing '/'.
	while (!p.empty() && p.back() == '/') {
		p.pop_back();
	}

	return p;
}

} // namespace Libs::Firmware