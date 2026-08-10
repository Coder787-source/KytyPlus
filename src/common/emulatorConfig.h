#ifndef KYTY_COMMON_EMULATOR_CONFIG_H_
#define KYTY_COMMON_EMULATOR_CONFIG_H_

#include "common/common.h"
#include "common/subsystems.h"

#include <filesystem>
#include <string>
#include <vector>

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

// Swapchain present mode (maps to vk::PresentModeKHR).
//   Fifo      -- VSync on, guaranteed by the Vulkan spec (safe default).
//   Mailbox   -- VSync on, lowest-latency tearing-free (triple-buffered).
//   Immediate -- VSync off, tearing allowed for lowest latency.
enum class PresentMode { Fifo, Mailbox, Immediate };

// How the guest frame is fitted into the window extent.
enum class AspectRatio { Stretch, Fit16x9, Fit4x3, Integer };

// Internal resolution scale for iGPU optimization.
//   Native  -- 100% (1280x720 or guest resolution)
//   Half    -- 50% (640x360) — major bandwidth/fill-rate savings
//   Quarter -- 25% (320x180) — extreme mode for very weak iGPUs
enum class ResolutionScale { Native, Half, Quarter };

// Upscaling method applied during the final guest->swapchain presentation.
//   Off   -- plain vkBlitImage (Nearest/Linear per PresentFilter)
//   Fsr31 -- AMD FSR-inspired spatial upscaler (EASU + RCAS), works on all Vulkan GPUs
enum class UpscalerMethod { Off, Fsr31 };

// Quality preset for the upscaler. Controls the internal render scale:
//   UltraQuality -- 77% (barely visible, minimal perf gain)
//   Quality      -- 67% (good balance)
//   Balanced     -- 59% (noticeable perf, slight quality loss)
//   Performance  -- 50% (maximum perf, visible softness)
enum class UpscalerQuality { UltraQuality, Quality, Balanced, Performance };

enum class OutputDirection { Silent, Console, File };

using Keymap = std::vector<std::string>;

constexpr uint32_t DEFAULT_CONSOLE_LANGUAGE = 1;
constexpr uint32_t MAX_CONSOLE_LANGUAGE     = 29;

struct ConfigOptions {
	uint32_t               screen_width                = 1280;
	uint32_t               screen_height               = 720;
	bool                   fullscreen_enabled          = false;
	uint32_t               vblank_frequency            = 60;
	uint32_t               console_language            = DEFAULT_CONSOLE_LANGUAGE;
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
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	bool                   red_zone_protection_enabled = false;
#endif
	bool                   ngg_rectlist_draw_enabled   = true;
	// Serialized as "host_code:pad_button,host_code:pad_button,..." (see
	// Libs::Controller::ParseInputBindingList/SerializeInputBindingList). Empty means "use the
	// emulator's built-in default bindings" -- set by the Qt launcher's Input Mapping dialog.
	std::string             keyboard_button_map;
	std::string             controller_button_map;
	// Legacy serialized host-input bindings consumed by hostInput.cpp. Keep this
	// alongside the newer launcher binding-list fields for upstream compatibility.
	Keymap                  keymap;
	bool                   readback_linear_images      = false;
	bool                   fullscreen                  = false;
	// Upscaling filter applied during the final guest->swapchain blit. The guest frame is
	// always stretched to the full window extent; this only changes the sampling quality of
	// that stretch. Default matches the historical fixed behaviour (bilinear).
	PresentFilter          present_filter              = PresentFilter::Linear;
	PresentMode            present_mode                = PresentMode::Fifo;
	AspectRatio            aspect_ratio                = AspectRatio::Stretch;
	// When true, first-encounter graphics pipelines are compiled on a fixed worker
	// thread pool instead of inline on the GPU thread, and draws that hit a
	// not-yet-ready pipeline skip recording instead of blocking. Eliminates the
	// multi-100 ms first-encounter stutter at the cost of a few skipped frames.
	// Matches the async-compilation strategy used by mature emulators (RPCS3/shadPS4).
	bool                   async_pipeline_compilation  = true;
	// SDL scancode (0x5b = F12) that captures a screenshot PNG; 0 disables.
	uint32_t               screenshot_hotkey           = 0x5b;
	// Folder screenshot PNGs are written to.
	std::filesystem::path  screenshot_folder           = "_Screenshots";
	// --- iGPU optimization options ---
	// Force iGPU optimization mode even if a discrete GPU is detected.
	bool                   force_igpu_mode             = false;
	// Internal resolution scale for iGPU: reduces render target size to save
	// fill rate and memory bandwidth. The presenter upscales back to window size.
	ResolutionScale        resolution_scale            = ResolutionScale::Native;
	// Texture LOD bias: positive values skip high-resolution mip levels,
	// reducing memory bandwidth. 0 = no bias, 1 = skip one mip level, etc.
	int32_t                texture_lod_bias            = 0;
	// Skip GPU staging buffer copies on UMA (unified memory architecture).
	// On iGPUs, CPU and GPU share the same physical memory so staging copies
	// are redundant. Auto-detected; this override forces the behavior.
	bool                   uma_staging_bypass          = false;
	// --- Upscaler options ---
	// Upscaling method for the guest->swapchain presentation blit.
	UpscalerMethod         upscaler_method             = UpscalerMethod::Off;
	// Quality preset controlling the internal render scale.
	UpscalerQuality        upscaler_quality            = UpscalerQuality::Quality;
	// RCAS sharpening strength (0.0 = no sharpening, 1.0 = maximum).
	float                  upscaler_sharpness          = 0.5f;
};

void Load(const ConfigOptions& cfg);

// Apply performance-floor defaults for integrated-GPU (Steam Deck / 780M class) machines.
// Only overrides values still at their built-in defaults, so explicit user choices win.
// Called once the Vulkan device type is known (see vma.cpp CreateAllocator).
void ApplyIgpuDefaults(bool integrated_gpu);

uint32_t GetScreenWidth();
uint32_t GetScreenHeight();
bool     FullscreenEnabled();
uint32_t GetVblankFrequency();
uint32_t GetConsoleLanguage();
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
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
bool RedZoneProtectionEnabled();
#endif
const Keymap& GetKeymap();

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

// --- iGPU optimization accessors ---
bool             ForceIgpuMode();
ResolutionScale  GetResolutionScale();
int32_t          GetTextureLodBias();
bool             UmaStagingBypass();

// Returns the scale factor for the selected resolution scale (1.0, 0.5, or 0.25).
float GetResolutionScaleFactor();

// --- Upscaler accessors ---
UpscalerMethod  GetUpscalerMethod();
UpscalerQuality GetUpscalerQuality();
float           GetUpscalerSharpness();

// Returns the render scale factor for the selected upscaler quality
// (e.g. Quality = 0.67, meaning render at 67% then upscale to full).
float GetUpscalerRenderScale();

} // namespace Config

#endif /* KYTY_COMMON_EMULATOR_CONFIG_H_ */