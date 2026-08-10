#ifndef KYTY_EMULATOR_CONTROLLER_H
#define KYTY_EMULATOR_CONTROLLER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "kernel/cpu_core.h"
#include "graphics/guest_gpu/gpu_translator.h"
#include "kernel/interrupt_controller.h"
#include "loader/binary_decryption.h"
#include "loader/elf_unpacker.h"
#include "kernel/syscall_dispatcher.h"

namespace Emulator {

/**
 * Scaffolding-only GUI/backend glue. Not used by Emulator::Run / main.cpp.
 * Kept self-contained so IDE indexing does not pull in real Memory HLE types.
 */
class EmulatorController {
public:
	EmulatorController() = default;
	~EmulatorController() { Stop(); }

	bool Start(const std::string& gamePath, const std::vector<uint8_t>& userKeys) {
		std::cout << "[Controller] Booting game: " << gamePath << std::endl;

		mem_manager_ = std::make_unique<StubMemoryManager>();
		interrupt_ctrl_ = std::make_unique<InterruptController>();
		gpu_translator_ = std::make_unique<GPUTranslator>();
		syscall_dispatcher_ = std::make_unique<SyscallDispatcher>();

		BinaryDecryption decryptor;
		const std::vector<uint8_t> ciphertext;
		const std::vector<uint8_t> plaintext = decryptor.Decrypt(ciphertext, userKeys);

		ElfUnpacker unpacker(mem_manager_.get());
		if (!unpacker.MapBinaryToMemory(plaintext)) {
			std::cerr << "[Controller] Failed to map binary to memory." << std::endl;
			return false;
		}

		cpu_core_ = std::make_unique<CPUCore>();
		is_running_ = true;
		emu_thread_ = std::thread([this]() { this->ExecutionLoop(); });
		return true;
	}

	void Stop() {
		is_running_ = false;
		if (emu_thread_.joinable()) {
			emu_thread_.join();
		}
	}

	bool IsRunning() const { return is_running_; }

private:
	void ExecutionLoop() {
		while (is_running_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	std::atomic<bool> is_running_ {false};
	std::thread emu_thread_;
	std::unique_ptr<StubMemoryManager> mem_manager_;
	std::unique_ptr<CPUCore> cpu_core_;
	std::unique_ptr<SyscallDispatcher> syscall_dispatcher_;
	std::unique_ptr<GPUTranslator> gpu_translator_;
	std::unique_ptr<InterruptController> interrupt_ctrl_;
};

} // namespace Emulator

#endif // KYTY_EMULATOR_CONTROLLER_H
