#include "graphics/guest_gpu/command_processor/pm4SoftIgnoreProbe.h"
#include "graphics/guest_gpu/pm4.h"
#include "libs/ngs2_pcm.h"
#include "libs/ngs2_vag_decoder.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Pm4 = Libs::Graphics::Pm4;
using namespace Libs::Audio;
using Libs::Graphics::Pm4SoftIgnore::ProbePacketStream;

static uint32_t Mix(uint32_t x) {
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return x;
}

// Emit random-but-structurally-valid Type-3 packets. The goal is to surface
// soft-ignore / EXIT paths without requiring a live command processor.
static void FuzzPm4Packets(uint32_t seed, uint32_t iterations) {
	uint32_t state = seed ? seed : 0xC0FFEEu;
	uint32_t soft_ignore_shaped = 0;
	uint32_t known_reg_shaped   = 0;
	std::vector<uint32_t> shaped_stream;
	shaped_stream.reserve(static_cast<size_t>(iterations) * 8u);

	constexpr std::array<uint32_t, 14> kKnownCx {
	    Pm4::PA_SU_POLY_OFFSET_DB_FMT_CNTL, Pm4::PA_SU_POLY_OFFSET_CLAMP,
	    Pm4::PA_SU_POLY_OFFSET_FRONT_SCALE, Pm4::PA_SU_POLY_OFFSET_FRONT_OFFSET,
	    Pm4::PA_SU_POLY_OFFSET_BACK_SCALE,  Pm4::PA_SU_POLY_OFFSET_BACK_OFFSET,
	    Pm4::CB_SHADER_MASK,               Pm4::DB_SHADER_CONTROL,
	    Pm4::SPI_BARYC_CNTL,               Pm4::SPI_PS_INPUT_ENA,
	    Pm4::SPI_INTERP_CONTROL_0,         Pm4::PA_SC_SHADER_CONTROL,
	    Pm4::DB_STENCILREFMASK_BF,         Pm4::CB_BLEND_ALPHA};

	for (uint32_t i = 0; i < iterations; ++i) {
		state = Mix(state);
		const uint32_t op = [&]() -> uint32_t {
			constexpr std::array<uint32_t, 8> kOps {
			    Pm4::IT_NOP, Pm4::IT_SET_CONTEXT_REG, Pm4::IT_SET_SH_REG, Pm4::IT_SET_UCONFIG_REG,
			    Pm4::IT_EVENT_WRITE, Pm4::IT_COPY_DATA, Pm4::IT_ACQUIRE_MEM, Pm4::IT_RELEASE_MEM};
			return kOps[state % kOps.size()];
		}();
		state              = Mix(state);
		const uint32_t len = 2u + (state % 16u);
		const uint32_t hdr = KYTY_PM4(len, op, Pm4::R_ZERO);
		if (KYTY_PM4_LEN(hdr) != len) {
			std::fprintf(stderr, "FAIL: PM4 length encode/decode mismatch\n");
			std::abort();
		}
		std::vector<uint32_t> packet(len);
		packet[0] = hdr;
		for (uint32_t w = 1; w < len; ++w) {
			state     = Mix(state);
			packet[w] = state;
		}
		if (op == Pm4::IT_SET_CONTEXT_REG && len >= 3) {
			packet[1] = kKnownCx[state % kKnownCx.size()];
			++soft_ignore_shaped;
			++known_reg_shaped;
		}
		shaped_stream.insert(shaped_stream.end(), packet.begin(), packet.end());
	}
	if (soft_ignore_shaped == 0 || known_reg_shaped == 0) {
		std::fprintf(stderr, "FAIL: PM4 fuzz produced no context-reg packets\n");
		std::abort();
	}

	const auto report = ProbePacketStream(shaped_stream, false);
	if (report.packets_parsed == 0) {
		std::fprintf(stderr, "FAIL: PM4 dry-run parsed zero packets\n");
		std::abort();
	}
	for (const auto& hit: report.packet_soft_ignores) {
		if (hit.bank != Libs::Graphics::Pm4SoftIgnore::Bank::Cx) {
			continue;
		}
		for (const auto known: kKnownCx) {
			if (hit.offset == known) {
				std::fprintf(stderr, "FAIL: known CX 0x%03x soft-ignored in fuzz dry-run\n", known);
				std::abort();
			}
		}
	}
}

static void FuzzNgs2Waveforms(uint32_t seed, uint32_t iterations) {
	uint32_t state = seed ? seed : 0xA11DEu;
	uint32_t supported = 0;
	uint32_t rejected  = 0;
	for (uint32_t i = 0; i < iterations; ++i) {
		state              = Mix(state);
		const uint32_t wt  = state & 0xffu;
		if (Ngs2Pcm::IsSupported(wt)) {
			++supported;
			if (Ngs2Pcm::BytesPerSample(wt) == 0) {
				std::fprintf(stderr, "FAIL: supported PCM reported 0 bytes\n");
				std::abort();
			}
		} else {
			++rejected;
		}

		state = Mix(state);
		std::array<uint8_t, Ngs2Vag::kHeaderSize + 32> blob {};
		if ((state & 1u) != 0) {
			blob[0] = 'V';
			blob[1] = 'A';
			blob[2] = 'G';
			blob[3] = 'p';
			blob[0x0f] = 16;
			blob[Ngs2Vag::kHeaderSize + 1] = static_cast<uint8_t>(1 + (state & 7u));
		} else {
			blob[0] = static_cast<uint8_t>(state);
		}
		Ngs2Vag::Waveform decoded {};
		const bool ok = Ngs2Vag::TryDecodeContainer(blob.data(), blob.size(), &decoded);
		if (ok && decoded.samples.empty()) {
			std::fprintf(stderr, "FAIL: VAG decode returned empty success\n");
			std::abort();
		}
		if (!ok && Ngs2Vag::IsContainer(blob.data(), blob.size()) && blob[0x0f] == 0) {
			// empty declared body is a valid reject path
		}
	}
	if (supported == 0 || rejected == 0) {
		std::fprintf(stderr, "FAIL: NGS2 fuzz did not exercise both accept/reject paths\n");
		std::abort();
	}
}

int main(int argc, char** argv) {
	uint32_t iterations = 4096;
	uint32_t seed       = 0x51FFu;
	for (int i = 1; i < argc; ++i) {
		if (std::strncmp(argv[i], "--iterations=", 13) == 0) {
			iterations = static_cast<uint32_t>(std::strtoul(argv[i] + 13, nullptr, 10));
		} else if (std::strncmp(argv[i], "--seed=", 7) == 0) {
			seed = static_cast<uint32_t>(std::strtoul(argv[i] + 7, nullptr, 10));
		}
	}
	FuzzPm4Packets(seed, iterations);
	FuzzNgs2Waveforms(seed ^ 0x9E3779B9u, iterations);
	return 0;
}
