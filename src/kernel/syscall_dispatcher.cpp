#include "syscall_dispatcher.h"
#include "memory.h"
#include "fileSystem.h"
#include <iostream>
#include <iomanip>

namespace Emulator {

SyscallDispatcher::SyscallDispatcher() {
    // We initialize the dispatcher. In a real scenario, we'd load these 
    // from a mapping file or a predefined header of PS5 syscall IDs.
    std::cout << "[Kernel] Syscall Dispatcher initialized. Awaiting handler registration." << std::endl;
}

SceResult SyscallDispatcher::Dispatch(SyscallContext& ctx, uint64_t syscall_id) {
    auto it = syscall_table_.find(syscall_id);
    
    if (it == syscall_table_.end()) {
        LogSyscall(syscall_id, SCE_ERROR);
        // To make games "playable", we often return a generic success or 
        // a specific "Not Implemented" error that the game might ignore.
        return SCE_ERROR; 
    }

    SceResult result = it->second(ctx, syscall_id);
    LogSyscall(syscall_id, result);
    return result;
}

void SyscallDispatcher::RegisterHandler(uint64_t id, SyscallHandler handler) {
    syscall_table_[id] = std::move(handler);
}

void SyscallDispatcher::LogSyscall(uint64_t id, SceResult result) {
    // High-performance logging: in a real build, this would be a ring buffer
    std::cout << "[Syscall] ID: 0x" << std::hex << std::setw(8) << std::setfill('0') << id 
              << " -> Result: " << std::dec << result << std::endl;
}

} // namespace Emulator
