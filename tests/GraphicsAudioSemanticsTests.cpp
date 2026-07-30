#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/host_gpu/renderer/image/imageInfo.h"
#include "graphics/host_gpu/renderer/polyOffsetBias.h"
#include "libs/ngs2_pcm.h"
#include "libs/ngs2_vag_decoder.h"
#include "loader/x64InstructionEmulator.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using Libs::Graphics::DecodePackedColorClear;
using Libs::Graphics::ImageInfo;
using Libs::Graphics::IsSupportedStandard64RenderTarget;
using Libs::Graphics::PolyOffsetBias;
using Libs::Graphics::PolyOffsetBiasResult;
using Libs::Graphics::ResolvePolyOffsetBias;
using Libs::Graphics::HW::ModeControl;
using Libs::Graphics::HW::PolyOffset;

namespace Pm4 = Libs::Graphics::Pm4;

static void Require(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", what);
		std::abort();
	}
}

static void TestPolyOffset() {
	ModeControl mc {};
	PolyOffset  po {};
	PolyOffsetBias bias {};

	mc.cull_back                = true;
	mc.poly_offset_front_enable = true;
	po.front_offset             = 1.5f;
	po.front_scale              = 2.0f;
	po.clamp                    = 0.25f;
	Require(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::Enabled, "front bias");
	Require(bias.enable && bias.constant == 1.5f && bias.slope == 2.0f && bias.clamp == 0.25f,
	        "front values");

	mc.cull_back               = false;
	mc.poly_offset_back_enable = true;
	po.back_offset             = 9.0f;
	Require(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::UnsupportedPerFace,
	        "per-face mismatch");

	po.back_offset = po.front_offset;
	po.back_scale  = po.front_scale;
	Require(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::Enabled, "matched faces");

	mc.cull_back    = true;
	po.front_offset = std::numeric_limits<float>::quiet_NaN();
	Require(ResolvePolyOffsetBias(mc, po, bias) == PolyOffsetBiasResult::NonFinite, "nan guard");
}

static void TestColorFastClear() {
	vk::ClearColorValue clear {};
	Require(DecodePackedColorClear(vk::Format::eR8G8B8A8Unorm, 0x44332211u, clear), "rgba8");
	Require(clear.float32[0] == 17.0f / 255.0f && clear.float32[1] == 34.0f / 255.0f &&
	            clear.float32[2] == 51.0f / 255.0f && clear.float32[3] == 68.0f / 255.0f,
	        "rgba8 channels");
	Require(DecodePackedColorClear(vk::Format::eA2B10G10R10UnormPack32, 0xC00FF3FFu, clear),
	        "a2b10");
	Require(!DecodePackedColorClear(vk::Format::eUndefined, 0, clear), "reject undefined");
}

static void TestStandard64KB() {
	ImageInfo info {};
	info.tile_mode       = Libs::Graphics::Prospero::GpuEnumValue(
	    Libs::Graphics::Prospero::TileMode::kStandard64KB);
	info.data.address    = 0x10000;
	info.extent.width    = 128;
	info.extent.height   = 128;
	info.bytes_per_block = 4;
	info.resources       = {1, 1};
	info.samples         = 1;
	info.pitch           = 128;
	info.data.size       = 128ull * 128ull * 4ull;
	Require(IsSupportedStandard64RenderTarget(info), "exact 128x128");

	info.data.address = 0x10001;
	Require(!IsSupportedStandard64RenderTarget(info), "unaligned address");
	info.data.address    = 0x10000;
	info.bytes_per_block = 8;
	Require(!IsSupportedStandard64RenderTarget(info), "bad bpp");
}

static void TestNgs2PcmVag() {
	using namespace Libs::Audio;
	Require(Ngs2Pcm::IsSupported(Ngs2Pcm::kWavePcmI16Little), "pcm16");
	Require(Ngs2Pcm::BytesPerSample(Ngs2Pcm::kWavePcmF32Little) == 4, "f32 size");
	Require(Ngs2Pcm::BytesPerSample(Ngs2Pcm::kWavePcmI8) == 1, "i8 size");
	Require(Ngs2Pcm::BytesPerSample(Ngs2Pcm::kWavePcmI16Big) == 2, "i16be size");
	Require(Ngs2Pcm::BytesPerSample(Ngs2Pcm::kWavePcmI32Little) == 4, "i32 size");
	Require(!Ngs2Pcm::IsSupported(0x4001u), "reject unknown waveform");
	Require(!Ngs2Pcm::IsSupported(0), "reject zero waveform");

	std::array<uint8_t, Ngs2Vag::kHeaderSize + 16> blob {};
	blob[0] = 'V';
	blob[1] = 'A';
	blob[2] = 'G';
	blob[3] = 'p';
	// declared body size = 16, sample rate = 44100
	blob[0x0c] = 0;
	blob[0x0d] = 0;
	blob[0x0e] = 0;
	blob[0x0f] = 16;
	blob[0x10] = 0;
	blob[0x11] = 0;
	blob[0x12] = 0xac;
	blob[0x13] = 0x44;
	// one silent ADPCM frame ending with stop flag
	blob[Ngs2Vag::kHeaderSize + 1] = 0x01;

	Require(Ngs2Vag::IsContainer(blob.data(), blob.size()), "vag magic");
	Ngs2Vag::Waveform decoded {};
	Require(Ngs2Vag::TryDecodeContainer(blob.data(), blob.size(), &decoded), "vag decode");
	Require(decoded.samples.size() == 28, "28 samples/frame");
	Require(decoded.sample_rate == 44100u, "sample rate");
	Require(!Ngs2Vag::IsContainer(blob.data(), 8), "short reject");
	Require(!Ngs2Vag::TryDecodeContainer(blob.data(), 8, &decoded), "short decode reject");

	// empty declared body still uses available frames; header-only has no frames
	auto empty = blob;
	empty[0x0f] = 0;
	Require(Ngs2Vag::TryDecodeContainer(empty.data(), empty.size(), &decoded),
	        "declared 0 uses available");
	Require(decoded.samples.size() == 28, "declared 0 still one frame");
	Require(!Ngs2Vag::TryDecodeContainer(blob.data(), Ngs2Vag::kHeaderSize, &decoded),
	        "header-only reject");

	// truncated payload (header claims 32 but only 16 present) → decode available frames
	auto truncated = blob;
	truncated[0x0f] = 32;
	Require(Ngs2Vag::TryDecodeContainer(truncated.data(), truncated.size(), &decoded),
	        "oversized declared uses available");
	Require(decoded.samples.size() == 28, "truncated still one frame");

	// bad magic
	auto bad = blob;
	bad[3] = 'X';
	Require(!Ngs2Vag::IsContainer(bad.data(), bad.size()), "bad magic");
	Require(!Ngs2Vag::TryDecodeContainer(bad.data(), bad.size(), &decoded), "bad magic decode");

	// zero sample rate → default 48000
	auto rate0 = blob;
	rate0[0x10] = rate0[0x11] = rate0[0x12] = rate0[0x13] = 0;
	Require(Ngs2Vag::TryDecodeContainer(rate0.data(), rate0.size(), &decoded), "rate0 decode");
	Require(decoded.sample_rate == 48000u, "default sample rate");

	// loop start flag (0x03) then stop (0x01)
	std::array<uint8_t, Ngs2Vag::kHeaderSize + 32> looped {};
	looped[0] = 'V';
	looped[1] = 'A';
	looped[2] = 'G';
	looped[3] = 'p';
	looped[0x0f] = 32;
	looped[0x13] = 0x80; // 32768 Hz BE
	looped[0x12] = 0x00;
	looped[Ngs2Vag::kHeaderSize + 1] = 0x03;
	looped[Ngs2Vag::kHeaderSize + 16 + 1] = 0x01;
	Require(Ngs2Vag::TryDecodeContainer(looped.data(), looped.size(), &decoded), "loop decode");
	Require(decoded.loop_start == 0, "loop start");
	Require(decoded.samples.size() == 56, "two frames");
}

static void TestNullPageSkip() {
	using Loader::X64InstructionEmulator::EstimateNullPageSkipLength;
	// mov [rax], ecx  -> 89 08
	const uint8_t mov_store[] = {0x89, 0x08};
	Require(EstimateNullPageSkipLength(mov_store, true) == 2, "mov r/m");
	Require(EstimateNullPageSkipLength(mov_store, false) == 1, "unreadable fallback");
	const uint8_t ret[] = {0xc3};
	Require(EstimateNullPageSkipLength(ret, true) == 1, "unknown opcode fallback");
}

static void TestPm4PolyOffsetPacketShape() {
	const uint32_t header =
	    KYTY_PM4(8, Pm4::IT_SET_CONTEXT_REG, 0) |
	    ((Pm4::PA_SU_POLY_OFFSET_DB_FMT_CNTL & 0xffffu) << 0);
	(void)header;
	Require(KYTY_PM4_LEN(KYTY_PM4(8, Pm4::IT_SET_CONTEXT_REG, 0)) == 8, "pm4 len");
	Require(Pm4::PA_SU_POLY_OFFSET_FRONT_SCALE + 1 == Pm4::PA_SU_POLY_OFFSET_FRONT_OFFSET,
	        "reg order");
	Require(Pm4::PA_SU_POLY_OFFSET_BACK_SCALE + 1 == Pm4::PA_SU_POLY_OFFSET_BACK_OFFSET,
	        "back reg order");
}

int main() {
	TestPolyOffset();
	TestColorFastClear();
	TestStandard64KB();
	TestNgs2PcmVag();
	TestNullPageSkip();
	TestPm4PolyOffsetPacketShape();
	return 0;
}
