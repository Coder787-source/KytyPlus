#include "graphics/host_gpu/renderer/image.h"

#include "common/assert.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/tile.h"
#include "graphics/host_gpu/gpuTiler.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/objects/textureCommon.h"
#include "graphics/host_gpu/regionDefinitions.h"
#include "graphics/host_gpu/renderer/framebufferCache.h"
#include "graphics/host_gpu/renderer/imageView.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/renderer/renderTarget.h"
#include "graphics/host_gpu/transfer.h"
#include "graphics/host_gpu/vma.h"
#include "graphics/shader/shader.h"

#include <algorithm>

namespace Libs::Graphics {

namespace {

TextureImageCreateParams MakeImageParams(const ImageInfo& info, bool storage) {
	TextureImageCreateParams params {};
	params.fmt        = info.format;
	params.width      = info.width;
	params.height     = info.height;
	params.base_level = SelectImageBackingBaseLevel(storage, info.base_level);
	params.levels     = info.levels;
	params.depth      = info.depth;
	params.type       = info.type;
	// Storage image views use identity component mapping. The guest storage write mapping is
	// validated before this point and intentionally does not become a Vulkan view swizzle.
	params.swizzle               = storage ? DstSel(4, 5, 6, 7) : info.swizzle;
	params.format_usage          = TextureFormatUsage::Sampled | TextureFormatUsage::Storage;
	params.required_format_usage = storage
	                                   ? TextureFormatUsage::Sampled | TextureFormatUsage::Storage
	                                   : TextureFormatUsage::Sampled;
	params.view_usage      = storage ? TextureFormatUsage::Sampled | TextureFormatUsage::Storage
	                                 : TextureFormatUsage::Sampled;
	params.image_layout    = TextureUploadDestination::MipLevels;
	params.allow_cube_view = !storage;
	params.compatible_format_views =
	    storage && (IsRgba8SrgbViewFormat(TextureGetFormat(info.format)) ||
	                info.format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32UInt) ||
	                info.format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32Float));
	params.owner = storage ? "StorageTextureCache" : "TextureCache";
	return params;
}

bool RenderTargetSupportsStorage(vk::Format format, vk::ImageCreateFlags flags) {
	const auto compatible = SrgbStorageViewFormat(format);
	const auto required_flags =
	    vk::ImageCreateFlagBits::eMutableFormat | vk::ImageCreateFlagBits::eExtendedUsage;
	const bool compatible_views = (flags & required_flags) == required_flags;
	return ImageViewOps::FormatSupportsStorage(format) ||
	       (compatible_views && compatible != vk::Format::eUndefined &&
	        ImageViewOps::FormatSupportsStorage(compatible));
}

vk::ImageCreateFlags RenderTargetCreateFlags(vk::Format format) {
	const bool compatible_format_view =
	    IsRgba8SrgbViewFormat(format) ||
	    BgraToRgbaSampledViewFormat(format) != vk::Format::eUndefined ||
	    format == vk::Format::eR8G8B8A8Uint || format == vk::Format::eR16G16B16A16Sfloat ||
	    format == vk::Format::eR16G16B16A16Uint;
	return compatible_format_view
	           ? vk::ImageCreateFlagBits::eMutableFormat | vk::ImageCreateFlagBits::eExtendedUsage
	           : vk::ImageCreateFlags {0};
}

vk::ImageUsageFlags RenderTargetUsage(vk::Format format, vk::ImageCreateFlags flags,
                                      uint32_t samples) {
	auto& graphics = GetRenderContext().GetGraphics();
	auto  usage    = static_cast<vk::ImageUsageFlags>(vk::ImageUsageFlagBits::eColorAttachment) |
	                 static_cast<vk::ImageUsageFlags>(vk::ImageUsageFlagBits::eTransferSrc) |
	                 static_cast<vk::ImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst);
	if (samples == 1) {
		usage |= vk::ImageUsageFlagBits::eSampled;
		if (RenderTargetSupportsStorage(format, flags)) {
			usage |= vk::ImageUsageFlagBits::eStorage;
		}
	}
	vk::ImageFormatProperties properties {};
	if (graphics.GetImageFormatProperties(format, vk::ImageType::e2D, vk::ImageTiling::eOptimal,
	                                      usage, flags, &properties) != vk::Result::eSuccess ||
	    !static_cast<bool>(properties.sampleCounts & vulkan_sample_count(samples))) {
		EXIT("TextureCache: render-target format does not support required usage, format=%d "
		     "usage=0x%x samples=%u supported=0x%x\n",
		     static_cast<int>(format), static_cast<vk::ImageUsageFlags::MaskType>(usage), samples,
		     static_cast<vk::SampleCountFlags::MaskType>(properties.sampleCounts));
	}
	return usage;
}

[[nodiscard]] uint32_t RenderTargetTransferFormatImpl(uint32_t bytes_per_element) {
	switch (bytes_per_element) {
		case 1: return Prospero::GpuEnumValue(Prospero::BufferFormat::k8UNorm);
		case 2: return Prospero::GpuEnumValue(Prospero::BufferFormat::k16UNorm);
		case 4: return Prospero::GpuEnumValue(Prospero::BufferFormat::k32Float);
		case 8: return Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16_16_16Float);
		case 16: return Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32_32_32Float);
		default:
			EXIT("TextureCache: unsupported render-target element size: %u\n", bytes_per_element);
	}
}

static constexpr uint32_t DummyTextureSwizzle() {
	return Prospero::GpuEnumValue(Prospero::CompSwizzle::kRed) |
	       (Prospero::GpuEnumValue(Prospero::CompSwizzle::kGreen) << 3u) |
	       (Prospero::GpuEnumValue(Prospero::CompSwizzle::kBlue) << 6u) |
	       (Prospero::GpuEnumValue(Prospero::CompSwizzle::kAlpha) << 9u);
}

TextureImageCreateParams MakeDummyTextureParams(bool uint_format, bool image_3d,
                                                TextureFormatUsage usage, const char* owner) {
	TextureImageCreateParams params {};
	params.fmt = static_cast<uint32_t>(
	    Prospero::GpuEnumValue(uint_format ? Prospero::BufferFormat::k8_8_8_8UInt
	                                       : Prospero::BufferFormat::k8_8_8_8UNorm));
	params.width                 = 1;
	params.height                = 1;
	params.base_level            = 0;
	params.levels                = 1;
	params.depth                 = 1;
	params.type                  = Prospero::GpuEnumValue(image_3d ? Prospero::ImageType::kColor3D
	                                                               : Prospero::ImageType::kColor2D);
	params.swizzle               = DummyTextureSwizzle();
	params.format_usage          = usage;
	params.required_format_usage = usage;
	params.view_usage            = usage;
	params.image_layout          = TextureUploadDestination::MipLevels;
	params.allow_cube_view       = true;
	params.storage_swizzle_fallback = TextureHasFormatUsage(usage, TextureFormatUsage::Storage);
	params.owner                    = owner;
	return params;
}

} // namespace

namespace ImageOps {

uint32_t RenderTargetTransferFormat(uint32_t bytes_per_element) {
	return RenderTargetTransferFormatImpl(bytes_per_element);
}

GpuTextureVulkanImage* CreateTexture(const ImageInfo& info, bool storage,
                                     vk::ComponentMapping& components) {
	auto* image = storage ? static_cast<GpuTextureVulkanImage*>(new StorageTextureVulkanImage)
	                      : new TextureVulkanImage;
	components  = TextureCreateImage(*image, MakeImageParams(info, storage));
	return image;
}

void CreateTextureViews(GpuTextureVulkanImage& image, const ImageInfo& info, bool storage,
                        vk::ComponentMapping components) {
	if (storage) {
		TextureCreateImageViews(image, components, info.type, 0, 0, 1, info.depth, false,
		                        TextureFormatUsage::Sampled | TextureFormatUsage::Storage);
	} else {
		TextureCreateImageViews(image, components, info.type, info.base_array, info.base_level,
		                        info.view_levels, info.depth, true, TextureFormatUsage::Sampled);
	}
}

void UploadRenderTargetLayers(RenderTextureVulkanImage& image, const RenderTargetInfo& info,
                              uint32_t base_layer, uint32_t layer_count, bool refresh) {
	if (info.layers == 0 || info.size % info.layers != 0 || layer_count == 0 ||
	    base_layer >= info.layers || layer_count > info.layers - base_layer ||
	    base_layer >= image.layers || layer_count > image.layers - base_layer) {
		EXIT("TextureCache: invalid render-target layer upload, base=%u count=%u "
		     "info_layers=%u image_layers=%u size=0x%016" PRIx64 "\n",
		     base_layer, layer_count, info.layers, image.layers, info.size);
	}
	if (info.samples != 1 || image.samples != 1) {
		EXIT("TextureCache: multisampled render-target upload is unsupported, samples=%u/%u\n",
		     info.samples, image.samples);
	}
	if (refresh) {
		Transfer::WaitForQueueIdle();
	}
	const auto slice_size  = info.size / info.layers;
	const auto upload_size = slice_size * layer_count;
	const bool standard64  = IsSupportedStandard64RenderTarget(info);
	if (standard64 || info.levels > 1 || info.layers > 1) {
		const auto format = RenderTargetTransferFormat(info.bytes_per_element);
		auto layout = TextureCalcUploadLayout(format, info.width, info.height, info.levels,
		                                      layer_count, info.pitch, info.tile_mode, upload_size,
		                                      false, false, "TextureCache render target");
		const bool render_target_tiled =
		    info.tile_mode == Prospero::GpuEnumValue(Prospero::TileMode::kRenderTarget);
		if (!standard64 &&
		    ((render_target_tiled && layout.tile_family != TileBlockFamily::RenderTarget64KB) ||
		     layout.pitch != info.pitch)) {
			EXIT("TextureCache: unsupported render-target mip upload layout, pitch=%u/%u tile=%u\n",
			     info.pitch, layout.pitch, info.tile_mode);
		}
		auto regions = TextureBuildUploadRegions(layout, info.format, info.width, info.height,
		                                         layer_count, info.levels, true, false,
		                                         TextureUploadDestination::MipLevels);
		for (auto& region: regions) {
			region.dst_layer += base_layer;
		}
		const auto source_address = info.address + slice_size * base_layer;
		TextureUploadGuestImage(image, reinterpret_cast<const void*>(source_address), upload_size,
		                        regions, layout, format, info.width, info.height, layer_count,
		                        info.levels, "TextureCache render target",
		                        vk::ImageLayout::eGeneral);
		return;
	}
	if (info.tile_mode == Prospero::GpuEnumValue(Prospero::TileMode::kRenderTarget) &&
	    Transfer::GuestBufferIsTiled(info.address, slice_size)) {
		const auto format = RenderTargetTransferFormat(info.bytes_per_element);
		auto layout  = TextureCalcUploadLayout(format, info.width, info.height, 1, 1, info.pitch,
		                                       info.tile_mode, slice_size, false, false,
		                                       "TextureCache render target");
		auto regions = TextureBuildUploadRegions(layout, info.format, info.width, info.height, 1, 1,
		                                         true, false, TextureUploadDestination::MipLevels);
		TextureUploadGuestImage(image, reinterpret_cast<const void*>(info.address), slice_size,
		                        regions, layout, format, info.width, info.height, 1, 1,
		                        "TextureCache render target", vk::ImageLayout::eGeneral);
	} else {
		Transfer::UploadImage(image, reinterpret_cast<const void*>(info.address), slice_size,
		                      info.pitch, vk::ImageLayout::eGeneral);
	}
}

void UploadRenderTarget(RenderTextureVulkanImage& image, const RenderTargetInfo& info,
                        bool refresh) {
	UploadRenderTargetLayers(image, info, 0, info.layers, refresh);
}

RenderTextureVulkanImage* CreateRenderTarget(const RenderTargetInfo& info) {
	auto& graphics       = GetRenderContext().GetGraphics();
	auto* image          = new RenderTextureVulkanImage;
	image->extent.width  = info.width;
	image->extent.height = info.height;
	image->format        = info.format;
	image->mip_levels    = info.levels;
	image->layers        = info.layers;
	image->samples       = info.samples;
	image->layout        = vk::ImageLayout::eUndefined;
	vk::ImageCreateInfo create {};
	create.sType           = vk::StructureType::eImageCreateInfo;
	create.flags           = RenderTargetCreateFlags(info.format);
	create.imageType       = vk::ImageType::e2D;
	create.extent          = {info.width, info.height, 1};
	create.mipLevels       = info.levels;
	create.arrayLayers     = info.layers;
	create.format          = info.format;
	create.tiling          = vk::ImageTiling::eOptimal;
	create.initialLayout   = vk::ImageLayout::eUndefined;
	create.usage           = RenderTargetUsage(info.format, create.flags, info.samples);
	create.sharingMode     = vk::SharingMode::eExclusive;
	create.samples         = vulkan_sample_count(info.samples);
	image->memory.property = vk::MemoryPropertyFlagBits::eDeviceLocal;
	if (!graphics.CreateImage(create, *image)) {
		EXIT("TextureCache: failed to create render target, addr=0x%016" PRIx64
		     " extent=%ux%u format=%d\n",
		     info.address, info.width, info.height, static_cast<int>(info.format));
	}
	ImageViewOps::CreateRenderTargetViews(*image);
	return image;
}

DepthStencilVulkanImage* CreateDepthTarget(const DepthTargetInfo& info) {
	auto&               graphics = GetRenderContext().GetGraphics();
	vk::ImageCreateInfo create {};
	create.sType         = vk::StructureType::eImageCreateInfo;
	create.imageType     = vk::ImageType::e2D;
	create.extent        = {info.width, info.height, 1};
	create.mipLevels     = 1;
	create.arrayLayers   = info.layers;
	create.format        = info.format;
	create.tiling        = vk::ImageTiling::eOptimal;
	create.initialLayout = vk::ImageLayout::eUndefined;
	create.usage         = DepthTargetImageUsage();
	create.sharingMode   = vk::SharingMode::eExclusive;
	create.samples       = vulkan_sample_count(info.samples);
	vk::ImageFormatProperties properties {};
	if (graphics.GetImageFormatProperties(
	        info.format, vk::ImageType::e2D, vk::ImageTiling::eOptimal, create.usage,
	        vk::ImageCreateFlags {}, &properties) != vk::Result::eSuccess ||
	    !static_cast<bool>(properties.sampleCounts & create.samples)) {
		EXIT("TextureCache: depth format does not support required usage, format=%d usage=0x%x "
		     "samples=%u supported=0x%x\n",
		     static_cast<int>(info.format),
		     static_cast<vk::ImageUsageFlags::MaskType>(create.usage), info.samples,
		     static_cast<vk::SampleCountFlags::MaskType>(properties.sampleCounts));
	}
	auto* image            = new DepthStencilVulkanImage;
	image->extent.width    = info.width;
	image->extent.height   = info.height;
	image->guest_pitch     = info.pitch;
	image->layers          = info.layers;
	image->samples         = info.samples;
	image->format          = info.format;
	image->layout          = vk::ImageLayout::eUndefined;
	image->compressed      = false;
	image->memory.property = vk::MemoryPropertyFlagBits::eDeviceLocal;
	if (!graphics.CreateImage(create, *image)) {
		EXIT("TextureCache: failed to create depth target, addr=0x%016" PRIx64
		     " extent=%ux%u format=%d\n",
		     info.address, info.width, info.height, static_cast<int>(info.format));
	}
	ImageViewOps::CreateDepthViews(*image);
	return image;
}

void ValidateVideoOut(const VideoOutInfo& info) {
	const auto compression =
	    ClassifyVideoOutCompression(info.compression != VideoOutCompression::Uncompressed,
	                                info.metadata_address, info.dcc_control, 0);
	const bool metadata_invalid = compression != VideoOutCompression::Uncompressed &&
	                              compression != VideoOutCompression::Unsupported &&
	                              (info.metadata_address >= TRACKER_ADDRESS_SIZE ||
	                               (info.metadata_address >= info.address &&
	                                info.metadata_address < info.address + info.size));
	if (info.address == 0 || info.size == 0 || info.address >= TRACKER_ADDRESS_SIZE ||
	    info.size > TRACKER_ADDRESS_SIZE - info.address || (info.address & 0xffffu) != 0 ||
	    info.width == 0 || info.height == 0 || info.width > 16384 || info.height > 16384 ||
	    info.pitch < info.width ||
	    info.tile_mode != Prospero::GpuEnumValue(Prospero::TileMode::kRenderTarget) ||
	    compression == VideoOutCompression::Unsupported || compression != info.compression ||
	    metadata_invalid || !IsSupportedVideoOutFormat(info)) {
		EXIT("TextureCache: unsupported video-out surface, addr=0x%016" PRIx64 " size=0x%016" PRIx64
		     " metadata=0x%016" PRIx64 " dcc=0x%08" PRIx32
		     " extent=%ux%u pitch=%u tile=%u guest_format=%u bpe=%u vk_format=%d\n",
		     info.address, info.size, info.metadata_address, info.dcc_control, info.width,
		     info.height, info.pitch, info.tile_mode, info.guest_format, info.bytes_per_element,
		     static_cast<int>(info.format));
	}
	TileSizeAlign exact {};
	TileGetTextureTotalSize(info.guest_format, info.width, info.height, 1, info.pitch, 1,
	                        info.tile_mode, false, exact);
	if (exact.align != 65536 || exact.size != info.size ||
	    TileGetTexturePitch(info.guest_format, info.width, 1, info.tile_mode) != info.pitch) {
		EXIT("TextureCache: video-out tile layout mismatch, addr=0x%016" PRIx64
		     " size=0x%016" PRIx64 " expected_size=0x%016" PRIx64 " align=0x%016" PRIx64
		     " pitch=%u\n",
		     info.address, info.size, exact.size, exact.align, info.pitch);
	}
	(void)RenderTargetUsage(info.format, vk::ImageCreateFlags {}, 1);
}

VideoOutVulkanImage* CreateVideoOut(const VideoOutInfo& info) {
	auto& graphics       = GetRenderContext().GetGraphics();
	auto* image          = new VideoOutVulkanImage;
	image->extent.width  = info.width;
	image->extent.height = info.height;
	image->format        = info.format;
	image->layout        = vk::ImageLayout::eUndefined;
	vk::ImageCreateInfo create {};
	create.sType           = vk::StructureType::eImageCreateInfo;
	create.imageType       = vk::ImageType::e2D;
	create.extent          = {info.width, info.height, 1};
	create.mipLevels       = 1;
	create.arrayLayers     = 1;
	create.format          = info.format;
	create.tiling          = vk::ImageTiling::eOptimal;
	create.initialLayout   = vk::ImageLayout::eUndefined;
	create.flags           = RenderTargetCreateFlags(info.format);
	create.usage           = RenderTargetUsage(info.format, create.flags, 1);
	create.sharingMode     = vk::SharingMode::eExclusive;
	create.samples         = vk::SampleCountFlagBits::e1;
	image->memory.property = vk::MemoryPropertyFlagBits::eDeviceLocal;
	if (!graphics.CreateImage(create, *image)) {
		EXIT("TextureCache: failed to create video-out image, addr=0x%016" PRIx64
		     " extent=%ux%u format=%d\n",
		     info.address, info.width, info.height, static_cast<int>(info.format));
	}
	ImageViewOps::CreateVideoOutViews(*image);
	return image;
}

void UploadVideoOut(VideoOutVulkanImage& image, const VideoOutInfo& info, bool refresh) {
	if (info.compression != VideoOutCompression::Uncompressed) {
		EXIT("TextureCache: compressed video-out guest upload is unsupported, "
		     "addr=0x%016" PRIx64 " metadata=0x%016" PRIx64 " dcc=0x%08" PRIx32 "\n",
		     info.address, info.metadata_address, info.dcc_control);
	}
	if (refresh) {
		Transfer::WaitForQueueIdle();
	}
	image.layout = vk::ImageLayout::eUndefined;
	if (!info.bgra16) {
		auto layout =
		    TextureCalcUploadLayout(info.guest_format, info.width, info.height, 1, 1, info.pitch,
		                            info.tile_mode, info.size, false, false, "VideoOut");
		auto regions = TextureBuildUploadRegions(layout, info.format, info.width, info.height, 1, 1,
		                                         false, false, TextureUploadDestination::MipLevels);
		TextureUploadGuestImage(image, reinterpret_cast<const void*>(info.address), info.size,
		                        regions, layout, info.guest_format, info.width, info.height, 1, 1,
		                        "VideoOut", vk::ImageLayout::eGeneral);
		return;
	}
	Transfer::ScratchBuffer scratch(info.size);
	TileBlockLayout         block {};
	EXIT_NOT_IMPLEMENTED(
	    !TileGetBlockLayout(TileBlockFamily::RenderTarget64KB, info.bytes_per_element, block));
	const GpuTileInfo tile_info {block.family,
	                             block.bytes_per_element,
	                             0,
	                             info.size,
	                             0,
	                             info.size,
	                             0,
	                             info.width,
	                             info.height,
	                             1,
	                             info.pitch};
	GpuDetile(reinterpret_cast<const void*>(info.address), scratch.Data(), info.size, info.size,
	          std::span<const GpuTileInfo>(&tile_info, 1));
	SwapVideoOutBgra16(scratch.Data(), info.size);
	Transfer::UploadImage(image, scratch.Data(), info.size, info.pitch, vk::ImageLayout::eGeneral);
}

void SwapVideoOutBgra16(void* data, uint64_t size) {
	auto* pixels = static_cast<uint16_t*>(data);
	for (uint64_t i = 0; i < size / sizeof(uint16_t); i += 4) {
		std::swap(pixels[i], pixels[i + 2]);
	}
}

GpuTextureVulkanImage* CreateDummyTexture(bool uint_format, bool image_3d, bool storage) {
	auto* image  = storage ? static_cast<GpuTextureVulkanImage*>(new StorageTextureVulkanImage)
	                       : new TextureVulkanImage;
	auto  usage  = storage ? TextureFormatUsage::Storage : TextureFormatUsage::Sampled;
	auto  layout = storage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
	auto  owner  = storage ? "DummyStorageTexture" : "DummySampledTexture";

	auto params     = MakeDummyTextureParams(uint_format, image_3d, usage, owner);
	auto components = TextureCreateImage(*image, params);

	static constexpr uint32_t zero = 0;
	Transfer::UploadImage(*image, &zero, sizeof(zero), 1, layout);
	TextureCreateImageViews(*image, components, params.type, 0, params.base_level, params.levels,
	                        params.depth, params.allow_cube_view, params.view_usage);
	return image;
}

void Destroy(VulkanImage& image) {
	auto& graphics = GetRenderContext().GetGraphics();
	KYTY_PROFILER_BLOCK("TextureCache::DeleteImage");
	switch (image.type) {
		case VulkanImageType::RenderTexture:
		case VulkanImageType::VideoOut:
			GetRenderContext().GetFramebufferCache().FreeFramebufferByColor(image);
			break;
		case VulkanImageType::DepthStencil:
			GetRenderContext().GetFramebufferCache().FreeFramebufferByDepth(
			    static_cast<DepthStencilVulkanImage&>(image));
			break;
		case VulkanImageType::Texture:
		case VulkanImageType::StorageTexture: break;
		case VulkanImageType::Unknown: EXIT("cannot destroy an untyped Vulkan image\n");
	}

	ImageViewOps::DestroyViews(image);
	graphics.DeleteImage(image);

	switch (image.type) {
		case VulkanImageType::Texture: delete &static_cast<TextureVulkanImage&>(image); break;
		case VulkanImageType::StorageTexture:
			delete &static_cast<StorageTextureVulkanImage&>(image);
			break;
		case VulkanImageType::RenderTexture:
			delete &static_cast<RenderTextureVulkanImage&>(image);
			break;
		case VulkanImageType::DepthStencil:
			delete &static_cast<DepthStencilVulkanImage&>(image);
			break;
		case VulkanImageType::VideoOut: delete &static_cast<VideoOutVulkanImage&>(image); break;
		case VulkanImageType::Unknown: EXIT("cannot delete an untyped Vulkan image\n");
	}
}

} // namespace ImageOps

} // namespace Libs::Graphics
