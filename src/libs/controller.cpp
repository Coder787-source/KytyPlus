#include "libs/controller.h"

#include "common/assert.h"
#include "common/common.h"
#include "common/logging/log.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "kernel/pthread.h"
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

struct PadLightBarParam {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t reserve;
};

struct ControllerState {
	uint64_t time                                  = 0;
	uint32_t buttons                               = 0;
	int      axes[static_cast<int>(Axis::AxisMax)] = {128, 128, 128, 128, 0, 0};
};

class GameController {
public:
	GameController()          = default;
	virtual ~GameController() = default;

	KYTY_CLASS_NO_COPY(GameController);

	void Connect(int id);
	void Disconnect(int id);
	void Button(int id, uint32_t button, bool down);
	void Axis(int id, Axis axis, int value);
	[[nodiscard]] int GetActiveId() const;
	void SetMotionEnabled(bool enabled);
	[[nodiscard]] bool GetMotionEnabled() const;
	void MicButton(int id, bool down);
	void GetConnectionInfo(bool* flag, int* count);
	void ReadState(ControllerState* state, bool* flag, int* count);
	int  ReadStates(ControllerState* states, int states_num, bool* flag, int* count);

private:
	static constexpr uint32_t STATES_MAX = 64;

	struct StatePrivate {
		bool obtained = false;
	};

	void                          CheckActive();
	[[nodiscard]] ControllerState GetLastState() const;
	void                          AddState(const ControllerState& state);

	mutable Common::Mutex m_mutex;
	std::vector<int>      m_connected_ids;
	int              m_active_id       = -1;
	bool             m_connected       = false;
	int              m_connected_count = 0;
	bool             m_motion_enabled  = false;
	bool             m_mic_muted       = false;
	ControllerState  m_states[STATES_MAX];
	StatePrivate     m_private[STATES_MAX];
	ControllerState  m_last_state;
	uint32_t         m_states_num  = 0;
	uint32_t         m_first_state = 0;
};

static GameController* g_controller = nullptr;

static constexpr int KEYBOARD_CONTROLLER_ID = -1000;

static uint8_t pad_connected_count_to_u8(int connected_count) {
	return static_cast<uint8_t>(connected_count > 255 ? 255 : connected_count);
}

static void pad_fill_data(PadData* data, const ControllerState& state, bool connected,
                          int connected_count, const ControllerExtendedState* ext) {
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
	data->connected              = connected;
	data->timestamp              = state.time;
	data->connected_count        = pad_connected_count_to_u8(connected_count);
	data->device_unique_data_len = 0;

	if (ext != nullptr) {
		if (ext->motion_valid) {
			data->angular_velocity_x = ext->gyro_x;
			data->angular_velocity_y = ext->gyro_y;
			data->angular_velocity_z = ext->gyro_z;
			data->acceleration_x     = ext->accel_x;
			data->acceleration_y     = ext->accel_y;
			data->acceleration_z     = ext->accel_z;
		}

		uint8_t touch_num = 0;
		if (ext->touch0_active) {
			data->touch_data_touch0_x  = ext->touch0_x;
			data->touch_data_touch0_y  = ext->touch0_y;
			data->touch_data_touch0_id = 1;
			touch_num++;
		}
		if (ext->touch1_active) {
			data->touch_data_touch1_x  = ext->touch1_x;
			data->touch_data_touch1_y  = ext->touch1_y;
			data->touch_data_touch1_id = 2;
			touch_num++;
		}
		data->touch_data_touch_num = touch_num;

		// DualSense Edge back paddles/Fn buttons have no documented bit in Sony's real ScePad
		// ABI (see ControllerExtendedState's doc comment), so they're surfaced through the
		// device-unique-data extension bytes that PadDeviceClassParseData (libPad.cpp) already
		// forwards to games as a bitmask, rather than a fabricated PAD_BUTTON_* bit.
		if (ext->edge_paddle_right || ext->edge_paddle_left || ext->edge_function_right ||
		    ext->edge_function_left) {
			uint8_t edge_bits = 0;
			edge_bits |= ext->edge_paddle_right ? 0x01 : 0;
			edge_bits |= ext->edge_paddle_left ? 0x02 : 0;
			edge_bits |= ext->edge_function_right ? 0x04 : 0;
			edge_bits |= ext->edge_function_left ? 0x08 : 0;

			data->device_unique_data_len = 1;
			data->device_unique_data[0]  = edge_bits;
		}
	}
}

KYTY_SUBSYSTEM_INIT(Controller) {
	EXIT_IF(g_controller != nullptr);

	g_controller = new GameController;
	g_controller->Connect(KEYBOARD_CONTROLLER_ID);
}

KYTY_SUBSYSTEM_UNEXPECTED_SHUTDOWN(Controller) {}

KYTY_SUBSYSTEM_DESTROY(Controller) {}

void GameController::Connect(int id) {
	Common::LockGuard lock(m_mutex);

	if (std::find(m_connected_ids.begin(), m_connected_ids.end(), id) != m_connected_ids.end()) {
		return;
	}

	m_connected_ids.push_back(id);

	CheckActive();
}

void GameController::Disconnect(int id) {
	Common::LockGuard lock(m_mutex);

	const auto it = std::find(m_connected_ids.begin(), m_connected_ids.end(), id);
	EXIT_IF(it == m_connected_ids.end());

	m_connected_ids.erase(it);

	CheckActive();
}

void GameController::CheckActive() {
	bool reset         = false;
	int  new_active_id = -1;
	bool new_connected = false;

	if (!m_connected_ids.empty()) {
		new_active_id = m_connected_ids[0];
		for (const auto id: m_connected_ids) {
			if (id != KEYBOARD_CONTROLLER_ID) {
				new_active_id = id;
				break;
			}
		}
		new_connected = true;
	}

	const bool was_connected = m_connected;
	const int  old_active_id = m_active_id;

	if (m_connected != new_connected || m_active_id != new_active_id) {
		m_active_id = new_active_id;
		m_connected = new_connected;
		if (!was_connected && new_connected) {
			m_connected_count++;
		}
		reset = true;
	}

	if (reset) {
		m_states_num = 0;
		m_last_state = ControllerState();

		// Player index 0 ("player 1", the center LED) is always assigned to whichever
		// physical controller is currently active, matching this emulator's single active-pad
		// model. The keyboard's pseudo-id has no LEDs, so it's skipped.
		if (old_active_id != KEYBOARD_CONTROLLER_ID && old_active_id != m_active_id) {
			ControllerSetPlayerIndex(old_active_id, -1);
		}
		if (m_active_id != KEYBOARD_CONTROLLER_ID && m_active_id >= 0) {
			ControllerSetPlayerIndex(m_active_id, 0);
		}
	}
}

ControllerState GameController::GetLastState() const {
	if (m_states_num == 0) {
		return m_last_state;
	}

	auto last = (m_first_state + m_states_num - 1) % STATES_MAX;

	return m_states[last];
}

void GameController::AddState(const ControllerState& state) {
	if (m_states_num >= STATES_MAX) {
		m_states_num  = STATES_MAX - 1;
		m_first_state = (m_first_state + 1) % STATES_MAX;
	}

	auto index = (m_first_state + m_states_num) % STATES_MAX;

	m_states[index] = state;
	m_last_state    = state;

	m_private[index].obtained = false;

	m_states_num++;
}

void GameController::Button(int id, uint32_t button, bool down) {
	Common::LockGuard lock(m_mutex);

	if (m_active_id == id) {
		auto state = GetLastState();

		state.time = LibKernel::KernelGetProcessTime();

		if (down) {
			state.buttons |= button;
		} else {
			state.buttons &= ~button;
		}

		AddState(state);
	}
}

void GameController::Axis(int id, Controller::Axis axis, int value) {
	Common::LockGuard lock(m_mutex);

	if (m_active_id == id) {
		auto state = GetLastState();

		state.time = LibKernel::KernelGetProcessTime();

		int axis_id = static_cast<int>(axis);

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

		AddState(state);
	}
}

int GameController::GetActiveId() const {
	Common::LockGuard lock(m_mutex);

	return m_active_id;
}

void GameController::SetMotionEnabled(bool enabled) {
	Common::LockGuard lock(m_mutex);

	m_motion_enabled = enabled;
}

bool GameController::GetMotionEnabled() const {
	Common::LockGuard lock(m_mutex);

	return m_motion_enabled;
}

void GameController::MicButton(int id, bool down) {
	Common::LockGuard lock(m_mutex);

	// Toggle-on-press, matching hid-playstation.c's dualsense_parse_report(): mute state flips
	// once when the button goes down, and stays put while held or on release.
	if (m_active_id == id && down) {
		m_mic_muted = !m_mic_muted;

		// mode 1 = solid LED, matching the console's own steady (non-pulsing) mic-mute
		// indicator; see DS5EffectsState_t::ucMicLightMode's documented encoding.
		DualSenseSetMicMuted(id, m_mic_muted, m_mic_muted ? 1 : 0);
	}
}

void GameController::GetConnectionInfo(bool* flag, int* count) {
	EXIT_IF(flag == nullptr);
	EXIT_IF(count == nullptr);

	Common::LockGuard lock(m_mutex);

	*flag  = m_connected;
	*count = m_connected_count;
}

void GameController::ReadState(ControllerState* state, bool* flag, int* count) {
	EXIT_IF(flag == nullptr);
	EXIT_IF(count == nullptr);
	EXIT_IF(state == nullptr);

	Common::LockGuard lock(m_mutex);

	*flag  = m_connected;
	*count = m_connected_count;
	*state = GetLastState();
}

int GameController::ReadStates(ControllerState* states, int states_num, bool* flag, int* count) {
	EXIT_IF(flag == nullptr);
	EXIT_IF(count == nullptr);
	EXIT_IF(states == nullptr);
	EXIT_IF(states_num < 1 || states_num > STATES_MAX);

	Common::LockGuard lock(m_mutex);

	*flag  = m_connected;
	*count = m_connected_count;

	int ret_num = 0;

	if (m_connected) {
		if (m_states_num != 0) {
			for (uint32_t i = 0; i < m_states_num; i++) {
				if (ret_num >= states_num) {
					break;
				}
				auto index = (m_first_state + i) % STATES_MAX;
				if (!m_private[index].obtained) {
					m_private[index].obtained = true;

					states[ret_num++] = m_states[index];
				}
			}
		}
	}

	return ret_num;
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

void ControllerMicButton(int id, bool down) {
	EXIT_IF(g_controller == nullptr);

	g_controller->MicButton(id, down);
}

int ControllerGetActiveId() {
	EXIT_IF(g_controller == nullptr);

	return g_controller->GetActiveId();
}

int KYTY_SYSV_ABI PadInit() {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI PadOpen(int user_id, int type, int index, const void* param) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n"
	     "\t type    = %d\n"
	     "\t index   = %d\n"
	     "\t param   = 0x%016" PRIx64 "\n",
	     user_id, type, index, reinterpret_cast<uint64_t>(param));

	constexpr int pad_error_invalid_arg = -2137915391; /* 0x80920001 */

	// Accept canonical login id 1000 and the secondary probe id 1 used by first-party titles
	// (Astro-family bootstrap / DualSense open paths).
	if ((user_id != 1000 && user_id != 1) || (type != 0 && type != 2 && type != 16) ||
	    index != 0) {
		return pad_error_invalid_arg;
	}

	int handle = 1;

	return handle;
}

int KYTY_SYSV_ABI PadGetHandle(int user_id, int type, int index) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n"
	     "\t type    = %d\n"
	     "\t index   = %d\n",
	     user_id, type, index);

	constexpr int pad_error_device_no_handle = -2137915384; /* 0x80920008 */

	if ((user_id != 1000 && user_id != 1) || (type != 0 && type != 2 && type != 16) ||
	    index != 0) {
		return pad_error_device_no_handle;
	}

	return 1;
}

int KYTY_SYSV_ABI PadSetMotionSensorState(int handle, bool enable) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	LOGF("\t enable = %s\n", (enable ? "true" : "false"));

	EXIT_IF(g_controller == nullptr);

	g_controller->SetMotionEnabled(enable);

	const int active_id = g_controller->GetActiveId();
	if (active_id != KEYBOARD_CONTROLLER_ID) {
		ControllerSetMotionSensorsEnabled(active_id, enable);
	}

	return OK;
}

int KYTY_SYSV_ABI PadSetAngularVelocityDeadbandState(int handle, bool enable) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	LOGF("\t enable = %s\n", (enable ? "true" : "false"));

	return OK;
}

int KYTY_SYSV_ABI PadResetOrientation(int handle) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	return OK;
}

int KYTY_SYSV_ABI PadGetControllerInformation(int handle, PadControllerInformation* info) {
	PRINT_NAME();

	EXIT_IF(g_controller == nullptr);

	int  connected_count = 0;
	bool connected       = false;

	g_controller->GetConnectionInfo(&connected, &connected_count);

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (info == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

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

static ControllerExtendedState pad_poll_active_extended_state() {
	ControllerExtendedState ext;

	const int active_id = g_controller->GetActiveId();
	if (active_id != KEYBOARD_CONTROLLER_ID) {
		ControllerPollExtendedState(active_id, g_controller->GetMotionEnabled(), &ext);
	}

	return ext;
}

int KYTY_SYSV_ABI PadReadState(int handle, PadData* data) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (data == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	EXIT_IF(g_controller == nullptr);

	int             connected_count = 0;
	bool            connected       = false;
	ControllerState state;

	g_controller->ReadState(&state, &connected, &connected_count);

	const auto ext = pad_poll_active_extended_state();
	pad_fill_data(data, state, connected, connected_count, &ext);

	return OK;
}

int KYTY_SYSV_ABI PadRead(int handle, PadData* data, int num) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(num < 1 || num > 64);
	if (handle != 1) {
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

	int ret_num = g_controller->ReadStates(states, num, &connected, &connected_count);

	if (!connected || ret_num == 0) {
		if (connected) {
			g_controller->ReadState(&states[0], &connected, &connected_count);
		}
		ret_num = 1;
	}

	// Extended/motion state (gyro, accel, touch) is only polled once "now" per call, but
	// ReadStates can return up to `num` buffered samples spanning many past frames (oldest
	// first). Stamping the same current motion snapshot onto every historical sample would
	// fabricate motion data games didn't actually see at those timestamps -- e.g. gyro-aim
	// integration across buffered samples would get identical readings repeated `ret_num`
	// times instead of the (unavailable) motion at each sample's own time. Only attach it to
	// the most recent sample and leave older ones without fabricated extended data.
	const auto ext = pad_poll_active_extended_state();
	for (int i = 0; i < ret_num; i++) {
		pad_fill_data(&data[i], states[i], connected, connected_count,
		             (i == ret_num - 1) ? &ext : nullptr);
	}

	return ret_num;
}

int KYTY_SYSV_ABI PadSetVibration(int handle, const PadVibrationParam* param) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (param == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	LOGF("\t large_motor = %d\n"
	     "\t small_motor = %d\n",
	     static_cast<int>(param->large_motor), static_cast<int>(param->small_motor));

	EXIT_IF(g_controller == nullptr);

	const int active_id = g_controller->GetActiveId();
	if (active_id != KEYBOARD_CONTROLLER_ID) {
		ControllerSetRumble(active_id, param->large_motor, param->small_motor);
	}

	return OK;
}

int KYTY_SYSV_ABI PadResetLightBar(int handle) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}

	EXIT_IF(g_controller == nullptr);

	const int active_id = g_controller->GetActiveId();
	if (active_id != KEYBOARD_CONTROLLER_ID) {
		ControllerSetLightBar(active_id, 0, 0, 0);
	}

	return OK;
}

int KYTY_SYSV_ABI PadSetLightBar(int handle, const PadLightBarParam* param) {
	PRINT_NAME();

	if (handle != 1) {
		return PAD_ERROR_INVALID_HANDLE;
	}
	if (param == nullptr) {
		return PAD_ERROR_INVALID_ARG;
	}

	LOGF("\t r = %d\n"
	     "\t g = %d\n"
	     "\t b = %d\n",
	     static_cast<int>(param->r), static_cast<int>(param->g), static_cast<int>(param->b));

	EXIT_IF(g_controller == nullptr);

	const int active_id = g_controller->GetActiveId();
	if (active_id != KEYBOARD_CONTROLLER_ID) {
		ControllerSetLightBar(active_id, param->r, param->g, param->b);
	}

	return OK;
}

} // namespace Libs::Controller
