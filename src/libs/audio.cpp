#include "libs/audio.h"

#include "SDL.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/logging/log.h"
#include "common/magicEnum.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "kernel/pthread.h"
#include "kernel/semaphore.h"
#include "libs/audio_internal.h"
#include "libs/errno.h"
#include "libs/libs.h"

#include <optional>
#include <string>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Libs::Audio {

namespace {

constexpr int AUDIO_OUT_PORT_TYPE_MAIN      = 0;
constexpr int AUDIO_OUT_PORT_TYPE_BGM       = 1;
constexpr int AUDIO_OUT_PORT_TYPE_VOICE     = 2;
constexpr int AUDIO_OUT_PORT_TYPE_PERSONAL  = 3;
constexpr int AUDIO_OUT_PORT_TYPE_PADSPK    = 4;
constexpr int AUDIO_OUT_PORT_TYPE_VIBRATION = 10;
constexpr int AUDIO_OUT_PORT_TYPE_AUDIO3D   = 126;
constexpr int AUDIO_OUT_PORT_TYPE_AUX       = 127;

constexpr uint32_t AUDIO_OUT_PARAM_FORMAT_MASK = 0x000000ffu;

static bool audio_out_port_type_is_valid(int type) {
	return (type >= AUDIO_OUT_PORT_TYPE_MAIN && type <= AUDIO_OUT_PORT_TYPE_PADSPK) ||
	       type == AUDIO_OUT_PORT_TYPE_VIBRATION || type == AUDIO_OUT_PORT_TYPE_AUDIO3D ||
	       type == AUDIO_OUT_PORT_TYPE_AUX;
}

// The DualSense exposes its built-in speaker and microphone to the host OS as an ordinary
// USB/Bluetooth audio class device (separate from its HID interface), which is why it shows up
// in the system's normal sound settings as an output+input device named after the controller
// (Windows shows it as "Wireless Controller"; other platforms/OSes commonly report "DualSense").
// SDL2 exposes it exactly like any other audio device once opened by name, so PADSPK (the
// PS5 SDK's dedicated "output to the pad's speaker" port type) and microphone capture can
// simply be pointed at that device by matching against known name substrings, falling back to
// whatever the OS default is when no DualSense audio endpoint is present or connected.
static std::optional<std::string> FindDualSenseAudioDeviceName(bool iscapture) {
	static constexpr const char* kNameNeedles[] = {"dualsense", "wireless controller"};

	const int count = SDL_GetNumAudioDevices(iscapture ? 1 : 0);
	for (int i = 0; i < count; i++) {
		const char* name = SDL_GetAudioDeviceName(i, iscapture ? 1 : 0);
		if (name == nullptr) {
			continue;
		}
		const auto lower = Common::ToLower(name);
		for (const auto* needle: kNameNeedles) {
			if (Common::ContainsStr(lower, needle)) {
				return std::string(name);
			}
		}
	}

	return std::nullopt;
}

} // namespace

class Audio {
public:
	using Format = AudioInternal::Format;

	class Id {
	public:
		explicit Id(int id): m_id(id - 1) {}
		[[nodiscard]] int  ToInt() const { return m_id + 1; }
		[[nodiscard]] bool IsValid() const { return m_id >= 0; }

		friend class Audio;

	private:
		Id() = default;
		static Id Invalid() { return {}; }
		static Id Create(int audio_id) {
			Id r;
			r.m_id = audio_id;
			return r;
		}
		[[nodiscard]] int GetId() const { return m_id; }

		int m_id = -1;
	};

	struct OutputParam {
		Id          handle;
		const void* data = nullptr;
	};

	Audio() = default;
	virtual ~Audio();

	KYTY_CLASS_NO_COPY(Audio);

	Id       AudioOutOpen(int type, uint32_t samples_num, uint32_t freq, Format format);
	bool     AudioOutClose(Id handle);
	bool     AudioOutValid(Id handle);
	bool     AudioOutSetVolume(Id handle, uint32_t bitflag, const int* volume);
	uint32_t AudioOutOutputs(OutputParam* params, uint32_t num, bool blocking = true);
	bool     AudioOutGetStatus(Id handle, int* type, int* channels_num);
	bool     AudioOutGetLastOutputTime(Id handle, uint64_t* output_time);

	Id       AudioInOpen(uint32_t type, uint32_t samples_num, uint32_t freq, Format format);
	bool     AudioInValid(Id handle);
	uint32_t AudioInInput(Id handle, void* dest);

	static constexpr int OUT_PORTS_MAX = 32;
	static constexpr int IN_PORTS_MAX  = 8;

private:
	struct PortOut {
		bool     used             = false;
		int      type             = 0;
		uint32_t samples_num      = 0;
		uint32_t freq             = 0;
		Format   format           = Format::Unknown;
		uint64_t last_output_time = 0;
		int      channels_num     = 0;
		int      volume[8]        = {};

		SDL_AudioDeviceID audio_device = 0;
		SDL_AudioSpec     audio_spec   = {};
	};

	struct PortIn {
		bool     used            = false;
		uint32_t type            = 0;
		uint32_t samples_num     = 0;
		uint32_t freq            = 0;
		Format   format          = Format::Unknown;
		uint64_t last_input_time = 0;

		SDL_AudioDeviceID audio_device = 0;
		SDL_AudioSpec     audio_spec   = {};
	};

	Common::Mutex m_mutex;
	PortOut       m_out_ports[OUT_PORTS_MAX];
	PortIn        m_in_ports[IN_PORTS_MAX];

	static bool            FormatIsFloat(Format format);
	static bool            FormatIsStd(Format format);
	static uint32_t        BytesPerSample(Format format);
	static uint32_t        FrameSize(const PortOut& port);
	static SDL_AudioFormat SdlFormat(Format format);
	static bool            OpenSdlDevice(PortOut* port);
	static void            CloseSdlDevice(PortOut* port);
	static const void*     PrepareOutputBuffer(const PortOut& port, const void* data,
	                                           std::vector<uint8_t>* buffer);
	static bool            QueueSdlAudio(PortOut* port, const void* data, bool blocking);
	static bool            OpenSdlCaptureDevice(PortIn* port);
	static void            CloseSdlCaptureDevice(PortIn* port);
	static uint32_t        DequeueSdlCapture(PortIn* port, void* dest, uint32_t dest_bytes);
};

static Audio* g_audio = nullptr;

namespace AudioInternal {

int AudioOutOpen(int type, uint32_t samples_num, uint32_t freq, Format format) {
	if (g_audio == nullptr) {
		return 0;
	}

	auto id = g_audio->AudioOutOpen(type, samples_num, freq, format);
	return id.IsValid() ? id.ToInt() : 0;
}

void AudioOutClose(int handle) {
	if (g_audio != nullptr && handle > 0) {
		(void)g_audio->AudioOutClose(Audio::Id(handle));
	}
}

uint32_t AudioOutOutputs(const OutputParam* params, uint32_t num, bool blocking) {
	if (g_audio == nullptr || params == nullptr || num == 0) {
		return 0;
	}

	std::vector<Audio::OutputParam> output_params;
	output_params.reserve(num);
	for (uint32_t i = 0; i < num; i++) {
		if (params[i].handle > 0 && params[i].data != nullptr) {
			output_params.push_back(
			    Audio::OutputParam {Audio::Id(params[i].handle), params[i].data});
		}
	}

	if (output_params.empty()) {
		return 0;
	}

	return g_audio->AudioOutOutputs(output_params.data(),
	                                static_cast<uint32_t>(output_params.size()), blocking);
}

} // namespace AudioInternal

KYTY_SUBSYSTEM_INIT(Audio) {
	EXIT_IF(g_audio != nullptr);

	g_audio = new Audio;
}

KYTY_SUBSYSTEM_UNEXPECTED_SHUTDOWN(Audio) {}

KYTY_SUBSYSTEM_DESTROY(Audio) {
	delete g_audio;
	g_audio = nullptr;
}

Audio::~Audio() {
	for (auto& port: m_out_ports) {
		CloseSdlDevice(&port);
	}
	for (auto& port: m_in_ports) {
		CloseSdlCaptureDevice(&port);
	}
}

bool Audio::FormatIsFloat(Format format) {
	return (format == Format::FloatMono || format == Format::FloatStereo ||
	        format == Format::Float8Ch || format == Format::Float8ChStd);
}

bool Audio::FormatIsStd(Format format) {
	return (format == Format::Signed16bit8ChStd || format == Format::Float8ChStd);
}

uint32_t Audio::BytesPerSample(Format format) {
	return FormatIsFloat(format) ? sizeof(float) : sizeof(int16_t);
}

uint32_t Audio::FrameSize(const PortOut& port) {
	return BytesPerSample(port.format) * port.channels_num;
}

SDL_AudioFormat Audio::SdlFormat(Format format) {
	return FormatIsFloat(format) ? AUDIO_F32SYS : AUDIO_S16SYS;
}

bool Audio::OpenSdlDevice(PortOut* port) {
	EXIT_IF(port == nullptr);

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		LOGF("AudioOut: SDL audio init failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_AudioSpec desired {};
	desired.freq     = static_cast<int>(port->freq);
	desired.format   = SdlFormat(port->format);
	desired.channels = static_cast<Uint8>(port->channels_num);
	desired.samples  = static_cast<Uint16>(port->samples_num);
	desired.callback = nullptr;

	SDL_AudioSpec obtained {};

	// ScePad's dedicated PADSPK port type means "play through the controller's own speaker" --
	// route it there specifically instead of the system default output when a DualSense audio
	// device is present, matching what the real console does.
	const char* device_name = nullptr;
	std::string dualsense_name;
	if (port->type == AUDIO_OUT_PORT_TYPE_PADSPK) {
		if (auto found = FindDualSenseAudioDeviceName(/* iscapture */ false)) {
			dualsense_name = *found;
			device_name    = dualsense_name.c_str();
		}
	}

	port->audio_device =
	    SDL_OpenAudioDevice(device_name, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
	if (port->audio_device == 0 && device_name != nullptr) {
		// The matched device may have been unplugged/reconnected between enumeration and open,
		// or may simply refuse this format; fall back to the default output rather than
		// failing the whole port.
		LOGF("AudioOut: failed to open DualSense speaker device '%s' (%s), falling back to "
		     "default\n",
		     device_name, SDL_GetError());
		port->audio_device =
		    SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
	}
	if (port->audio_device == 0) {
		LOGF("AudioOut: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
		return false;
	}

	port->audio_spec = obtained;
	SDL_PauseAudioDevice(port->audio_device, 0);

	LOGF("AudioOut: opened SDL device%s%s (%d Hz, %u ch, format 0x%04x)\n",
	     device_name != nullptr ? " " : "", device_name != nullptr ? device_name : "",
	     obtained.freq, obtained.channels, obtained.format);
	return true;
}

void Audio::CloseSdlDevice(PortOut* port) {
	EXIT_IF(port == nullptr);

	if (port->audio_device != 0 && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
		SDL_ClearQueuedAudio(port->audio_device);
		SDL_CloseAudioDevice(port->audio_device);
	}

	port->audio_device = 0;
	port->audio_spec   = {};
}

bool Audio::OpenSdlCaptureDevice(PortIn* port) {
	EXIT_IF(port == nullptr);

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		LOGF("AudioIn: SDL audio init failed: %s\n", SDL_GetError());
		return false;
	}

	// ScePad's audio-input port models the DualSense's own microphone specifically (as opposed
	// to some other system mic), so prefer its capture device by name, same as the speaker
	// output side; fall back to the OS default capture device if no DualSense mic is found.
	const char* device_name = nullptr;
	std::string dualsense_name;
	if (auto found = FindDualSenseAudioDeviceName(/* iscapture */ true)) {
		dualsense_name = *found;
		device_name    = dualsense_name.c_str();
	}

	SDL_AudioSpec desired {};
	desired.freq     = static_cast<int>(port->freq);
	desired.format   = AUDIO_S16SYS;
	desired.channels = static_cast<Uint8>(port->format == Format::Signed16bitStereo ? 2 : 1);
	desired.samples  = static_cast<Uint16>(port->samples_num);
	desired.callback = nullptr;

	SDL_AudioSpec obtained {};

	port->audio_device =
	    SDL_OpenAudioDevice(device_name, 1, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
	if (port->audio_device == 0 && device_name != nullptr) {
		LOGF("AudioIn: failed to open DualSense mic device '%s' (%s), falling back to default\n",
		     device_name, SDL_GetError());
		port->audio_device =
		    SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
	}
	if (port->audio_device == 0) {
		// No capture device available at all (e.g. headless CI, no mic hardware/permission).
		// AudioInInput() falls back to silence in this case rather than failing the open call
		// outright, since games generally tolerate a muted mic far better than a failed open.
		LOGF("AudioIn: SDL_OpenAudioDevice (capture) failed: %s\n", SDL_GetError());
		return false;
	}

	port->audio_spec = obtained;
	SDL_PauseAudioDevice(port->audio_device, 0);

	LOGF("AudioIn: opened SDL capture device%s%s (%d Hz, %u ch, format 0x%04x)\n",
	     device_name != nullptr ? " " : "", device_name != nullptr ? device_name : "",
	     obtained.freq, obtained.channels, obtained.format);
	return true;
}

void Audio::CloseSdlCaptureDevice(PortIn* port) {
	EXIT_IF(port == nullptr);

	if (port->audio_device != 0 && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
		SDL_ClearQueuedAudio(port->audio_device);
		SDL_CloseAudioDevice(port->audio_device);
	}

	port->audio_device = 0;
	port->audio_spec   = {};
}

uint32_t Audio::DequeueSdlCapture(PortIn* port, void* dest, uint32_t dest_bytes) {
	EXIT_IF(port == nullptr);
	EXIT_IF(dest == nullptr);

	if (port->audio_device == 0) {
		std::memset(dest, 0, dest_bytes);
		return 0;
	}

	const bool needs_convert = (port->audio_spec.format != AUDIO_S16SYS) ||
	                           (static_cast<int>(port->audio_spec.freq) !=
	                            static_cast<int>(port->freq)) ||
	                           (port->audio_spec.channels !=
	                            (port->format == Format::Signed16bitStereo ? 2 : 1));

	if (!needs_convert) {
		const auto got = SDL_DequeueAudio(port->audio_device, dest, dest_bytes);
		if (got < dest_bytes) {
			// Underrun: pad the remainder with silence rather than leaving stale/uninitialized
			// data, matching how a real quiet microphone would behave.
			std::memset(static_cast<uint8_t*>(dest) + got, 0, dest_bytes - got);
		}
		return dest_bytes;
	}

	// Hardware gave us a different rate/format/channel count than requested (common on capture
	// devices, which often refuse SDL_AUDIO_ALLOW_ANY_CHANGE-driven adjustments to the same
	// degree playback devices do) -- convert from what we actually got into the S16 mono/stereo
	// format the guest asked for. Source-side sizing is approximate (converted size scales
	// roughly with the requested/obtained rate and channel ratio), so this pulls a generously
	// sized chunk of whatever is queued and converts as much of it as is available; any
	// shortfall is padded with silence exactly like the direct (no-conversion) path above.
	SDL_AudioCVT cvt {};
	const int    channels = (port->format == Format::Signed16bitStereo ? 2 : 1);
	const int    cvt_result =
	    SDL_BuildAudioCVT(&cvt, port->audio_spec.format, port->audio_spec.channels,
	                      port->audio_spec.freq, AUDIO_S16SYS, static_cast<Uint8>(channels),
	                      static_cast<int>(port->freq));
	if (cvt_result < 0) {
		LOGF("AudioIn: SDL_BuildAudioCVT failed: %s\n", SDL_GetError());
		std::memset(dest, 0, dest_bytes);
		return dest_bytes;
	}

	if (cvt_result == 0) {
		// Same layout after all, just a stale spec check false-positive; dequeue directly.
		const auto got = SDL_DequeueAudio(port->audio_device, dest, dest_bytes);
		if (got < dest_bytes) {
			std::memset(static_cast<uint8_t*>(dest) + got, 0, dest_bytes - got);
		}
		return dest_bytes;
	}

	const auto            len_mult      = static_cast<uint32_t>(std::max(cvt.len_mult, 1));
	const uint32_t        src_capacity  = dest_bytes * len_mult;
	std::vector<uint8_t>  convert_buffer(src_capacity);
	const auto            got =
	    SDL_DequeueAudio(port->audio_device, convert_buffer.data(), src_capacity / len_mult);
	if (got == 0) {
		std::memset(dest, 0, dest_bytes);
		return dest_bytes;
	}

	cvt.buf = convert_buffer.data();
	cvt.len = static_cast<int>(got);
	if (SDL_ConvertAudio(&cvt) < 0) {
		LOGF("AudioIn: SDL_ConvertAudio failed: %s\n", SDL_GetError());
		std::memset(dest, 0, dest_bytes);
		return dest_bytes;
	}

	const auto copy_size = std::min<uint32_t>(static_cast<uint32_t>(cvt.len_cvt), dest_bytes);
	std::memcpy(dest, cvt.buf, copy_size);
	if (copy_size < dest_bytes) {
		std::memset(static_cast<uint8_t*>(dest) + copy_size, 0, dest_bytes - copy_size);
	}
	return dest_bytes;
}

const void* Audio::PrepareOutputBuffer(const PortOut& port, const void* data,
                                       std::vector<uint8_t>* buffer) {
	EXIT_IF(data == nullptr);
	EXIT_IF(buffer == nullptr);

	const auto frames           = port.samples_num;
	const auto channels         = static_cast<uint32_t>(port.channels_num);
	const auto bytes_per_sample = BytesPerSample(port.format);
	const auto src_size         = frames * channels * bytes_per_sample;

	bool volume_changed = false;
	for (uint32_t ch = 0; ch < channels; ch++) {
		if (port.volume[ch] != 32768) {
			volume_changed = true;
			break;
		}
	}

	if (!volume_changed && !FormatIsStd(port.format)) {
		return data;
	}

	buffer->resize(src_size);

	static constexpr uint32_t STD_8CH_MAP[8] = {0, 1, 2, 3, 6, 7, 4, 5};

	if (FormatIsFloat(port.format)) {
		auto*       dst = reinterpret_cast<float*>(buffer->data());
		const auto* src = static_cast<const float*>(data);

		for (uint32_t frame = 0; frame < frames; frame++) {
			for (uint32_t ch = 0; ch < channels; ch++) {
				const auto src_ch =
				    (FormatIsStd(port.format) && channels == 8 ? STD_8CH_MAP[ch] : ch);
				dst[frame * channels + ch] = src[frame * channels + src_ch] *
				                             (static_cast<float>(port.volume[ch]) / 32768.0f);
			}
		}
	} else {
		auto*       dst = reinterpret_cast<int16_t*>(buffer->data());
		const auto* src = static_cast<const int16_t*>(data);

		for (uint32_t frame = 0; frame < frames; frame++) {
			for (uint32_t ch = 0; ch < channels; ch++) {
				const auto src_ch =
				    (FormatIsStd(port.format) && channels == 8 ? STD_8CH_MAP[ch] : ch);
				int64_t sample =
				    static_cast<int64_t>(src[frame * channels + src_ch]) * port.volume[ch] / 32768;
				if (sample > std::numeric_limits<int16_t>::max()) {
					sample = std::numeric_limits<int16_t>::max();
				} else if (sample < std::numeric_limits<int16_t>::min()) {
					sample = std::numeric_limits<int16_t>::min();
				}
				dst[frame * channels + ch] = static_cast<int16_t>(sample);
			}
		}
	}

	return buffer->data();
}

bool Audio::QueueSdlAudio(PortOut* port, const void* data, bool blocking) {
	EXIT_IF(port == nullptr);

	if (port->audio_device == 0 || data == nullptr) {
		return false;
	}

	std::vector<uint8_t> prepared_buffer;
	const void*          prepared_data = PrepareOutputBuffer(*port, data, &prepared_buffer);
	const auto           prepared_size = FrameSize(*port) * port->samples_num;

	std::vector<uint8_t> convert_buffer;
	const void*          queue_data = prepared_data;
	uint32_t             queue_size = prepared_size;

	SDL_AudioCVT cvt {};
	const int    cvt_result =
	    SDL_BuildAudioCVT(&cvt, SdlFormat(port->format), static_cast<Uint8>(port->channels_num),
	                      static_cast<int>(port->freq), port->audio_spec.format,
	                      port->audio_spec.channels, port->audio_spec.freq);

	if (cvt_result < 0) {
		LOGF("AudioOut: SDL_BuildAudioCVT failed: %s\n", SDL_GetError());
		return false;
	}

	if (cvt_result > 0) {
		convert_buffer.resize(prepared_size * cvt.len_mult);
		std::memcpy(convert_buffer.data(), prepared_data, prepared_size);

		cvt.buf = convert_buffer.data();
		cvt.len = static_cast<int>(prepared_size);

		if (SDL_ConvertAudio(&cvt) < 0) {
			LOGF("AudioOut: SDL_ConvertAudio failed: %s\n", SDL_GetError());
			return false;
		}

		queue_data = cvt.buf;
		queue_size = static_cast<uint32_t>(cvt.len_cvt);
	}

	if (blocking) {
		const auto min_queued_size = queue_size * 2u;
		const auto wait_start      = LibKernel::KernelGetProcessTime();
		while (SDL_GetQueuedAudioSize(port->audio_device) > min_queued_size) {
			// Prefer brief underrun risk over wiping the SDL queue — ClearQueuedAudio
			// under load produced dropouts that games heard as silence.
			if (LibKernel::KernelGetProcessTime() - wait_start > 200000) {
				break;
			}
			Common::Thread::SleepMicro(1000);
		}
	}

	if (SDL_QueueAudio(port->audio_device, queue_data, queue_size) < 0) {
		LOGF("AudioOut: SDL_QueueAudio failed: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

Audio::Id Audio::AudioOutOpen(int type, uint32_t samples_num, uint32_t freq, Format format) {
	Common::LockGuard lock(m_mutex);

	if (samples_num == 0 || freq == 0) {
		LOGF("AudioOut: reject open with samples_num=%u freq=%u\n", samples_num, freq);
		return Id::Invalid();
	}

	for (int id = 0; id < OUT_PORTS_MAX; id++) {
		if (!m_out_ports[id].used) {
			auto& port = m_out_ports[id];

			port.used             = true;
			port.type             = type;
			port.samples_num      = samples_num;
			port.freq             = freq;
			port.format           = format;
			port.last_output_time = 0;

			switch (format) {
				case Format::Signed16bitMono:
				case Format::FloatMono: port.channels_num = 1; break;
				case Format::Signed16bitStereo:
				case Format::FloatStereo: port.channels_num = 2; break;
				case Format::Signed16bit8Ch:
				case Format::Float8Ch:
				case Format::Signed16bit8ChStd:
				case Format::Float8ChStd: port.channels_num = 8; break;
				default: EXIT("unknown format");
			}

			for (int i = 0; i < port.channels_num; i++) {
				port.volume[i] = 32768;
			}

			if (type != AUDIO_OUT_PORT_TYPE_VIBRATION) {
				if (!OpenSdlDevice(&port)) {
					// Returning a "valid" port with no SDL device made every later
					// Output report success while queueing nothing (#105-class silence).
					port = {};
					return Id::Invalid();
				}
			}

			return Id::Create(id);
		}
	}

	return Id::Invalid();
}

bool Audio::AudioOutClose(Id handle) {
	Common::LockGuard lock(m_mutex);

	if (AudioOutValid(handle)) {
		auto& port = m_out_ports[handle.GetId()];

		CloseSdlDevice(&port);
		port = {};

		return true;
	}

	return false;
}

bool Audio::AudioOutValid(Id handle) {
	Common::LockGuard lock(m_mutex);

	return (handle.GetId() >= 0 && handle.GetId() < OUT_PORTS_MAX &&
	        m_out_ports[handle.GetId()].used);
}

bool Audio::AudioOutGetStatus(Id handle, int* type, int* channels_num) {
	Common::LockGuard lock(m_mutex);

	if (AudioOutValid(handle)) {
		auto& port = m_out_ports[handle.GetId()];

		*type         = port.type;
		*channels_num = port.channels_num;

		return true;
	}

	return false;
}

bool Audio::AudioOutGetLastOutputTime(Id handle, uint64_t* output_time) {
	Common::LockGuard lock(m_mutex);

	if (output_time == nullptr || !AudioOutValid(handle)) {
		return false;
	}

	*output_time = m_out_ports[handle.GetId()].last_output_time;
	return true;
}

bool Audio::AudioOutSetVolume(Id handle, uint32_t bitflag, const int* volume) {
	Common::LockGuard lock(m_mutex);

	if (AudioOutValid(handle)) {
		auto& port = m_out_ports[handle.GetId()];

		for (int i = 0; i < port.channels_num; i++, bitflag >>= 1u) {
			auto bit = bitflag & 0x1u;

			if (bit == 1) {
				int src_index = i;
				if (port.format == Format::Float8ChStd ||
				    port.format == Format::Signed16bit8ChStd) {
					switch (i) {
						case 4: src_index = 6; break;
						case 5: src_index = 7; break;
						case 6: src_index = 4; break;
						case 7: src_index = 5; break;
						default:;
					}
				}
				port.volume[i] = volume[src_index];

				LOGF("\t port.volume[%d] = volume[%d] (%d)\n", i, src_index, volume[src_index]);
			}
		}

		return true;
	}

	return false;
}

uint32_t Audio::AudioOutOutputs(OutputParam* params, uint32_t num, bool blocking) {
	EXIT_NOT_IMPLEMENTED(num == 0);
	EXIT_NOT_IMPLEMENTED(!AudioOutValid(params[0].handle));

	const auto& first_port = m_out_ports[params[0].handle.GetId()];
	EXIT_NOT_IMPLEMENTED(first_port.freq == 0);

	// Guest titles pace on wall-clock gaps between Output calls (shadPS4/Kyty guidance).
	// Keep simulated delay; SDL queue depth still back-pressures inside QueueSdlAudio.
	uint64_t block_time   = (1000000 * first_port.samples_num) / first_port.freq;
	uint64_t current_time = LibKernel::KernelGetProcessTime();

	uint64_t max_wait_time = 0;

	for (uint32_t i = 0; i < num; i++) {
		uint64_t next_time = m_out_ports[params[i].handle.GetId()].last_output_time + block_time;
		uint64_t wait_time = (next_time > current_time ? next_time - current_time : 0);
		max_wait_time      = (wait_time > max_wait_time ? wait_time : max_wait_time);
	}

	if (blocking && max_wait_time != 0) {
		Common::Thread::SleepMicro(max_wait_time);
	}

	for (uint32_t i = 0; i < num; i++) {
		auto& port = m_out_ports[params[i].handle.GetId()];

		if (params[i].data == nullptr) {
			continue;
		}
		if (!QueueSdlAudio(&port, params[i].data, blocking)) {
			static std::atomic<uint32_t> queue_fail_logs {0};
			if (queue_fail_logs.fetch_add(1, std::memory_order_relaxed) < 16) {
				LOGF("AudioOut: QueueSdlAudio failed handle=%d device=%u\n",
				     params[i].handle.ToInt(), port.audio_device);
			}
		}
	}

	for (uint32_t i = 0; i < num; i++) {
		m_out_ports[params[i].handle.GetId()].last_output_time = LibKernel::KernelGetProcessTime();
	}

	return first_port.samples_num;
}

Audio::Id Audio::AudioInOpen(uint32_t type, uint32_t samples_num, uint32_t freq, Format format) {
	Common::LockGuard lock(m_mutex);

	for (int id = 0; id < IN_PORTS_MAX; id++) {
		if (!m_in_ports[id].used) {
			auto& port = m_in_ports[id];

			port.used        = true;
			port.type        = type;
			port.samples_num = samples_num;
			port.freq        = freq;
			port.format      = format;

			switch (format) {
				case Format::Signed16bitMono:
				case Format::Signed16bitStereo: break;
				default: EXIT("unknown format");
			}

			// Failing to open a capture device (no mic present/permitted, headless
			// environment, etc.) is not fatal to opening the port -- AudioInInput() falls back
			// to reporting silence, mirroring how a real DualSense with, say, a disabled
			// microphone would still let the port open and just yield no signal.
			OpenSdlCaptureDevice(&port);

			return Id::Create(id);
		}
	}

	return Id::Invalid();
}

bool Audio::AudioInValid(Id handle) {
	Common::LockGuard lock(m_mutex);

	return (handle.GetId() >= 0 && handle.GetId() < IN_PORTS_MAX &&
	        m_in_ports[handle.GetId()].used);
}

uint32_t Audio::AudioInInput(Id handle, void* dest) {
	EXIT_NOT_IMPLEMENTED(!AudioInValid(handle));
	EXIT_NOT_IMPLEMENTED(dest == nullptr);

	Common::LockGuard lock(m_mutex);

	auto& port = m_in_ports[handle.GetId()];

	uint64_t block_time   = (1000000 * port.samples_num) / port.freq;
	uint64_t current_time = LibKernel::KernelGetProcessTime();

	uint64_t next_time = port.last_input_time + block_time;
	uint64_t wait_time = (next_time > current_time ? next_time - current_time : 0);

	// Pace to the same wall-clock cadence the guest expects a real mic to deliver samples at,
	// then hand back whatever's actually been captured since (or silence, if no capture device
	// is available -- see AudioInOpen).
	Common::Thread::SleepMicro(wait_time);

	const auto channels    = (port.format == Format::Signed16bitStereo ? 2u : 1u);
	const auto dest_bytes  = port.samples_num * channels * sizeof(int16_t);
	DequeueSdlCapture(&port, dest, dest_bytes);

	port.last_input_time = LibKernel::KernelGetProcessTime();

	return port.samples_num;
}

namespace AudioOut {

LIB_NAME("AudioOut", "AudioOut");

struct AudioOutOutputParam {
	int         handle;
	const void* ptr;
};

struct AudioOutPortState {
	uint16_t output;
	uint8_t  channel;
	uint8_t  reserved1[1];
	int16_t  volume;
	uint16_t reroute_counter;
	uint64_t flag;
	uint64_t reserved2[2];
};

int KYTY_SYSV_ABI AudioOutInit() {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI AudioOutOpen(int user_id, int type, int index, uint32_t len, uint32_t freq,
                               uint32_t param) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n"
	     "\t type    = %d\n"
	     "\t index   = %d\n"
	     "\t len     = %u\n"
	     "\t freq    = %u\n",
	     user_id, type, index, len, freq);

	if (!audio_out_port_type_is_valid(type)) {
		return AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
	}
	EXIT_NOT_IMPLEMENTED(index != 0);

	Audio::Format format       = Audio::Format::Unknown;
	const auto    format_param = param & AUDIO_OUT_PARAM_FORMAT_MASK;

	switch (format_param) {
		case 0: format = Audio::Format::Signed16bitMono; break;
		case 1: format = Audio::Format::Signed16bitStereo; break;
		case 2: format = Audio::Format::Signed16bit8Ch; break;
		case 3: format = Audio::Format::FloatMono; break;
		case 4: format = Audio::Format::FloatStereo; break;
		case 5: format = Audio::Format::Float8Ch; break;
		case 6: format = Audio::Format::Signed16bit8ChStd; break;
		case 7: format = Audio::Format::Float8ChStd; break;
		default:;
	}

	LOGF("\t param   = %u (format=%u, %s)\n", param, format_param,
	     Common::EnumName(format).c_str());

	EXIT_NOT_IMPLEMENTED(format == Audio::Format::Unknown);

	if (freq == 0 || len == 0) {
		return AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
	}

	EXIT_IF(g_audio == nullptr);

	auto id = g_audio->AudioOutOpen(type, len, freq, format);

	if (!id.IsValid()) {
		return AUDIO_OUT_ERROR_PORT_FULL;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI AudioOutClose(int handle) {
	PRINT_NAME();

	if (!g_audio->AudioOutClose(Audio::Id(handle))) {
		return AUDIO_OUT_ERROR_INVALID_PORT;
	}

	return OK;
}

int KYTY_SYSV_ABI AudioOutGetPortState(int handle, AudioOutPortState* state) {
	PRINT_NAME();

	int type         = 0;
	int channels_num = 0;

	if (!g_audio->AudioOutGetStatus(Audio::Id(handle), &type, &channels_num)) {
		return AUDIO_OUT_ERROR_INVALID_PORT;
	}

	EXIT_NOT_IMPLEMENTED(state == nullptr);

	state->reroute_counter = 0;
	state->volume          = 127;

	switch (type) {
		case AUDIO_OUT_PORT_TYPE_MAIN:
		case AUDIO_OUT_PORT_TYPE_BGM:
		case AUDIO_OUT_PORT_TYPE_AUDIO3D:
			state->output  = 1;
			state->channel = (channels_num > 2 ? 2 : channels_num);
			break;
		case AUDIO_OUT_PORT_TYPE_VOICE:
		case AUDIO_OUT_PORT_TYPE_PERSONAL:
			state->output  = 0x40;
			state->channel = 1;
			break;
		case AUDIO_OUT_PORT_TYPE_PADSPK:
		case AUDIO_OUT_PORT_TYPE_VIBRATION:
			state->output  = 4;
			state->channel = 1;
			break;
		case AUDIO_OUT_PORT_TYPE_AUX:
			state->output  = 0x80;
			state->channel = (channels_num > 2 ? 2 : channels_num);
			break;
		default: EXIT("unknown port type: %d\n", type);
	}

	LOGF("\t output  = %" PRIu16 "\n"
	     "\t channel = %" PRIu8 "\n",
	     state->output, state->channel);

	return OK;
}

int KYTY_SYSV_ABI AudioOutGetLastOutputTime(int handle, uint64_t* output_time) {
	PRINT_NAME();

	if (output_time == nullptr) {
		return AUDIO_OUT_ERROR_INVALID_POINTER;
	}
	EXIT_IF(g_audio == nullptr);

	if (!g_audio->AudioOutGetLastOutputTime(Audio::Id(handle), output_time)) {
		return AUDIO_OUT_ERROR_INVALID_PORT;
	}

	return OK;
}

int KYTY_SYSV_ABI AudioOutSetVolume(int handle, uint32_t flag, int* vol) {
	PRINT_NAME();

	LOGF("\t handle = %d\n"
	     "\t flag   = %u\n",
	     handle, flag);

	EXIT_IF(g_audio == nullptr);
	EXIT_NOT_IMPLEMENTED(vol == nullptr);

	if (!g_audio->AudioOutSetVolume(Audio::Id(handle), flag, vol)) {
		return AUDIO_OUT_ERROR_INVALID_PORT;
	}

	return OK;
}

int KYTY_SYSV_ABI AudioOutOutputs(AudioOutOutputParam* param, uint32_t num) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(param == nullptr);

	Audio::OutputParam params[Audio::OUT_PORTS_MAX];

	EXIT_IF(g_audio == nullptr);

	for (uint32_t i = 0; i < num; i++) {
		params[i].handle = Audio::Id(param[i].handle);
		params[i].data   = param[i].ptr;

		if (!g_audio->AudioOutValid(params[i].handle)) {
			return AUDIO_OUT_ERROR_INVALID_PORT;
		}
	}

	return static_cast<int>(g_audio->AudioOutOutputs(params, num));
}

int KYTY_SYSV_ABI AudioOutOutput(int handle, const void* ptr) {
	// EXIT_NOT_IMPLEMENTED(ptr == nullptr);

	Audio::OutputParam params[1];

	EXIT_IF(g_audio == nullptr);

	params[0].handle = Audio::Id(handle);
	params[0].data   = ptr;

	if (!g_audio->AudioOutValid(params[0].handle)) {
		return AUDIO_OUT_ERROR_INVALID_PORT;
	}

	return static_cast<int>(g_audio->AudioOutOutputs(params, 1));
}

} // namespace AudioOut

namespace AudioIn {

LIB_NAME("AudioIn", "AudioIn");

int KYTY_SYSV_ABI AudioInOpen(int user_id, uint32_t type, uint32_t index, uint32_t len,
                              uint32_t freq, uint32_t param) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n"
	     "\t type    = %u\n"
	     "\t index   = %d\n"
	     "\t len     = %u\n"
	     "\t freq    = %u\n",
	     user_id, type, index, len, freq);

	if (user_id != 255 && user_id != 1) {
		LOGF("\t temporary: accepting unsupported audio input user_id %d\n", user_id);
	}
	EXIT_NOT_IMPLEMENTED(type != 1);
	EXIT_NOT_IMPLEMENTED(index != 0);

	Audio::Format format = Audio::Format::Unknown;

	switch (param) {
		case 0: format = Audio::Format::Signed16bitMono; break;
		case 2: format = Audio::Format::Signed16bitStereo; break;
		default:
			LOGF("\t temporary: using signed 16-bit stereo for unsupported audio input param %u\n",
			     param);
			format = Audio::Format::Signed16bitStereo;
			break;
	}

	LOGF("\t param   = %u (%s)\n", param, Common::EnumName(format).c_str());

	EXIT_IF(g_audio == nullptr);

	auto id = g_audio->AudioInOpen(type, len, freq, format);

	if (!id.IsValid()) {
		return AUDIO_IN_ERROR_PORT_FULL;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI AudioInInput(int handle, void* dest) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(dest == nullptr);

	EXIT_IF(g_audio == nullptr);

	if (!g_audio->AudioInValid(Audio::Id(handle))) {
		return AUDIO_IN_ERROR_INVALID_HANDLE;
	}

	return static_cast<int>(g_audio->AudioInInput(Audio::Id(handle), dest));
}

} // namespace AudioIn

namespace VoiceQoS {

LIB_NAME("VoiceQoS", "VoiceQoS");

int KYTY_SYSV_ABI VoiceQoSInit(void* mem_block, uint32_t mem_size, int32_t app_type) {
	PRINT_NAME();

	LOGF("\t mem_block = %016" PRIx64 "\n"
	     "\t mem_size = %" PRIu32 "\n"
	     "\t app_type = %" PRId32 "\n",
	     reinterpret_cast<uint64_t>(mem_block), mem_size, app_type);

	return OK;
}

} // namespace VoiceQoS

namespace Acm {

LIB_NAME("Acm", "Acm");

struct AcmBatchInfo {
	void*  buffer;
	size_t offset;
	size_t buffer_size;
};

struct AcmBatchError {
	uint32_t reserved[8];
};

static std::atomic_uint32_t g_acm_next_context {1};
static std::atomic_uint32_t g_acm_next_batch {1};

static void acm_advance_batch(AcmBatchInfo* info, size_t bytes) {
	if (info == nullptr || info->buffer == nullptr || info->buffer_size == 0) {
		return;
	}

	info->offset = std::min(info->buffer_size, info->offset + bytes);
}

int KYTY_SYSV_ABI AcmContextCreate(AcmContextId* context) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(context == nullptr);

	*context = g_acm_next_context.fetch_add(1, std::memory_order_relaxed);

	LOGF("\t context = %" PRIu32 "\n", *context);

	return OK;
}

int KYTY_SYSV_ABI AcmContextDestroy(AcmContextId context) {
	PRINT_NAME();
	LOGF("\t context = %" PRIu32 "\n", context);
	return OK;
}

int KYTY_SYSV_ABI AcmBatchStartBuffer(AcmContextId context, const void* batch_commands,
                                      size_t batch_size, AcmBatchError* batch_error,
                                      AcmBatchId* batch) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(batch == nullptr);

	if (batch_error != nullptr) {
		std::memset(batch_error, 0, sizeof(AcmBatchError));
	}

	*batch = g_acm_next_batch.fetch_add(1, std::memory_order_relaxed);

	return OK;
}

int KYTY_SYSV_ABI AcmBatchStartBuffers(AcmContextId context, uint32_t batch_info_count,
                                       const AcmBatchInfo* const batch_info[],
                                       AcmBatchError* batch_error, AcmBatchId* batch) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(batch_info_count != 0 && batch_info == nullptr);
	EXIT_NOT_IMPLEMENTED(batch == nullptr);

	if (batch_error != nullptr) {
		std::memset(batch_error, 0, sizeof(AcmBatchError));
	}

	*batch = g_acm_next_batch.fetch_add(1, std::memory_order_relaxed);

	return OK;
}

int KYTY_SYSV_ABI AcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout) {
	return OK;
}

int KYTY_SYSV_ABI AcmBatchJobNotification(AcmBatchInfo* batch_info) {
	PRINT_NAME();
	acm_advance_batch(batch_info, 2 * 16);
	return OK;
}

int KYTY_SYSV_ABI AcmConvReverbSharedInput(AcmBatchInfo* batch_info, uint32_t block_count, void* in,
                                           uint32_t count, const void* const ir[],
                                           const float* gain, void* const out[]) {
	PRINT_NAME();
	(void)block_count;
	(void)in;
	(void)count;
	(void)ir;
	(void)gain;
	(void)out;
	acm_advance_batch(batch_info, 1024);
	return OK;
}

int KYTY_SYSV_ABI AcmConvReverbSharedIr(AcmBatchInfo* batch_info, uint32_t block_count,
                                        const void* ir, uint32_t count, void* const in[],
                                        const float* gain, void* const out[]) {
	PRINT_NAME();
	(void)block_count;
	(void)ir;
	(void)count;
	(void)in;
	(void)gain;
	(void)out;
	acm_advance_batch(batch_info, 1024);
	return OK;
}

int KYTY_SYSV_ABI AcmFft(AcmBatchInfo* batch_info, int size, int count, int input_format,
                         const void* const input[], int output_format, void* const output[],
                         uint32_t flags) {
	PRINT_NAME();
	(void)size;
	(void)count;
	(void)input_format;
	(void)input;
	(void)output_format;
	(void)output;
	(void)flags;
	acm_advance_batch(batch_info, 256);
	return OK;
}

int KYTY_SYSV_ABI AcmIfft(AcmBatchInfo* batch_info, int size, int count, int input_format,
                          const void* const input[], int output_format, void* const output[],
                          uint32_t flags) {
	PRINT_NAME();
	(void)size;
	(void)count;
	(void)input_format;
	(void)input;
	(void)output_format;
	(void)output;
	(void)flags;
	acm_advance_batch(batch_info, 256);
	return OK;
}

int KYTY_SYSV_ABI AcmPanner(AcmBatchInfo* batch_info, uint32_t in_count, const float* const in[],
                            uint32_t biquad_count, uint32_t biquad_update_count, uint32_t out_count,
                            const void* const parameter[], void* const state[],
                            const float* const out_init[], float* const out[]) {
	PRINT_NAME();
	(void)in_count;
	(void)in;
	(void)biquad_count;
	(void)biquad_update_count;
	(void)out_count;
	(void)parameter;
	(void)state;
	(void)out_init;
	(void)out;
	acm_advance_batch(batch_info, 512);
	return OK;
}

} // namespace Acm

namespace Audio3d {

LIB_NAME("Audio3d", "Audio3d");

#include "libs/audio3d_impl.inc"

} // namespace Audio3d

namespace Ngs2 {

LIB_NAME("Ngs2", "Ngs2");

// PS-ADPCM ("VAGp") decode path (#69): included into namespace Libs::Audio::Ngs2
// so the sampler mixer can detect VAG-wrapped sampler voices and decode them
// to PCM16 on demand without touching the existing PCM plumbing.
#include "libs/ngs2_vag_decoder.inc"

struct Ngs2SystemOption {
	size_t    size                     = 0;
	char      name[64]                 = {};
	uintptr_t job_scheduler_options[4] = {};
	uint32_t  flags                    = 0;
	uint32_t  max_grain_samples        = 0;
	uint32_t  num_grain_samples        = 0;
	uint32_t  sample_rate              = 0;
	uint32_t  max_voice_channels       = 0;
	uint32_t  reserved[5]              = {};
};

struct Ngs2RackOption {
	size_t   size                   = 0;
	char     name[64]               = {};
	uint32_t flags                  = 0;
	uint32_t max_grain_samples      = 0;
	uint32_t max_voices             = 0;
	uint32_t max_input_delay_blocks = 0;
	uint32_t max_matrices           = 0;
	uint32_t max_ports              = 0;
	uint32_t max_voice_channels     = 0;
	uint32_t max_output_channels    = 0;
	uint32_t reserved[18]           = {};
};

struct Ngs2MasteringRackOption {
	Ngs2RackOption rack_option;
	uint32_t       max_channels          = 0;
	uint32_t       num_peak_meter_blocks = 0;
};

struct Ngs2SubmixerRackOption {
	Ngs2RackOption rack_option;
	uint32_t       max_channels          = 0;
	uint32_t       max_envelope_points   = 0;
	uint32_t       max_filters           = 0;
	uint32_t       max_inputs            = 0;
	uint32_t       num_peak_meter_blocks = 0;
};

struct Ngs2SamplerRackOption {
	Ngs2RackOption rack_option;
	uint32_t       max_channel_works        = 0;
	uint32_t       max_codec_caches         = 0;
	uint32_t       max_waveform_blocks      = 0;
	uint32_t       max_envelope_points      = 0;
	uint32_t       max_filters              = 0;
	uint32_t       max_atrac9_decoders      = 0;
	uint32_t       max_atrac9_channel_works = 0;
	uint32_t       max_ajm_atrac9_decoders  = 0;
	uint32_t       num_peak_meter_blocks    = 0;
};

struct Ngs2ReverbRackOption {
	Ngs2RackOption rack_option;
	uint32_t       max_channels = 0;
	uint32_t       reverb_size  = 0;
};

struct Ngs2CustomModuleOption {
	uint32_t size = 0;
};

struct Ngs2CustomRackModuleInfo {
	const Ngs2CustomModuleOption* option           = nullptr;
	uint32_t                      module_id        = 0;
	uint32_t                      source_buffer_id = 0;
	uint32_t                      extra_buffer_id  = 0;
	uint32_t                      dest_buffer_id   = 0;
	uint32_t                      state_offset     = 0;
	uint32_t                      state_size       = 0;
	uint32_t                      reserved         = 0;
	uint32_t                      reserved2        = 0;
};

struct Ngs2CustomRackPortInfo {
	uint32_t source_buffer_id = 0;
	uint32_t reserved         = 0;
};

struct Ngs2CustomRackOption {
	Ngs2RackOption           rack_option;
	uint32_t                 state_size  = 0;
	uint32_t                 num_buffers = 0;
	uint32_t                 num_modules = 0;
	uint32_t                 reserved    = 0;
	Ngs2CustomRackModuleInfo module[24];
	Ngs2CustomRackPortInfo   port[16];
};

struct Ngs2CustomSubmixerRackOption {
	Ngs2CustomRackOption custom_rack_option;
	uint32_t             max_channels = 0;
	uint32_t             max_inputs   = 0;
};

struct Ngs2CustomSamplerRackOption {
	Ngs2CustomRackOption custom_rack_option;
	uint32_t             max_channel_works        = 0;
	uint32_t             max_waveform_blocks      = 0;
	uint32_t             max_atrac9_decoders      = 0;
	uint32_t             max_atrac9_channel_works = 0;
	uint32_t             max_ajm_atrac9_decoders  = 0;
	uint32_t             max_codec_caches         = 0;
};

struct Ngs2CustomMasteringRackOption {
	Ngs2CustomRackOption custom_rack_option;
	uint32_t             max_channels          = 0;
	uint32_t             num_peak_meter_blocks = 0;
};

union Ngs2RackOptionUnion {
	Ngs2RackOption                common;
	Ngs2SamplerRackOption         sampler;
	Ngs2MasteringRackOption       mastering;
	Ngs2SubmixerRackOption        submixer;
	Ngs2ReverbRackOption          reverb;
	Ngs2CustomSubmixerRackOption  custom_submixer;
	Ngs2CustomSamplerRackOption   custom_sampler;
	Ngs2CustomMasteringRackOption custom_mastering;
};

struct Ngs2ContextBufferInfo {
	void*     host_buffer      = nullptr;
	size_t    host_buffer_size = 0;
	uintptr_t reserved[5]      = {};
	uintptr_t user_data        = 0;
};

struct Ngs2RenderBufferInfo {
	void*    buffer        = nullptr;
	size_t   buffer_size   = 0;
	uint32_t waveform_type = 0;
	uint32_t num_channels  = 0;
};

struct Ngs2WaveformFormat {
	uint32_t waveform_type = 0;
	uint32_t num_channels  = 0;
	uint32_t sample_rate   = 0;
	uint32_t config_data   = 0;
	uint32_t frame_offset  = 0;
	uint32_t frame_margin  = 0;
};

struct Ngs2WaveformBlock {
	uintptr_t data_offset      = 0;
	size_t    data_size        = 0;
	uint32_t  num_repeats      = 0;
	uint32_t  num_skip_samples = 0;
	uint32_t  num_samples      = 0;
	uint32_t  reserved         = 0;
	uintptr_t user_data        = 0;
};

struct Ngs2WaveformInfo {
	Ngs2WaveformFormat format;
	uint32_t           data_offset              = 0;
	uint32_t           data_size                = 0;
	uint32_t           loop_begin_position      = 0;
	uint32_t           loop_end_position        = 0;
	uint32_t           num_samples              = 0;
	uint32_t           audio_unit_size          = 0;
	uint32_t           num_audio_unit_samples   = 0;
	uint32_t           num_audio_unit_per_frame = 0;
	uint32_t           audio_frame_size         = 0;
	uint32_t           num_audio_frame_samples  = 0;
	uint32_t           num_delay_samples        = 0;
	uint32_t           num_blocks               = 0;
	Ngs2WaveformBlock  blocks[4];
};

struct Ngs2PanParam {
	float angle     = 0.0f;
	float distance  = 0.0f;
	float fbw_level = 0.0f;
	float lfe_level = 0.0f;
};

struct Ngs2PanWork {
	float    speaker_angles[8] = {};
	float    unit_angle        = 0.0f;
	uint32_t num_speakers      = 0;
};

struct Ngs2GeomVector {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct Ngs2GeomCone {
	float inner_level = 0.0f;
	float inner_angle = 0.0f;
	float outer_level = 0.0f;
	float outer_angle = 0.0f;
};

struct Ngs2GeomRolloff {
	uint32_t model              = 0;
	float    max_distance       = 0.0f;
	float    rolloff_factor     = 0.0f;
	float    reference_distance = 0.0f;
};

struct Ngs2GeomListenerParam {
	Ngs2GeomVector position;
	Ngs2GeomVector orient_front;
	Ngs2GeomVector orient_up;
	Ngs2GeomVector velocity;
	float          sound_speed = 0.0f;
	uint32_t       reserved[2] = {};
};

struct Ngs2GeomListenerWork {
	float          matrix[4][4] = {};
	Ngs2GeomVector velocity;
	float          sound_speed = 0.0f;
	uint32_t       coordinate  = 0;
	uint32_t       reserved[3] = {};
};

struct Ngs2GeomSourceParam {
	Ngs2GeomVector  position;
	Ngs2GeomVector  velocity;
	Ngs2GeomVector  direction;
	Ngs2GeomCone    cone;
	Ngs2GeomRolloff rolloff;
	float           doppler_factor = 0.0f;
	float           fbw_level      = 0.0f;
	float           lfe_level      = 0.0f;
	float           max_level      = 0.0f;
	float           min_level      = 0.0f;
	float           radius         = 0.0f;
	uint32_t        num_speakers   = 0;
	uint32_t        matrix_format  = 0;
	uint32_t        reserved[2]    = {};
};

struct Ngs2GeomA3dAttribute {
	Ngs2GeomVector position;
	float          volume      = 0.0f;
	uint32_t       reserved[4] = {};
};

struct Ngs2GeomAttribute {
	float                pitch_ratio = 0.0f;
	float                level[64]   = {};
	Ngs2GeomA3dAttribute a3d_attrib;
	uint32_t             reserved[4] = {};
};

using Ngs2BufferAllocHandler = int32_t KYTY_SYSV_ABI (*)(Ngs2ContextBufferInfo*);
using Ngs2BufferFreeHandler  = int32_t  KYTY_SYSV_ABI (*)(Ngs2ContextBufferInfo*);

struct Ngs2BufferAllocator {
	Ngs2BufferAllocHandler alloc_handler = nullptr;
	Ngs2BufferFreeHandler  free_handler  = nullptr;
	uintptr_t              user_data     = 0;
};

struct Ngs2Internal {
	Ngs2SystemOption    option;
	Ngs2BufferAllocator allocator;
	Ngs2Internal*       next = nullptr;
	Common::Mutex       mutex;
};

enum class Ngs2RackType {
	Sampler,
	Submixer,
	Mastering,
	Reverb,
	CustomSubmixer,
	CustomSampler,
	CustomMastering,
};

struct Ngs2RackInternal {
	Ngs2Internal*       ngs  = nullptr;
	Ngs2RackInternal*   next = nullptr;
	Ngs2RackType        type = Ngs2RackType::Sampler;
	Ngs2RackOptionUnion option;
	Ngs2BufferAllocator allocator;
};

enum class Ngs2VoicePlayState { Empty, Playing, Paused, Stopped };

enum class Ngs2VoicePlayEvent { None, Play, Pause, Resume, Stop, StopImm, Kill };

struct Ngs2VoiceInternal {
	Ngs2VoicePlayEvent event          = Ngs2VoicePlayEvent::None;
	Ngs2VoicePlayState state          = Ngs2VoicePlayState::Empty;
	Ngs2RackInternal*  rack           = nullptr;
	uintptr_t          callback       = 0;
	uintptr_t          callback_data  = 0;
	uint32_t           callback_flags = 0;

	// Sampler PCM playback (SETUP + waveform blocks/address).
	Ngs2WaveformFormat format {};
	const uint8_t*     waveform_data       = nullptr;
	uint32_t           waveform_data_size  = 0;
	uint32_t           block_num_samples   = 0;
	uint32_t           block_num_repeats   = 0;
	uint32_t           sample_pos          = 0;
	uint64_t           num_decoded_samples = 0;
	uint64_t           decoded_data_size   = 0;
	uintptr_t          waveform_user_data  = 0;
	// Lazily-decoded VAG container (#69): when waveform_data points at a "VAGp"
	// blob, the first mix pass decodes it into pcm16_cache, then redirects
	// waveform_data/size at the cache and rewrites format.waveform_type to the
	// little-endian PCM16 the existing mixer already understands.
	bool                vag_decoded         = false;
	std::vector<int16_t> vag_pcm16_cache;
	float              port_volume         = 1.0f;
	float              pitch_ratio         = 1.0f;
	float              matrix_levels[8]    = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	uint32_t           num_matrix_levels   = 1;
};

struct Ngs2VoiceParamHeader {
	uint16_t size;
	int16_t  next;
	uint32_t id;
};

// Guest layouts for sampler VoiceControl (match Orbis/shadPS4/psOff).
struct Ngs2SamplerVoiceSetupParam {
	Ngs2VoiceParamHeader header;
	Ngs2WaveformFormat   format;
	uint32_t             flags;
	uint32_t             reserved;
};

struct Ngs2SamplerWaveformBlockGuest {
	uint32_t  data_offset      = 0;
	uint32_t  data_size        = 0;
	uint32_t  num_repeats      = 0;
	uint32_t  num_skip_samples = 0;
	uint32_t  num_samples      = 0;
	uint32_t  reserved         = 0;
	uintptr_t user_data        = 0;
};

struct Ngs2SamplerVoiceWaveformBlocksParam {
	Ngs2VoiceParamHeader                 header;
	const void*                          data       = nullptr;
	uint32_t                             flags      = 0;
	uint32_t                             num_blocks = 0;
	const Ngs2SamplerWaveformBlockGuest* a_block    = nullptr;
};

struct Ngs2SamplerVoiceWaveformAddressParam {
	Ngs2VoiceParamHeader header;
	const void*          from = nullptr;
	const void*          to   = nullptr;
};

struct Ngs2SamplerVoicePitchParam {
	Ngs2VoiceParamHeader header;
	float                ratio    = 1.0f;
	uint32_t             reserved = 0;
};

struct Ngs2VoiceEventParam {
	Ngs2VoiceParamHeader header;
	uint32_t             event_id;
};

struct Ngs2VoicePatchParam {
	Ngs2VoiceParamHeader header;
	uint32_t             port;
	uint32_t             dest_input_id;
	uintptr_t            dest_handle;
};

struct Ngs2VoiceMatrixLevelsParam {
	Ngs2VoiceParamHeader header;
	uint32_t             matrix_id;
	uint32_t             num_levels;
	const float*         levels;
};

struct Ngs2VoicePortMatrixParam {
	Ngs2VoiceParamHeader header;
	uint32_t             port;
	int32_t              matrix_id;
};

struct Ngs2VoicePortVolumeParam {
	Ngs2VoiceParamHeader header;
	uint32_t             port;
	float                level;
};

struct Ngs2VoicePortDelayParam {
	Ngs2VoiceParamHeader header;
	uint32_t             port;
	uint32_t             num_samples;
};

struct Ngs2VoiceCallbackParam {
	Ngs2VoiceParamHeader header;
	uintptr_t            callback;
	uintptr_t            callback_data;
	uint32_t             flags;
	uint32_t             reserved;
};

struct Ngs2VoiceState {
	uint32_t state_flags;
};

struct Ngs2SamplerVoiceState {
	Ngs2VoiceState voice_state;
	float          envelope_height;
	float          peak_height;
	uint32_t       reserved;
	uint64_t       num_decoded_samples;
	uint64_t       decoded_data_size;
	uint64_t       user_data;
	const void*    waveform_data;
};

static Ngs2Internal*     g_ngs_list   = nullptr;
static Ngs2RackInternal* g_racks_list = nullptr;
// Guards mutation/traversal of the g_ngs_list/g_racks_list head pointers and next-links
// themselves. Per-system ngs->mutex only protects a given system's own state (voices, render),
// not the global intrusive lists that Destroy/Render/Create walk across all systems.
static Common::Mutex g_ngs_lists_mutex;

static_assert(sizeof(Ngs2SystemOption) == 144);
static_assert(sizeof(Ngs2RackOption) == 176);

static uint32_t Ngs2GetStateFlags(const Ngs2VoiceInternal* voice) {
	switch (voice->state) {
		case Ngs2VoicePlayState::Empty: return 0;
		case Ngs2VoicePlayState::Playing: return 0x3;
		case Ngs2VoicePlayState::Paused: return 0x5;
		case Ngs2VoicePlayState::Stopped: return 0xb;
	}

	return 0;
}

// Orbis waveformType PCM values (psOff / SDK dumps).
constexpr uint32_t kNgs2WavePcmI8         = 0x10;
constexpr uint32_t kNgs2WavePcmU8         = 0x11;
constexpr uint32_t kNgs2WavePcmI16Little  = 0x12;
constexpr uint32_t kNgs2WavePcmI16Big     = 0x13;
constexpr uint32_t kNgs2WavePcmI32Little  = 0x16;
constexpr uint32_t kNgs2WavePcmI32Big     = 0x17;
constexpr uint32_t kNgs2WavePcmF32Little  = 0x18;
constexpr uint32_t kNgs2WavePcmF32Big     = 0x19;

static uint32_t Ngs2PcmBytesPerSample(uint32_t waveform_type) {
	switch (waveform_type) {
		case kNgs2WavePcmI8:
		case kNgs2WavePcmU8: return 1;
		case kNgs2WavePcmI16Little:
		case kNgs2WavePcmI16Big: return 2;
		case kNgs2WavePcmI32Little:
		case kNgs2WavePcmI32Big:
		case kNgs2WavePcmF32Little:
		case kNgs2WavePcmF32Big: return 4;
		default: return 0;
	}
}

static bool Ngs2IsSupportedPcm(uint32_t waveform_type) {
	return Ngs2PcmBytesPerSample(waveform_type) != 0;
}

static float Ngs2ReadPcmSample(const uint8_t* data, uint32_t waveform_type) {
	switch (waveform_type) {
		case kNgs2WavePcmI8: return static_cast<float>(static_cast<int8_t>(data[0])) / 128.0f;
		case kNgs2WavePcmU8: return (static_cast<float>(data[0]) - 128.0f) / 128.0f;
		case kNgs2WavePcmI16Little: {
			const int16_t v = static_cast<int16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
			return static_cast<float>(v) / 32768.0f;
		}
		case kNgs2WavePcmI16Big: {
			const int16_t v = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
			return static_cast<float>(v) / 32768.0f;
		}
		case kNgs2WavePcmI32Little: {
			const int32_t v = static_cast<int32_t>(data[0] | (static_cast<uint32_t>(data[1]) << 8) |
			                                       (static_cast<uint32_t>(data[2]) << 16) |
			                                       (static_cast<uint32_t>(data[3]) << 24));
			return static_cast<float>(v) / 2147483648.0f;
		}
		case kNgs2WavePcmI32Big: {
			const int32_t v = static_cast<int32_t>((static_cast<uint32_t>(data[0]) << 24) |
			                                       (static_cast<uint32_t>(data[1]) << 16) |
			                                       (static_cast<uint32_t>(data[2]) << 8) | data[3]);
			return static_cast<float>(v) / 2147483648.0f;
		}
		case kNgs2WavePcmF32Little: {
			float v = 0.0f;
			std::memcpy(&v, data, sizeof(float));
			return v;
		}
		case kNgs2WavePcmF32Big: {
			uint8_t swapped[4] = {data[3], data[2], data[1], data[0]};
			float   v         = 0.0f;
			std::memcpy(&v, swapped, sizeof(float));
			return v;
		}
		default: return 0.0f;
	}
}

static void Ngs2WritePcmSample(uint8_t* data, uint32_t waveform_type, float sample) {
	sample = std::clamp(sample, -1.0f, 1.0f);
	switch (waveform_type) {
		case kNgs2WavePcmI8:
			data[0] = static_cast<uint8_t>(static_cast<int8_t>(sample * 127.0f));
			break;
		case kNgs2WavePcmU8:
			data[0] = static_cast<uint8_t>(std::clamp(sample * 128.0f + 128.0f, 0.0f, 255.0f));
			break;
		case kNgs2WavePcmI16Little: {
			const auto v = static_cast<int16_t>(sample * 32767.0f);
			data[0]      = static_cast<uint8_t>(v & 0xff);
			data[1]      = static_cast<uint8_t>((v >> 8) & 0xff);
			break;
		}
		case kNgs2WavePcmI16Big: {
			const auto v = static_cast<int16_t>(sample * 32767.0f);
			data[0]      = static_cast<uint8_t>((v >> 8) & 0xff);
			data[1]      = static_cast<uint8_t>(v & 0xff);
			break;
		}
		case kNgs2WavePcmI32Little: {
			const auto v = static_cast<int32_t>(sample * 2147483647.0f);
			data[0]      = static_cast<uint8_t>(v & 0xff);
			data[1]      = static_cast<uint8_t>((v >> 8) & 0xff);
			data[2]      = static_cast<uint8_t>((v >> 16) & 0xff);
			data[3]      = static_cast<uint8_t>((v >> 24) & 0xff);
			break;
		}
		case kNgs2WavePcmI32Big: {
			const auto v = static_cast<int32_t>(sample * 2147483647.0f);
			data[0]      = static_cast<uint8_t>((v >> 24) & 0xff);
			data[1]      = static_cast<uint8_t>((v >> 16) & 0xff);
			data[2]      = static_cast<uint8_t>((v >> 8) & 0xff);
			data[3]      = static_cast<uint8_t>(v & 0xff);
			break;
		}
		case kNgs2WavePcmF32Little: std::memcpy(data, &sample, sizeof(float)); break;
		case kNgs2WavePcmF32Big: {
			uint8_t raw[4];
			std::memcpy(raw, &sample, sizeof(float));
			data[0] = raw[3];
			data[1] = raw[2];
			data[2] = raw[1];
			data[3] = raw[0];
			break;
		}
		default: break;
	}
}

static uint32_t Ngs2VoiceTotalSamples(const Ngs2VoiceInternal* voice) {
	if (voice->block_num_samples != 0) {
		return voice->block_num_samples;
	}
	const uint32_t bps = Ngs2PcmBytesPerSample(voice->format.waveform_type);
	const uint32_t ch  = voice->format.num_channels;
	if (bps == 0 || ch == 0) {
		return 0;
	}
	return voice->waveform_data_size / (bps * ch);
}

static void Ngs2MixVoiceIntoBuffer(Ngs2VoiceInternal* voice, const Ngs2RenderBufferInfo* out,
                                   uint32_t num_out_samples, uint32_t out_sample_rate) {
	if (voice->state != Ngs2VoicePlayState::Playing || voice->waveform_data == nullptr) {
		return;
	}

	// #69: lazy-decode VAG ("VAGp") sampler payloads to mono PCM16 on first mix.
	// After this substitution the voice presents as a regular PCM I16 LE
	// waveform and the rest of the existing PCM mixer handles it natively.
	if (!voice->vag_decoded && VagIsVagContainer(voice->waveform_data, voice->waveform_data_size)) {
		Ngs2VagWaveform decoded;
		if (VagTryDecodeContainer(voice->waveform_data, voice->waveform_data_size, &decoded)) {
			voice->vag_pcm16_cache   = std::move(decoded.samples);
			voice->format.waveform_type = kNgs2WavePcmI16Little;
			voice->format.sample_rate   = decoded.sample_rate > 0 ? decoded.sample_rate : 48000;
			voice->format.num_channels  = 1;
			voice->waveform_data      = reinterpret_cast<const uint8_t*>(voice->vag_pcm16_cache.data());
			voice->waveform_data_size = static_cast<uint32_t>(voice->vag_pcm16_cache.size() * sizeof(int16_t));
			voice->block_num_samples  = 0; // recompute via Ngs2VoiceTotalSamples
			static std::atomic<uint32_t> vag_logs {0};
			if (vag_logs.fetch_add(1, std::memory_order_relaxed) < 16) {
				LOGF_COLOR(Log::Color::Yellow,
				           "Ngs2: decoded VAG sampler voice pcm16=%-6u rate=%u loops=[%d,%d]\n",
				           static_cast<uint32_t>(voice->vag_pcm16_cache.size()),
				           voice->format.sample_rate, decoded.loop_start, decoded.loop_end);
			}
		}
		voice->vag_decoded = true;
	}

	if (!Ngs2IsSupportedPcm(voice->format.waveform_type) || !Ngs2IsSupportedPcm(out->waveform_type)) {
		static std::atomic_uint32_t soft_logs {0};
		if (soft_logs.fetch_add(1, std::memory_order_relaxed) < 16) {
			LOGF_COLOR(Log::Color::Yellow,
			           "Ngs2: soft-skip mix unsupported waveform_type in=0x%" PRIx32
			           " out=0x%" PRIx32 "\n",
			           voice->format.waveform_type, out->waveform_type);
		}
		return;
	}
	if (out->buffer == nullptr || out->num_channels == 0 || num_out_samples == 0) {
		return;
	}

	const uint32_t in_bps  = Ngs2PcmBytesPerSample(voice->format.waveform_type);
	const uint32_t out_bps = Ngs2PcmBytesPerSample(out->waveform_type);
	const uint32_t in_ch   = voice->format.num_channels;
	const uint32_t out_ch  = out->num_channels;
	const uint32_t in_rate =
	    voice->format.sample_rate != 0 ? voice->format.sample_rate : out_sample_rate;
	const float pitch = (voice->pitch_ratio > 0.0f) ? voice->pitch_ratio : 1.0f;
	const float step =
	    (static_cast<float>(in_rate) / static_cast<float>(out_sample_rate != 0 ? out_sample_rate : 1)) *
	    pitch;

	uint32_t total = Ngs2VoiceTotalSamples(voice);
	if (total == 0) {
		return;
	}

	const size_t out_frame_bytes = static_cast<size_t>(out_bps) * out_ch;
	const size_t max_frames_by_size =
	    out->buffer_size / (out_frame_bytes != 0 ? out_frame_bytes : 1);
	const uint32_t frames =
	    static_cast<uint32_t>(std::min<size_t>(num_out_samples, max_frames_by_size));

	auto* out_bytes = static_cast<uint8_t*>(out->buffer);
	float pos       = static_cast<float>(voice->sample_pos);

	for (uint32_t f = 0; f < frames; f++) {
		while (static_cast<uint32_t>(pos) >= total) {
			if (voice->block_num_repeats > 0) {
				voice->block_num_repeats--;
				pos = 0.0f;
				voice->sample_pos = 0;
			} else {
				voice->state = Ngs2VoicePlayState::Empty;
				voice->sample_pos = total;
				return;
			}
		}

		const uint32_t src_frame = static_cast<uint32_t>(pos);
		const uint8_t* src =
		    voice->waveform_data + static_cast<size_t>(src_frame) * in_bps * in_ch;

		for (uint32_t oc = 0; oc < out_ch; oc++) {
			const uint32_t ic = (in_ch == 1) ? 0 : (oc < in_ch ? oc : (in_ch - 1));
			float          level = voice->port_volume;
			if (voice->num_matrix_levels > 0) {
				const uint32_t mi =
				    (oc < voice->num_matrix_levels) ? oc : (voice->num_matrix_levels - 1);
				level *= voice->matrix_levels[mi];
			}
			const float s =
			    Ngs2ReadPcmSample(src + static_cast<size_t>(ic) * in_bps, voice->format.waveform_type) *
			    level;
			uint8_t* dst = out_bytes + static_cast<size_t>(f) * out_frame_bytes +
			               static_cast<size_t>(oc) * out_bps;
			const float mixed = Ngs2ReadPcmSample(dst, out->waveform_type) + s;
			Ngs2WritePcmSample(dst, out->waveform_type, mixed);
		}

		pos += step;
		voice->num_decoded_samples++;
		voice->decoded_data_size += in_bps * in_ch;
	}

	voice->sample_pos = static_cast<uint32_t>(pos);
}

static void Ngs2SoftIgnoreSamplerCtl(uint32_t cid, uint16_t size) {
	static std::atomic_uint32_t soft_logs {0};
	if (soft_logs.fetch_add(1, std::memory_order_relaxed) < 32) {
		LOGF_COLOR(Log::Color::Yellow,
		           "Ngs2: soft-ignore sampler ctl cid=0x%" PRIx32 " size=%" PRIu16 "\n", cid, size);
	}
}

static Ngs2SystemOption Ngs2DefaultSystemOption() {
	Ngs2SystemOption option {};
	option.size              = sizeof(Ngs2SystemOption);
	option.max_grain_samples = 512;
	option.num_grain_samples = 256;
	option.sample_rate       = 48000;
	return option;
}

int KYTY_SYSV_ABI Ngs2SystemResetOption(Ngs2SystemOption* option) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(option == nullptr);

	*option = Ngs2DefaultSystemOption();
	return OK;
}

static Ngs2Internal* Ngs2CreateSystemInternal(const Ngs2SystemOption* option, void* host_buffer) {
	auto* ngs = new (host_buffer) Ngs2Internal;

	ngs->option = *option;

	Common::LockGuard lock(g_ngs_lists_mutex);
	ngs->next  = g_ngs_list;
	g_ngs_list = ngs;

	return ngs;
}

// Unlinks and returns whether `ngs` was found in the global system list. Must be called
// before the backing host_buffer for `ngs` is freed/reused, otherwise Ngs2SystemRender's
// rack traversal (which filters on rack->ngs == ngs) and any lingering rack still pointing
// at this system would read freed memory.
static bool Ngs2UnlinkSystem(Ngs2Internal* ngs) {
	Common::LockGuard lock(g_ngs_lists_mutex);
	Ngs2Internal**    cur = &g_ngs_list;
	while (*cur != nullptr) {
		if (*cur == ngs) {
			*cur = ngs->next;
			return true;
		}
		cur = &(*cur)->next;
	}
	return false;
}

// Unlinks `rack` from the global rack list and returns whether it was found. Racks are never
// pointed to after this returns, so it's safe for the caller to treat the backing buffer as
// free once this returns.
static bool Ngs2UnlinkRack(Ngs2RackInternal* rack) {
	Common::LockGuard   lock(g_ngs_lists_mutex);
	Ngs2RackInternal** cur = &g_racks_list;
	while (*cur != nullptr) {
		if (*cur == rack) {
			*cur = rack->next;
			return true;
		}
		cur = &(*cur)->next;
	}
	return false;
}

static bool Ngs2RackIsCustom(Ngs2RackType type) {
	switch (type) {
		case Ngs2RackType::CustomSubmixer:
		case Ngs2RackType::CustomSampler:
		case Ngs2RackType::CustomMastering: return true;
		default: return false;
	}
}

int KYTY_SYSV_ABI Ngs2SystemQueryBufferSize(const Ngs2SystemOption* option,
                                            Ngs2ContextBufferInfo*  buffer_info) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(buffer_info == nullptr);

	auto default_option = Ngs2DefaultSystemOption();
	if (option == nullptr) {
		option = &default_option;
		LOGF("\t option            = nullptr, using reset defaults\n");
	}

	EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2SystemOption));

	std::memset(buffer_info, 0, sizeof(Ngs2ContextBufferInfo));
	buffer_info->host_buffer_size = sizeof(Ngs2Internal);

	return OK;
}

int KYTY_SYSV_ABI Ngs2SystemCreate(const Ngs2SystemOption*      option,
                                   const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(buffer_info == nullptr);
	EXIT_NOT_IMPLEMENTED(handle == nullptr);
	EXIT_NOT_IMPLEMENTED(buffer_info->host_buffer == nullptr);
	EXIT_NOT_IMPLEMENTED(buffer_info->host_buffer_size < sizeof(Ngs2Internal));

	auto default_option = Ngs2DefaultSystemOption();
	if (option == nullptr) {
		option = &default_option;
		LOGF("\t option            = nullptr, using reset defaults\n");
	}

	EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2SystemOption));

	auto* ngs = Ngs2CreateSystemInternal(option, buffer_info->host_buffer);

	*handle = reinterpret_cast<uintptr_t>(ngs);

	return OK;
}

static void Ngs2FillDefaultRackOption(uint32_t rack_id, Ngs2RackOptionUnion* option) {
	EXIT_NOT_IMPLEMENTED(option == nullptr);

	*option = {};

	switch (rack_id) {
		case 0x1000:
			option->sampler.rack_option.size                   = sizeof(Ngs2SamplerRackOption);
			option->sampler.rack_option.max_grain_samples      = 512;
			option->sampler.rack_option.max_voices             = 256;
			option->sampler.rack_option.max_input_delay_blocks = 0;
			option->sampler.rack_option.max_matrices           = 1;
			option->sampler.rack_option.max_ports              = 8;
			option->sampler.max_channel_works                  = 256;
			option->sampler.max_codec_caches                   = 32;
			option->sampler.max_waveform_blocks                = 4;
			option->sampler.max_envelope_points                = 4;
			option->sampler.max_filters                        = 8;
			option->sampler.max_atrac9_decoders                = 256;
			option->sampler.max_atrac9_channel_works           = 256;
			option->sampler.max_ajm_atrac9_decoders            = 0;
			option->sampler.num_peak_meter_blocks              = 8;
			break;
		case 0x2000:
			option->submixer.rack_option.size                   = sizeof(Ngs2SubmixerRackOption);
			option->submixer.rack_option.max_grain_samples      = 512;
			option->submixer.rack_option.max_voices             = 1;
			option->submixer.rack_option.max_input_delay_blocks = 1;
			option->submixer.rack_option.max_matrices           = 1;
			option->submixer.rack_option.max_ports              = 8;
			option->submixer.max_channels                       = 8;
			option->submixer.max_envelope_points                = 4;
			option->submixer.max_filters                        = 8;
			option->submixer.max_inputs                         = 1;
			option->submixer.num_peak_meter_blocks              = 8;
			break;
		case 0x2001:
			option->reverb.rack_option.size                   = sizeof(Ngs2ReverbRackOption);
			option->reverb.rack_option.max_grain_samples      = 512;
			option->reverb.rack_option.max_voices             = 1;
			option->reverb.rack_option.max_input_delay_blocks = 1;
			option->reverb.rack_option.max_matrices           = 1;
			option->reverb.rack_option.max_ports              = 8;
			option->reverb.max_channels                       = 8;
			option->reverb.reverb_size                        = 1;
			break;
		case 0x3000:
			option->mastering.rack_option.size                   = sizeof(Ngs2MasteringRackOption);
			option->mastering.rack_option.max_grain_samples      = 512;
			option->mastering.rack_option.max_voices             = 1;
			option->mastering.rack_option.max_input_delay_blocks = 1;
			option->mastering.rack_option.max_matrices           = 0;
			option->mastering.rack_option.max_ports              = 0;
			option->mastering.max_channels                       = 8;
			option->mastering.num_peak_meter_blocks              = 8;
			break;
		case 0x4002:
			// FIXME: Temporary PS5 progress fallback. This mirrors Prospero reset helper's
			// common custom-submixer defaults, but the custom module internals are still stubbed.
			option->custom_submixer.custom_rack_option.rack_option.size =
			    sizeof(Ngs2CustomSubmixerRackOption);
			option->custom_submixer.custom_rack_option.rack_option.max_grain_samples      = 512;
			option->custom_submixer.custom_rack_option.rack_option.max_voices             = 1;
			option->custom_submixer.custom_rack_option.rack_option.max_input_delay_blocks = 1;
			option->custom_submixer.custom_rack_option.rack_option.max_matrices           = 1;
			option->custom_submixer.custom_rack_option.rack_option.max_ports              = 8;
			option->custom_submixer.custom_rack_option.num_buffers                        = 1;
			option->custom_submixer.max_channels                                          = 8;
			option->custom_submixer.max_inputs                                            = 1;
			break;
		case 0x4001:
			option->custom_sampler.custom_rack_option.rack_option.size =
			    sizeof(Ngs2CustomSamplerRackOption);
			option->custom_sampler.custom_rack_option.rack_option.max_grain_samples      = 512;
			option->custom_sampler.custom_rack_option.rack_option.max_voices             = 256;
			option->custom_sampler.custom_rack_option.rack_option.max_input_delay_blocks = 0;
			option->custom_sampler.custom_rack_option.rack_option.max_matrices           = 1;
			option->custom_sampler.custom_rack_option.rack_option.max_ports              = 8;
			option->custom_sampler.custom_rack_option.num_buffers                        = 1;
			option->custom_sampler.max_channel_works                                     = 256;
			option->custom_sampler.max_waveform_blocks                                   = 4;
			option->custom_sampler.max_atrac9_decoders                                   = 256;
			option->custom_sampler.max_atrac9_channel_works                              = 256;
			option->custom_sampler.max_ajm_atrac9_decoders                               = 0;
			option->custom_sampler.max_codec_caches                                      = 32;
			break;
		case 0x4003:
			// Mirrors the plain mastering defaults; custom module internals are still stubbed,
			// same as the other custom rack types above.
			option->custom_mastering.custom_rack_option.rack_option.size =
			    sizeof(Ngs2CustomMasteringRackOption);
			option->custom_mastering.custom_rack_option.rack_option.max_grain_samples      = 512;
			option->custom_mastering.custom_rack_option.rack_option.max_voices             = 1;
			option->custom_mastering.custom_rack_option.rack_option.max_input_delay_blocks = 1;
			option->custom_mastering.custom_rack_option.rack_option.max_matrices           = 0;
			option->custom_mastering.custom_rack_option.rack_option.max_ports              = 0;
			option->custom_mastering.custom_rack_option.num_buffers                        = 1;
			option->custom_mastering.max_channels                                          = 8;
			option->custom_mastering.num_peak_meter_blocks                                 = 8;
			break;
		default:
			EXIT("Ngs2: unsupported rack_id 0x%" PRIx32 "\n", rack_id);
	}
}

int KYTY_SYSV_ABI Ngs2RackQueryBufferSize(uint32_t rack_id, const Ngs2RackOption* option,
                                          Ngs2ContextBufferInfo* buffer_info) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(buffer_info == nullptr);

	Ngs2RackOptionUnion default_option {};
	if (option == nullptr) {
		Ngs2FillDefaultRackOption(rack_id, &default_option);
		option = &default_option.common;
		LOGF("\t option     = nullptr, using reset defaults for rack_id 0x%" PRIx32 "\n", rack_id);
	}

	LOGF("\t rack_id    = 0x%" PRIx32 "\n"
	     "\t max_voices = %u\n",
	     rack_id, option->max_voices);

	buffer_info->host_buffer_size =
	    sizeof(Ngs2RackInternal) + sizeof(Ngs2VoiceInternal) * option->max_voices;

	return OK;
}

int KYTY_SYSV_ABI Ngs2SystemCreateWithAllocator(const Ngs2SystemOption*    option,
                                                const Ngs2BufferAllocator* allocator,
                                                uintptr_t*                 handle) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(allocator == nullptr);
	EXIT_NOT_IMPLEMENTED(handle == nullptr);
	EXIT_NOT_IMPLEMENTED(allocator->alloc_handler == nullptr);
	EXIT_NOT_IMPLEMENTED(allocator->free_handler == nullptr);

	auto default_option = Ngs2DefaultSystemOption();
	if (option == nullptr) {
		option = &default_option;
		LOGF("\t option            = nullptr, using reset defaults\n");
	}

	EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2SystemOption));

	LOGF("\t name              = %.64s\n"
	     "\t flags             = %u\n"
	     "\t max_grain_samples = %u\n"
	     "\t num_grain_samples = %u\n"
	     "\t sample_rate       = %u\n"
	     "\t max_voice_channels = %u\n"
	     "\t alloc_handler     = 0x%016" PRIx64 "\n"
	     "\t free_handler      = 0x%016" PRIx64 "\n"
	     "\t user_data         = 0x%016" PRIx64 "\n",
	     option->name, option->flags, option->max_grain_samples, option->num_grain_samples,
	     option->sample_rate, option->max_voice_channels,
	     reinterpret_cast<uint64_t>(allocator->alloc_handler),
	     reinterpret_cast<uint64_t>(allocator->free_handler),
	     reinterpret_cast<uint64_t>(allocator->user_data));

	Ngs2ContextBufferInfo buf {};
	buf.host_buffer      = nullptr;
	buf.host_buffer_size = sizeof(Ngs2Internal);
	buf.user_data        = allocator->user_data;

	int result = allocator->alloc_handler(&buf);

	EXIT_NOT_IMPLEMENTED(result != OK);
	EXIT_NOT_IMPLEMENTED(buf.host_buffer == nullptr);

	auto* ngs      = Ngs2CreateSystemInternal(option, buf.host_buffer);
	ngs->allocator = *allocator;

	*handle = reinterpret_cast<uintptr_t>(ngs);

	return OK;
}

int KYTY_SYSV_ABI Ngs2SystemSetGrainSamples(uintptr_t system_handle, uint32_t num_samples) {
	PRINT_NAME();
	LOGF("\t system_handle = 0x%016" PRIx64 "\n"
	     "\t num_samples   = %u\n",
	     static_cast<uint64_t>(system_handle), num_samples);

	EXIT_NOT_IMPLEMENTED(system_handle == 0);

	auto* ngs                     = reinterpret_cast<Ngs2Internal*>(system_handle);
	ngs->option.num_grain_samples = num_samples;

	return OK;
}

int KYTY_SYSV_ABI Ngs2SystemDestroy(uintptr_t system_handle, Ngs2ContextBufferInfo* buffer_info) {
	PRINT_NAME();
	LOGF("\t system_handle = 0x%016" PRIx64 "\n", static_cast<uint64_t>(system_handle));

	if (system_handle != 0) {
		auto* ngs = reinterpret_cast<Ngs2Internal*>(system_handle);
		// Any rack still pointing at this system is now dangling once the guest frees/reuses
		// this system's host_buffer; racks must be destroyed before their owning system, but
		// defensively drop them from the global list too so a stray Ngs2SystemRender walk
		// never dereferences a rack whose ngs backpointer is about to go stale.
		{
			Common::LockGuard lock(g_ngs_lists_mutex);
			Ngs2RackInternal** cur = &g_racks_list;
			while (*cur != nullptr) {
				if ((*cur)->ngs == ngs) {
					*cur = (*cur)->next;
				} else {
					cur = &(*cur)->next;
				}
			}
		}
		Ngs2UnlinkSystem(ngs);
		ngs->~Ngs2Internal();
	}

	if (buffer_info != nullptr) {
		std::memset(buffer_info, 0, sizeof(Ngs2ContextBufferInfo));
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2RackCreate(uintptr_t system_handle, uint32_t rack_id,
                                 const Ngs2RackOption*        option,
                                 const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(buffer_info == nullptr);
	EXIT_NOT_IMPLEMENTED(handle == nullptr);
	EXIT_NOT_IMPLEMENTED(buffer_info->host_buffer == nullptr);
	EXIT_NOT_IMPLEMENTED(buffer_info->host_buffer_size == 0);
	EXIT_NOT_IMPLEMENTED(system_handle == 0);

	Ngs2RackOptionUnion default_option {};
	if (option == nullptr) {
		Ngs2FillDefaultRackOption(rack_id, &default_option);
		option = &default_option.common;
		LOGF("\t option                 = nullptr, using reset defaults for rack_id 0x%" PRIx32
		     "\n",
		     rack_id);
	}

	EXIT_NOT_IMPLEMENTED(option->size < sizeof(Ngs2RackOption));

	LOGF("\t rack_id                = 0x%" PRIx32 "\n"
	     "\t name                   = %.64s\n"
	     "\t flags                  = %u\n"
	     "\t max_grain_samples      = %u\n"
	     "\t max_voices             = %u\n"
	     "\t max_input_delay_blocks = %u\n"
	     "\t max_matrices           = %u\n"
	     "\t max_ports              = %u\n"
	     "\t max_voice_channels     = %u\n"
	     "\t max_output_channels    = %u\n"
	     "\t host_buffer            = 0x%016" PRIx64 "\n"
	     "\t host_buffer_size      = 0x%016" PRIx64 "\n",
	     rack_id, option->name, option->flags, option->max_grain_samples, option->max_voices,
	     option->max_input_delay_blocks, option->max_matrices, option->max_ports,
	     option->max_voice_channels, option->max_output_channels,
	     reinterpret_cast<uint64_t>(buffer_info->host_buffer),
	     reinterpret_cast<uint64_t>(buffer_info->host_buffer_size));

	auto* ngs    = reinterpret_cast<Ngs2Internal*>(system_handle);
	auto* rack   = static_cast<Ngs2RackInternal*>(buffer_info->host_buffer);
	auto* voices = reinterpret_cast<Ngs2VoiceInternal*>(rack + 1);

	Common::LockGuard lock(ngs->mutex);

	switch (rack_id) {
		case 0x1000:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2SamplerRackOption));
			rack->option.sampler = *reinterpret_cast<const Ngs2SamplerRackOption*>(option);
			rack->type           = Ngs2RackType::Sampler;
			break;
		case 0x2000:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2SubmixerRackOption));
			rack->option.submixer = *reinterpret_cast<const Ngs2SubmixerRackOption*>(option);
			rack->type            = Ngs2RackType::Submixer;
			break;
		case 0x2001:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2ReverbRackOption));
			rack->option.reverb = *reinterpret_cast<const Ngs2ReverbRackOption*>(option);
			rack->type          = Ngs2RackType::Reverb;
			break;
		case 0x3000:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2MasteringRackOption));
			rack->option.mastering = *reinterpret_cast<const Ngs2MasteringRackOption*>(option);
			rack->type             = Ngs2RackType::Mastering;
			break;
		case 0x4002:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2CustomSubmixerRackOption));
			rack->option.custom_submixer =
			    *reinterpret_cast<const Ngs2CustomSubmixerRackOption*>(option);
			rack->type = Ngs2RackType::CustomSubmixer;
			break;
		case 0x4001:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2CustomSamplerRackOption));
			rack->option.custom_sampler =
			    *reinterpret_cast<const Ngs2CustomSamplerRackOption*>(option);
			rack->type = Ngs2RackType::CustomSampler;
			break;
		case 0x4003:
			EXIT_NOT_IMPLEMENTED(option->size != sizeof(Ngs2CustomMasteringRackOption));
			rack->option.custom_mastering =
			    *reinterpret_cast<const Ngs2CustomMasteringRackOption*>(option);
			rack->type = Ngs2RackType::CustomMastering;
			break;
		default: EXIT("Ngs2: unsupported rack_id 0x%" PRIx32 "\n", rack_id);
	}

	LOGF("\t type                   = %s\n", Common::EnumName(rack->type).c_str());

	rack->allocator = Ngs2BufferAllocator();
	rack->ngs       = ngs;

	{
		Common::LockGuard lists_lock(g_ngs_lists_mutex);
		rack->next   = g_racks_list;
		g_racks_list = rack;
	}

	for (uint32_t i = 0; i < option->max_voices; i++) {
		voices[i].rack  = rack;
		voices[i].event = Ngs2VoicePlayEvent::None;
		voices[i].state = Ngs2VoicePlayState::Empty;
	}

	*handle = reinterpret_cast<uintptr_t>(rack);

	return OK;
}

int KYTY_SYSV_ABI Ngs2RackCreateWithAllocator(uintptr_t system_handle, uint32_t rack_id,
                                              const Ngs2RackOption*      option,
                                              const Ngs2BufferAllocator* allocator,
                                              uintptr_t*                 handle) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(allocator == nullptr);
	EXIT_NOT_IMPLEMENTED(handle == nullptr);
	EXIT_NOT_IMPLEMENTED(allocator->alloc_handler == nullptr);
	EXIT_NOT_IMPLEMENTED(allocator->free_handler == nullptr);
	EXIT_NOT_IMPLEMENTED(system_handle == 0);

	Ngs2RackOptionUnion default_option {};
	if (option == nullptr) {
		Ngs2FillDefaultRackOption(rack_id, &default_option);
		option = &default_option.common;
		LOGF("\t option                 = nullptr, using reset defaults for rack_id 0x%" PRIx32
		     "\n",
		     rack_id);
	}

	EXIT_NOT_IMPLEMENTED(option->size < sizeof(Ngs2RackOption));

	LOGF("\t rack_id                = 0x%" PRIx32 "\n"
	     "\t name                   = %.64s\n"
	     "\t flags                  = %u\n"
	     "\t max_grain_samples      = %u\n"
	     "\t max_voices             = %u\n"
	     "\t max_input_delay_blocks = %u\n"
	     "\t max_matrices           = %u\n"
	     "\t max_ports              = %u\n"
	     "\t max_voice_channels     = %u\n"
	     "\t max_output_channels    = %u\n"
	     "\t alloc_handler          = 0x%016" PRIx64 "\n"
	     "\t free_handler           = 0x%016" PRIx64 "\n"
	     "\t user_data              = 0x%016" PRIx64 "\n",
	     rack_id, option->name, option->flags, option->max_grain_samples, option->max_voices,
	     option->max_input_delay_blocks, option->max_matrices, option->max_ports,
	     option->max_voice_channels, option->max_output_channels,
	     reinterpret_cast<uint64_t>(allocator->alloc_handler),
	     reinterpret_cast<uint64_t>(allocator->free_handler),
	     reinterpret_cast<uint64_t>(allocator->user_data));

	Ngs2ContextBufferInfo buf {};
	buf.host_buffer      = nullptr;
	buf.host_buffer_size = 0;
	buf.user_data        = allocator->user_data;

	Ngs2RackQueryBufferSize(rack_id, option, &buf);

	EXIT_NOT_IMPLEMENTED(buf.host_buffer_size == 0);

	int result = allocator->alloc_handler(&buf);

	EXIT_NOT_IMPLEMENTED(result != OK);
	EXIT_NOT_IMPLEMENTED(buf.host_buffer == nullptr);

	result = Ngs2RackCreate(system_handle, rack_id, option, &buf, handle);

	if (result == OK) {
		auto* rack      = static_cast<Ngs2RackInternal*>(buf.host_buffer);
		rack->allocator = *allocator;
	}

	return result;
}

int KYTY_SYSV_ABI Ngs2RackDestroy(uintptr_t rack_handle, Ngs2ContextBufferInfo* buffer_info) {
	PRINT_NAME();
	LOGF("\t rack_handle = 0x%016" PRIx64 "\n", static_cast<uint64_t>(rack_handle));

	if (rack_handle != 0) {
		auto* rack = reinterpret_cast<Ngs2RackInternal*>(rack_handle);
		// Must unlink before the guest is allowed to free/reuse this rack's host_buffer,
		// otherwise Ngs2SystemRender's `for (rack = g_racks_list; ...)` walk (which runs
		// under a different system's lock, or concurrently on another thread) dereferences
		// freed memory through the stale rack->next chain.
		Ngs2UnlinkRack(rack);
	}

	if (buffer_info != nullptr) {
		std::memset(buffer_info, 0, sizeof(Ngs2ContextBufferInfo));
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2RackLock(uintptr_t rack_handle) {
	PRINT_NAME();
	LOGF("\t rack_handle = 0x%016" PRIx64 "\n", static_cast<uint64_t>(rack_handle));

	EXIT_NOT_IMPLEMENTED(rack_handle == 0);

	auto* rack = reinterpret_cast<Ngs2RackInternal*>(rack_handle);

	EXIT_NOT_IMPLEMENTED(rack->ngs == nullptr);

	rack->ngs->mutex.Lock();

	return OK;
}

int KYTY_SYSV_ABI Ngs2RackUnlock(uintptr_t rack_handle) {
	PRINT_NAME();
	LOGF("\t rack_handle = 0x%016" PRIx64 "\n", static_cast<uint64_t>(rack_handle));

	EXIT_NOT_IMPLEMENTED(rack_handle == 0);

	auto* rack = reinterpret_cast<Ngs2RackInternal*>(rack_handle);

	EXIT_NOT_IMPLEMENTED(rack->ngs == nullptr);

	rack->ngs->mutex.Unlock();

	return OK;
}

int KYTY_SYSV_ABI Ngs2SystemRender(uintptr_t system_handle, const Ngs2RenderBufferInfo* buffer_info,
                                   uint32_t num_buffer_info) {
	static std::atomic_uint32_t render_log_count = 0;
	const auto log_index     = render_log_count.fetch_add(1, std::memory_order_relaxed);
	const bool log_this_call = (log_index < 16 || (log_index % 600) == 0);

	if (log_this_call) {
		PRINT_NAME();
		LOGF("\t call_count      = %" PRIu32 "\n", log_index + 1);
	}

	EXIT_NOT_IMPLEMENTED(buffer_info == nullptr);
	EXIT_NOT_IMPLEMENTED(system_handle == 0);
	EXIT_NOT_IMPLEMENTED(num_buffer_info == 0);

	auto* ngs = reinterpret_cast<Ngs2Internal*>(system_handle);

	Common::LockGuard lock(ngs->mutex);

	for (uint32_t i = 0; i < num_buffer_info; i++) {
		if (buffer_info[i].buffer != nullptr && buffer_info[i].buffer_size != 0) {
			std::memset(buffer_info[i].buffer, 0, buffer_info[i].buffer_size);
		}
	}

	const uint32_t grain_samples =
	    ngs->option.num_grain_samples != 0 ? ngs->option.num_grain_samples : 256;
	const uint32_t out_rate = ngs->option.sample_rate != 0 ? ngs->option.sample_rate : 48000;

	Common::LockGuard lists_lock(g_ngs_lists_mutex);
	for (auto* rack = g_racks_list; rack != nullptr; rack = rack->next) {
		if (rack->ngs != ngs) {
			continue;
		}
		auto* voices = reinterpret_cast<Ngs2VoiceInternal*>(rack + 1);

		for (uint32_t i = 0; i < rack->option.common.max_voices; i++) {
			auto& voice = voices[i];
			switch (voice.event) {
				case Ngs2VoicePlayEvent::None:
					// Stopped is a one-shot status; Playing/Paused persist until an event.
					if (voice.state == Ngs2VoicePlayState::Stopped) {
						voice.state = Ngs2VoicePlayState::Empty;
					}
					break;
				case Ngs2VoicePlayEvent::Play:
					voice.state      = Ngs2VoicePlayState::Playing;
					voice.sample_pos = 0;
					break;
				case Ngs2VoicePlayEvent::Pause:
					if (voice.state == Ngs2VoicePlayState::Playing) {
						voice.state = Ngs2VoicePlayState::Paused;
					}
					break;
				case Ngs2VoicePlayEvent::Resume:
					if (voice.state == Ngs2VoicePlayState::Paused) {
						voice.state = Ngs2VoicePlayState::Playing;
					}
					break;
				case Ngs2VoicePlayEvent::Stop:
					if (voice.state == Ngs2VoicePlayState::Playing ||
					    voice.state == Ngs2VoicePlayState::Paused) {
						voice.state = Ngs2VoicePlayState::Stopped;
					}
					break;
				case Ngs2VoicePlayEvent::StopImm:
				case Ngs2VoicePlayEvent::Kill:
					voice.state      = Ngs2VoicePlayState::Empty;
					voice.sample_pos = 0;
					break;
			}
			voice.event = Ngs2VoicePlayEvent::None;

			if ((rack->type == Ngs2RackType::Sampler || rack->type == Ngs2RackType::CustomSampler) &&
			    voice.state == Ngs2VoicePlayState::Playing) {
				for (uint32_t b = 0; b < num_buffer_info; b++) {
					Ngs2MixVoiceIntoBuffer(&voice, &buffer_info[b], grain_samples, out_rate);
				}
			}
		}
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2ParseWaveformData(const void* data, size_t data_size,
                                        Ngs2WaveformInfo* info) {
	PRINT_NAME();
	LOGF("\t data = 0x%016" PRIx64 ", data_size = 0x%016" PRIx64 "\n",
	     reinterpret_cast<uint64_t>(data), static_cast<uint64_t>(data_size));

	EXIT_NOT_IMPLEMENTED(info == nullptr);

	std::memset(info, 0, sizeof(Ngs2WaveformInfo));
	info->format.waveform_type = 0x12; // PCM I16 LE default
	info->format.num_channels  = 1;
	info->format.sample_rate   = 48000;
	info->data_size =
	    static_cast<uint32_t>(std::min<size_t>(data_size, std::numeric_limits<uint32_t>::max()));
	info->num_audio_unit_samples   = 1;
	info->num_audio_unit_per_frame = 1;
	info->num_audio_frame_samples  = 1;

	// Best-effort RIFF/WAVE detection so PCM titles get honest format metadata.
	if (data != nullptr && data_size >= 44) {
		const auto* bytes = static_cast<const uint8_t*>(data);
		if (std::memcmp(bytes, "RIFF", 4) == 0 && std::memcmp(bytes + 8, "WAVE", 4) == 0) {
			size_t offset = 12;
			while (offset + 8 <= data_size) {
				const auto* chunk_id   = bytes + offset;
				const uint32_t chunk_size =
				    static_cast<uint32_t>(bytes[offset + 4]) |
				    (static_cast<uint32_t>(bytes[offset + 5]) << 8) |
				    (static_cast<uint32_t>(bytes[offset + 6]) << 16) |
				    (static_cast<uint32_t>(bytes[offset + 7]) << 24);
				offset += 8;
				if (offset + chunk_size > data_size) {
					break;
				}
				if (std::memcmp(chunk_id, "fmt ", 4) == 0 && chunk_size >= 16) {
					const uint16_t audio_format =
					    static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
					const uint16_t channels =
					    static_cast<uint16_t>(bytes[offset + 2] | (bytes[offset + 3] << 8));
					const uint32_t sample_rate =
					    static_cast<uint32_t>(bytes[offset + 4]) |
					    (static_cast<uint32_t>(bytes[offset + 5]) << 8) |
					    (static_cast<uint32_t>(bytes[offset + 6]) << 16) |
					    (static_cast<uint32_t>(bytes[offset + 7]) << 24);
					const uint16_t bits =
					    static_cast<uint16_t>(bytes[offset + 14] | (bytes[offset + 15] << 8));
					info->format.num_channels = channels;
					info->format.sample_rate  = sample_rate != 0 ? sample_rate : 48000;
					if (audio_format == 1) {
						if (bits == 8) {
							info->format.waveform_type = 0x10;
						} else if (bits == 16) {
							info->format.waveform_type = 0x12;
						} else if (bits == 32) {
							info->format.waveform_type = 0x16;
						}
					} else if (audio_format == 3 && bits == 32) {
						info->format.waveform_type = 0x18;
					}
					LOGF("\t riff pcm format=0x%" PRIx32 " ch=%u rate=%u bits=%u\n",
					     info->format.waveform_type, info->format.num_channels,
					     info->format.sample_rate, bits);
					break;
				}
				offset += chunk_size + (chunk_size & 1u);
			}
		}
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2CalcWaveformBlock(const Ngs2WaveformFormat* format, uint32_t sample_pos,
                                        uint32_t num_samples, Ngs2WaveformBlock* block) {
	PRINT_NAME();
	LOGF("\t format = 0x%016" PRIx64 ", sample_pos = %" PRIu32 ", num_samples = %" PRIu32 "\n",
	     reinterpret_cast<uint64_t>(format), sample_pos, num_samples);

	EXIT_NOT_IMPLEMENTED(block == nullptr);

	std::memset(block, 0, sizeof(Ngs2WaveformBlock));
	block->num_samples = num_samples;
	return OK;
}

int KYTY_SYSV_ABI Ngs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle,
                              uint32_t num_speakers) {
	PRINT_NAME();
	LOGF("\t work = 0x%016" PRIx64 ", num_speakers = %" PRIu32 "\n",
	     reinterpret_cast<uint64_t>(work), num_speakers);

	EXIT_NOT_IMPLEMENTED(work == nullptr);

	std::memset(work, 0, sizeof(Ngs2PanWork));
	work->unit_angle   = unit_angle;
	work->num_speakers = std::min<uint32_t>(num_speakers, 8);
	if (speaker_angles != nullptr) {
		for (uint32_t i = 0; i < work->num_speakers; i++) {
			work->speaker_angles[i] = speaker_angles[i];
		}
	}
	return OK;
}

int KYTY_SYSV_ABI Ngs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params,
                                         uint32_t num_params, uint32_t matrix_format,
                                         float* out_volume_matrix) {
	PRINT_NAME();
	LOGF("\t work = 0x%016" PRIx64 ", params = 0x%016" PRIx64 ", num_params = %" PRIu32
	     ", matrix_format = %" PRIu32 "\n",
	     reinterpret_cast<uint64_t>(work), reinterpret_cast<uint64_t>(params), num_params,
	     matrix_format);

	EXIT_NOT_IMPLEMENTED(out_volume_matrix == nullptr && num_params != 0);

	const auto channels = (matrix_format == 0 ? 2u : std::min<uint32_t>(matrix_format, 8));
	for (uint32_t p = 0; p < num_params; p++) {
		for (uint32_t c = 0; c < channels; c++) {
			out_volume_matrix[p * channels + c] = (c == 0 ? 1.0f : 0.0f);
		}
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(out_listener_param == nullptr);

	std::memset(out_listener_param, 0, sizeof(Ngs2GeomListenerParam));
	out_listener_param->orient_front.z = 1.0f;
	out_listener_param->orient_up.y    = 1.0f;
	out_listener_param->sound_speed    = 343.0f;

	return OK;
}

int KYTY_SYSV_ABI Ngs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(out_source_param == nullptr);

	std::memset(out_source_param, 0, sizeof(Ngs2GeomSourceParam));
	out_source_param->direction.z                = 1.0f;
	out_source_param->cone.inner_level           = 1.0f;
	out_source_param->cone.inner_angle           = 360.0f;
	out_source_param->cone.outer_level           = 1.0f;
	out_source_param->cone.outer_angle           = 360.0f;
	out_source_param->rolloff.model              = 0;
	out_source_param->rolloff.max_distance       = 1000000.0f;
	out_source_param->rolloff.rolloff_factor     = 1.0f;
	out_source_param->rolloff.reference_distance = 1.0f;
	out_source_param->doppler_factor             = 1.0f;
	out_source_param->fbw_level                  = 1.0f;
	out_source_param->lfe_level                  = 1.0f;
	out_source_param->max_level                  = 1.0f;
	out_source_param->min_level                  = 0.0f;
	out_source_param->num_speakers               = 2;
	out_source_param->matrix_format              = 2;

	return OK;
}

int KYTY_SYSV_ABI Ngs2GeomCalcListener(const Ngs2GeomListenerParam* param,
                                       Ngs2GeomListenerWork* out_work, uint32_t flags) {
	PRINT_NAME();
	LOGF("\t flags = 0x%08" PRIx32 "\n", flags);

	EXIT_NOT_IMPLEMENTED(param == nullptr);
	EXIT_NOT_IMPLEMENTED(out_work == nullptr);

	std::memset(out_work, 0, sizeof(Ngs2GeomListenerWork));
	for (uint32_t i = 0; i < 4; i++) {
		out_work->matrix[i][i] = 1.0f;
	}
	out_work->velocity    = param->velocity;
	out_work->sound_speed = (param->sound_speed > 0.0f ? param->sound_speed : 343.0f);
	out_work->coordinate  = flags & 0x1u;

	return OK;
}

int KYTY_SYSV_ABI Ngs2GeomApply(const Ngs2GeomListenerWork* listener,
                                const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib,
                                uint32_t flags) {
	PRINT_NAME();
	LOGF("\t flags = 0x%08" PRIx32 "\n", flags);

	EXIT_NOT_IMPLEMENTED(listener == nullptr);
	EXIT_NOT_IMPLEMENTED(source == nullptr);
	EXIT_NOT_IMPLEMENTED(out_attrib == nullptr);

	std::memset(out_attrib, 0, sizeof(Ngs2GeomAttribute));
	out_attrib->pitch_ratio         = 1.0f;
	out_attrib->a3d_attrib.position = source->position;
	out_attrib->a3d_attrib.volume   = std::max(source->min_level, source->max_level);

	const auto channels =
	    std::min<uint32_t>((source->matrix_format == 0 ? 2u : source->matrix_format), 8);
	const auto level = (source->max_level > 0.0f ? source->max_level : 1.0f);
	for (uint32_t ch = 0; ch < channels; ch++) {
		out_attrib->level[ch * 8 + ch] = level;
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2RackGetVoiceHandle(uintptr_t rack_handle, uint32_t voice_id,
                                         uintptr_t* handle) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(handle == nullptr);
	EXIT_NOT_IMPLEMENTED(rack_handle == 0);

	LOGF("\t voice_id = %u\n", voice_id);

	auto* rack   = reinterpret_cast<Ngs2RackInternal*>(rack_handle);
	auto* voices = reinterpret_cast<Ngs2VoiceInternal*>(rack_handle + sizeof(Ngs2RackInternal));

	if (voice_id >= rack->option.common.max_voices) {
		LOGF("\t warning: voice_id %u >= max_voices %u, using last available stub voice\n",
		     voice_id, rack->option.common.max_voices);
		if (rack->option.common.max_voices == 0) {
			return -1;
		}
		voice_id = rack->option.common.max_voices - 1;
	}

	EXIT_IF(voices[voice_id].rack != rack);

	*handle = reinterpret_cast<uintptr_t>(voices + voice_id);

	return OK;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int KYTY_SYSV_ABI Ngs2VoiceControl(uintptr_t voice_handle, const Ngs2VoiceParamHeader* param_list) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(param_list == nullptr);
	EXIT_NOT_IMPLEMENTED(voice_handle == 0);

	auto* voice = reinterpret_cast<Ngs2VoiceInternal*>(voice_handle);

	Common::LockGuard lock(voice->rack->ngs->mutex);

	const auto* param = param_list;

	for (;;) {
		LOGF("\t id   = 0x%08" PRIx32 "\n"
		     "\t size = %" PRIu16 "\n"
		     "\t next = %" PRId16 "\n",
		     param->id, param->size, param->next);

		auto rack_id = param->id >> 16u;

		EXIT_NOT_IMPLEMENTED(((param->id >> 15u) & 0x1u) != 0);

		switch (rack_id) {
			case 0x0000: {
				auto cid = param->id & 0x7fffu;
				switch (cid) {
					case 0x0001: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoiceMatrixLevelsParam));
						const auto* ml = reinterpret_cast<const Ngs2VoiceMatrixLevelsParam*>(param);
						LOGF("\t matrix_id  = %u\n"
						     "\t num_levels = %u\n"
						     "\t levels     = 0x%016" PRIx64 "\n",
						     ml->matrix_id, ml->num_levels, reinterpret_cast<uint64_t>(ml->levels));
						if (ml->levels != nullptr && ml->num_levels > 0) {
							const uint32_t n = std::min<uint32_t>(ml->num_levels, 8);
							for (uint32_t i = 0; i < n; i++) {
								voice->matrix_levels[i] = ml->levels[i];
							}
							voice->num_matrix_levels = n;
						}
						break;
					}
					case 0x0002: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoicePortVolumeParam));
						const auto* volume =
						    reinterpret_cast<const Ngs2VoicePortVolumeParam*>(param);
						voice->port_volume = volume->level;
						LOGF("\t port  = %u\n"
						     "\t level = %f\n",
						     volume->port, volume->level);
						break;
					}
					case 0x0003: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoicePortMatrixParam));
						const auto* pm = reinterpret_cast<const Ngs2VoicePortMatrixParam*>(param);
						LOGF("\t port      = %u\n"
						     "\t matrix_id = %d\n",
						     pm->port, pm->matrix_id);
						break;
					}
					case 0x0004: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoicePortDelayParam));
						const auto* delay = reinterpret_cast<const Ngs2VoicePortDelayParam*>(param);
						LOGF("\t port        = %u\n"
						     "\t num_samples = %u\n",
						     delay->port, delay->num_samples);
						break;
					}
					case 0x0005: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoicePatchParam));
						const auto* patch = reinterpret_cast<const Ngs2VoicePatchParam*>(param);
						LOGF("\t connect->port          = %u\n"
						     "\t connect->dest_input_id = %u\n"
						     "\t connect->dest_handle   = 0x%016" PRIx64 "\n",
						     patch->port, patch->dest_input_id, patch->dest_handle);
						break;
					}
					case 0x0006: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoiceEventParam));
						const auto* event = reinterpret_cast<const Ngs2VoiceEventParam*>(param);
						switch (event->event_id) {
							case 0x0001: voice->event = Ngs2VoicePlayEvent::Play; break;
							case 0x0002: voice->event = Ngs2VoicePlayEvent::Stop; break;
							case 0x0004: voice->event = Ngs2VoicePlayEvent::StopImm; break;
							case 0x0008: voice->event = Ngs2VoicePlayEvent::Kill; break;
							case 0x0010: voice->event = Ngs2VoicePlayEvent::Pause; break;
							case 0x0020: voice->event = Ngs2VoicePlayEvent::Resume; break;
							default: {
								static std::atomic_uint32_t soft_logs {0};
								if (soft_logs.fetch_add(1, std::memory_order_relaxed) < 32) {
									LOGF_COLOR(Log::Color::Yellow,
									           "Ngs2: soft-ignore unknown voice event_id=0x%08" PRIx32
									           "\n",
									           event->event_id);
								}
								break;
							}
						}
						LOGF("\t event = %u\n", event->event_id);
						break;
					}
					case 0x0007: {
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2VoiceCallbackParam));
						const auto* callback =
						    reinterpret_cast<const Ngs2VoiceCallbackParam*>(param);
						voice->callback       = callback->callback;
						voice->callback_data  = callback->callback_data;
						voice->callback_flags = callback->flags;
						LOGF("\t callback      = 0x%016" PRIx64 "\n"
						     "\t callback_data = 0x%016" PRIx64 "\n"
						     "\t flags         = 0x%08" PRIx32 "\n",
						     static_cast<uint64_t>(voice->callback),
						     static_cast<uint64_t>(voice->callback_data), voice->callback_flags);
						break;
					}
					default: EXIT("unknown id: 0x%04" PRIx32 "\n", cid);
				}
				break;
			}
			case 0x1000: {
				EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::Sampler);
				const auto cid = param->id & 0x7fffu;
				switch (cid) {
					case 0x0000: { // SETUP
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2SamplerVoiceSetupParam));
						const auto* setup =
						    reinterpret_cast<const Ngs2SamplerVoiceSetupParam*>(param);
						voice->format = setup->format;
						LOGF("\t setup waveform_type = 0x%" PRIx32 ", channels = %" PRIu32
						     ", rate = %" PRIu32 "\n",
						     setup->format.waveform_type, setup->format.num_channels,
						     setup->format.sample_rate);
						break;
					}
					case 0x0001: { // ADD_WAVEFORM_BLOCKS
						EXIT_NOT_IMPLEMENTED(param->size <
						                     sizeof(Ngs2SamplerVoiceWaveformBlocksParam));
						const auto* blocks =
						    reinterpret_cast<const Ngs2SamplerVoiceWaveformBlocksParam*>(param);
						LOGF("\t blocks data = 0x%016" PRIx64 ", flags = 0x%" PRIx32
						     ", num_blocks = %" PRIu32 ", a_block = 0x%016" PRIx64 "\n",
						     reinterpret_cast<uint64_t>(blocks->data), blocks->flags,
						     blocks->num_blocks, reinterpret_cast<uint64_t>(blocks->a_block));
						if (blocks->data == nullptr || blocks->num_blocks == 0 ||
						    blocks->a_block == nullptr) {
							voice->waveform_data      = nullptr;
							voice->waveform_data_size = 0;
							voice->block_num_samples  = 0;
							voice->block_num_repeats  = 0;
							voice->sample_pos         = 0;
							break;
						}
						const auto& block0 = blocks->a_block[0];
						voice->waveform_data =
						    static_cast<const uint8_t*>(blocks->data) + block0.data_offset;
						voice->waveform_data_size = block0.data_size;
						voice->block_num_samples  = block0.num_samples;
						voice->block_num_repeats  = block0.num_repeats;
						voice->waveform_user_data = block0.user_data;
						voice->sample_pos         = block0.num_skip_samples;
						LOGF("\t block0 offset = %" PRIu32 ", size = %" PRIu32
						     ", samples = %" PRIu32 ", repeats = %" PRIu32 "\n",
						     block0.data_offset, block0.data_size, block0.num_samples,
						     block0.num_repeats);
						break;
					}
					case 0x0002: { // REPLACE_WAVEFORM_ADDRESS
						EXIT_NOT_IMPLEMENTED(param->size !=
						                     sizeof(Ngs2SamplerVoiceWaveformAddressParam));
						const auto* addr =
						    reinterpret_cast<const Ngs2SamplerVoiceWaveformAddressParam*>(param);
						LOGF("\t addr from = 0x%016" PRIx64 ", to = 0x%016" PRIx64 "\n",
						     reinterpret_cast<uint64_t>(addr->from),
						     reinterpret_cast<uint64_t>(addr->to));
						if (addr->from == nullptr || addr->to == nullptr || addr->to < addr->from) {
							voice->waveform_data      = nullptr;
							voice->waveform_data_size = 0;
							voice->sample_pos         = 0;
							break;
						}
						voice->waveform_data = static_cast<const uint8_t*>(addr->from);
						voice->waveform_data_size = static_cast<uint32_t>(
						    reinterpret_cast<uintptr_t>(addr->to) -
						    reinterpret_cast<uintptr_t>(addr->from));
						voice->sample_pos = 0;
						break;
					}
					case 0x0005: { // SET_PITCH
						EXIT_NOT_IMPLEMENTED(param->size != sizeof(Ngs2SamplerVoicePitchParam));
						const auto* pitch =
						    reinterpret_cast<const Ngs2SamplerVoicePitchParam*>(param);
						voice->pitch_ratio = pitch->ratio > 0.0f ? pitch->ratio : 1.0f;
						LOGF("\t pitch_ratio = %f\n", voice->pitch_ratio);
						break;
					}
					default: Ngs2SoftIgnoreSamplerCtl(cid, param->size); break;
				}
				break;
			}
			case 0x2000: EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::Submixer); break;
			case 0x2001: EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::Reverb); break;
			case 0x3000: EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::Mastering); break;
			case 0x4000: {
				EXIT_NOT_IMPLEMENTED(!Ngs2RackIsCustom(voice->rack->type));
				auto cid       = param->id & 0xffffu;
				auto module_id = (cid >> 8u) & 0xffu;
				auto ctl_id    = (cid >> 5u) & 0x7u;
				auto module_no = cid & 0x1fu;
				LOGF("\t custom module_id = 0x%02" PRIx32 ", ctl_id = 0x%" PRIx32
				     ", module_no = %" PRIu32 "\n",
				     module_id, ctl_id, module_no);
				break;
			}
			case 0x4001:
				EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::CustomSampler);
				break;
			case 0x4002:
				EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::CustomSubmixer);
				break;
			case 0x4003:
				EXIT_NOT_IMPLEMENTED(voice->rack->type != Ngs2RackType::CustomMastering);
				break;
			default: {
				static std::atomic<uint32_t> soft_logs {0};
				if (soft_logs.fetch_add(1, std::memory_order_relaxed) < 16) {
					LOGF_COLOR(Log::Color::Yellow,
					           "Ngs2: soft-ignore unknown voice rack_id 0x%" PRIx32 "\n", rack_id);
				}
				break;
			}
		}

		if (param->next == 0) {
			break;
		}
		param = reinterpret_cast<const Ngs2VoiceParamHeader*>(reinterpret_cast<uintptr_t>(param) +
		                                                      param->next);
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2VoiceRunCommands(uintptr_t voice_handle, const void* commands,
                                       uint32_t num_commands, uint32_t flags) {
	PRINT_NAME();

	(void)voice_handle;
	(void)commands;
	(void)num_commands;
	(void)flags;

	return OK;
}

int KYTY_SYSV_ABI Ngs2VoiceGetState(uintptr_t voice_handle, Ngs2VoiceState* state,
                                    size_t state_size) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(state == nullptr);
	EXIT_NOT_IMPLEMENTED(voice_handle == 0);

	auto* voice = reinterpret_cast<Ngs2VoiceInternal*>(voice_handle);

	Common::LockGuard lock(voice->rack->ngs->mutex);

	switch (voice->rack->type) {
		case Ngs2RackType::Sampler:
		case Ngs2RackType::CustomSampler: {
			if (state_size != sizeof(Ngs2SamplerVoiceState)) {
				LOGF("\t warning: sampler state_size = 0x%016" PRIx64 ", expected 0x%016" PRIx64
				     "\n",
				     static_cast<uint64_t>(state_size),
				     static_cast<uint64_t>(sizeof(Ngs2SamplerVoiceState)));
			}
			std::memset(state, 0, state_size);

			state->state_flags = Ngs2GetStateFlags(voice);
			if (state_size < sizeof(Ngs2SamplerVoiceState)) {
				LOGF("\t state_flags = %u\n", state->state_flags);
				break;
			}

			auto* sampler                = reinterpret_cast<Ngs2SamplerVoiceState*>(state);
			sampler->envelope_height     = 1.0f;
			sampler->peak_height         = 0.0f;
			sampler->reserved            = 0;
			sampler->num_decoded_samples = voice->num_decoded_samples;
			sampler->decoded_data_size   = voice->decoded_data_size;
			sampler->user_data           = voice->waveform_user_data;
			sampler->waveform_data       = voice->waveform_data;
			LOGF("\t state_flags = %u\n", sampler->voice_state.state_flags);
			break;
		}
		default: EXIT("unknown type: %s\n", Common::EnumName(voice->rack->type).c_str());
	}

	return OK;
}

int KYTY_SYSV_ABI Ngs2VoiceGetStateFlags(uintptr_t voice_handle, uint32_t* state_flags) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(state_flags == nullptr);
	EXIT_NOT_IMPLEMENTED(voice_handle == 0);

	auto* voice = reinterpret_cast<Ngs2VoiceInternal*>(voice_handle);

	Common::LockGuard lock(voice->rack->ngs->mutex);

	*state_flags = Ngs2GetStateFlags(voice);

	LOGF("\t state_flags = %u\n", *state_flags);

	return OK;
}

} // namespace Ngs2

} // namespace Libs::Audio
