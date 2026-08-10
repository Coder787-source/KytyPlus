#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>

#include "DualSenseEmulator.hpp"
#include "DualSenseFeedbackLoop.hpp"

/**
 * Scaffolding-only pad syscall router. Real pad HLE lives in src/libs/controller.cpp + libPad.cpp.
 */
class ScePadDispatcher {
public:
	ScePadDispatcher()
	    : m_emulator(std::make_shared<DualSenseEmulator>()),
	      m_feedbackLoop(std::make_shared<DualSenseFeedbackLoop>(m_emulator)) {
		InitializeSyscallTable();
	}

	ScePadDispatcher(std::shared_ptr<DualSenseEmulator> emu,
	                 std::shared_ptr<DualSenseFeedbackLoop> loop)
	    : m_emulator(std::move(emu)), m_feedbackLoop(std::move(loop)) {
		if (!m_emulator) {
			m_emulator = std::make_shared<DualSenseEmulator>();
		}
		if (!m_feedbackLoop) {
			m_feedbackLoop = std::make_shared<DualSenseFeedbackLoop>(m_emulator);
		}
		InitializeSyscallTable();
	}

	uint64_t Dispatch(uint64_t syscallId, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
		if (auto it = m_syscallTable.find(syscallId); it != m_syscallTable.end()) {
			return it->second(arg0, arg1, arg2);
		}
		return 0x80000000;
	}

	// Scaffolding stubs for IDE/smoke includes.
	int scePadReadState(int /*handle*/, void* /*state*/) { return 0; }
	int HandleIdc(uint32_t /*id*/, void* /*buf*/, size_t /*len*/) { return 0; }

private:
	void InitializeSyscallTable() {
		m_syscallTable[0x1001] = [this](uint64_t, uint64_t, uint64_t) {
			return m_emulator->PollInput();
		};
		m_syscallTable[0x1002] = [this](uint64_t a0, uint64_t a1, uint64_t a2) {
			return m_emulator->SetTriggerEffect(a0, a1, a2);
		};
		m_syscallTable[0x1003] = [this](uint64_t a0, uint64_t, uint64_t) {
			return m_feedbackLoop->GetMirroredStateValue(static_cast<uint32_t>(a0));
		};
	}

	std::shared_ptr<DualSenseEmulator> m_emulator;
	std::shared_ptr<DualSenseFeedbackLoop> m_feedbackLoop;
	std::unordered_map<uint64_t, std::function<uint64_t(uint64_t, uint64_t, uint64_t)>> m_syscallTable;
};
