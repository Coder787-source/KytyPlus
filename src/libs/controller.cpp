#include "libs/controller.h"

#include "common/assert.h"
#include "common/common.h"
#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "kernel/pthread.h"
#include "libs/dualsense.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "libs/padData.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace Libs::Controller {

LIB_NAME("Pad", "Pad");

constexpr int PAD_ERROR_INVALID_ARG    = -2137915391; /* 0x80920001 */
constexpr int PAD_ERROR_INVALID_HANDLE = -2137915389; /* 0x80920003 */

struct PadControllerInformation {
	float    touch_pixel_density;
	uint16_t touch_resolution_x;
	uint16_t touch_resolution_y;
	uint8_t  stick_dead_zone_left;
	uint8_t  stick_dead_zone_right;
	uint8_t  connection_type;
	uint8_t  connected_count;
	bool     connected;
	int      device_class;
	uint8_t  reserve[8];
};

struct PadVibrationParam {
	uint8_t large_motor;
	uint8_t small_motor;
};

struct ControllerState {
	uint64_t time                                  = 0;
	uint32_t buttons                               = 0;
	int      axes[static_cast<int>(Axis::AxisMax)] = {128, 128, 128, 128, 0, 0};
};

// Local-multiplayer: one GameController owns up to PAD_MAX_CONTROLLERS player
// slots. Each slot keeps an independent input stream + history, so a guest that
// opens pad handle N reads only the pad bound to slot N-1.
class GameController {
public:
	GameController()          = default;
	virtual ~GameController() = default;

	KYTY_CLASS_NO_COPY(GameController);

	void Connect(int id);
	void Disconnect(int id);
	void Button(int id, uint32_t button, bool down);
	void Axis(int id, Axis axis, int value);
	void ResetInputState();
	void GetConnectionInfo(bool* flag, int* count);
	void ReadState(ControllerState* state, bool* flag, int* count);
	int  ReadStates(ControllerState* states, int states_num, bool* flag, int* count);
	int  ReadStateForSlot(int index, ControllerState* state, bool* flag, int* count);
	int  ReadStatesForSlot(int index, ControllerState* states, int states_num, bool* flag,
	                          int* count);

private:
	static constexpr uint32_t STATES_MAX = 64;

	struct SlotState {
		ControllerState last_state;
		ControllerState states[STATES_MAX];
		bool            obtained[STATES_MAX] = {};
		uint32_t        states_num          = 0;
		uint32_t        first_state         = 0;
	};

	void EnsureSlot(int index);
	int  OwnerSlot(int id) const;
	int  FallbackSlot() const;
	int  PresentCount() const;
	bool SlotConnected(int index) const;

	[[nodiscard]] ControllerState GetLastState(int index) const;
	void                          AddState(int index, const ControllerState& state);

	Common::Mutex          m_mutex;
	int                    m_ids[PAD_MAX_CONTROLLERS] = {-1, -1, -1, -1};
	std::vector<SlotState> m_slots;
};
static GameController* g_controller = nullptr;
// Native DualSense driver instance (spec-accurate, hardware-unvalidated).
// Owned by the Controller subsystem; opened on subsystem init if a device is
// present, polled on an internal thread, and closed on subsystem destroy.
static ::Libs::DualSense::DualSenseDriver* g_dualsense = nullptr;

void* DualSenseDriverInstance() {
	return g_dualsense;
}

// Input callback: forward a parsed DualSense state into the real pad flow.
// This mirrors what window.cpp does for SDL events, but for a real DualSense
// connected via the native HID driver. Uses a fixed controller id.
static constexpr int DUALSENSE_CONTROLLER_ID = -2000;

static void DualSenseInputCallback(const ::Libs::DualSense::InputState& state, void* /*user*/) {
	if (!state.valid) return;
	// Buttons: the DualSense button bitmask overlaps with the PAD_BUTTON_* layout
	// used by the rest of the pad system. Forward each button as a press/release
	// event into the real ControllerButton flow. We diff against the previous
	// reported buttons to only emit edge transitions.
	static uint32_t prev_buttons = 0;
	const uint32_t changed = state.buttons ^ prev_buttons;
	// Map DualSense bits to PAD_BUTTON_* (the layouts happen to match for the
	// face/dpad/L1R1/L2R2/Create/Options/L3R3 set; PS/touchpad are extra).
	const uint32_t mappable =
		::Libs::DualSense::ButtonCross | ::Libs::DualSense::ButtonCircle | ::Libs::DualSense::ButtonSquare |
		::Libs::DualSense::ButtonTriangle | ::Libs::DualSense::ButtonDpadUp | ::Libs::DualSense::ButtonDpadRight |
		::Libs::DualSense::ButtonDpadDown | ::Libs::DualSense::ButtonDpadLeft | ::Libs::DualSense::ButtonL1 |
		::Libs::DualSense::ButtonR1 | ::Libs::DualSense::ButtonL2 | ::Libs::DualSense::ButtonR2 |
		::Libs::DualSense::ButtonCreate | ::Libs::DualSense::ButtonOptions |
		::Libs::DualSense::ButtonL3 | ::Libs::DualSense::ButtonR3 | ::Libs::DualSense::ButtonTouchpad;
	const uint32_t mask = changed & mappable;
	for (uint32_t b = 1; b != 0; b <<= 1) {
		if ((mask & b) == 0) continue;
		uint32_t pad_btn = 0;
		switch (b) {
			case ::Libs::DualSense::ButtonCross:    pad_btn = PAD_BUTTON_CROSS; break;
			case ::Libs::DualSense::ButtonCircle:   pad_btn = PAD_BUTTON_CIRCLE; break;
			case ::Libs::DualSense::ButtonSquare:   pad_btn = PAD_BUTTON_SQUARE; break;
			case ::Libs::DualSense::ButtonTriangle: pad_btn = PAD_BUTTON_TRIANGLE; break;
			case ::Libs::DualSense::ButtonDpadUp:   pad_btn = PAD_BUTTON_UP; break;
			case ::Libs::DualSense::ButtonDpadRight: pad_btn = PAD_BUTTON_RIGHT; break;
			case ::Libs::DualSense::ButtonDpadDown: pad_btn = PAD_BUTTON_DOWN; break;
			case ::Libs::DualSense::ButtonDpadLeft: pad_btn = PAD_BUTTON_LEFT; break;
			case ::Libs::DualSense::ButtonL1:       pad_btn = PAD_BUTTON_L1; break;
			case ::Libs::DualSense::ButtonR1:       pad_btn = PAD_BUTTON_R1; break;
			case ::Libs::DualSense::ButtonL2:       pad_btn = PAD_BUTTON_L2; break;
			case ::Libs::DualSense::ButtonR2:       pad_btn = PAD_BUTTON_R2; break;
			// PS5 "Create" has no dedicated PAD_BUTTON_* bit in this pad layout; skip.
			case ::Libs::DualSense::ButtonCreate:   continue;
			case ::Libs::DualSense::ButtonOptions:  pad_btn = PAD_BUTTON_OPTIONS; break;
			case ::Libs::DualSense::ButtonL3:       pad_btn = PAD_BUTTON_L3; break;
			case ::Libs::DualSense::ButtonR3:       pad_btn = PAD_BUTTON_R3; break;
			case ::Libs::DualSense::ButtonTouchpad: pad_btn = PAD_BUTTON_TOUCH_PAD; break;
			default: continue;
		}
		ControllerButton(DUALSENSE_CONTROLLER_ID, pad_btn, (state.buttons & b) != 0);
	}
	prev_buttons = state.buttons;

	// Axes: forward sticks and analog triggers.
	ControllerAxis(DUALSENSE_CONTROLLER_ID, Axis::LeftX, state.left_stick_x);
	ControllerAxis(DUALSENSE_CONTROLLER_ID, Axis::LeftY, state.left_stick_y);
	ControllerAxis(DUALSENSE_CONTROLLER_ID, Axis::RightX, state.right_stick_x);
	ControllerAxis(DUALSENSE_CONTROLLER_ID, Axis::RightY, state.right_stick_y);
	ControllerAxis(DUALSENSE_CONTROLLER_ID, Axis::TriggerLeft, state.l2);
	ControllerAxis(DUALSENSE_CONTROLLER_ID, Axis::TriggerRight, state.r2);
}

static uint8_t pad_connected_count_to_u8(int connected_count) {
	return static_cast<uint8_t>(connected_count > 255 ? 255 : connected_count);
}

static void pad_fill_data(PadData* data, const ControllerState& state, bool connected,
                          int connected_count) {
	EXIT_IF(data == nullptr);

	std::memset(data, 0, sizeof(*data));

	data->buttons                = state.buttons;
	data->left_stick_x           = state.axes[static_cast<int>(Axis::LeftX)];
	data->left_stick_y           = state.axes[static_cast<int>(Axis::LeftY)];
	data->right_stick_x          = state.axes[static_cast<int>(Axis::RightX)];
	data->right_stick_y          = state.axes[static_cast<int>(Axis::RightY)];
	data->analog_buttons_l2      = state.axes[static_cast<int>(Axis::TriggerLeft)];
	data->analog_buttons_r2      = state.axes[static_cast<int>(Axis::TriggerRight)];
	data->orientation_w          = 1.0f;
	data->touch_data_touch0_id   = 1;
	data->touch_data_touch1_id   = 2;
	data->connected              = connected;
	data->timestamp              = state.time;
	data->connected_count        = pad_connected_count_to_u8(connected_count);
	data->device_unique_data_len = 0;
}

KYTY_SUBSYSTEM_INIT(Controller) {
	EXIT_IF(g_controller != nullptr);

	g_controller = new GameController;
	g_controller->ResetInputState();

	// The native DualSense HID driver is opt-in and off by default. SDL already
	// handles every standard pad (including a DualSense); this unvalidated driver is
	// only opened when explicitly enabled via Config::DualSenseEnabled(), so it never
	// competes with the SDL controller path for the same device.
	g_dualsense = nullptr;
	if (Config::DualSenseEnabled()) {
		g_dualsense = new ::Libs::DualSense::DualSenseDriver;
		if (g_dualsense->Open()) {
			g_dualsense->SetInputCallback(&DualSenseInputCallback, nullptr);
			g_controller->Connect(DUALSENSE_CONTROLLER_ID);
		} else {
			// No DualSense found; the SDL path remains the active input source.
			delete g_dualsense;
			g_dualsense = nullptr;
		}
	}
}

KYTY_SUBSYSTEM_UNEXPECTED_SHUTDOWN(Controller) {}

KYTY_SUBSYSTEM_DESTROY(Controller) {
	if (g_dualsense != nullptr) {
		g_dualsense->Close();
		delete g_dualsense;
		g_dualsense = nullptr;
	}
}

// --- multi-player helpers --------------------------------------------------

int ControllerIndexFromUserId(int user_id) {
	if (user_id < PAD_USER_ID_BASE) {
		return -1;
	}
	const int index = user_id - PAD_USER_ID_BASE;
	return index < PAD_MAX_CONTROLLERS ? index : -1;
}

int ControllerIndexFromHandle(int handle) {
	const int index = handle - PAD_HANDLE_BASE;
	return (index >= 0 && index < PAD_MAX_CONTROLLERS) ? index : -1;
}

int ControllerConnectedCount() {
	if (g_controller == nullptr) {
		return 0;
	}
	bool connected = false;
	int  count     = 0;
	g_controller->GetConnectionInfo(&connected, &count);
	return count;
}

void ControllerConnect(int id) {
	EXIT_IF(g_controller == nullptr);
	g_controller->Connect(id);
}

void ControllerDisconnect(int id) {
	EXIT_IF(g_controller == nullptr);
	g_controller->Disconnect(id);
}

void ControllerButton(int id, uint32_t button, bool down) {
	EXIT_IF(g_controller == nullptr);
	g_controller->Button(id, button, down);
}

void ControllerAxis(int id, Axis axis, int value) {
	EXIT_IF(g_controller == nullptr);
	g_controller->Axis(id, axis, value);
}

void ControllerResetInputState() {
	EXIT_IF(g_controller == nullptr);
	g_controller->ResetInputState();
}

// --- GameController implementation -----------------------------------------

void GameController::EnsureSlot(int index) {
	if (index < 0 || index >= PAD_MAX_CONTROLLERS) {
		return;
	}
	while (static_cast<int>(m_slots.size()) <= index) {
		m_slots.emplace_back();
	}
}

int GameController::OwnerSlot(int id) const {
	for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
		if (m_ids[i] == id) {
			return i;
		}
	}
	return -1;
}

int GameController::FallbackSlot() const {
	// Keyboard/mouse (and any stray unowned event) follows the primary pad.
	return 0;
}

int GameController::PresentCount() const {
	int count = 1; // player 1 is always present (it may be keyboard-driven)
	for (int i = 1; i < PAD_MAX_CONTROLLERS; i++) {
		if (m_ids[i] != -1) {
			count++;
		}
	}
	return count;
}

bool GameController::SlotConnected(int index) const {
	if (index == 0) {
		return true;
	}
	return index > 0 && index < PAD_MAX_CONTROLLERS && m_ids[index] != -1;
}

void GameController::Connect(int id) {
	Common::LockGuard lock(m_mutex);

	// HOST_INPUT_CONTROLLER_ID is a merged keyboard/mouse stream, not a physical
	// pad; it never claims a player slot (it augments the primary slot instead).
	if (id == HOST_INPUT_CONTROLLER_ID) {
		EnsureSlot(0);
		return;
	}

	// A pad already bound to a slot is left where it is.
	if (OwnerSlot(id) != -1) {
		return;
	}

	// Bind to the lowest free slot: first pad -> slot 0 (player 1), second pad ->
	// slot 1 (player 2), and so on. Slot N maps to pad handle N+1.
	for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
		if (m_ids[i] == -1) {
			EnsureSlot(i);
			m_ids[i] = id;
			return;
		}
	}
	// At capacity; ignore extra devices.
}

void GameController::Disconnect(int id) {
	Common::LockGuard lock(m_mutex);

	const int slot = OwnerSlot(id);
	if (slot == -1) {
		return;
	}

	m_ids[slot] = -1;
	EnsureSlot(slot);
	auto& s       = m_slots[slot];
	s.states_num  = 0;
	s.first_state = 0;
	s.last_state  = ControllerState();
}

ControllerState GameController::GetLastState(int index) const {
	if (index < 0 || index >= static_cast<int>(m_slots.size())) {
		return ControllerState();
	}
	const auto& s = m_slots[index];
	if (s.states_num == 0) {
		return s.last_state;
	}
	auto last = (s.first_state + s.states_num - 1) % STATES_MAX;
	return s.states[last];
}

void GameController::AddState(int index, const ControllerState& state) {
	EnsureSlot(index);
	auto& s = m_slots[index];

	if (s.states_num >= STATES_MAX) {
		s.states_num  = STATES_MAX - 1;
		s.first_state = (s.first_state + 1) % STATES_MAX;
	}

	auto i = (s.first_state + s.states_num) % STATES_MAX;

	s.states[i]   = state;
	s.last_state  = state;
	s.obtained[i] = false;

	s.states_num++;
}

void GameController::Button(int id, uint32_t button, bool down) {
	Common::LockGuard lock(m_mutex);

	int slot = OwnerSlot(id);
	if (slot == -1) {
		slot = FallbackSlot();
	}

	auto state = GetLastState(slot);
	state.time = LibKernel::KernelGetProcessTime();

	if (down) {
		state.buttons |= button;
	} else {
		state.buttons &= ~button;
	}

	AddState(slot, state);
}

void GameController::Axis(int id, Controller::Axis axis, int value) {
	Common::LockGuard lock(m_mutex);

	int slot = OwnerSlot(id);
	if (slot == -1) {
		slot = FallbackSlot();
	}

	auto state = GetLastState(slot);
	state.time = LibKernel::KernelGetProcessTime();

	const int axis_id = static_cast<int>(axis);
	EXIT_IF(axis_id < 0 || axis_id >= static_cast<int>(Controller::Axis::AxisMax));

	state.axes[axis_id] = value;

	if (axis == Controller::Axis::TriggerLeft) {
		if (value > 0) {
			state.buttons |= PAD_BUTTON_L2;
		} else {
			state.buttons &= ~PAD_BUTTON_L2;
		}
	}

	if (axis == Controller::Axis::TriggerRight) {
		if (value > 0) {
			state.buttons |= PAD_BUTTON_R2;
		} else {
			state.buttons &= ~PAD_BUTTON_R2;
		}
	}

	AddState(slot, state);
}

void GameController::ResetInputState() {
	Common::LockGuard lock(m_mutex);

	for (auto& s : m_slots) {
		s.states_num  = 0;
		s.first_state = 0;
		s.last_state  = ControllerState();
	}

	EnsureSlot(0);
	ControllerState state {};
	state.time = LibKernel::KernelGetProcessTime();
	AddState(0, state);
}

void GameController::GetConnectionInfo(bool* flag, int* count) {
	EXIT_IF(flag == nullptr);
	EXIT_IF(count == nullptr);

	Common::LockGuard lock(m_mutex);

	*flag  = true;
	*count = PresentCount();
}

void GameController::ReadState(ControllerState* state, bool* flag, int* count) {
	ReadStateForSlot(0, state, flag, count);
}

int GameController::ReadStates(ControllerState* states, int states_num, bool* flag, int* count) {
	return ReadStatesForSlot(0, states, states_num, flag, count);
}

int GameController::ReadStateForSlot(int index, ControllerState* state, bool* flag, int* count) {
	EXIT_IF(flag == nullptr);
	EXIT_IF(count == nullptr);
	EXIT_IF(state == nullptr);

	Common::LockGuard lock(m_mutex);

	EnsureSlot(index);

	*flag  = SlotConnected(index);
	*count = PresentCount();
	*state = GetLastState(index);

	return 0;
}

int GameController::ReadStatesForSlot(int index, ControllerState* states, int states_num,
	                                     bool* flag, int* count) {
	EXIT_IF(flag == nullptr);
	EXIT_IF(count == nullptr);
	EXIT_IF(states == nullptr);
	EXIT_IF(states_num < 1 || states_num > STATES_MAX);

	Common::LockGuard lock(m_mutex);

	if (index < 0 || index >= PAD_MAX_CONTROLLERS) {
		*flag  = false;
		*count = PresentCount();
		return 0;
	}

	EnsureSlot(index);

	*flag  = SlotConnected(index);
	*count = PresentCount();

	int ret_num = 0;

	auto& s = m_slots[index];
	if (SlotConnected(index) && s.states_num != 0) {
		for (uint32_t i = 0; i < s.states_num; i++) {
			if (ret_num >= states_num) {
				break;
			}
			auto state_index = (s.first_state + i) % STATES_MAX;
			if (!s.obtained[state_index]) {
				s.obtained[state_index] = true;
				states[ret_num++]       = s.states[state_index];
			}
		}
	}

	return ret_num;
}
int KYTY_SYSV_ABI PadInit() {
	PRINT_NAME();

	return OK;
}

static bool PadOpenArgsAreValid(int user_id, int type, int index) {
	constexpr int user_id_system     = 0xff;
	constexpr int port_type_standard = 0;
	constexpr int port_type_special  = 2;
	constexpr int port_type_remote   = 16;
	// Local multiplayer: each of the up to 4 local users (1000..1003) may open
	// exactly one personal pad port, selected by index. The system user may still
	// open the single remote-control port.
	const bool personal_port = ControllerIndexFromUserId(user_id) >= 0 && index == 0 &&
	                           (type == port_type_standard || type == port_type_special);
	const bool system_remote_control =
	    user_id == user_id_system && type == port_type_remote && index == 0;
	return personal_port || system_remote_control;
}

int KYTY_SYSV_ABI PadOpen(int user_id, int type, int index, const void* param) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n"
	     "\t type    = %d\n"
	     "\t index   = %d\n"
	     "\t param   = 0x%016" PRIx64 "\n",
	     user_id, type, index, reinterpret_cast<uint64_t>(param));

	constexpr int pad_error_invalid_arg = -2137915391; /* 0x80920001 */

	if (!PadOpenArgsAreValid(user_id, type, index)) {
		return pad_error_invalid_arg;
	}

	// Handle N corresponds to player N (user id 1000 + N - 1). The remote-control
	// port keeps handle 1. Deriving from the user (not the port index) matches the
	// PS5 pad service, where every player opens its standard port with index 0.
	const int handle = (user_id == 0xff)
	                       ? PAD_HANDLE_BASE
	                       : (ControllerIndexFromUserId(user_id) + PAD_HANDLE_BASE);

	return handle;
}

int KYTY_SYSV_ABI PadGetHandle(int user_id, int type, int index) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n"
	     "\t type    = %d\n"
	     "\t index   = %d\n",
	     user_id, type, index);

	constexpr int pad_error_device_no_handle = -2137915384; /* 0x80920008 */

	if (!PadOpenArgsAreValid(user_id, type, index)) {
		return pad_error_device_no_handle;
	}

	return (user_id == 0xff)
	           ? PAD_HANDLE_BASE
	           : (ControllerIndexFromUserId(user_id) + PAD_HANDLE_BASE);
}

int KYTY_SYSV_ABI PadSetMotionSensorState(int handle, bool enable) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	LOGF("\t enable = %s\n", (enable ? "true" : "false"));

	return OK;
}

int KYTY_SYSV_ABI PadSetAngularVelocityDeadbandState(int handle, bool enable) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	LOGF("\t enable = %s\n", (enable ? "true" : "false"));

	return OK;
}

int KYTY_SYSV_ABI PadResetOrientation(int handle) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	return OK;
}

int KYTY_SYSV_ABI PadGetControllerInformation(int handle, PadControllerInformation* info) {
	PRINT_NAME();

	EXIT_IF(g_controller == nullptr);

	const int index = ControllerIndexFromHandle(handle);
	if (index < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (info == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	int             connected_count = 0;
	bool            connected       = false;
	ControllerState state;

	g_controller->ReadStateForSlot(index, &state, &connected, &connected_count);

	std::memset(info, 0, sizeof(*info));

	info->touch_pixel_density   = 44.86f;
	info->touch_resolution_x    = 1920;
	info->touch_resolution_y    = 943;
	info->stick_dead_zone_left  = controller_get_axis(-32768, 32767, 8000) - 128;
	info->stick_dead_zone_right = controller_get_axis(-32768, 32767, 8000) - 128;
	info->connection_type       = 0;
	info->connected_count       = pad_connected_count_to_u8(connected_count);
	info->connected             = connected;
	info->device_class          = 0;

	return OK;
}

int KYTY_SYSV_ABI PadReadState(int handle, PadData* data) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (data == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	EXIT_IF(g_controller == nullptr);

	int             connected_count = 0;
	bool            connected       = false;
	ControllerState state;

	g_controller->ReadStateForSlot(ControllerIndexFromHandle(handle), &state, &connected,
	                               &connected_count);

	pad_fill_data(data, state, connected, connected_count);

	return OK;
}

int KYTY_SYSV_ABI PadRead(int handle, PadData* data, int num) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(num < 1 || num > 64);
	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (data == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	std::memset(data, 0, sizeof(PadData) * static_cast<size_t>(num));

	EXIT_IF(g_controller == nullptr);

	int             connected_count = 0;
	bool            connected       = false;
	ControllerState states[64]      = {};

	int ret_num = g_controller->ReadStatesForSlot(ControllerIndexFromHandle(handle), states, num,
	                                              &connected, &connected_count);

	if (!connected || ret_num == 0) {
		if (connected) {
			g_controller->ReadStateForSlot(ControllerIndexFromHandle(handle), &states[0], &connected,
			                               &connected_count);
		}
		ret_num = 1;
	}

	for (int i = 0; i < ret_num; i++) {
		pad_fill_data(&data[i], states[i], connected, connected_count);
	}

	return ret_num;
}

int KYTY_SYSV_ABI PadSetVibration(int handle, const PadVibrationParam* param) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (param == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	LOGF("\t large_motor = %d\n"
	     "\t small_motor = %d\n",
	     static_cast<int>(param->large_motor), static_cast<int>(param->small_motor));

	// Drive the real DualSense rumble actuators if a device is attached.
	auto* ds = static_cast<Libs::DualSense::DualSenseDriver*>(DualSenseDriverInstance());
	if (ds != nullptr && ds->IsOpen()) {
		Libs::DualSense::VibrationParam v;
		v.large_motor = param->large_motor;
		v.small_motor = param->small_motor;
		ds->SetVibration(v);
	}

	return OK;
}

int KYTY_SYSV_ABI PadResetLightBar(int handle) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	return OK;
}

int KYTY_SYSV_ABI PadSetLightBar(int handle, const PadLightBarParam* param) {
	PRINT_NAME();

	if (ControllerIndexFromHandle(handle) < 0) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (param == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	// Drive the real DualSense lightbar if a device is attached.
	auto* ds = static_cast<Libs::DualSense::DualSenseDriver*>(DualSenseDriverInstance());
	if (ds != nullptr && ds->IsOpen()) {
		Libs::DualSense::LightBarColor c;
		// PadLightBarParam carries RGB fields (r/g/b) per the pad data layout.
		c.r = param->r;
		c.g = param->g;
		c.b = param->b;
		ds->SetLightBar(c);
	}

	return OK;
}

} // namespace Libs::Controller
