#include "cpu_core.h"
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Emulator {

JITMemoryManager::~JITMemoryManager() {
    for (void* ptr : allocated_pages_) {
#ifdef _WIN32
        VirtualFree(ptr, 0, MEM_RELEASE);
#endif
    }
}

void* JITMemoryManager::AllocateExecutableMemory(size_t size) {
#ifdef _WIN32
    // Allocate memory with READ/WRITE first to load the JIT'd code
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) {
        throw std::runtime_error("Failed to allocate JIT memory via VirtualAlloc");
    }
    allocated_pages_.push_back(ptr);
    return ptr;
#else
    // POSIX implementation would go here (mmap)
    return nullptr;
#endif
}

bool JITMemoryManager::SetMemoryProtection(void* address, size_t size, uint32_t protection) {
#ifdef _WIN32
    DWORD oldProtect;
    // protection would be mapped to PAGE_EXECUTE_READ etc.
    return VirtualProtect(address, size, PAGE_EXECUTE_READ, &oldProtect);
#else
    return false;
#endif
}

CPUCore::CPUCore(std::shared_ptr<JITMemoryManager> jit_mgr) 
    : jit_manager_(std::move(jit_mgr)) {
    std::cout << "[CPU] Core initialized. Ready for JIT mapping." << std::endl;
}

void CPUCore::LoadCodeBlock(const std::vector<uint8_t>& code) {
    size_t size = code.size();
    void* exec_mem = jit_manager_->AllocateExecutableMemory(size);
    
    // Copy translated machine code into the buffer
    std::memcpy(exec_mem, code.data(), size);
    
    // Transition from RW (Read-Write) to RX (Read-Execute) for security/stability
    if (!jit_manager_->SetMemoryProtection(exec_mem, size, 0)) {
        throw std::runtime_error("Failed to set JIT memory to executable");
    }

    current_execution_block_ = exec_mem;
    block_size_ = size;
    
    // Set RIP to the start of the new block
    regs_.rip = reinterpret_cast<uint64_t>(exec_mem);
}

void CPUCore::Step() {
    if (!current_execution_block_) {
        std::cerr << "[CPU] No executable block loaded. CPU Halted." << std::endl;
        return;
    }
    // In a real JIT, we would execute the block and handle the return
    std::cout << "[CPU] Executing block at 0x" << std::hex << regs_.rip << std::dec << std::endl;
}

void CPUCore::Run() {
    while (true) {
        Step();
        // Break loop based on guest state or host signals
        break; 
    }
}

void CPUCore::SetRegister(const std::string& reg, uint64_t value) {
    if (reg == "rax") regs_.rax = value;
    else if (reg == "rbx") regs_.rbx = value;
    else if (reg == "rip") regs_.rip = value;
    // ... other registers
}

uint64_t CPUCore::GetRegister(const std::string& reg) const {
    if (reg == "rax") return regs_.rax;
    if (reg == "rip") return regs_.rip;
    return 0;
}

void CPUCore::HandleException(uint64_t fault_address, uint32_t errorCode) {
    std::cerr << "[CPU] Memory Fault at 0x" << std::hex << fault_address 
              << " Error: " << errorCode << std::dec << std::endl;
}

} // namespace Emulator
