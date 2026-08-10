#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "DualSenseEmulator.hpp"

/**
 * Scaffolding-only DualSense feedback mirror. Not used by the real SDL DualSense path.
 */
class DualSenseFeedbackLoop {
public:
	DualSenseFeedbackLoop() : m_emulator(std::make_shared<DualSenseEmulator>()) {}

	explicit DualSenseFeedbackLoop(std::shared_ptr<DualSenseEmulator> emulator)
	    : m_emulator(std::move(emulator)) {
		if (!m_emulator) {
			m_emulator = std::make_shared<DualSenseEmulator>();
		}
	}

	struct HardwareStateMirror {
		std::atomic<int> leftTriggerMode {0};
		std::atomic<int> rightTriggerMode {0};
		std::atomic<float> leftTriggerForce {0.0f};
		std::atomic<float> rightTriggerForce {0.0f};
		std::atomic<bool> hapticsActive {false};
	};

	void SyncHardwareState() {
		std::lock_guard<std::mutex> lock(m_stateMutex);
		const auto currentState = m_emulator->GetCurrentState();
		m_mirror.leftTriggerMode.store(currentState.leftTriggerMode);
		m_mirror.rightTriggerMode.store(currentState.rightTriggerMode);
		m_mirror.leftTriggerForce.store(currentState.leftTriggerForce);
		m_mirror.rightTriggerForce.store(currentState.rightTriggerForce);
		m_mirror.hapticsActive.store(currentState.isHapticsRunning);
	}

	// Alias used by SystemOrchestrator scaffolding.
	void SyncState(DualSenseEmulator* emu) {
		if (emu != nullptr) {
			m_emulator = std::shared_ptr<DualSenseEmulator>(emu, [](DualSenseEmulator*) {});
		}
		SyncHardwareState();
	}

	uint32_t GetMirroredStateValue(uint32_t offset) {
		switch (offset) {
		case 0x00:
			return static_cast<uint32_t>(m_mirror.leftTriggerMode.load());
		case 0x04:
			return static_cast<uint32_t>(m_mirror.rightTriggerMode.load());
		case 0x08:
			return static_cast<uint32_t>(m_mirror.leftTriggerForce.load() * 255.0f);
		case 0x0C:
			return static_cast<uint32_t>(m_mirror.rightTriggerForce.load() * 255.0f);
		default:
			return 0;
		}
	}

private:
	std::shared_ptr<DualSenseEmulator> m_emulator;
	HardwareStateMirror m_mirror;
	std::mutex m_stateMutex;
};
