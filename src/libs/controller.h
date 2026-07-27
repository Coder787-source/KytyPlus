#ifndef EMULATOR_INCLUDE_EMULATOR_CONTROLLER_H_
#define EMULATOR_INCLUDE_EMULATOR_CONTROLLER_H_

#include "common/abi.h"
#include "common/common.h"
#include "common/subsystems.h"

#include <string>
#include <vector>

namespace Libs::Controller {

KYTY_SUBSYSTEM_DEFINE(Controller);

constexpr uint32_t PAD_BUTTON_L3        = 0x00000002;
constexpr uint32_t PAD_BUTTON_R3        = 0x00000004;
constexpr uint32_t PAD_BUTTON_OPTIONS   = 0x00000008;
constexpr uint32_t PAD_BUTTON_UP        = 0x00000010;
constexpr uint32_t PAD_BUTTON_RIGHT     = 0x00000020;
constexpr uint32_t PAD_BUTTON_DOWN      = 0x00000040;
constexpr uint32_t PAD_BUTTON_LEFT      = 0x00000080;
constexpr uint32_t PAD_BUTTON_L2        = 0x00000100;
constexpr uint32_t PAD_BUTTON_R2        = 0x00000200;
constexpr uint32_t PAD_BUTTON_L1        = 0x00000400;
constexpr uint32_t PAD_BUTTON_R1        = 0x00000800;
constexpr uint32_t PAD_BUTTON_TRIANGLE  = 0x00001000;
constexpr uint32_t PAD_BUTTON_CIRCLE    = 0x00002000;
constexpr uint32_t PAD_BUTTON_CROSS     = 0x00004000;
constexpr uint32_t PAD_BUTTON_SQUARE    = 0x00008000;
constexpr uint32_t PAD_BUTTON_TOUCH_PAD = 0x00100000;

enum class Axis {
	LeftX        = 0,
	LeftY        = 1,
	RightX       = 2,
	RightY       = 3,
	TriggerLeft  = 4,
	TriggerRight = 5,

	AxisMax
};

struct PadControllerInformation;
struct PadData;
struct PadVibrationParam;
struct PadLightBarParam;

inline int controller_get_axis(int min, int max, int value) {
	int v = (255 * (value - min)) / (max - min);
	return (v < 0 ? 0 : (v > 255 ? 255 : v));
}

void ControllerConnect(int id);
void ControllerDisconnect(int id);
void ControllerButton(int id, uint32_t button, bool down);
void ControllerAxis(int id, Axis axis, int value);
int  ControllerGetActiveId();

// Reports a press/release of the DualSense's dedicated microphone-mute button. Like the Linux
// driver (hid-playstation.c's dualsense_parse_report()), mute state is toggled on each *press*
// (not held), and the mic-mute LED is kept in sync with it via DualSenseSetMicLed. Real hardware
// has no bit for this button in ScePadData either (Sony handles mic muting at the system level,
// outside the game-visible button mask), so this only drives the LED and isn't forwarded as a
// PAD_BUTTON_* bit.
void ControllerMicButton(int id, bool down);

// Implemented by the windowing/input backend (SDL2). Drives real hardware feedback on the
// physical controller matching the given connection id. SDL2's HIDAPI driver abstracts the
// transport, so this works the same whether the DualSense is connected over USB or wirelessly
// over Bluetooth.
void ControllerSetRumble(int id, uint8_t large_motor, uint8_t small_motor);
void ControllerSetLightBar(int id, uint8_t r, uint8_t g, uint8_t b);
void ControllerSetMotionSensorsEnabled(int id, bool enabled);

// Implemented by the windowing/input backend (SDL2). Sets which of the 5 white "player index"
// LEDs below a DualSense's touchpad are lit (SDL reproduces the same left-to-right centered
// mapping the PS5 itself uses, see hid-playstation.c's dualsense_set_player_leds()). A
// player_index of -1 turns the player LEDs off.
void ControllerSetPlayerIndex(int id, int player_index);

// One trigger's adaptive-effect command, sent close to verbatim to the DualSense's HID output
// report. The overall output-report layout (offsets, sizes, enable-bit meanings for
// rumble/lightbar/LED) is verified against two independent sources that agree byte-for-byte:
// Sony's own upstreamed Linux kernel driver (hid-playstation.c's dualsense_output_report_common,
// GPL-2.0-or-later) and this project's bundled SDL2 (SDL_hidapi_ps5.c's DS5EffectsState_t).
// Neither of those implements adaptive trigger effects, so the *exact* meaning of these
// particular bytes -- and the two enable-bit flags that gate them -- is inferred from
// widely-corroborated community reverse-engineering instead: independent DualSense libraries
// agree on an 11-byte "mode + 10 params" block per trigger, at exactly the offset of an
// otherwise-unused/reserved region in the verified report structure. Treat this as best-effort:
// it may not be byte-perfect, but it is bounded and defensive, so a wrong/unknown mode does
// nothing rather than corrupting unrelated report fields.
struct DualSenseTriggerCommand {
	uint8_t mode      = 0;
	uint8_t param[10] = {};
};

// Sends an adaptive-trigger effect update to the DualSense matching the given connection id, via
// SDL2's generic controller-effect passthrough (SDL_GameControllerSendEffect). This works
// identically on Windows and Linux, and over both USB and Bluetooth, because SDL2's own HIDAPI
// PS5 driver already abstracts away those transport differences -- it just forwards our bytes
// into the report region it doesn't otherwise touch. A harmless no-op if id isn't a connected
// DualSense (e.g. the synthetic keyboard id, or no controller/a non-PS5 controller connected).
void DualSenseSetTriggerEffect(int id, const DualSenseTriggerCommand& left,
                               const DualSenseTriggerCommand& right);

// Sets the DualSense's real hardware microphone mute state matching the given connection id,
// and keeps the mic-mute LED in sync with it. Both the enable-bit gating (valid_flag0/1) and the
// mute bit itself (power_save_control's DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE) are verified
// directly against Linux's hid-playstation.c (dualsense_output_worker()'s update_mic_mute
// handling, GPL-2.0-or-later) -- unlike the trigger-effect bytes, this one has an authoritative
// source, not just community reverse-engineering. led_mode: 0 = off, 1 = solid, 2 = pulsing
// (SDL2's documented encoding for the same LED byte).
void DualSenseSetMicMuted(int id, bool muted, uint8_t led_mode);

// Sets the DualSense's speaker and/or microphone hardware volume (0-255 each). Gating
// enable-bits are verified against hid-playstation.c (DS_OUTPUT_VALID_FLAG0_SPEAKER_VOLUME_ENABLE
// / MIC_VOLUME_ENABLE); the exact perceptual range each byte covers is not part of that driver
// (which just hardcodes 0x64 for speaker) -- community reverse-engineering suggests speaker
// tops out around 0x64 and mic around 0x40, with values above the practical max simply not
// increasing further, so this is safe to call with the full 0-255 range regardless.
void DualSenseSetAudioVolume(int id, uint8_t speaker_volume, uint8_t mic_volume);

struct ControllerExtendedState {
	bool     touch0_active = false;
	uint16_t touch0_x      = 0;
	uint16_t touch0_y      = 0;
	bool     touch1_active = false;
	uint16_t touch1_x      = 0;
	uint16_t touch1_y      = 0;
	bool     motion_valid  = false;
	float    gyro_x        = 0.0f;
	float    gyro_y        = 0.0f;
	float    gyro_z        = 0.0f;
	float    accel_x       = 0.0f;
	float    accel_y       = 0.0f;
	float    accel_z       = 0.0f;

	// DualSense Edge-only back paddles and Fn buttons, false/unused on a standard DualSense.
	// SDL's builtin PS5 mapping table (SDL_gamecontroller.c) maps all four physical Fn/paddle
	// inputs onto the public PADDLE1..4 enum as: paddle1 = right back paddle, paddle2 = left
	// back paddle, paddle3 = right Fn button, paddle4 = left Fn button. Sony's real ScePad ABI
	// has no documented public bit for any of these -- on real hardware, they're remapped by
	// the user to an existing button via the Edge companion app, rather than exposed as a
	// distinct input. Since this emulator has no such remapping UI, they're surfaced
	// best-effort as their own state instead of inventing a new ABI bit.
	bool     edge_paddle_right   = false; // paddle1
	bool     edge_paddle_left    = false; // paddle2
	bool     edge_function_right = false; // paddle3
	bool     edge_function_left  = false; // paddle4
};

// Implemented by the windowing/input backend (SDL2). Polls the physical controller matching
// the given connection id for touchpad finger(s) and (if enabled) motion-sensor state. SDL2's
// PS4/PS5 HIDAPI drivers decode both directly from the device, over USB or Bluetooth alike.
void ControllerPollExtendedState(int id, bool motion_enabled, ControllerExtendedState* out);

enum class BatteryLevel { Unknown = -1, Empty, Low, Medium, Full, Wired };

// Implemented by the windowing/input backend (SDL2), via SDL_JoystickCurrentPowerLevel(), which
// SDL2's own PS5 HIDAPI driver already decodes from the DualSense's battery report byte (10
// levels over Bluetooth, always "wired" over USB since the console/PC supplies power directly).
// Note: unlike most of this file, this isn't part of Sony's real ScePad ABI at all -- on actual
// PS5 hardware, games don't read battery level through controller input; the system UI shows it
// via a separate service. This is exposed here as a standalone query (e.g. for a future
// emulator-side HUD/overlay) rather than forced into a fabricated PadData field.
BatteryLevel ControllerGetBatteryLevel(int id);

// Real host keyboard/mouse state, decoupled from the synthetic keyboard-as-gamepad mapping
// above (KeyboardKeyToPadButton in window.cpp forwards a handful of keys as PAD_BUTTON_* bits;
// this is the separate, independent path backing libScePad's actual LibKeyboard/LibMouse HLE
// exports, matching how Sony splits those into distinct APIs from ScePad itself).
// Populated by the windowing backend (SDL2, see window.cpp's GameEventKeyboard/GameEventMouse).

// SDL2's scancode enum is explicitly documented as matching the USB HID usage table (page 0x07,
// keyboard/keypad) 1:1 for the keys it defines, so no separate SDL->HID lookup table is needed --
// the scancode value itself is already the HID usage id games expect in ScePad's key_code field.
void KeyboardRawKeyEvent(uint16_t hid_usage_id, bool down);
// hid_modifier_bitmask uses the standard USB HID boot-keyboard modifier byte layout (bit0=LCtrl,
// bit1=LShift, bit2=LAlt, bit3=LGui, bit4=RCtrl, bit5=RShift, bit6=RAlt, bit7=RGui).
void KeyboardRawModifierState(uint8_t hid_modifier_bitmask);

struct RawKeyboardState {
	uint8_t  modifier_key           = 0;
	uint16_t key_code[16]           = {};
	int32_t  length                 = 0;
};
// Snapshots the currently-held keys (up to 16, matching ScePad's KEYBOARD_MAX_KEYCODES) and
// modifier bitmask. Does not consume/reset anything -- unlike mouse motion, "which keys are
// down right now" is level state, not a per-read delta.
RawKeyboardState KeyboardGetRawState();

// Mouse button bits use the standard USB HID boot-mouse report layout (bit0=Button1/Left,
// bit1=Button2/Right, bit2=Button3/Middle, bit3=Button4/X1, bit4=Button5/X2).
constexpr uint32_t MOUSE_BUTTON_LEFT   = 0x01;
constexpr uint32_t MOUSE_BUTTON_RIGHT  = 0x02;
constexpr uint32_t MOUSE_BUTTON_MIDDLE = 0x04;
constexpr uint32_t MOUSE_BUTTON_X1     = 0x08;
constexpr uint32_t MOUSE_BUTTON_X2     = 0x10;

// Sets or clears a single button bit (for a specific button press/release event).
void MouseRawButtonEvent(uint32_t button_bit, bool down);
// Overwrites the full button mask at once (for events -- like SDL mouse motion -- that report
// the complete current state of all buttons together, rather than a single button changing).
void MouseRawButtonState(uint32_t button_mask);
void MouseRawMotion(int32_t dx, int32_t dy);
void MouseRawWheel(int32_t delta);

struct RawMouseState {
	uint32_t buttons = 0;
	int32_t  x_axis  = 0;
	int32_t  y_axis  = 0;
	int32_t  wheel   = 0;
};
// Returns the current button state plus motion/wheel accumulated since the last call, then
// resets the accumulators to zero -- matching a real mouse's per-poll relative-delta report
// semantics (each read gets the real movement that happened since the previous read, not a
// stale duplicate of the same sample repeated across calls).
RawMouseState MouseConsumeRawState();

// --- User-configurable input mapping (Qt launcher's Input Mapping dialog writes this) ---
//
// window.cpp's SDL event handling calls LookupKeyboardPadButton/LookupControllerPadButton
// instead of hardcoding the host-key/button -> PAD_BUTTON_* switch directly, so a remap applies
// immediately without touching window.cpp. An empty/unset map falls back to the built-in
// defaults (same bindings as before this remapping feature existed), so existing users who never
// open the mapping dialog see no behavior change.

struct InputBinding {
	int      host_code;  // SDL_Keycode (keyboard map) or SDL_GameControllerButton (controller map)
	uint32_t pad_button; // One of the PAD_BUTTON_* constants above, or 0 to explicitly unbind.
};

// Replaces the full keyboard/controller remap table. An empty vector reverts to built-in
// defaults. Safe to call at any time (e.g. when the user saves the mapping dialog) -- takes
// effect on the very next input event, no restart required.
void SetKeyboardButtonMap(const std::vector<InputBinding>& bindings);
void SetControllerButtonMap(const std::vector<InputBinding>& bindings);

// Parses/serializes a mapping as "host_code:pad_button,host_code:pad_button,...". Used to pass
// the mapping between the Qt launcher (which owns the user-facing settings) and the emulator
// process (which applies it) via a single CLI argument, and to persist it in QSettings.
std::vector<InputBinding> ParseInputBindingList(const std::string& serialized);
std::string                SerializeInputBindingList(const std::vector<InputBinding>& bindings);

// Looks up the configured pad button for a host key/controller-button code, falling back to the
// built-in default bindings if no custom map has been set (or the code isn't in it and the
// custom map doesn't include an explicit override for it). Returns 0 if the code has no mapping
// at all, custom or default.
uint32_t LookupKeyboardPadButton(int key_code);
uint32_t LookupControllerPadButton(int button);

// Implemented by the windowing/input backend (SDL2, window.cpp) since the default bindings are
// expressed in terms of SDL_Keycode/SDL_GameControllerButton values. This is also what the Qt
// launcher's Input Mapping dialog reads to pre-populate/reset its list of bindings, so the
// dialog's "defaults" always exactly match what an unmapped key/button actually does.
uint32_t                        DefaultKeyboardPadButton(int key_code);
uint32_t                        DefaultControllerPadButton(int button);
const std::vector<InputBinding>& DefaultKeyboardBindings();
const std::vector<InputBinding>& DefaultControllerBindings();

int KYTY_SYSV_ABI PadInit();
int KYTY_SYSV_ABI PadOpen(int user_id, int type, int index, const void* param);
int KYTY_SYSV_ABI PadGetHandle(int user_id, int type, int index);
int KYTY_SYSV_ABI PadSetMotionSensorState(int handle, bool enable);
int KYTY_SYSV_ABI PadSetAngularVelocityDeadbandState(int handle, bool enable);
int KYTY_SYSV_ABI PadResetOrientation(int handle);
int KYTY_SYSV_ABI PadGetControllerInformation(int handle, PadControllerInformation* info);
int KYTY_SYSV_ABI PadReadState(int handle, PadData* data);
int KYTY_SYSV_ABI PadRead(int handle, PadData* data, int num);
int KYTY_SYSV_ABI PadSetVibration(int handle, const PadVibrationParam* param);
int KYTY_SYSV_ABI PadResetLightBar(int handle);
int KYTY_SYSV_ABI PadSetLightBar(int handle, const PadLightBarParam* param);

} // namespace Libs::Controller

#endif /* EMULATOR_INCLUDE_EMULATOR_CONTROLLER_H_ */
