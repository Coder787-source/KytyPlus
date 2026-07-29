#pragma once

#include <iostream>
#include <memory>
#include <expected>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include "Kernel/SyscallDispatcher.hpp"
#include "Loader/ElfLoader.hpp"
#include "Core/JitDispatcher.hpp"
#include "Gpu/ShaderTranslator.hpp"
#include "DualSenseEmulator.hpp"
#include "DualSenseFeedbackLoop.hpp"
#include "AstroCompatLayer.hpp"

namespace KytyPS5 {

    /**
     * @brief Implements the master boot sequence for the PS5 Emulator.
     * Coordinates the transition from cold boot to guest execution.
     */
    class SystemOrchestrator {
    public:
        SystemOrchestrator() 
            : syscall_bridge_(std::make_unique<Kernel::SyscallDispatcher>()),
              elf_loader_(std::make_unique<Loader::ElfLoader>()),
              jit_dispatcher_(std::make_unique<Core::JitDispatcher>()),
              shader_translator_(std::make_unique<Gpu::ShaderTranslator>()),
              dualsense_emu_(std::make_unique<DualSenseEmulator>()),
              feedback_loop_(std::make_unique<DualSenseFeedbackLoop>()),
              astro_compat_(std::make_unique<AstroCompatLayer>()) {}

        std::expected<void, std::string> Boot(const std::string& image_path) {
            std::cout << "[Boot] Loading image: " << image_path << std::endl;

            // 1. Load and Map ELF
            auto load_result = elf_loader_->Load(image_path);
            if (!load_result) return std::unexpected("ELF Loading Failed");

            // 2. Initialize Guest Memory Space
            std::cout << "[Boot] Initializing Virtual Memory..." << std::endl;
            auto entry_point = load_result.value().entry_point;

            // 3. Start JIT Execution Loop
            std::cout << "[Boot] Jumping to Entry Point: 0x" << std::hex << entry_point << std::endl;
            return ExecuteGuestLoop(entry_point);
        }

    private:
        std::expected<void, std::string> ExecuteGuestLoop(uint64_t entry_point) {
            bool running = true;
            while (running) {
                // Sync DualSense Feedback Loop to satisfy Astro hardware checks
                feedback_loop_->SyncState(dualsense_emu_.get());

                // Simulate CPU cycles
                auto result = jit_dispatcher_->Dispatch(entry_point);
                
                if (result.has_value()) {
                    // Check if we hit a syscall
                    if (result.value().is_syscall) {
                        HandleSyscall(result.value().call_id, result.value().args);
                    }
                    entry_point = result.value().next_pc;
                } else {
                    return std::unexpected("CPU Execution Panic");
                }

                // Prevent host thread saturation
                std::this_thread::yield();
            }
            return {};
        }

        void HandleSyscall(uint32_t id, const std::vector<uint64_t>& args) {
            if (args.size() >= 3) {
                syscall_bridge_->Execute(id, args[0], args[1], args[2]);
            }
        }

        std::unique_ptr<Kernel::SyscallDispatcher> syscall_bridge_;
        std::unique_ptr<Loader::ElfLoader> elf_loader_;
        std::unique_ptr<Core::JitDispatcher> jit_dispatcher_;
        std::unique_ptr<Gpu::ShaderTranslator> shader_translator_;
        std::unique_ptr<DualSenseEmulator> dualsense_emu_;
        std::unique_ptr<DualSenseFeedbackLoop> feedback_loop_;
        std::unique_ptr<AstroCompatLayer> astro_compat_;
    };

}
