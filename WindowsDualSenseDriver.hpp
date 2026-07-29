#pragma once
#include <windows.h>
#include <hidpi.h>
#include <iostream>
#include <vector>
#include "DualSenseEmulator.hpp"

#pragma comment(lib, "hid.lib")

/**
 * @brief Implementation of the hardware bridge for Windows HID.
 */
class WindowsDualSenseDriver : public IDualSenseHardware {
public:
    WindowsDualSenseDriver() : m_deviceHandle(nullptr) {}
    ~WindowsDualSenseDriver() { Shutdown(); }

    bool Initialize() override {
        // VendorID: Sony 0x054C, ProductID: DualSense 0x0CE6
        m_deviceHandle = CreateFileW(
            L"\\\\.\\HID#VID_054C&PID_0CE6", 
            GENERIC_READ | GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE, 
            NULL, OPEN_EXISTING, 0, NULL
        );

        if (m_deviceHandle == INVALID_HANDLE_VALUE) {
            std::cerr << "[DualSense] Failed to open HID handle. Ensure controller is connected via USB/BT." << std::endl;
            return false;
        }
        return true;
    }

    void Shutdown() override {
        if (m_deviceHandle && m_deviceHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_deviceHandle);
            m_deviceHandle = nullptr;
        }
    }

    void SendTriggerUpdate(TriggerID id, TriggerMode mode, uint8_t intensity) override {
        // Simplified HID report structure for Adaptive Triggers
        std::vector<uint8_t> report(64, 0);
        report[0] = 0x05; // Report ID for Output
        report[1] = 0x01; // Trigger enable
        
        uint8_t triggerOffset = (id == TriggerID::Left) ? 10 : 20;
        report[triggerOffset] = static_cast<uint8_t>(mode);
        report[triggerOffset + 1] = intensity;

        DWORD written;
        WriteFile(m_deviceHandle, report.data(), (DWORD)report.size(), &written, NULL);
    }

    void StreamHapticData(const HapticWaveform& waveform) override {
        // In a real implementation, this would chunk the waveform into 
        // high-frequency HID reports compatible with the voice-coil actuators.
    }

    void UpdateLightbar(const ColorRGB& color) override {
        std::vector<uint8_t> report(64, 0);
        report[0] = 0x05; 
        report[5] = color.r;
        report[6] = color.g;
        report[7] = color.b;

        DWORD written;
        WriteFile(m_deviceHandle, report.data(), (DWORD)report.size(), &written, NULL);
    }

    ControllerState PollState() override {
        ControllerState state = {};
        uint8_t buffer[64];
        DWORD read;

        if (ReadFile(m_deviceHandle, buffer, sizeof(buffer), &read, NULL)) {
            // Simple mapping of HID buffer to ControllerState
            state.buttons[0] = buffer[5] & 0x01; // Example: Cross button
            state.leftStickX = (float)buffer[1] / 127.0f;
            state.leftStickY = (float)buffer[2] / 127.0f;
        }
        return state;
    }

private:
    HANDLE m_deviceHandle;
};
