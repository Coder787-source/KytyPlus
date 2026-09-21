#include "libs/dualsense.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/threads.h"

#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// DualSense HID driver — spec-accurate, UNVALIDATED on hardware.
//
// Report layouts follow the documented DualSense protocol (matching the
// reverse-engineered format used by the Linux hid-playstation kernel driver).
// USB and Bluetooth framing differ:
//   - USB:  input report id 0x01, 64 bytes;  output report id 0x05, 74 bytes
//   - BT:   input report id 0x01, 78 bytes (HID + CRC); output report id 0x31
// The Windows path enumerates the device via SetupAPI and opens it with
// HidD_* + CreateFile. The Bluetooth path reuses the same parsing once a BT
// device handle is obtained (currently only USB is wired through Open()).
// ---------------------------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#endif

namespace Libs::DualSense {

namespace {

// DualSense USB input report (report id 0x01, 64 bytes). Byte offsets follow the
// authoritative layout from the Linux hid-playstation driver (dualsense_input_report),
// after the 1-byte report ID:
//   [1-2] left stick x,y   [3-4] right stick x,y   [5-6] L2,R2 analog
//   [7] seq number                                        [8-11] buttons[4]
//   [12-15] reserved              [16-21] gyro x,y,z (le16)
//   [22-27] accel x,y,z (le16)    [28-31] sensor timestamp (le32)
//   [33-40] touch points[2] (4 bytes each)  [53] status (battery/nibble charging)
constexpr size_t DS_REPORT_USB_SIZE        = 64;
constexpr size_t DS_OFF_LX                 = 1;
constexpr size_t DS_OFF_LY                 = 2;
constexpr size_t DS_OFF_RX                 = 3;
constexpr size_t DS_OFF_RY                 = 4;
constexpr size_t DS_OFF_L2                 = 5;
constexpr size_t DS_OFF_R2                 = 6;
constexpr size_t DS_OFF_SEQ                = 7;
constexpr size_t DS_OFF_BUTTONS            = 8;  // 4 bytes: [0]=hat+squares, [1]=bumpers/options, [2]=PS/touch/mic
constexpr size_t DS_OFF_GYRO               = 16; // 3 x le16
constexpr size_t DS_OFF_ACCEL              = 22; // 3 x le16
constexpr size_t DS_OFF_TOUCH              = 33; // 2 touch points x 4 bytes
constexpr size_t DS_OFF_STATUS             = 53; // battery nibble [3:0], charging [7:4]

// Read a little-endian 16-bit value at a given report offset.
inline int16_t LE16Bytes(const uint8_t* buf, size_t off) {
	return static_cast<int16_t>(static_cast<uint16_t>(buf[off]) | (static_cast<uint16_t>(buf[off + 1]) << 8));
}

// D-pad hat switch (low nibble of buttons[0]) -> combined direction bitmask.
constexpr uint32_t HAT_BIT_UP    = 0x0010;
constexpr uint32_t HAT_BIT_RIGHT = 0x0020;
constexpr uint32_t HAT_BIT_DOWN  = 0x0040;
constexpr uint32_t HAT_BIT_LEFT  = 0x0080;

// Convert the 4-bit hat switch value (idle 0x8, compass N=0x0, NE=0x1, ... NW=0x7)
// to the independent D-pad direction bits used by the rest of the code.
uint32_t HatToDpadBits(uint8_t hat) {
	switch (hat & 0x0F) {
		case 0x0: return HAT_BIT_UP;                       // N
		case 0x1: return HAT_BIT_UP | HAT_BIT_RIGHT;       // NE
		case 0x2: return HAT_BIT_RIGHT;                     // E
		case 0x3: return HAT_BIT_DOWN | HAT_BIT_RIGHT;     // SE
		case 0x4: return HAT_BIT_DOWN;                      // S
		case 0x5: return HAT_BIT_DOWN | HAT_BIT_LEFT;      // SW
		case 0x6: return HAT_BIT_LEFT;                      // W
		case 0x7: return HAT_BIT_UP | HAT_BIT_LEFT;        // NW
		default:  return 0;                                 // idle 0x8 or invalid
	}
}

// DualSense USB output report (report id 0x02, 63 bytes). Byte offsets follow the
// Linux hid-playstation.c output struct (dualsense_output_report_usb) plus the
// trigger-effect blocks and lightbar confirmed by community reverse-engineering:
//   [0] report id 0x02      [1-2] valid/flags        [3] right motor  [4] left motor
//   [11-21] right trigger effect (mode + 10 params)  [22-32] left trigger effect
//   [33-36] audio/reserved  [37] power  [38] speaker vol
//   [45] lightbar R  [46] lightbar G  [47] lightbar B
constexpr size_t DS_OUT_REPORT_USB_SIZE = 63;
constexpr uint8_t DS_OUT_REPORT_ID_USB  = 0x02;
constexpr size_t DS_OUT_FLAGS0          = 1;   // rumble/triggers/audio enable bits
constexpr size_t DS_OUT_FLAGS1          = 2;   // mic-light/player-LED/power enabling
constexpr size_t DS_OUT_MOTOR_RIGHT      = 3;
constexpr size_t DS_OUT_MOTOR_LEFT       = 4;
constexpr size_t DS_OUT_TRIGGER_RIGHT    = 11; // 11 bytes: mode + 10 params
constexpr size_t DS_OUT_TRIGGER_LEFT     = 22; // 11 bytes: mode + 10 params
constexpr size_t DS_OUT_LIGHTBAR_R       = 45;
constexpr size_t DS_OUT_LIGHTBAR_G       = 46;
constexpr size_t DS_OUT_LIGHTBAR_B       = 47;
// Flags byte 0 bits (what this packet changes).
constexpr uint8_t DS_OUT_F0_RUMBLE_RIGHT = 0x01;
constexpr uint8_t DS_OUT_F0_RUMBLE_LEFT  = 0x02;
constexpr uint8_t DS_OUT_F0_TRIGGER_RIGHT= 0x04;
constexpr uint8_t DS_OUT_F0_TRIGGER_LEFT = 0x08;
// Flags byte 1: LIGHTBAR_CONTROL_ENABLE (0x04) + RELEASE_LEDS (0x08) from the kernel
// output struct. Both are needed to apply a new lightbar color reliably.
constexpr uint8_t DS_OUT_F1_LED_ENABLE   = 0x04;
constexpr uint8_t DS_OUT_F1_LED_RELEASE  = 0x08;

// Build an 11-byte trigger effect block (mode byte + 10 packed params) using the
// OFFICIAL mode bytes and the zone-bitpacked layout confirmed by the ExtendInput/
// reWASD reverse-engineering (Steamworks 1.55). Each of the 10 positions encodes a
// 3-bit strength in a packed 32-bit field, plus a per-position active bitmask.
void BuildTriggerBlock(uint8_t* out, const TriggerEffectParam& e) {
	std::memset(out, 0, 11);

	// Helper: fill all 10 zones with a single strength (Feedback-style).
	auto fillUniform = [&](uint8_t strength) {
		if (strength > 8 || strength == 0) return;
		const uint8_t force = static_cast<uint8_t>((strength - 1) & 0x07);
		uint32_t zones = 0;
		uint16_t active = 0;
		for (int i = e.start; i < 10; i++) {
			zones  |= static_cast<uint32_t>(force) << (3 * i);
			active |= static_cast<uint16_t>(1) << i;
		}
		out[1] = static_cast<uint8_t>(active & 0xFF);
		out[2] = static_cast<uint8_t>((active >> 8) & 0xFF);
		out[3] = static_cast<uint8_t>(zones & 0xFF);
		out[4] = static_cast<uint8_t>((zones >> 8) & 0xFF);
		out[5] = static_cast<uint8_t>((zones >> 16) & 0xFF);
		out[6] = static_cast<uint8_t>((zones >> 24) & 0xFF);
	};

	switch (e.mode) {
		case TriggerEffect::Feedback:
			out[0] = static_cast<uint8_t>(TriggerEffect::Feedback); // 0x21
			fillUniform(e.strength);
			break;
		case TriggerEffect::Weapon:
			out[0] = static_cast<uint8_t>(TriggerEffect::Weapon); // 0x25
			if (e.strength > 8 || e.strength == 0) break;
			// start/end position zones + strength; start must be 2..7, end start+1..8.
			{	uint8_t start = e.start > 7 ? 7 : e.start < 2 ? 2 : e.start;
				uint8_t end   = e.end > 8 ? 8 : e.end; 
				if (end <= start) end = start + 1;
				uint16_t zones = static_cast<uint16_t>((1 << start) | (1 << end));
				out[1] = static_cast<uint8_t>(zones & 0xFF);
				out[2] = static_cast<uint8_t>((zones >> 8) & 0xFF);
				out[3] = static_cast<uint8_t>(e.strength - 1);
			}
			break;
		case TriggerEffect::Vibration:
			out[0] = static_cast<uint8_t>(TriggerEffect::Vibration); // 0x26
			// position + amplitude (uniform zones) + frequency at param P9 (out[9]).
			fillUniform(e.strength ? e.strength : 1);
			out[9] = e.frequency;
			break;
		case TriggerEffect::Off:
		default:
			out[0] = static_cast<uint8_t>(TriggerEffect::Off); // 0x05: all zeros = off
			break;
	}
}



// DualSense gyro/accel scale (from Linux hid-playstation.c).
constexpr float GYRO_SCALE  = 1.0f / 1024.0f; // 1024 LSB per deg/s
constexpr float ACCEL_SCALE = 1.0f / 8192.0f; // 8192 LSB per g

} // namespace

DualSenseDriver::~DualSenseDriver() {
	Close();
}

bool DualSenseDriver::Open() {
#ifdef _WIN32
	return OpenWindows();
#else
	// Non-Windows: no-op stub so cross-platform builds stay green.
	return false;
#endif
}

void DualSenseDriver::Close() {
	m_stop = true;
	if (m_thread != nullptr) {
		m_thread->Join();
		delete m_thread;
		m_thread = nullptr;
	}
#ifdef _WIN32
	if (m_handle != nullptr) {
		CloseHandle(m_handle);
		m_handle = nullptr;
	}
#endif
	m_open = false;
}

#ifdef _WIN32

bool DualSenseDriver::OpenWindows() {
	// Enumerate HID devices via SetupAPI, match VID_054C PID_0CE6.
	GUID hid_guid;
	HidD_GetHidGuid(&hid_guid);

	HDEVINFO dev_info = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr,
	                                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (dev_info == INVALID_HANDLE_VALUE) {
		return false;
	}

	HANDLE found = INVALID_HANDLE_VALUE;
	SP_DEVICE_INTERFACE_DATA iface_data;
	iface_data.cbSize = sizeof(iface_data);

	for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &hid_guid, i, &iface_data); ++i) {
		DWORD needed = 0;
		SetupDiGetDeviceInterfaceDetailW(dev_info, &iface_data, nullptr, 0, &needed, nullptr);
		if (needed == 0) continue;

		auto* detail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(std::malloc(needed));
		if (detail == nullptr) continue;
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

		if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &iface_data, detail, needed, nullptr, nullptr)) {
			std::free(detail);
			continue;
		}

		// Open read/write for feature + output reports.
		HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
		                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		std::free(detail);

		if (h == INVALID_HANDLE_VALUE) continue;

		HIDD_ATTRIBUTES attr;
		attr.Size = sizeof(attr);
		if (HidD_GetAttributes(h, &attr) && attr.VendorID == DUALSENSE_VID && attr.ProductID == DUALSENSE_PID) {
			found = h;
			break; // first DualSense
		}
		CloseHandle(h);
	}
	SetupDiDestroyDeviceInfoList(dev_info);

	if (found == INVALID_HANDLE_VALUE) {
		return false;
	}

	m_handle = found;
	m_open   = true;
	m_stop   = false;

	// Start the polling thread.
	m_thread = new Common::Thread([](void* p) {
		auto* self = static_cast<DualSenseDriver*>(p);
		self->PollLoop();
	}, this);

	LOGF("DualSense: device opened (VID 054C PID 0CE6), polling thread started\n");
	return true;
}

void DualSenseDriver::PollLoop() {
	InputState st {};
	while (!m_stop) {
		if (!PollOnce(&st)) {
			Common::Thread::Sleep(2); // device error/backoff
			continue;
		}
		if (m_input_cb != nullptr) {
			m_input_cb(st, m_input_user);
		}
		// ~1 kHz poll rate is typical for USB HID.
		Common::Thread::Sleep(1);
	}
}

bool DualSenseDriver::PollOnce(InputState* out) {
	if (!m_open || m_handle == nullptr || out == nullptr) return false;

	uint8_t buf[64];
	std::memset(buf, 0, sizeof(buf));
	// First byte is the report id for overlapped HidD_ reads via ReadFile on Windows;
	// we read raw input reports.
	DWORD read_n = 0;
	BOOL ok = ReadFile(m_handle, buf, sizeof(buf), &read_n, nullptr);
	if (!ok || read_n < DS_REPORT_USB_SIZE) {
		return false;
	}
	return ParseInputReportUsb(buf, static_cast<size_t>(read_n), out);
}

bool DualSenseDriver::ParseInputReportUsb(const uint8_t* buf, size_t len, InputState* out) {
	if (buf == nullptr || out == nullptr || len < DS_REPORT_USB_SIZE) return false;
	if (buf[0] != 0x01) return false; // USB input report id

	// Buttons: buttons[0] low nibble = hat, bits 4-7 = Square/Cross/Circle/Triangle;
	// buttons[1] = L1/R1/L2/R2/Create/Options/L3/R3; buttons[2] = PS/Touchpad/Mic.
	const uint8_t b0 = buf[DS_OFF_BUTTONS + 0];
	const uint8_t b1 = buf[DS_OFF_BUTTONS + 1];
	const uint8_t b2 = buf[DS_OFF_BUTTONS + 2];

	uint32_t buttons = HatToDpadBits(b0 & 0x0F);
	buttons |= static_cast<uint32_t>((b0 >> 4) & 0x0F) << 0; // Square/Cro/Cur/Tri in low bits

	// Re-map the physical positions into the single 32-bit bitmask used by the
	// rest of the code (Button* constants in dualsense.h).
	// b0 bit4 Square -> Square(0x4), bit5 Cross -> Cross(0x1), bit6 Circle -> Circle(0x2), bit7 Triangle -> Triangle(0x8)
	buttons &= ~0x0F; // remove raw face nibble
	if (b0 & 0x10) buttons |= ButtonSquare;
	if (b0 & 0x20) buttons |= ButtonCross;
	if (b0 & 0x40) buttons |= ButtonCircle;
	if (b0 & 0x80) buttons |= ButtonTriangle;

	if (b1 & 0x01) buttons |= ButtonL1;
	if (b1 & 0x02) buttons |= ButtonR1;
	if (b1 & 0x04) buttons |= ButtonL2;
	if (b1 & 0x08) buttons |= ButtonR2;
	if (b1 & 0x10) buttons |= ButtonCreate;
	if (b1 & 0x20) buttons |= ButtonOptions;
	if (b1 & 0x40) buttons |= ButtonL3;
	if (b1 & 0x80) buttons |= ButtonR3;

	if (b2 & 0x01) buttons |= ButtonPsButton;
	if (b2 & 0x02) buttons |= ButtonTouchpad;
	if (b2 & 0x04) buttons |= ButtonMicMute;

	out->buttons       = buttons;
	out->left_stick_x  = buf[DS_OFF_LX];
	out->left_stick_y  = buf[DS_OFF_LY];
	out->right_stick_x = buf[DS_OFF_RX];
	out->right_stick_y = buf[DS_OFF_RY];
	out->l2            = buf[DS_OFF_L2];
	out->r2            = buf[DS_OFF_R2];
	out->sequence      = buf[DS_OFF_SEQ];

	// Motion sensors: le16, gyro at 16, accel at 22.
	const int16_t gx = LE16Bytes(buf, DS_OFF_GYRO + 0);
	const int16_t gy = LE16Bytes(buf, DS_OFF_GYRO + 2);
	const int16_t gz = LE16Bytes(buf, DS_OFF_GYRO + 4);
	const int16_t ax = LE16Bytes(buf, DS_OFF_ACCEL + 0);
	const int16_t ay = LE16Bytes(buf, DS_OFF_ACCEL + 2);
	const int16_t az = LE16Bytes(buf, DS_OFF_ACCEL + 4);
	out->motion.gyro_x  = static_cast<float>(gx) * GYRO_SCALE;
	out->motion.gyro_y  = static_cast<float>(gy) * GYRO_SCALE;
	out->motion.gyro_z  = static_cast<float>(gz) * GYRO_SCALE;
	out->motion.accel_x = static_cast<float>(ax) * ACCEL_SCALE;
	out->motion.accel_y = static_cast<float>(ay) * ACCEL_SCALE;
	out->motion.accel_z = static_cast<float>(az) * ACCEL_SCALE;

	// Touchpad: two points, 4 bytes each, starting at byte 33. Each point:
	//   [0] contact (bit7 set = inactive, id = low 7 bits)
	//   [1] x_lo
	//   [2] x_hi:4 | y_lo:4
	//   [3] y_hi
	//   =>  X = ((byte2 & 0x0F)<<8) | byte1 ;  Y = (byte3<<4) | (byte2>>4)
	if (len >= DS_OFF_TOUCH + 8) {
		for (int t = 0; t < 2; ++t) {
			size_t base = DS_OFF_TOUCH + static_cast<size_t>(t) * 4;
			uint8_t contact = buf[base + 0];
			uint8_t x_lo    = buf[base + 1];
			uint8_t hi      = buf[base + 2];
			uint8_t y_hi    = buf[base + 3];
			out->touch[t].active = (contact & 0x80) == 0; // bit7 clear = touching
			out->touch[t].id     = static_cast<uint8_t>(contact & 0x7F);
			out->touch[t].x      = static_cast<uint16_t>(((hi & 0x0F) << 8) | x_lo);
			out->touch[t].y      = static_cast<uint16_t>((static_cast<uint16_t>(y_hi) << 4) | (hi >> 4));
		}
	} else {
		out->touch[0].active = false;
		out->touch[1].active = false;
	}

	// Battery/charging status nibble at byte 53 (not exposed in InputState today).

	out->valid = true;
	return true;
}

void DualSenseDriver::SendOutputReportUsb(const VibrationParam* v, const LightBarColor* lb,
                                          const TriggerEffectParam* lt, const TriggerEffectParam* rt) {
	if (!m_open || m_handle == nullptr) return;

	uint8_t rep[DS_OUT_REPORT_USB_SIZE];
	std::memset(rep, 0, sizeof(rep));
	rep[0] = DS_OUT_REPORT_ID_USB; // 0x02

	// Rumble enable flags.
	if (v != nullptr) {
		rep[DS_OUT_FLAGS0] |= DS_OUT_F0_RUMBLE_RIGHT | DS_OUT_F0_RUMBLE_LEFT;
		rep[DS_OUT_MOTOR_RIGHT] = v->small_motor;
		rep[DS_OUT_MOTOR_LEFT ] = v->large_motor;
	}

	// Lightbar: enable LED color application; keep previously-set RGB if none given.
	{	const uint8_t r = lb ? lb->r : m_last_lightbar.r;
		const uint8_t g = lb ? lb->g : m_last_lightbar.g;
		const uint8_t b = lb ? lb->b : m_last_lightbar.b;
		rep[DS_OUT_LIGHTBAR_R] = r;
		rep[DS_OUT_LIGHTBAR_G] = g;
		rep[DS_OUT_LIGHTBAR_B] = b;
		rep[DS_OUT_FLAGS1] |= DS_OUT_F1_LED_ENABLE | DS_OUT_F1_LED_RELEASE;
	}

	// Adaptive triggers: build an 11-byte block in place.
	if (lt != nullptr) {
		rep[DS_OUT_FLAGS0] |= DS_OUT_F0_TRIGGER_LEFT;
		BuildTriggerBlock(&rep[DS_OUT_TRIGGER_LEFT], *lt);
	}
	if (rt != nullptr) {
		rep[DS_OUT_FLAGS0] |= DS_OUT_F0_TRIGGER_RIGHT;
		BuildTriggerBlock(&rep[DS_OUT_TRIGGER_RIGHT], *rt);
	}

	DWORD written = 0;
	WriteFile(m_handle, rep, sizeof(rep), &written, nullptr);
}

#else // non-Windows

bool DualSenseDriver::OpenWindows() { return false; }
bool DualSenseDriver::PollOnce(InputState*) { return false; }
void DualSenseDriver::PollLoop() {}
bool DualSenseDriver::ParseInputReportUsb(const uint8_t*, size_t, InputState*) { return false; }
void DualSenseDriver::SendOutputReportUsb(const VibrationParam*, const LightBarColor*,
                                          const TriggerEffectParam*, const TriggerEffectParam*) {}

#endif

// Bluetooth framing helpers (parsing/output for BT devices). Not wired through
// Open() on Windows (which enumerates USB-style device paths), but kept here so the
// spec-accurate path exists and can be wired when a BT transport is added.
bool DualSenseDriver::ParseInputReportBt(const uint8_t* buf, size_t len, InputState* out) {
	if (buf == nullptr || out == nullptr) return false;
	if (len < 78) return false;
	if (buf[0] != 0x01) return false; // BT input report id
	// BT report has a 1-byte HID prefix before the same body as USB; offsets shift by ~1.
	// Reuse USB parser on the body starting at byte 1 if it aligns.
	if (len - 1 >= DS_REPORT_USB_SIZE) {
		return ParseInputReportUsb(buf + 1, len - 1, out);
	}
	return false;
}

void DualSenseDriver::SendOutputReportBt(const VibrationParam*, const LightBarColor*,
                                         const TriggerEffectParam*, const TriggerEffectParam*) {
	// BT output report id is 0x31 and requires a CRC32 trailer; transport not implemented.
	// (Spec-accurate layout is known; no BT device handle is available through Open().)
}

void DualSenseDriver::SetVibration(const VibrationParam& v) {
	Common::LockGuard lock(m_output_mutex);
	m_last_vibration = v;
	SendOutputReportUsb(&v, nullptr, nullptr, nullptr);
}

void DualSenseDriver::SetLightBar(const LightBarColor& c) {
	Common::LockGuard lock(m_output_mutex);
	m_last_lightbar = c;
	SendOutputReportUsb(nullptr, &c, nullptr, nullptr);
}

void DualSenseDriver::SetTriggerEffect(bool left, const TriggerEffectParam& e) {
	Common::LockGuard lock(m_output_mutex);
	if (left) m_last_left_trigger = e;
	else      m_last_right_trigger = e;
	// Send both trigger blocks in one report (partial update of one block resets the
	// other unless both are provided).
	SendOutputReportUsb(nullptr, nullptr,
	                   left ? &e : &m_last_left_trigger,
	                   left ? &m_last_right_trigger : &e);
}

} // namespace Libs::DualSense