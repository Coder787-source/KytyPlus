#include "syscall_map.h"
#include "kernel_services.h"

namespace Emulator {

const std::unordered_map<uint64_t, SyscallHandler> SyscallMap::Table = {
    { (uint64_t)SyscallID::SceKernelAllocateMemory, [](uint64_t s, uint64_t f, uint64_t, uint64_t, uint64_t) {
        return KernelServices::Instance().AllocateMemory(s, (uint32_t)f);
    }},
    { (uint64_t)SyscallID::SceKernelFreeMemory, [](uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) {
        return KernelServices::Instance().FreeMemory(a) ? 0 : 1;
    }},
    { (uint64_t)SyscallID::SceKernelCreateThread, [](uint64_t r, uint64_t a, uint64_t, uint64_t, uint64_t) {
        return KernelServices::Instance().CreateThread(reinterpret_cast<void*>(r), reinterpret_cast<void*>(a));
    }},
    { (uint64_t)SyscallID::SceKernelJoinThread, [](uint64_t tid, uint64_t, uint64_t, uint64_t, uint64_t) {
        return (uint64_t)KernelServices::Instance().JoinThread(tid);
    }},
    { (uint64_t)SyscallID::SceKernelGetProcessorCount, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        return KernelServices::Instance().GetProcessorCount();
    }},
    { (uint64_t)SyscallID::SceKernelGetTotalMemory, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        return KernelServices::Instance().GetTotalMemory();
    }},
    { (uint64_t)SyscallID::SceKernelYield, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        KernelServices::Instance().Yield();
        return 0;
    }},
    { (uint64_t)SyscallID::SceKernelMapMemory, [](uint64_t a, uint64_t s, uint64_t p, uint64_t, uint64_t) {
        return KernelServices::Instance().MapMemory(a, s, (uint32_t)p) ? 0 : 1;
    }}
};

} // namespace Emulator
