// Included into audio.cpp inside namespace Libs::Audio.
//
// Clean-room port of SharpEmu's Ngs2VagDecoder (GPL-2.0-or-later,
// https://github.com/sharpemu/sharpemu). Decodes the Sony "VAGp" PS-ADPCM
// container into mono PCM16 so the existing Ngs2 PCM mixer can render NGS2
// sampler voices that point at compressed waveforms (#69 / VAG-wrapped SFX).
//
// Algorithm is the publicly documented PSX SPU ADPCM: 16-byte frames with a
// predictor/shift byte, a flags byte, then 14 bytes = 28 4-bit nibbles. Five
// standard predictor filters; loop/stop markers carried in per-frame flags.

namespace { // anonymous, file-local inside Libs::Audio

constexpr uint32_t kVagHeaderSize = 0x30;
constexpr uint32_t kVagMagic      = 0x56414770u; // "VAGp"

// Standard PS-ADPCM predictor filters (coefficients are scaled by 1/64 later).
constexpr int kVagCoeff0[5] = {0, 60, 115, 98, 122};
constexpr int kVagCoeff1[5] = {0, 0, -52, -55, -60};

struct Ngs2VagWaveform {
	std::vector<int16_t> samples;
	uint32_t             sample_rate = 48000;
	int32_t              loop_start  = -1;
	int32_t              loop_end    = -1;
};

static inline uint32_t VagReadBE32(const uint8_t* p) {
	return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
	       (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

static inline bool VagIsVagContainer(const uint8_t* data, size_t size) {
	return data != nullptr && size >= kVagHeaderSize && VagReadBE32(data) == kVagMagic;
}

// Decode raw 16-byte-framed PS-ADPCM (no container header) into PCM16 and
// resolve loop points from the per-frame flag bytes.
static Ngs2VagWaveform VagDecodeFrames(const uint8_t* frames, size_t frame_bytes,
                                       uint32_t sample_rate) {
	Ngs2VagWaveform out;
	out.sample_rate = sample_rate > 0 ? sample_rate : 48000;
	const size_t frame_count = frame_bytes / 16;
	out.samples.resize(frame_count * 28);

	int32_t hist1 = 0;
	int32_t hist2 = 0;
	size_t  out_index = 0;
	bool    ended = false;

	for (size_t frame = 0; frame < frame_count && !ended; frame++) {
		const size_t offset = frame * 16;
		const uint8_t header = frames[offset];
		const uint32_t shift  = header & 0x0Fu;
		uint32_t      filter = (header >> 4u) & 0x0Fu;
		if (filter > 4u) {
			filter = 0;
		}

		// Per-frame loop marker (exact PS-ADPCM values, not bit masks):
		//   3 = loop start, 6 = loop end + jump back, 1/7 = one-shot end.
		const uint8_t flags = frames[offset + 1];
		if (flags == 0x03u) {
			out.loop_start = static_cast<int32_t>(out_index);
		}

		const int f0 = kVagCoeff0[filter];
		const int f1 = kVagCoeff1[filter];
		for (size_t i = 0; i < 14; i++) {
			const uint8_t d = frames[offset + 2 + i];
			for (uint32_t nibble = 0; nibble < 2; nibble++) {
				const uint32_t raw = (nibble == 0) ? (d & 0x0Fu) : ((d >> 4u) & 0x0Fu);
				// Sign-extend the 4-bit sample into the top nibble, then scale.
				const int32_t s         = static_cast<int32_t>(static_cast<int16_t>(raw << 12)) >> shift;
				const int32_t predicted = (hist1 * f0 + hist2 * f1) >> 6;
				int32_t       sample    = s + predicted;
				if (sample < INT16_MIN) {
					sample = INT16_MIN;
				} else if (sample > INT16_MAX) {
					sample = INT16_MAX;
				}
				out.samples[out_index++] = static_cast<int16_t>(sample);
				hist2 = hist1;
				hist1 = sample;
			}
		}

		if (flags == 0x06u) {
			out.loop_end = static_cast<int32_t>(out_index);
		} else if (flags == 0x01u || flags == 0x07u) {
			ended = true;
		}
	}

	if (out_index != out.samples.size()) {
		out.samples.resize(out_index);
	}
	if (out.loop_start >= 0 && out.loop_end <= out.loop_start) {
		out.loop_end = static_cast<int32_t>(out_index);
	}
	return out;
}

// Decode a full "VAGp" container into mono PCM16. Returns false when the
// header is missing/short so callers can skip unsupported formats safely.
static bool VagTryDecodeContainer(const uint8_t* data, size_t size, Ngs2VagWaveform* out) {
	if (out == nullptr || !VagIsVagContainer(data, size)) {
		return false;
	}
	const int32_t  declared_size = static_cast<int32_t>(VagReadBE32(data + 0x0Cu));
	uint32_t       sample_rate   = static_cast<uint32_t>(static_cast<int32_t>(VagReadBE32(data + 0x10u)));
	if (sample_rate == 0) {
		sample_rate = 48000;
	}

	const uint8_t* body       = data + kVagHeaderSize;
	const size_t   body_size  = size - kVagHeaderSize;
	const size_t  available  = body_size - (body_size % 16);
	size_t        frame_bytes = 0;
	if (declared_size > 0 && static_cast<size_t>(declared_size) <= available) {
		frame_bytes = static_cast<size_t>(declared_size) - (static_cast<size_t>(declared_size) % 16);
	} else {
		frame_bytes = available;
	}
	if (frame_bytes == 0) {
		return false;
	}

	*out = VagDecodeFrames(body, frame_bytes, sample_rate);
	return !out->samples.empty();
}

} // anonymous namespace
