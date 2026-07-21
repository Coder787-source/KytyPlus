#include "graphics/host_gpu/objects/textureCommon.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/transfer.h"
#include "graphics/host_gpu/vma.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cinttypes>
#include <cstring>

namespace Libs::Graphics {
namespace {

struct RenderTargetFormatMapping {
	Prospero::ChannelLayout layout;
	Prospero::ChannelType   type;
	Prospero::ChannelOrder  order;
	RenderTargetFormatInfo  info;
};

constexpr RenderTargetFormatMapping kRenderTargetFormats[] = {
    {Prospero::ChannelLayout::k8_8,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR8G8Unorm, 2}},
    {Prospero::ChannelLayout::k8_8_8_8,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR8G8B8A8Unorm, 4}},
    {Prospero::ChannelLayout::k8_8_8_8,
     Prospero::ChannelType::kSNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR8G8B8A8Snorm, 4}},
    {Prospero::ChannelLayout::k8_8_8_8,
     Prospero::ChannelType::kSrgb,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR8G8B8A8Srgb, 4}},
    {Prospero::ChannelLayout::k8_8_8_8,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kAlt,
     {vk::Format::eB8G8R8A8Unorm, 4}},
    {Prospero::ChannelLayout::k8_8_8_8,
     Prospero::ChannelType::kSNorm,
     Prospero::ChannelOrder::kAlt,
     {vk::Format::eB8G8R8A8Snorm, 4}},
    {Prospero::ChannelLayout::k8_8_8_8,
     Prospero::ChannelType::kSrgb,
     Prospero::ChannelOrder::kAlt,
     {vk::Format::eB8G8R8A8Srgb, 4}},
    {Prospero::ChannelLayout::k5_5_5_1,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR5G5B5A1UnormPack16, 2}},
    {Prospero::ChannelLayout::k4_4_4_4,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kReversed,
     {vk::Format::eB4G4R4A4UnormPack16, 2}},
    {Prospero::ChannelLayout::k10_10_10_2,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eA2B10G10R10UnormPack32, 4}},
    {Prospero::ChannelLayout::k10_10_10_2,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kAlt,
     {vk::Format::eA2R10G10B10UnormPack32, 4}},
    {Prospero::ChannelLayout::k11_11_10,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eB10G11R11UfloatPack32, 4}},
    {Prospero::ChannelLayout::k16,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16Unorm, 2}},
    {Prospero::ChannelLayout::k16,
     Prospero::ChannelType::kUInt,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16Uint, 2}},
    {Prospero::ChannelLayout::k16,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16Sfloat, 2}},
    {Prospero::ChannelLayout::k16_16,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16G16Unorm, 4}},
    {Prospero::ChannelLayout::k16_16,
     Prospero::ChannelType::kSNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16G16Snorm, 4}},
    {Prospero::ChannelLayout::k16_16,
     Prospero::ChannelType::kUInt,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16G16Uint, 4}},
    {Prospero::ChannelLayout::k16_16,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16G16Sfloat, 4}},
    {Prospero::ChannelLayout::k16_16_16_16,
     Prospero::ChannelType::kUNorm,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16G16B16A16Unorm, 8}},
    {Prospero::ChannelLayout::k16_16_16_16,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR16G16B16A16Sfloat, 8}},
    {Prospero::ChannelLayout::k16_16_16_16,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kAlt,
     {vk::Format::eR16G16B16A16Sfloat, 8, Prospero::ColorMappingBgra}},
    {Prospero::ChannelLayout::k16_16_16_16,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kReversed,
     {vk::Format::eR16G16B16A16Sfloat, 8, Prospero::ColorMappingAbgr}},
    {Prospero::ChannelLayout::k32,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR32Sfloat, 4}},
    {Prospero::ChannelLayout::k32_32,
     Prospero::ChannelType::kUInt,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR32G32Uint, 8}},
    {Prospero::ChannelLayout::k32_32,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR32G32Sfloat, 8}},
    {Prospero::ChannelLayout::k32_32_32_32,
     Prospero::ChannelType::kFloat,
     Prospero::ChannelOrder::kStandard,
     {vk::Format::eR32G32B32A32Sfloat, 16}},
};

} // namespace

// TODO: cleanup!
RenderTargetFormatInfo TextureGetRenderTargetFormat(uint32_t raw_layout, uint32_t raw_type,
                                                    uint32_t raw_order) {
	const auto layout = static_cast<Prospero::ChannelLayout>(raw_layout);
	const auto type   = static_cast<Prospero::ChannelType>(raw_type);
	const auto order  = static_cast<Prospero::ChannelOrder>(raw_order);

	if (layout == Prospero::ChannelLayout::k8 && type == Prospero::ChannelType::kUNorm &&
	    raw_order <= Prospero::GpuEnumValue(Prospero::ChannelOrder::kAltReversed)) {
		return {vk::Format::eR8Unorm, 1};
	}
	for (const auto& mapping: kRenderTargetFormats) {
		if (mapping.layout == layout && mapping.type == type && mapping.order == order) {
			return mapping.info;
		}
	}
	EXIT("unsupported render-target format combination: layout=%u type=%u order=%u\n", raw_layout,
	     raw_type, raw_order);
}

static uint64_t GetLevelSrcOffset(const TileSizeOffset& level_size) {
	return (level_size.src_size != 0 ? level_size.src_offset : level_size.offset);
}

static uint64_t GetLevelSrcSize(const TileSizeOffset& level_size) {
	return (level_size.src_size != 0 ? level_size.src_size : level_size.size);
}

static uint32_t GetTextureLevelDepth(uint32_t depth, uint32_t level, bool volume_texture) {
	return volume_texture ? std::max(depth >> level, 1u) : depth;
}

static size_t GetTextureRegionCount(uint32_t depth, uint64_t levels, bool volume_texture) {
	size_t count = 0;
	for (uint32_t level = 0; level < levels; level++) {
		count += GetTextureLevelDepth(depth, level, volume_texture);
	}
	return count;
}

uint64_t TextureUploadSliceSourceOffset(const TextureUploadLayout& layout, uint32_t level,
                                        uint32_t slice) {
	if (level >= 16 || layout.level_sizes[level].size == 0) {
		EXIT("invalid texture upload slice source, level=%u slice=%u\n", level, slice);
	}
	const auto level_offset = GetLevelSrcOffset(layout.level_sizes[level]);
	const auto slice_stride =
	    layout.source_slice_stride != 0 ? layout.source_slice_stride : layout.slice_stride;
	if (slice_stride != 0 && slice > (UINT64_MAX - level_offset) / slice_stride) {
		EXIT("texture upload slice source offset overflow, level=%u slice=%u\n", level, slice);
	}
	return level_offset + static_cast<uint64_t>(slice) * slice_stride;
}

uint64_t TextureCalcUploadSize(const TextureUploadLayout&          layout,
                               const std::vector<BufferImageCopy>& regions, uint64_t levels,
                               uint32_t depth) {
	uint64_t size = 0;

	for (const auto& r: regions) {
		size = std::max<uint64_t>(size, static_cast<uint64_t>(r.offset) +
		                                    layout.level_sizes[r.dst_level].size);
	}

	for (uint32_t level = 0; level < levels; level++) {
		const auto src_size  = GetLevelSrcSize(layout.level_sizes[level]);
		const auto mip_depth = GetTextureLevelDepth(depth, level, layout.volume_texture);
		for (uint32_t z = 0; z < mip_depth; z++) {
			size = std::max<uint64_t>(size,
			                          TextureUploadSliceSourceOffset(layout, level, z) + src_size);
		}
	}

	return size;
}

vk::ComponentSwizzle TextureGetComponentSwizzle(uint8_t s) {
	switch (static_cast<Prospero::CompSwizzle>(s)) {
		case Prospero::CompSwizzle::kZero: return vk::ComponentSwizzle::eZero;
		case Prospero::CompSwizzle::kOne: return vk::ComponentSwizzle::eOne;
		case Prospero::CompSwizzle::kRed: return vk::ComponentSwizzle::eR;
		case Prospero::CompSwizzle::kGreen: return vk::ComponentSwizzle::eG;
		case Prospero::CompSwizzle::kBlue: return vk::ComponentSwizzle::eB;
		case Prospero::CompSwizzle::kAlpha: return vk::ComponentSwizzle::eA;
		default: EXIT("unknown swizzle: %d\n", static_cast<int>(s));
	}
	return vk::ComponentSwizzle::eIdentity;
}

static uint32_t TextureGetDstSel(uint32_t swizzle, uint32_t channel) {
	return (swizzle >> (channel * 3u)) & 0x7u;
}

vk::ComponentMapping TextureGetComponentMapping(uint32_t swizzle) {
	vk::ComponentMapping components {};
	components.r = TextureGetComponentSwizzle(static_cast<uint8_t>(TextureGetDstSel(swizzle, 0)));
	components.g = TextureGetComponentSwizzle(static_cast<uint8_t>(TextureGetDstSel(swizzle, 1)));
	components.b = TextureGetComponentSwizzle(static_cast<uint8_t>(TextureGetDstSel(swizzle, 2)));
	components.a = TextureGetComponentSwizzle(static_cast<uint8_t>(TextureGetDstSel(swizzle, 3)));
	return components;
}

bool TextureCheckFormat(vk::ImageCreateInfo& image_info) {
	auto& graphics = GetRenderContext().GetGraphics();
	vk::ImageFormatProperties props {};
	if (graphics.GetImageFormatProperties(image_info.format, image_info.imageType,
	                                      image_info.tiling, image_info.usage, image_info.flags,
	                                      &props) == vk::Result::eErrorFormatNotSupported) {
		auto apply_fallback = [&](vk::Format replacement, const char* message) {
			image_info.format = replacement;
			const bool result = TextureCheckFormat(image_info);
			LOGF("%s [%s]\n", message, (!result ? "FAIL" : "SUCCESS"));
			return result;
		};

		if (image_info.format == vk::Format::eR8G8B8A8Srgb) {
			// TODO() convert SRGB -> LINEAR in shader
			return apply_fallback(
			    vk::Format::eR8G8B8A8Unorm,
			    "replace vk::Format::eR8G8B8A8Srgb => vk::Format::eR8G8B8A8Unorm");
		}
		if (image_info.format == vk::Format::eB8G8R8A8Srgb) {
			// TODO() convert SRGB -> LINEAR in shader
			return apply_fallback(
			    vk::Format::eB8G8R8A8Unorm,
			    "replace vk::Format::eB8G8R8A8Srgb => vk::Format::eB8G8R8A8Unorm");
		}
		return false;
	}
	return true;
}

static bool TextureCheckFormatExact(const vk::ImageCreateInfo& image_info) {
	auto& graphics = GetRenderContext().GetGraphics();
	vk::ImageFormatProperties props {};
	return graphics.GetImageFormatProperties(image_info.format, image_info.imageType,
	                                         image_info.tiling, image_info.usage, image_info.flags,
	                                         &props) != vk::Result::eErrorFormatNotSupported;
}

bool TextureCheckStorageSwizzle(vk::ImageCreateInfo& image_info, vk::ComponentMapping& components) {
	if (image_info.usage & vk::ImageUsageFlagBits::eStorage) {
		if (components.r == vk::ComponentSwizzle::eR && components.g == vk::ComponentSwizzle::eG &&
		    components.b == vk::ComponentSwizzle::eB && components.a == vk::ComponentSwizzle::eA) {
			return true;
		}

		if (components.r == vk::ComponentSwizzle::eB && components.g == vk::ComponentSwizzle::eG &&
		    components.b == vk::ComponentSwizzle::eR && components.a == vk::ComponentSwizzle::eA &&
		    image_info.format == vk::Format::eR8G8B8A8Srgb) {
			LOGF("replace vk::Format::eR8G8B8A8Srgb => vk::Format::eB8G8R8A8Srgb\n");

			components.r      = vk::ComponentSwizzle::eR;
			components.g      = vk::ComponentSwizzle::eG;
			components.b      = vk::ComponentSwizzle::eB;
			components.a      = vk::ComponentSwizzle::eA;
			image_info.format = vk::Format::eB8G8R8A8Srgb;
			return true;
		}

		// TODO() swizzle channels in shader

		return false;
	}
	return true;
}

vk::ImageUsageFlags TextureGetUsage(TextureFormatUsage usage) {
	vk::ImageUsageFlags vk_usage =
	    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
	if (TextureHasFormatUsage(usage, TextureFormatUsage::Sampled)) {
		vk_usage |= vk::ImageUsageFlagBits::eSampled;
	}
	if (TextureHasFormatUsage(usage, TextureFormatUsage::Storage)) {
		vk_usage |= vk::ImageUsageFlagBits::eStorage;
	}
	return vk_usage;
}

vk::ImageUsageFlags TextureGetViewUsage(TextureFormatUsage usage) {
	vk::ImageUsageFlags vk_usage = {};
	if (TextureHasFormatUsage(usage, TextureFormatUsage::Sampled)) {
		vk_usage |= vk::ImageUsageFlagBits::eSampled;
	}
	if (TextureHasFormatUsage(usage, TextureFormatUsage::Storage)) {
		vk_usage |= vk::ImageUsageFlagBits::eStorage;
	}
	return vk_usage;
}

vk::Format TextureGetFormat(uint32_t fmt) {
	const auto vk_format = VulkanFormat(fmt);
	if (vk_format != vk::Format::eUndefined) {
		return vk_format;
	}
	EXIT("unknown format: fmt = %u\n", fmt);
	return vk::Format::eUndefined;
}

static uint32_t AlignUpU32(uint32_t value, uint32_t alignment) {
	return (value + alignment - 1u) & ~(alignment - 1u);
}

uint32_t TextureGetAtlasSliceYStride(vk::Format format, uint32_t mip_height, uint32_t depth,
                                     uint64_t levels) {
	return (depth > 1 && levels > 1 && Transfer::IsBlockCompressedFormat(format)
	            ? AlignUpU32(mip_height, 4u)
	            : mip_height);
}

uint32_t TextureCalcStackedImageHeight(vk::Format format, uint32_t height, uint32_t depth,
                                       uint64_t levels) {
	auto image_height = height * depth;
	if (depth <= 1 || levels <= 1 || !Transfer::IsBlockCompressedFormat(format)) {
		return image_height;
	}

	uint32_t mip_height = height;
	for (uint32_t level = 0; level < levels; level++) {
		const auto mip_image_height =
		    TextureGetAtlasSliceYStride(format, mip_height, depth, levels) * depth;
		image_height = std::max<uint32_t>(image_height, mip_image_height << level);
		if (mip_height > 1) {
			mip_height /= 2;
		}
	}

	return image_height;
}

uint32_t TextureCalcMipmapAtlasImageHeight(vk::Format format, uint32_t width, uint32_t height,
                                           uint32_t depth, uint64_t levels) {
	auto image_height = height * depth;
	if (levels <= 1) {
		return image_height;
	}

	uint32_t mip_height = height;
	for (uint32_t level = 0; level < levels; level++) {
		const auto mipmap_offset = Transfer::MipmapAtlasOffset(level, width, height);
		const auto mip_bottom =
		    static_cast<uint32_t>(mipmap_offset.second) +
		    TextureGetAtlasSliceYStride(format, mip_height, depth, levels) * depth;
		image_height = std::max<uint32_t>(image_height, mip_bottom);
		if (mip_height > 1) {
			mip_height /= 2;
		}
	}

	return image_height;
}

bool TextureIs3DTexture(uint64_t type) {
	return static_cast<Prospero::ImageType>(type) == Prospero::ImageType::kColor3D;
}

bool TextureIsCubeTexture(uint64_t type) {
	return static_cast<Prospero::ImageType>(type) == Prospero::ImageType::kCube;
}

bool TextureIsLayeredTexture(uint64_t type) {
	const auto image_type = static_cast<Prospero::ImageType>(type);
	return image_type == Prospero::ImageType::kCube ||
	       image_type == Prospero::ImageType::kColor1DArray ||
	       image_type == Prospero::ImageType::kColor2DArray ||
	       image_type == Prospero::ImageType::kColor2DMsaaArray;
}

bool TextureCanCreateCubeView(uint64_t type, uint32_t base_array, uint32_t layer_count) {
	return TextureIsCubeTexture(type) && base_array % 6u == 0 && layer_count >= 6 &&
	       layer_count % 6u == 0;
}

vk::ComponentMapping TextureCreateImage(VulkanImage& vk_obj,
                                        const TextureImageCreateParams& params) {
	auto& graphics = GetRenderContext().GetGraphics();
	EXIT_IF(params.owner == nullptr);

	const bool array_texture  = TextureIsLayeredTexture(params.type);
	const bool volume_texture = TextureIs3DTexture(params.type);

	auto pixel_format = TextureGetFormat(params.fmt);
	EXIT_NOT_IMPLEMENTED(pixel_format == vk::Format::eUndefined);
	EXIT_NOT_IMPLEMENTED(params.width == 0);
	EXIT_NOT_IMPLEMENTED(params.height == 0);
	EXIT_NOT_IMPLEMENTED(params.depth == 0);
	EXIT_NOT_IMPLEMENTED(params.levels == 0 || params.levels > 16);

	uint32_t image_height = 0;
	uint32_t image_mips   = 0;
	if (params.image_layout == TextureUploadDestination::MipAtlas) {
		EXIT_NOT_IMPLEMENTED(params.base_level != 0);
		const auto atlas_depth = (array_texture || volume_texture ? 1u : params.depth);
		image_height           = TextureCalcMipmapAtlasImageHeight(
		    pixel_format, static_cast<uint32_t>(params.width), static_cast<uint32_t>(params.height),
		    atlas_depth, params.levels);
		image_mips = 1;
	} else {
		image_height =
		    (array_texture || volume_texture
		         ? static_cast<uint32_t>(params.height)
		         : TextureCalcStackedImageHeight(pixel_format, static_cast<uint32_t>(params.height),
		                                         params.depth, params.levels));
		image_mips = static_cast<uint32_t>(params.levels);
	}

	vk::ComponentMapping components =
	    TextureGetComponentMapping(static_cast<uint32_t>(params.swizzle));

	vk::ImageCreateInfo image_info {};
	image_info.sType = vk::StructureType::eImageCreateInfo;
	image_info.pNext = nullptr;
	image_info.flags =
	    (params.allow_cube_view && TextureCanCreateCubeView(params.type, 0, params.depth)
	         ? vk::ImageCreateFlagBits::eCubeCompatible
	         : vk::ImageCreateFlags {}) |
	    (volume_texture ? vk::ImageCreateFlagBits::e2DArrayCompatible : vk::ImageCreateFlags {}) |
	    (params.compatible_format_views ? vk::ImageCreateFlagBits::eMutableFormat
	                                    : vk::ImageCreateFlags {});
	image_info.imageType     = (volume_texture ? vk::ImageType::e3D : vk::ImageType::e2D);
	image_info.extent.width  = static_cast<uint32_t>(params.width);
	image_info.extent.height = image_height;
	image_info.extent.depth  = (volume_texture ? params.depth : 1);
	image_info.mipLevels     = image_mips;
	image_info.arrayLayers   = (array_texture ? params.depth : 1);
	image_info.format        = pixel_format;
	image_info.tiling        = vk::ImageTiling::eOptimal;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.usage         = TextureGetUsage(params.format_usage);
	image_info.sharingMode   = vk::SharingMode::eExclusive;
	image_info.samples       = vk::SampleCountFlagBits::e1;

	const bool storage_view = TextureHasFormatUsage(params.view_usage, TextureFormatUsage::Storage);
	if (storage_view && !TextureCheckStorageSwizzle(image_info, components)) {
		if (!params.storage_swizzle_fallback) {
			EXIT("swizzle is not supported");
		}

		static std::atomic_uint log_count {0};
		if (log_count.fetch_add(1, std::memory_order_relaxed) < 16) {
			LOGF("\t %s swizzle 0x%08" PRIx64 " is not supported, using identity mapping\n",
			     params.owner, params.swizzle);
		}
		components.r = vk::ComponentSwizzle::eR;
		components.g = vk::ComponentSwizzle::eG;
		components.b = vk::ComponentSwizzle::eB;
		components.a = vk::ComponentSwizzle::eA;
	}

	const auto view_checked_format = image_info.format;
	const auto requested_usage     = params.format_usage;
	const auto required_usage      = params.required_format_usage;
	const bool has_optional_usage  = static_cast<uint32_t>(requested_usage & ~required_usage) != 0;
	if (!TextureCheckFormatExact(image_info) && has_optional_usage) {
		auto required_info   = image_info;
		required_info.usage  = TextureGetUsage(required_usage);
		required_info.format = view_checked_format;
		if (TextureCheckFormatExact(required_info)) {
			static std::atomic_uint log_count {0};
			if (log_count.fetch_add(1, std::memory_order_relaxed) < 16) {
				LOGF("\t %s usage 0x%08x is not supported for format %d, using required "
				     "usage 0x%08x\n",
				     params.owner, static_cast<uint32_t>(TextureGetUsage(requested_usage)),
				     static_cast<int>(image_info.format),
				     static_cast<uint32_t>(TextureGetUsage(required_usage)));
			}
			image_info = required_info;
		}
	}

	if (!TextureCheckFormat(image_info)) {
		if (has_optional_usage) {
			image_info.format = view_checked_format;
			image_info.usage  = TextureGetUsage(required_usage);
		}
		if (!has_optional_usage || !TextureCheckFormat(image_info)) {
			EXIT("format is not supported");
		}
	}

	vk_obj.extent.width  = image_info.extent.width;
	vk_obj.extent.height = image_info.extent.height;
	vk_obj.layers        = image_info.arrayLayers;
	vk_obj.mip_levels    = image_info.mipLevels;
	vk_obj.format        = image_info.format;
	vk_obj.image         = nullptr;
	vk_obj.layout        = image_info.initialLayout;

	vk_obj.memory.property = vk::MemoryPropertyFlagBits::eDeviceLocal;

	bool created = graphics.CreateImage(image_info, vk_obj);
	EXIT_NOT_IMPLEMENTED(!created);

	return components;
}

void TextureCreateImageViews(VulkanImage& vk_obj,
                             vk::ComponentMapping components, uint64_t type, uint32_t base_array,
                             uint32_t base_level, uint32_t level_count, uint32_t depth,
                             bool allow_cube_view, TextureFormatUsage view_usage) {
	auto& graphics = GetRenderContext().GetGraphics();
	EXIT_IF(level_count == 0 || base_level + level_count > vk_obj.mip_levels);

	const bool layered_texture = TextureIsLayeredTexture(type);
	const bool volume_texture  = TextureIs3DTexture(type);
	const auto layer_count     = (layered_texture && base_array < depth ? depth - base_array : 1u);
	const auto volume_slices   = std::max(depth >> base_level, 1u);

	vk::ImageViewUsageCreateInfo usage_info {};
	usage_info.sType = vk::StructureType::eImageViewUsageCreateInfo;
	usage_info.pNext = nullptr;
	usage_info.usage = TextureGetViewUsage(view_usage);

	vk::ImageViewCreateInfo create_info {};
	create_info.sType                           = vk::StructureType::eImageViewCreateInfo;
	create_info.pNext                           = (usage_info.usage ? &usage_info : nullptr);
	create_info.flags                           = {};
	create_info.image                           = vk_obj.image;
	create_info.viewType                        = vk::ImageViewType::e2D;
	create_info.format                          = vk_obj.format;
	create_info.components                      = components;
	create_info.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
	create_info.subresourceRange.baseArrayLayer = (layered_texture ? base_array : 0);
	create_info.subresourceRange.baseMipLevel   = base_level;
	create_info.subresourceRange.layerCount     = layer_count;
	create_info.subresourceRange.levelCount     = level_count;
	if (volume_texture) {
		create_info.viewType                        = vk::ImageViewType::e3D;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount     = 1;
	} else if (allow_cube_view && TextureCanCreateCubeView(type, base_array, layer_count)) {
		create_info.viewType =
		    (layer_count > 6 ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube);
	} else if (layered_texture) {
		create_info.viewType = vk::ImageViewType::e2DArray;
	}

	auto create_view = [&](int index) {
		const auto result =
		    graphics.device.createImageView(&create_info, nullptr, &vk_obj.image_view[index]);
		if (result != vk::Result::eSuccess || vk_obj.image_view[index] == nullptr) {
			EXIT("failed to create texture image view: result=%d format=%d index=%d\n",
			     static_cast<int>(result), static_cast<int>(create_info.format), index);
		}
	};
	create_view(VulkanImage::VIEW_DEFAULT);

	create_info.viewType                    = vk::ImageViewType::e2DArray;
	create_info.subresourceRange.layerCount = volume_texture ? volume_slices : layer_count;
	create_view(VulkanImage::VIEW_DEFAULT_ARRAY);
}

static uint64_t CalcTextureSliceStride(const TileSizeOffset* level_sizes, uint64_t levels,
                                       uint64_t total_size, uint32_t depth) {
	uint64_t stride = 0;
	for (uint32_t i = 0; i < levels; i++) {
		stride =
		    std::max(stride, static_cast<uint64_t>(level_sizes[i].offset) + level_sizes[i].size);
	}

	if (depth > 1 && total_size != 0 && total_size % depth == 0) {
		const auto guest_stride = total_size / depth;
		if (guest_stride >= stride) {
			stride = guest_stride;
		}
	}

	return stride;
}

static uint64_t CalcLinearUploadLevelSize(uint32_t fmt, uint32_t pitch, uint32_t height) {
	if (const uint32_t bytes_per_element = Prospero::NumBytesPerElement(fmt);
	    bytes_per_element != 0) {
		return static_cast<uint64_t>(pitch) * height * bytes_per_element;
	}

	if (const uint32_t bytes_per_block = Prospero::BlockCompressedBytesPerBlock(fmt);
	    bytes_per_block != 0) {
		const uint32_t blocks_w = std::max((pitch + 3u) / 4u, 1u);
		const uint32_t blocks_h = std::max((height + 3u) / 4u, 1u);
		return static_cast<uint64_t>(blocks_w) * blocks_h * bytes_per_block;
	}

	return 0;
}

static uint64_t SetLinearUploadLevels(TileSizeOffset* level_sizes, uint32_t fmt, uint64_t height,
                                      uint64_t levels, uint32_t base_pitch) {
	uint64_t offset = 0;
	auto     pitch  = base_pitch;
	auto     h      = static_cast<uint32_t>(height);

	for (uint32_t i = 0; i < levels; i++) {
		const auto size = CalcLinearUploadLevelSize(fmt, pitch, h);
		EXIT_NOT_IMPLEMENTED(size > 0xffffffffull);
		EXIT_NOT_IMPLEMENTED(offset > 0xffffffffull);

		level_sizes[i].size       = static_cast<uint32_t>(size);
		level_sizes[i].offset     = static_cast<uint32_t>(offset);
		level_sizes[i].src_size   = 0;
		level_sizes[i].src_offset = 0;
		level_sizes[i].x          = 0;
		level_sizes[i].y          = 0;

		offset += size;
		if (pitch > 1) {
			pitch /= 2;
		}
		if (h > 1) {
			h /= 2;
		}
	}

	return offset;
}

TextureUploadLayout TextureCalcUploadLayout(uint32_t fmt, uint64_t width, uint64_t height,
                                            uint64_t levels, uint32_t depth, uint64_t pitch,
                                            uint64_t tile, uint64_t upload_size,
                                            bool allow_depth_tile, bool volume_texture,
                                            const char* owner) {
	TextureUploadLayout layout {};
	layout.tile           = static_cast<uint32_t>(tile);
	layout.pitch          = static_cast<uint32_t>(pitch);
	layout.volume_texture = volume_texture;

	if (fmt != 0) {
		if (layout.tile != 0) {
			const auto tile_mode = static_cast<Prospero::TileMode>(layout.tile);
			switch (tile_mode) {
				case Prospero::TileMode::kStandard256B:
					if (TileIsStandard256BTextureSupported(fmt)) {
						layout.tile_family = TileBlockFamily::Standard256B;
					}
					break;
				case Prospero::TileMode::kStandard4KB:
					if (TileIsStandard4KBTextureSupported(fmt)) {
						layout.tile_family = TileBlockFamily::Standard4KB;
					}
					break;
				case Prospero::TileMode::kStandard64KB:
					if (TileIsStandard64KBTextureSupported(fmt)) {
						layout.tile_family = TileBlockFamily::Standard64KB;
					}
					break;
				case Prospero::TileMode::kPrt:
					if (TileIsStandard64KBTextureSupported(fmt)) {
						layout.tile_family = TileBlockFamily::Prt64KB;
					}
					break;
				case Prospero::TileMode::kRenderTarget:
					if (Prospero::RenderTargetBytesPerElement(fmt) != 0) {
						layout.tile_family = TileBlockFamily::RenderTarget64KB;
					}
					break;
				case Prospero::TileMode::kDepth:
					if (allow_depth_tile && Prospero::RenderTargetBytesPerElement(fmt) != 0) {
						layout.tile_family = TileBlockFamily::Depth64KB;
					}
					break;
				default: break;
			}
			if (layout.tile_family == TileBlockFamily::Count) {
				EXIT("%s: unsupported typed tiled upload: fmt=%u tile=%u "
				     "size=%" PRIu64 " extent=%" PRIu64 "x%" PRIu64 " pitch=%" PRIu64
				     " levels=%" PRIu64 "\n",
				     owner, static_cast<uint32_t>(fmt), layout.tile, upload_size, width, height,
				     pitch, levels);
			}
		}

		layout.pitch = TileGetTexturePitch(fmt, width, levels, layout.tile);

		TileGetTextureSize(fmt, width, height, layout.pitch, levels, layout.tile, nullptr,
		                   layout.level_sizes, layout.padded_sizes);

		if (static_cast<Prospero::TileMode>(layout.tile) != Prospero::TileMode::kLinear) {
			if (layout.volume_texture) {
				layout.slice_stride = SetLinearUploadLevels(layout.level_sizes, fmt, height, levels,
				                                            static_cast<uint32_t>(width));
			} else {
				TileSizeOffset tiled_levels[16] {};
				std::copy_n(layout.level_sizes, levels, tiled_levels);
				layout.source_slice_stride =
				    CalcTextureSliceStride(tiled_levels, levels, upload_size, depth);
				SetLinearUploadLevels(layout.level_sizes, fmt, height, levels, layout.pitch);
				for (uint32_t i = 0; i < levels; ++i) {
					if (tiled_levels[i].src_size > tiled_levels[i].size) {
						layout.first_tail_level = std::min(layout.first_tail_level, i);
					}
					layout.level_sizes[i].src_offset = GetLevelSrcOffset(tiled_levels[i]);
					layout.level_sizes[i].src_size   = GetLevelSrcSize(tiled_levels[i]);
					layout.level_sizes[i].x          = tiled_levels[i].x;
					layout.level_sizes[i].y          = tiled_levels[i].y;
				}
			}
		} else if (layout.volume_texture) {
			layout.slice_stride =
			    CalcTextureSliceStride(layout.level_sizes, levels, upload_size, depth);
		}
	} else {
		EXIT("%s: legacy texture upload format unsupported: fmt=0 tile=%u size=%" PRIu64
		     " extent=%" PRIu64 "x%" PRIu64 " pitch=%" PRIu64 " levels=%" PRIu64 "\n",
		     owner, layout.tile, upload_size, width, height, pitch, levels);
	}

	if (!layout.volume_texture) {
		layout.slice_stride =
		    CalcTextureSliceStride(layout.level_sizes, levels, upload_size, depth);
	}
	return layout;
}

std::vector<BufferImageCopy> TextureBuildUploadRegions(const TextureUploadLayout& layout,
                                                       vk::Format image_format, uint32_t width,
                                                       uint32_t height, uint32_t depth,
                                                       uint64_t levels, bool array_texture,
                                                       bool                     volume_texture,
                                                       TextureUploadDestination destination) {
	uint32_t mip_width  = width;
	uint32_t mip_height = height;
	uint32_t mip_pitch  = volume_texture && static_cast<Prospero::TileMode>(layout.tile) !=
	                                            Prospero::TileMode::kLinear
	                          ? width
	                          : layout.pitch;

	std::vector<BufferImageCopy> regions;
	regions.reserve(GetTextureRegionCount(depth, levels, volume_texture));
	for (uint32_t i = 0; i < levels; i++) {
		EXIT_NOT_IMPLEMENTED(layout.level_sizes[i].size == 0);

		const auto mipmap_offset = Transfer::MipmapAtlasOffset(i, width, height);
		const auto mip_depth     = GetTextureLevelDepth(depth, i, volume_texture);

		for (uint32_t z = 0; z < mip_depth; z++) {
			const auto      slice_offset = z * layout.slice_stride;
			BufferImageCopy region {};

			region.offset = static_cast<uint32_t>(layout.level_sizes[i].offset + slice_offset);
			region.width  = mip_width;
			region.height = mip_height;
			region.copy_height =
			    (!array_texture && !volume_texture && depth > 1 && levels > 1 &&
			             Transfer::IsBlockCompressedFormat(image_format)
			         ? TextureGetAtlasSliceYStride(image_format, mip_height, depth, levels)
			         : 0);
			region.dst_layer = (array_texture ? z : 0);
			region.dst_z     = (volume_texture ? static_cast<int>(z) : 0);
			if (!layout.volume_texture &&
			    static_cast<Prospero::TileMode>(layout.tile) == Prospero::TileMode::kLinear &&
			    layout.padded_sizes[i].width != 0) {
				region.pitch = layout.padded_sizes[i].width;
			} else {
				region.pitch = mip_pitch;
			}

			if (destination == TextureUploadDestination::MipLevels) {
				region.dst_level = i;
				region.dst_x     = 0;
				region.dst_y =
				    (array_texture || volume_texture
				         ? 0
				         : static_cast<int>(z * TextureGetAtlasSliceYStride(
				                                    image_format, mip_height, depth, levels)));
			} else {
				region.dst_level = 0;
				region.dst_x     = mipmap_offset.first;
				region.dst_y =
				    (array_texture || volume_texture
				         ? mipmap_offset.second
				         : mipmap_offset.second +
				               static_cast<int>(z * TextureGetAtlasSliceYStride(
				                                        image_format, mip_height, depth, levels)));
			}
			regions.push_back(region);
		}

		if (mip_width > 1) {
			mip_width /= 2;
		}
		if (mip_height > 1) {
			mip_height /= 2;
		}
		if (mip_pitch > 1) {
			mip_pitch /= 2;
		}
	}

	return regions;
}

std::vector<ImageBufferCopy>
TextureBuildDownloadRegions(const std::vector<BufferImageCopy>& upload_regions) {
	std::vector<ImageBufferCopy> regions;
	regions.reserve(upload_regions.size());
	for (const auto& region: upload_regions) {
		regions.push_back({region.offset, region.pitch, region.dst_level, region.width,
		                   region.height, region.copy_height, region.dst_layer, region.dst_x,
		                   region.dst_y, region.dst_z, region.aspect});
	}
	return regions;
}

static uint64_t FmaskRegionCopySize(const BufferImageCopy& region) {
	const uint64_t row_length = (region.pitch != 0 ? region.pitch : region.width);
	if (region.height == 0 || region.width == 0) {
		return 0;
	}
	return ((static_cast<uint64_t>(region.height - 1u) * row_length) + region.width) *
	       sizeof(uint32_t);
}

static void UploadFmaskIdentity(VulkanImage& vk_obj,
                                const std::vector<BufferImageCopy>& regions,
                                vk::ImageLayout dst_layout, const char* owner) {
	constexpr uint32_t kIdentityFmaskPattern = 0x76543210u;

	std::vector<BufferImageCopy> upload_regions = regions;
	uint64_t                     upload_size    = 0;
	for (auto& region: upload_regions) {
		upload_size = (upload_size + 255u) & ~uint64_t {255};
		EXIT_NOT_IMPLEMENTED(upload_size > UINT32_MAX);
		region.offset = static_cast<uint32_t>(upload_size);
		upload_size += FmaskRegionCopySize(region);
	}

	LOGF("%s: temporary: decoding PS5 FMASK8_S4_F4 as identity pattern 0x%08" PRIx32
	     ", upload_size=%" PRIu64 " regions=%zu\n",
	     owner, kIdentityFmaskPattern, upload_size, upload_regions.size());

	Transfer::ScratchBuffer temp_buf(upload_size);
	auto*                   words = static_cast<uint32_t*>(temp_buf.Data());
	for (uint64_t i = 0; i < upload_size / sizeof(uint32_t); i++) {
		words[i] = kIdentityFmaskPattern;
	}

	Transfer::UploadImage(vk_obj, temp_buf.Data(), upload_size, upload_regions,
	                      dst_layout);
}

struct GpuTileElementLayout {
	uint32_t bytes = 0;
	uint32_t wide  = 1;
	uint32_t tall  = 1;
};

static bool GetGpuTileElementLayout(uint32_t fmt, GpuTileElementLayout& out) {
	if (const auto bytes = Prospero::NumBytesPerElement(fmt); bytes != 0) {
		out = {bytes, 1, 1};
		return true;
	}
	if (const auto bytes = Prospero::BlockCompressedBytesPerBlock(fmt); bytes != 0) {
		out = {bytes, 4, 4};
		return true;
	}
	return false;
}

static bool SetGpuTileSize(uint64_t offset, uint64_t length, uint64_t capacity, uint64_t& size) {
	if (offset > capacity || length > capacity - offset) {
		return false;
	}
	size = length;
	return true;
}

bool TextureBuildGpuTileInfos(uint64_t size, const std::vector<BufferImageCopy>& regions,
                              const TextureUploadLayout& layout, uint32_t fmt, uint32_t depth,
                              uint64_t levels, std::vector<GpuTileInfo>& out_infos) {
	if (size == 0 || levels == 0 || levels > 16 || depth == 0 ||
	    regions.size() != GetTextureRegionCount(depth, levels, layout.volume_texture) ||
	    Prospero::IsFmaskTextureFormat(fmt)) {
		return false;
	}

	GpuTileElementLayout element {};
	if (layout.tile_family == TileBlockFamily::RenderTarget64KB ||
	    layout.tile_family == TileBlockFamily::Depth64KB) {
		element.bytes = Prospero::RenderTargetBytesPerElement(fmt);
	} else if (!GetGpuTileElementLayout(fmt, element)) {
		return false;
	}
	if (element.bytes == 0) {
		return false;
	}

	std::vector<GpuTileInfo> infos;
	infos.reserve(regions.size());
	if (layout.volume_texture) {
		TileVolumeLayout volume {};
		if (!TileGetTextureVolumeLayout(fmt, regions[0].width, regions[0].height, depth,
		                                static_cast<uint32_t>(levels), layout.tile, volume)) {
			return false;
		}
		element = {volume.bytes_per_element, volume.texel_width, volume.texel_height};
		TileBlockLayout block {};
		if (!TileGetBlockLayout(volume.family, element.bytes, block)) return false;

		size_t region_base = 0;
		for (uint32_t level = 0; level < levels; ++level) {
			const uint32_t mip_depth     = GetTextureLevelDepth(depth, level, true);
			const bool     tail          = level >= volume.first_tail_level;
			const uint64_t linear_stride = layout.slice_stride;
			for (uint32_t z = 0; z < mip_depth; z += block.block_depth) {
				const uint32_t copy_depth = std::min(block.block_depth, mip_depth - z);
				const auto&    region     = regions[region_base + z];
				GpuTileInfo    info {};
				info.family            = block.family;
				info.bytes_per_element = block.bytes_per_element;
				info.linear_offset     = region.offset;
				info.tiled_offset =
				    static_cast<uint64_t>(z / block.block_depth) * volume.block_slice_size +
				    volume.level_offsets[level];
				const uint64_t linear_span =
				    static_cast<uint64_t>(copy_depth - 1u) * linear_stride +
				    layout.level_sizes[level].size;
				if (!SetGpuTileSize(info.linear_offset, linear_span, size, info.linear_size) ||
				    !SetGpuTileSize(info.tiled_offset, volume.level_sizes[level], size,
				                    info.tiled_size)) {
					return false;
				}
				info.linear_slice_stride = linear_stride;
				info.width       = std::max((region.width + element.wide - 1u) / element.wide, 1u);
				info.height      = std::max((region.height + element.tall - 1u) / element.tall, 1u);
				info.depth       = copy_depth;
				info.surface_z   = block.block_depth == 1 ? static_cast<uint32_t>(region.dst_z) : 0;
				info.pitch       = std::max((region.pitch + element.wide - 1u) / element.wide, 1u);
				info.tail_x      = tail ? volume.tail_x[level] : 0;
				info.tail_y      = tail ? volume.tail_y[level] : 0;
				info.tail        = tail;
				info.tiled_width = volume.level_widths[level];
				info.tiled_height = volume.level_heights[level];
				infos.push_back(info);
			}
			region_base += mip_depth;
		}
	} else {
		const auto base_family = layout.tile_family;
		if (base_family == TileBlockFamily::Count) {
			return false;
		}

		size_t region_index = 0;
		for (uint32_t level = 0; level < levels; level++) {
			const auto&     level_size = layout.level_sizes[level];
			const bool      tail       = level >= layout.first_tail_level;
			const auto      family     = base_family;
			TileBlockLayout block {};
			if (!TileGetBlockLayout(family, element.bytes, block)) {
				return false;
			}
			const auto level_depth = GetTextureLevelDepth(depth, level, layout.volume_texture);
			for (uint32_t z = 0; z < level_depth; z++) {
				const auto& region = regions[region_index++];
				GpuTileInfo info {};
				info.family            = block.family;
				info.bytes_per_element = block.bytes_per_element;
				info.linear_offset     = region.offset;
				info.tiled_offset      = TextureUploadSliceSourceOffset(layout, level, z);
				if (!SetGpuTileSize(info.linear_offset, level_size.size, size, info.linear_size) ||
				    !SetGpuTileSize(info.tiled_offset, GetLevelSrcSize(level_size), size,
				                    info.tiled_size)) {
					return false;
				}
				info.width     = std::max((region.width + element.wide - 1u) / element.wide, 1u);
				info.height    = std::max((region.height + element.tall - 1u) / element.tall, 1u);
				info.surface_z = base_family == TileBlockFamily::RenderTarget64KB ||
				                         base_family == TileBlockFamily::Depth64KB
				                     ? region.dst_layer
				                     : 0;
				info.pitch     = std::max((region.pitch + element.wide - 1u) / element.wide, 1u);
				info.tail      = tail;
				info.tail_x    = tail ? level_size.x : 0;
				info.tail_y    = tail ? level_size.y : 0;
				info.tiled_width =
				    layout.padded_sizes[level].width != 0
				        ? std::max((layout.padded_sizes[level].width + element.wide - 1u) /
				                       element.wide,
				                   1u)
				        : info.pitch;
				info.tiled_height =
				    layout.padded_sizes[level].height != 0
				        ? std::max((layout.padded_sizes[level].height + element.tall - 1u) /
				                       element.tall,
				                   1u)
				        : info.height;
				infos.push_back(info);
			}
		}
	}

	if (infos.empty()) {
		return false;
	}
	out_infos = std::move(infos);
	return true;
}

void TextureUploadGuestImage(VulkanImage& vk_obj, const void* src_data,
                             uint64_t size, const std::vector<BufferImageCopy>& regions,
                             const TextureUploadLayout& layout, uint32_t fmt, uint64_t width,
                             uint64_t height, uint32_t depth, uint64_t levels, const char* owner,
                             vk::ImageLayout dst_layout) {
	if (fmt == 0) {
		EXIT("%s: texture upload format unsupported: fmt=0 tile=%u size=%" PRIu64 " extent=%" PRIu64
		     "x%" PRIu64 " depth=%u pitch=%u levels=%" PRIu64 "\n",
		     owner, layout.tile, size, width, height, depth, layout.pitch, levels);
	}
	if (static_cast<Prospero::TileMode>(layout.tile) == Prospero::TileMode::kLinear) {
		Transfer::UploadImage(vk_obj, src_data, size, regions, dst_layout);
		return;
	}
	if (layout.tile_family == TileBlockFamily::Depth64KB && Prospero::IsFmaskTextureFormat(fmt)) {
		UploadFmaskIdentity(vk_obj, regions, dst_layout, owner);
		return;
	}

	std::vector<GpuTileInfo> infos;
	if (!TextureBuildGpuTileInfos(size, regions, layout, fmt, depth, levels, infos)) {
		EXIT("%s: GPU tiled upload unsupported: fmt=%u tile=%u size=%" PRIu64 " extent=%" PRIu64
		     "x%" PRIu64 " depth=%u pitch=%u levels=%" PRIu64 "\n",
		     owner, fmt, layout.tile, size, width, height, depth, layout.pitch, levels);
	}
	Transfer::UploadTiledImage(vk_obj, src_data, size, size, infos, regions, dst_layout);
}

} // namespace Libs::Graphics
