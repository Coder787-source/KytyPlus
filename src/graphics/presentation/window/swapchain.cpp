#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_gamecontroller.h"
#include "SDL_hints.h"
#include "SDL_joystick.h"
#include "SDL_keyboard.h"
#include "SDL_keycode.h"
#include "SDL_mouse.h"
#include "SDL_pixels.h"
#include "SDL_rwops.h"
#include "SDL_stdinc.h"
#include "SDL_surface.h"
#include "SDL_thread.h"
#include "SDL_touch.h"
#include "SDL_video.h"
#include "SDL_vulkan.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/emulatorConfig.h"
#include "graphics/presentation/fsrUpscaler.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "common/stringUtils.h"
#include "common/subsystems.h"
#include "common/systemInfo.h"
#include "common/threads.h"
#include "common/timer.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/vma.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/presentation/presenter.h"
#include "graphics/presentation/renderDoc.h"
#include "graphics/presentation/videoOut.h"
#include "graphics/presentation/window/windowInternal.h"
#include "libs/controller.h"
#include "loader/systemContent.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fmt/format.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>

// stb_image_write is implemented inline here; it only needs the impl in one TU.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vulkan/vk_platform.h>

// IWYU pragma: no_include <intrin.h>

#define KYTY_ENABLE_DEBUG_PRINTF
#define KYTY_DBG_INPUT

namespace Libs::Graphics {

struct Presenter::Frame {
	VulkanImage                    image;
	std::unique_ptr<CommandBuffer> present_commands;
	bool                           busy = false;
	bool                           reusing_last = false;

	void Configure(GraphicContext& graphics, vk::Extent2D extent, vk::Format format);
	void Transit(vk::CommandBuffer command, vk::ImageLayout layout, vk::AccessFlags2 access);
	void CopyFrom(CommandBuffer& command, Image& source);
	void Clear(CommandBuffer& command, const vk::ClearColorValue& color);
};

class FramePool final {
public:
	explicit FramePool(WindowContext& window): m_window(window) {}
	~FramePool() {
		for (auto& frame: m_frames) {
			if (frame->present_commands != nullptr) {
				frame->present_commands->WaitForFenceOnly();
			}
			if (frame->image.image != nullptr) {
				m_window.graphic_ctx.DeleteImage(frame->image);
			}
		}
	}
	KYTY_CLASS_NO_COPY(FramePool);

	void Initialize(uint32_t count, vk::Format format) {
		if (count == 0 || format == vk::Format::eUndefined) {
			EXIT("prepared-frame pool requires at least one frame\n");
		}
		Common::LockGuard lock(m_mutex);
		if (!m_frames.empty()) {
			EXIT("prepared-frame pool was initialized twice\n");
		}
		m_format = format;
		m_frames.reserve(count);
		for (uint32_t i = 0; i < count; i++) {
			auto frame = std::make_unique<Presenter::Frame>();
			m_free.push_back(frame.get());
			m_frames.push_back(std::move(frame));
		}
	}

	void SetFormat(vk::Format format) {
		if (format == vk::Format::eUndefined) {
			EXIT("prepared-frame pool requires a presentation format\n");
		}
		Common::LockGuard lock(m_mutex);
		m_format = format;
	}

	vk::Format GetFormat() {
		Common::LockGuard lock(m_mutex);
		if (m_format == vk::Format::eUndefined) {
			EXIT("prepared-frame pool has no presentation format\n");
		}
		return m_format;
	}

	Presenter::Frame* Acquire() {
		m_mutex.Lock();
		if (m_frames.empty()) {
			EXIT("prepared-frame pool was used before swapchain initialization\n");
		}
		while (m_free.empty()) {
			m_available.Wait(&m_mutex);
		}
		auto* frame = m_free.front();
		m_free.pop_front();
		if (frame->busy) {
			EXIT("prepared-frame pool returned an invalid frame\n");
		}
		if (m_last_frame == frame) {
			m_last_frame = nullptr;
		}
		frame->busy         = true;
		frame->reusing_last = false;
		m_mutex.Unlock();

		WaitForFrame(*frame);
		return frame;
	}

	Presenter::Frame* AcquireLast() {
		m_mutex.Lock();
		auto* frame = m_last_frame;
		if (frame == nullptr) {
			m_mutex.Unlock();
			return nullptr;
		}
		auto free = std::find(m_free.begin(), m_free.end(), frame);
		if (free == m_free.end() || frame->busy) {
			m_mutex.Unlock();
			EXIT("last submitted frame is not available for reuse\n");
		}
		m_free.erase(free);
		m_last_frame       = nullptr;
		frame->busy         = true;
		frame->reusing_last = true;
		m_mutex.Unlock();

		WaitForFrame(*frame);
		return frame;
	}

	void ValidateForPresent(Presenter::Frame* frame, bool reuse) {
		Common::LockGuard lock(m_mutex);
		if (frame == nullptr || !frame->busy || frame->reusing_last != reuse) {
			EXIT("prepared frame has invalid presentation ownership\n");
		}
	}

	void Release(Presenter::Frame* frame, bool make_last = false) {
		if (frame == nullptr) {
			EXIT("cannot release a null prepared frame\n");
		}
		Common::LockGuard lock(m_mutex);
		if (!frame->busy) {
			EXIT("prepared frame was released twice\n");
		}
		frame->busy         = false;
		frame->reusing_last = false;
		if (make_last) {
			m_last_frame = frame;
		}
		m_free.push_back(frame);
		m_available.Signal();
	}

private:
	static void WaitForFrame(Presenter::Frame& frame) {
		// The producer only waits here. Reset stays on the presentation thread that owns the
		// allocating Vulkan command pool.
		if (frame.present_commands != nullptr) {
			frame.present_commands->WaitForFenceOnly();
		}
	}

	WindowContext&                               m_window;
	Common::Mutex                               m_mutex;
	Common::CondVar                             m_available;
	std::vector<std::unique_ptr<Presenter::Frame>> m_frames;
	std::deque<Presenter::Frame*>                  m_free;
	Presenter::Frame*                              m_last_frame = nullptr;
	vk::Format                                  m_format = vk::Format::eUndefined;
};

void Presenter::Frame::Configure(GraphicContext& graphics, vk::Extent2D extent,
                                 vk::Format format) {
	if (extent.width == 0 || extent.height == 0 || format == vk::Format::eUndefined) {
		EXIT("unsupported prepared frame, extent=%ux%u format=%d\n", extent.width, extent.height,
		     static_cast<int>(format));
	}
	const auto features = graphics.GetFormatProperties(format).optimalTilingFeatures;
	const auto required = vk::FormatFeatureFlagBits::eBlitSrc |
	                      vk::FormatFeatureFlagBits::eSampledImageFilterLinear |
	                      vk::FormatFeatureFlagBits::eTransferSrc |
	                      vk::FormatFeatureFlagBits::eTransferDst;
	if ((features & required) != required) {
		EXIT("prepared presentation format lacks optimal blit support: format=%d features=0x%x\n",
		     static_cast<int>(format),
		     static_cast<vk::FormatFeatureFlags::MaskType>(features));
	}

	auto&      dst        = image;
	const bool compatible = dst.image != nullptr && dst.extent.width == extent.width &&
	                        dst.extent.height == extent.height && dst.format == format;
	if (compatible) {
		return;
	}
	if (dst.image != nullptr) {
		graphics.DeleteImage(dst);
		dst.memory = {};
	}

	dst.extent          = {extent.width, extent.height, 1};
	dst.format          = format;
	dst.layers          = 1;
	dst.mip_levels      = 1;
	dst.state           = {};
	dst.subresource_states.clear();
	dst.memory.property = vk::MemoryPropertyFlagBits::eDeviceLocal;

	vk::ImageCreateInfo create {};
	create.sType         = vk::StructureType::eImageCreateInfo;
	create.imageType     = vk::ImageType::e2D;
	create.extent        = {dst.extent.width, dst.extent.height, 1};
	create.mipLevels     = 1;
	create.arrayLayers   = 1;
	create.format        = dst.format;
	create.tiling        = vk::ImageTiling::eOptimal;
	create.initialLayout = vk::ImageLayout::eUndefined;
	create.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
	create.sharingMode = vk::SharingMode::eExclusive;
	create.samples     = vk::SampleCountFlagBits::e1;
	if (!graphics.CreateImage(create, dst)) {
		EXIT("failed to allocate prepared presentation image, extent=%ux%u format=%d\n",
		     dst.extent.width, dst.extent.height, static_cast<int>(dst.format));
	}
}

void Presenter::Frame::Transit(vk::CommandBuffer command, vk::ImageLayout layout,
                               vk::AccessFlags2 access) {
	const auto stage = access == vk::AccessFlagBits2::eTransferRead ||
	                           access == vk::AccessFlagBits2::eTransferWrite
	                       ? vk::PipelineStageFlagBits2::eTransfer
	                       : vk::PipelineStageFlagBits2::eAllCommands;
	constexpr auto writes = vk::AccessFlagBits2::eTransferWrite |
	                        vk::AccessFlagBits2::eShaderWrite |
	                        vk::AccessFlagBits2::eMemoryWrite;
	if (image.state.layout == layout && image.state.access_mask == access &&
	    !static_cast<bool>(image.state.access_mask & writes)) {
		return;
	}
	vk::ImageMemoryBarrier2 barrier {};
	barrier.srcStageMask                    = image.state.pl_stage;
	barrier.srcAccessMask                   = image.state.access_mask;
	barrier.dstStageMask                    = stage;
	barrier.dstAccessMask                   = access;
	barrier.oldLayout                       = image.state.layout;
	barrier.newLayout                       = layout;
	barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	barrier.image                           = image.image;
	barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseMipLevel   = 0;
	barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
	vk::DependencyInfo dependency {};
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers    = &barrier;
	command.pipelineBarrier2(dependency);
	image.state = {stage, access, layout};
	image.subresource_states.clear();
}

void Presenter::Frame::CopyFrom(CommandBuffer& command_buffer, Image& source) {
	command_buffer.EndRendering();
	auto command = command_buffer.Handle();
	source.Transit(vk::ImageLayout::eTransferSrcOptimal,
	               vk::AccessFlagBits2::eTransferRead, {}, command);
	Transit(command, vk::ImageLayout::eTransferDstOptimal,
	        vk::AccessFlagBits2::eTransferWrite);
	vk::ImageCopy copy {};
	copy.srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0,
	                       source.backing.layers};
	copy.dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, image.layers};
	copy.extent = {std::min(source.backing.extent.width, image.extent.width),
	               std::min(source.backing.extent.height, image.extent.height), 1};
	EXIT_IF(copy.srcSubresource.layerCount != copy.dstSubresource.layerCount);
	command.copyImage(source.backing.image, vk::ImageLayout::eTransferSrcOptimal,
	                  image.image, vk::ImageLayout::eTransferDstOptimal, copy);
	Transit(command, vk::ImageLayout::eTransferSrcOptimal,
	        vk::AccessFlagBits2::eTransferRead);
}

void Presenter::Frame::Clear(CommandBuffer& command_buffer,
                             const vk::ClearColorValue& color) {
	command_buffer.EndRendering();
	auto command = command_buffer.Handle();
	Transit(command, vk::ImageLayout::eTransferDstOptimal,
	        vk::AccessFlagBits2::eTransferWrite);
	const vk::ImageSubresourceRange range {
	    vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
	command.clearColorImage(image.image, vk::ImageLayout::eTransferDstOptimal, &color, 1,
	                        &range);
	Transit(command, vk::ImageLayout::eTransferSrcOptimal,
	        vk::AccessFlagBits2::eTransferRead);
}

class Swapchain final {
public:
	enum class Status : uint8_t { Success, Recreate, SurfaceLost };

	explicit Swapchain(WindowContext& window): m_window(window) {}
	~Swapchain();
	KYTY_CLASS_NO_COPY(Swapchain);

	void Create();
	void Recreate(bool surface_lost = false);
	[[nodiscard]] Status AcquireNextImage();
	void                 RecordPresentCommands(CommandBuffer& command, VulkanImage& source);
	void                 Submit(CommandBuffer& command);
	[[nodiscard]] Status Present();

	// Captures the current swapchain image to a PNG in Config::GetScreenshotFolder().
	// Safe to call from the present thread after the swapchain image has been recorded
	// (i.e. after RecordPresentCommands + Submit). Returns the written file path on success.
	[[nodiscard]] std::string CaptureScreenshot();

	[[nodiscard]] uint32_t ImageCount() const noexcept {
		return static_cast<uint32_t>(m_images.size());
	}
	[[nodiscard]] vk::Format Format() const noexcept { return m_format; }

private:
	void Destroy();
	void RefreshSurfaceSize();

	WindowContext&             m_window;
	FsrUpscaler              m_fsr;
	vk::SwapchainKHR           m_handle = nullptr;
	vk::Format                 m_format = vk::Format::eUndefined;
	vk::Extent2D               m_extent {};
	std::vector<vk::Image>     m_images;
	std::vector<vk::ImageView> m_image_views;
	std::vector<vk::Semaphore> m_image_acquired;
	std::vector<vk::Semaphore> m_render_complete;
	uint32_t                   m_image_index = static_cast<uint32_t>(-1);
	uint32_t                   m_frame_index = 0;
};

struct Presenter::Impl {
	explicit Impl(WindowContext& owner)
	    : renderer(*owner.render_context), window(owner), swapchain(owner),
	      present_scheduler(renderer, owner.graphic_ctx), frames(owner) {
		EXIT_IF(owner.render_context == nullptr);
		swapchain.Create();
		frames.Initialize(swapchain.ImageCount(), swapchain.Format());
	}

	void RecoverSwapchain(Swapchain::Status status) {
		LOGF("Recovering Vulkan swapchain%s\n",
		     status == Swapchain::Status::SurfaceLost ? " and surface" : "");
		swapchain.Recreate(status == Swapchain::Status::SurfaceLost);
		frames.SetFormat(swapchain.Format());
	}

	Image& ResolveSurface(const ImageInfo& info) {
		TextureCache::ImageDesc desc {};
		desc.info                  = info;
		desc.view_info.format      = info.pixel_format;
		desc.view_info.type        = vk::ImageViewType::e2D;
		desc.view_info.aspect      = vk::ImageAspectFlagBits::eColor;
		desc.view_info.base_level  = 0;
		desc.view_info.level_count = 1;
		desc.view_info.base_layer  = 0;
		desc.view_info.layer_count = 1;
		desc.view_info.usage       = vk::ImageUsageFlagBits::eTransferSrc;
		desc.type                  = TextureCache::BindingType::VideoOut;

		auto& cache = renderer.GetTextureCache();
		auto& image = cache.GetImage(cache.FindImage(desc));
		image.usage.video_out = true;
		return image;
	}

	RenderContext& renderer;
	WindowContext& window;
	Swapchain      swapchain;
	CommandScheduler present_scheduler;
	FramePool      frames;
};

void Swapchain::Create() {
	auto& graphics = m_window.graphic_ctx;
	EXIT_IF(graphics.screen_width == 0);
	EXIT_IF(graphics.screen_height == 0);
	EXIT_IF(graphics.device == nullptr);
	EXIT_IF(m_window.surface == nullptr);

	Common::LockGuard lock(m_window.mutex);
	const auto&       surface = m_window.surface_capabilities;
	EXIT_NOT_IMPLEMENTED(surface.formats.empty());

	m_extent = surface.capabilities.currentExtent;
	if (m_extent.width == std::numeric_limits<uint32_t>::max()) {
		// Minimized Win32 surfaces can report maxImageExtent=0x0; clamp would yield illegal 0.
		const uint32_t max_w = surface.capabilities.maxImageExtent.width;
		const uint32_t max_h = surface.capabilities.maxImageExtent.height;
		m_extent.width =
		    (max_w == 0) ? 1
		                 : std::clamp(graphics.screen_width,
		                              surface.capabilities.minImageExtent.width, max_w);
		m_extent.height =
		    (max_h == 0) ? 1
		                 : std::clamp(graphics.screen_height,
		                              surface.capabilities.minImageExtent.height, max_h);
		if (max_w == 0 || max_h == 0) {
			static std::atomic<uint32_t> degenerate_logs {0};
			if (degenerate_logs.fetch_add(1, std::memory_order_relaxed) < 8) {
				LOGF_COLOR(Log::Color::Yellow,
				           "Swapchain: surface reports 0x0 max extent (window likely minimized), "
				           "falling back to %ux%u\n",
				           m_extent.width, m_extent.height);
			}
		}
	} else if (m_extent.width == 0 || m_extent.height == 0) {
		m_extent.width  = std::max(1u, graphics.screen_width);
		m_extent.height = std::max(1u, graphics.screen_height);
		static std::atomic<uint32_t> degenerate_logs {0};
		if (degenerate_logs.fetch_add(1, std::memory_order_relaxed) < 8) {
			LOGF_COLOR(Log::Color::Yellow,
			           "Swapchain: surface currentExtent is 0x0 (window likely minimized), "
			           "falling back to %ux%u\n",
			           m_extent.width, m_extent.height);
		}
	}
	uint32_t image_count = surface.capabilities.minImageCount + 1;
	if (surface.capabilities.maxImageCount != 0) {
		image_count = std::min(image_count, surface.capabilities.maxImageCount);
	}
	const auto transform =
	    surface.capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity
	        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
	        : surface.capabilities.currentTransform;
	const auto composite =
	    surface.capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque
	        ? vk::CompositeAlphaFlagBitsKHR::eOpaque
	        : vk::CompositeAlphaFlagBitsKHR::eInherit;

	vk::SurfaceFormatKHR format {vk::Format::eR8G8B8A8Unorm,
	                             vk::ColorSpaceKHR::eSrgbNonlinear};
	if (surface.formats.size() != 1 ||
	    surface.formats.front().format != vk::Format::eUndefined) {
		const auto it = std::find_if(surface.formats.begin(), surface.formats.end(),
		                             [](const vk::SurfaceFormatKHR& candidate) {
			                             return candidate.format ==
			                                        vk::Format::eB8G8R8A8Unorm ||
			                                    candidate.format ==
			                                        vk::Format::eR8G8B8A8Unorm;
		                             });
		if (it == surface.formats.end()) {
			EXIT("no supported UNORM swapchain format\n");
		}
		format = *it;
	}
	m_format = format.format;
	const auto swapchain_features =
	    graphics.GetFormatProperties(m_format).optimalTilingFeatures;
	if (!static_cast<bool>(swapchain_features & vk::FormatFeatureFlagBits::eBlitDst)) {
		EXIT("swapchain format cannot be a blit destination: format=%d\n",
		     static_cast<int>(m_format));
	}

	vk::SwapchainCreateInfoKHR create_info {};
	create_info.sType            = vk::StructureType::eSwapchainCreateInfoKHR;
	create_info.surface          = m_window.surface;
	create_info.minImageCount    = image_count;
	create_info.imageFormat      = format.format;
	create_info.imageColorSpace  = format.colorSpace;
	create_info.imageExtent      = m_extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage =
	    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
	// FSR RCAS pass writes to the swapchain image via imageStore (storage).
	if (Config::GetUpscalerMethod() == Config::UpscalerMethod::Fsr31) {
		create_info.imageUsage |= vk::ImageUsageFlagBits::eStorage;
	}
	create_info.imageSharingMode = vk::SharingMode::eExclusive;
	create_info.preTransform     = transform;
	create_info.compositeAlpha   = composite;
	// Resolve the requested present mode against what the surface actually exposes.
	// vk::PresentModeKHR::eFifo is guaranteed by the Vulkan spec, so it is the safe fallback.
	const auto& supported = m_window.surface_capabilities.present_modes;
	vk::PresentModeKHR chosen_mode = vk::PresentModeKHR::eFifo;
	switch (Config::GetPresentMode()) {
		case Config::PresentMode::Mailbox: {
			const auto it = std::find(supported.begin(), supported.end(),
			                         vk::PresentModeKHR::eMailbox);
			if (it != supported.end()) {
				chosen_mode = vk::PresentModeKHR::eMailbox;
			} else {
				static std::atomic<uint32_t> mb_logged {0};
				if (mb_logged.fetch_add(1, std::memory_order_relaxed) == 0) {
					LOGF_COLOR(Log::Color::Yellow,
					           "Present: mailbox mode not supported by surface; falling back to Fifo\n");
				}
			}
			break;
		}
		case Config::PresentMode::Immediate: {
			const auto it = std::find(supported.begin(), supported.end(),
			                         vk::PresentModeKHR::eImmediate);
			if (it != supported.end()) {
				chosen_mode = vk::PresentModeKHR::eImmediate;
			} else {
				static std::atomic<uint32_t> im_logged {0};
				if (im_logged.fetch_add(1, std::memory_order_relaxed) == 0) {
					LOGF_COLOR(Log::Color::Yellow,
					           "Present: immediate mode not supported by surface; falling back to Fifo\n");
				}
			}
			break;
		}
		case Config::PresentMode::Fifo:
		default: chosen_mode = vk::PresentModeKHR::eFifo; break;
	}
	create_info.presentMode      = chosen_mode;
	create_info.clipped          = VK_TRUE;
	RequireVulkanSuccess(graphics.device.createSwapchainKHR(&create_info, nullptr, &m_handle),
	                     "vkCreateSwapchainKHR");
	EXIT_IF(m_handle == nullptr);

	m_images = EnumerateVulkan<vk::Image>(
	    "vkGetSwapchainImagesKHR", [&](uint32_t* count, vk::Image* images) {
		    return graphics.device.getSwapchainImagesKHR(m_handle, count, images);
	    });
	EXIT_NOT_IMPLEMENTED(m_images.empty());

	m_image_views.resize(m_images.size());
	for (size_t i = 0; i < m_images.size(); i++) {
		vk::ImageViewCreateInfo view {};
		view.sType                           = vk::StructureType::eImageViewCreateInfo;
		view.image                           = m_images[i];
		view.viewType                        = vk::ImageViewType::e2D;
		view.format                          = m_format;
		view.components                      = {};
		view.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.baseMipLevel   = 0;
		view.subresourceRange.layerCount     = 1;
		view.subresourceRange.levelCount     = 1;
		RequireVulkanSuccess(
		    graphics.device.createImageView(&view, nullptr, &m_image_views[i]),
		    "vkCreateImageView");
		EXIT_IF(m_image_views[i] == nullptr);
	}

	vk::SemaphoreCreateInfo semaphore_info {};
	semaphore_info.sType = vk::StructureType::eSemaphoreCreateInfo;
	m_image_acquired.resize(m_images.size());
	m_render_complete.resize(m_images.size());
	for (size_t i = 0; i < m_images.size(); i++) {
		RequireVulkanSuccess(
		    graphics.device.createSemaphore(&semaphore_info, nullptr, &m_image_acquired[i]),
		    "create swapchain image-acquired semaphore");
		RequireVulkanSuccess(
		    graphics.device.createSemaphore(&semaphore_info, nullptr, &m_render_complete[i]),
		    "create swapchain render-complete semaphore");
	}
	m_image_index = static_cast<uint32_t>(-1);
	m_frame_index = 0;

	// Initialize FSR upscaler if enabled.
	if (Config::GetUpscalerMethod() == Config::UpscalerMethod::Fsr31) {
		if (!m_fsr.Create(graphics)) {
			LOGF_COLOR(Log::Color::Yellow,
			           "FSR: failed to initialise upscaler pipelines; falling back to blit\n");
		}
	}
}

Swapchain::~Swapchain() {
	Destroy();
}

void Swapchain::Destroy() {
	if (m_handle == nullptr && m_image_acquired.empty() && m_render_complete.empty() &&
	    m_image_views.empty()) {
		return;
	}
	auto& graphics = m_window.graphic_ctx;

	// Destroy FSR resources before the device goes away.
	m_fsr.Destroy();

	{
		Common::LockGuard queue_lock(graphics.queue_mutex);
		RequireVulkanSuccess(graphics.queue.waitIdle(), "wait for swapchain queue");
	}

	for (const auto semaphore: m_image_acquired) {
		if (semaphore != nullptr) {
			graphics.device.destroySemaphore(semaphore, nullptr);
		}
	}
	for (const auto semaphore: m_render_complete) {
		if (semaphore != nullptr) {
			graphics.device.destroySemaphore(semaphore, nullptr);
		}
	}
	for (const auto view: m_image_views) {
		if (view != nullptr) {
			graphics.device.destroyImageView(view, nullptr);
		}
	}
	if (m_handle != nullptr) {
		graphics.device.destroySwapchainKHR(m_handle, nullptr);
	}

	m_handle      = nullptr;
	m_format      = vk::Format::eUndefined;
	m_extent      = {};
	m_image_index = static_cast<uint32_t>(-1);
	m_frame_index = 0;
	m_images.clear();
	m_image_views.clear();
	m_image_acquired.clear();
	m_render_complete.clear();
}

void Swapchain::RefreshSurfaceSize() {
	int width  = 0;
	int height = 0;
	SDL_Vulkan_GetDrawableSize(m_window.window, &width, &height);
	if (width > 0 && height > 0) {
		m_window.graphic_ctx.screen_width  = static_cast<uint32_t>(width);
		m_window.graphic_ctx.screen_height = static_cast<uint32_t>(height);
	}

	m_window.RefreshSurfaceCapabilities();
}

void Swapchain::Recreate(bool surface_lost) {
	Destroy();
	if (surface_lost) {
#if defined(__APPLE__)
		// Surface recreation goes through SDL_Vulkan_CreateSurface, which touches the
		// window's view/layer and must run on the main thread on macOS.
		m_window.RunOnMainThread([this] { m_window.RecreateSurface(); }, true);
#else
		m_window.RecreateSurface();
#endif
	}
	RefreshSurfaceSize();
	Create();
}

Swapchain::Status Swapchain::AcquireNextImage() {
	EXIT_IF(m_handle == nullptr || m_frame_index >= m_image_acquired.size());
	m_image_index = static_cast<uint32_t>(-1);
	const auto result = m_window.graphic_ctx.device.acquireNextImageKHR(
	    m_handle, std::numeric_limits<uint64_t>::max(), m_image_acquired[m_frame_index], nullptr,
	    &m_image_index);
	switch (result) {
		case vk::Result::eSuccess: break;
		case vk::Result::eSuboptimalKHR:
			LOGF("vkAcquireNextImageKHR returned vk::Result::eSuboptimalKHR\n");
			return Status::Recreate;
		case vk::Result::eErrorOutOfDateKHR:
			LOGF("vkAcquireNextImageKHR returned vk::Result::eErrorOutOfDateKHR\n");
			return Status::Recreate;
		case vk::Result::eErrorUnknown:
			LOGF("vkAcquireNextImageKHR returned vk::Result::eErrorUnknown\n");
			return Status::Recreate;
		case vk::Result::eErrorSurfaceLostKHR:
			LOGF("vkAcquireNextImageKHR returned vk::Result::eErrorSurfaceLostKHR\n");
			return Status::SurfaceLost;
		default: EXIT("vkAcquireNextImageKHR failed: %s\n", VulkanToString(result).c_str());
	}
	EXIT_IF(m_image_index >= m_images.size());
	return Status::Success;
}

void Swapchain::RecordPresentCommands(CommandBuffer& command, VulkanImage& source) {
	if (source.state.layout != vk::ImageLayout::eTransferSrcOptimal) {
		EXIT("invalid prepared presentation image, vk_image=%p layout=%d\n",
		     static_cast<void*>(source.image), static_cast<int>(source.state.layout));
	}
	EXIT_IF(m_image_index >= m_images.size());
	auto vk_command = command.Handle();
	command.Begin();

	vk::ImageMemoryBarrier to_transfer {};
	to_transfer.sType                           = vk::StructureType::eImageMemoryBarrier;
	to_transfer.dstAccessMask                   = vk::AccessFlagBits::eTransferWrite;
	to_transfer.oldLayout                       = vk::ImageLayout::eUndefined;
	to_transfer.newLayout                       = vk::ImageLayout::eTransferDstOptimal;
	to_transfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_transfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_transfer.image                           = m_images[m_image_index];
	to_transfer.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
	to_transfer.subresourceRange.baseMipLevel   = 0;
	to_transfer.subresourceRange.levelCount     = 1;
	to_transfer.subresourceRange.baseArrayLayer = 0;
	to_transfer.subresourceRange.layerCount     = 1;
	vk_command.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
	                           vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags {}, 0,
	                           nullptr, 0, nullptr, 1, &to_transfer);

	if (m_fsr.IsReady()) {
		// FSR two-pass compute upscaler: EASU → RCAS.
		// Dispatch() handles all image transitions internally (source, intermediate, dest).
		m_fsr.Dispatch(vk_command, source, m_images[m_image_index], m_format,
		               source.extent.width, source.extent.height,
		               m_extent.width, m_extent.height,
		               Config::GetUpscalerSharpness());
	} else {
	// Clear the swapchain image to opaque black before the blit so that letterbox /
	// pillarbox bars (when an aspect-ratio mode shrinks the blit rectangle) render as
	// black instead of undefined content.
	if (Config::GetAspectRatio() != Config::AspectRatio::Stretch) {
		const vk::ClearColorValue black {};
		const vk::ImageSubresourceRange clear_range {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
		vk_command.clearColorImage(m_images[m_image_index],
		                           vk::ImageLayout::eTransferDstOptimal, &black, 1, &clear_range);
	}

	vk::ImageBlit region {};
	region.srcSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
	region.srcSubresource.mipLevel       = 0;
	region.srcSubresource.baseArrayLayer = 0;
	region.srcSubresource.layerCount     = 1;
	region.srcOffsets[1].x               = static_cast<int>(source.extent.width);
	region.srcOffsets[1].y               = static_cast<int>(source.extent.height);
	region.srcOffsets[1].z               = 1;
	region.dstSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
	region.dstSubresource.mipLevel       = 0;
	region.dstSubresource.baseArrayLayer = 0;
	region.dstSubresource.layerCount     = 1;
	// Compute the destination rectangle inside the swapchain extent according to the
	// configured aspect-ratio mode. dstOffsets[0] is the top-left, dstOffsets[1] the
	// bottom-right (exclusive). Anything outside the rectangle keeps the prior clear
	// colour, giving letterbox/pillarbox bars or a centred integer-scaled image.
	int dst_x0 = 0;
	int dst_y0 = 0;
	int dst_x1 = static_cast<int>(m_extent.width);
	int dst_y1 = static_cast<int>(m_extent.height);
	const auto sw = static_cast<int>(source.extent.width);
	const auto sh = static_cast<int>(source.extent.height);
	if (sw > 0 && sh > 0) {
		switch (Config::GetAspectRatio()) {
			case Config::AspectRatio::Fit16x9:
			case Config::AspectRatio::Fit4x3: {
				const int target_ar_num =
				    (Config::GetAspectRatio() == Config::AspectRatio::Fit16x9) ? 16 : 4;
				const int target_ar_den =
				    (Config::GetAspectRatio() == Config::AspectRatio::Fit16x9) ? 9 : 3;
				// Scale the source to fit inside the window while preserving its own
				// aspect ratio (the PS5 source is already the guest's chosen resolution).
				const int sw_ext = m_extent.width;
				const int sh_ext = m_extent.height;
				int fit_w = sw_ext;
				int fit_h = sw_ext * target_ar_den / target_ar_num;
				if (fit_h > sh_ext) {
					fit_h = sh_ext;
					fit_w = sh_ext * target_ar_num / target_ar_den;
				}
				dst_x0 = (sw_ext - fit_w) / 2;
				dst_y0 = (sh_ext - fit_h) / 2;
				dst_x1 = dst_x0 + fit_w;
				dst_y1 = dst_y0 + fit_h;
				break;
			}
			case Config::AspectRatio::Integer: {
				// Largest integer scale that still fits, centred. Uses the source
				// (guest) resolution as the base, so pixel-art stays crisp.
				const int scale = std::max(1, static_cast<int>(std::min(m_extent.width / sw, m_extent.height / sh)));
				const int fit_w = sw * scale;
				const int fit_h = sh * scale;
				dst_x0 = static_cast<int>(m_extent.width / 2u) - fit_w / 2;
				dst_y0 = static_cast<int>(m_extent.height / 2u) - fit_h / 2;
				dst_x1 = dst_x0 + fit_w;
				dst_y1 = dst_y0 + fit_h;
				break;
			}
			case Config::AspectRatio::Stretch:
			default: break; // leave the full-extent rectangle
		}
	}
	region.dstOffsets[0].x               = dst_x0;
	region.dstOffsets[0].y               = dst_y0;
	region.dstOffsets[0].z               = 0;
	region.dstOffsets[1].x               = dst_x1;
	region.dstOffsets[1].y               = dst_y1;
	region.dstOffsets[1].z               = 1;
	vk::Filter present_filter = vk::Filter::eLinear;
	switch (Config::GetPresentFilter()) {
		case Config::PresentFilter::Nearest: present_filter = vk::Filter::eNearest; break;
		case Config::PresentFilter::Cubic: {
			present_filter = vk::Filter::eLinear; // cubic requires an enabled extension; fall back
			static std::atomic<uint32_t> cubic_logged {0};
			if (cubic_logged.fetch_add(1, std::memory_order_relaxed) == 0) {
				LOGF_COLOR(Log::Color::Yellow,
				           "Present: cubic filter requested but the cubic-filter extension is not "
				           "enabled at device creation; falling back to linear\n");
			}
			break;
		}
		case Config::PresentFilter::Linear:
		default: present_filter = vk::Filter::eLinear; break;
	}

	vk_command.blitImage(source.image, vk::ImageLayout::eTransferSrcOptimal,
	                     m_images[m_image_index], vk::ImageLayout::eTransferDstOptimal, 1, &region,
	                     present_filter);

	vk::ImageMemoryBarrier to_present {};
	to_present.sType                           = vk::StructureType::eImageMemoryBarrier;
	to_present.srcAccessMask                   = vk::AccessFlagBits::eTransferWrite;
	to_present.dstAccessMask                   = vk::AccessFlagBits::eMemoryRead;
	to_present.oldLayout                       = vk::ImageLayout::eTransferDstOptimal;
	to_present.newLayout                       = vk::ImageLayout::ePresentSrcKHR;
	to_present.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_present.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_present.image                           = m_images[m_image_index];
	to_present.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
	to_present.subresourceRange.baseMipLevel   = 0;
	to_present.subresourceRange.levelCount     = 1;
	to_present.subresourceRange.baseArrayLayer = 0;
	to_present.subresourceRange.layerCount     = 1;
	vk_command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                           vk::PipelineStageFlagBits::eAllCommands,
	                           vk::DependencyFlagBits::eByRegion, 0,
	                           nullptr, 0, nullptr, 1, &to_present);
	} // end blit fallback
	command.End();
}

void Swapchain::Submit(CommandBuffer& command) {
	EXIT_IF(m_frame_index >= m_image_acquired.size() || m_image_index >= m_render_complete.size());
	SubmitInfo submit;
	submit.AddWait(m_image_acquired[m_frame_index], 1, vk::PipelineStageFlagBits::eTransfer);
	submit.AddSignal(m_render_complete[m_image_index]);
	command.Execute(submit);
}

Swapchain::Status Swapchain::Present() {
	EXIT_IF(m_image_index >= m_render_complete.size());
	const auto ready = m_render_complete[m_image_index];
	vk::PresentInfoKHR present {};
	present.sType              = vk::StructureType::ePresentInfoKHR;
	present.swapchainCount     = 1;
	present.pSwapchains        = &m_handle;
	present.pImageIndices      = &m_image_index;
	present.pWaitSemaphores    = &ready;
	present.waitSemaphoreCount = 1;

	vk::Result result;
	{
		Common::LockGuard lock(m_window.graphic_ctx.queue_mutex);
		result = m_window.graphic_ctx.queue.presentKHR(&present);
	}
	switch (result) {
		case vk::Result::eSuccess: break;
		case vk::Result::eSuboptimalKHR:
			LOGF("vkQueuePresentKHR returned vk::Result::eSuboptimalKHR\n");
			return Status::Recreate;
		case vk::Result::eErrorOutOfDateKHR:
			LOGF("vkQueuePresentKHR returned vk::Result::eErrorOutOfDateKHR\n");
			return Status::Recreate;
		case vk::Result::eErrorSurfaceLostKHR:
			LOGF("vkQueuePresentKHR returned vk::Result::eErrorSurfaceLostKHR\n");
			return Status::SurfaceLost;
		default: EXIT("vkQueuePresentKHR failed: %s\n", VulkanToString(result).c_str());
	}
	m_frame_index = (m_frame_index + 1u) % static_cast<uint32_t>(m_images.size());
	return Status::Success;
}

std::string Swapchain::CaptureScreenshot() {
	auto& graphics = m_window.graphic_ctx;
	if (m_handle == nullptr || m_image_index >= m_images.size()) {
		LOGF("Screenshot: swapchain not ready, skipping\n");
		return {};
	}
	if (m_extent.width == 0 || m_extent.height == 0) {
		LOGF("Screenshot: zero-sized swapchain, skipping\n");
		return {};
	}

	const uint32_t w = m_extent.width;
	const uint32_t h = m_extent.height;
	const vk::Format src_format = m_format;
	// Only the common UNORM swapchain formats are handled. Both are 4 bytes/pixel; flip
	// channels into RGBA8 for stb_image_write below.
	if (src_format != vk::Format::eB8G8R8A8Unorm && src_format != vk::Format::eR8G8B8A8Unorm) {
		LOGF("Screenshot: unsupported swapchain format %d, skipping\n",
		     static_cast<int>(src_format));
		return {};
	}

	// Host-visible staging buffer for the readback.
	VulkanBuffer staging {};
	staging.usage = vk::BufferUsageFlagBits::eTransferDst;
	staging.memory.property =
	    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	const vk::DeviceSize row_pitch = static_cast<vk::DeviceSize>(w) * 4u;
	const vk::DeviceSize buf_size  = row_pitch * static_cast<vk::DeviceSize>(h);
	graphics.CreateBuffer(buf_size, staging);
	if (staging.buffer == nullptr) {
		LOGF("Screenshot: failed to allocate staging buffer\n");
		return {};
	}

	// One-shot command buffer for the image -> buffer copy.
	CommandScheduler scheduler(*m_window.render_context, graphics);
	CommandBuffer   command(scheduler);
	command.WaitForFenceAndReset();
	auto vk_command = command.Handle();
	command.Begin();

	// PresentSrcKHR -> TransferSrcOptimal
	vk::ImageMemoryBarrier to_src {};
	to_src.sType                           = vk::StructureType::eImageMemoryBarrier;
	to_src.srcAccessMask                   = vk::AccessFlagBits::eMemoryRead;
	to_src.dstAccessMask                   = vk::AccessFlagBits::eTransferRead;
	to_src.oldLayout                       = vk::ImageLayout::ePresentSrcKHR;
	to_src.newLayout                       = vk::ImageLayout::eTransferSrcOptimal;
	to_src.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_src.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_src.image                           = m_images[m_image_index];
	to_src.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
	to_src.subresourceRange.baseMipLevel   = 0;
	to_src.subresourceRange.levelCount     = 1;
	to_src.subresourceRange.baseArrayLayer = 0;
	to_src.subresourceRange.layerCount     = 1;
	vk_command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                           vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags {}, 0,
	                           nullptr, 0, nullptr, 1, &to_src);

	vk::BufferImageCopy region {};
	region.bufferOffset                    = 0;
	region.bufferRowLength                 = w; // tight packing
	region.bufferImageHeight               = h;
	region.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
	region.imageSubresource.mipLevel       = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount     = 1;
	region.imageOffset                     = vk::Offset3D {0, 0, 0};
	region.imageExtent                     = vk::Extent3D {w, h, 1};
	vk_command.copyImageToBuffer(m_images[m_image_index],
	                             vk::ImageLayout::eTransferSrcOptimal, staging.buffer, 1,
	                             &region);

	// TransferSrcOptimal -> PresentSrcKHR (restore so the next Present works)
	vk::ImageMemoryBarrier to_present {};
	to_present.sType                           = vk::StructureType::eImageMemoryBarrier;
	to_present.srcAccessMask                   = vk::AccessFlagBits::eTransferRead;
	to_present.dstAccessMask                   = vk::AccessFlagBits::eMemoryRead;
	to_present.oldLayout                       = vk::ImageLayout::eTransferSrcOptimal;
	to_present.newLayout                       = vk::ImageLayout::ePresentSrcKHR;
	to_present.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_present.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	to_present.image                           = m_images[m_image_index];
	to_present.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
	to_present.subresourceRange.baseMipLevel   = 0;
	to_present.subresourceRange.levelCount     = 1;
	to_present.subresourceRange.baseArrayLayer = 0;
	to_present.subresourceRange.layerCount     = 1;
	vk_command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
	                           vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags {}, 0,
	                           nullptr, 0, nullptr, 1, &to_present);

	command.End();
	command.Execute();
	command.WaitForFenceOnly();

	// Read back and convert to RGBA8.
	void* mapped = nullptr;
	graphics.MapMemory(staging.memory, mapped);
	EXIT_IF(mapped == nullptr);

	std::vector<uint8_t> rgba(static_cast<size_t>(buf_size));
	std::memcpy(rgba.data(), mapped, static_cast<size_t>(buf_size));
	graphics.UnmapMemory(staging.memory);

	if (src_format == vk::Format::eB8G8R8A8Unorm) {
		for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
			std::swap(rgba[i + 0], rgba[i + 2]); // B -> R, R -> B
		}
	}

	graphics.DeleteBuffer(staging);

	// Write the PNG into the configured screenshot folder with a timestamped name.
	namespace fs = std::filesystem;
	const fs::path folder = Config::GetScreenshotFolder();
	std::error_code ec {};
	fs::create_directories(folder, ec);
	if (ec) {
		LOGF("Screenshot: failed to create folder '%s': %s\n", folder.string().c_str(),
		     ec.message().c_str());
		return {};
	}

	const auto now = std::chrono::system_clock::now();
	const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
		                 now.time_since_epoch()) % 1000;
	const auto t_c = std::chrono::system_clock::to_time_t(now);
	std::tm tm {};
#ifdef _WIN32
	localtime_s(&tm, &t_c);
#else
	localtime_r(&t_c, &tm);
#endif
	const auto filename =
	    fmt::format("kyty_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}_{:03d}.png",
	                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
	                tm.tm_sec, static_cast<int>(ms.count()));
	const fs::path full = folder / filename;

	const int ok = stbi_write_png(full.string().c_str(), static_cast<int>(w),
	                              static_cast<int>(h), 4, rgba.data(),
	                              static_cast<int>(row_pitch));
	if (ok == 0) {
		LOGF("Screenshot: stbi_write_png failed for '%s'\n", full.string().c_str());
		return {};
	}

	LOGF_COLOR(Log::Color::Green, "Screenshot saved: %s (%ux%u)\n",
	           full.string().c_str(), w, h);
	return full.string();
}

Presenter::Presenter(WindowContext& window): m_impl(std::make_unique<Impl>(window)) {}

Presenter::~Presenter() = default;

Presenter::Frame& Presenter::PrepareFrame(CommandBuffer& buffer, const ImageInfo& info) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(buffer.IsInvalid());
	auto* frame = m_impl->frames.Acquire();
	Common::LockGuard render_lock(m_impl->renderer.GetMutex());
	auto&             image = m_impl->ResolveSurface(info);
	if (image.backing.format == vk::Format::eUndefined) {
		EXIT("unsupported presentation source, image=%p\n", static_cast<const void*>(&image));
	}

	auto frame_format = info.pixel_format;
	switch (frame_format) {
		case vk::Format::eR8G8B8A8Srgb: frame_format = vk::Format::eR8G8B8A8Unorm; break;
		case vk::Format::eB8G8R8A8Srgb: frame_format = vk::Format::eB8G8R8A8Unorm; break;
		default: break;
	}
	frame->Configure(m_impl->window.graphic_ctx,
	                 {image.backing.extent.width, image.backing.extent.height},
	                 frame_format);
	frame->CopyFrom(buffer, image);
	return *frame;
}

Presenter::Frame& Presenter::PrepareBlankFrame(uint32_t width, uint32_t height, bool opaque,
                                                CommandBuffer* producer) {
	KYTY_PROFILER_FUNCTION();
	auto              format = m_impl->frames.GetFormat();
	auto*             frame  = m_impl->frames.Acquire();
	Common::LockGuard render_lock(m_impl->renderer.GetMutex());
	frame->Configure(m_impl->window.graphic_ctx, {width, height}, format);
	vk::ClearColorValue clear {};
	clear.float32[3] = opaque ? 1.0f : 0.0f;
	if (producer != nullptr) {
		EXIT_IF(producer->IsInvalid());
		frame->Clear(*producer, clear);
	} else {
		if (frame->present_commands == nullptr) {
			frame->present_commands =
			    std::make_unique<CommandBuffer>(m_impl->present_scheduler);
		}
		auto& command = *frame->present_commands;
		command.WaitForFenceAndReset();
		command.Begin();
		frame->Clear(command, clear);
		command.End();
		command.Execute();
	}
	return *frame;
}

Presenter::Frame* Presenter::PrepareLastFrame() {
	return m_impl->frames.AcquireLast();
}

bool Presenter::IsGuestPaused() const noexcept {
	return m_impl->window.loop.paused.load(std::memory_order_acquire);
}

RenderContext& Presenter::Renderer() const noexcept {
	return m_impl->renderer;
}

void Presenter::Present(Frame& frame, bool reuse) {
	KYTY_PROFILER_FUNCTION();
	m_impl->frames.ValidateForPresent(&frame, reuse);

	auto& window = m_impl->window;
	// Skip presenting while minimized instead of churning the swapchain against a 0x0 surface.
	if (window.window_minimized) {
		m_impl->frames.Release(&frame, reuse);
		return;
	}
	if (window.window_hidden) {
#if defined(__APPLE__)
		// AppKit traps if a window is shown off the main thread; marshal and wait so the
		// swapchain below is recreated against a visible window.
		window.RunOnMainThread(
		    [&window] {
			    window.UpdateIcon();
			    SDL_ShowWindow(window.window);
		    },
		    true);
#else
		window.UpdateIcon();

		SDL_ShowWindow(window.window);
#endif

		window.window_hidden = false;
		m_impl->RecoverSwapchain(Swapchain::Status::Recreate);
	}

	auto& swapchain = m_impl->swapchain;
	for (uint32_t attempt = 0; attempt < 2; attempt++) {
		auto status = swapchain.AcquireNextImage();
		if (status != Swapchain::Status::Success) {
			m_impl->RecoverSwapchain(status);
			continue;
		}
		if (frame.present_commands == nullptr) {
			frame.present_commands =
			    std::make_unique<CommandBuffer>(m_impl->present_scheduler);
		}
		{
			Common::LockGuard render_lock(m_impl->renderer.GetMutex());
			frame.present_commands->WaitForFenceAndReset();
			auto& command = *frame.present_commands;
			swapchain.RecordPresentCommands(command, frame.image);
			swapchain.Submit(command);
		}
		status = swapchain.Present();
		if (status != Swapchain::Status::Success) {
			m_impl->RecoverSwapchain(status);
			continue;
		}

		RenderDocOnPresent();
		window.UpdateTitle();
		m_impl->frames.Release(&frame, true);
		return;
	}
	LOGF("Vulkan presentation retry exhausted; dropping frame\n");
	m_impl->frames.Release(&frame, reuse);
}

void Presenter::Discard(Frame& frame) {
	m_impl->frames.Release(&frame);
}

std::string Presenter::CaptureScreenshot() {
	return m_impl->swapchain.CaptureScreenshot();
}

WindowContext::WindowContext() = default;

} // namespace Libs::Graphics
