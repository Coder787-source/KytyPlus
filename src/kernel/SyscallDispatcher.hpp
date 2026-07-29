#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include "kyty_expected.hpp"
#include <unordered_map>
#include <functional>
#include <span>
#include <mutex>
#include <shared_mutex>

namespace KytyPS5::Kernel {

    enum class SyscallError {
        InvalidAddress,
        PermissionDenied,
        OutOfMemory,
        UnsupportedCall,
        DeviceTimeout
    };

    using GuestAddr = uint64_t;
    using HostPtr = void*;

    struct MemoryRegion {
        GuestAddr start;
        size_t size;
        uint32_t permissions;
    };

    class MemoryManager {
    public:
        static MemoryManager& Instance() {
            static MemoryManager instance;
            return instance;
        }

        std::expected<HostPtr, SyscallError> MapRegion(GuestAddr guest_addr, size_t size, uint32_t perms) {
            std::unique_lock lock(mutex_);
            if (guest_addr % 4096 != 0) return std::unexpected(SyscallError::InvalidAddress);

            void* host_ptr = std::malloc(size);
            if (!host_ptr) return std::unexpected(SyscallError::OutOfMemory);

            regions_.push_back({guest_addr, size, perms});
            address_map_[guest_addr] = host_ptr;
            return host_ptr;
        }

        std::expected<HostPtr, SyscallError> Translate(GuestAddr guest_addr) {
            std::shared_lock lock(mutex_);
            for (const auto& [base, ptr] : address_map_) {
                if (guest_addr >= base && guest_addr < base + 0x100000000) { 
                    return static_cast<HostPtr>(static_cast<uint8_t*>(ptr) + (guest_addr - base));
                }
            }
            return std::unexpected(SyscallError::InvalidAddress);
        }

    private:
        MemoryManager() = default;
        std::shared_mutex mutex_;
        std::vector<MemoryRegion> regions_;
        std::unordered_map<GuestAddr, HostPtr> address_map_;
    };

    class SyscallDispatcher {
    public:
        using SyscallHandler = std::function<std::expected<uint64_t, SyscallError>(uint64_t, uint64_t, uint64_t)>;

        SyscallDispatcher() {
            RegisterHandlers();
        }

        std::expected<uint64_t, SyscallError> Execute(uint32_t call_id, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
            if (!handlers_.contains(call_id)) {
                return std::unexpected(SyscallError::UnsupportedCall);
            }
            return handlers_[call_id](arg1, arg2, arg3);
        }

    private:
        void RegisterHandlers() {
            handlers_[0x101] = [this](uint64_t size, uint64_t perms, uint64_t) -> std::expected<uint64_t, SyscallError> {
                auto result = MemoryManager::Instance().MapRegion(0x1000000, size, perms);
                if (!result) return std::unexpected(result.error());
                return 0x1000000;
            };

            handlers_[0x202] = [this](uint64_t buffer_addr, uint64_t size, uint64_t) -> std::expected<uint64_t, SyscallError> {
                auto host_ptr = MemoryManager::Instance().Translate(buffer_addr);
                if (!host_ptr) return std::unexpected(host_ptr.error());
                return 0;
            };

            handlers_[0x303] = [](uint64_t, uint64_t, uint64_t) -> std::expected<uint64_t, SyscallError> {
                return 0;
            };
        }

        std::unordered_map<uint32_t, SyscallHandler> handlers_;
    };

}
