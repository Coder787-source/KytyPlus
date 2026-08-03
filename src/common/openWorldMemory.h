#pragma once

#include "common/common.h"
#include "common/virtualMemory.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Common {

/**
 * @brief Enhanced memory manager for large open-world games (GTA V, etc.)
 * 
 * Features:
 * - Pre-allocated memory pools for streaming
 * - Defragmentation support
 * - Large allocation optimization (8GB+ footprint)
 * - Streaming buffer management
 */
namespace OpenWorldMemory {

/**
 * @brief Memory pool configuration for open-world games
 */
struct MemoryPoolConfig {
    uint64_t pool_size;           ///< Total pool size in bytes (default: 10GB)
    uint64_t streaming_buffer;    ///< Dedicated streaming buffer (default: 2GB)
    uint64_t texture_pool;        ///< Texture memory pool (default: 4GB)
    uint64_t script_pool;         ///< Script/memory pool (default: 1GB)
    uint64_t reserved_pool;       ///< Reserved for system (default: 3GB)
    bool     enable_defrag;       ///< Enable automatic defragmentation
    uint32_t defrag_threshold;    ///< Defrag when fragmentation > this % (default: 40)
};

/**
 * @brief Memory allocation statistics
 */
struct MemoryStats {
    uint64_t total_allocated;
    uint64_t total_free;
    uint64_t largest_free_block;
    uint64_t fragmentation_percent;
    uint64_t allocation_count;
    uint64_t streaming_buffer_used;
    uint64_t texture_pool_used;
    uint64_t script_pool_used;
};

/**
 * @brief Memory pool region
 */
struct MemoryRegion {
    uint64_t base_address;
    uint64_t size;
    uint64_t used;
    bool     is_streaming;
    bool     is_texture;
    bool     is_script;
    uint32_t allocation_id;
    
    MemoryRegion() 
        : base_address(0), size(0), used(0), is_streaming(false), 
          is_texture(false), is_script(false), allocation_id(0) {}
    
    MemoryRegion(uint64_t base, uint64_t sz, bool streaming = false, 
                 bool texture = false, bool script = false)
        : base_address(base), size(sz), used(0), is_streaming(streaming),
          is_texture(texture), is_script(script), allocation_id(0) {}
    
    uint64_t free_space() const { return size - used; }
    float    usage_percent() const { return size > 0 ? (float)used / size * 100.0f : 0.0f; }
};

/**
 * @brief Open-world memory manager singleton
 */
class OpenWorldMemoryManager {
public:
    static OpenWorldMemoryManager& Instance() {
        static OpenWorldMemoryManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize memory pools for open-world games
     * @param config Memory pool configuration
     * @return true if initialization successful
     */
    bool Initialize(const MemoryPoolConfig& config = MemoryPoolConfig());
    
    /**
     * @brief Shutdown and free all memory pools
     */
    void Shutdown();
    
    /**
     * @brief Allocate memory from appropriate pool
     * @param size Allocation size in bytes
     * @param is_streaming True if this is for asset streaming
     * @param is_texture True if this is for texture data
     * @param is_script True if this is for script data
     * @return Allocated address or 0 on failure
     */
    uint64_t Allocate(uint64_t size, bool is_streaming = false, 
                      bool is_texture = false, bool is_script = false);
    
    /**
     * @brief Free previously allocated memory
     * @param address Address to free
     * @return true if successful
     */
    bool Free(uint64_t address);
    
    /**
     * @brief Allocate from streaming buffer specifically
     * @param size Size in bytes
     * @return Allocated address or 0 on failure
     */
    uint64_t AllocateStreaming(uint64_t size);
    
    /**
     * @brief Free streaming allocation
     * @param address Address to free
     * @return true if successful
     */
    bool FreeStreaming(uint64_t address);
    
    /**
     * @brief Get memory statistics
     * @return Current memory stats
     */
    MemoryStats GetStats() const;
    
    /**
     * @brief Check if memory manager is initialized
     */
    bool IsInitialized() const { return initialized_; }
    
    /**
     * @brief Get fragmentation percentage
     */
    uint64_t GetFragmentationPercent() const;
    
    /**
     * @brief Trigger manual defragmentation
     * @return true if defragmentation successful
     */
    bool Defragment();
    
    /**
     * @brief Pre-allocate streaming buffer for predictable performance
     * @param size Size to pre-allocate
     * @return true if successful
     */
    bool PreallocateStreamingBuffer(uint64_t size);
    
private:
    OpenWorldMemoryManager() = default;
    ~OpenWorldMemoryManager();
    
    // Prevent copying
    OpenWorldMemoryManager(const OpenWorldMemoryManager&) = delete;
    OpenWorldMemoryManager& operator=(const OpenWorldMemoryManager&) = delete;
    
    /**
     * @brief Find best-fit region for allocation
     */
    MemoryRegion* FindBestFit(uint64_t size, bool is_streaming, bool is_texture, bool is_script);
    
    /**
     * @brief Split region if allocation is smaller than region
     */
    bool SplitRegion(MemoryRegion* region, uint64_t alloc_size);
    
    /**
     * @brief Merge adjacent free regions
     */
    void MergeFreeRegions();
    
    /**
     * @brief Calculate fragmentation
     */
    uint64_t CalculateFragmentation() const;
    
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_ {false};
    
    MemoryPoolConfig config_;
    std::vector<MemoryRegion> regions_;
    
    uint64_t pool_base_address_ = 0;
    uint64_t pool_total_size_ = 0;
    
    std::atomic<uint32_t> next_allocation_id_ {1};
    std::atomic<uint64_t> total_allocations_ {0};
    std::atomic<uint64_t> total_frees_ {0};
};

/**
 * @brief RAII wrapper for streaming allocations
 */
class StreamingAllocationGuard {
public:
    explicit StreamingAllocationGuard(uint64_t size) 
        : address_(OpenWorldMemory::OpenWorldMemoryManager::Instance().AllocateStreaming(size)) {}
    
    ~StreamingAllocationGuard() {
        if (address_ != 0) {
            OpenWorldMemory::OpenWorldMemoryManager::Instance().FreeStreaming(address_);
        }
    }
    
    uint64_t GetAddress() const { return address_; }
    bool IsValid() const { return address_ != 0; }
    
    // Prevent copying
    StreamingAllocationGuard(const StreamingAllocationGuard&) = delete;
    StreamingAllocationGuard& operator=(const StreamingAllocationGuard&) = delete;
    
    // Allow moving
    StreamingAllocationGuard(StreamingAllocationGuard&& other) noexcept 
        : address_(other.address_) {
        other.address_ = 0;
    }
    
    StreamingAllocationGuard& operator=(StreamingAllocationGuard&& other) noexcept {
        if (this != &other) {
            if (address_ != 0) {
                OpenWorldMemory::OpenWorldMemoryManager::Instance().FreeStreaming(address_);
            }
            address_ = other.address_;
            other.address_ = 0;
        }
        return *this;
    }
    
private:
    uint64_t address_;
};

} // namespace OpenWorldMemory

} // namespace Common
