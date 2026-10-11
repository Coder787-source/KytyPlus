#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GRAPHICSRENDER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GRAPHICSRENDER_H_

#include "common/abi.h"
#include "common/assert.h"
#include "common/common.h"
#include "graphics/host_gpu/renderer/pipeline/descriptorCache.h"
#include "graphics/host_gpu/renderer/renderTarget.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <array>
#include <memory>
#include <vector>

namespace Libs::Graphics {

namespace HW {
class Context;
class UserConfig;
class Shader;
} // namespace HW

struct GraphicContext;
struct ShaderBufferResource;
struct ShaderComputeInputInfo;
struct CommandSlot;
struct VulkanBuffer;
struct VulkanDescriptorSet;
struct RenderDepthInfo;
struct RenderColorInfo;
struct DrawCallInfo;
struct DrawEmitInfo;
struct DrawIndexBufferSource;

// Guest-side indirect draw arguments, consumed on the GPU via vkCmdDraw[Indexed]Indirect.
struct DrawIndirectSource {
	bool     enabled      = false;
	uint64_t args_vaddr   = 0;
	uint32_t draw_count   = 1;
	uint32_t stride_bytes = 20;
	bool     indexed      = true;
};
struct DrawRenderState;
class RenderContext;
class CommandScheduler;
struct RenderExecutorTestAccess;

enum class CommandBufferDebugOp : uint32_t {
	DispatchDirect,
	DrawIndex,
	DrawIndexAuto,
	EopWrite,
	EopInterrupt,
	EopWriteBack,
	EopFlip,
	EopWriteBackFlip,
	EopOnlyFlip,
	Unknown,
};

class FenceResourceRetainer {
public:
	FenceResourceRetainer() = default;
	~FenceResourceRetainer();
	KYTY_CLASS_NO_COPY(FenceResourceRetainer);

	void               Retain(std::shared_ptr<void> resource);
	void               ReleaseAfterFence() noexcept;
	[[nodiscard]] bool Empty() const noexcept { return m_resources.empty(); }

private:
	std::vector<std::shared_ptr<void>> m_resources;
};

struct SubmitInfo {
	static constexpr uint32_t MaxSemaphores = 3;

	std::array<vk::Semaphore, MaxSemaphores>          wait_semaphores {};
	std::array<uint64_t, MaxSemaphores>               wait_ticks {};
	std::array<vk::PipelineStageFlags, MaxSemaphores> wait_stages {};
	std::array<vk::Semaphore, MaxSemaphores>          signal_semaphores {};
	std::array<uint64_t, MaxSemaphores>               signal_ticks {};
	uint32_t                                          num_wait_semaphores   = 0;
	uint32_t                                          num_signal_semaphores = 0;

	void AddWait(vk::Semaphore semaphore, uint64_t tick = 1,
	             vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eAllCommands) {
		EXIT_IF(semaphore == nullptr || num_wait_semaphores >= MaxSemaphores);
		wait_semaphores[num_wait_semaphores] = semaphore;
		wait_ticks[num_wait_semaphores]      = tick;
		wait_stages[num_wait_semaphores++]   = stage;
	}

	void AddSignal(vk::Semaphore semaphore, uint64_t tick = 1) {
		EXIT_IF(semaphore == nullptr || num_signal_semaphores >= MaxSemaphores);
		signal_semaphores[num_signal_semaphores] = semaphore;
		signal_ticks[num_signal_semaphores++]    = tick;
	}
};

class CommandBuffer {
public:
	explicit CommandBuffer(CommandScheduler& scheduler);
	~CommandBuffer();

	KYTY_CLASS_NO_COPY(CommandBuffer);

	[[nodiscard]] bool IsInvalid() const;

	void Begin() const;
	void End() const;
	void Execute(const SubmitInfo& submit = {});
	void SetDebugInfo(uint32_t op, uint64_t submit_id, uint32_t arg0 = 0, uint32_t arg1 = 0,
	                  uint32_t arg2 = 0, uint32_t arg3 = 0, uint64_t arg4 = 0);
	void BeginRendering(const RenderState& state) const;
	void EndRendering() const;
	// KytyPlus: true while a dynamic-rendering pass is open (between BeginRendering and
	// EndRendering). EmitGlobalBarrier uses this to avoid splitting the pass for an
	// in-pass partial flush.
	[[nodiscard]] bool IsRendering() const { return m_rendering; }
	// KytyPlus: a cheap in-render-pass execution+memory barrier (vkCmdPipelineBarrier, no
	// render-pass split). Used for guest partial flushes that ask for a TC/L2 writeback -
	// the barrier still performs the cache flush, we just don't end/restart the pass.
	void PipelineMemoryBarrier(vk::PipelineStageFlags src_stage, vk::AccessFlags src_access,
	                           vk::PipelineStageFlags dst_stage, vk::AccessFlags dst_access) const;
	void WaitForFenceOnly();
	void WaitForFence();
	void WaitForFenceAndReset();
	void RetireBufferAfterFence(std::unique_ptr<VulkanBuffer> buffer);
	void RetainResourceUntilFence(std::shared_ptr<void> resource);
	void RecycleDescriptorAfterFence(VulkanDescriptorSet& set);

	[[nodiscard]] vk::CommandBuffer Handle() const;
	[[nodiscard]] GraphicContext&   GetGraphics() const noexcept { return m_graphics; }
	[[nodiscard]] RenderContext&    GetContext() const noexcept { return m_context; }
	[[nodiscard]] bool              IsExecute() const { return m_execute; }

private:
	void Release();
	void FinalizeFence(bool reset_recording);
	void ReleaseResourcesAfterFence();
	void DeleteBuffersAfterFence();
	void RecycleDescriptorsAfterFence();

	RenderContext&                             m_context;
	CommandScheduler&                          m_scheduler;
	GraphicContext&                            m_graphics;
	CommandSlot*                               m_slot            = nullptr;
	bool                                       m_execute         = false;
	bool                                       m_fence_waited    = false;
	uint64_t                                   m_submit_seq      = 0;
	uint32_t                                   m_debug_op        = 0;
	uint64_t                                   m_debug_submit_id = 0;
	uint32_t                                   m_debug_arg0      = 0;
	uint32_t                                   m_debug_arg1      = 0;
	uint32_t                                   m_debug_arg2      = 0;
	uint32_t                                   m_debug_arg3      = 0;
	uint64_t                                   m_debug_arg4      = 0;
	// KytyPlus: per-submission census, reported when the GPU refuses to retire the submit.
	uint32_t                                   m_debug_draw_count     = 0;
	uint32_t                                   m_debug_dispatch_count = 0;
	uint32_t                                   m_debug_compute_count  = 0;
	uint32_t                                   m_debug_flip_count     = 0;
	uint32_t                                   m_debug_eop_count      = 0;
	std::vector<std::unique_ptr<VulkanBuffer>> m_retired_buffers;
	FenceResourceRetainer                      m_fence_resources;
	std::vector<VulkanDescriptorSet*>          m_descriptor_sets_after_fence;
	mutable RenderState                        m_render_state;
	mutable bool                               m_rendering = false;
};

class RenderCommandBuffer final: public CommandBuffer {
public:
	explicit RenderCommandBuffer(CommandScheduler& scheduler): CommandBuffer(scheduler) {}

	void Bind(HW::Context& registers, HW::UserConfig& user_config, HW::Shader& shaders) noexcept {
		m_registers   = &registers;
		m_user_config = &user_config;
		m_shaders     = &shaders;
	}

	[[nodiscard]] HW::Context&    GetRegisters() const noexcept { return *m_registers; }
	[[nodiscard]] HW::UserConfig& GetUserConfig() const noexcept { return *m_user_config; }
	[[nodiscard]] HW::Shader&     GetShaders() const noexcept { return *m_shaders; }

private:
	HW::Context*    m_registers   = nullptr;
	HW::UserConfig* m_user_config = nullptr;
	HW::Shader*     m_shaders     = nullptr;
};

class RenderExecutor {
public:
	explicit RenderExecutor(RenderContext& context): m_context(context) {}
	KYTY_CLASS_NO_COPY(RenderExecutor);

	void DrawIndex(uint64_t submit_id, RenderCommandBuffer& buffer, uint32_t index_type_and_size,
	               uint32_t index_count, const void* index_addr, uint32_t flags, uint32_t type,
	               uint32_t instance_count = 1, uint32_t render_target_slice_offset = 0,
	               int32_t vertex_offset_add = 0, uint32_t first_instance = 0);
	// GPU-driven draw: the vertex/index counts come from a guest buffer written by a compute
	// shader, so the arguments are consumed on the GPU instead of being read by the CPU.
	void DrawIndirect(uint64_t submit_id, RenderCommandBuffer& buffer,
	                  uint32_t index_type_and_size, uint64_t args_vaddr,
	                  uint32_t draw_count, uint32_t stride_bytes, bool indexed,
	                  uint32_t flags, uint32_t instance_count,
	                  uint32_t render_target_slice_offset = 0);
	// Records the guest index buffer that GPU-driven draws should bind. Indirect arguments
	// supply counts and offsets only, never the index buffer itself.
	void SetIndirectIndexBuffer(uint64_t index_vaddr, uint64_t index_size);
	void DrawAuto(uint64_t submit_id, RenderCommandBuffer& buffer, uint32_t index_count,
	              uint32_t flags, uint32_t render_target_slice_offset = 0,
	              uint32_t instance_count = 1, uint32_t first_vertex = 0,
	              uint32_t first_instance = 0);
	void DispatchDirect(uint64_t submit_id, RenderCommandBuffer& buffer, uint32_t thread_group_x,
	                    uint32_t thread_group_y, uint32_t thread_group_z, uint32_t mode);

	[[nodiscard]] DescriptorCache::PreparedBindings
	     PrepareBindings(CommandBuffer& buffer, const ShaderStageRuntime& runtime,
	                     vk::ShaderStageFlags shader_stage, DescriptorCache::Stage stage);
	void RebindBuffers(CommandBuffer& buffer, DescriptorCache::PreparedBindings& bindings);
	// Returns false when an image binding cannot be produced (for example an image whose
	// creation was soft-skipped). Callers must skip the draw/dispatch in that case.
	[[nodiscard]] bool RebindImages(CommandBuffer& buffer,
	                               DescriptorCache::PreparedBindings& bindings);
	void CommitBindings(CommandBuffer& buffer, vk::PipelineBindPoint pipeline_bind_point,
	                    vk::PipelineLayout layout, DescriptorCache::PreparedBindings& bindings);

private:
	struct GraphicsBindings {
		DescriptorCache::PreparedBindings                vertex;
		std::optional<DescriptorCache::PreparedBindings> pixel;
		bool                                             valid = true;
	};

	[[nodiscard]] DescriptorCache::TextureBinding
	ResolveTexture(const ShaderRecompiler::IR::ImageResource&   resource,
	               const ShaderRecompiler::IR::DescriptorValue& value);
	[[nodiscard]] GraphicsBindings PrepareGraphicsBindings(CommandBuffer&            buffer,
	                                                       const ShaderStageRuntime& vertex,
	                                                       const ShaderStageRuntime& pixel,
	                                                       bool                      pixel_active);
	void ResolveRenderColorTarget(uint64_t submit_id, RenderCommandBuffer& buffer,
	                              RenderColorInfo& target, uint32_t render_target_slice_offset = 0,
	                              uint32_t render_target_slot = UINT32_MAX,
	                              bool ignore_target_mask = false, bool exact_format = false);
	void ResolveRenderDepthTarget(uint64_t submit_id, RenderCommandBuffer& buffer,
	                              RenderDepthInfo& target);
	[[nodiscard]] bool PrepareDrawRenderState(uint64_t submit_id, RenderCommandBuffer& buffer,
	                                          const DrawCallInfo& draw,
	                                          uint32_t            render_target_slice_offset,
	                                          bool log_setup_phases, DrawRenderState& state);
	void ExecutePreparedDraw(uint64_t submit_id, RenderCommandBuffer& buffer,
	                         const DrawCallInfo& draw, DrawRenderState& state,
	                         vk::PrimitiveTopology topology, const DrawEmitInfo& emit,
	                         const DrawIndexBufferSource& index_source, bool log_pipeline_phase,
	                         bool set_bind_debug, bool set_auto_debug,
	                         const DrawIndirectSource& indirect = {});
	// Returns std::nullopt when a render target cannot be acquired (for example its backing
	// image was never created); callers skip the draw in that case.
	[[nodiscard]] std::optional<RenderState>
	AcquireRenderTargets(CommandBuffer& buffer, RenderColorInfo* colors, uint32_t color_count,
	                     RenderDepthInfo& depth);
	[[nodiscard]] bool        ResolveColorTargets(uint64_t submit_id, RenderCommandBuffer& buffer,
	                                              uint32_t render_target_slice_offset);
	void                      BindImage(ImageId id, bool storage);
	void                      BindRenderTarget(ImageId id);
	void                      TrackImageBinding(ImageId id);
	void                      ResetBindings();
	[[nodiscard]] bool        TryConsumeComputeMetaClear(const ShaderComputeInputInfo& input,
	                                                     const RenderCommandBuffer&    buffer);

	RenderContext&                      m_context;
	std::vector<std::shared_ptr<Image>> m_bound_images;
	// Guest index buffer currently bound for GPU-driven (indirect) draws.
	uint64_t                            m_indirect_index_vaddr = 0;
	uint64_t                            m_indirect_index_size  = 0;

	friend struct RenderExecutorTestAccess;
};

[[nodiscard]] bool ResolveComputeImageClear(const ShaderComputeInputInfo& input, uint32_t group_x,
                                            uint32_t group_y, uint32_t group_z, uint32_t mode,
                                            ShaderBufferResource& descriptor,
                                            uint32_t& packed_clear, uint64_t& size);

} // namespace Libs::Graphics

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GRAPHICSRENDER_H_ */
