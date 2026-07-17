#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_

#include "common/assert.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/imageInfo.h"
#include "graphics/shader/recompiler/ShaderIR.h"
#include "graphics/shader/shader.h"

namespace Libs::Graphics {

[[nodiscard]] inline bool IsSupportedStorageSwizzle(uint32_t format, uint32_t swizzle) noexcept {
	const bool single_channel =
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8UNorm) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32Float);
	return swizzle == DstSel(4, 5, 6, 7) ||
	       (single_channel && (swizzle == DstSel(4, 0, 0, 0) || swizzle == DstSel(4, 0, 0, 1))) ||
	       (format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8UNorm) &&
	        (swizzle == DstSel(4, 5, 6, 1) || swizzle == DstSel(6, 5, 4, 7))) ||
	       (format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32_32_32Float) &&
	        swizzle == DstSel(5, 6, 7, 4));
}

[[nodiscard]] inline bool IsSupportedStorageDepthTile(uint32_t format, uint32_t type,
                                                      uint32_t width, uint32_t height,
                                                      uint32_t depth) noexcept {
	return format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8UInt) &&
	       type == Prospero::GpuEnumValue(Prospero::ImageType::kColor2DArray) && width == 1 &&
	       height == 1 && depth == 1;
}

[[noreturn]] inline void UnsupportedColorView(const char* usage, VkFormat image_format,
                                              VkFormat view_format, uint32_t swizzle) noexcept {
	EXIT("unsupported %s color image view: image_format=%d view_format=%d swizzle=0x%03x\n", usage,
	     static_cast<int>(image_format), static_cast<int>(view_format), swizzle);
}

[[nodiscard]] inline VkFormat BgraToRgbaSampledViewFormat(VkFormat image_format) noexcept {
	switch (image_format) {
		case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		default: return VK_FORMAT_UNDEFINED;
	}
}

[[nodiscard]] inline bool IsBgraToRgbaSampledView(VkFormat image_format,
                                                  VkFormat view_format) noexcept {
	switch (image_format) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			switch (view_format) {
				case VK_FORMAT_R8G8B8A8_UNORM:
				case VK_FORMAT_R8G8B8A8_SRGB: return true;
				default: return false;
			}
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
			return view_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		default: return false;
	}
}

[[nodiscard]] inline VkFormat BgraSrgbStorageViewFormat(VkFormat image_format) noexcept {
	return image_format == VK_FORMAT_B8G8R8A8_SRGB ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED;
}

[[nodiscard]] inline bool IsBgraSrgbStorageView(VkFormat image_format, VkFormat view_format,
                                                uint32_t swizzle) noexcept {
	return view_format == BgraSrgbStorageViewFormat(image_format) && swizzle == DstSel(6, 5, 4, 7);
}

[[nodiscard]] inline int SelectSampledColorView(VkFormat image_format, VkFormat view_format,
                                                uint32_t swizzle) noexcept {
	if (image_format == view_format) {
		switch (swizzle) {
			case DstSel(4, 5, 6, 7): return VulkanImage::VIEW_DEFAULT;
			case DstSel(4, 0, 0, 1): return VulkanImage::VIEW_R001;
			case DstSel(4, 5, 0, 1): return VulkanImage::VIEW_RG01;
			case DstSel(4, 5, 6, 1): return VulkanImage::VIEW_RGB1;
			default: break;
		}
	}
	if (image_format == VK_FORMAT_R16G16B16A16_SFLOAT && view_format == image_format &&
	    swizzle == DstSel(7, 6, 5, 4)) {
		return VulkanImage::VIEW_ABGR;
	}
	if (IsBgraToRgbaSampledView(image_format, view_format) && swizzle == DstSel(6, 5, 4, 7)) {
		return VulkanImage::VIEW_BGRA_TO_RGBA;
	}
	UnsupportedColorView("sampled", image_format, view_format, swizzle);
}

[[nodiscard]] inline int SelectSampledDepthView(VkFormat image_format, VkFormat view_format,
                                                uint32_t swizzle) noexcept {
	if (IsSupportedSampledDepthFormat(image_format, view_format)) {
		switch (swizzle) {
			case DstSel(4, 4, 4, 4): return VulkanImage::VIEW_DEPTH_TEXTURE;
			case DstSel(4, 0, 0, 0): return VulkanImage::VIEW_R000;
			case DstSel(4, 0, 0, 1): return VulkanImage::VIEW_R001;
			default: break;
		}
	}
	EXIT("unsupported sampled depth image view: image_format=%d view_format=%d swizzle=0x%03x\n",
	     static_cast<int>(image_format), static_cast<int>(view_format), swizzle);
}

[[nodiscard]] inline bool
IsSupportedSampledDepthResource(const ShaderRecompiler::IR::ImageResource& resource) noexcept {
	return resource.kind == ShaderRecompiler::IR::ResourceKind::Image &&
	       resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2D &&
	       resource.mip_mode == ShaderRecompiler::IR::ImageMipMode::None && resource.read &&
	       !resource.written && !resource.atomic;
}

[[nodiscard]] inline int SelectStorageColorView(VkFormat image_format, VkFormat view_format,
                                                uint32_t swizzle) noexcept {
	const bool single_channel =
	    view_format == VK_FORMAT_R8_UNORM || view_format == VK_FORMAT_R8_UINT ||
	    view_format == VK_FORMAT_R16_UINT || view_format == VK_FORMAT_R32_UINT ||
	    view_format == VK_FORMAT_R16_SFLOAT || view_format == VK_FORMAT_R32_SFLOAT;
	const bool swizzle_ok =
	    swizzle == DstSel(4, 5, 6, 7) ||
	    (single_channel && (swizzle == DstSel(4, 0, 0, 0) || swizzle == DstSel(4, 0, 0, 1))) ||
	    (view_format == VK_FORMAT_R8G8B8A8_UNORM &&
	     (swizzle == DstSel(4, 5, 6, 1) || swizzle == DstSel(6, 5, 4, 7))) ||
	    (view_format == VK_FORMAT_R32G32B32A32_SFLOAT && swizzle == DstSel(5, 6, 7, 4));
	if ((image_format != view_format &&
	     !IsBgraSrgbStorageView(image_format, view_format, swizzle)) ||
	    !swizzle_ok) {
		UnsupportedColorView("storage", image_format, view_format, swizzle);
	}
	return VulkanImage::VIEW_STORAGE;
}

[[nodiscard]] inline bool
IsSupportedStorageImageResource(const ShaderRecompiler::IR::ImageResource& resource) noexcept {
	return (resource.kind == ShaderRecompiler::IR::ResourceKind::StorageImage ||
	        resource.kind == ShaderRecompiler::IR::ResourceKind::StorageImageUint) &&
	       (resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2D ||
	        resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim3D ||
	        resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2DArray) &&
	       resource.mip_mode == ShaderRecompiler::IR::ImageMipMode::None && resource.written &&
	       !resource.atomic && !resource.depth_compare;
}

inline void
ValidateStorageImageResource(const ShaderRecompiler::IR::ImageResource& resource) noexcept {
	if (!IsSupportedStorageImageResource(resource)) {
		EXIT("unsupported storage color image resource: kind=%u dimension=%u mip=%u "
		     "read=%d written=%d atomic=%d depth_compare=%d\n",
		     static_cast<uint32_t>(resource.kind), static_cast<uint32_t>(resource.dimension),
		     static_cast<uint32_t>(resource.mip_mode), resource.read, resource.written,
		     resource.atomic, resource.depth_compare);
	}
}

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_
