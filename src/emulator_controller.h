#ifndef KYTY_EMULATOR_CONTROLLER_H
#define KYTY_EMULATOR_CONTROLLER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <iostream>

// Core components
#include "cpu_core.h"
#include "memory.h"
#include "syscall_dispatcher.h"
#include "gpu_translator.h"
#include "interrupt_controller.h"
#include "loader/elf_unpacker.h"
#include "loader/binary_decryption.h"

namespace Emulator {

/**
 * @brief EmulatorController is the orchestrator that connects the GUI to the backend.
 * This class manages the lifecycle of the emulation session.
 */
class EmulatorController {
public:
    EmulatorController() : is_running_(false) {}
    ~EmulatorController() { Stop(); }

    /**
     * @brief Initializes all subsystems and boots the game.
     * This is the function the GUI "Start" button should call.
     */
    bool Start(const std::string& gamePath, const std::vector<uint8_t>& userKeys) {
        std::cout << "[Controller] Booting game: " << gamePath << std::endl;

        try {
            // 1. Initialize Backend Systems
            mem_manager_ = std::make_unique<MemoryManager>();
            interrupt_ctrl_ = std::make_unique<InterruptController>();
            gpu_translator_ = std::make_unique<GPUTranslator>();
            
            // 2. Setup Syscall Dispatcher and link it to the kernel handlers
            syscall_dispatcher_ = std::make_unique<SyscallDispatcher>();
            
            // 3. Load and Decrypt Binary
            BinaryDecryption decryptor;
            // In a real scenario, read file from gamePath into ciphertext
            std::vector<uint8_t> ciphertext = { /* Read from disk */ }; 
            std::vector<uint8_t> plaintext = decryptor.Decrypt(ciphertext, userKeys);

            // 4. Unpack ELF to Memory
            ElfUnpacker unpacker(mem_manager_.get());
            if (!unpacker.MapBinaryToMemory(plaintext)) {
                std::cerr << "[Controller] Failed to map binary to memory." << std::endl;
                return false;
            }

            // 5. Initialize CPU
            cpu_core_ = std::make_unique<CPUCore>();

            // 6. Launch Execution Thread
            is_running_ = true;
            emu_thread_ = std::thread([this]() {
                this->ExecutionLoop();
            });

            return true;
        } catch (const std::exception& e) {
            std::cerr << "[Controller] Critical failure during boot: " << e.what() << std::endl;
            return false;
        }
    }

    void Stop() {
        is_running_ = false;
        if (emu_thread_.joinable()) {
            emu_thread_.join();
        }
        std::cout << "[Controller] Emulator stopped." << std::endl;
    }

    bool IsRunning() const { return is_running_; }

private:
    void ExecutionLoop() {
        std::cout << "[Controller] CPU Execution Thread Started." << std::endl;
        
        while (is_running_) {
            // The heart of the emulator:
            // 1. CPU executes a block of instructions
            // 2. If syscall occurs, call syscall_dispatcher_->Dispatch(...)
            // 3. Handle GPU commands via gpu_translator_
            // 4. Check interrupts via interrupt_ctrl_
            
            // Simulation of the loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
            
            // Check if guest CPU hit a halt instruction or crashed
            if (/* guest_cpu_halted */ false) {
                is_running_ = false;
            }
        }
    }

    std::atomic<bool> is_running_;
    std::thread emu_thread_;

    // System Components
    std::unique_ptr<MemoryManager> mem_manager_;
    std::unique_ptr<CPUCore> cpu_core_;
    std::unique_ptr<SyscallDispatcher> syscall_dispatcher_;
    std::unique_ptr<GPUTranslator> gpu_translator_;
    std::unique_ptr<InterruptController> interrupt_ctrl_;
};

} // namespace Emulator

#endif // KYTY_EMULATOR_CONTROLLER_H
