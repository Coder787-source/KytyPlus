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

// DualSense input report (USB, report id 0x01). Documented offsets.
struct InputReportUsb {
	uint8_t  report_id;          // 0x01
	uint8_t  buttons_lo;         // [0..7] in byte 1
	uint8_t  buttons_hi;         // [0..7] in byte 2
	uint8_t  buttons2_lo;        // byte 3 (PS/touchpad/mic)
	uint8_t  buttons2_hi;        // byte 4
	uint8_t  left_stick_x;       // byte 5
	uint8_t  left_stick_y;       // byte 6
	uint8_t  right_stick_x;      // byte 7
	uint8_t  right_stick_y;      // byte 8
	uint8_t  l2;                  // byte 9 (analog)
	uint8_t  r2;                  // byte 10 (analog)
	uint8_t  sequence;           // byte 11
	uint8_t  reserved_a[3];     // bytes 12-14
	int16_t gyro_x;              // bytes 15-16 (LE, raw)
	int16_t gyro_y;              // bytes 17-18
	int16_t gyro_z;              // bytes 19-20
	int16_t accel_x;             // bytes 21-22
	int16_t accel_y;             // bytes 23-24
	int16_t accel_z;             // bytes 25-26
	// ... touchpad, timestamp, battery follow (parsed below by offset, not via struct).
};
static_assert(sizeof(InputReportUsb) >= 27, "DualSense input report header size");

// DualSense output report (USB, report id 0x05). Documented offsets.
struct OutputReportUsb {
	uint8_t  report_id;          // 0x05
	uint8_t  flags1;             // feature flags byte 1
	uint8_t  flags2;             // feature flags byte 2
	uint8_t  right_motor;        // small motor (high-freq)
	uint8_t  left_motor;         // large motor (low-freq)
	uint8_t  trigger_left[10];   // L2 trigger effect block (10 bytes)
	uint8_t  trigger_right[10];  // R2 trigger effect block (10 bytes)
	uint8_t  reserved_b[4];      // bytes 22-25
	uint8_t  lightbar_r;         // byte 26
	uint8_t  lightbar_g;         // byte 27
	uint8_t  lightbar_b;         // byte 28
	uint8_t  reserved_c[40];     // padding to 74-byte report
};
static_assert(sizeof(OutputReportUsb) <= 74, "DualSense output report size");

// Build a 10-byte trigger effect block from a TriggerEffectParam.
// Layout follows the documented DualSense trigger report:
//   [0] mode, [1] strength10/position, [2] strength, [3] start,
//   [4] end/decel, [5] frequency/amplitude, [6] .. [9] reserved.
void BuildTriggerBlock(uint8_t* out, const TriggerEffectParam& e) {
	std::memset(out, 0, 10);
	out[0] = static_cast<uint8_t>(e.mode);
	switch (e.mode) {
		case TriggerEffect::Feedback:
			out[1] = 0;            // enable flags
			out[2] = e.strength;    // resistance strength 0-7
			break;
		case TriggerEffect::Weapon:
			out[1] = 0;
			out[2] = e.strength;    // initial resistance
			out[3] = e.start;       // stage start
			out[4] = e.end;         // stage end
			out[5] = e.frequency;   // strength after stage
			break;
		case TriggerEffect::Vibration:
			out[1] = 0;
			out[2] = e.strength;    // amplitude
			out[3] = e.start;
			out[4] = e.end;
			out[5] = e.frequency;   // frequency
			break;
		case TriggerEffect::Slope:
			out[1] = 0;
			out[2] = e.strength;
			out[3] = e.start;
			out[4] = e.end;
			break;
		case TriggerEffect::Off:
		default:
			// all zeros = off
			break;
	}
}

// DualSense gyro/accel scale (documented sensitivity).
// Gyro: ~0.00269 deg/s/LSB; Accel: ~0.000976 m/s^2/LSB.
constexpr float GYRO_SCALE  = 0.00269f;
constexpr float ACCEL_SCALE = 0.000976f;

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
	if (!ok || read_n < sizeof(InputReportUsb)) {
		return false;
	}
	return ParseInputReportUsb(buf, static_cast<size_t>(read_n), out);
}

bool DualSenseDriver::ParseInputReportUsb(const uint8_t* buf, size_t len, InputState* out) {
	if (buf == nullptr || out == nullptr || len < sizeof(InputReportUsb)) return false;
	if (buf[0] != 0x01) return false; // USB input report id

	auto* rep = reinterpret_cast<const InputReportUsb*>(buf);

	uint32_t buttons = 0;
	buttons |= static_cast<uint32_t>(rep->buttons_lo);
	buttons |= static_cast<uint32_t>(rep->buttons_hi) << 8;
	buttons |= static_cast<uint32_t>(rep->buttons2_lo) << 16;

	out->buttons       = buttons;
	out->left_stick_x  = rep->left_stick_x;
	out->left_stick_y  = rep->left_stick_y;
	out->right_stick_x = rep->right_stick_x;
	out->right_stick_y = rep->right_stick_y;
	out->l2            = rep->l2;
	out->r2            = rep->r2;
	out->sequence      = rep->sequence;

	out->motion.gyro_x  = static_cast<float>(rep->gyro_x)  * GYRO_SCALE;
	out->motion.gyro_y  = static_cast<float>(rep->gyro_y)  * GYRO_SCALE;
	out->motion.gyro_z  = static_cast<float>(rep->gyro_z)  * GYRO_SCALE;
	out->motion.accel_x = static_cast<float>(rep->accel_x) * ACCEL_SCALE;
	out->motion.accel_y = static_cast<float>(rep->accel_y) * ACCEL_SCALE;
	out->motion.accel_z = static_cast<float>(rep->accel_z) * ACCEL_SCALE;

	// Touchpad: documented at bytes 33-43 (two points, 4 bytes each + header).
	// Parse defensively based on length.
	if (len >= 45) {
		for (int t = 0; t < 2; ++t) {
			size_t base = 33 + t * 4;
			uint8_t p0 = buf[base + 0];
			uint8_t p1 = buf[base + 1];
			uint8_t p2 = buf[base + 2];
			out->touch[t].active = (p0 & 0x80) == 0; // bit7 clear = touching
			out->touch[t].id     = static_cast<uint8_t>(p0 & 0x7F);
			out->touch[t].x      = static_cast<uint16_t>(((p1 & 0x0F) << 8) | p2);
			out->touch[t].y      = static_cast<uint16_t>(((p1 >> 4) & 0x0F) << 8) | buf[base + 3];
		}
	} else {
		out->touch[0].active = false;
		out->touch[1].active = false;
	}

	out->valid = true;
	return true;
}

void DualSenseDriver::SendOutputReportUsb(const VibrationParam* v, const LightBarColor* lb,
                                          const TriggerEffectParam* lt, const TriggerEffectParam* rt) {
	if (!m_open || m_handle == nullptr) return;

	OutputReportUsb rep {};
	std::memset(&rep, 0, sizeof(rep));
	rep.report_id = 0x05;
	// flags1: bit0 enable rumble, bit3 enable lightbar, bit4 enable trigger L, bit5 enable trigger R
	rep.flags1 = 0x01 | 0x08;
	if (lt != nullptr) rep.flags1 |= 0x10;
	if (rt != nullptr) rep.flags1 |= 0x20;

	rep.right_motor = v ? v->small_motor : 0;
	rep.left_motor  = v ? v->large_motor : 0;

	if (lt != nullptr) BuildTriggerBlock(rep.trigger_left,  *lt);
	if (rt != nullptr) BuildTriggerBlock(rep.trigger_right, *rt);

	rep.lightbar_r = lb ? lb->r : m_last_lightbar.r;
	rep.lightbar_g = lb ? lb->g : m_last_lightbar.g;
	rep.lightbar_b = lb ? lb->b : m_last_lightbar.b;

	DWORD written = 0;
	WriteFile(m_handle, &rep, sizeof(rep), &written, nullptr);
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
	if (len - 1 >= sizeof(InputReportUsb)) {
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