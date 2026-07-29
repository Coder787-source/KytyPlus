#pragma once
#include <memory>
#include <vector>
#include <variant>
#include <cstdint>

enum class TriggerID { Left = 0, Right = 1 };
enum class TriggerMode { 
    Off = 0, 
    Feedback = 1, 
    Slope = 2, 
    Vibration = 3, 
    Sustained = 4 
};

struct ColorRGB {
    uint8_t r, g, b;
};

struct MotionData {
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
};

struct HapticWaveform {
    std::vector<float> samples; 
    uint32_t sampleRate = 48000;
};

struct ControllerState {
    bool buttons[18];
    float leftStickX, leftStickY;
    float rightStickX, rightStickY;
    float l2Value, r2Value;
    MotionData imu;
};

class IDualSenseHardware {
public:
    virtual ~IDualSenseHardware() = default;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void SendTriggerUpdate(TriggerID id, TriggerMode mode, uint8_t intensity) = 0;
    virtual void StreamHapticData(const HapticWaveform& waveform) = 0;
    virtual void UpdateLightbar(const ColorRGB& color) = 0;
    virtual ControllerState PollState() = 0;
};

class DualSenseEmulator {
public:
    explicit DualSenseEmulator(std::unique_ptr<IDualSenseHardware> hardware) 
        : m_hardware(std::move(hardware)) {}

    void SetTrigger(TriggerID id, TriggerMode mode, uint8_t strength) {
        m_hardware->SendTriggerUpdate(id, mode, strength);
    }

    void PlayHapticEffect(const HapticWaveform& effect) {
        m_hardware->StreamHapticData(effect);
    }

    void SetLightbar(ColorRGB color) {
        m_hardware->UpdateLightbar(color);
    }

    ControllerState GetCurrentState() {
        return m_hardware->PollState();
    }

private:
    std::unique_ptr<IDualSenseHardware> m_hardware;
};
