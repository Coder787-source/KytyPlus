#include "common/assert.h"
#include "common/common.h"
#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "common/threads.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/descriptorCache.h"
#include "graphics/host_gpu/renderer/framebufferCache.h"
#include "graphics/host_gpu/renderer/imageView.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/transfer.h"
#include "graphics/host_gpu/vma.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <memory>
namespace Libs::Graphics {
static std::atomic<uint64_t> g_command_buffer_submit_seq = 0;

static void ResetNativeCommandBuffer(vk::CommandBuffer buffer) {
	EXIT_IF(buffer == nullptr);
	const auto result = buffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
	if (result != vk::Result::eSuccess) {
		EXIT("failed to reset Vulkan command buffer: %s (%d)\n", VulkanToString(result).c_str(),
		     static_cast<int>(result));
	}
}

struct CommandSlot {
	Common::Mutex*    pool_mutex = nullptr;
	uint32_t          id         = 0;
	vk::CommandBuffer buffer     = nullptr;
	vk::Fence         fence      = nullptr;
	bool              busy       = false;
};

class ThreadCommandPool {
public:
	ThreadCommandPool() = default;

	KYTY_CLASS_NO_COPY(ThreadCommandPool);

	CommandSlot* Allocate();
	void         Destroy();

private:
	void         Create();
	CommandSlot* CreateSlot();

	Common::Mutex           m_mutex;
	vk::CommandPool         m_pool = nullptr;
	std::deque<CommandSlot> m_slots;
};

static RenderContext*                 g_render_ctx = nullptr;
static thread_local ThreadCommandPool g_command_pool;

RenderContext& GetRenderContext() noexcept {
	return *g_render_ctx;
}

FenceResourceRetainer::~FenceResourceRetainer() {
	if (!m_resources.empty()) {
		EXIT("fence resource retainer destroyed before release\n");
	}
}

void FenceResourceRetainer::Retain(std::shared_ptr<void> resource) {
	if (resource == nullptr) {
		EXIT("cannot retain a null fence resource\n");
	}
	if (std::ranges::none_of(m_resources, [&resource](const auto& retained) {
		    return retained.get() == resource.get();
	    })) {
		m_resources.push_back(std::move(resource));
	}
}

void FenceResourceRetainer::ReleaseAfterFence() noexcept {
	m_resources.clear();
}

void GraphicsRenderInit(GraphicContext& graphics) {
	g_render_ctx = new RenderContext(graphics);
}

void GraphicsRenderReleaseThreadCommandPool() {
	g_command_pool.Destroy();
}

CommandBuffer::CommandBuffer()
    : m_graphics(GetRenderContext().GetGraphics()), m_slot(g_command_pool.Allocate()),
      m_host_stream(m_graphics) {}

void ThreadCommandPool::Create() {
	auto& graphics = GetRenderContext().GetGraphics();
	EXIT_IF(m_pool != nullptr || graphics.queue_family == static_cast<uint32_t>(-1));

	vk::CommandPoolCreateInfo pool_info {};
	pool_info.sType            = vk::StructureType::eCommandPoolCreateInfo;
	pool_info.pNext            = nullptr;
	pool_info.queueFamilyIndex = graphics.queue_family;
	pool_info.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	const auto result = graphics.device.createCommandPool(&pool_info, nullptr, &m_pool);
	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess || m_pool == nullptr);
}

CommandSlot* ThreadCommandPool::CreateSlot() {
	auto&                         graphics = GetRenderContext().GetGraphics();
	vk::CommandBufferAllocateInfo alloc_info {};
	alloc_info.sType              = vk::StructureType::eCommandBufferAllocateInfo;
	alloc_info.commandPool        = m_pool;
	alloc_info.level              = vk::CommandBufferLevel::ePrimary;
	alloc_info.commandBufferCount = 1;

	vk::CommandBuffer buffer = nullptr;
	if (graphics.device.allocateCommandBuffers(&alloc_info, &buffer) != vk::Result::eSuccess) {
		EXIT("Can't allocate command buffers");
	}

	vk::FenceCreateInfo fence_info {};
	fence_info.sType = vk::StructureType::eFenceCreateInfo;
	fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

	vk::Fence fence = nullptr;
	if (graphics.device.createFence(&fence_info, nullptr, &fence) != vk::Result::eSuccess) {
		graphics.device.freeCommandBuffers(m_pool, 1, &buffer);
		EXIT("Can't create fence");
	}
	auto& slot      = m_slots.emplace_back();
	slot.pool_mutex = &m_mutex;
	slot.id         = static_cast<uint32_t>(m_slots.size() - 1);
	slot.buffer     = buffer;
	slot.fence      = fence;
	return &slot;
}

CommandSlot* ThreadCommandPool::Allocate() {
	Common::LockGuard lock(m_mutex);
	if (m_pool == nullptr) {
		Create();
	}
	auto  it   = std::ranges::find_if(m_slots, [](const auto& slot) { return !slot.busy; });
	auto* slot = it != m_slots.end() ? &*it : CreateSlot();
	slot->busy = true;
	ResetNativeCommandBuffer(slot->buffer);
	return slot;
}

void ThreadCommandPool::Destroy() {
	Common::LockGuard lock(m_mutex);
	if (m_pool == nullptr) {
		return;
	}
	EXIT_IF(std::ranges::any_of(m_slots, [](const auto& slot) { return slot.busy; }));
	auto& graphics = GetRenderContext().GetGraphics();
	for (const auto& slot: m_slots) {
		graphics.device.destroyFence(slot.fence, nullptr);
	}
	graphics.device.destroyCommandPool(m_pool, nullptr);
	m_slots.clear();
	m_pool = nullptr;
}

bool CommandBuffer::IsInvalid() const {
	return m_slot == nullptr;
}

vk::CommandBuffer CommandBuffer::Handle() const {
	EXIT_IF(IsInvalid());

	const auto handle = m_slot->buffer;
	EXIT_IF(handle == nullptr);
	return handle;
}

void CommandBuffer::Release() {
	EXIT_IF(IsInvalid());

	Common::LockGuard lock(*m_slot->pool_mutex);

	WaitForFence();

	m_host_stream.Release();

	m_slot->busy = false;
	ResetNativeCommandBuffer(m_slot->buffer);
	ReleaseResourcesAfterFence();
	m_slot = nullptr;

	EXIT_NOT_IMPLEMENTED(!IsInvalid());
}

void CommandBuffer::DeleteAfterFence(VulkanBuffer& buffer) {
	m_delete_after_fence.push_back(&buffer);
}

void CommandBuffer::RetainResourceUntilFence(std::shared_ptr<void> resource) {
	if (IsInvalid() || m_execute) {
		EXIT("cannot retain a resource on an invalid or submitted command buffer\n");
	}
	m_fence_resources.Retain(std::move(resource));
}

void CommandBuffer::RecycleDescriptorAfterFence(VulkanDescriptorSet& set) {
	m_descriptor_sets_after_fence.push_back(&set);
}

void CommandBuffer::RecycleDescriptorsAfterFence() {
	for (auto* set: m_descriptor_sets_after_fence) {
		GetRenderContext().GetDescriptorCache().Recycle(*set);
	}
	m_descriptor_sets_after_fence.clear();
}

void CommandBuffer::Begin() const {
	auto buffer = Handle();

	vk::CommandBufferBeginInfo begin_info {};
	begin_info.sType            = vk::StructureType::eCommandBufferBeginInfo;
	begin_info.pNext            = nullptr;
	begin_info.flags            = {};
	begin_info.pInheritanceInfo = nullptr;

	auto result = buffer.begin(&begin_info);

	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);
}

void CommandBuffer::End() const {
	auto buffer = Handle();

	auto result = buffer.end();

	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);
}

void CommandBuffer::SetDebugInfo(uint32_t op, uint64_t submit_id, uint32_t arg0, uint32_t arg1,
                                 uint32_t arg2, uint32_t arg3, uint64_t arg4) {
	m_debug_op        = op;
	m_debug_submit_id = submit_id;
	m_debug_arg0      = arg0;
	m_debug_arg1      = arg1;
	m_debug_arg2      = arg2;
	m_debug_arg3      = arg3;
	m_debug_arg4      = arg4;
}

void CommandBuffer::Execute() {
	Submit(nullptr, {}, nullptr);
}

void CommandBuffer::ExecuteWithSemaphore(vk::Semaphore          wait_semaphore,
                                         vk::PipelineStageFlags wait_stage,
                                         vk::Semaphore          signal_semaphore) {
	EXIT_IF(wait_semaphore == nullptr || signal_semaphore == nullptr);
	Submit(wait_semaphore, wait_stage, signal_semaphore);
}

void CommandBuffer::Submit(vk::Semaphore wait_semaphore, vk::PipelineStageFlags wait_stage,
                           vk::Semaphore signal_semaphore) {
	EXIT_IF(IsInvalid());
	EXIT_IF(m_execute);

	const bool has_wait   = wait_semaphore != nullptr;
	const bool has_signal = signal_semaphore != nullptr;
	auto       buffer     = Handle();
	auto       fence      = m_slot->fence;

	vk::SubmitInfo submit_info {};
	submit_info.sType                = vk::StructureType::eSubmitInfo;
	submit_info.pNext                = nullptr;
	submit_info.waitSemaphoreCount   = has_wait ? 1u : 0u;
	submit_info.pWaitSemaphores      = has_wait ? &wait_semaphore : nullptr;
	submit_info.pWaitDstStageMask    = has_wait ? &wait_stage : nullptr;
	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &buffer;
	submit_info.signalSemaphoreCount = has_signal ? 1u : 0u;
	submit_info.pSignalSemaphores    = has_signal ? &signal_semaphore : nullptr;

	auto& graphics = GetRenderContext().GetGraphics();
	EXIT_IF(graphics.queue == nullptr);

	auto result = graphics.device.resetFences(1, &fence);
	if (result != vk::Result::eSuccess) {
		LOGF("vkResetFences failed before submit: %s (%d)\n", VulkanToString(result).c_str(),
		     static_cast<int>(result));
	}
	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);

	if (Config::GraphicsDebugDumpEnabled()) {
		LOGF("vkQueueSubmit begin: slot=%u wait_semaphore=%p signal_semaphore=%p"
		     " debug_op=%u debug_submit=%" PRIu64 " args=%u,%u,%u,%u,0x%016" PRIx64 "\n",
		     m_slot->id, static_cast<void*>(wait_semaphore), static_cast<void*>(signal_semaphore),
		     m_debug_op, m_debug_submit_id, m_debug_arg0, m_debug_arg1, m_debug_arg2, m_debug_arg3,
		     m_debug_arg4);
	}

	{
		Common::LockGuard lock(graphics.queue_mutex);
		m_submit_seq = g_command_buffer_submit_seq.fetch_add(1, std::memory_order_relaxed) + 1;
		result       = graphics.queue.submit(1, &submit_info, fence);
	}

	m_execute      = true;
	m_fence_waited = false;

	if (result != vk::Result::eSuccess) {
		LOGF("vkQueueSubmit failed: %s (%d), slot=%u submit_seq=%" PRIu64
		     " debug_op=%u debug_submit=%" PRIu64 " args=%u,%u,%u,%u,0x%016" PRIx64 "\n",
		     VulkanToString(result).c_str(), static_cast<int>(result), m_slot->id, m_submit_seq,
		     m_debug_op, m_debug_submit_id, m_debug_arg0, m_debug_arg1, m_debug_arg2, m_debug_arg3,
		     m_debug_arg4);
	}
	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);
}

void CommandBuffer::WaitForFence() {
	FinalizeFence(false);
}

void CommandBuffer::WaitForFenceOnly() {
	EXIT_IF(IsInvalid());
	if (!m_execute || m_fence_waited) {
		return;
	}
	auto device = GetRenderContext().GetGraphics().device;
	auto result = device.waitForFences(1, &m_slot->fence, VK_TRUE, UINT64_MAX);
	if (result != vk::Result::eSuccess) {
		LOGF("vkWaitForFences failed: %s (%d), slot=%u submit_seq=%" PRIu64
		     " debug_op=%u debug_submit=%" PRIu64 " args=%u,%u,%u,%u,0x%016" PRIx64 "\n",
		     VulkanToString(result).c_str(), static_cast<int>(result), m_slot->id, m_submit_seq,
		     m_debug_op, m_debug_submit_id, m_debug_arg0, m_debug_arg1, m_debug_arg2, m_debug_arg3,
		     m_debug_arg4);
	}
	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);
	m_fence_waited = true;
}

void CommandBuffer::WaitForFenceAndReset() {
	FinalizeFence(true);
}

void CommandBuffer::FinalizeFence(bool reset_recording) {
	const bool was_executed = m_execute;
	WaitForFenceOnly();
	if (was_executed) {
		m_execute      = false;
		m_fence_waited = false;
		if (reset_recording) {
			ResetNativeCommandBuffer(m_slot->buffer);
			m_recording_generation++;
		}
	}
	if (reset_recording) {
		m_host_stream.Reset();
	}
	if (was_executed) {
		ReleaseResourcesAfterFence();
	}
	DeleteBuffersAfterFence();
}

void CommandBuffer::ReleaseResourcesAfterFence() {
	RecycleDescriptorsAfterFence();
	m_fence_resources.ReleaseAfterFence();
}

void CommandBuffer::DeleteBuffersAfterFence() {
	for (auto* buffer: m_delete_after_fence) {
		GetRenderContext().GetGraphics().DeleteBuffer(*buffer);
		delete buffer;
	}
	m_delete_after_fence.clear();
}

void CommandBuffer::BeginRenderPass(VulkanFramebuffer& framebuffer, RenderColorInfo* colors,
                                    uint32_t requested_color_count, RenderDepthInfo& depth) const {
	auto buffer = Handle();

	EXIT_IF(colors == nullptr);
	EXIT_IF(requested_color_count > RENDER_COLOR_ATTACHMENTS_MAX);

	bool with_depth = (depth.format != vk::Format::eUndefined && depth.vulkan_buffer != nullptr);
	uint32_t color_count = 0;
	for (uint32_t i = 0; i < requested_color_count; i++) {
		if (colors[i].vulkan_buffer == nullptr) {
			break;
		}
		color_count++;
	}
	bool with_color = (color_count != 0);

	EXIT_NOT_IMPLEMENTED(!with_depth && !with_color);

	vk::ClearValue clears[RENDER_COLOR_ATTACHMENTS_MAX + 1] = {};
	for (uint32_t i = 0; i < color_count; i++) {
		clears[i].color = colors[i].color_clear_value;
	}
	clears[color_count].depthStencil = {depth.depth_clear_value, depth.stencil_clear_value};

	vk::Extent2D extent = (with_color ? colors[0].extent : depth.vulkan_buffer->extent);

	vk::RenderPassBeginInfo render_pass_info {};
	render_pass_info.sType             = vk::StructureType::eRenderPassBeginInfo;
	render_pass_info.pNext             = nullptr;
	render_pass_info.renderPass        = framebuffer.render_pass;
	render_pass_info.framebuffer       = framebuffer.framebuffer;
	render_pass_info.renderArea.offset = {0, 0};
	render_pass_info.renderArea.extent = extent;
	render_pass_info.clearValueCount   = color_count + (with_depth ? 1u : 0u);
	render_pass_info.pClearValues      = clears;

	for (uint32_t i = 0; i < color_count; i++) {
		const auto color_initial_layout = framebuffer.color_layout[i];
		if (colors[i].vulkan_buffer->layout != color_initial_layout) {
			if (graphics_debug_dump_enabled()) {
				LOGF("BeginRenderPass: color%u initial barrier image=%p mem=%" PRIu64 " %s -> %s\n",
				     i, VulkanHandleToPointer(colors[i].vulkan_buffer->image),
				     colors[i].vulkan_buffer->memory.unique_id,
				     VulkanToString(colors[i].vulkan_buffer->layout).c_str(),
				     VulkanToString(color_initial_layout).c_str());
			}

			vk::ImageMemoryBarrier image_memory_barrier {};
			image_memory_barrier.sType               = vk::StructureType::eImageMemoryBarrier;
			image_memory_barrier.pNext               = nullptr;
			image_memory_barrier.srcAccessMask       = {};
			image_memory_barrier.dstAccessMask       = vk::AccessFlagBits::eColorAttachmentRead |
			                                           vk::AccessFlagBits::eColorAttachmentWrite;
			image_memory_barrier.oldLayout           = colors[i].vulkan_buffer->layout;
			image_memory_barrier.newLayout           = color_initial_layout;
			image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_memory_barrier.image               = colors[i].vulkan_buffer->image;
			image_memory_barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
			image_memory_barrier.subresourceRange.baseMipLevel   = 0;
			image_memory_barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
			image_memory_barrier.subresourceRange.baseArrayLayer = 0;
			image_memory_barrier.subresourceRange.layerCount     = colors[i].vulkan_buffer->layers;

			buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
			                       vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::DependencyFlags {}, 0, nullptr, 0, nullptr, 1,
			                       &image_memory_barrier);

			colors[i].vulkan_buffer->layout = image_memory_barrier.newLayout;
		} else if (graphics_debug_dump_enabled()) {
			LOGF("BeginRenderPass: color%u initial image=%p mem=%" PRIu64 " layout=%s\n", i,
			     VulkanHandleToPointer(colors[i].vulkan_buffer->image),
			     colors[i].vulkan_buffer->memory.unique_id,
			     VulkanToString(colors[i].vulkan_buffer->layout).c_str());
		}
	}

	const auto depth_layout =
	    with_depth ? framebuffer.depth_layout : vk::ImageLayout::eDepthStencilAttachmentOptimal;

	if (with_depth && depth.vulkan_buffer->layout != depth_layout) {
		vk::ImageMemoryBarrier image_memory_barrier {};
		image_memory_barrier.sType = vk::StructureType::eImageMemoryBarrier;
		image_memory_barrier.pNext = nullptr;
		image_memory_barrier.srcAccessMask =
		    vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
		image_memory_barrier.dstAccessMask =
		    (depth_layout == vk::ImageLayout::eDepthStencilReadOnlyOptimal
		         ? vk::AccessFlagBits::eMemoryRead
		         : vk::AccessFlagBits::eMemoryWrite);
		image_memory_barrier.oldLayout           = depth.vulkan_buffer->layout;
		image_memory_barrier.newLayout           = depth_layout;
		image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.image               = depth.vulkan_buffer->image;
		image_memory_barrier.subresourceRange.aspectMask =
		    ImageViewOps::DepthAspectMask(depth.vulkan_buffer->format);
		image_memory_barrier.subresourceRange.baseMipLevel   = 0;
		image_memory_barrier.subresourceRange.levelCount     = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount     = depth.vulkan_buffer->layers;

		buffer.pipelineBarrier(
		    vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader,
		    vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader,
		    vk::DependencyFlags {}, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

		depth.vulkan_buffer->layout = image_memory_barrier.newLayout;
	}

	buffer.beginRenderPass(&render_pass_info, vk::SubpassContents::eInline);

	for (uint32_t i = 0; i < color_count; i++) {
		colors[i].vulkan_buffer->layout = RENDER_COLOR_IMAGE_LAYOUT;
		if (colors[i].vulkan_buffer->type == VulkanImageType::RenderTexture) {
			static_cast<RenderTextureVulkanImage*>(colors[i].vulkan_buffer)->initial_clear_pending =
			    false;
		}
	}
	if (with_depth) {
		depth.vulkan_buffer->initial_depth_clear_pending   = false;
		depth.vulkan_buffer->initial_stencil_clear_pending = false;
	}
}

void CommandBuffer::EndRenderPass() const {
	auto buffer = Handle();

	buffer.endRenderPass();
}

} // namespace Libs::Graphics
