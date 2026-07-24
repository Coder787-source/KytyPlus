#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_

#include "common/assert.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/imageInfo.h"
#include "graphics/shader/recompiler/ShaderIR.h"
#include "graphics/shader/shader.h"

namespace Libs::Graphics {

[[nodiscard]] inline bool IsValidSampledColorSwizzle(uint32_t swizzle) noexcept {
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

[[nodiscard]] inline bool IsSupportedStorageSwizzle(uint32_t format, uint32_t swizzle) noexcept {
	// Storage image component remaps are applied in the SPIR-V emitter
	// (InverseStorageSwizzleComponent). Any valid channel sel for a supported storage format is
	// real hardware-correct behavior — do not dummy-null just because the sel is uncommon.
	if (!IsValidSampledColorSwizzle(swizzle)) {
		return false;
	}
	const bool single_channel =
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8UNorm) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32Float);
	const bool two_channel =
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8UNorm);
	const bool four_channel =
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8UNorm) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8SNorm) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8Srgb) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16_16_16Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16_16_16UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k16_16_16_16SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32_32_32Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32_32_32UInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32_32_32_32SInt) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k11_11_10Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k10_11_11Float) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k10_10_10_2UNorm) ||
	    format == Prospero::GpuEnumValue(Prospero::BufferFormat::k2_10_10_10UNorm);
	return single_channel || two_channel || four_channel;
}

[[nodiscard]] inline bool IsSupportedStorageDepthTile(uint32_t format, uint32_t type,
                                                      uint32_t width, uint32_t height,
                                                      uint32_t depth) noexcept {
	// Depth-tiled uint UAVs (#23 launcher path: 3840x2160 k8UInt 2D-array tile=kDepth;
	// stencil/HTile-adjacent write-only images).
	if (format != Prospero::GpuEnumValue(Prospero::BufferFormat::k8UInt) &&
	    format != Prospero::GpuEnumValue(Prospero::BufferFormat::k32UInt)) {
		return false;
	}
	if (width == 0 || height == 0 || depth == 0) {
		return false;
	}
	if (format == Prospero::GpuEnumValue(Prospero::BufferFormat::k8UInt) &&
	    (type == Prospero::GpuEnumValue(Prospero::ImageType::kColor2DArray) ||
	     type == Prospero::GpuEnumValue(Prospero::ImageType::kColor2D))) {
		return true;
	}
	return format == Prospero::GpuEnumValue(Prospero::BufferFormat::k32UInt) &&
	       type == Prospero::GpuEnumValue(Prospero::ImageType::kColor2D);
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
			switch (view_format) {
				case vk::Format::eR8G8B8A8Unorm:
				case vk::Format::eR8G8B8A8Srgb: return true;
				default: return false;
			}
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
	if (!IsValidSampledColorSwizzle(swizzle)) {
		return false;
	}
	if (image_format == view_format || IsRgba8SrgbReinterpretation(image_format, view_format)) {
		return true;
	}
	// Bit-exact reinterpretations (unorm<->uint, float<->uint). The component mapping of a
	// sampled view is orthogonal to the format reinterpretation in Vulkan, so any valid swizzle
	// is fine here, same as the identical-format and sRGB cases above. Games use this e.g. for
	// sampling an RGBA8 render target as uint with a BGRA channel order.
	if (IsRgba16UintFloatReinterpretation(image_format, view_format) ||
	    IsRgba8UnormUintReinterpretation(image_format, view_format)) {
		return true;
	}
	if (IsBgraToRgbaSampledView(image_format, view_format) && swizzle == DstSel(6, 5, 4, 7)) {
		return true;
	}
	// BGRA display/RT images rebound as RGBA8 uint with BGRA channel order (#41 Callisto).
	return (image_format == vk::Format::eB8G8R8A8Unorm ||
	        image_format == vk::Format::eB8G8R8A8Srgb) &&
	       view_format == vk::Format::eR8G8B8A8Uint && swizzle == DstSel(6, 5, 4, 7);
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
	// depth_compare is expected for shadow maps (cube/array PCF). Compare state lives on the
	// paired sampler; the image resource itself remains a readable depth view.
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

[[nodiscard]] inline int SelectStorageColorView(vk::Format image_format, vk::Format view_format,
                                                uint32_t swizzle) noexcept {
	const bool swizzle_ok = IsValidSampledColorSwizzle(swizzle);
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

namespace ImageViewOps {

[[nodiscard]] vk::ImageAspectFlags DepthAspectMask(vk::Format format);
[[nodiscard]] bool FormatSupportsStorage(vk::Format format);

void CreateRenderTargetViews(RenderTextureVulkanImage& image);
void CreateDepthViews(DepthStencilVulkanImage& image);
void CreateVideoOutViews(VideoOutVulkanImage& image);
void DestroyViews(VulkanImage& image);

} // namespace ImageViewOps

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGEVIEW_H_
