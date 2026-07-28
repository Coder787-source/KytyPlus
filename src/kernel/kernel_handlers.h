#ifndef KYTY_KERNEL_HANDLERS_H
#define KYTY_KERNEL_HANDLERS_H

#include "syscall_dispatcher.h"
#include "memory.h"
#include "fileSystem.h"
#include <vector>
#include <string>

namespace Emulator {

/**
 * @brief Implementation of specific PS5 Kernel handlers to resolve boot crashes.
 */
class KernelHandlers {
public:
    static SceResult HandleAllocateMemory(SyscallContext& ctx, uint64_t size) {
        if (size == 0) return SCE_ERROR;
        
        // Call into the actual MemoryManager
        void* addr = ctx.memory->Allocate(size); 
        if (!addr) return SCE_ERROR;

        // Return the address in RAX
        ctx.regs->rax = reinterpret_cast<uint64_t>(addr);
        return SCE_OK;
    }

    static SceResult HandleCreateThread(SyscallContext& ctx, uint64_t entry_point) {
        std::cout << "[Kernel] Creating guest thread at 0x" << std::hex << entry_point << std::dec << std::endl;
        
        // In a real emulator, this would spawn a new CPUCore instance or a pthread
        // For now, we simulate successful thread creation to allow the game to boot
        ctx.regs->rax = 0x1001; // Mock Thread ID
        return SCE_OK;
    }

    static SceResult HandleFileOpen(SyscallContext& ctx, uint64_t path_ptr) {
        // Extract path from guest memory
        char path[256];
        ctx.memory->Read(path_ptr, path, 256);
        
        std::cout << "[Kernel] Opening file: " << path << std::endl;
        
        // Call into the actual FileSystem
        int fd = ctx.fs->OpenFile(path);
        if (fd < 0) return SCE_ERROR;

        ctx.regs->rax = static_cast<uint64_t>(fd);
        return SCE_OK;
    }

    /**
     * @brief Binds all implemented handlers to the dispatcher.
     */
    static void BindAll(SyscallDispatcher& dispatcher) {
        // These IDs are simulated based on common PS5 SDK patterns
        dispatcher.RegisterHandler(0x100, HandleAllocateMemory);
        dispatcher.RegisterHandler(0x200, HandleCreateThread);
        dispatcher.RegisterHandler(0x300, HandleFileOpen);
    }
};

} // namespace Emulator

#endif // KYTY_KERNEL_HANDLERS_H
