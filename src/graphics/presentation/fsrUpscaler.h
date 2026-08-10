#ifndef EMULATOR_SRC_GRAPHICS_PRESENTATION_FSR_UPSCALER_H_
#define EMULATOR_SRC_GRAPHICS_PRESENTATION_FSR_UPSCALER_H_

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/host_gpu/vulkanInstance.h"

namespace Libs::Graphics {

struct VulkanImage;

// FSR-inspired spatial upscaler: EASU (edge-adaptive upscaling) + RCAS (sharpening).
// Replaces the plain vkBlitImage presentation stretch with a two-pass compute pipeline.
// Works on any Vulkan 1.3 GPU.
class FsrUpscaler {
public:
	FsrUpscaler()  = default;
	~FsrUpscaler();
	KYTY_CLASS_NO_COPY(FsrUpscaler);

	// Create compute pipelines, descriptor layouts, and intermediate resources.
	// Must be called once after the Vulkan device is ready.
	bool Create(VulkanInstance& gfx);

	// Destroy all Vulkan resources.
	void Destroy();

	// Run the two-pass upscaler on a command buffer.
	// source: guest frame (must be in eTransferSrcOptimal or eShaderReadOnlyOptimal).
	// dest:   swapchain image (will be transitioned from eUndefined → eTransferDstOptimal → ePresentSrcKHR).
	// src_w/h: guest resolution. dst_w/h: window resolution.
	// sharpness: 0.0–1.0 RCAS strength.
	void Dispatch(vk::CommandBuffer cmd, VulkanImage& source, vk::Image dest,
	              vk::Format dest_format,
	              uint32_t src_w, uint32_t src_h,
	              uint32_t dst_w, uint32_t dst_h,
	              float sharpness);

	// Returns true if Create() succeeded and the pipelines are ready.
	[[nodiscard]] bool IsReady() const { return m_ready; }

private:
	struct IntermediateImage {
		vk::Image       image     = nullptr;
		vk::ImageView   view      = nullptr;
		vk::DeviceMemory memory   = nullptr;
		uint32_t        width     = 0;
		uint32_t        height    = 0;
	};

	bool CreatePipelines();
	bool CreateDescriptorResources();
	bool EnsureIntermediate(uint32_t width, uint32_t height);

	VulkanInstance*      m_gfx   = nullptr;
	bool                 m_ready = false;

	// Descriptor set layout: binding 0 = sampler2D, binding 1 = storageImage, binding 2 = UBO.
	vk::DescriptorSetLayout m_ds_layout      = nullptr;
	vk::PipelineLayout      m_pipeline_layout = nullptr;

	// Compute pipelines.
	vk::Pipeline m_easu_pipeline = nullptr;
	vk::Pipeline m_rcas_pipeline = nullptr;

	// Descriptor pool and sets (one per pass).
	vk::DescriptorPool m_desc_pool = nullptr;
	vk::DescriptorSet  m_easu_ds  = nullptr;
	vk::DescriptorSet  m_rcas_ds  = nullptr;

	// Uniform buffers (one per pass, holds the push-constant-equivalent UBO).
	vk::Buffer       m_easu_ubo     = nullptr;
	vk::DeviceMemory m_easu_ubo_mem = nullptr;
	vk::Buffer       m_rcas_ubo     = nullptr;
	vk::DeviceMemory m_rcas_ubo_mem = nullptr;

	// Sampler for reading the source / intermediate images.
	vk::Sampler m_linear_sampler = nullptr;

	// Intermediate image: EASU writes here, RCAS reads from here.
	IntermediateImage m_intermediate;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_PRESENTATION_FSR_UPSCALER_H_
