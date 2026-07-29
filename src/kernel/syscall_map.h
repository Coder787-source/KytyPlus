#pragma once
#include <cstdint>
#include <unordered_map>
#include <functional>

namespace Emulator {

// PS5 Syscall IDs (Estimated based on common PS5 kernel patterns)
enum class SyscallID : uint64_t {
    SceKernelAllocateMemory = 0x100,
    SceKernelFreeMemory = 0x101,
    SceKernelCreateThread = 0x200,
    SceKernelJoinThread = 0x201,
    SceKernelGetProcessorCount = 0x300,
    SceKernelGetTotalMemory = 0x301,
    SceKernelYield = 0x400,
    SceKernelMapMemory = 0x102
};

using SyscallHandler = std::function<uint64_t(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)>;

struct SyscallMap {
    static const std::unordered_map<uint64_t, SyscallHandler> Table;
};

} // namespace Emulator
