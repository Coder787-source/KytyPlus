#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "kyty_expected.hpp"

#include "AstroCompatLayer.hpp"
#include "DualSenseEmulator.hpp"
#include "DualSenseFeedbackLoop.hpp"
#include "../GPU/ShaderTranslator.hpp"
#include "JitDispatcher.hpp"
#include "../kernel/SyscallDispatcher.hpp"
#include "core/loader/ElfLoader.hpp"

namespace KytyPS5 {

/**
 * Scaffolding-only boot coordinator. Not used by the real Kyty main()/emulator path.
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

		auto load_result = elf_loader_->Load(image_path);
		if (!load_result) {
			return std::unexpected(load_result.error());
		}

		std::cout << "[Boot] Initializing Virtual Memory..." << std::endl;
		const auto entry_point = load_result->entry_point;

		std::cout << "[Boot] Jumping to Entry Point: 0x" << std::hex << entry_point << std::dec
		          << std::endl;
		return ExecuteGuestLoop(entry_point);
	}

private:
	std::expected<void, std::string> ExecuteGuestLoop(uint64_t entry_point) {
		bool running = true;
		while (running) {
			feedback_loop_->SyncState(dualsense_emu_.get());

			auto result = jit_dispatcher_->Dispatch(entry_point);
			if (!result) {
				return std::unexpected("CPU Execution Panic");
			}

			if (result->is_syscall) {
				HandleSyscall(result->call_id, result->args);
			}
			entry_point = result->next_pc;

			// Scaffolding loop exits immediately so including this header can't spin forever
			// if someone constructs SystemOrchestrator in a test.
			running = false;
			std::this_thread::yield();
		}
		return {};
	}

	void HandleSyscall(uint32_t id, const std::vector<uint64_t>& args) {
		if (args.size() >= 3) {
			(void)syscall_bridge_->Execute(id, args[0], args[1], args[2]);
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

} // namespace KytyPS5

