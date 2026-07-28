#ifndef KYTY_SYSCALL_DISPATCHER_H
#define KYTY_SYSCALL_DISPATCHER_H

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace Emulator {

/**
 * @brief The result of a guest syscall execution.
 * PS5 syscalls typically return a status code (SceResult).
 */
using SceResult = int32_t;
constexpr SceResult SCE_OK = 0;
constexpr SceResult SCE_ERROR = -1;

/**
 * @brief Context passed to every syscall handler.
 * Provides access to the CPU state and the kernel's internal subsystems.
 */
struct SyscallContext {
    struct GuestRegisters* regs;
    class MemoryManager* memory;
    class FileSystem* fs;
};

/**
 * @brief Function signature for all kernel syscall handlers.
 */
using SyscallHandler = std::function<SceResult(SyscallContext&, uint64_t)>;

class SyscallDispatcher {
public:
    SyscallDispatcher();
    ~SyscallDispatcher() = default;

    /**
     * @brief Dispatches a syscall by its ID.
     * @return The result of the syscall or SCE_ERROR if not implemented.
     */
    SceResult Dispatch(SyscallContext& ctx, uint64_t syscall_id);

    /**
     * @brief Registers a handler for a specific syscall ID.
     */
    void RegisterHandler(uint64_t id, SyscallHandler handler);

private:
    std::unordered_map<uint64_t, SyscallHandler> syscall_table_;
    
    // Internal helper to log syscall activity for debugging
    void LogSyscall(uint64_t id, SceResult result);
};

} // namespace Emulator

#endif // KYTY_SYSCALL_DISPATCHER_H
