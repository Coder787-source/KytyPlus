#ifndef KYTY_CPU_CORE_H
#define KYTY_CPU_CORE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

namespace Emulator {

/**
 * @brief Represents the architectural state of the Guest CPU.
 * Uses C++20 aligned structures for performance and precise mapping.
 */
struct alignas(16) GuestRegisters {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
};

/**
 * @brief JITMemoryManager handles the allocation of executable memory pages.
 * Implements RAII to ensure memory is freed and protections are reset.
 */
class JITMemoryManager {
public:
    JITMemoryManager() = default;
    ~JITMemoryManager();

    // Prevent copying to avoid double-free of executable memory
    JITMemoryManager(const JITMemoryManager&) = delete;
    JITMemoryManager& operator=(const JITMemoryManager&) = delete;

    /**
     * @brief Allocates a block of memory and marks it as executable.
     * @param size Size of the block in bytes.
     * @return Pointer to the allocated executable memory.
     */
    void* AllocateExecutableMemory(size_t size);

    /**
     * @brief Changes protection of a memory region.
     */
    bool SetMemoryProtection(void* address, size_t size, uint32_t protection);

private:
    std::vector<void*> allocated_pages_;
};

/**
 * @brief CPUCore manages the execution loop and the interface between 
 * guest instructions and the host CPU.
 */
class CPUCore {
public:
    CPUCore() : CPUCore(std::make_shared<JITMemoryManager>()) {}
    explicit CPUCore(std::shared_ptr<JITMemoryManager> jit_mgr);
    ~CPUCore() = default;

    void Step();
    void Run();
    
    void SetRegister(const std::string& reg, uint64_t value);
    uint64_t GetRegister(const std::string& reg) const;

    // Interface for the JIT compiler to push translated blocks
    void LoadCodeBlock(const std::vector<uint8_t>& code);

private:
    GuestRegisters regs_{};
    std::shared_ptr<JITMemoryManager> jit_manager_;
    void* current_execution_block_ = nullptr;
    size_t block_size_ = 0;

    void HandleException(uint64_t fault_address, uint32_t errorCode);
};

} // namespace Emulator

#endif // KYTY_CPU_CORE_H
