#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <iostream>

namespace Emulator {

struct MemoryRegion {
    uint64_t start;
    uint64_t size;
    bool executable;
};

class KernelServices {
public:
    static KernelServices& Instance() {
        static KernelServices instance;
        return instance;
    }

    // --- Memory Management (Mocks for SceKernel) ---
    uint64_t AllocateMemory(uint64_t size, uint32_t flags);
    bool FreeMemory(uint64_t address);
    bool MapMemory(uint64_t address, uint64_t size, uint32_t prot);

    // --- Threading (Mocks for SceKernel/Pthreads) ---
    uint64_t CreateThread(void* start_routine, void* arg);
    int JoinThread(uint64_t thread_id);
    void Yield();

    // --- System Information ---
    uint32_t GetProcessorCount() { return 8; } // PS5 has 8 Zen 2 cores
    uint64_t GetTotalMemory() { return 16ULL * 1024 * 1024 * 1024; } // 16GB GDDR6

private:
    KernelServices() = default;
    std::mutex mem_mutex_;
    std::vector<MemoryRegion> allocations_;
    std::unordered_map<uint64_t, std::thread> active_threads_;
    uint64_t next_thread_id_ = 1;
};

} // namespace Emulator
