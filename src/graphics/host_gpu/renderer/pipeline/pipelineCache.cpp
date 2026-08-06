#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"
#include "graphics/host_gpu/renderer/pipeline/asyncPipelineCompiler.h"
#include "common/emulatorConfig.h"

#include "common/assert.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/image/imageView.h"
#include "graphics/host_gpu/renderer/pipelineCacheData.h"
#include "graphics/host_gpu/renderer/polyOffsetBias.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace Libs::Graphics {

namespace {

constexpr auto   kDriverCachePath    = "_Cache/vulkan_pipeline_cache.bin";
constexpr size_t kMaxDriverCacheSize = 64u * 1024u * 1024u;

void NormalizeStaticParamsForDynamicState(PipelineStaticParameters& static_params) {
	static_params.viewport_scale[0]  = 0.5f;
	static_params.viewport_scale[1]  = 0.5f;
	static_params.viewport_scale[2]  = 1.0f;
	static_params.viewport_offset[0] = 0.5f;
	static_params.viewport_offset[1] = 0.5f;
	static_params.viewport_offset[2] = 0.0f;

	static_params.scissor_ltrb[0] = 0;
	static_params.scissor_ltrb[1] = 0;
	static_params.scissor_ltrb[2] = 1;
	static_params.scissor_ltrb[3] = 1;
}

} // namespace

PipelineCache::PipelineCache(GraphicContext& graphics, DescriptorCache& descriptor_cache)
    : m_graphics(graphics), m_descriptor_cache(descriptor_cache) {
	EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
	LoadDriverCache();
}

PipelineCache::~PipelineCache() {
	SaveDriverCache();

	// Stop worker threads before destroying any Vulkan handles they might be
	// compiling into. Shutdown() drains the queue and joins all workers on the
	// main thread, so no worker outlives the device.
	m_async_compiler.reset();

	auto destroy = [this](const auto& pipelines) {
		for (const auto& [key, pipeline]: pipelines) {
			(void)key;
			m_graphics.device.destroyPipeline(pipeline->pipeline, nullptr);
			m_graphics.device.destroyPipelineLayout(pipeline->pipeline_layout, nullptr);
		}
	};
	destroy(m_graphics_pipelines);
	destroy(m_compute_pipelines);

	if (m_driver_cache != nullptr) {
		m_graphics.device.destroyPipelineCache(m_driver_cache, nullptr);
		m_driver_cache = nullptr;
	}
}

void PipelineCache::LoadDriverCache() {
	std::vector<uint8_t> initial_data;
	if (Common::File::IsFileExisting(kDriverCachePath)) {
		Common::File file(kDriverCachePath, Common::File::Mode::Read);
		if (!file.IsInvalid() && file.Size() <= kMaxDriverCacheSize) {
			auto buffer = file.ReadWholeBuffer();
			initial_data.resize(buffer.Size());
			if (!initial_data.empty()) {
				std::memcpy(initial_data.data(), buffer.GetDataConst(), buffer.Size());
			}
			if (!PipelineCacheDataIsCompatible(initial_data,
			                                   m_graphics.GetPhysicalDeviceProperties())) {
				LOGF("PipelineCache: ignoring incompatible driver cache\n");
				initial_data.clear();
			}
		}
	}

	vk::PipelineCacheCreateInfo create_info {};
	create_info.initialDataSize = initial_data.size();
	create_info.pInitialData    = initial_data.empty() ? nullptr : initial_data.data();

	auto result = m_graphics.device.createPipelineCache(&create_info, nullptr, &m_driver_cache);
	if (result != vk::Result::eSuccess && !initial_data.empty()) {
		LOGF("PipelineCache: cached data rejected (%s), retrying empty\n",
		     VulkanToString(result).c_str());
		create_info.initialDataSize = 0;
		create_info.pInitialData    = nullptr;
		result = m_graphics.device.createPipelineCache(&create_info, nullptr, &m_driver_cache);
	}
	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess || m_driver_cache == nullptr);
}

void PipelineCache::SaveDriverCache() const {
	if (m_driver_cache == nullptr) {
		return;
	}

	size_t size   = 0;
	auto   result = m_graphics.device.getPipelineCacheData(m_driver_cache, &size, nullptr);
	if (result != vk::Result::eSuccess || size == 0 || size > kMaxDriverCacheSize ||
	    size > std::numeric_limits<uint32_t>::max()) {
		LOGF("PipelineCache: could not query driver cache (%s, size=%" PRIu64 ")\n",
		     VulkanToString(result).c_str(), static_cast<uint64_t>(size));
		return;
	}

	std::vector<uint8_t> data(size);
	result = m_graphics.device.getPipelineCacheData(m_driver_cache, &size, data.data());
	if (result != vk::Result::eSuccess || size == 0) {
		LOGF("PipelineCache: could not read driver cache (%s)\n", VulkanToString(result).c_str());
		return;
	}
	data.resize(size);

	const auto path = std::filesystem::path(kDriverCachePath);
	if (!Common::File::CreateDirectories(path.parent_path())) {
		LOGF("PipelineCache: could not create cache directory\n");
		return;
	}
	Common::File file(path, Common::File::Mode::Write);
	if (file.IsInvalid()) {
		LOGF("PipelineCache: could not open cache file for writing\n");
		return;
	}
	file.Write(data.data(), static_cast<uint32_t>(data.size()));
}

void PipelineCache::MaybeSaveDriverCache() {
	m_new_pipeline_count++;
	// Persist at 1, 2, 4, 8... new pipelines. This survives quick_exit without adding disk I/O
	// to every pipeline compilation.
	if ((m_new_pipeline_count & (m_new_pipeline_count - 1u)) == 0) {
		SaveDriverCache();
	}
}

bool PipelineStaticParameters::operator==(const PipelineStaticParameters& other) const noexcept {
	return std::memcmp(this, &other, sizeof(*this)) == 0;
}

PipelineCache::GraphicsPipeline& PipelineCache::CreateGraphicsPipeline(
    RenderColorInfo* colors, uint32_t color_count, RenderDepthInfo& depth,
    ShaderVertexInputInfo& vs_input_info, RenderCommandBuffer& command,
    ShaderPixelInputInfo* ps_input_info, vk::PrimitiveTopology topology, bool ps_active,
    std::span<const uint32_t> vs_spirv, std::span<const uint32_t> ps_spirv) {
	KYTY_PROFILER_BLOCK("PipelineCache::CreatePipeline(Gfx)", profiler::colors::DeepOrangeA200);

	EXIT_IF(colors == nullptr);
	EXIT_IF(color_count > RENDER_COLOR_ATTACHMENTS_MAX);
	EXIT_IF(vs_spirv.empty());
	EXIT_IF(ps_active && ps_spirv.empty());

	Common::LockGuard lock(m_mutex);
	auto&             ctx    = command.GetRegisters();
	auto&             sh_ctx = command.GetShaders();

	const auto&           vertex_info                              = sh_ctx.GetVs();
	const auto&           ps_regs                                  = sh_ctx.GetPs();
	const HW::BlendColor& bclr                                     = ctx.GetBlendColor();
	uint32_t              color_mask[RENDER_COLOR_ATTACHMENTS_MAX] = {};
	for (uint32_t i = 0; i < color_count; i++) {
		color_mask[i] =
		    (colors[i].image_id ? colors[i].export_mapping.ApplyMask(render_target_mask_slot(
		                              ctx.GetRenderTargetMask(), colors[i].target_slot))
		                        : 0);
	}
	const HW::ModeControl& mc = ctx.GetModeControl();
	const HW::PolyOffset& po = ctx.GetPolyOffset();

	auto     vs_id = ShaderGetIdVS(vertex_info, vs_input_info, true);
	ShaderId ps_id {};
	if (ps_active) {
		ps_id = ShaderGetIdPS(ps_regs, *ps_input_info, true);
	}

	PipelineStaticParameters static_params {};
	GraphicsPipeline         p {};
	p.ps_shader_id = ps_id;
	p.vs_shader_id = vs_id;

	static_params.color_count = color_count;
	PipelineRenderingState rendering {};
	rendering.color_count       = color_count;
	uint32_t attachment_samples = 0;
	for (uint32_t i = 0; i < color_count; i++) {
		EXIT_IF(!colors[i].image_id || colors[i].format == vk::Format::eUndefined);
		rendering.color_formats[i] = colors[i].format;
		if (attachment_samples == 0) {
			attachment_samples = colors[i].samples;
		} else if (attachment_samples != colors[i].samples) {
			EXIT("mixed color attachment sample counts are unsupported: %u and %u\n",
			     attachment_samples, colors[i].samples);
		}
	}
	const bool with_depth =
	    depth.format != vk::Format::eUndefined && static_cast<bool>(depth.image_id);
	if (with_depth) {
		const auto aspects = ImageViewOps::DepthAspectMask(depth.format);
		rendering.depth_format =
		    aspects & vk::ImageAspectFlagBits::eDepth ? depth.format : vk::Format::eUndefined;
		rendering.stencil_format =
		    aspects & vk::ImageAspectFlagBits::eStencil ? depth.format : vk::Format::eUndefined;
		if (attachment_samples == 0) {
			attachment_samples = depth.samples;
		} else if (attachment_samples != depth.samples) {
			EXIT("mixed color/depth sample counts are unsupported: %u and %u\n", attachment_samples,
			     depth.samples);
		}
	}
	EXIT_IF(attachment_samples == 0 ||
	        vulkan_sample_count(attachment_samples) == vk::SampleCountFlagBits {});

	if (ps_active && depth.depth_test_enable && ps_input_info->ps_execute_on_noop) {
		static std::atomic<uint32_t> log_count {0};
		if (log_count.fetch_add(1, std::memory_order_relaxed) < 16) {
			LOGF("Pipeline: temporary: accepting EXEC_ON_NOOP with depth test enabled\n");
		}
	}

	const auto& clip_control = ctx.GetClipControl();
	EXIT_NOT_IMPLEMENTED(!clip_control.IsZClipModeRepresentable());
	static_params.negative_one_to_one = !clip_control.dx_clip_space;
	static_params.depth_clip_enable   = clip_control.IsZClipEnabled();
	static_params.topology            = topology;
	static_params.samples             = attachment_samples;
	static_params.sample_shading_enable =
	    ps_active && attachment_samples > 1 && ps_input_info->ps_sample_shading;
	if (static_params.sample_shading_enable && !m_graphics.sample_rate_shading_enabled) {
		EXIT("Pipeline: sample-rate shading is required but unsupported by the host\n");
	}
	static_params.with_depth         = with_depth;
	static_params.depth_test_enable  = depth.depth_test_enable;
	static_params.depth_write_enable = (depth.depth_write_enable && !depth.depth_clear_enable);
	static_params.depth_compare_op   = depth.depth_compare_op;
	static_params.depth_bounds_test_enable = depth.depth_bounds_test_enable;
	static_params.depth_min_bounds         = depth.depth_min_bounds;
	static_params.depth_max_bounds         = depth.depth_max_bounds;
	static_params.stencil_test_enable      = depth.stencil_test_enable;
	static_params.stencil_front            = depth.stencil_static_front;
	static_params.stencil_back             = depth.stencil_static_back;
	PolyOffsetBias bias {};
	switch (ResolvePolyOffsetBias(mc, po, bias)) {
		case PolyOffsetBiasResult::UnsupportedPerFace:
			EXIT("per-face polygon offset cannot be represented by Vulkan core depth bias\n");
		case PolyOffsetBiasResult::NonFinite:
			EXIT("polygon offset contains a non-finite value\n");
		case PolyOffsetBiasResult::Enabled:
			static_params.depth_bias_enable   = true;
			static_params.depth_bias_constant = bias.constant;
			static_params.depth_bias_clamp    = bias.clamp;
			static_params.depth_bias_slope    = bias.slope;
			break;
		case PolyOffsetBiasResult::Disabled: break;
	}
	for (uint32_t i = 0; i < RENDER_COLOR_ATTACHMENTS_MAX; i++) {
		static_params.color_mask[i] = color_mask[i];
	}
	static_params.cull_back  = mc.cull_back;
	static_params.cull_front = mc.cull_front;
	static_params.face       = mc.face;

	for (uint32_t i = 0; i < color_count; i++) {
		const auto& rt                        = ctx.GetRenderTarget(colors[i].target_slot);
		const auto& bc                        = ctx.GetBlendControl(colors[i].target_slot);
		static_params.color_srcblend[i]       = bc.color_srcblend;
		static_params.color_comb_fcn[i]       = bc.color_comb_fcn;
		static_params.color_destblend[i]      = bc.color_destblend;
		static_params.alpha_srcblend[i]       = bc.alpha_srcblend;
		static_params.alpha_comb_fcn[i]       = bc.alpha_comb_fcn;
		static_params.alpha_destblend[i]      = bc.alpha_destblend;
		static_params.separate_alpha_blend[i] = bc.separate_alpha_blend;
		static_params.blend_enable[i]         = bc.enable;
		static_params.blend_bypass[i]         = rt.info.blend_bypass;
	}
	static_params.blend_color_red   = bclr.red;
	static_params.blend_color_green = bclr.green;
	static_params.blend_color_blue  = bclr.blue;
	static_params.blend_color_alpha = bclr.alpha;

	NormalizeStaticParamsForDynamicState(static_params);

	GraphicsPipelineKey key {};
	key.rendering     = rendering;
	key.vs_shader_id  = p.vs_shader_id;
	key.ps_shader_id  = p.ps_shader_id;
	key.static_params = static_params;

	if (auto iter = m_graphics_pipelines.find(key); iter != m_graphics_pipelines.end()) {
		return *iter->second;
	}

	// --- Async fast path: if async compilation is enabled and this pipeline is not
	//     already cached or in-flight, hand the (already-built) key/rendering/static
	//     params + copies of the SPIR-V and input info to the worker pool and return a
	//     sentinel so the draw path skips recording until the worker publishes the
	//     finished pipeline. Falls through to the synchronous compile below when async
	//     is disabled or the job is already pending (rare double-submit guard).
	if (AsyncCompilationEnabled()) {
		if (m_async_compiler != nullptr && m_async_compiler->IsPending(key)) {
			// Still compiling on a worker; skip this draw until it lands.
			return AsyncPendingSentinel();
		}
		SubmitAsyncCompile(key, rendering, static_params, vs_input_info, ps_input_info,
		                   vs_spirv, ps_spirv, vs_id.hash0, vs_id.crc32, ps_id.hash0,
		                   ps_id.crc32, ps_active);
		return AsyncPendingSentinel();
	}

	if (graphics_debug_dump_enabled()) {
		ShaderDbgDumpInputInfo(vs_input_info);
		if (ps_active) {
			ShaderDbgDumpInputInfo(*ps_input_info);
		}
		LOGF("PipelineTrace: shader binaries VS=0x%08" PRIx32 "/0x%08" PRIx32 " words=%" PRIu64
		     " PS=0x%08" PRIx32 "/0x%08" PRIx32 " words=%" PRIu64 "\n",
		     vs_id.hash0, vs_id.crc32, static_cast<uint64_t>(vs_spirv.size()), ps_id.hash0,
		     ps_id.crc32, static_cast<uint64_t>(ps_spirv.size()));
	}

	auto cached = std::make_unique<GraphicsPipeline>(p);
	LogPipelineTrace("CreatePipelineInternal begin", vs_id.hash0, vs_id.crc32, ps_id.hash0,
	                 ps_id.crc32);
	CreatePipelineInternal(m_graphics, m_descriptor_cache, *cached, m_driver_cache, rendering,
	                       vs_input_info, vs_spirv, ps_input_info, ps_spirv, static_params,
	                       vs_id.hash0, vs_id.crc32, ps_id.hash0, ps_id.crc32, ps_active);
	LogPipelineTrace("CreatePipelineInternal done", vs_id.hash0, vs_id.crc32, ps_id.hash0,
	                 ps_id.crc32);

	EXIT_NOT_IMPLEMENTED(cached->pipeline == nullptr);
	EXIT_NOT_IMPLEMENTED(cached->pipeline_layout == nullptr);

	auto [iter, inserted] = m_graphics_pipelines.emplace(std::move(key), std::move(cached));
	EXIT_IF(!inserted);
	MaybeSaveDriverCache();

	return *iter->second;
}

PipelineCache::ComputePipeline&
PipelineCache::CreateComputePipeline(ShaderComputeInputInfo&      input_info,
                                     const HW::ComputeShaderInfo& cs_regs,
                                     std::span<const uint32_t>    cs_spirv) {
	KYTY_PROFILER_BLOCK("PipelineCache::CreatePipeline(Compute)", profiler::colors::RedA100);

	EXIT_IF(cs_spirv.empty());

	Common::LockGuard lock(m_mutex);

	auto cs_id = ShaderGetIdCS(cs_regs, input_info, true);

	ComputePipeline p {};
	p.cs_shader_id = cs_id;

	ComputePipelineKey key {};
	key.cs_shader_id = p.cs_shader_id;

	if (auto iter = m_compute_pipelines.find(key); iter != m_compute_pipelines.end()) {
		return *iter->second;
	}

	if (graphics_debug_dump_enabled()) {
		ShaderDbgDumpInputInfo(input_info);
	}

	auto cached = std::make_unique<ComputePipeline>(p);
	CreatePipelineInternal(m_graphics, m_descriptor_cache, *cached, m_driver_cache, input_info,
	                       cs_spirv);

	EXIT_NOT_IMPLEMENTED(cached->pipeline == nullptr);
	EXIT_NOT_IMPLEMENTED(cached->pipeline_layout == nullptr);

	auto [iter, inserted] = m_compute_pipelines.emplace(std::move(key), std::move(cached));
	EXIT_IF(!inserted);
	MaybeSaveDriverCache();

	return *iter->second;

PipelineCache::GraphicsPipeline& PipelineCache::AsyncPendingSentinel() noexcept {
	static GraphicsPipeline sentinel {};
	sentinel.pipeline        = nullptr;
	sentinel.pipeline_layout = nullptr;
	return sentinel;
}

bool PipelineCache::AsyncCompilationEnabled() const noexcept {
	return Config::AsyncPipelineCompilationEnabled();
}

bool PipelineCache::IsAsyncPending(const GraphicsPipelineKey& key) const {
	if (!AsyncCompilationEnabled() || m_async_compiler == nullptr) {
		return false;
	}
	return m_async_compiler->IsPending(key);
}

PipelineCache::PipelineLookupResult
PipelineCache::TryGetGraphicsPipeline(const GraphicsPipelineKey& key, GraphicsPipeline*& out) {
	Common::LockGuard lock(m_mutex);
	if (const auto iter = m_graphics_pipelines.find(key); iter != m_graphics_pipelines.end()) {
		out = iter->second.get();
		return PipelineLookupResult::Ready;
	}
	if (AsyncCompilationEnabled() && m_async_compiler != nullptr &&
	    m_async_compiler->IsPending(key)) {
		out = nullptr;
		return PipelineLookupResult::Pending;
	}
	out = nullptr;
	return PipelineLookupResult::Absent;
}

void PipelineCache::PublishCompiledPipeline(GraphicsPipelineKey key,
                                             std::unique_ptr<GraphicsPipeline> pipeline) {
	Common::LockGuard lock(m_mutex);
	// A synchronous CreateGraphicsPipeline call may have raced ahead and inserted
	// the same key (e.g. if async was disabled mid-flight, or the GPU thread fell
	// back to sync compile). Drop the duplicate rather than overwrite the live one.
	if (m_graphics_pipelines.find(key) != m_graphics_pipelines.end()) {
		// Destroy the losing pipeline immediately on this (worker) thread. The Vulkan
		// device is alive (Shutdown hasn't run) and device.destroyPipeline is safe to
		// call from a non-main thread.
		if (pipeline && pipeline->pipeline != nullptr) {
			m_graphics.device.destroyPipeline(pipeline->pipeline, nullptr);
		}
		if (pipeline && pipeline->pipeline_layout != nullptr) {
			m_graphics.device.destroyPipelineLayout(pipeline->pipeline_layout, nullptr);
		}
		return;
	}
	auto [iter, inserted] =
	    m_graphics_pipelines.emplace(std::move(key), std::move(pipeline));
	(void)iter;
	EXIT_IF(!inserted);
	// Driver cache writes are throttled elsewhere; avoid spamming disk from workers.
}

void PipelineCache::SubmitAsyncCompile(GraphicsPipelineKey                key,
                                        PipelineRenderingState            rendering,
                                        PipelineStaticParameters          static_params,
                                        const ShaderVertexInputInfo&      vs_input_info,
                                        const ShaderPixelInputInfo*      ps_input_info,
                                        std::span<const uint32_t>         vs_spirv,
                                        std::span<const uint32_t>         ps_spirv,
                                        uint32_t vs_hash0, uint32_t vs_crc32,
                                        uint32_t ps_hash0, uint32_t ps_crc32, bool ps_active) {
	if (!AsyncCompilationEnabled()) {
		return;
	}
	if (m_async_compiler == nullptr) {
		m_async_compiler = std::make_unique<AsyncPipelineCompiler>(
		    m_graphics, m_descriptor_cache, m_driver_cache, *this);
	}
	AsyncPipelineCompiler::CompileRequest req {};
	req.key          = key;
	req.rendering    = rendering;
	req.static_params = static_params;
	req.vs_input_info = vs_input_info;
	if (ps_active && ps_input_info != nullptr) {
		req.ps_input_info_storage = std::make_unique<ShaderPixelInputInfo>(*ps_input_info);
	}
	req.vs_spirv.assign(vs_spirv.begin(), vs_spirv.end());
	if (ps_active) {
		req.ps_spirv.assign(ps_spirv.begin(), ps_spirv.end());
	}
	req.vs_hash0  = vs_hash0;
	req.vs_crc32  = vs_crc32;
	req.ps_hash0  = ps_hash0;
	req.ps_crc32  = ps_crc32;
	req.ps_active = ps_active;
	m_async_compiler->Submit(std::move(req));
}
}
} // namespace Libs::Graphics
