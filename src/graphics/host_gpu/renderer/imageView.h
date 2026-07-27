#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_

#include "common/assert.h"
#include "graphics/host_gpu/renderer/imageInfo.h"
#include "graphics/shader/recompiler/ShaderIR.h"
#include "graphics/shader/shader.h"

namespace Libs::Graphics {

[[nodiscard]] inline bool IsValidImageSwizzle(uint32_t swizzle) noexcept {
	if ((swizzle & ~0xfffu) != 0) {
		return false;
	}
	for (uint32_t channel = 0; channel < 4; channel++) {
		switch (GetDstSel(swizzle, channel)) {
			case 0:
			case 1:
			case 4:
			case 5:
			case 6:
			case 7: break;
			default: return false;
		}
	}
	return true;
}

[[noreturn]] inline void UnsupportedColorView(const char* usage, vk::Format image_format,
                                              vk::Format view_format, uint32_t swizzle) noexcept {
	EXIT("unsupported %s color image view: image_format=%d view_format=%d swizzle=0x%03x\n", usage,
	     static_cast<int>(image_format), static_cast<int>(view_format), swizzle);
}

[[nodiscard]] inline vk::Format BgraToRgbaSampledViewFormat(vk::Format image_format) noexcept {
	switch (image_format) {
		case vk::Format::eB8G8R8A8Unorm: return vk::Format::eR8G8B8A8Unorm;
		case vk::Format::eB8G8R8A8Srgb: return vk::Format::eR8G8B8A8Srgb;
		case vk::Format::eA2R10G10B10UnormPack32: return vk::Format::eA2B10G10R10UnormPack32;
		default: return vk::Format::eUndefined;
	}
}

[[nodiscard]] inline bool IsBgraToRgbaSampledView(vk::Format image_format,
                                                  vk::Format view_format) noexcept {
	switch (image_format) {
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Srgb:
			return view_format == vk::Format::eR8G8B8A8Unorm ||
			       view_format == vk::Format::eR8G8B8A8Srgb;
		case vk::Format::eA2R10G10B10UnormPack32:
			return view_format == vk::Format::eA2B10G10R10UnormPack32;
		default: return false;
	}
}

[[nodiscard]] inline vk::Format BgraSrgbStorageViewFormat(vk::Format image_format) noexcept {
	return image_format == vk::Format::eB8G8R8A8Srgb ? vk::Format::eR8G8B8A8Unorm
	                                                 : vk::Format::eUndefined;
}

[[nodiscard]] inline vk::Format SrgbStorageViewFormat(vk::Format image_format) noexcept {
	return image_format == vk::Format::eR8G8B8A8Srgb ? vk::Format::eR8G8B8A8Unorm
	                                                 : BgraSrgbStorageViewFormat(image_format);
}

[[nodiscard]] inline bool IsBgraSrgbStorageView(vk::Format image_format, vk::Format view_format,
                                                uint32_t swizzle) noexcept {
	return view_format == BgraSrgbStorageViewFormat(image_format) && swizzle == DstSel(6, 5, 4, 7);
}

[[nodiscard]] inline bool IsSupportedSampledColorView(vk::Format image_format,
                                                      vk::Format view_format,
                                                      uint32_t   swizzle) noexcept {
	if (!IsValidImageSwizzle(swizzle)) {
		return false;
	}
	if (image_format == view_format || IsRgba8SrgbReinterpretation(image_format, view_format)) {
		return true;
	}
	if ((IsRgba16UintFloatReinterpretation(image_format, view_format) ||
	     IsRgba8UnormUintReinterpretation(image_format, view_format)) &&
	    swizzle == DstSel(4, 5, 6, 7)) {
		return true;
	}
	return IsBgraToRgbaSampledView(image_format, view_format) && swizzle == DstSel(6, 5, 4, 7);
}

[[nodiscard]] inline uint32_t
SelectSampledColorView(vk::Format image_format, vk::Format view_format, uint32_t swizzle) noexcept {
	if (IsSupportedSampledColorView(image_format, view_format, swizzle)) {
		return swizzle;
	}
	UnsupportedColorView("sampled", image_format, view_format, swizzle);
}

[[nodiscard]] inline bool IsSupportedSampledDepthView(vk::Format image_format,
                                                      vk::Format view_format,
                                                      uint32_t   swizzle) noexcept {
	if (!IsSupportedSampledDepthFormat(image_format, view_format)) {
		return false;
	}
	switch (swizzle) {
		case DstSel(4, 4, 4, 4):
		case DstSel(4, 0, 0, 0):
		case DstSel(4, 0, 0, 1): return true;
		default: return false;
	}
}

[[nodiscard]] inline uint32_t
SelectSampledDepthView(vk::Format image_format, vk::Format view_format, uint32_t swizzle) noexcept {
	if (IsSupportedSampledDepthView(image_format, view_format, swizzle)) {
		return swizzle;
	}
	EXIT("unsupported sampled depth image view: image_format=%d view_format=%d swizzle=0x%03x\n",
	     static_cast<int>(image_format), static_cast<int>(view_format), swizzle);
}

[[nodiscard]] inline bool
IsSupportedSampledDepthResource(const ShaderRecompiler::IR::ImageResource& resource) noexcept {
	return resource.kind == ShaderRecompiler::IR::ResourceKind::Image &&
	       (resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2D ||
	        resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2DArray) &&
	       resource.mip_mode == ShaderRecompiler::IR::ImageMipMode::None && resource.read &&
	       !resource.written && !resource.atomic;
}

[[nodiscard]] inline bool
IsSupportedSampledDepthUintResource(const ShaderRecompiler::IR::ImageResource& resource) noexcept {
	return resource.kind == ShaderRecompiler::IR::ResourceKind::ImageUint &&
	       resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2D &&
	       resource.mip_mode == ShaderRecompiler::IR::ImageMipMode::None && resource.read &&
	       !resource.written && !resource.atomic && !resource.depth_compare;
}

inline void ValidateStorageColorView(vk::Format image_format, vk::Format view_format,
                                     uint32_t swizzle) noexcept {
	if ((image_format != view_format &&
	     !IsBgraSrgbStorageView(image_format, view_format, swizzle)) ||
	    !IsValidImageSwizzle(swizzle)) {
		UnsupportedColorView("storage", image_format, view_format, swizzle);
	}
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

namespace ImageViewOps {

[[nodiscard]] vk::ImageAspectFlags DepthAspectMask(vk::Format format);
[[nodiscard]] bool                 FormatsCompatible(vk::Format base, vk::Format view) noexcept;
} // namespace ImageViewOps

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_
