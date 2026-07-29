#include "kernel_services.h"
#include <iostream>
#include <cstdlib>

namespace Emulator {

uint64_t KernelServices::AllocateMemory(uint64_t size, uint32_t flags) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    
    std::cout << "[Kernel] Allocating " << size << " bytes (Flags: " << flags << ")" << std::endl;
    
    // In a real emulator, we would manage a virtual address space.
    // Here, we use host malloc and treat the pointer as the guest address.
    void* ptr = std::malloc(size);
    if (!ptr) return 0;

    uint64_t addr = reinterpret_cast<uint64_t>(ptr);
    allocations_.push_back({addr, size, (flags & 0x4) != 0}); // Assume 0x4 is EXECUTE flag
    
    return addr;
}

bool KernelServices::FreeMemory(uint64_t address) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    
    for (auto it = allocations_.begin(); it != allocations_.end(); ++it) {
        if (it->start == address) {
            std::free(reinterpret_cast<void*>(address));
            allocations_.erase(it);
            return true;
        }
    }
    return false;
}

bool KernelServices::MapMemory(uint64_t address, uint64_t size, uint32_t prot) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    std::cout << "[Kernel] Mapping memory at 0x" << std::hex << address << " with prot " << prot << std::dec << std::endl;
    // For a semi-playable state, we simulate a successful mapping.
    return true;
}

uint64_t KernelServices::CreateThread(void* start_routine, void* arg) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    
    uint64_t tid = next_thread_id_++;
    std::cout << "[Kernel] Creating Thread ID: " << tid << std::endl;

    // Wrap the guest routine in a host thread
    active_threads_.emplace(tid, std::thread([start_routine, arg]() {
        // This is a simplified guest-to-host thread execution
        typedef void (*ThreadRoutine)(void*);
        auto routine = reinterpret_cast<ThreadRoutine>(start_routine);
        routine(arg);
    }));

    return tid;
}

int KernelServices::JoinThread(uint64_t thread_id) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    
    auto it = active_threads_.find(thread_id);
    if (it != active_threads_.end()) {
        if (it->second.joinable()) {
            it->second.join();
        }
        active_threads_.erase(it);
        return 0; // Success
    }
    return -1; // Error: Thread not found
}

void KernelServices::Yield() {
    std::this_thread::yield();
}

} // namespace Emulator
