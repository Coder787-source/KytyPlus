#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace KytyPS5::Core {

    using GuestAddr = uint64_t;
    using HostAddr  = uint64_t;

    /**
     * @brief Represents a single translated block of guest code.
     */
    struct TranslationBlock {
        GuestAddr guest_pc;
        void* host_code_ptr;
        size_t code_size;

        TranslationBlock(GuestAddr pc, void* ptr, size_t size) 
            : guest_pc(pc), host_code_ptr(ptr), code_size(size) {}
    };

    /**
     * @brief Manages the mapping between Guest PCs and Host machine code.
     * Implements a Page-aligned executable memory pool to avoid frequent syscalls.
     */
    class ICache {
    public:
        explicit ICache(size_t initial_pool_size = 1024 * 1024 * 64) // 64MB default
            : pool_size_(initial_pool_size), pool_offset_(0) {
            AllocatePool();
        }

        ~ICache() {
            if (exec_pool_) {
#if defined(_WIN32)
                VirtualFree(exec_pool_, 0, MEM_RELEASE);
#endif
            }
        }

        // Non-copyable, non-movable
        ICache(const ICache&) = delete;
        ICache& operator=(const ICache&) = delete;

        /**
         * @brief Lookup a translated block for a given guest address.
         * @return Host address of the executable code, or 0 if not cached.
         */
        HostAddr Lookup(GuestAddr pc) {
            std::shared_lock lock(cache_mutex_);
            auto it = cache_map_.find(pc);
            return (it != cache_map_.end()) ? reinterpret_cast<HostAddr>(it->second) : 0;
        }

        /**
         * @brief Caches a newly translated block of code.
         * @param pc The guest starting address.
         * @param code The compiled host machine code bytes.
         * @return The host address where the code was placed.
         */
        HostAddr Insert(GuestAddr pc, const std::vector<uint8_t>& code) {
            std::unique_lock lock(cache_mutex_);

            if (cache_map_.count(pc)) return reinterpret_cast<HostAddr>(cache_map_[pc]);

            size_t size = code.size();
            if (pool_offset_ + size > pool_size_) {
                std::cerr << "ICache: Executable pool exhausted. Needs expansion.\n";
                return 0;
            }

            uint8_t* destination = static_cast<uint8_t*>(exec_pool_) + pool_offset_;
            std::memcpy(destination, code.data(), size);
            
            pool_offset_ += size;
            
            // Align to 8 bytes for performance
            pool_offset_ = (pool_offset_ + 7) & ~7;

            cache_map_[pc] = destination;
            return reinterpret_cast<HostAddr>(destination);
        }

        void Flush() {
            std::unique_lock lock(cache_mutex_);
            cache_map_.clear();
            pool_offset_ = 0;
            // In a real scenario, we might want to re-allocate or defragment here.
        }

    private:
        void AllocatePool() {
#if defined(_WIN32)
            exec_pool_ = VirtualAlloc(nullptr, pool_size_, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!exec_pool_) throw std::bad_alloc();
#else
            // POSIX implementation would use mmap with PROT_EXEC | PROT_WRITE
            exec_pool_ = nullptr; 
#endif
        }

        void* exec_pool_ = nullptr;
        size_t pool_size_;
        size_t pool_offset_;
        
        std::unordered_map<GuestAddr, void*> cache_map_;
        std::shared_mutex cache_mutex_;
    };

}
