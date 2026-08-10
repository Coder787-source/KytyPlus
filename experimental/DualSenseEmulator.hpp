#pragma once

#include <cstdint>
#include <memory>
#include <vector>

enum class TriggerID { Left = 0, Right = 1 };
enum class TriggerMode {
	Off = 0,
	Feedback = 1,
	Slope = 2,
	Vibration = 3,
	Sustained = 4
};

struct ColorRGB {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

struct MotionData {
	float accelX = 0;
	float accelY = 0;
	float accelZ = 0;
	float gyroX = 0;
	float gyroY = 0;
	float gyroZ = 0;
};

struct HapticWaveform {
	std::vector<float> samples;
	uint32_t sampleRate = 48000;
};

struct ControllerState {
	bool buttons[18] {};
	float leftStickX = 0;
	float leftStickY = 0;
	float rightStickX = 0;
	float rightStickY = 0;
	float l2Value = 0;
	float r2Value = 0;
	MotionData imu {};
	// Mirrored trigger/haptic fields used by DualSenseFeedbackLoop scaffolding.
	int leftTriggerMode = 0;
	int rightTriggerMode = 0;
	float leftTriggerForce = 0;
	float rightTriggerForce = 0;
	bool isHapticsRunning = false;
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

class NullDualSenseHardware : public IDualSenseHardware {
public:
	bool Initialize() override { return true; }
	void Shutdown() override {}
	void SendTriggerUpdate(TriggerID, TriggerMode, uint8_t) override {}
	void StreamHapticData(const HapticWaveform&) override {}
	void UpdateLightbar(const ColorRGB&) override {}
	ControllerState PollState() override { return {}; }
};

class DualSenseEmulator {
public:
	DualSenseEmulator() : m_hardware(std::make_unique<NullDualSenseHardware>()) {}

	explicit DualSenseEmulator(std::unique_ptr<IDualSenseHardware> hardware)
	    : m_hardware(std::move(hardware)) {
		if (!m_hardware) {
			m_hardware = std::make_unique<NullDualSenseHardware>();
		}
	}

	// Scaffolding helpers used by ScePadDispatcher.
	uint64_t PollInput() { return 0; }
	uint64_t SetTriggerEffect(uint64_t, uint64_t, uint64_t) { return 0; }

	void SetTrigger(TriggerID id, TriggerMode mode, uint8_t strength) {
		m_hardware->SendTriggerUpdate(id, mode, strength);
	}

	void PlayHapticEffect(const HapticWaveform& effect) { m_hardware->StreamHapticData(effect); }

	void SetLightbar(ColorRGB color) { m_hardware->UpdateLightbar(color); }

	ControllerState GetCurrentState() { return m_hardware->PollState(); }

private:
	std::unique_ptr<IDualSenseHardware> m_hardware;
};
