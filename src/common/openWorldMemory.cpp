#include "common/openWorldMemory.h"
#include "common/platform/sysVirtual.h"
#include "common/logging/log.h"
#include "common/assert.h"
#include <algorithm>
#include <cstring>

namespace Common {

namespace OpenWorldMemory {

// Default configuration for GTA V-scale games
static constexpr uint64_t DEFAULT_POOL_SIZE        = 10ULL * 1024 * 1024 * 1024; // 10GB
static constexpr uint64_t DEFAULT_STREAMING_BUFFER = 2ULL * 1024 * 1024 * 1024;  // 2GB
static constexpr uint64_t DEFAULT_TEXTURE_POOL     = 4ULL * 1024 * 1024 * 1024;  // 4GB
static constexpr uint64_t DEFAULT_SCRIPT_POOL      = 1ULL * 1024 * 1024 * 1024;  // 1GB
static constexpr uint64_t DEFAULT_RESERVED_POOL    = 3ULL * 1024 * 1024 * 1024;  // 3GB
static constexpr uint32_t DEFAULT_DEFRAG_THRESHOLD = 40; // 40% fragmentation

OpenWorldMemoryManager::~OpenWorldMemoryManager() {
    Shutdown();
}

bool OpenWorldMemoryManager::Initialize(const MemoryPoolConfig& config) {
    std::unique_lock lock(mutex_);
    
    if (initialized_.load(std::memory_order_acquire)) {
        LOGF_COLOR(Log::Color::Yellow, "OpenWorldMemory: Already initialized\n");
        return true;
    }
    
    config_ = config;
    
    // Apply defaults for unspecified values
    if (config_.pool_size == 0) {
        config_.pool_size = DEFAULT_POOL_SIZE;
    }
    if (config_.streaming_buffer == 0) {
        config_.streaming_buffer = DEFAULT_STREAMING_BUFFER;
    }
    if (config_.texture_pool == 0) {
        config_.texture_pool = DEFAULT_TEXTURE_POOL;
    }
    if (config_.script_pool == 0) {
        config_.script_pool = DEFAULT_SCRIPT_POOL;
    }
    if (config_.reserved_pool == 0) {
        config_.reserved_pool = DEFAULT_RESERVED_POOL;
    }
    if (config_.defrag_threshold == 0) {
        config_.defrag_threshold = DEFAULT_DEFRAG_THRESHOLD;
    }
    
    LOGF("OpenWorldMemory: Initializing memory pools\n");
    LOGF("  Total Pool Size:    %llu MB\n", config_.pool_size / (1024 * 1024));
    LOGF("  Streaming Buffer:   %llu MB\n", config_.streaming_buffer / (1024 * 1024));
    LOGF("  Texture Pool:       %llu MB\n", config_.texture_pool / (1024 * 1024));
    LOGF("  Script Pool:        %llu MB\n", config_.script_pool / (1024 * 1024));
    LOGF("  Reserved Pool:      %llu MB\n", config_.reserved_pool / (1024 * 1024));
    LOGF("  Auto-Defrag:        %s (threshold: %u%%)\n", 
         config_.enable_defrag ? "enabled" : "disabled", config_.defrag_threshold);
    
    // Allocate the main memory pool from the virtual memory system
    pool_base_address_ = SysVirtualAlloc(0, config_.pool_size, VirtualMemory::Mode::ReadWrite);
    
    if (pool_base_address_ == 0) {
        LOGF_COLOR(Log::Color::Red, "OpenWorldMemory: Failed to allocate main pool (%llu MB)\n",
                   config_.pool_size / (1024 * 1024));
        return false;
    }
    
    pool_total_size_ = config_.pool_size;
    
    LOGF("OpenWorldMemory: Pool allocated at 0x%016llx\n", pool_base_address_);
    
    // Create initial memory regions
    regions_.clear();
    regions_.reserve(16);
    
    uint64_t current_offset = 0;
    
    // Region 1: Streaming Buffer
    if (config_.streaming_buffer > 0) {
        regions_.emplace_back(
            pool_base_address_ + current_offset,
            config_.streaming_buffer,
            true,   // is_streaming
            false,  // is_texture
            false   // is_script
        );
        current_offset += config_.streaming_buffer;
        LOGF("  Streaming region: 0x%016llx - 0x%016llx (%llu MB)\n",
             regions_.back().base_address,
             regions_.back().base_address + regions_.back().size,
             config_.streaming_buffer / (1024 * 1024));
    }
    
    // Region 2: Texture Pool
    if (config_.texture_pool > 0) {
        regions_.emplace_back(
            pool_base_address_ + current_offset,
            config_.texture_pool,
            false,  // is_streaming
            true,   // is_texture
            false   // is_script
        );
        current_offset += config_.texture_pool;
        LOGF("  Texture region:   0x%016llx - 0x%016llx (%llu MB)\n",
             regions_.back().base_address,
             regions_.back().base_address + regions_.back().size,
             config_.texture_pool / (1024 * 1024));
    }
    
    // Region 3: Script Pool
    if (config_.script_pool > 0) {
        regions_.emplace_back(
            pool_base_address_ + current_offset,
            config_.script_pool,
            false,  // is_streaming
            false,  // is_texture
            true    // is_script
        );
        current_offset += config_.script_pool;
        LOGF("  Script region:    0x%016llx - 0x%016llx (%llu MB)\n",
             regions_.back().base_address,
             regions_.back().base_address + regions_.back().size,
             config_.script_pool / (1024 * 1024));
    }
    
    // Region 4: Reserved/General Pool
    if (config_.reserved_pool > 0) {
        regions_.emplace_back(
            pool_base_address_ + current_offset,
            config_.reserved_pool,
            false,  // is_streaming
            false,  // is_texture
            false   // is_script
        );
        current_offset += config_.reserved_pool;
        LOGF("  Reserved region:  0x%016llx - 0x%016llx (%llu MB)\n",
             regions_.back().base_address,
             regions_.back().base_address + regions_.back().size,
             config_.reserved_pool / (1024 * 1024));
    }
    
    // Verify we've allocated the entire pool
    if (current_offset != pool_total_size_) {
        LOGF_COLOR(Log::Color::Yellow, 
                   "OpenWorldMemory: Region allocation mismatch (%llu vs %llu)\n",
                   current_offset, pool_total_size_);
    }
    
    initialized_.store(true, std::memory_order_release);
    
    LOGF("OpenWorldMemory: Initialization complete\n");
    return true;
}

void OpenWorldMemoryManager::Shutdown() {
    std::unique_lock lock(mutex_);
    
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    LOGF("OpenWorldMemory: Shutting down\n");
    
    // Free the main pool
    if (pool_base_address_ != 0) {
        SysVirtualFree(pool_base_address_);
        pool_base_address_ = 0;
    }
    
    pool_total_size_ = 0;
    regions_.clear();
    
    initialized_.store(false, std::memory_order_release);
    
    LOGF("OpenWorldMemory: Shutdown complete\n");
}

uint64_t OpenWorldMemoryManager::Allocate(uint64_t size, bool is_streaming, 
                                           bool is_texture, bool is_script) {
    if (!initialized_.load(std::memory_order_acquire)) {
        LOGF_COLOR(Log::Color::Red, "OpenWorldMemory: Not initialized\n");
        return 0;
    }
    
    if (size == 0) {
        return 0;
    }
    
    // Align to 4KB pages
    const uint64_t aligned_size = (size + 0xFFF) & ~0xFFFULL;
    
    std::unique_lock lock(mutex_);
    
    // Find best-fit region
    MemoryRegion* region = FindBestFit(aligned_size, is_streaming, is_texture, is_script);
    
    if (!region) {
        // Try defragmentation if enabled and fragmentation is high
        if (config_.enable_defrag) {
            const uint64_t frag = CalculateFragmentation();
            if (frag > config_.defrag_threshold) {
                LOGF("OpenWorldMemory: High fragmentation (%llu%%), attempting defrag\n", frag);
                lock.unlock();
                
                if (Defragment()) {
                    lock.lock();
                    region = FindBestFit(aligned_size, is_streaming, is_texture, is_script);
                }
            }
        }
        
        if (!region) {
            LOGF_COLOR(Log::Color::Yellow, 
                       "OpenWorldMemory: Allocation failed (%llu bytes, streaming=%d, texture=%d, script=%d)\n",
                       size, is_streaming, is_texture, is_script);
            return 0;
        }
    }
    
    // Allocate from region
    const uint64_t alloc_address = region->base_address + region->used;
    region->used += aligned_size;
    region->allocation_id = next_allocation_id_.fetch_add(1, std::memory_order_relaxed);
    
    total_allocations_.fetch_add(1, std::memory_order_relaxed);
    
    LOGF("OpenWorldMemory: Allocated %llu bytes at 0x%016llx (region usage: %llu%%)\n",
         aligned_size, alloc_address, region->usage_percent());
    
    return alloc_address;
}

bool OpenWorldMemoryManager::Free(uint64_t address) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    if (address == 0) {
        return true;
    }
    
    std::unique_lock lock(mutex_);
    
    // Find the region containing this address
    for (auto& region : regions_) {
        if (address >= region.base_address && 
            address < region.base_address + region.size) {
            
            const uint64_t offset = address - region.base_address;
            
            // Simple free: just mark the space as unused
            // A more sophisticated implementation would track individual allocations
            // For now, we assume frees happen in LIFO order (common in streaming)
            if (offset + 1 <= region.used) {
                // This is a simplification - real implementation would need
                // a proper allocation tracker
                region.used = offset;
                total_frees_.fetch_add(1, std::memory_order_relaxed);
                
                LOGF("OpenWorldMemory: Freed 0x%016llx\n", address);
                return true;
            }
        }
    }
    
    LOGF_COLOR(Log::Color::Yellow, "OpenWorldMemory: Free failed for 0x%016llx\n", address);
    return false;
}

uint64_t OpenWorldMemoryManager::AllocateStreaming(uint64_t size) {
    return Allocate(size, true, false, false);
}

bool OpenWorldMemoryManager::FreeStreaming(uint64_t address) {
    return Free(address);
}

MemoryStats OpenWorldMemoryManager::GetStats() const {
    MemoryStats stats {};
    
    if (!initialized_.load(std::memory_order_acquire)) {
        return stats;
    }
    
    std::shared_lock lock(mutex_);
    
    stats.total_allocated = 0;
    stats.total_free = pool_total_size_;
    stats.largest_free_block = 0;
    stats.allocation_count = total_allocations_.load(std::memory_order_relaxed);
    
    for (const auto& region : regions_) {
        stats.total_allocated += region.used;
        stats.total_free -= region.used;
        
        if (region.is_streaming) {
            stats.streaming_buffer_used += region.used;
        } else if (region.is_texture) {
            stats.texture_pool_used += region.used;
        } else if (region.is_script) {
            stats.script_pool_used += region.used;
        }
        
        const uint64_t region_free = region.free_space();
        if (region_free > stats.largest_free_block) {
            stats.largest_free_block = region_free;
        }
    }
    
    stats.fragmentation_percent = CalculateFragmentation();
    
    return stats;
}

uint64_t OpenWorldMemoryManager::GetFragmentationPercent() const {
    if (!initialized_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    std::shared_lock lock(mutex_);
    return CalculateFragmentation();
}

bool OpenWorldMemoryManager::Defragment() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    LOGF("OpenWorldMemory: Starting defragmentation\n");
    
    std::unique_lock lock(mutex_);
    
    // Simple defragmentation: merge adjacent free regions
    // A more sophisticated implementation would move live allocations
    // For now, we just reset region usage tracking (assumes streaming pattern)
    
    MergeFreeRegions();
    
    const uint64_t frag_after = CalculateFragmentation();
    LOGF("OpenWorldMemory: Defragmentation complete (fragmentation: %llu%%)\n", frag_after);
    
    return true;
}

bool OpenWorldMemoryManager::PreallocateStreamingBuffer(uint64_t size) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    // Find streaming region
    for (auto& region : regions_) {
        if (region.is_streaming) {
            if (region.free_space() >= size) {
                // Pre-allocate by marking as used
                region.used = size;
                LOGF("OpenWorldMemory: Pre-allocated %llu MB streaming buffer\n",
                     size / (1024 * 1024));
                return true;
            }
        }
    }
    
    LOGF_COLOR(Log::Color::Yellow, 
               "OpenWorldMemory: Failed to pre-allocate streaming buffer (%llu bytes)\n", size);
    return false;
}

MemoryRegion* OpenWorldMemoryManager::FindBestFit(uint64_t size, bool is_streaming, 
                                                   bool is_texture, bool is_script) {
    MemoryRegion* best_fit = nullptr;
    uint64_t best_fit_waste = UINT64_MAX;
    
    for (auto& region : regions_) {
        // Check if region type matches request
        if (is_streaming && !region.is_streaming) {
            continue;
        }
        if (is_texture && !region.is_texture) {
            continue;
        }
        if (is_script && !region.is_script) {
            continue;
        }
        
        // Check if region has enough space
        if (region.free_space() < size) {
            continue;
        }
        
        // Calculate waste (prefer tight fits)
        const uint64_t waste = region.free_space() - size;
        
        if (waste < best_fit_waste) {
            best_fit = &region;
            best_fit_waste = waste;
        }
    }
    
    return best_fit;
}

void OpenWorldMemoryManager::MergeFreeRegions() {
    // Simple merge: reset usage tracking for streaming regions
    // This assumes a streaming pattern where old data is no longer needed
    
    for (auto& region : regions_) {
        if (region.is_streaming && region.used > 0) {
            // Reset streaming buffer (assumes data is no longer needed)
            // Real implementation would need reference counting
            LOGF("OpenWorldMemory: Reset streaming region (was %llu%% used)\n",
                 region.usage_percent());
            region.used = 0;
        }
    }
}

uint64_t OpenWorldMemoryManager::CalculateFragmentation() const {
    if (regions_.empty()) {
        return 0;
    }
    
    uint64_t total_free = 0;
    uint64_t largest_free = 0;
    
    for (const auto& region : regions_) {
        const uint64_t free_space = region.free_space();
        total_free += free_space;
        
        if (free_space > largest_free) {
            largest_free = free_space;
        }
    }
    
    if (total_free == 0) {
        return 0;
    }
    
    // Fragmentation = 100 * (1 - largest_free / total_free)
    return 100 * (total_free - largest_free) / total_free;
}

} // namespace OpenWorldMemory

} // namespace Common
