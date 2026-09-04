#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "package/pfsParser.h"

namespace Libs::Firmware {

// ============================================================================
// PfscBlockReader — correct PFSC layer for a single compressed inode stream.
//
// On-disk layout (per compressed inode stream; constants from pfsParser.h,
// verified against MkPFS consts.py):
//   +0x00  'PFSC' magic
//   +0x04  unknown/flags (MkPFS writes 2)
//   +0x0C  u32 block_count
//   +0x10  u32 uncompressed_size (total logical size of this stream)
//   ...    (rest of the 0x30-byte header)
//   +0x400 offset table: (block_count + 1) entries, u64 each.
//          Entry[i] = byte offset of compressed block i within the stream;
//          the +1 terminator entry gives the end of the last block.
//   +0x10000 first compressed block.
//
// The compression algorithm is NOT recorded in the stream. Stock codec is
// zlib/deflate; images repacked with Oodle/Kraken tools exist. The codec id
// is passed in from the PFS inode flags / container metadata and routed
// through DecompressionProvider so backends stay swappable and proprietary
// Oodle cores are loaded user-side at runtime (never redistributed).
// ============================================================================

class PfscBlockReader {
public:
	// read_backing(offset, length, dst) must return true when `length` bytes
	// were read into dst.
	using ReadBacking = std::function<bool(uint64_t offset, uint32_t length, void* dst)>;

	struct BlockInfo {
		uint64_t stream_offset; // compressed offset of block i (from offset table)
		uint32_t comp_len;       // comp_len[i] = off[i+1] - off[i]
		uint32_t raw_len;        // logical length; PFSC_LOGICAL_BLOCK_SIZE except the last
	};

	PfscBlockReader()                                       = default;
	~PfscBlockReader()                                      = default;
	PfscBlockReader(const PfscBlockReader&)            = delete;
	PfscBlockReader& operator=(const PfscBlockReader&) = delete;

	// Parses the PFSC header + offset table. `stream_offset` is where this
	// PFSC stream starts inside the backing file.
	bool Init(uint64_t stream_offset, uint32_t algo_id, ReadBacking read_backing);

	bool IsInitialized() const {
		return m_initialized;
	}

	uint64_t LogicalSize() const {
		return m_logical_size;
	}

	uint32_t AlgoId() const {
		return m_algo_id;
	}

	// Reads logical [off, off+size) into dst. Returns bytes read, or -1 on
	// hard I/O / decompression failure. A short read at EOF is normal.
	int64_t Read(uint64_t off, size_t size, void* dst);

private:
	uint32_t LogicalBlockSizeFor(uint32_t block_index) const;
	bool     ReadFromBacking(uint64_t off, uint32_t len, void* dst);
	bool     DecodeBlockInto(size_t block_index, void* dst);
	uint8_t* CacheGet(size_t block_index);
	void     CacheInsert(size_t block_index, std::vector<uint8_t>&& data);

	uint64_t m_stream_offset = 0;
	uint32_t m_algo_id      = 0;
	uint64_t m_logical_size = 0;
	uint32_t m_block_count  = 0;
	bool     m_initialized  = false;

	std::vector<uint8_t> m_header;
	std::vector<uint64_t> m_block_offsets; // block_count + 1 entries
	std::vector<BlockInfo> m_blocks;

	// LRU cache of decoded blocks (per-reader; budget-checked on insert).
	// A decoded 64KiB block is worth caching because games re-read TOCs and
	// stream headers far more than random offsets.
	struct CacheKey {
		uint64_t stream_offset;
		size_t   block_index;
		bool     operator==(const CacheKey& o) const {
			return stream_offset == o.stream_offset && block_index == o.block_index;
		}
	};
	struct CacheKeyHash {
		size_t operator()(const CacheKey& k) const {
			return std::hash<uint64_t>()(k.stream_offset) * 0x9E3779B97F4A7C15ULL
			     ^ std::hash<size_t>()(k.block_index);
		}
	};

	static constexpr size_t kCacheBudgetBytes = 256ull << 20; // 256 MiB

	std::unordered_map<CacheKey, std::vector<uint8_t>, CacheKeyHash> m_cache;
	std::vector<CacheKey>                                             m_lru;
	size_t                                                             m_cache_bytes = 0;

	ReadBacking m_read_backing;
};

// ============================================================================
// PfsVolume — runtime mountable PFS volume.
//
// Parses superblock + inode table + dirents at Mount() time, then serves
// on-demand block reads (with PFSC decompression where inodes demand it).
// FileSystem::Mount dispatches pkg/pfs-backed mounts to this so the already
// registered Kernel* syscalls (open/read/pread/lseek/fstat/getdents) can
// serve PFS-backed files instead of host folders only.
// ============================================================================

class PfsVolume {
public:
	// Reads `length` bytes at backing offset into dst. Returns false on I/O error.
	using ReadBacking = PfscBlockReader::ReadBacking;

	struct Options {
		uint32_t algo_id = 0; // PFSC codec id used by this image (0 = zlib stock)
	};

	struct EntryInfo {
		std::string name;     // path relative to volume root, no leading '/'
		uint32_t    inode  = 0;
		uint64_t    size   = 0;
		bool        is_dir = false;
		bool        is_compressed = false;
		int64_t     first_block = 0; // db[0]: first data block (files) / first dir block (dirs)
	};

	PfsVolume()           = default;
	virtual ~PfsVolume()  = default;

	PfsVolume(const PfsVolume&)            = delete;
	PfsVolume& operator=(const PfsVolume&) = delete;

	// Mount from a backing reader over the PFS image. Returns false on parse
	// failure (bad superblock, no root inode, ...).
	bool Mount(ReadBacking read_backing, const Options& options);

	// Path lookup. Accepts volume-relative ('sc0/foo.bin') and rooted
	// ('/sc0/foo.bin') paths. Returns nullptr when not found.
	const EntryInfo* Lookup(const std::string& path) const;

	// All entries (volume-relative paths), breadth as walked at mount.
	const std::vector<EntryInfo>& Entries() const {
		return m_entries;
	}

	// Direct children of a directory path (volume-relative, ''/'.' = root).
	std::vector<EntryInfo> ListDir(const std::string& dir_path) const;

	// On-demand file read through the PFSC block layer when the inode is
	// compressed, plain block reads otherwise. Returns bytes read or -1.
	int64_t ReadFile(const EntryInfo& entry, uint64_t offset, size_t size, void* dst);

	uint32_t BlockSize() const {
		return m_block_size;
	}
	uint32_t NumBlocks() const {
		return m_num_blocks;
	}
	uint32_t NumInodes() const {
		return m_num_inodes;
	}
	uint32_t Version() const {
		return m_version;
	}

	bool IsOk() const {
		return m_mounted;
	}

private:
	// ---- low-level helpers ----
	bool     ReadBlockBacking(uint64_t block_num, std::vector<uint8_t>& out);
	bool     ReadInodeByNumber(uint32_t inode_num, PfsParser::InodeInfo& out);
	bool     ResolveIndirect(const PfsParser::InodeInfo& inode, std::vector<int64_t>& out_blocks);
	bool     ReadDirData(const PfsParser::InodeInfo& dir_inode,
	                     std::vector<std::pair<std::string, uint32_t>>& out);
	bool     ParseDirentsFromBlock(const uint8_t* data, size_t size,
	                               std::vector<std::pair<std::string, uint32_t>>& out);
	bool     ReadFileBlocks(const PfsParser::InodeInfo& inode, uint64_t offset, size_t size, void* dst,
	                        int64_t& bytes_read);
	bool     InitPfscStream(uint32_t inode_number, const PfsParser::InodeInfo& inode,
	                        std::shared_ptr<PfscBlockReader>& out);

	void     BuildEntryListRecursive(const PfsParser::InodeInfo& dir_inode, const std::string& prefix);

	static std::string NormalizePath(const std::string& path);

	ReadBacking m_read_backing;
	Options     m_options;

	uint32_t m_block_size = 0;
	uint32_t m_num_blocks = 0;
	uint32_t m_num_inodes = 0;
	uint32_t m_version    = 0;
	uint32_t m_mode       = 0;
	bool     m_mounted    = false;

	std::vector<EntryInfo> m_entries;

	// inode number -> parsed inode (populated during mount walk; guarded for
	// potential later lazy fills)
	std::unordered_map<uint32_t, PfsParser::InodeInfo> m_inode_cache;
	mutable std::mutex                                  m_inode_cache_mutex;

	// per-inode PFSC stream readers, created on demand
	std::unordered_map<uint32_t, std::shared_ptr<PfscBlockReader>> m_pfsc_readers;
	std::mutex                                                      m_pfsc_readers_mutex;
};

} // namespace Libs::Firmware