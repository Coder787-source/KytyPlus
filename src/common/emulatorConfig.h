#ifndef KYTY_COMMON_EMULATOR_CONFIG_H_
#define KYTY_COMMON_EMULATOR_CONFIG_H_

#include "common/common.h"
#include "common/subsystems.h"

#include <filesystem>
#include <string>

namespace Config {

KYTY_SUBSYSTEM_DEFINE(Config);

enum class ShaderOptimizationType { None, Size, Performance };

enum class ShaderLogDirection { Silent, Console, File };

enum class ProfilerDirection { None, Network };

// Filter used by the presenter to scale the guest frame to the swapchain/window extent.
//   Nearest  -- sharp pixels, ideal for pixel-art titles (e.g. Dead Cells)
//   Linear   -- bilinear smoothing (matches the original fixed behaviour)
//   Cubic    -- smoother upscaling where the device exposes the cubic filter extension
enum class PresentFilter { Nearest, Linear, Cubic };

enum class OutputDirection { Silent, Console, File };

struct ConfigOptions {
	uint32_t               screen_width                = 1280;
	uint32_t               screen_height               = 720;
	uint32_t               vblank_frequency            = 60;
	bool                   vulkan_validation_enabled   = false;
	bool                   shader_validation_enabled   = false;
	// Match the launcher's playable default so CLI launches without
	// --shader-optimization-type still get SPIR-V performance passes.
	ShaderOptimizationType shader_optimization_type    = ShaderOptimizationType::Performance;
	ShaderLogDirection     shader_log_direction        = ShaderLogDirection::Silent;
	std::filesystem::path  shader_log_folder           = "_Shaders";
	bool                   command_buffer_dump_enabled = false;
	std::filesystem::path  command_buffer_dump_folder  = "_Buffers";
	bool                   graphics_debug_dump_enabled = false;
	OutputDirection        printf_direction            = OutputDirection::Console;
	std::filesystem::path  printf_output_file          = "_kyty.txt";
	ProfilerDirection      profiler_direction          = ProfilerDirection::None;
	bool                   spirv_debug_printf_enabled  = false;
	bool                   renderdoc_enabled           = false;
	bool                   ngg_rectlist_draw_enabled   = true;
	// Serialized as "host_code:pad_button,host_code:pad_button,..." (see
	// Libs::Controller::ParseInputBindingList/SerializeInputBindingList). Empty means "use the
	// emulator's built-in default bindings" -- set by the Qt launcher's Input Mapping dialog.
	std::string             keyboard_button_map;
	std::string             controller_button_map;
	bool                   readback_linear_images      = false;
	bool                   fullscreen                  = false;
	// Upscaling filter applied during the final guest->swapchain blit. The guest frame is
	// always stretched to the full window extent; this only changes the sampling quality of
	// that stretch. Default matches the historical fixed behaviour (bilinear).
	PresentFilter          present_filter              = PresentFilter::Linear;
	// When true, first-encounter graphics pipelines are compiled on a fixed worker
	// thread pool instead of inline on the GPU thread, and draws that hit a
	// not-yet-ready pipeline skip recording instead of blocking. Eliminates the
	// multi-100 ms first-encounter stutter at the cost of a few skipped frames.
	// Matches the async-compilation strategy used by mature emulators (RPCS3/shadPS4).
	bool                   async_pipeline_compilation  = true;
};

void Load(const ConfigOptions& cfg);

uint32_t GetScreenWidth();
uint32_t GetScreenHeight();
uint32_t GetVblankFrequency();
bool     VulkanValidationEnabled();

bool                   ShaderValidationEnabled();
ShaderOptimizationType GetShaderOptimizationType();
ShaderLogDirection     GetShaderLogDirection();
std::filesystem::path  GetShaderLogFolder();

bool                  CommandBufferDumpEnabled();
std::filesystem::path GetCommandBufferDumpFolder();

bool GraphicsDebugDumpEnabled();

OutputDirection       GetPrintfDirection();
std::filesystem::path GetPrintfOutputFile();

ProfilerDirection GetProfilerDirection();

bool SpirvDebugPrintfEnabled();

bool RenderDocEnabled();
bool NggRectlistDrawEnabled();
bool ReadbackLinearImagesEnabled();
bool FullscreenEnabled();

// Upscaling filter for the final guest->swapchain presentation blit.
PresentFilter GetPresentFilter();

// Swapchain present mode (vsync / latency / tearing control).
PresentMode GetPresentMode();

// How the guest frame is fit into the window extent.
AspectRatio GetAspectRatio();

// SDL scancode that triggers a screenshot; 0 disables the hotkey.
uint32_t GetScreenshotHotkey();

// Folder screenshots are written to.
std::filesystem::path GetScreenshotFolder();

// Async graphics pipeline compilation: worker-thread compile + draw-skip fallback.
bool AsyncPipelineCompilationEnabled();

} // namespace Config

#endif /* KYTY_COMMON_EMULATOR_CONFIG_H_ */
