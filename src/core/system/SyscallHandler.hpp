#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace KytyPS5::Core {

    /**
     * @brief Interface for handling guest kernel requests.
     */
    class ISyscallHandler {
    public:
        virtual ~ISyscallHandler() = default;
        virtual uint64_t HandleSyscall(uint32_t call_id, std::span<uint64_t> args) = 0;
    };

    /**
     * @brief Concrete implementation of the PS5 Kernel interface.
     */
    class KernelInterface : public ISyscallHandler {
    public:
        KernelInterface() {
            // Initialize syscall table
            syscall_table_[0x100] = [this](auto args) { return this->SysPrint(args); };
            syscall_table_[0x200] = [this](auto args) { return this->SysExit(args); };
        }

        uint64_t HandleSyscall(uint32_t call_id, std::span<uint64_t> args) override {
            auto it = syscall_table_.find(call_id);
            if (it != syscall_table_.end()) {
                return it->second(args);
            }
            
            std::cerr << "[Kernel] Unimplemented syscall ID: 0x" << std::hex << call_id << std::dec << std::endl;
            return 0xFFFFFFFFFFFFFFFF; // Return error code
        }

    private:
        uint64_t SysPrint(std::span<uint64_t> args) {
            // Simplified print implementation
            const char* msg = reinterpret_cast<const char*>(args[0]);
            std::cout << "[Guest Output] " << msg << std::endl;
            return 0;
        }

        uint64_t SysExit(std::span<uint64_t> args) {
            std::cout << "[Kernel] Guest requested exit." << std::endl;
            // Signal the emulator loop to terminate
            return 0;
        }

        using SyscallFunc = std::function<uint64_t(std::span<uint64_t>)>;
        std::unordered_map<uint32_t, SyscallFunc> syscall_table_;
    };

}
