#include "syscall_dispatcher.h"
#include <iostream>

// Note: this file intentionally does NOT include syscall_map.h. syscall_map.h declares its own
// SyscallHandler alias with an incompatible signature
// (std::function<uint64_t(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t)>) from the one declared
// in syscall_dispatcher.h (std::function<SceResult(SyscallContext&, uint64_t)>), and
// SyscallDispatcher::Dispatch's declared signature in the header only takes
// (SyscallContext&, uint64_t) -- it has no way to forward the extra 4 raw register arguments
// SyscallMap::Table's handlers expect. Reconciling the two designs (unifying the handler
// signature so SyscallDispatcher can route through SyscallMap::Table) is a bigger change than a
// build fix should make silently; this keeps SyscallDispatcher self-contained and buildable
// against its own declared contract instead.

namespace Emulator {

SyscallDispatcher::SyscallDispatcher() {
    std::cout << "[Syscall] Dispatcher initialized. Loading syscall table..." << std::endl;
}

SceResult SyscallDispatcher::Dispatch(SyscallContext& ctx, uint64_t syscall_id) {
    auto it = syscall_table_.find(syscall_id);

    if (it != syscall_table_.end()) {
        const SceResult result = it->second(ctx, syscall_id);
        LogSyscall(syscall_id, result);
        return result;
    }

    std::cerr << "[Syscall] Unimplemented syscall ID: 0x" << std::hex << syscall_id << std::dec
               << std::endl;
    LogSyscall(syscall_id, SCE_ERROR);
    return SCE_ERROR;
}

void SyscallDispatcher::RegisterHandler(uint64_t id, SyscallHandler handler) {
    syscall_table_[id] = std::move(handler);
}

void SyscallDispatcher::LogSyscall(uint64_t id, SceResult result) {
    std::cout << "[Syscall] id=0x" << std::hex << id << std::dec << " result=" << result
              << std::endl;
}

} // namespace Emulator
