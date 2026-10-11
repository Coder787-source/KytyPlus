#include "graphics/host_gpu/renderer/renderDraw.h"

#include "common/assert.h"
#include "common/common.h"
#include "common/emulatorConfig.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/graphicsRun.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/guest_gpu/tile.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/pipeline/descriptorCache.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"
#include "graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.h"
#include "graphics/host_gpu/renderer/pipeline/shaderSubgroup.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/shader/recompiler/ir/ResourceMaterialization.h"
#include "graphics/shader/recompiler/ir/ShaderIR.h"
#include "graphics/shader/shader.h"
#include "kernel/eventQueue.h"
#include "kernel/memory.h"
#include "kernel/pthread.h"
#include "libs/errno.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

int32_t ResolveVertexOffset(uint32_t index_offset, const ShaderVertexInputInfo& vs_input_info) {
	if (index_offset != 0 || !vs_input_info.fetch_embedded) {
		return static_cast<int32_t>(index_offset);
	}

	EXIT_IF(!vs_input_info.stage);
	const auto& program   = *vs_input_info.stage.program;
	const auto& resources = *vs_input_info.stage.resources;
	if (program.info.vertex_offset_sgpr >= static_cast<int32_t>(program.user_data_base)) {
		const auto index =
		    static_cast<uint32_t>(program.info.vertex_offset_sgpr) - program.user_data_base;
		if (index < resources.user_data.size()) {
			return static_cast<int32_t>(resources.user_data[index]);
		}
	}

	return 0;
}

static std::atomic<uint32_t> g_draw_state_log_count   = 0;
static std::atomic<uint32_t> g_draw_input_log_count   = 0;
static std::atomic<uint32_t> g_mrt_state_log_count    = 0;
static std::atomic<uint32_t> g_shader_stage_log_count = 0;

static std::atomic<uint32_t> g_framebuffer_skip_log_count = 0;

static std::atomic<uint64_t> g_draw_attempts {0};
static std::atomic<uint64_t> g_draw_submitted {0};

static std::atomic<uint64_t> g_draw_metadata {0};
static std::atomic<uint64_t> g_draw_geometry {0};
static std::atomic<uint64_t> g_draw_invalid_vs {0};
static std::atomic<uint64_t> g_draw_framebuffer {0};
static std::atomic<uint64_t> g_draw_pipeline_pending {0};
static std::atomic<uint64_t> g_draw_bindings_missing {0};
static std::atomic<uint64_t> g_draw_rt_missing {0};
static std::atomic<uint64_t> g_draw_resolves {0};
static std::atomic<uint64_t> g_draw_video_attempts {0};
static std::atomic<uint64_t> g_draw_video_submitted {0};
static std::atomic<uint64_t> g_draw_video_pending {0};
static std::atomic<uint64_t> g_draw_indirect_submitted {0};
static std::atomic<uint64_t> g_draw_indirect_unsupported {0};
static std::atomic<uint64_t> g_draw_vertices {0};
// Index/instance counts of the draw currently being tested for a GE skip, so the
// skip log can report how much geometry a rejected draw would have produced.
static std::atomic<uint32_t> g_skip_debug_index_count {0};
static std::atomic<uint32_t> g_skip_debug_instance_count {0};
static std::atomic<uint64_t> g_draw_instances {0};
static void ReportDrawProgress() {
	// These counters are diagnostic only. In a normal (silent) game run, avoid a
	// clock read and atomic update for every draw submitted by the guest.
	if (Log::IsSilent()) {
		return;
	}
	g_draw_attempts.fetch_add(1, std::memory_order_relaxed);
	static std::atomic<int64_t> last {0};
	const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now().time_since_epoch()).count();
	auto previous = last.load(std::memory_order_relaxed);
	if (now - previous >= 2000 && last.compare_exchange_strong(previous, now)) {
		LOGF("Draw progress: attempts=%llu submitted=%llu metadata=%llu geometry_skipped=%llu invalid_vs=%llu framebuffer=%llu pipeline_pending=%llu bindings_missing=%llu rt_missing=%llu resolves=%llu video_attempts=%llu video_submitted=%llu video_pending=%llu indirect_submitted=%llu indirect_unsupported=%llu vertices=%llu instances=%llu\n",
		     static_cast<unsigned long long>(g_draw_attempts.exchange(0)),
		     static_cast<unsigned long long>(g_draw_submitted.exchange(0)),
		     static_cast<unsigned long long>(g_draw_metadata.exchange(0)),
		     static_cast<unsigned long long>(g_draw_geometry.exchange(0)),
		     static_cast<unsigned long long>(g_draw_invalid_vs.exchange(0)),
		     static_cast<unsigned long long>(g_draw_framebuffer.exchange(0)),
		     static_cast<unsigned long long>(g_draw_pipeline_pending.exchange(0)),
		     static_cast<unsigned long long>(g_draw_bindings_missing.exchange(0)),
		     static_cast<unsigned long long>(g_draw_rt_missing.exchange(0)),
		     static_cast<unsigned long long>(g_draw_resolves.exchange(0)),
		     static_cast<unsigned long long>(g_draw_video_attempts.exchange(0)),
		     static_cast<unsigned long long>(g_draw_video_submitted.exchange(0)),
		     static_cast<unsigned long long>(g_draw_video_pending.exchange(0)),
		     static_cast<unsigned long long>(g_draw_indirect_submitted.exchange(0)),
		     static_cast<unsigned long long>(g_draw_indirect_unsupported.exchange(0)),
		     static_cast<unsigned long long>(g_draw_vertices.exchange(0)),
		     static_cast<unsigned long long>(g_draw_instances.exchange(0)));
	}
}

static float ConvertPolygonOffsetConstantFactor(float guest_factor, const HW::PolyOffset& offset,
                                                vk::Format host_depth_format) {
	if (offset.db_is_float_fmt) {
		return guest_factor;
	}

	int host_depth_bits = 0;
	switch (host_depth_format) {
		case vk::Format::eD16Unorm:
		case vk::Format::eD16UnormS8Uint: host_depth_bits = 16; break;
		case vk::Format::eD24UnormS8Uint: host_depth_bits = 24; break;
		default:
			// A fixed-point guest bias cannot be represented exactly by a floating-point host
			// attachment without VK_EXT_depth_bias_control.
			return guest_factor;
	}
	return std::ldexp(guest_factor, host_depth_bits + offset.neg_num_db_bits);
}

static const char* RenderColorTypeName(RenderColorType type) {
	switch (type) {
		case RenderColorType::NoColorOutput: return "NoColorOutput";
		case RenderColorType::RenderTexture: return "RenderTexture";
		default: return "Unknown";
	}
}

static bool IsDualSourceBlendFactor(uint32_t factor) {
	return factor >= 0x0fu && factor <= 0x12u;
}

static void LogFramebufferSkip(const char* draw_name, const RenderColorInfo& color,
                               const RenderDepthInfo& depth, const RenderCommandBuffer& buffer,
                               uint32_t index_count, uint32_t flags) {
	const auto& ctx  = buffer.GetRegisters();
	const auto& ucfg = buffer.GetUserConfig();
	if (!graphics_debug_dump_enabled()) {
		return;
	}

	auto log_id = g_framebuffer_skip_log_count.fetch_add(1, std::memory_order_relaxed);
	if (log_id >= 128) {
		return;
	}

	LOGF(
	    "DrawFramebufferSkip[%u]: %s color=%s color_addr=0x%010" PRIx64 " color_size=0x%016" PRIx64
	    " color_image=%s depth_format=%s depth_image=%s depth_vaddr_num=%d target_mask=0x%08" PRIx32
	    " prim=%u index_count=%u flags=0x%08" PRIx32 "\n",
	    log_id, draw_name, RenderColorTypeName(color.type), color.base_addr, color.buffer_size,
	    color.image_id ? "yes" : "no", VulkanToString(depth.format).c_str(),
	    depth.image_id ? "yes" : "no", depth.vaddr_num, ctx.GetRenderTargetMask(),
	    ucfg.GetPrimType(), index_count, flags);
}

static void LogMrtState(const char* draw_name, const RenderCommandBuffer& buffer,
                        const ShaderPixelInputInfo& ps_input_info) {
	const auto& ctx            = buffer.GetRegisters();
	const auto& sh_regs        = ctx.GetShaderRegisters();
	const auto  rt_mask        = ctx.GetRenderTargetMask();
	const auto  cb_shader_mask = sh_regs.m_cbShaderMask;
	const auto& bc0            = ctx.GetBlendControl(0);

	bool interesting = rt_mask != 0x0f || (cb_shader_mask & ~0x0fu) != 0 ||
	                   IsDualSourceBlendFactor(bc0.color_srcblend) ||
	                   IsDualSourceBlendFactor(bc0.color_destblend) ||
	                   (bc0.separate_alpha_blend && (IsDualSourceBlendFactor(bc0.alpha_srcblend) ||
	                                                 IsDualSourceBlendFactor(bc0.alpha_destblend)));

	for (uint32_t i = 1; i < 8; i++) {
		const auto& rt = ctx.GetRenderTarget(i);
		if (rt.base.addr != 0 || ps_input_info.target_output_mode[i] != 0 ||
		    ((rt_mask >> (i * 4u)) & 0x0fu) != 0 || ((cb_shader_mask >> (i * 4u)) & 0x0fu) != 0) {
			interesting = true;
		}
	}

	if (!interesting) {
		return;
	}

	auto log_id = g_mrt_state_log_count.fetch_add(1);
	if (log_id >= 32) {
		return;
	}

	LOGF("MrtState[%u]: %s rt_mask=0x%08" PRIx32 " cb_shader_mask=0x%08" PRIx32
	     " blend0=%s src=%u dst=%u alpha_src=%u alpha_dst=%u sep_alpha=%s\n",
	     log_id, draw_name, rt_mask, cb_shader_mask, bc0.enable ? "true" : "false",
	     bc0.color_srcblend, bc0.color_destblend, bc0.alpha_srcblend, bc0.alpha_destblend,
	     bc0.separate_alpha_blend ? "true" : "false");

	for (uint32_t i = 0; i < 8; i++) {
		const auto& rt  = ctx.GetRenderTarget(i);
		const auto& bc  = ctx.GetBlendControl(i);
		const auto  ctm = (rt_mask >> (i * 4u)) & 0x0fu;
		const auto  csm = (cb_shader_mask >> (i * 4u)) & 0x0fu;

		if (rt.base.addr == 0 && ps_input_info.target_output_mode[i] == 0 && ctm == 0 && csm == 0 &&
		    !bc.enable) {
			continue;
		}

		LOGF("MrtState[%u]: slot=%u addr=0x%010" PRIx64
		     " target_mask=0x%x shader_mask=0x%x out_mode=%u"
		     " fmt=0x%08" PRIx32 " nfmt=0x%08" PRIx32 " order=0x%08" PRIx32
		     " width=%u height=%u tile=%u"
		     " blend=%s src=%u dst=%u alpha_src=%u alpha_dst=%u\n",
		     log_id, i, rt.base.addr, ctm, csm, ps_input_info.target_output_mode[i], rt.info.format,
		     rt.info.channel_type, rt.info.channel_order, rt.attrib2.width + 1,
		     rt.attrib2.height + 1, rt.attrib3.tile_mode, bc.enable ? "true" : "false",
		     bc.color_srcblend, bc.color_destblend, bc.alpha_srcblend, bc.alpha_destblend);
	}
}

static void LogDrawTargetState(const char* draw_name, const RenderColorInfo& color,
                               const RenderDepthInfo& depth, const RenderCommandBuffer& buffer,
                               const ShaderPixelInputInfo& ps_input_info, uint32_t index_count,
                               uint32_t flags) {
	const auto& ctx  = buffer.GetRegisters();
	const auto& ucfg = buffer.GetUserConfig();
	if (color.type == RenderColorType::NoColorOutput) {
		return;
	}

	auto log_id = g_draw_state_log_count.fetch_add(1);
	if (log_id >= 192) {
		return;
	}

	const auto& cc             = ctx.GetColorControl();
	const auto& bc             = ctx.GetBlendControl(color.target_slot);
	const auto& dc             = ctx.GetDepthControl();
	const auto& vp             = ctx.GetScreenViewport();
	const auto& vp0            = vp.viewports[0];
	const auto& ps_resources   = ps_input_info.stage.program->info;
	const auto  sampled_images = std::count_if(
	    ps_resources.images.begin(), ps_resources.images.end(), [](const auto& image) {
		    return image.kind == ShaderRecompiler::IR::ResourceKind::Image ||
		           image.kind == ShaderRecompiler::IR::ResourceKind::ImageUint;
	    });

	vk::Extent2D extent = color.image_id ? color.extent : vk::Extent2D {};
	auto         sc     = calc_final_scissor(vp, ctx.GetScanModeControl(), extent);

	LOGF(
	    "DrawTargetState[%u]: frame=%d %s target=%s addr=0x%010" PRIx64
	    " extent=%ux%u prim=%u index_count=%u flags=0x%08" PRIx32 " color_mask=0x%08" PRIx32
	    " clear=%s clear_rgba=(%.3f,%.3f,%.3f,%.3f) cc_mode=%u cc_op=0x%02x"
	    " blend=%s src=%u dst=%u comb=%u ps_tex=%d sampled=%d storage=%d ps_kill=%s target_mode0=%u"
	    " depth_test=%s depth_write=%s depth_func=%u depth_clear=%s viewport=(%.1f,%.1f %.1fx%.1f) "
	    "scissor=(%d,%d)-(%d,%d)\n",
	    log_id, buffer.GetContext().GetGpu().GetFrameNum(), draw_name,
	    RenderColorTypeName(color.type), color.base_addr, extent.width, extent.height,
	    ucfg.GetPrimType(), index_count, flags, ctx.GetRenderTargetMask(),
	    color.color_clear_enable ? "true" : "false", color.color_clear_value.float32[0],
	    color.color_clear_value.float32[1], color.color_clear_value.float32[2],
	    color.color_clear_value.float32[3], cc.mode, cc.op, bc.enable ? "true" : "false",
	    bc.color_srcblend, bc.color_destblend, bc.color_comb_fcn,
	    static_cast<int>(ps_resources.images.size()), static_cast<int>(sampled_images),
	    static_cast<int>(ps_resources.images.size() - sampled_images),
	    ps_input_info.ps_pixel_kill_enable ? "true" : "false", ps_input_info.target_output_mode[0],
	    dc.z_enable ? "true" : "false", dc.z_write_enable ? "true" : "false", dc.zfunc,
	    depth.depth_clear_enable ? "true" : "false", vp0.xoffset - vp0.xscale,
	    vp0.yoffset - vp0.yscale, vp0.xscale * 2.0f, vp0.yscale * 2.0f, sc.left, sc.top, sc.right,
	    sc.bottom);

	LogMrtState(draw_name, buffer, ps_input_info);
}

static void LogDrawInputState(const RenderCommandBuffer& buffer, const RenderColorInfo& color,
                              const ShaderVertexInputInfo& vs_input_info,
                              uint32_t index_type_and_size, uint32_t index_count,
                              const void* index_addr) {
	auto log_id = g_draw_input_log_count.fetch_add(1);
	if (log_id >= 64) {
		return;
	}

	LOGF("DrawInputState[%u]: frame=%d target=%s addr=0x%010" PRIx64
	     " index_type=%u index_count=%u index_addr=0x%016" PRIx64
	     " vs_resources=%d vs_buffers=%d\n",
	     log_id, buffer.GetContext().GetGpu().GetFrameNum(), RenderColorTypeName(color.type),
	     color.base_addr, index_type_and_size, index_count, reinterpret_cast<uint64_t>(index_addr),
	     vs_input_info.resources_num, vs_input_info.buffers_num);

	for (int bi = 0; bi < vs_input_info.buffers_num; bi++) {
		const auto& b = vs_input_info.buffers[bi];
		LOGF("DrawInputState[%u]: vb[%d] addr=0x%010" PRIx64
		     " stride=%u records=%u fetch_index=%u attr_num=%d\n",
		     log_id, bi, b.addr, b.stride, b.num_records, b.fetch_index, b.attr_num);

		const auto* bytes = reinterpret_cast<const uint8_t*>(b.addr);
		if (bytes != nullptr && b.stride != 0) {
			const uint32_t records = std::min<uint32_t>(b.num_records, 4u);
			for (uint32_t rec = 0; rec < records; rec++) {
				const auto* rec_bytes = bytes + static_cast<uint64_t>(rec) * b.stride;
				const auto  dword_num = std::min<uint32_t>(b.stride / 4u, 12u);
				uint32_t    raw[12]   = {};
				float       flt[12]   = {};
				for (uint32_t i = 0; i < dword_num; i++) {
					std::memcpy(&raw[i], rec_bytes + i * 4u, sizeof(raw[i]));
					std::memcpy(&flt[i], rec_bytes + i * 4u, sizeof(flt[i]));
				}
				LOGF("DrawInputState[%u]: vb[%d].rec[%u] stride=%u dwords=%u raw=%08" PRIx32
				     " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
				     " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
				     " f=(%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f)\n",
				     log_id, bi, rec, b.stride, dword_num, raw[0], raw[1], raw[2], raw[3], raw[4],
				     raw[5], raw[6], raw[7], raw[8], flt[0], flt[1], flt[2], flt[3], flt[4], flt[5],
				     flt[6], flt[7], flt[8]);

				for (int ai = 0; ai < b.attr_num; ai++) {
					const auto  res_index = b.attr_indices[ai];
					const auto& r         = vs_input_info.resources[res_index];
					const auto& rd        = vs_input_info.resources_dst[res_index];
					const auto  offset    = b.attr_offsets[ai];
					if (offset + 4u <= b.stride &&
					    r.Format() ==
					        Prospero::GpuEnumValue(Prospero::BufferFormat::k8_8_8_8UNorm)) {
						uint32_t packed = 0;
						std::memcpy(&packed, rec_bytes + offset, sizeof(packed));
						const auto r8 = (packed >> 0u) & 0xffu;
						const auto g8 = (packed >> 8u) & 0xffu;
						const auto b8 = (packed >> 16u) & 0xffu;
						const auto a8 = (packed >> 24u) & 0xffu;
						LOGF("DrawInputState[%u]: vb[%d].rec[%u].attr[%d] dst=v%d fmt=56 "
						     "rgba8=%02" PRIx32 "%02" PRIx32 "%02" PRIx32 "%02" PRIx32
						     " rgba=(%.3f,%.3f,%.3f,%.3f)\n",
						     log_id, bi, rec, ai, rd.register_start, r8, g8, b8, a8,
						     static_cast<double>(r8) / 255.0, static_cast<double>(g8) / 255.0,
						     static_cast<double>(b8) / 255.0, static_cast<double>(a8) / 255.0);
					}
				}
			}
		}

		for (int ai = 0; ai < b.attr_num; ai++) {
			const auto  res_index = b.attr_indices[ai];
			const auto& r         = vs_input_info.resources[res_index];
			const auto& rd        = vs_input_info.resources_dst[res_index];
			LOGF("DrawInputState[%u]: attr[%d] res=%d offset=%u dst=v%d regs=%d fetch_index=%u "
			     "sharp=%08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 "\n",
			     log_id, ai, res_index, b.attr_offsets[ai], rd.register_start, rd.registers_num,
			     rd.fetch_index, r.fields[0], r.fields[1], r.fields[2], r.fields[3]);
		}
	}
}

static PipelineDynamicParameters BuildGraphicsDynamicParams(const RenderCommandBuffer& buffer,
                                                            const RenderColorInfo*     colors,
                                                            uint32_t                   color_count,
                                                            const RenderDepthInfo&     depth) {
	EXIT_IF(colors == nullptr);
	const auto& ctx = buffer.GetRegisters();

	PipelineDynamicParameters ret {};
	ret.color_write_count   = color_count;
	ret.stencil_test_enable = depth.stencil_test_enable;

	const auto&  vp = ctx.GetScreenViewport();
	vk::Extent2D framebuffer_extent {};
	if (color_count > 0 && colors[0].image_id) {
		framebuffer_extent = colors[0].extent;
	} else if (depth.image_id) {
		framebuffer_extent = {depth.width, depth.height};
	}

	const auto final_scissor = calc_final_scissor(vp, ctx.GetScanModeControl(), framebuffer_extent);

	ret.viewport_scale[0]  = vp.viewports[0].xscale;
	ret.viewport_scale[1]  = vp.viewports[0].yscale;
	ret.viewport_scale[2]  = vp.viewports[0].zscale;
	ret.viewport_offset[0] = vp.viewports[0].xoffset;
	ret.viewport_offset[1] = vp.viewports[0].yoffset;
	ret.viewport_offset[2] = vp.viewports[0].zoffset;
	ret.scissor_ltrb[0]    = final_scissor.left;
	ret.scissor_ltrb[1]    = final_scissor.top;
	ret.scissor_ltrb[2]    = final_scissor.right;
	ret.scissor_ltrb[3]    = final_scissor.bottom;
	ret.line_width         = ctx.GetLineWidth();
	ret.stencil_front      = depth.stencil_dynamic_front;
	ret.stencil_back       = depth.stencil_dynamic_back;

	const auto& mode        = ctx.GetModeControl();
	const auto& poly_offset = ctx.GetPolyOffset();
	const bool  use_front   = mode.poly_offset_front_enable && !mode.cull_front;
	const bool  use_back    = mode.poly_offset_back_enable && !mode.cull_back;
	ret.depth_bias_enable   = use_front || use_back;
	if (ret.depth_bias_enable) {
		// Vulkan has one bias for both faces. Prefer a visible front face when both are enabled.
		const float guest_constant_factor =
		    use_front ? poly_offset.front_offset : poly_offset.back_offset;
		ret.depth_bias_constant_factor =
		    ConvertPolygonOffsetConstantFactor(guest_constant_factor, poly_offset, depth.format);
		ret.depth_bias_clamp = poly_offset.clamp;
		ret.depth_bias_slope_factor =
		    (use_front ? poly_offset.front_scale : poly_offset.back_scale) / 16.0f;
	}

	// CB_COLOR_CONTROL.operation controls special CB operations, not the normal color component
	// write mask. Use CB_TARGET_MASK here so scanout passes with mode=Disable do not go black.
	for (uint32_t i = 0; i < RENDER_COLOR_ATTACHMENTS_MAX; i++) {
		ret.color_write_enable[i] =
		    (i < color_count &&
		     render_target_mask_slot(ctx.GetRenderTargetMask(), colors[i].target_slot) != 0);
	}

	return ret;
}

static void SetDynamicParams(const RenderCommandBuffer& buffer, vk::CommandBuffer vk_buffer,
                             const PipelineDynamicParameters& dynamic_params) {
	KYTY_PROFILER_FUNCTION();

	vk::Viewport viewport {};
	viewport.x        = dynamic_params.viewport_offset[0] - dynamic_params.viewport_scale[0];
	viewport.y        = dynamic_params.viewport_offset[1] - dynamic_params.viewport_scale[1];
	viewport.width    = dynamic_params.viewport_scale[0] * 2.0f;
	viewport.height   = dynamic_params.viewport_scale[1] * 2.0f;
	viewport.minDepth = dynamic_params.viewport_offset[2];
	viewport.maxDepth = dynamic_params.viewport_scale[2] + dynamic_params.viewport_offset[2];
	vk_buffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor {};
	scissor.offset = {dynamic_params.scissor_ltrb[0], dynamic_params.scissor_ltrb[1]};
	scissor.extent = {
	    static_cast<uint32_t>(dynamic_params.scissor_ltrb[2] - dynamic_params.scissor_ltrb[0]),
	    static_cast<uint32_t>(dynamic_params.scissor_ltrb[3] - dynamic_params.scissor_ltrb[1])};
	vk_buffer.setScissor(0, 1, &scissor);

	float line_width = dynamic_params.line_width;
	if (line_width != 1.0f) {
		static bool logged = false;
		if (!logged) {
			LOGF("Render: temporary: clamping Vulkan line width %f to 1.0 because wideLines is "
			     "not enabled\n",
			     line_width);
			logged = true;
		}
		line_width = 1.0f;
	}
	vk_buffer.setLineWidth(line_width);

	vk_buffer.setDepthBiasEnable(dynamic_params.depth_bias_enable ? VK_TRUE : VK_FALSE);
	if (dynamic_params.depth_bias_enable) {
		vk_buffer.setDepthBias(dynamic_params.depth_bias_constant_factor,
		                       dynamic_params.depth_bias_clamp,
		                       dynamic_params.depth_bias_slope_factor);
	}

	if (dynamic_params.stencil_test_enable) {
		vk_buffer.setStencilCompareMask(vk::StencilFaceFlagBits::eFront,
		                                dynamic_params.stencil_front.compareMask);
		vk_buffer.setStencilCompareMask(vk::StencilFaceFlagBits::eBack,
		                                dynamic_params.stencil_back.compareMask);
		vk_buffer.setStencilWriteMask(vk::StencilFaceFlagBits::eFront,
		                              dynamic_params.stencil_front.writeMask);
		vk_buffer.setStencilWriteMask(vk::StencilFaceFlagBits::eBack,
		                              dynamic_params.stencil_back.writeMask);
		vk_buffer.setStencilReference(vk::StencilFaceFlagBits::eFront,
		                              dynamic_params.stencil_front.reference);
		vk_buffer.setStencilReference(vk::StencilFaceFlagBits::eBack,
		                              dynamic_params.stencil_back.reference);
	}

#if defined(__APPLE__)
	// MoltenVK has no VK_EXT_color_write_enable; the pipeline is created without the
	// eColorWriteEnableEXT dynamic state and relies on the static colorWriteMask instead.
#else
	vk::Bool32 enable[RENDER_COLOR_ATTACHMENTS_MAX] = {};
	for (uint32_t i = 0; i < dynamic_params.color_write_count; i++) {
		enable[i] = (dynamic_params.color_write_enable[i] ? VK_TRUE : VK_FALSE);
	}
	if (dynamic_params.color_write_count != 0) {
		vk_buffer.setColorWriteEnableEXT(dynamic_params.color_write_count, enable);
	}
#endif
}

static bool DrawHasValidVertexShader(const HW::Shader& sh_ctx) {

	const auto& vs = sh_ctx.GetVs();
	return vs.gs_regs.chksum != 0 && ShaderAddressValid(vs.es_regs.data_addr);
}

static bool PixelShaderHasDepthOrCoverageSideEffects(const HW::ShaderRegisters& sh_regs) {
	const auto& db = sh_regs.db_shader_control;
	return sh_regs.shader_z_format != 0 || db.shader_kill_enable || db.shader_z_export_enable ||
	       db.shader_mask_export_enable || db.shader_dual_export_enable ||
	       db.shader_execute_on_noop;
}

static bool CanFoldTriangleCopyDraw(const RenderCommandBuffer& buffer) {
	const auto& ctx = buffer.GetRegisters();
	const auto& sh = ctx.GetShaderRegisters();
	const auto& ucfg = buffer.GetUserConfig();
	const auto& ge = ucfg.GetGeControl();
	const auto stages = ctx.GetShaderStages();
	const auto required = Pm4::VGT_SHADER_STAGES_EN_GS_EN_MASK |
	                      Pm4::VGT_SHADER_STAGES_EN_PRIMGEN_EN_MASK;
	return (stages & required) == required &&
	       (stages & Pm4::VGT_SHADER_STAGES_EN_ES_EN_MASK) != 0 &&
	       (stages & (Pm4::VGT_SHADER_STAGES_EN_LS_EN_MASK |
	                  Pm4::VGT_SHADER_STAGES_EN_HS_EN_MASK)) == 0 &&
	       sh.m_geNggSubgrpCntl == 1 && sh.m_vgtGsMaxVertOut == 3 &&
	       sh.m_vgtGsOutPrimType == 2 && sh.m_geMaxOutputPerSubgroup == 0xc0 &&
	       ge.primitive_group_size <= 0x40 && ge.vertex_group_size <= 0x40 &&
	       (sh.m_paClVsOutCntl & (1u << 19u)) == 0 &&
	       ((sh.m_paClVsOutCntl & (1u << 18u)) == 0 ||
	        buffer.GetContext().GetGraphics().shader_output_layer_enabled) &&
	       (ucfg.GetPrimType() == Prospero::GpuEnumValue(Prospero::PrimitiveType::kTriList) ||
	        ucfg.GetPrimType() == Prospero::GpuEnumValue(Prospero::PrimitiveType::kTriStrip)) &&
	       ShaderCanFoldTriangleCopyGeometry(buffer.GetShaders().GetVs());
}

static bool ShouldSkipGeShader(const RenderCommandBuffer& buffer) {
	if (CanFoldTriangleCopyDraw(buffer)) {
		return false;
	}
	const auto& ctx         = buffer.GetRegisters();
	const auto& ucfg        = buffer.GetUserConfig();
	const auto& sh_ctx      = buffer.GetShaders();
	const auto& sh_regs     = ctx.GetShaderRegisters();
	const auto& ge_cntl     = ucfg.GetGeControl();
	const auto& vertex_info = sh_ctx.GetVs();
	const auto  stages      = ctx.GetShaderStages();

	const auto is_known_gs_out_prim_type = [](uint32_t value) {
		switch (static_cast<Prospero::GsOutputPrimitiveType>(value)) {
			case Prospero::GsOutputPrimitiveType::kPoints:
			case Prospero::GsOutputPrimitiveType::kLines:
			case Prospero::GsOutputPrimitiveType::kTriangles:
			case Prospero::GsOutputPrimitiveType::k2dRectangle:
			case Prospero::GsOutputPrimitiveType::kRectList: return true;
		}

		return false;
	};

	// Decode the active geometry stages from VGT_SHADER_STAGES_EN instead of
	// matching one literal mask. The old check accepted exactly 0x02002000 and
	// rejected every other non-zero combination, which is why NGG drew only for
	// that single front-end. The bits used here are the architected field positions
	// (PRIMGEN_PASSTHRU_EN bit 25, PRIMGEN_EN bit 13, ES_EN bits 3..4, GS_EN bit 5,
	// VS_EN bits 6..7), so any front-end that enables the vertex+ES passthrough path
	// is handled the same way rather than falling through to an unconditional skip.
	const bool stages_have_vs = (stages & Pm4::VGT_SHADER_STAGES_EN_VS_EN_MASK) != 0;
	const bool stages_have_es = (stages & Pm4::VGT_SHADER_STAGES_EN_ES_EN_MASK) != 0;
	const bool stages_have_gs = (stages & Pm4::VGT_SHADER_STAGES_EN_GS_EN_MASK) != 0;
	const bool stages_primgen = (stages & Pm4::VGT_SHADER_STAGES_EN_PRIMGEN_EN_MASK) != 0;
	const bool stages_primgen_passthru =
	    (stages & Pm4::VGT_SHADER_STAGES_EN_PRIMGEN_PASSTHRU_EN_MASK) != 0;
	const bool stages_tessellation =
	    (stages & (Pm4::VGT_SHADER_STAGES_EN_LS_EN_MASK | Pm4::VGT_SHADER_STAGES_EN_HS_EN_MASK)) != 0;

	// NGG/primgen vertex path. The primgen pass-through front-end (PRIMGEN_EN bit 13
	// plus PRIMGEN_PASSTHRU_EN bit 25, i.e. 0x02002000) runs the vertex work through the
	// NGG unit and sets NEITHER VS_EN nor ES_EN, so a front-end test that demands VS_EN
	// rejects exactly the combination this path exists to handle. Accept any of the
	// vertex-processing front-ends instead.
	const bool stages_have_vertex_frontend =
	    stages_have_vs || stages_have_es || stages_primgen_passthru;
	// Shader base registers retain their values when a stage is disabled. CB4 switches
	// from ES+GS to primgen passthrough without clearing the old GS address; only the
	// stage-enable bits determine whether that address still names an active shader.
	const bool ngg_vertex_path =
	    stages_have_vertex_frontend && vertex_info.es_regs.data_addr != 0 &&
	    vertex_info.gs_regs.chksum != 0 && !stages_have_gs &&
	    (stages_primgen_passthru || stages_primgen || stages_have_es) &&
	    sh_regs.m_vgtGsMaxVertOut == 0x00000000 &&
	    is_known_gs_out_prim_type(sh_regs.m_vgtGsOutPrimType);

	// A GS-enabled draw normally needs a real geometry-shader implementation, which this
	// renderer does not have. It can still be executed when the GS provably passes its input
	// through unchanged: one primitive in, one primitive out, emitted as triangles, with no
	// amplification. The ES output is then already the final vertex stream, so the draw can
	// run with the GS stage folded away.
	//   max_vert_out == 3 with out-prim == triangles means three vertices per input
	//   primitive (a single triangle); max_output_per_subgroup bounds the vertices emitted
	//   per subgroup.
	const bool gs_passthrough = stages_have_gs && stages_have_es && stages_primgen &&
	                          !stages_tessellation && vertex_info.es_regs.data_addr != 0 &&
	                          vertex_info.gs_regs.chksum != 0 &&
	                          sh_regs.m_geNggSubgrpCntl <= 0x00000001 &&
	                          sh_regs.m_vgtGsMaxVertOut == 0x00000003 &&
	                          sh_regs.m_vgtGsOutPrimType == 0x00000002 &&
	                          sh_regs.m_geMaxOutputPerSubgroup <= 0x00000040 &&
	                          ge_cntl.primitive_group_size <= 0x0040 &&
	                          ge_cntl.vertex_group_size <= 0x0040;
	// A stage combination we do not model: tessellation stages, or a mask that enables
	// neither a plain VS draw nor the NGG vertex path.
	const bool unsupported_stage_mask =
	    stages != 0 && (stages_tessellation || !stages_have_vertex_frontend) &&
	    !ngg_vertex_path && !gs_passthrough;
	// Executing a geometry shader still needs a GS implementation. An inactive GS
	// register must not reject subsequent vertex-only draws; a passthrough GS needs
	// no implementation at all.
	const bool unsupported_gs_stage = stages_have_gs && !gs_passthrough;
	const bool ge_group_size =
	    ge_cntl.primitive_group_size > 0x0040 || ge_cntl.vertex_group_size > 0x0040;
	const bool ge_shader_regs =
	    (sh_regs.m_geNggSubgrpCntl != 0x00000000 && sh_regs.m_geNggSubgrpCntl != 0x00000001) ||
	    (sh_regs.m_vgtGsMaxVertOut != 0x00000000 && !ngg_vertex_path && !gs_passthrough) ||
	    (!is_known_gs_out_prim_type(sh_regs.m_vgtGsOutPrimType) && !gs_passthrough) ||
	    (sh_regs.m_geMaxOutputPerSubgroup > 0x00000040 && !gs_passthrough);

	if (unsupported_stage_mask || unsupported_gs_stage || ge_group_size || ge_shader_regs) {
		g_draw_geometry.fetch_add(1, std::memory_order_relaxed);
		if ((ctx.GetRenderTarget(0).base.addr & 0xfffffe000000ULL) == 0x8fc0000000ULL) {
			static std::atomic<uint32_t> cb4_output_ge_skip_log {0};
			const auto count = cb4_output_ge_skip_log.fetch_add(1);
			if (count < 12 || count % 64 == 0) {
				LOGF("CB4 output GE skip: rt=0x%016" PRIx64 " mask=0x%08" PRIx32
				     " ps=0x%016" PRIx64 "\n", ctx.GetRenderTarget(0).base.addr,
				     ctx.GetRenderTargetMask(), sh_ctx.GetPs().ps_regs.data_addr);
			}
		}
		const auto log_id = g_shader_stage_log_count.fetch_add(1);
		if (log_id < 32 || (log_id % 256) == 0) {
			LOGF("Skipping unsupported GE shader draw: stages=0x%08" PRIx32
			     " prim_group=0x%04" PRIx16 " vert_group=0x%04" PRIx16 " ngg=0x%08" PRIx32
			     " max_out=0x%08" PRIx32 " gs_max_vert=0x%08" PRIx32 " gs_out_prim=0x%08" PRIx32
			     " es=0x%016" PRIx64 " gs=0x%016" PRIx64 " vertices=%u instances=%u\n",
			     stages, ge_cntl.primitive_group_size, ge_cntl.vertex_group_size,
			     sh_regs.m_geNggSubgrpCntl, sh_regs.m_geMaxOutputPerSubgroup,
			     sh_regs.m_vgtGsMaxVertOut, sh_regs.m_vgtGsOutPrimType,
			     vertex_info.es_regs.data_addr, vertex_info.gs_regs.data_addr,
			     g_skip_debug_index_count.load(std::memory_order_relaxed),
			     g_skip_debug_instance_count.load(std::memory_order_relaxed));
		}
		return true;
	}

	return false;
}

struct DrawRenderState {
	RenderDepthInfo           depth_info;
	RenderColorInfo           color_info[RENDER_COLOR_ATTACHMENTS_MAX] = {};
	uint32_t                  color_count                              = 0;
	bool                      ps_active                                = true;
	RenderState               rendering;
	ShaderVertexInputInfo     vs_input_info;
	ShaderPixelInputInfo      ps_input_info;
	std::span<const uint32_t> vs_shader;
	std::span<const uint32_t> ps_shader;
};

struct DrawCallInfo {
	const char*          name           = nullptr;
	CommandBufferDebugOp debug_op       = CommandBufferDebugOp::DrawIndex;
	uint32_t             index_count    = 0;
	uint32_t             flags          = 0;
	uint32_t             instance_count = 0;
	uint32_t             first_instance = 0;
};

std::optional<RenderState> RenderExecutor::AcquireRenderTargets(CommandBuffer& buffer,
                                                      RenderColorInfo* colors,
                                                      uint32_t color_count,
                                                      RenderDepthInfo& depth) {
	EXIT_IF(colors == nullptr || color_count > RENDER_COLOR_ATTACHMENTS_MAX);
	auto&       cache = m_context.GetTextureCache();
	RenderState state {};
	state.width                 = std::numeric_limits<uint32_t>::max();
	state.height                = std::numeric_limits<uint32_t>::max();
	state.num_layers            = std::numeric_limits<uint32_t>::max();
	state.num_color_attachments = color_count;
	uint32_t attachment_samples = 0;
	for (uint32_t i = 0; i < color_count; i++) {
		auto& target = colors[i];
		EXIT_IF(!target.image_id);
		const auto old_image = cache.ResolveOwner(target.image_id);
		if (old_image == nullptr || (!old_image->registered && !old_image->info.data.Empty()) ||
		    old_image->binding.needs_rebind) {
			if (old_image != nullptr) {
				old_image->binding = {};
			}
			target.image_id = cache.FindImage(target.desc);
			BindRenderTarget(target.image_id);
		}
		target.image_view = cache.FindRenderTarget(target.image_id, target.desc);
		auto& image       = cache.GetImage(target.image_id);
		EXIT_IF(image.backing.samples != target.samples);
		if (target.image_view == nullptr) {
			// KytyPlus: the render target has no backing image (its creation was soft-skipped),
			// so no view exists. Skip this draw instead of aborting the process.
			SOFT_EXIT("render target has no view; skipping draw\n");
			return std::nullopt;
		}
		if (attachment_samples == 0) {
			attachment_samples = target.samples;
		} else if (attachment_samples != target.samples) {
			EXIT("mixed color attachment sample counts are unsupported: %u and %u\n",
			     attachment_samples, target.samples);
		}
		const auto& view   = target.desc.view_info;
		const auto  layout = image.binding.is_bound ? vk::ImageLayout::eGeneral
		                                            : vk::ImageLayout::eColorAttachmentOptimal;
		image.Transit(layout,
		              vk::AccessFlagBits2::eColorAttachmentRead |
		                  vk::AccessFlagBits2::eColorAttachmentWrite,
		              ImageSubresourceRange {view.base_level, view.level_count, view.base_layer,
		                                     view.layer_count},
		              buffer.Handle());
		state.width             = std::min(state.width, target.extent.width);
		state.height            = std::min(state.height, target.extent.height);
		state.num_layers        = std::min(state.num_layers, view.layer_count);
		auto& attachment        = state.color_attachments[i];
		attachment.image_view   = target.image_view;
		attachment.image_layout = layout;
		attachment.clear_value  = target.color_clear_value.uint32;
		attachment.is_clear     = target.color_clear_enable;
		if (target.color_clear_enable && target.metadata_addr != 0 &&
		    !cache.TouchMeta(target.metadata_addr, target.base_array_layer, false)) {
			EXIT("failed to consume color metadata clear state\n");
		}
	}
	if (depth.image_id) {
		const auto owner = cache.ResolveOwner(depth.image_id);
		if (owner == nullptr || !owner->registered || owner->binding.needs_rebind) {
			EXIT("depth target changed after render-state discovery\n");
		}
		depth.image_view = cache.FindDepthTarget(depth.image_id, depth.desc);
		if (depth.htile && depth.depth_clear_enable && !cache.ClearMeta(depth.htile_buffer_vaddr)) {
			EXIT("failed to acquire HTile metadata for a depth clear\n");
		}
		depth.depth_meta_clear_enable =
		    depth.htile &&
		    cache.IsMetaCleared(depth.htile_buffer_vaddr, depth.desc.view_info.base_layer);
		depth.depth_load_clear_enable = depth.depth_clear_enable || depth.depth_meta_clear_enable;
		if (depth.depth_meta_clear_enable &&
		    !cache.TouchMeta(depth.htile_buffer_vaddr, depth.desc.view_info.base_layer, false)) {
			EXIT("failed to consume HTile clear state\n");
		}
		auto& image = cache.GetImage(depth.image_id);
		EXIT_IF(image.backing.samples != depth.samples);
		if (depth.image_view == nullptr) {
			// KytyPlus: see the color-attachment case above.
			SOFT_EXIT("depth target has no view; skipping draw\n");
			return std::nullopt;
		}
		if (attachment_samples == 0) {
			attachment_samples = depth.samples;
		} else if (attachment_samples != depth.samples) {
			EXIT("mixed color/depth sample counts are unsupported: %u and %u\n", attachment_samples,
			     depth.samples);
		}
		const auto layout = depth_attachment_layout(depth);
		const auto writes = depth.AttachmentWriteAspects();
		auto       access = vk::AccessFlags2 {vk::AccessFlagBits2::eDepthStencilAttachmentRead};
		if (writes) {
			access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
		}
		const auto& view = depth.desc.view_info;
		image.Transit(layout, access,
		              ImageSubresourceRange {view.base_level, view.level_count, view.base_layer,
		                                     view.layer_count},
		              buffer.Handle());
		state.width               = std::min(state.width, depth.width);
		state.height              = std::min(state.height, depth.height);
		state.num_layers          = std::min(state.num_layers, view.layer_count);
		const auto aspects        = ImageViewOps::DepthAspectMask(depth.format);
		auto&      attachment     = state.depth_stencil_attachment;
		attachment.image_view     = depth.image_view;
		attachment.image_layout   = layout;
		attachment.clear_value[0] = std::bit_cast<uint32_t>(depth.depth_clear_value);
		attachment.clear_value[1] = depth.stencil_clear_value;
		attachment.has_depth      = static_cast<bool>(aspects & vk::ImageAspectFlagBits::eDepth);
		attachment.depth_clear    = depth.depth_load_clear_enable;
		attachment.has_stencil    = static_cast<bool>(aspects & vk::ImageAspectFlagBits::eStencil);
		attachment.stencil_clear  = depth.stencil_clear_enable;
	}
	if (attachment_samples == 0 ||
	    vulkan_sample_count(attachment_samples) == vk::SampleCountFlagBits {}) {
		EXIT("render state has no valid attachments\n");
	}
	if (state.num_layers == std::numeric_limits<uint32_t>::max()) {
		state.num_layers = 1;
	}
	EXIT_IF(state.width == 0 || state.height == 0 || state.num_layers == 0 ||
	        state.width == std::numeric_limits<uint32_t>::max() ||
	        state.height == std::numeric_limits<uint32_t>::max());
	return state;
}

static bool DrawHasActivePixelShader(const RenderCommandBuffer& buffer,
                                     const DrawRenderState& state, const DrawCallInfo& draw) {
	EXIT_IF(draw.name == nullptr);
	const auto& ctx    = buffer.GetRegisters();
	const auto& sh_ctx = buffer.GetShaders();

	const bool with_depth = (state.depth_info.format != vk::Format::eUndefined &&
	                         static_cast<bool>(state.depth_info.image_id));
	if (state.color_count != 0 || !with_depth) {
		return true;
	}

	const auto& sh_regs = ctx.GetShaderRegisters();
	const auto& ps      = sh_ctx.GetPs();
	return ShaderAddressValid(ps.ps_regs.data_addr) &&
	       PixelShaderHasDepthOrCoverageSideEffects(sh_regs);
}

enum class CbColorMode : uint8_t {
	Disable            = 0,
	Normal             = 1,
	EliminateFastClear = 2,
	Resolve            = 3,
	FmaskDecompress    = 5,
	DccDecompress      = 6,
};

static bool ConsumeMetadataColorOperation(const RenderCommandBuffer& buffer) {
	const auto& ctx  = buffer.GetRegisters();
	const auto  mode = ctx.GetColorControl().mode;
	const auto& rt0 = ctx.GetRenderTarget(0);
	if (mode == 2 || mode == 5 || mode == 6) {
		static std::atomic<uint64_t> cb4_output_metadata_count {0};
		const auto count = cb4_output_metadata_count.fetch_add(1);
		if (count < 128 || count % 1024 == 0) {
			LOGF("CB4 metadata: count=%llu mode=%u rt0=0x%016" PRIx64
			     " cmask=0x%016" PRIx64 " fast_clear=%u clear0=0x%08" PRIx32
			     " clear1=0x%08" PRIx32 " rt1=0x%016" PRIx64 "\n",
			     static_cast<unsigned long long>(count), mode, rt0.base.addr,
			     rt0.cmask.addr, rt0.info.cmask_fast_clear_enable ? 1u : 0u,
			     rt0.clear_word0.word0, rt0.clear_word1.word1,
			     ctx.GetRenderTarget(1).base.addr);
		}
	}
	// These AGC CB modes run color-buffer metadata/decompression operations. The shader is a
	// dummy vehicle for the CB, and its exported color must not be applied as a normal draw.
	// Kyty currently stores host images as expanded Vulkan images and does not track CMASK/DCC
	// metadata state, so the matching host operation is a no-op.
	return mode == static_cast<uint8_t>(CbColorMode::EliminateFastClear) ||
	       mode == static_cast<uint8_t>(CbColorMode::FmaskDecompress) ||
	       mode == static_cast<uint8_t>(CbColorMode::DccDecompress);
}

struct DrawEmitInfo {
	bool     indexed           = false;
	bool     draw_prim7_as_ngg = false;
	uint32_t draw_vertex_count = 0;
	int32_t  vertex_offset     = 0;
	uint32_t first_vertex      = 0;
};

struct DrawIndexBufferSource {
	bool          enabled   = false;
	uint64_t      address   = 0;
	const void*   host_data = nullptr;
	uint64_t      size      = 0;
	vk::IndexType type      = vk::IndexType::eUint16;
};

struct PreparedIndexBuffer {
	std::shared_ptr<void> owner;
	vk::Buffer            buffer   = nullptr;
	uint64_t              address  = 0;
	uint64_t              size     = 0;
	vk::DeviceSize        offset   = 0;
	vk::IndexType         type     = vk::IndexType::eUint16;
	bool                  streamed = false;
};

static uint64_t VertexBufferDescriptorSize(const ShaderVertexInputBuffer& buffer) {
	return (buffer.stride != 0 ? static_cast<uint64_t>(buffer.stride) * buffer.num_records
	                           : buffer.num_records);
}

struct VertexBufferRange {
	uint64_t      base_address  = 0;
	uint64_t      requested_end = 0;
	uint64_t      acquired_end  = 0;
	BufferBinding binding;

	[[nodiscard]] uint64_t RequestedSize() const { return requested_end - base_address; }
};

struct PreparedVertexBuffers {
	static constexpr uint32_t MaxBuffers = ShaderVertexInputInfo::RES_MAX;

	std::array<vk::Buffer, MaxBuffers>            buffers {};
	std::array<vk::DeviceSize, MaxBuffers>        offsets {};
	std::array<std::shared_ptr<void>, MaxBuffers> owners {};
	uint32_t                                      count       = 0;
	uint32_t                                      owner_count = 0;
};

static PreparedVertexBuffers AcquireVertexBuffers(RenderCommandBuffer&         buffer,
                                                  const ShaderVertexInputInfo& vs_input_info) {
	EXIT_IF(vs_input_info.buffers_num < 0 ||
	        vs_input_info.buffers_num > ShaderVertexInputInfo::RES_MAX);

	// Collect the non-empty guest vertex ranges.
	std::array<VertexBufferRange, ShaderVertexInputInfo::RES_MAX> ranges {};
	uint32_t                                                      range_count = 0;
	for (int i = 0; i < vs_input_info.buffers_num; i++) {
		const auto& vertex = vs_input_info.buffers[i];
		const auto  size   = VertexBufferDescriptorSize(vertex);
		if (size == 0) {
			continue;
		}
		if (vertex.addr == 0 || size > UINT64_MAX - vertex.addr) {
			EXIT("invalid vertex buffer range: addr=0x%016" PRIx64 " size=0x%016" PRIx64 "\n",
			     vertex.addr, size);
		}
		ranges[range_count++] = {vertex.addr, vertex.addr + size};
	}

	std::sort(ranges.begin(), ranges.begin() + range_count,
	          [](const VertexBufferRange& left, const VertexBufferRange& right) {
		          return left.base_address < right.base_address;
		          });

	// Merge overlapping or touching ranges before acquiring host buffers.
	std::array<VertexBufferRange, ShaderVertexInputInfo::RES_MAX> merged_ranges {};
	uint32_t                                                      merged_count = 0;
	for (uint32_t i = 0; i < range_count; i++) {
		const auto& range = ranges[i];
		if (merged_count != 0 &&
		    merged_ranges[merged_count - 1].requested_end >= range.base_address) {
			merged_ranges[merged_count - 1].requested_end =
			    std::max(merged_ranges[merged_count - 1].requested_end, range.requested_end);
			continue;
		}
		merged_ranges[merged_count++] = {range.base_address, range.requested_end};
	}

	auto& cache = buffer.GetContext().GetBufferCache();
	for (uint32_t i = 0; i < merged_count; i++) {
		auto& range = merged_ranges[i];
		// PPSA20298
		const auto size =
		    Libs::LibKernel::Memory::ClampRangeSize(range.base_address, range.RequestedSize());
		range.acquired_end = range.base_address + size;
		range.binding      = cache.ObtainBuffer(buffer, range.base_address, size);
	}

	// Rebuild slot bindings, offsetting non-empty slots into their acquired merged range.
	PreparedVertexBuffers prepared;
	prepared.count = static_cast<uint32_t>(vs_input_info.buffers_num);
	std::shared_ptr<Buffer> null_owner;
	vk::Buffer              null_buffer = nullptr;
	for (int i = 0; i < vs_input_info.buffers_num; i++) {
		const auto& vertex = vs_input_info.buffers[i];
		const auto  size   = VertexBufferDescriptorSize(vertex);
		if (size == 0) {
			if (null_owner == nullptr) {
				null_owner  = cache.ObtainNullBuffer();
				null_buffer = null_owner->Handle();
			}
			prepared.buffers[i] = null_buffer;
			prepared.offsets[i] = 0;
			continue;
		}

		const auto range = std::find_if(merged_ranges.begin(), merged_ranges.begin() + merged_count,
		                                [&](const VertexBufferRange& value) {
			                                return vertex.addr >= value.base_address &&
			                                       vertex.addr < value.acquired_end;
			                                });
		if (range == merged_ranges.begin() + merged_count) {
			EXIT("vertex buffer address is outside the acquired range: addr=0x%016" PRIx64 "\n",
			     vertex.addr);
		}

		prepared.buffers[i] = range->binding.buffer;
		prepared.offsets[i] = range->binding.offset + vertex.addr - range->base_address;
	}

	if (null_owner != nullptr) {
		prepared.owners[prepared.owner_count++] = std::move(null_owner);
	}
	for (uint32_t i = 0; i < merged_count; i++) {
		if (merged_ranges[i].binding.owner != nullptr) {
			EXIT_IF(prepared.owner_count >= PreparedVertexBuffers::MaxBuffers);
			prepared.owners[prepared.owner_count++] = std::move(merged_ranges[i].binding.owner);
		}
	}
	return prepared;
}

static void SetDrawDebugPhase(RenderCommandBuffer& buffer, uint64_t submit_id,
                              const DrawCallInfo& draw, uint32_t phase) {
	EXIT_IF(draw.name == nullptr);

	buffer.SetDebugInfo(static_cast<uint32_t>(draw.debug_op), submit_id, phase, draw.index_count,
	                    draw.flags, draw.instance_count, draw.first_instance);
}

static bool GetDrawTopology(const HW::UserConfig& ucfg, bool auto_draw, bool use_ngg_rectlist_draw,
                            vk::PrimitiveTopology& topology) {

	topology = vk::PrimitiveTopology::ePointList;

	switch (static_cast<Prospero::PrimitiveType>(ucfg.GetPrimType())) {
		case Prospero::PrimitiveType::kNone: return false;
		case Prospero::PrimitiveType::kPointList:
			topology = vk::PrimitiveTopology::ePointList;
			break;
		case Prospero::PrimitiveType::kLineList: topology = vk::PrimitiveTopology::eLineList; break;
		case Prospero::PrimitiveType::kLineStrip:
			topology = vk::PrimitiveTopology::eLineStrip;
			break;
		case Prospero::PrimitiveType::kTriList:
			topology = vk::PrimitiveTopology::eTriangleList;
			break;
		case Prospero::PrimitiveType::kTriFan:
			topology = vk::PrimitiveTopology::eTriangleFan;
			break;
		case Prospero::PrimitiveType::kTriStrip:
			topology = vk::PrimitiveTopology::eTriangleStrip;
			break;
		case Prospero::PrimitiveType::kRectList:
			topology = (auto_draw && use_ngg_rectlist_draw ? vk::PrimitiveTopology::eTriangleStrip
			                                               : vk::PrimitiveTopology::eTriangleList);
			break;
		case Prospero::PrimitiveType::kRectListLegacy:
			// Legacy rect lists are drawn as triangle strips. The previous code treated
			// this as fatal whenever auto_draw was false, which is exactly the case for
			// *indexed indirect* draws (the path PS5 GPU-driven culling uses), so the
			// entire 3D world was killed here while 2D/UI draws kept working. Resolve it
			// to a real topology instead; the indirect submit path decides on its own
			// whether it can honour the per-draw counts.
			topology = vk::PrimitiveTopology::eTriangleStrip;
			break;
		case Prospero::PrimitiveType::kQuadListLegacy:
			topology = vk::PrimitiveTopology::eTriangleFan;
			break;
		default: EXIT("unknown primitive type: %u\n", ucfg.GetPrimType());
	}

	return true;
}

bool RenderExecutor::PrepareDrawRenderState(uint64_t submit_id, RenderCommandBuffer& buffer,
                                            const DrawCallInfo& draw,
                                            uint32_t            render_target_slice_offset,
                                            bool log_setup_phases, DrawRenderState& state) {
	EXIT_IF(draw.name == nullptr);
	auto& ctx = buffer.GetRegisters();

	if (ResolveColorTargets(submit_id, buffer, render_target_slice_offset)) {
		return false;
	}
	if (log_setup_phases) {
		LogDrawPhase(draw.name, "ResolveRenderColorTarget");
	}
	for (uint32_t slot = 0; slot < RENDER_COLOR_ATTACHMENTS_MAX; slot++) {
		if (slot == 0 || (render_target_mask_slot(ctx.GetRenderTargetMask(), slot) != 0 &&
		                  ctx.GetRenderTarget(slot).base.addr != 0)) {
			ResolveRenderColorTarget(submit_id, buffer, state.color_info[state.color_count],
			                         render_target_slice_offset, slot);
			if (state.color_info[state.color_count].image_id) {
				state.color_count++;
			}
		}
	}
	for (uint32_t i = 0; i < state.color_count; i++) {
		const auto& color = state.color_info[i];
		const auto& blend = ctx.GetBlendControl(color.target_slot);
		if (color.format == vk::Format::eR16G16B16A16Sfloat && blend.enable &&
		    blend.separate_alpha_blend &&
		    blend.alpha_srcblend == static_cast<uint8_t>(Prospero::BlendFactor::kZero) &&
		    blend.alpha_destblend == static_cast<uint8_t>(Prospero::BlendFactor::kOneMinusSrcAlpha)) {
			static std::atomic<uint64_t> alpha_preserve_log {0};
			const auto count = alpha_preserve_log.fetch_add(1);
			if (count < 64 || count % 512 == 0) {
				const auto& rt = ctx.GetRenderTarget(color.target_slot);
				LOGF("CB4 alpha preserve: count=%llu frame=%llu addr=0x%016" PRIx64
				     " cmask=0x%016" PRIx64 " fast_clear=%u clear0=0x%08" PRIx32
				     " clear1=0x%08" PRIx32 " ps=0x%016" PRIx64
				     " dcc=0x%016" PRIx64 " dcc_enable=%u dcc_key=%u\n",
				     static_cast<unsigned long long>(count),
				     static_cast<unsigned long long>(m_context.GetGpu().GetFrameNum()),
				     rt.base.addr, rt.cmask.addr, rt.info.cmask_fast_clear_enable ? 1u : 0u,
				     rt.clear_word0.word0, rt.clear_word1.word1,
				     buffer.GetShaders().GetPs().ps_regs.data_addr, rt.dcc_addr.addr,
				     rt.info.dcc_compression_enable ? 1u : 0u,
				     rt.dcc.dcc_clear_key_enable ? 1u : 0u);
			}
		}
	}
	if (log_setup_phases) {
		LogDrawPhase(draw.name, "ResolveRenderDepthTarget");
	}
	ResolveRenderDepthTarget(submit_id, buffer, state.depth_info);
	for (uint32_t i = 0; i < state.color_count; i++) {
		if ((state.color_info[i].base_addr & 0xfffffe000000ULL) == 0x8fc0000000ULL) {
			g_draw_video_attempts.fetch_add(1, std::memory_order_relaxed);
			break;
		}
	}

	const bool with_depth = (state.depth_info.format != vk::Format::eUndefined &&
	                         static_cast<bool>(state.depth_info.image_id));
	if (state.color_count == 0 && !with_depth) {
		g_draw_framebuffer.fetch_add(1, std::memory_order_relaxed);
		if (m_context.GetGpu().GetFrameNum() >= 14500) {
			static std::atomic<uint32_t> cb4_no_fb_log {0};
			if (cb4_no_fb_log.fetch_add(1) < 64) {
				const auto& shaders = buffer.GetShaders();
				LOGF("CB4 no framebuffer: frame=%d mode=%u mask=0x%08" PRIx32
				     " rt0=0x%016" PRIx64 " rt1=0x%016" PRIx64
				     " vs=0x%016" PRIx64 " ps=0x%016" PRIx64 "\n",
				     m_context.GetGpu().GetFrameNum(), ctx.GetColorControl().mode,
				     ctx.GetRenderTargetMask(), ctx.GetRenderTarget(0).base.addr,
				     ctx.GetRenderTarget(1).base.addr, shaders.GetVs().es_regs.data_addr,
				     shaders.GetPs().ps_regs.data_addr);
			}
		}
		LogFramebufferSkip(draw.name, state.color_info[0], state.depth_info, buffer,
		                   draw.index_count, draw.flags);
		return false;
	}
	state.ps_active = DrawHasActivePixelShader(buffer, state, draw);

	return true;
}

static bool RefreshShaders(RenderCommandBuffer& buffer, const DrawCallInfo& draw, bool log_phases,
                           DrawRenderState& state) {
	EXIT_IF(draw.name == nullptr);
	auto& ctx    = buffer.GetRegisters();
	auto& sh_ctx = buffer.GetShaders();

	const auto& vertex_shader_info = sh_ctx.GetVs();
	const auto& pixel_shader_info  = sh_ctx.GetPs();
	const auto& shader_regs        = ctx.GetShaderRegisters();

	state.vs_shader     = {};
	state.ps_shader     = {};
	state.ps_input_info = {};
	std::array<Prospero::ColorComponentMapping, RENDER_COLOR_ATTACHMENTS_MAX>
	    target_export_mapping {};
	for (uint32_t i = 0; i < state.color_count; i++) {
		target_export_mapping[state.color_info[i].target_slot] = state.color_info[i].export_mapping;
	}
	const auto lane_mask_mode = SelectGraphicsLaneMaskMode(64u);

	if (log_phases) {
		LogDrawPhase(draw.name, "ShaderCompileInfoVS");
	}
	if (!ShaderCompileInfoVS(vertex_shader_info, shader_regs, lane_mask_mode, state.vs_input_info,
	                         state.vs_shader, CanFoldTriangleCopyDraw(buffer))) {
		// KytyPlus: the recompiler already reported the specific gap. Skip this draw and
		// keep rendering rather than aborting the process on one unsupported instruction.
		LOGF("GraphicsRender%s: skipping draw, VS recompile failed\n", draw.name);
		return false;
	}

	if (!state.ps_active) {
		return true;
	}
	if (log_phases) {
		LogDrawPhase(draw.name, "ShaderCompileInfoPS");
	}
	if (!ShaderCompileInfoPS(pixel_shader_info, shader_regs, lane_mask_mode, state.vs_input_info,
	                         target_export_mapping, state.ps_input_info, state.ps_shader)) {
		// KytyPlus: see the VS note above. Skip the draw instead of aborting.
		LOGF("GraphicsRender%s: skipping draw, PS recompile failed\n", draw.name);
		return false;
	}
	return true;
}

static PreparedVertexBuffers PrepareVertexBuffers(uint64_t                     submit_id,
                                                       RenderCommandBuffer&         buffer,
                                                       const DrawCallInfo&          draw,
                                                       const ShaderVertexInputInfo& vs_input_info) {
	EXIT_IF(draw.name == nullptr);
	(void)submit_id;

	LogDrawPhase(draw.name, "PrepareVertexBuffers");
	return AcquireVertexBuffers(buffer, vs_input_info);
}

static void RebindVertexBuffers(RenderCommandBuffer&         buffer,
                                const ShaderVertexInputInfo& vs_input_info,
                                PreparedVertexBuffers&     prepared) {
	prepared = AcquireVertexBuffers(buffer, vs_input_info);
}

static PreparedIndexBuffer PrepareIndexBuffer(RenderCommandBuffer&         buffer,
                                              const DrawIndexBufferSource& source) {
	PreparedIndexBuffer prepared;
	if (!source.enabled) {
		return prepared;
	}
	EXIT_IF(source.size == 0);
	prepared.address = source.address;
	prepared.size    = source.size;
	prepared.type    = source.type;
	if (source.host_data != nullptr) {
		auto binding =
		    buffer.GetContext().GetBufferCache().UploadTransient(source.host_data, source.size, 16);
		prepared.owner    = std::move(binding.owner);
		prepared.buffer   = binding.buffer;
		prepared.offset   = binding.offset;
		prepared.streamed = true;
	} else {
		auto binding =
		    buffer.GetContext().GetBufferCache().ObtainBuffer(buffer, source.address, source.size);
		prepared.owner  = std::move(binding.owner);
		prepared.buffer = binding.buffer;
		prepared.offset = binding.offset;
	}
	return prepared;
}

static void RebindIndexBuffer(RenderCommandBuffer& buffer, PreparedIndexBuffer& prepared) {
	if (prepared.size == 0 || prepared.streamed) {
		return;
	}
	auto binding =
	    buffer.GetContext().GetBufferCache().ObtainBuffer(buffer, prepared.address, prepared.size);
	prepared.owner  = std::move(binding.owner);
	prepared.buffer = binding.buffer;
	prepared.offset = binding.offset;
}

static void CommitVertexBuffers(RenderCommandBuffer& buffer, vk::CommandBuffer vk_buffer,
                                PreparedVertexBuffers& prepared) {
	for (uint32_t i = 0; i < prepared.owner_count; i++) {
		if (prepared.owners[i] != nullptr) {
			buffer.RetainResourceUntilFence(std::move(prepared.owners[i]));
		}
	}
	for (uint32_t i = 0; i < prepared.count; i++) {
		EXIT_IF(prepared.buffers[i] == nullptr);
	}
	if (prepared.count != 0) {
		vk_buffer.bindVertexBuffers(0, prepared.count, prepared.buffers.data(),
		                            prepared.offsets.data());
	}
}

static void CommitIndexBuffer(RenderCommandBuffer& buffer, vk::CommandBuffer vk_buffer,
                              const PreparedIndexBuffer& prepared) {
	if (prepared.size == 0) {
		return;
	}
	if (prepared.owner != nullptr) {
		buffer.RetainResourceUntilFence(prepared.owner);
	}
	EXIT_IF(prepared.buffer == nullptr);
	vk_buffer.bindIndexBuffer(prepared.buffer, prepared.offset, prepared.type);
}

// Acquire the guest buffer holding GPU-written indirect draw arguments so it can serve as a
// Vulkan indirect buffer. Buffer-cache allocations already carry eIndirectBuffer usage (see
// ReadFlags in streamBuffer.h), so the compute shader writes stay in place and no host
// round-trip is required.
static PreparedIndexBuffer PrepareIndirectBuffer(RenderCommandBuffer& buffer,
                                                  const DrawIndirectSource& source) {
	PreparedIndexBuffer prepared;
	if (!source.enabled || source.draw_count == 0) {
		return prepared;
	}
	EXIT_IF(source.args_vaddr == 0);
	const uint32_t stride    = source.stride_bytes != 0 ? source.stride_bytes : 1;
	const uint64_t args_size = static_cast<uint64_t>(source.draw_count) * stride;
	auto binding = buffer.GetContext().GetBufferCache().ObtainBuffer(
	    buffer, source.args_vaddr, args_size, /*is_written=*/false, /*is_read=*/true);
	prepared.owner  = std::move(binding.owner);
	prepared.buffer = binding.buffer;
	prepared.offset = binding.offset;
	prepared.size   = args_size;
	return prepared;
}

static void LogDrawStateIfNeeded(const RenderCommandBuffer& buffer, const DrawCallInfo& draw,
                                 const DrawRenderState& state, bool always_log,
                                 bool force_legacy_rect_log, uint32_t index_type_and_size,
                                 const void* index_addr) {
	EXIT_IF(draw.name == nullptr);

	if (!graphics_debug_dump_enabled()) {
		return;
	}

	if (!always_log && !force_legacy_rect_log) {
		return;
	}

	if (state.ps_active) {
		LogDrawTargetState(draw.name, state.color_info[0], state.depth_info, buffer,
		                   state.ps_input_info, draw.index_count, draw.flags);
	}
	LogDrawInputState(buffer, state.color_info[0], state.vs_input_info, index_type_and_size,
	                  draw.index_count, index_addr);
	// LogDrawTextureState(draw.name, state.color_info[0], state.ps_input_info);
}

static bool IsHostExpandedRectListDrawSupported(const ShaderVertexInputInfo& vs_input_info,
                                                const DrawCallInfo&          draw,
                                                const DrawEmitInfo&          emit) {
	if (!emit.draw_prim7_as_ngg) {
		return true;
	}

	if (vs_input_info.buffers_num != 0) {
		return false;
	}

	return draw.index_count == 3 || draw.index_count == emit.draw_vertex_count;
}

static void EmitDrawPrimitives(const HW::UserConfig& ucfg, vk::CommandBuffer vk_buffer,
                               const ShaderVertexInputInfo& vs_input_info, const DrawCallInfo& draw,
                               const DrawEmitInfo& emit) {
	EXIT_IF(draw.name == nullptr);

	switch (static_cast<Prospero::PrimitiveType>(ucfg.GetPrimType())) {
		case Prospero::PrimitiveType::kPointList:
		case Prospero::PrimitiveType::kLineList:
		case Prospero::PrimitiveType::kLineStrip:
		case Prospero::PrimitiveType::kTriList:
		case Prospero::PrimitiveType::kTriFan:
		case Prospero::PrimitiveType::kTriStrip:
			if (emit.indexed) {
				vk_buffer.drawIndexed(draw.index_count, draw.instance_count, 0, emit.vertex_offset,
				                      draw.first_instance);
			} else {
				vk_buffer.draw(draw.index_count, draw.instance_count, emit.first_vertex,
				               draw.first_instance);
			}
			break;
		case Prospero::PrimitiveType::kRectList:
			if (emit.indexed) {
				vk_buffer.drawIndexed(draw.index_count, draw.instance_count, 0, emit.vertex_offset,
				                      draw.first_instance);
			} else {
				EXIT_NOT_IMPLEMENTED(
				    !IsHostExpandedRectListDrawSupported(vs_input_info, draw, emit));
				vk_buffer.draw(emit.draw_vertex_count, draw.instance_count, emit.first_vertex,
				               draw.first_instance);
			}
			break;
		case Prospero::PrimitiveType::kRectListLegacy:
			if (emit.indexed) {
				EXIT("unknown primitive type: %u\n", ucfg.GetPrimType());
			}
			// Sarah
			EXIT_NOT_IMPLEMENTED(!(draw.index_count == 3 && vs_input_info.buffers_num == 0));
			vk_buffer.draw(4, draw.instance_count, emit.first_vertex, draw.first_instance);
			break;
		case Prospero::PrimitiveType::kQuadListLegacy:
			EXIT_NOT_IMPLEMENTED((draw.index_count & 0x3u) != 0);
			for (uint32_t i = 0; i < draw.index_count; i += 4) {
				if (emit.indexed) {
					vk_buffer.drawIndexed(4, draw.instance_count, i, emit.vertex_offset,
					                      draw.first_instance);
				} else {
					vk_buffer.draw(4, draw.instance_count, i + emit.first_vertex,
					               draw.first_instance);
				}
			}
			break;
		default: EXIT("unknown primitive type: %u\n", ucfg.GetPrimType());
	}
}

void RenderExecutor::ExecutePreparedDraw(uint64_t submit_id, RenderCommandBuffer& buffer,
                                         const DrawCallInfo& draw, DrawRenderState& state,
                                         vk::PrimitiveTopology topology, const DrawEmitInfo& emit,
                                         const DrawIndexBufferSource& index_source,
                                         bool log_pipeline_phase, bool set_bind_debug,
                                         bool set_auto_debug,
                                         const DrawIndirectSource& indirect) {
	EXIT_IF(draw.name == nullptr);
	auto& ucfg = buffer.GetUserConfig();

	LogDrawPhase(draw.name, "PrepareBindings");
	auto bindings        = PrepareGraphicsBindings(buffer, state.vs_input_info.stage,
	                                               state.ps_input_info.stage, state.ps_active);
	if (!bindings.valid) {
		// KytyPlus: an image binding could not be produced (its backing image was never
		// created). The descriptor set would receive a null view, so skip this draw entirely
		// rather than aborting the process.
		LogDrawPhase(draw.name, "BindingsUnavailable-SkipDraw");
		g_draw_bindings_missing.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	auto vertex_bindings = PrepareVertexBuffers(submit_id, buffer, draw, state.vs_input_info);
	auto index_binding   = PrepareIndexBuffer(buffer, index_source);
	RebindVertexBuffers(buffer, state.vs_input_info, vertex_bindings);
	RebindIndexBuffer(buffer, index_binding);
	{
		auto rendering =
		    AcquireRenderTargets(buffer, state.color_info, state.color_count, state.depth_info);
		if (!rendering.has_value()) {
			LogDrawPhase(draw.name, "RenderTargetUnavailable-SkipDraw");
			g_draw_rt_missing.fetch_add(1, std::memory_order_relaxed);
			return;
		}
		state.rendering = *rendering;
	}

	if (log_pipeline_phase) {
		LogDrawPhase(draw.name, "CreatePipeline");
	}
	auto& pipeline = m_context.GetPipelineCache().CreateGraphicsPipeline(
	    state.color_info, state.color_count, state.depth_info, state.vs_input_info, buffer,
	    &state.ps_input_info, topology, state.ps_active, state.vs_shader, state.ps_shader);

	// Async pipeline compilation: when the pipeline is being compiled on a worker
	// thread, CreateGraphicsPipeline returns a sentinel with a null pipeline handle.
	// Skip recording this draw entirely (no bindPipeline, no vkCmdDraw) so the frame
	// keeps flowing; the next frames retry and bind the real pipeline once it lands.
	// A few skipped draws are far cheaper than blocking the GPU thread on a
	// multi-100 ms vkCreateGraphicsPipelines call.
	if (pipeline.pipeline == nullptr) {
		for (uint32_t i = 0; i < state.color_count; i++) {
			if ((state.color_info[i].base_addr & 0xfffffe000000ULL) == 0x8fc0000000ULL) {
				g_draw_video_pending.fetch_add(1, std::memory_order_relaxed);
				break;
			}
		}
		LogDrawPhase(draw.name, "PipelinePending-SkipDraw");
		g_draw_pipeline_pending.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	// Resource preparation above may synchronously finish and restart the scheduler. From this
	// point onward, every operation targets the current command buffer and cannot touch guest
	// memory.
	auto       vk_buffer = buffer.Handle();
	if (set_bind_debug) {
		SetDrawDebugPhase(buffer, submit_id, draw, 0x100u);
	}
	if (set_auto_debug) {
		SetDrawDebugPhase(buffer, submit_id, draw, 0x200u);
	}
	CommitVertexBuffers(buffer, vk_buffer, vertex_bindings);
	CommitBindings(buffer, vk::PipelineBindPoint::eGraphics, pipeline.pipeline_layout,
	               bindings.vertex);
	if (bindings.pixel.has_value()) {
		if (set_auto_debug) {
			SetDrawDebugPhase(buffer, submit_id, draw, 0x300u);
		}
		CommitBindings(buffer, vk::PipelineBindPoint::eGraphics, pipeline.pipeline_layout,
		               *bindings.pixel);
	}
	CommitIndexBuffer(buffer, vk_buffer, index_binding);
	auto indirect_binding = indirect.enabled ? PrepareIndirectBuffer(buffer, indirect)
	                                        : PreparedIndexBuffer {};

	const auto dynamic_params =
	    BuildGraphicsDynamicParams(buffer, state.color_info, state.color_count, state.depth_info);
	// No viewport scaling here: the guest's render targets are scaled where the guest
	// declares them (CB_COLOR0_ATTRIB2), so the viewports it derives from those
	// dimensions are already correct. Overriding them too would double-apply the scale.
	SetDynamicParams(buffer, vk_buffer, dynamic_params);

	LogDrawPhase(draw.name, "BeginRendering");
	if (set_auto_debug) {
		SetDrawDebugPhase(buffer, submit_id, draw, 0x400u);
	}
	m_context.GetCommandScheduler().BeginRendering(state.rendering);
	vk_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
	if (set_auto_debug) {
		SetDrawDebugPhase(buffer, submit_id, draw, 0x500u);
	}
	if (indirect.enabled && indirect_binding.buffer != nullptr) {
		// GPU-driven draw. The vertex/index counts, instance counts and base offsets are read
		// by the GPU straight out of the guest buffer the culling compute shader wrote, so no
		// host synchronization (and no per-draw GPU stall) is needed. The record count is
		// clamped to the argument records that actually fit in the acquired range.
		if (indirect_binding.owner != nullptr) {
			buffer.RetainResourceUntilFence(std::move(indirect_binding.owner));
		}
		const auto     stride    = static_cast<vk::DeviceSize>(indirect.stride_bytes);
		const uint32_t max_draws = static_cast<uint32_t>(indirect_binding.size / stride);
		const uint32_t count     = std::min(indirect.draw_count, max_draws);
		if (count == 0) {
			return;
		}
		switch (static_cast<Prospero::PrimitiveType>(ucfg.GetPrimType())) {
			case Prospero::PrimitiveType::kPointList:
			case Prospero::PrimitiveType::kLineList:
			case Prospero::PrimitiveType::kLineStrip:
			case Prospero::PrimitiveType::kTriList:
			case Prospero::PrimitiveType::kTriFan:
			case Prospero::PrimitiveType::kTriStrip:
			// Rect lists (NGG and legacy) are the dominant PS5 geometry type. They carry
			// their per-vertex count in the indirect args, which the GPU reads directly, so
			// they can be issued as indirect draws exactly like the topologies above. These
			// were previously rejected as "unsupported", which silently dropped every
			// GPU-driven draw and left only the non-indirect 2D/UI geometry on screen.
			case Prospero::PrimitiveType::kRectList:
			case Prospero::PrimitiveType::kRectListLegacy:
				// Each record supplies its own counts, instance count and base vertex, so the
				// host never needs to read any of them. One command per record is issued
				// because this Vulkan-Hpp only exposes the single-draw form.
				if (indirect.indexed) {
					for (uint32_t i = 0; i < count; i++) {
						vk_buffer.drawIndexedIndirect(indirect_binding.buffer,
						                             indirect_binding.offset + i * stride, 1,
						                             static_cast<uint32_t>(stride));
					}
				} else {
					for (uint32_t i = 0; i < count; i++) {
						vk_buffer.drawIndirect(indirect_binding.buffer,
						                       indirect_binding.offset + i * stride, 1,
						                       static_cast<uint32_t>(stride));
					}
				}
				break;
			default:
				// The remaining primitive types need per-draw CPU knowledge of the vertex or
				// index count (triangle fans, rect lists, legacy quads), which only an indirect
				// readback could provide. Skip rather than draw garbage.
				LogDrawPhase(draw.name, "IndirectUnsupportedTopology-SkipDraw");
				g_draw_indirect_unsupported.fetch_add(1, std::memory_order_relaxed);
				return;
		}
		g_draw_indirect_submitted.fetch_add(count, std::memory_order_relaxed);
	} else {
		EmitDrawPrimitives(ucfg, vk_buffer, state.vs_input_info, draw, emit);
	}
	if (!Log::IsSilent()) {
		g_draw_submitted.fetch_add(1, std::memory_order_relaxed);
		// Per-draw geometry detail for non-video targets is diagnostic only.
		if (m_context.GetGpu().GetFrameNum() >= 900) {
			bool is_video = false;
			for (uint32_t i = 0; i < state.color_count; i++) {
				if ((state.color_info[i].base_addr & 0xfffffe000000ULL) == 0x8fc0000000ULL) {
					is_video = true;
				}
			}
			if (!is_video) {
				static std::atomic<uint32_t> cb4_world_draw_log {0};
				const auto                   n = cb4_world_draw_log.fetch_add(1);
				if (n < 20 || (n % 2000) == 0) {
					const auto& c0 = state.color_count != 0 ? state.color_info[0] : RenderColorInfo {};
					LOGF("WorldDraw: n=%llu frame=%d idx=%u inst=%u vbuf=%d rt=0x%016llx depth=%u ps=%d\n",
					     static_cast<unsigned long long>(n + 1), m_context.GetGpu().GetFrameNum(),
					     draw.index_count, draw.instance_count, state.vs_input_info.buffers_num,
					     c0.base_addr, state.depth_info.format != vk::Format::eUndefined ? 1u : 0u,
					     state.ps_active ? 1 : 0);
				}
			}
		}
		g_draw_vertices.fetch_add(
		    static_cast<uint64_t>(draw.index_count) * std::max<uint32_t>(draw.instance_count, 1u),
		    std::memory_order_relaxed);
		g_draw_instances.fetch_add(std::max<uint32_t>(draw.instance_count, 1u),
		                            std::memory_order_relaxed);
	}
	for (uint32_t i = 0; i < state.color_count; i++) {
		if ((state.color_info[i].base_addr & 0xfffffe000000ULL) != 0x8fc0000000ULL) {
			continue;
		}
		g_draw_video_submitted.fetch_add(1, std::memory_order_relaxed);
		if (m_context.GetGpu().GetFrameNum() >= 14500) {
			static std::atomic<uint32_t> cb4_output_submit_log {0};
			const auto count = cb4_output_submit_log.fetch_add(1);
			if (count < 12 || count % 64 == 0) {
				LOGF("CB4 output submit: frame=%d ps=0x%016" PRIx64 " rt=0x%016" PRIx64 " sources=%zu\n",
				     m_context.GetGpu().GetFrameNum(), buffer.GetShaders().GetPs().ps_regs.data_addr,
				     state.color_info[i].base_addr, bindings.pixel ? bindings.pixel->resources.images.size() : 0);
				if (bindings.pixel) {
					for (uint32_t j = 0; j < std::min<uint32_t>(bindings.pixel->resources.images.size(), 4); j++) {
						LOGF("CB4 output source[%u]: addr=0x%016" PRIx64 "\n", j,
						     bindings.pixel->resources.images[j].desc.info.data.address);
					}
				}
			}
		}
		break;
	}

	if (set_auto_debug) {
		SetDrawDebugPhase(buffer, submit_id, draw, 0x600u);
	}
	vk::PipelineStageFlags shader_write_stages = {};
	if (HasShaderBufferWrites(state.vs_input_info.stage) ||
	    HasShaderImageWrites(state.vs_input_info.stage)) {
		shader_write_stages |= vk::PipelineStageFlagBits::eVertexShader;
	}
	if (state.ps_active &&
	    (HasShaderBufferWrites(state.ps_input_info.stage) ||
	     HasShaderImageWrites(state.ps_input_info.stage))) {
		shader_write_stages |= vk::PipelineStageFlagBits::eFragmentShader;
	}
	if (shader_write_stages) {
		m_context.GetCommandScheduler().EndRendering();
		ShaderWriteBarrier(vk_buffer, shader_write_stages);
	}
	LogDrawPhase(draw.name, "DrawComplete");
	if (set_auto_debug) {
		SetDrawDebugPhase(buffer, submit_id, draw, 0x700u);
	}
}

void RenderExecutor::DrawIndex(uint64_t submit_id, RenderCommandBuffer& buffer,
                               uint32_t index_type_and_size, uint32_t index_count,
                               const void* index_addr, uint32_t flags, uint32_t type,
                               uint32_t instance_count, uint32_t render_target_slice_offset,
                               int32_t vertex_offset_add, uint32_t first_instance) {
	KYTY_PROFILER_FUNCTION();

	EXIT_IF(buffer.IsInvalid());
	auto& ucfg   = buffer.GetUserConfig();
	auto& sh_ctx = buffer.GetShaders();

	buffer.SetDebugInfo(static_cast<uint32_t>(CommandBufferDebugOp::DrawIndex), submit_id,
	                    index_count, flags, type, instance_count,
	                    reinterpret_cast<uint64_t>(index_addr));

	Common::LockGuard lock(m_context.GetMutex());
	if (index_count == 0) {
		return;
	}

	ReportDrawProgress();
	if (ConsumeMetadataColorOperation(buffer)) {
		g_draw_metadata.fetch_add(1, std::memory_order_relaxed);
		ResetBindings();
		return;
	}

	if (!DrawHasValidVertexShader(sh_ctx)) {
		g_draw_invalid_vs.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	// Record what this draw would have produced before the GE skip test runs.
	if (!Log::IsSilent()) {
		g_skip_debug_index_count.store(index_count, std::memory_order_relaxed);
		g_skip_debug_instance_count.store(instance_count, std::memory_order_relaxed);
	}
	if (ShouldSkipGeShader(buffer)) {
		return;
	}

	if (graphics_debug_dump_enabled()) {
		sh_print("GraphicsRenderDrawIndex():Shader:", sh_ctx);
		uc_print("GraphicsRenderDrawIndex():UserConfig:", ucfg);
		hw_print(buffer);

		LOGF("GraphicsRenderDrawIndex():Parameters:\n"
		     "\t index_type_and_size = 0x%08" PRIx32 "\n"
		     "\t index_count         = 0x%08" PRIx32 "\n"
		     "\t index_addr          = 0x%016" PRIx64 "\n"
		     "\t flags               = 0x%08" PRIx32 "\n"
		     "\t type                = 0x%08" PRIx32 "\n"
		     "\t instance_count      = 0x%08" PRIx32 "\n"
		     "\t rt_slice_offset     = 0x%08" PRIx32 "\n"
		     "\t vertex_offset_add   = 0x%08" PRIx32 "\n"
		     "\t first_instance      = 0x%08" PRIx32 "\n",
		     index_type_and_size, index_count, reinterpret_cast<uint64_t>(index_addr), flags, type,
		     instance_count, render_target_slice_offset, static_cast<uint32_t>(vertex_offset_add),
		     first_instance);
	}

	uc_check(ucfg);

	hw_check(buffer);

	vk::PrimitiveTopology topology = vk::PrimitiveTopology::ePointList;
	if (!GetDrawTopology(ucfg, false, false, topology)) {
		return;
	}

	vk::IndexType index_type           = vk::IndexType::eUint16;
	uint64_t      index_size           = 0;
	bool          expand_index8_to_u16 = false;

	switch (static_cast<Prospero::IndexType>(index_type_and_size)) {
		case Prospero::IndexType::kIndex16:
			index_type = vk::IndexType::eUint16;
			index_size = 2 * static_cast<uint64_t>(index_count);
			break;
		case Prospero::IndexType::kIndex32:
			index_type = vk::IndexType::eUint32;
			index_size = 4 * static_cast<uint64_t>(index_count);
			break;
		// Some games use it - need vulkan extension
		case Prospero::IndexType::kIndex8:
			index_type           = vk::IndexType::eUint16;
			index_size           = static_cast<uint64_t>(index_count);
			expand_index8_to_u16 = true;
			break;
		default: EXIT("unknown index_type_and_size: %u\n", index_type_and_size);
	}

	EXIT_NOT_IMPLEMENTED(flags != 0);
	EXIT_NOT_IMPLEMENTED(type != 1);
	const DrawCallInfo    draw {"DrawIndex",    CommandBufferDebugOp::DrawIndex,
	                            index_count,    flags,
	                            instance_count, first_instance};
	std::vector<uint16_t> expanded_indices;
	if (expand_index8_to_u16) {
		EXIT_NOT_IMPLEMENTED(index_addr == nullptr);
		const auto* src = static_cast<const uint8_t*>(index_addr);
		expanded_indices.resize(index_count);
		for (uint32_t i = 0; i < index_count; i++) {
			expanded_indices[i] = src[i];
		}
	}

	DrawIndexBufferSource index_source {};
	index_source.enabled = true;
	index_source.address = reinterpret_cast<uint64_t>(index_addr);
	index_source.host_data =
	    expanded_indices.empty() ? nullptr : static_cast<const void*>(expanded_indices.data());
	index_source.size =
	    expanded_indices.empty() ? index_size : expanded_indices.size() * sizeof(uint16_t);
	index_source.type = index_type;

	DrawRenderState state {};
	if (!PrepareDrawRenderState(submit_id, buffer, draw, render_target_slice_offset, true, state)) {
		ResetBindings();
		return;
	}

	if (!RefreshShaders(buffer, draw, true, state)) {
		ResetBindings();
		return;
	}

	LogDrawStateIfNeeded(buffer, draw, state, true, false, index_type_and_size, index_addr);

	const auto vertex_offset =
	    ResolveVertexOffset(ucfg.GetIndexOffset(), state.vs_input_info) + vertex_offset_add;

	DrawEmitInfo emit {};
	emit.indexed       = true;
	emit.vertex_offset = vertex_offset;

	ExecutePreparedDraw(submit_id, buffer, draw, state, topology, emit, index_source, true, true,
	                    false);
	ResetBindings();
}

void RenderExecutor::SetIndirectIndexBuffer(uint64_t index_vaddr, uint64_t index_size) {
	m_indirect_index_vaddr = index_vaddr;
	m_indirect_index_size  = index_size;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RenderExecutor::DrawIndirect(uint64_t submit_id, RenderCommandBuffer& buffer,
                                  uint32_t index_type_and_size, uint64_t args_vaddr,
                                  uint32_t draw_count, uint32_t stride_bytes, bool indexed,
                                  uint32_t flags, uint32_t instance_count,
                                  uint32_t render_target_slice_offset) {
	KYTY_PROFILER_FUNCTION();

	EXIT_IF(buffer.IsInvalid());
	EXIT_IF(draw_count == 0);
	EXIT_IF(args_vaddr == 0);
	auto& ucfg   = buffer.GetUserConfig();
	auto& sh_ctx = buffer.GetShaders();

	buffer.SetDebugInfo(static_cast<uint32_t>(CommandBufferDebugOp::DrawIndex), submit_id,
	                    draw_count, flags, 1, instance_count, args_vaddr);

	Common::LockGuard lock(m_context.GetMutex());

	// The argument records are produced by a compute shader, so the counts are only known to
	// the GPU. Account for the call, but never read the buffer from the host here: a CPU load
	// races the producing dispatch and observes stale zeros (which is why GPU-driven geometry
	// used to vanish).
	ReportDrawProgress();
	if (ConsumeMetadataColorOperation(buffer)) {
		g_draw_metadata.fetch_add(1, std::memory_order_relaxed);
		ResetBindings();
		return;
	}

	if (!DrawHasValidVertexShader(sh_ctx)) {
		g_draw_invalid_vs.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	// Record what this draw would have produced before the GE skip test runs.
	if (!Log::IsSilent()) {
		g_skip_debug_index_count.store(draw_count, std::memory_order_relaxed);
		g_skip_debug_instance_count.store(instance_count, std::memory_order_relaxed);
	}
	if (ShouldSkipGeShader(buffer)) {
		return;
	}

	uc_check(ucfg);
	hw_check(buffer);

	vk::PrimitiveTopology topology = vk::PrimitiveTopology::ePointList;
	if (!GetDrawTopology(ucfg, !indexed, false, topology)) {
		return;
	}

	// 8-bit indices are widened to 16-bit on the host, which cannot be expressed as an
	// indirect draw; that front-end has to keep using the CPU-read path.
	const auto index_type = static_cast<Prospero::IndexType>(index_type_and_size);
	if (indexed && index_type == Prospero::IndexType::kIndex8) {
		g_draw_indirect_unsupported.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	DrawIndexBufferSource index_source {};
	if (indexed) {
		index_source.enabled = true;
		index_source.address = m_indirect_index_vaddr;
		index_source.type    = index_type == Prospero::IndexType::kIndex16 ? vk::IndexType::eUint16
		                                                                  : vk::IndexType::eUint32;
		index_source.size    = m_indirect_index_size;
	}

	const DrawCallInfo draw {"DrawIndirect", CommandBufferDebugOp::DrawIndex, draw_count, flags,
	                         instance_count, 0};

	DrawRenderState state {};
	if (!PrepareDrawRenderState(submit_id, buffer, draw, render_target_slice_offset, true, state)) {
		ResetBindings();
		return;
	}

	if (!RefreshShaders(buffer, draw, !indexed, state)) {
		ResetBindings();
		return;
	}

	DrawEmitInfo emit {};
	emit.indexed       = indexed;
	emit.vertex_offset = ResolveVertexOffset(ucfg.GetIndexOffset(), state.vs_input_info);

	DrawIndirectSource indirect {};
	indirect.enabled      = true;
	indirect.args_vaddr   = args_vaddr;
	indirect.draw_count   = draw_count;
	indirect.stride_bytes = stride_bytes;
	indirect.indexed      = indexed;

	ExecutePreparedDraw(submit_id, buffer, draw, state, topology, emit, index_source, true, true,
	                    false, indirect);
	ResetBindings();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RenderExecutor::DrawAuto(uint64_t submit_id, RenderCommandBuffer& buffer, uint32_t index_count,
                              uint32_t flags, uint32_t render_target_slice_offset,
                              uint32_t instance_count, uint32_t first_vertex,
                              uint32_t first_instance) {
	KYTY_PROFILER_FUNCTION();

	EXIT_IF(buffer.IsInvalid());
	auto& ucfg   = buffer.GetUserConfig();
	auto& sh_ctx = buffer.GetShaders();

	buffer.SetDebugInfo(static_cast<uint32_t>(CommandBufferDebugOp::DrawIndexAuto), submit_id,
	                    index_count, flags, first_vertex, instance_count, first_instance);

	Common::LockGuard lock(m_context.GetMutex());
	if (index_count == 0) {
		return;
	}

	ReportDrawProgress();
	if (ConsumeMetadataColorOperation(buffer)) {
		g_draw_metadata.fetch_add(1, std::memory_order_relaxed);
		ResetBindings();
		return;
	}

	if (!DrawHasValidVertexShader(sh_ctx)) {
		g_draw_invalid_vs.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	// Record what this draw would have produced before the GE skip test runs.
	if (!Log::IsSilent()) {
		g_skip_debug_index_count.store(index_count, std::memory_order_relaxed);
		g_skip_debug_instance_count.store(instance_count, std::memory_order_relaxed);
	}
	if (ShouldSkipGeShader(buffer)) {
		return;
	}

	if (graphics_debug_dump_enabled()) {
		sh_print("GraphicsRenderDrawIndexAuto():Shader:", sh_ctx);
		uc_print("GraphicsRenderDrawIndexAuto():UserConfig:", ucfg);
		hw_print(buffer);

		LOGF("GraphicsRenderDrawIndexAuto():Parameters:\n"
		     "\t index_count         = 0x%08" PRIx32 "\n"
		     "\t flags               = 0x%08" PRIx32 "\n"
		     "\t rt_slice_offset     = 0x%08" PRIx32 "\n"
		     "\t instance_count      = 0x%08" PRIx32 "\n"
		     "\t first_vertex        = 0x%08" PRIx32 "\n"
		     "\t first_instance      = 0x%08" PRIx32 "\n",
		     index_count, flags, render_target_slice_offset, instance_count, first_vertex,
		     first_instance);
	}

	uc_check(ucfg);

	hw_check(buffer);

	EXIT_NOT_IMPLEMENTED(flags != 0);
	const DrawCallInfo draw {"DrawIndexAuto", CommandBufferDebugOp::DrawIndexAuto,
	                         index_count,     flags,
	                         instance_count,  first_instance};

	DrawRenderState state {};
	if (!PrepareDrawRenderState(submit_id, buffer, draw, render_target_slice_offset, false,
	                            state)) {
		ResetBindings();
		return;
	}

	vk::PrimitiveTopology topology              = vk::PrimitiveTopology::ePointList;
	const bool            use_ngg_rectlist_draw = Config::NggRectlistDrawEnabled();

	if (!GetDrawTopology(ucfg, true, use_ngg_rectlist_draw, topology)) {
		ResetBindings();
		return;
	}
	const bool draw_prim7_as_ngg =
	    (use_ngg_rectlist_draw &&
	     ucfg.GetPrimType() == Prospero::GpuEnumValue(Prospero::PrimitiveType::kRectList));

	if (!RefreshShaders(buffer, draw, false, state)) {
		ResetBindings();
		return;
	}

	if (draw_prim7_as_ngg && state.vs_input_info.buffers_num == 0 &&
	    state.vs_input_info.param_export_mask == 0 && state.ps_input_info.input_num != 0) {
		if (graphics_debug_dump_enabled()) {
			LOGF("DrawIndexAuto: skipping rect-list draw with no VS param exports and PS inputs: "
			     "ps_inputs=%u ps=0x%016" PRIx64 " es=0x%016" PRIx64 " gs=0x%016" PRIx64 "\n",
			     state.ps_input_info.input_num, sh_ctx.GetPs().ps_regs.chksum,
			     sh_ctx.GetVs().es_regs.data_addr, sh_ctx.GetVs().gs_regs.data_addr);
		}
		ResetBindings();
		return;
	}

	LogDrawStateIfNeeded(buffer, draw, state, false,
	                     ucfg.GetPrimType() ==
	                         Prospero::GpuEnumValue(Prospero::PrimitiveType::kRectListLegacy),
	                     0, nullptr);

	const uint32_t draw_vertex_count = (draw_prim7_as_ngg ? 4u : index_count);
	const auto     vertex_offset = ResolveVertexOffset(ucfg.GetIndexOffset(), state.vs_input_info) +
	                               static_cast<int32_t>(first_vertex);
	DrawEmitInfo   emit {};
	emit.draw_prim7_as_ngg = draw_prim7_as_ngg;
	emit.draw_vertex_count = draw_vertex_count;
	emit.first_vertex      = static_cast<uint32_t>(vertex_offset);

	DrawIndexBufferSource index_source {};
	ExecutePreparedDraw(submit_id, buffer, draw, state, topology, emit, index_source, false, false,
	                    true);
	ResetBindings();
}

bool RenderExecutor::ResolveColorTargets(uint64_t submit_id, RenderCommandBuffer& buffer,
                                         uint32_t render_target_slice_offset) {
	const auto& hw = buffer.GetRegisters();
	if (hw.GetColorControl().mode != 3) {
		return false;
	}
	g_draw_resolves.fetch_add(1, std::memory_order_relaxed);

	const auto& src_rt = hw.GetRenderTarget(0);
	const auto& dst_rt = hw.GetRenderTarget(1);
	if ((dst_rt.base.addr & 0xfffffe000000ULL) == 0x8fc0000000ULL) {
		static std::atomic<uint64_t> cb4_output_resolve_count {0};
		const auto count = cb4_output_resolve_count.fetch_add(1);
		if (count < 12 || count % 1000 == 0) {
			LOGF("CB4 output resolve: frame=%llu src=0x%016" PRIx64 " dst=0x%016" PRIx64 "\n",
			     static_cast<unsigned long long>(m_context.GetGpu().GetFrameNum()),
			     src_rt.base.addr, dst_rt.base.addr);
		}
	}
	if (src_rt.base.addr == 0 || dst_rt.base.addr == 0) {
		return false;
	}

	RenderColorInfo src {};
	RenderColorInfo dst {};
	ResolveRenderColorTarget(submit_id, buffer, src, render_target_slice_offset, 0, true, true);
	ResolveRenderColorTarget(submit_id, buffer, dst, render_target_slice_offset, 1, true, true);
	if (!src.image_id || !dst.image_id || src.type == RenderColorType::NoColorOutput ||
	    dst.type == RenderColorType::NoColorOutput) {
		return false;
	}
	if (src.base_addr == dst.base_addr && src.base_mip_level == dst.base_mip_level &&
	    src.base_array_layer == dst.base_array_layer) {
		return true;
	}

	auto& cache = m_context.GetTextureCache();
	cache.MarkGpuWritten(dst.image_id);
	auto& source      = cache.GetImage(src.image_id);
	auto& destination = cache.GetImage(dst.image_id);
	destination.Resolve(source, {src.base_mip_level, 1, src.base_array_layer, 1},
	                    {dst.base_mip_level, 1, dst.base_array_layer, 1});
	return true;
}

} // namespace Libs::Graphics
