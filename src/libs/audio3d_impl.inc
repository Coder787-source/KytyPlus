// Included into audio.cpp inside namespace Audio3d — real bed/object mix + AudioOut path.

constexpr uint32_t kAudio3dObjectInvalid = 0xffffffffu;
constexpr uint32_t kAudio3dMaxPorts      = 4;
constexpr uint32_t kAudio3dSampleRate    = 48000;
constexpr float    kDownmixFront         = 1.0f;
constexpr float    kDownmixCenter        = 0.7071f;
constexpr float    kDownmixSurround      = 0.7071f;

struct Audio3dOpenParameters {
	size_t   size        = 0x20;
	uint32_t granularity = 256;
	uint32_t rate        = 0;
	uint32_t max_objects = 512;
	uint32_t queue_depth = 2;
	uint32_t buffer_mode = 2;
	uint32_t pad         = 0;
};

enum class Audio3dFormat : uint32_t { S16 = 0, Float = 1 };

enum class Audio3dAttributeId : uint32_t {
	Pcm                   = 1,
	Position              = 2,
	Gain                  = 3,
	Spread                = 4,
	Priority              = 5,
	Passthrough           = 6,
	Ambisonics            = 7,
	ApplicationSpecific   = 8,
	ResetState            = 9,
	Restricted            = 10,
	OutputRoute           = 11,
};

struct Audio3dPcm {
	Audio3dFormat format        = Audio3dFormat::S16;
	uint32_t      pad           = 0;
	void*         sample_buffer = nullptr;
	uint32_t      num_samples   = 0;
};

struct Audio3dAttribute {
	uint32_t attribute_id = 0;
	uint32_t pad          = 0;
	void*    value        = nullptr;
	uint64_t value_size   = 0;
};

struct Audio3dFrame {
	uint8_t*      sample_buffer = nullptr;
	uint32_t      num_samples   = 0;
	uint32_t      num_channels  = 1;
	Audio3dFormat format        = Audio3dFormat::S16;
};

struct Audio3dObject {
	std::deque<Audio3dFrame>                           pcm_queue;
	std::unordered_map<uint32_t, std::vector<uint8_t>> persistent;
	bool                                               unreserved = false;
};

struct Audio3dInternal {
	Audio3dOpenParameters                       params {};
	Common::Mutex                               mutex;
	std::deque<Audio3dFrame>                    bed_queue;
	std::deque<Audio3dFrame>                    mixed_queue;
	std::unordered_map<uint32_t, Audio3dObject> objects;
	uint32_t                                    next_object_id          = 0;
	int                                         audio_out_handle        = -1;
	int                                         user_id                 = 0;
	float                                       late_reverb_level       = 0.0f;
	float                                       downmix_spread_radius   = 2.0f;
	int                                         downmix_spread_height_aware = 0;
	bool                                        used                    = false;
};

static Audio3dInternal g_ports[kAudio3dMaxPorts] = {};

static void Audio3dFreeFrame(Audio3dFrame& frame) {
	std::free(frame.sample_buffer);
	frame.sample_buffer = nullptr;
}

static void Audio3dClearQueue(std::deque<Audio3dFrame>& queue) {
	for (auto& frame: queue) {
		Audio3dFreeFrame(frame);
	}
	queue.clear();
}

static int Audio3dConvertAndEnqueue(std::deque<Audio3dFrame>& queue, const Audio3dPcm& pcm,
                                    uint32_t num_channels, uint32_t granularity,
                                    uint32_t max_entries) {
	if (pcm.sample_buffer == nullptr || pcm.num_samples == 0) {
		return AUDIO_OUT_ERROR_INVALID_PARAM;
	}
	const uint32_t bytes_per_sample =
	    (pcm.format == Audio3dFormat::S16) ? sizeof(int16_t) : sizeof(float);
	if ((reinterpret_cast<uintptr_t>(pcm.sample_buffer) & (bytes_per_sample - 1u)) != 0) {
		return AUDIO_OUT_ERROR_INVALID_PARAM;
	}
	if (queue.size() >= max_entries) {
		return AUDIO_OUT_ERROR_BUSY;
	}

	const uint32_t dst_bytes = granularity * num_channels * bytes_per_sample;
	auto*          copy      = static_cast<uint8_t*>(std::calloc(1, dst_bytes));
	if (copy == nullptr) {
		return AUDIO_OUT_ERROR_OUT_OF_MEMORY;
	}
	const uint32_t samples_to_copy = std::min(pcm.num_samples, granularity);
	std::memcpy(copy, pcm.sample_buffer,
	            static_cast<size_t>(samples_to_copy) * num_channels * bytes_per_sample);
	queue.push_back(Audio3dFrame {copy, granularity, num_channels, pcm.format});
	return OK;
}

static void Audio3dMixQueueInto(std::deque<Audio3dFrame>& queue, float* mix_float,
                                uint32_t granularity, float gain) {
	if (queue.empty()) {
		return;
	}
	if (gain == 0.0f) {
		Audio3dFrame data = queue.front();
		queue.pop_front();
		Audio3dFreeFrame(data);
		return;
	}

	Audio3dFrame   data     = queue.front();
	queue.pop_front();
	const uint32_t frames   = std::min(granularity, data.num_samples);
	const uint32_t channels = data.num_channels;

	auto accumulate = [&](auto sample_at) {
		for (uint32_t i = 0; i < frames; i++) {
			float left  = 0.0f;
			float right = 0.0f;
			if (channels == 1) {
				const float v = sample_at(i, 0);
				left = right = v;
			} else {
				left  = kDownmixFront * sample_at(i, 0);
				right = kDownmixFront * sample_at(i, 1);
				if (channels >= 3) {
					const float center = kDownmixCenter * sample_at(i, 2);
					left += center;
					right += center;
				}
				if (channels >= 6) {
					left += kDownmixSurround * sample_at(i, 4);
					right += kDownmixSurround * sample_at(i, 5);
				}
				if (channels >= 8) {
					left += kDownmixSurround * sample_at(i, 6);
					right += kDownmixSurround * sample_at(i, 7);
				}
			}
			mix_float[i * 2 + 0] += left * gain;
			mix_float[i * 2 + 1] += right * gain;
		}
	};

	if (data.format == Audio3dFormat::S16) {
		const auto* src = reinterpret_cast<const int16_t*>(data.sample_buffer);
		accumulate([&](uint32_t i, uint32_t c) {
			return static_cast<float>(src[i * channels + c]) / 32768.0f;
		});
	} else {
		const auto* src = reinterpret_cast<const float*>(data.sample_buffer);
		accumulate([&](uint32_t i, uint32_t c) { return src[i * channels + c]; });
	}
	Audio3dFreeFrame(data);
}

static int Audio3dProcessMix(Audio3dInternal* port) {
	const uint32_t granularity = port->params.granularity != 0 ? port->params.granularity : 256;
	const uint32_t out_samples = granularity * 2;
	auto*          mix_float   = static_cast<float*>(std::calloc(out_samples, sizeof(float)));
	if (mix_float == nullptr) {
		return AUDIO_OUT_ERROR_OUT_OF_MEMORY;
	}

	Audio3dMixQueueInto(port->bed_queue, mix_float, granularity, 1.0f);
	for (auto& entry: port->objects) {
		auto& obj  = entry.second;
		float gain = 0.0f;
		auto  it   = obj.persistent.find(static_cast<uint32_t>(Audio3dAttributeId::Gain));
		if (it != obj.persistent.end() && it->second.size() >= sizeof(float)) {
			std::memcpy(&gain, it->second.data(), sizeof(float));
		}
		Audio3dMixQueueInto(obj.pcm_queue, mix_float, granularity, gain);
	}

	auto* mix_s16 = static_cast<int16_t*>(std::malloc(out_samples * sizeof(int16_t)));
	if (mix_s16 == nullptr) {
		std::free(mix_float);
		return AUDIO_OUT_ERROR_OUT_OF_MEMORY;
	}
	for (uint32_t i = 0; i < out_samples; i++) {
		const float v = std::clamp(mix_float[i], -1.0f, 1.0f);
		mix_s16[i]    = static_cast<int16_t>(v * 32767.0f);
	}
	std::free(mix_float);

	port->mixed_queue.push_back(
	    Audio3dFrame {reinterpret_cast<uint8_t*>(mix_s16), granularity, 2, Audio3dFormat::S16});

	for (auto it = port->objects.begin(); it != port->objects.end();) {
		if (it->second.unreserved && it->second.pcm_queue.empty()) {
			it = port->objects.erase(it);
		} else {
			++it;
		}
	}
	return OK;
}

static int Audio3dEnsureAudioOut(Audio3dInternal* port) {
	if (port->audio_out_handle >= 0) {
		return OK;
	}
	EXIT_IF(g_audio == nullptr);
	const uint32_t grain = port->params.granularity != 0 ? port->params.granularity : 256;
	auto           id =
	    g_audio->AudioOutOpen(AUDIO_OUT_PORT_TYPE_AUDIO3D, grain, kAudio3dSampleRate,
	                          Audio::Format::Signed16bitStereo);
	if (!id.IsValid()) {
		return AUDIO_OUT_ERROR_PORT_FULL;
	}
	port->audio_out_handle = id.ToInt();
	return OK;
}

int KYTY_SYSV_ABI Audio3dInitialize(int64_t reserved) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(reserved != 0);
	return OK;
}

void KYTY_SYSV_ABI Audio3dGetDefaultOpenParameters(Audio3dOpenParameters* p) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(sizeof(Audio3dOpenParameters) != 0x20);
	*p = Audio3dOpenParameters();
}

int KYTY_SYSV_ABI Audio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters,
                                  uint32_t* id) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(parameters == nullptr);
	EXIT_NOT_IMPLEMENTED(id == nullptr);
	EXIT_NOT_IMPLEMENTED(parameters->size != 0x20);

	LOGF("\t user_id     = %d\n"
	     "\t granularity = %u\n"
	     "\t rate        = %u\n"
	     "\t max_objects = %u\n"
	     "\t queue_depth = %u\n"
	     "\t buffer_mode = %u\n",
	     user_id, parameters->granularity, parameters->rate, parameters->max_objects,
	     parameters->queue_depth, parameters->buffer_mode);

	EXIT_NOT_IMPLEMENTED(parameters->buffer_mode != 2);
	EXIT_NOT_IMPLEMENTED(user_id != 255 && user_id != 1);

	uint32_t port = 0;
	for (; port < kAudio3dMaxPorts; port++) {
		if (!g_ports[port].used) {
			break;
		}
	}
	EXIT_NOT_IMPLEMENTED(port >= kAudio3dMaxPorts);

	g_ports[port].params                      = *parameters;
	g_ports[port].user_id                     = user_id;
	g_ports[port].audio_out_handle            = -1;
	g_ports[port].next_object_id              = 0;
	g_ports[port].late_reverb_level           = 0.0f;
	g_ports[port].downmix_spread_radius       = 2.0f;
	g_ports[port].downmix_spread_height_aware = 0;
	g_ports[port].used                        = true;
	Audio3dClearQueue(g_ports[port].bed_queue);
	Audio3dClearQueue(g_ports[port].mixed_queue);
	g_ports[port].objects.clear();
	*id = port;
	return OK;
}

int KYTY_SYSV_ABI Audio3dPortClose(uint32_t port_id) {
	PRINT_NAME();
	if (port_id >= kAudio3dMaxPorts || !g_ports[port_id].used) {
		return AUDIO_OUT_ERROR_INVALID_PORT;
	}
	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	if (port.audio_out_handle >= 0) {
		(void)g_audio->AudioOutClose(Audio::Id(port.audio_out_handle));
		port.audio_out_handle = -1;
	}
	Audio3dClearQueue(port.mixed_queue);
	Audio3dClearQueue(port.bed_queue);
	for (auto& entry: port.objects) {
		Audio3dClearQueue(entry.second.pcm_queue);
	}
	port.objects.clear();
	port.used = false;
	return OK;
}

int KYTY_SYSV_ABI Audio3dPortSetAttribute(uint32_t port_id, uint32_t attribute_id,
                                          const void* attribute, size_t attribute_size) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);
	EXIT_NOT_IMPLEMENTED(attribute == nullptr);
	LOGF("\t attribute_id = 0x%" PRIx32 "\n", attribute_id);

	switch (attribute_id) {
		case 0x10001:
			EXIT_NOT_IMPLEMENTED(attribute_size != 4);
			g_ports[port_id].late_reverb_level = *static_cast<const float*>(attribute);
			LOGF("\t late_reverb_level = %f\n", g_ports[port_id].late_reverb_level);
			break;
		case 0x10002:
			EXIT_NOT_IMPLEMENTED(attribute_size != 4);
			g_ports[port_id].downmix_spread_radius = *static_cast<const float*>(attribute);
			LOGF("\t downmix_spread_radius = %f\n", g_ports[port_id].downmix_spread_radius);
			break;
		case 0x10003:
			EXIT_NOT_IMPLEMENTED(attribute_size != 4);
			g_ports[port_id].downmix_spread_height_aware = *static_cast<const int*>(attribute);
			LOGF("\t downmix_spread_height_aware = %d\n",
			     g_ports[port_id].downmix_spread_height_aware);
			break;
		default: {
			static std::atomic_uint32_t soft_logs {0};
			if (soft_logs.fetch_add(1, std::memory_order_relaxed) < 16) {
				LOGF_COLOR(Log::Color::Yellow,
				           "Audio3d: soft-ignore port attribute 0x%" PRIx32 " size=%zu\n",
				           attribute_id, attribute_size);
			}
			break;
		}
	}
	return OK;
}

int KYTY_SYSV_ABI Audio3dPortGetQueueLevel(uint32_t port_id, uint32_t* queue_level,
                                           uint32_t* queue_available) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);
	EXIT_NOT_IMPLEMENTED(queue_level == nullptr && queue_available == nullptr);

	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	const uint32_t    depth = port.params.queue_depth != 0 ? port.params.queue_depth : 2;
	const uint32_t    used =
	    static_cast<uint32_t>(std::min<size_t>(port.mixed_queue.size(), depth));
	const uint32_t available = depth - used;
	LOGF("\t queue_level = %u, available = %u\n", used, available);
	if (queue_level != nullptr) {
		*queue_level = used;
	}
	if (queue_available != nullptr) {
		*queue_available = available;
	}
	return OK;
}

int KYTY_SYSV_ABI Audio3dPortAdvance(uint32_t port_id) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);

	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	EXIT_NOT_IMPLEMENTED(port.params.buffer_mode == 0);

	const uint32_t depth = port.params.queue_depth != 0 ? port.params.queue_depth : 2;
	if (port.mixed_queue.size() >= depth) {
		static std::atomic_uint32_t soft_logs {0};
		if (soft_logs.fetch_add(1, std::memory_order_relaxed) < 16) {
			LOGF_COLOR(Log::Color::Yellow, "Audio3d: Advance dropped, mixed queue full\n");
		}
		return AUDIO_OUT_ERROR_BUSY;
	}
	return Audio3dProcessMix(&port);
}

int KYTY_SYSV_ABI Audio3dPortPush(uint32_t port_id, uint32_t blocking) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);
	LOGF("\t blocking = %u\n", blocking);

	auto& port = g_ports[port_id];
	EXIT_NOT_IMPLEMENTED(port.params.buffer_mode != 2);

	const int open_ret = Audio3dEnsureAudioOut(&port);
	if (open_ret != OK) {
		return open_ret;
	}

	const uint32_t depth = port.params.queue_depth != 0 ? port.params.queue_depth : 2;

	auto submit_one = [&]() -> int {
		Audio3dFrame frame;
		{
			Common::LockGuard lock(port.mutex);
			if (port.mixed_queue.empty()) {
				return OK;
			}
			frame = port.mixed_queue.front();
			port.mixed_queue.pop_front();
		}
		Audio::OutputParam params[1];
		params[0].handle      = Audio::Id(port.audio_out_handle);
		params[0].data        = frame.sample_buffer;
		const auto queued     = g_audio->AudioOutOutputs(params, 1, blocking != 0);
		Audio3dFreeFrame(frame);
		if (queued == 0) {
			return AUDIO_OUT_ERROR_BUSY;
		}
		return OK;
	};

	{
		Common::LockGuard lock(port.mutex);
		if (port.mixed_queue.size() < depth) {
			return OK;
		}
	}

	int ret = submit_one();
	if (ret != OK) {
		return ret;
	}
	if (blocking == 0) {
		return OK;
	}
	while (true) {
		{
			Common::LockGuard lock(port.mutex);
			if (port.mixed_queue.size() < depth) {
				break;
			}
		}
		ret = submit_one();
		if (ret != OK) {
			return ret;
		}
	}
	return OK;
}

int KYTY_SYSV_ABI Audio3dObjectReserve(uint32_t port_id, uint32_t* object_id) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(object_id == nullptr);
	*object_id = kAudio3dObjectInvalid;
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);

	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	if (port.objects.size() >= port.params.max_objects) {
		return AUDIO_OUT_ERROR_PORT_FULL;
	}
	do {
		++port.next_object_id;
	} while (port.next_object_id == 0 || port.next_object_id == kAudio3dObjectInvalid ||
	         port.objects.find(port.next_object_id) != port.objects.end());

	*object_id = port.next_object_id;
	port.objects.emplace(*object_id, Audio3dObject {});
	LOGF("\t object_id = %u\n", *object_id);
	return OK;
}

int KYTY_SYSV_ABI Audio3dObjectUnreserve(uint32_t port_id, uint32_t object_id) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);
	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	auto              it = port.objects.find(object_id);
	if (it == port.objects.end()) {
		return OK;
	}
	it->second.unreserved = true;
	if (it->second.pcm_queue.empty()) {
		port.objects.erase(it);
	}
	return OK;
}

int KYTY_SYSV_ABI Audio3dObjectSetAttributes(uint32_t port_id, uint32_t object_id,
                                             uint64_t                num_attributes,
                                             const Audio3dAttribute* attribute_array) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);
	EXIT_NOT_IMPLEMENTED(num_attributes == 0 || attribute_array == nullptr);

	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	auto              it = port.objects.find(object_id);
	if (it == port.objects.end() || it->second.unreserved) {
		return OK;
	}
	auto& obj = it->second;

	for (uint64_t i = 0; i < num_attributes; i++) {
		if (attribute_array[i].attribute_id ==
		    static_cast<uint32_t>(Audio3dAttributeId::ResetState)) {
			Audio3dClearQueue(obj.pcm_queue);
			obj.persistent.clear();
			break;
		}
	}

	for (uint64_t i = 0; i < num_attributes; i++) {
		const auto& attribute = attribute_array[i];
		switch (static_cast<Audio3dAttributeId>(attribute.attribute_id)) {
			case Audio3dAttributeId::ResetState: break;
			case Audio3dAttributeId::Pcm: {
				if (attribute.value == nullptr || attribute.value_size < sizeof(Audio3dPcm)) {
					continue;
				}
				const auto* pcm = static_cast<const Audio3dPcm*>(attribute.value);
				const int   ret = Audio3dConvertAndEnqueue(
                    obj.pcm_queue, *pcm, 1, port.params.granularity, port.params.queue_depth);
				if (ret != OK) {
					return ret;
				}
				break;
			}
			default: {
				if (attribute.value != nullptr && attribute.value_size > 0) {
					const auto* src = static_cast<const uint8_t*>(attribute.value);
					obj.persistent[attribute.attribute_id].assign(src,
					                                              src + attribute.value_size);
				}
				break;
			}
		}
	}
	return OK;
}

int KYTY_SYSV_ABI Audio3dBedWrite(uint32_t port_id, uint32_t num_channels, uint32_t format,
                                  void* buffer, uint32_t num_samples) {
	PRINT_NAME();
	EXIT_NOT_IMPLEMENTED(port_id >= kAudio3dMaxPorts);
	EXIT_NOT_IMPLEMENTED(!g_ports[port_id].used);
	EXIT_NOT_IMPLEMENTED(format > 1);
	EXIT_NOT_IMPLEMENTED(num_channels != 2 && num_channels != 6 && num_channels != 8);
	EXIT_NOT_IMPLEMENTED(buffer == nullptr || num_samples == 0);

	LOGF("\t channels = %u, format = %u, samples = %u\n", num_channels, format, num_samples);

	auto&             port = g_ports[port_id];
	Common::LockGuard lock(port.mutex);
	Audio3dPcm        pcm;
	pcm.format        = static_cast<Audio3dFormat>(format);
	pcm.sample_buffer = buffer;
	pcm.num_samples   = num_samples;
	return Audio3dConvertAndEnqueue(port.bed_queue, pcm, num_channels, port.params.granularity,
	                                port.params.queue_depth);
}

int KYTY_SYSV_ABI Audio3dBedWrite2(uint32_t port_id, uint32_t num_channels, uint32_t format,
                                   void* buffer, uint32_t num_samples, uint32_t /*output_route*/,
                                   bool /*restricted*/) {
	return Audio3dBedWrite(port_id, num_channels, format, buffer, num_samples);
}
