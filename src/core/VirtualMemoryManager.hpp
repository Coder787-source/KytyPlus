#pragma once

#include <unordered_map>
#include <mutex>
#include "kyty_expected.hpp"
#include <vector>
#include <iostream>
#include <windows.h>

namespace KytyPS5::Core {

    /**
     * @brief VirtualMemoryManager handles GVA to HVA translation and page protections.
     * Commercial games rely on strict NX (No-Execute) and Read-Only pages to prevent exploits.
     */
    class VirtualMemoryManager {
    public:
        static constexpr uint64_t GUEST_BASE_OFFSET = 0x800000000000; // High-half kernel/user split

        enum class MemoryProtection {
            ReadOnly = PAGE_READONLY,
            ReadWrite = PAGE_READWRITE,
            ReadExecute = PAGE_EXECUTE_READ,
            All = PAGE_EXECUTE_READWRITE
        };

        /**
         * @brief Map a region of guest memory to host memory.
         */
        kyty::expected<void*, std::string> MapRegion(uint64_t guest_addr, size_t size, MemoryProtection prot) {
            std::lock_guard lock(mutex_);

            if (guest_addr % 4096 != 0) return kyty::unexpected("Address not page-aligned");

            // Using VirtualAlloc to ensure the host OS manages these pages with proper DEP/NX
            void* host_ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, static_cast<DWORD>(prot));
            
            if (!host_ptr) {
                return kyty::unexpected("Host VirtualAlloc failed");
            }

            std::lock_guard mapping_lock(mapping_mutex_);
            mappings_[guest_addr] = { host_ptr, size };

            return host_ptr;
        }

        /**
         * @brief Resolves a Guest Virtual Address (GVA) to a Host Virtual Address (HVA).
         * This is the hot path for every LDR/STR operation.
         */
        void* Resolve(uint64_t guest_addr) {
            // Fast path: check if it falls within the primary guest offset
            // In a production build, this would use a TLB (Translation Lookaside Buffer)
            
            std::lock_guard lock(mapping_mutex_);
            
            // Find the range containing the guest_addr
            for (auto const& [base, info] : mappings_) {
                if (guest_addr >= base && guest_addr < base + info.size) {
                    return static_cast<uint8_t*>(info.ptr) + (guest_addr - base);
                }
            }

            return nullptr; // Page Fault
        }

        void Unmap(uint64_t guest_addr) {
            std::lock_guard lock(mapping_mutex_);
            if (auto it = mappings_.find(guest_addr); it != mappings_.end()) {
                VirtualFree(it->second.ptr, 0, MEM_RELEASE);
                mappings_.erase(it);
            }
        }

    private:
        struct MappingInfo {
            void* ptr;
            size_t size;
        };

        std::mutex mutex_;
        std::mutex mapping_mutex_;
        std::unordered_map<uint64_t, MappingInfo> mappings_;
    };

}
