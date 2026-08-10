#include "graphics/guest_gpu/gpu_format.h"

#include "graphics/guest_gpu/gpu_defs.h"

#include <array>

namespace Libs::Graphics::Prospero {
namespace {

struct FormatInfo {
	uint32_t format;
	uint32_t bytes_per_element;
	uint32_t block_compressed_bytes_per_block;
	uint32_t render_target_bytes_per_element;
	bool     supported_sampled_texture_format;
	bool     unsigned_integer_texture_format;
};

constexpr FormatInfo kFormatInfo[] = {
    {GpuEnumValue(BufferFormat::k8UNorm), 1, 0, 1, true, false},
    {GpuEnumValue(BufferFormat::k8SNorm), 0, 0, 1, false, false},
    {GpuEnumValue(BufferFormat::k8UInt), 1, 0, 1, true, true},
    {GpuEnumValue(BufferFormat::k16UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k16SNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k16UInt), 2, 0, 2, true, true},
    {GpuEnumValue(BufferFormat::k16SInt), 2, 0, 2, false, false},
    {GpuEnumValue(BufferFormat::k16Float), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8SNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8UInt), 2, 0, 2, true, true},
    {GpuEnumValue(BufferFormat::k8_8SInt), 2, 0, 2, false, false},
    {GpuEnumValue(BufferFormat::k32UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k32SInt), 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k32Float), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16UNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16SNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k16_16SInt), 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k16_16Float), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k11_11_10Float), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k10_10_10_2UNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k10_10_10_2UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k8_8_8_8UNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8SNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k8_8_8_8SInt), 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k32_32UInt), 8, 0, 8, true, true},
    {GpuEnumValue(BufferFormat::k32_32SInt), 8, 0, 8, false, false},
    {GpuEnumValue(BufferFormat::k32_32Float), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16UNorm), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16SNorm), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16UInt), 8, 0, 8, true, true},
    {GpuEnumValue(BufferFormat::k16_16_16_16SInt), 8, 0, 8, false, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16Float), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k32_32_32UInt), 12, 0, 12, true, true},
    {GpuEnumValue(BufferFormat::k32_32_32SInt), 12, 0, 12, false, false},
    {GpuEnumValue(BufferFormat::k32_32_32Float), 12, 0, 12, true, false},
    {GpuEnumValue(BufferFormat::k32_32_32_32UInt), 16, 0, 16, true, true},
    {GpuEnumValue(BufferFormat::k32_32_32_32SInt), 16, 0, 16, false, false},
    {GpuEnumValue(BufferFormat::k32_32_32_32Float), 16, 0, 16, true, false},
    {GpuEnumValue(BufferFormat::k8Srgb), 1, 0, 0, true, false},
    {GpuEnumValue(BufferFormat::k8_8Srgb), 2, 0, 0, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8Srgb), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k9_9_9_5Float), 4, 0, 0, true, false},
    {GpuEnumValue(BufferFormat::k5_6_5UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k5_5_5_1UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k4_4_4_4UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::kFmask8_S4_F4), 1, 0, 1, true, false},
    {GpuEnumValue(BufferFormat::kBc1UNorm), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc1Srgb), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc2UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc2Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc3UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc3Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc4UNorm), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc4SNorm), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc5UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc5SNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc6UFloat), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc6SFloat), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc7UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc7Srgb), 0, 16, 0, true, false},
    // ASTC formats: 16 bytes per block regardless of block footprint.
    {GpuEnumValue(BufferFormat::kAstc4x4UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc4x4Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc5x4UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc5x4Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc5x5UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc5x5Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc6x5UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc6x5Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc6x6UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc6x6Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc8x5UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc8x5Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc8x6UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc8x6Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc8x8UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc8x8Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x5UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x5Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x6UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x6Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x8UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x8Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x10UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc10x10Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc12x10UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc12x10Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc12x12UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kAstc12x12Srgb), 0, 16, 0, true, false},
};

constexpr auto MakeFormatInfoLookup() {
	constexpr uint32_t                            kMaxFormat = GpuEnumValue(BufferFormat::kAstc12x12Srgb);
	std::array<const FormatInfo*, kMaxFormat + 1> lookup {};
	for (const auto& info: kFormatInfo) {
		lookup[info.format] = &info;
	}
	return lookup;
}

constexpr auto kFormatInfoLookup = MakeFormatInfoLookup();

const FormatInfo* FindFormatInfo(uint32_t format) {
	return format < kFormatInfoLookup.size() ? kFormatInfoLookup[format] : nullptr;
}

} // namespace

uint32_t NumBytesPerElement(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->bytes_per_element : 0;
}

uint32_t BlockCompressedBytesPerBlock(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->block_compressed_bytes_per_block : 0;
}

uint32_t RenderTargetBytesPerElement(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->render_target_bytes_per_element : 0;
}

bool IsSupportedTextureFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr && info->supported_sampled_texture_format;
}

bool IsUintTextureFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr && info->unsigned_integer_texture_format;
}

bool IsFmaskTextureFormat(uint32_t format) {
	return format == GpuEnumValue(BufferFormat::kFmask8_S4_F4);
}

bool IsBlockCompressedFormat(uint32_t format) {
	return BlockCompressedBytesPerBlock(format) != 0 && !IsASTCTextureFormat(format);
}

bool IsASTCTextureFormat(uint32_t format) {
	switch (format) {
		case GpuEnumValue(BufferFormat::kAstc4x4UNorm):
		case GpuEnumValue(BufferFormat::kAstc4x4Srgb):
		case GpuEnumValue(BufferFormat::kAstc5x4UNorm):
		case GpuEnumValue(BufferFormat::kAstc5x4Srgb):
		case GpuEnumValue(BufferFormat::kAstc5x5UNorm):
		case GpuEnumValue(BufferFormat::kAstc5x5Srgb):
		case GpuEnumValue(BufferFormat::kAstc6x5UNorm):
		case GpuEnumValue(BufferFormat::kAstc6x5Srgb):
		case GpuEnumValue(BufferFormat::kAstc6x6UNorm):
		case GpuEnumValue(BufferFormat::kAstc6x6Srgb):
		case GpuEnumValue(BufferFormat::kAstc8x5UNorm):
		case GpuEnumValue(BufferFormat::kAstc8x5Srgb):
		case GpuEnumValue(BufferFormat::kAstc8x6UNorm):
		case GpuEnumValue(BufferFormat::kAstc8x6Srgb):
		case GpuEnumValue(BufferFormat::kAstc8x8UNorm):
		case GpuEnumValue(BufferFormat::kAstc8x8Srgb):
		case GpuEnumValue(BufferFormat::kAstc10x5UNorm):
		case GpuEnumValue(BufferFormat::kAstc10x5Srgb):
		case GpuEnumValue(BufferFormat::kAstc10x6UNorm):
		case GpuEnumValue(BufferFormat::kAstc10x6Srgb):
		case GpuEnumValue(BufferFormat::kAstc10x8UNorm):
		case GpuEnumValue(BufferFormat::kAstc10x8Srgb):
		case GpuEnumValue(BufferFormat::kAstc10x10UNorm):
		case GpuEnumValue(BufferFormat::kAstc10x10Srgb):
		case GpuEnumValue(BufferFormat::kAstc12x10UNorm):
		case GpuEnumValue(BufferFormat::kAstc12x10Srgb):
		case GpuEnumValue(BufferFormat::kAstc12x12UNorm):
		case GpuEnumValue(BufferFormat::kAstc12x12Srgb):
			return true;
		default: return false;
	}
}

} // namespace Libs::Graphics::Prospero