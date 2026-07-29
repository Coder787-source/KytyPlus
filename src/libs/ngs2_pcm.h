#pragma once

#include <cstdint>

namespace Libs::Audio::Ngs2Pcm {

// Orbis waveformType PCM values (psOff / SDK dumps).
constexpr uint32_t kWavePcmI8        = 0x10;
constexpr uint32_t kWavePcmU8        = 0x11;
constexpr uint32_t kWavePcmI16Little = 0x12;
constexpr uint32_t kWavePcmI16Big    = 0x13;
constexpr uint32_t kWavePcmI32Little = 0x16;
constexpr uint32_t kWavePcmI32Big    = 0x17;
constexpr uint32_t kWavePcmF32Little = 0x18;
constexpr uint32_t kWavePcmF32Big    = 0x19;

[[nodiscard]] inline constexpr uint32_t BytesPerSample(uint32_t waveform_type) noexcept {
	switch (waveform_type) {
		case kWavePcmI8:
		case kWavePcmU8: return 1;
		case kWavePcmI16Little:
		case kWavePcmI16Big: return 2;
		case kWavePcmI32Little:
		case kWavePcmI32Big:
		case kWavePcmF32Little:
		case kWavePcmF32Big: return 4;
		default: return 0;
	}
}

[[nodiscard]] inline constexpr bool IsSupported(uint32_t waveform_type) noexcept {
	return BytesPerSample(waveform_type) != 0;
}

} // namespace Libs::Audio::Ngs2Pcm
