#include "graphics/host_gpu/renderer/pipelineCache.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/framebufferCache.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <utility>
#include <vector>

namespace Libs::Graphics {

namespace {

// Flush the on-disk pipeline cache to disk after this many freshly compiled pipelines. Keeps the
// cache reasonably fresh even if the emulator is closed without a clean shutdown, while avoiding a
// disk write on every single new pipeline.
constexpr uint32_t kDiskCacheFlushInterval = 16;

std::filesystem::path GetPipelineCachePath() {
	return std::filesystem::path("_PipelineCache") / "vk_pipeline_cache.bin";
}

// Returns true if the serialized cache blob was produced by the same driver/device that is running
// now. The Vulkan spec allows drivers to ignore mismatched data, but validating up front avoids
// feeding a foreign blob (recorded on a different GPU) into vkCreatePipelineCache.
bool DiskCacheMatchesDevice(const std::vector<uint8_t>&         data,
                            const vk::PhysicalDeviceProperties& props) {
	// VkPipelineCacheHeaderVersionOne: length(u32) + version(u32) + vendorID(u32) + deviceID(u32) +
	// pipelineCacheUUID(16 bytes) = 32 bytes minimum.
	constexpr size_t kHeaderSize = 32;
	if (data.size() < kHeaderSize) {
		return false;
	}
	uint32_t header_length = 0;
	uint32_t header_version = 0;
	uint32_t vendor_id = 0;
	uint32_t device_id = 0;
	std::memcpy(&header_length, data.data() + 0, sizeof(uint32_t));
	std::memcpy(&header_version, data.data() + 4, sizeof(uint32_t));
	std::memcpy(&vendor_id, data.data() + 8, sizeof(uint32_t));
	std::memcpy(&device_id, data.data() + 12, sizeof(uint32_t));

	if (header_length < kHeaderSize) {
		return false;
	}
	if (header_version != static_cast<uint32_t>(vk::PipelineCacheHeaderVersion::eOne)) {
		return false;
	}
	if (vendor_id != props.vendorID || device_id != props.deviceID) {
		return false;
	}
	return std::memcmp(data.data() + 16, props.pipelineCacheUUID.data(), VK_UUID_SIZE) == 0;
}

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

bool PipelineStaticParameters::operator==(const PipelineStaticParameters& other) const noexcept {
	return std::memcmp(this, &other, sizeof(*this)) == 0;
}

void PipelineCache::EnsureDiskCacheLocked() {
	if (m_disk_cache_ready) {
		return;
	}
	m_disk_cache_ready = true;

	std::vector<uint8_t> initial_data;
	const auto           path = GetPipelineCachePath();
	std::error_code      ec;
	if (std::filesystem::exists(path, ec)) {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (file) {
			const auto size = static_cast<std::streamoff>(file.tellg());
			if (size > 0) {
				initial_data.resize(static_cast<size_t>(size));
				file.seekg(0, std::ios::beg);
				file.read(reinterpret_cast<char*>(initial_data.data()), size);
				if (!file) {
					initial_data.clear();
				}
			}
		}
	}

	if (!initial_data.empty() &&
	    !DiskCacheMatchesDevice(initial_data, m_graphics.physical_device_properties)) {
		LOGF("PipelineCache: on-disk cache does not match the current device, ignoring it\n");
		initial_data.clear();
	}

	vk::PipelineCacheCreateInfo create_info {};
	create_info.sType           = vk::StructureType::ePipelineCacheCreateInfo;
	create_info.pNext           = nullptr;
	create_info.flags           = {};
	create_info.initialDataSize = initial_data.size();
	create_info.pInitialData    = initial_data.empty() ? nullptr : initial_data.data();

	auto result = m_graphics.device.createPipelineCache(&create_info, nullptr, &m_disk_cache);
	if (result != vk::Result::eSuccess) {
		LOGF("PipelineCache: vkCreatePipelineCache failed (%s), continuing without a disk cache\n",
		     VulkanToString(result).c_str());
		m_disk_cache = nullptr;
		return;
	}
	LOGF("PipelineCache: initialized driver pipeline cache from %" PRIu64 " preloaded bytes\n",
	     static_cast<uint64_t>(initial_data.size()));
}

void PipelineCache::FlushDiskCacheLocked() {
	if (m_disk_cache == nullptr) {
		return;
	}

	size_t size = 0;
	if (m_graphics.device.getPipelineCacheData(m_disk_cache, &size, nullptr) !=
	        vk::Result::eSuccess ||
	    size == 0) {
		return;
	}
	std::vector<uint8_t> data(size);
	if (m_graphics.device.getPipelineCacheData(m_disk_cache, &size, data.data()) !=
	    vk::Result::eSuccess) {
		return;
	}
	data.resize(size);

	const auto      path = GetPipelineCachePath();
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);

	// Write to a temporary file and rename so a crash mid-write cannot corrupt the cache.
	const auto tmp_path = std::filesystem::path(path).concat(".tmp");
	{
		std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
		if (!file) {
			return;
		}
		file.write(reinterpret_cast<const char*>(data.data()),
		           static_cast<std::streamsize>(data.size()));
		if (!file) {
			return;
		}
	}
	std::filesystem::rename(tmp_path, path, ec);
	if (ec) {
		std::filesystem::remove(tmp_path, ec);
		return;
	}
	m_pipelines_since_flush = 0;
}

void PipelineCache::MaybeFlushDiskCacheLocked() {
	if (m_disk_cache == nullptr) {
		return;
	}
	if (++m_pipelines_since_flush >= kDiskCacheFlushInterval) {
		FlushDiskCacheLocked();
	}
}

PipelineCache::GraphicsPipeline& PipelineCache::CreateGraphicsPipeline(
    VulkanFramebuffer& framebuffer, RenderColorInfo* colors, uint32_t color_count,
    RenderDepthInfo& depth, ShaderVertexInputInfo& vs_input_info, RenderCommandBuffer& command,
    ShaderPixelInputInfo* ps_input_info, vk::PrimitiveTopology topology, bool ps_active,
    std::span<const uint32_t> vs_spirv, std::span<const uint32_t> ps_spirv) {
	KYTY_PROFILER_BLOCK("PipelineCache::CreatePipeline(Gfx)", profiler::colors::DeepOrangeA200);

	EXIT_IF(colors == nullptr);
	EXIT_IF(color_count > RENDER_COLOR_ATTACHMENTS_MAX);
	EXIT_IF(vs_spirv.empty());
	EXIT_IF(ps_active && ps_spirv.empty());

	Common::LockGuard lock(m_mutex);
	EnsureDiskCacheLocked();
	auto&             ctx    = command.GetRegisters();
	auto&             sh_ctx = command.GetShaders();

	const auto&           vertex_info                              = sh_ctx.GetVs();
	const auto&           ps_regs                                  = sh_ctx.GetPs();
	const HW::BlendColor& bclr                                     = ctx.GetBlendColor();
	uint32_t              color_mask[RENDER_COLOR_ATTACHMENTS_MAX] = {};
	for (uint32_t i = 0; i < color_count; i++) {
		color_mask[i] = (colors[i].vulkan_buffer != nullptr
		                     ? colors[i].export_mapping.ApplyMask(render_target_mask_slot(
		                           ctx.GetRenderTargetMask(), colors[i].target_slot))
		                     : 0);
	}
	const HW::ModeControl& mc = ctx.GetModeControl();

	auto     vs_id = ShaderGetIdVS(vertex_info, vs_input_info, true);
	ShaderId ps_id {};
	if (ps_active) {
		ps_id = ShaderGetIdPS(ps_regs, *ps_input_info, true);
	}

	PipelineStaticParameters static_params {};
	GraphicsPipeline         p {};
	p.render_pass_id = framebuffer.render_pass_id;
	p.ps_shader_id   = ps_id;
	p.vs_shader_id   = vs_id;

	static_params.color_count = color_count;

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
	static_params.samples             = framebuffer.samples;
	static_params.sample_shading_enable =
	    ps_active && framebuffer.samples > 1 && ps_input_info->ps_sample_shading;
	if (static_params.sample_shading_enable && !m_graphics.sample_rate_shading_enabled) {
		EXIT("Pipeline: sample-rate shading is required but unsupported by the host\n");
	}
	static_params.with_depth =
	    (depth.format != vk::Format::eUndefined && depth.vulkan_buffer != nullptr);
	static_params.depth_test_enable  = depth.depth_test_enable;
	static_params.depth_write_enable = (depth.depth_write_enable && !depth.depth_clear_enable);
	static_params.depth_compare_op   = depth.depth_compare_op;
	static_params.depth_bounds_test_enable = depth.depth_bounds_test_enable;
	static_params.depth_min_bounds         = depth.depth_min_bounds;
	static_params.depth_max_bounds         = depth.depth_max_bounds;
	static_params.stencil_test_enable      = depth.stencil_test_enable;
	static_params.stencil_front            = depth.stencil_static_front;
	static_params.stencil_back             = depth.stencil_static_back;
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
	key.render_pass_id = p.render_pass_id;
	key.vs_shader_id   = p.vs_shader_id;
	key.ps_shader_id   = p.ps_shader_id;
	key.static_params  = static_params;

	if (auto iter = m_graphics_pipelines.find(key); iter != m_graphics_pipelines.end()) {
		return *iter->second;
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
	CreatePipelineInternal(*cached, framebuffer.render_pass, vs_input_info, vs_spirv, ps_input_info,
	                       ps_spirv, static_params, vs_id.hash0, vs_id.crc32, ps_id.hash0,
	                       ps_id.crc32, ps_active, m_disk_cache);
	LogPipelineTrace("CreatePipelineInternal done", vs_id.hash0, vs_id.crc32, ps_id.hash0,
	                 ps_id.crc32);

	EXIT_NOT_IMPLEMENTED(cached->pipeline == nullptr);
	EXIT_NOT_IMPLEMENTED(cached->pipeline_layout == nullptr);

	auto [iter, inserted] = m_graphics_pipelines.emplace(std::move(key), std::move(cached));
	EXIT_IF(!inserted);

	MaybeFlushDiskCacheLocked();

	return *iter->second;
}

PipelineCache::ComputePipeline&
PipelineCache::CreateComputePipeline(ShaderComputeInputInfo&      input_info,
                                     const HW::ComputeShaderInfo& cs_regs,
                                     std::span<const uint32_t>    cs_spirv) {
	KYTY_PROFILER_BLOCK("PipelineCache::CreatePipeline(Compute)", profiler::colors::RedA100);

	EXIT_IF(cs_spirv.empty());

	Common::LockGuard lock(m_mutex);
	EnsureDiskCacheLocked();

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
	CreatePipelineInternal(*cached, input_info, cs_spirv, m_disk_cache);

	EXIT_NOT_IMPLEMENTED(cached->pipeline == nullptr);
	EXIT_NOT_IMPLEMENTED(cached->pipeline_layout == nullptr);

	auto [iter, inserted] = m_compute_pipelines.emplace(std::move(key), std::move(cached));
	EXIT_IF(!inserted);

	MaybeFlushDiskCacheLocked();

	return *iter->second;
}
} // namespace Libs::Graphics
