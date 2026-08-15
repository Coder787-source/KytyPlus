#ifndef EMULATOR_INCLUDE_EMULATOR_LIBS_DUALSENSE_H_
#define EMULATOR_INCLUDE_EMULATOR_LIBS_DUALSENSE_H_

// DualSense (PS5 controller) native HID driver.
//
// Implements the publicly documented DualSense HID protocol directly (not via SDL),
// providing:
//   - Input report parsing: buttons, analog sticks, L2/R2 triggers, touchpad, gyro/accel.
//   - Output report construction: rumble motors, lightbar RGB, adaptive trigger effects.
//
// STATUS: Implemented against the public DualSense HID spec. NOT validated on hardware.
// The report layouts (IDs, offsets, bit fields) follow the documented protocol used by
// the Linux kernel hid-playstation driver, but no physical DualSense has been used to
// confirm runtime behaviour. Treat as spec-accurate, unvalidated.
//
// Platform: full HID path is Windows-only (SetupAPI + HidD_* + CreateFile/WriteFile).
// On macOS/Linux this compiles as a no-op stub so cross-platform builds stay green.

#include "common/common.h"
#include "common/threads.h"

#include <cstdint>

namespace Libs::DualSense {

// DualSense USB IDs.
constexpr uint16_t DUALSENSE_VID = 0x054C;
constexpr uint16_t DUALSENSE_PID = 0x0CE6;

// DualSense button bitmask (input report bytes 1-2, little-endian 16-bit).
// Matches the documented DualSense layout.
enum : uint32_t {
	ButtonCross       = 0x0001,
	ButtonCircle      = 0x0002,
	ButtonSquare      = 0x0004,
	ButtonTriangle    = 0x0008,
	ButtonDpadUp      = 0x0010,
	ButtonDpadRight   = 0x0020,
	ButtonDpadDown   = 0x0040,
	ButtonDpadLeft   = 0x0080,
	ButtonL1         = 0x0100,
	ButtonR1         = 0x0200,
	ButtonL2         = 0x0400,
	ButtonR2         = 0x0800,
	ButtonCreate     = 0x1000, // Share/Create
	ButtonOptions    = 0x2000,
	ButtonL3         = 0x4000,
	ButtonR3         = 0x8000,
	// Second 16-bit field (input report bytes 3-4).
	ButtonPsButton   = 0x00010000,
	ButtonTouchpad   = 0x00020000, // touchpad click
	ButtonMicMute    = 0x00040000,
};

// Adaptive trigger effect modes (output report, documented values).
enum class TriggerEffect : uint8_t {
	Off         = 0x05,
	Feedback    = 0x01, // continuous resistance
	Weapon      = 0x02, // stage + resistance (weapon cocking)
	Vibration   = 0x26, // vibration segment
	Slope       = 0x21, // slope-based resistance
};

struct TriggerEffectParam {
	TriggerEffect mode = TriggerEffect::Off;
	uint8_t strength   = 0;     // 0-7 resistance strength (feedback/weapon)
	uint8_t start      = 0;     // 0-255 start position
	uint8_t end        = 0;     // 0-255 end position (for stage/weapon)
	uint8_t frequency = 0;     // 0-255 vibration frequency (vibration mode)
};

struct LightBarColor {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

struct VibrationParam {
	uint8_t large_motor = 0; // right actuator (low-freq)
	uint8_t small_motor = 0; // left actuator (high-freq)
};

struct MotionData {
	float accel_x = 0.0f;
	float accel_y = 0.0f;
	float accel_z = 0.0f;
	float gyro_x  = 0.0f;
	float gyro_y  = 0.0f;
	float gyro_z  = 0.0f;
};

struct TouchPoint {
	bool  active = false;
	uint8_t id   = 0;
	uint16_t x   = 0;
	uint16_t y   = 0;
};

// Parsed input state from a single DualSense input report.
struct InputState {
	uint32_t    buttons      = 0;
	uint8_t     left_stick_x  = 128; // 0-255, 128 = centered
	uint8_t     left_stick_y  = 128;
	uint8_t     right_stick_x = 128;
	uint8_t     right_stick_y = 128;
	uint8_t     l2            = 0;   // 0-255 analog trigger
	uint8_t     r2            = 0;
	uint8_t     sequence      = 0;
	MotionData  motion {};
	TouchPoint  touch[2] {};
	bool        valid = false;
};

// The driver. A single instance owns one DualSense device and a polling thread.
// Thread-safe for output calls (SetVibration/SetLightBar/SetTriggerEffect) from the
// emulator thread; input polling runs on an internal worker thread.
class DualSenseDriver {
public:
	DualSenseDriver()  = default;
	~DualSenseDriver();

	KYTY_CLASS_NO_COPY(DualSenseDriver);

	// Open the first available DualSense device. Returns true on success.
	// On non-Windows platforms, always returns false (no-op stub).
	bool Open();

	// Close the device and stop the polling thread.
	void Close();

	[[nodiscard]] bool IsOpen() const { return m_open; }

	// Polling: run on internal thread. The callback receives the latest parsed state.
	// Set before Open(); called from the polling thread.
	using InputCallback = void (*)(const InputState& state, void* user);
	void SetInputCallback(InputCallback cb, void* user) {
		m_input_cb   = cb;
		m_input_user = user;
	}

	// Output: send rumble / lightbar / trigger effect. No-op if device not open.
	void SetVibration(const VibrationParam& v);
	void SetLightBar(const LightBarColor& c);
	void SetTriggerEffect(bool left, const TriggerEffectParam& e);

	// Manually poll one input report (blocking with timeout). Returns false on failure
	// or if no device is open. Mainly for diagnostics; the internal thread uses this.
	bool PollOnce(InputState* out);

private:
	void PollLoop();

	bool OpenWindows();
	bool ParseInputReportUsb(const uint8_t* buf, size_t len, InputState* out);
	bool ParseInputReportBt(const uint8_t* buf, size_t len, InputState* out);
	void SendOutputReportUsb(const VibrationParam* v, const LightBarColor* lb,
	                         const TriggerEffectParam* lt, const TriggerEffectParam* rt);
	void SendOutputReportBt(const VibrationParam* v, const LightBarColor* lb,
	                        const TriggerEffectParam* lt, const TriggerEffectParam* rt);

	bool          m_open = false;
	void*         m_handle = nullptr;      // platform device handle (opaque)
	InputCallback m_input_cb   = nullptr;
	void*         m_input_user = nullptr;

	Common::Thread* m_thread   = nullptr;
	bool           m_stop      = false;

	// Cached last-sent output state to allow partial updates (e.g. rumble without
	// touching the lightbar) and to coalesce concurrent calls.
	VibrationParam     m_last_vibration {};
	LightBarColor      m_last_lightbar {};
	TriggerEffectParam m_last_left_trigger {};
	TriggerEffectParam m_last_right_trigger {};
	Common::Mutex       m_output_mutex;

	uint8_t  m_output_seq = 0; // DualSense output report sequence counter
};

} // namespace Libs::DualSense

#endif /* EMULATOR_INCLUDE_EMULATOR_LIBS_DUALSENSE_H_ */