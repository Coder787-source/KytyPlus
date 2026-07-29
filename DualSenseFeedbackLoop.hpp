#pragma once
#include <iostream>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include "DualSenseEmulator.hpp"

/**
 * @brief DualSenseFeedbackLoop implements the "Read-Back" mechanism.
 * Astro's Playroom polls the controller to verify if a requested 
 * haptic state was actually applied to the hardware.
 */
class DualSenseFeedbackLoop {
public:
    explicit DualSenseFeedbackLoop(std::shared_ptr<DualSenseEmulator> emulator) 
        : m_emulator(std::move(emulator)) {}

    struct HardwareStateMirror {
        std::atomic<int> leftTriggerMode{0};
        std::atomic<int> rightTriggerMode{0};
        std::atomic<float> leftTriggerForce{0.0f};
        std::atomic<float> rightTriggerForce{0.0f};
        std::atomic<bool> hapticsActive{false};
    };

    void SyncHardwareState() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto currentState = m_emulator->GetCurrentState();
        
        m_mirror.leftTriggerMode.store(currentState.leftTriggerMode);
        m_mirror.rightTriggerMode.store(currentState.rightTriggerMode);
        m_mirror.leftTriggerForce.store(currentState.leftTriggerForce);
        m_mirror.rightTriggerForce.store(currentState.rightTriggerForce);
        m_mirror.hapticsActive.store(currentState.isHapticsRunning);
    }

    // Called by the SceKernel syscall handler when the game asks "Is the trigger actually resisting?"
    uint32_t GetMirroredStateValue(uint32_t offset) {
        switch(offset) {
            case 0x00: return m_mirror.leftTriggerMode.load();
            case 0x04: return m_mirror.rightTriggerMode.load();
            case 0x08: return static_cast<uint32_t>(m_mirror.leftTriggerForce.load() * 255.0f);
            case 0x0C: return static_cast<uint32_t>(m_mirror.rightTriggerForce.load() * 255.0f);
            default: return 0;
        }
    }

private:
    std::shared_ptr<DualSenseEmulator> m_emulator;
    HardwareStateMirror m_mirror;
    std::mutex m_stateMutex;
};
