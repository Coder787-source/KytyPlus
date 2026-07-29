#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include "DualSenseEmulator.hpp"
#include "DualSenseFeedbackLoop.hpp"

/**
 * @brief ScePadDispatcher maps SceKernel pad syscalls to the Emulator logic.
 * Ensures minimal latency for high-frequency input polling.
 */
class ScePadDispatcher {
public:
    ScePadDispatcher(std::shared_ptr<DualSenseEmulator> emu, std::shared_ptr<DualSenseFeedbackLoop> loop)
        : m_emulator(std::move(emu)), m_feedbackLoop(std::move(loop)) {
        InitializeSyscallTable();
    }

    // Primary entry point for kernel syscalls
    uint64_t Dispatch(uint64_t syscallId, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
        if (auto it = m_syscallTable.find(syscallId); it != m_syscallTable.end()) {
            return it->second(arg0, arg1, arg2);
        }
        return 0x80000000; // Return generic error for unhandled syscall
    }

private:
    void InitializeSyscallTable() {
        // Map PS5 ScePad syscalls to our emulator methods
        m_syscallTable[0x1001] = [this](uint64_t a0, uint64_t a1, uint64_t a2) {
            return (uint64_t)m_emulator->PollInput(); 
        };
        m_syscallTable[0x1002] = [this](uint64_t a0, uint64_t a1, uint64_t a2) {
            return (uint64_t)m_emulator->SetTriggerEffect(a0, a1, a2);
        };
        m_syscallTable[0x1003] = [this](uint64_t a0, uint64_t a1, uint64_t a2) {
            return m_feedbackLoop->GetMirroredStateValue(a0);
        };
    }

    std::shared_ptr<DualSenseEmulator> m_emulator;
    std::shared_ptr<DualSenseFeedbackLoop> m_feedbackLoop;
    std::unordered_map<uint64_t, std::function<uint64_t(uint64_t, uint64_t, uint64_t)>> m_syscallTable;
};
