#ifndef KYTY_COMMON_EMULATOR_CONFIG_H_
#define KYTY_COMMON_EMULATOR_CONFIG_H_

#include "common/common.h"

#include <cstddef>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Config {

void Initialize();
void Shutdown();

struct Lifecycle {
	static constexpr const char* name       = "Config";
	static constexpr auto        initialize = Config::Initialize;
	static constexpr auto        shutdown   = Config::Shutdown;
};

enum class ShaderOptimizationType { None, Size, Performance };

enum class LogDirection { Silent, Console, File };

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
//   Half    -- 50% (640x360) â€” major bandwidth/fill-rate savings
//   Quarter -- 25% (320x180) â€” extreme mode for very weak iGPUs
enum class ResolutionScale { Native, Half, Quarter };

// Upscaling method applied during the final guest->swapchain presentation.
//   Off   -- plain vkBlitImage (Nearest/Linear per PresentFilter)
//   Fsr1  -- AMD FSR 1.0-inspired spatial upscaler (EASU + RCAS), works on all Vulkan GPUs
enum class UpscalerMethod { Off, Fsr1 };

// Quality preset for the upscaler. Controls the internal render scale:
//   UltraQuality -- 77% (barely visible, minimal perf gain)
//   Quality      -- 67% (good balance)
//   Balanced     -- 59% (noticeable perf, slight quality loss)
//   Performance  -- 50% (maximum perf, visible softness)
enum class UpscalerQuality { UltraQuality, Quality, Balanced, Performance };

enum class OutputDirection { Silent, Console, File };


using Keymap = std::vector<std::string>;
using ControllerColor = std::array<uint8_t, 3>;

constexpr uint32_t DEFAULT_CONSOLE_LANGUAGE = 1;
constexpr uint32_t MAX_CONSOLE_LANGUAGE     = 29;
constexpr std::size_t MAX_USER_NAME_LENGTH = 16;
constexpr int32_t DEFAULT_USER_ID           = 1000;

constexpr bool IsConfiguredUserIdValid(int32_t user_id) {
	constexpr int32_t USER_ID_EVERYONE = 0xfe;
	constexpr int32_t USER_ID_SYSTEM   = 0xff;
	return user_id >= 0 && user_id != USER_ID_EVERYONE && user_id != USER_ID_SYSTEM;
}

struct ConfigOptions {
	uint32_t               screen_width                = 1280;
	uint32_t               screen_height               = 720;
	std::string            user_name                   = "Kyty";
	int32_t                user_id                     = DEFAULT_USER_ID;
	std::string            audio_input_device;
	std::optional<ControllerColor> controller_color;
	uint32_t               controller_speaker_volume      = 50;
	uint32_t               controller_vibration_intensity = 100;
	int32_t                gpu_index                   = -1;
	bool                   fullscreen_enabled          = false;
	bool                   hdr_enabled                 = false;
	bool                   hide_cursor_enabled         = false;
	bool                   splash_enabled              = true;
	bool                   vr_enabled                  = false;
	bool                   amd_cpu_enabled             = false;
	bool                   network_connected           = false;
	uint32_t               vblank_frequency            = 60;
	uint32_t               console_language            = DEFAULT_CONSOLE_LANGUAGE;
	bool                   vulkan_validation_enabled   = true;  // Enabled by default for debugging
	bool                   shader_validation_enabled   = false;
	// Match the launcher's playable default so CLI launches without
	// --shader-optimization-type still get SPIR-V performance passes.
	ShaderOptimizationType shader_optimization_type    = ShaderOptimizationType::Performance;
	LogDirection     shader_log_direction        = LogDirection::Silent;
	std::filesystem::path  shader_log_folder           = "_Shaders";
	bool                   command_buffer_dump_enabled = false;
	std::filesystem::path  command_buffer_dump_folder  = "_Buffers";
	bool                   graphics_debug_dump_enabled = false;
	LogDirection           printf_direction            = LogDirection::Silent;
	std::filesystem::path  printf_output_file          = "_kyty.txt";
	bool                   profiler_enabled            = false;
	bool                   spirv_debug_printf_enabled  = false;
	bool                   gpu_assisted_validation_enabled = false;
	bool                   renderdoc_enabled           = false;
	bool                   sync_raw_image_buffers      = false;
	bool                   tessellation_enabled        = false;
	bool                   trophy_enabled              = true;
	uint32_t               trophy_duration_seconds     = 6;
	bool                   playgo_hack_enabled         = false;
	bool                   skip_notice_screen          = false;
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
	// Multiplier applied to display-sized guest render targets (>= 1920x1080).
	// 0 means "not set": the iGPU auto-detection picks a default, otherwise the
	// guest renders at its native resolution. This is the only knob that actually
	// reduces the guest's own fill rate; the presenter-side upscaler runs too late
	// to save any work.
	float                  render_scale                = 0.0f;
	// Opt-in guest launch request, separate from the host window. Engines may
	// ignore these arguments; never shrink attachments to pretend it worked.
	uint32_t               guest_render_width          = 1920;
	uint32_t               guest_render_height         = 1080;
	// Texture LOD bias: positive values skip high-resolution mip levels,
	// reducing memory bandwidth. 0 = no bias, 1 = skip one mip level, etc.
	int32_t                texture_lod_bias            = 0;
	// Skip GPU staging buffer copies on UMA (unified memory architecture).
	// On iGPUs, CPU and GPU share the same physical memory so staging copies
	// are redundant. Auto-detected; this override forces the behavior.
	bool                   uma_staging_bypass          = false;
	// --- Upscaler options ---
	// Upscaling method for the guest->swapchain presentation blit.
	UpscalerMethod         upscaler_method             = UpscalerMethod::Fsr1;
	// Explicit source/output sizes determine the actual upscale ratio.
	UpscalerQuality        upscaler_quality            = UpscalerQuality::Performance;
	// RCAS sharpening strength (0.0 = no sharpening, 1.0 = maximum).
	float                  upscaler_sharpness          = 0.3f;
	// FSR result is independent of the window size; smaller displays downsample it.
	// Zero selects the window extent instead. Ignored when the upscaler is Off.
	uint32_t               fsr_output_width            = 3840;
	uint32_t               fsr_output_height           = 2160;
	// Opt-in native DualSense HID driver (adaptive triggers / lightbar / motion via
	// libPad). DEFAULT OFF: SDL already handles every standard pad (including a
	// DualSense), and this driver is unvalidated on hardware, so it is gated behind
	// an explicit flag to avoid any conflict with the SDL controller path.
	bool                   dualsense_enabled           = false;
	// Report a CONNECTED console to guest libNetCtl. DEFAULT OFF: Kyty previously
	// always reported "offline" for this API specifically so games would skip network
	// features. Turning it on makes sceNetCtl* report the real host adapter state, but
	// games will then actually attempt online paths (which may hit stubbed Np*/Ssl*
	// services). Opt-in so it cannot silently change existing titles' behavior.
	bool                   network_online_enabled      = false;
	// Strict unimplemented-path handling. DEFAULT OFF: unimplemented graphics paths
	// (unknown PM4 opcodes, unmapped context/shader registers, non-default shader
	// config, unsupported shader instructions) are logged and skipped so rendering
	// continues with (possibly wrong) output instead of terminating the process.
	// Turn this ON to restore the historical fail-fast behaviour, where the first
	// unimplemented path aborts with a stack trace -- useful when bisecting a
	// specific crash. Applies to the SOFT_* guard macros only; genuine invariant
	// failures (EXIT_IF/EXIT on invalid internal state) always abort.
	bool                   strict_unimplemented_enabled = false;
	std::vector<std::string> guest_arguments {""};
};

void Load(const ConfigOptions& cfg);

// Apply performance-floor defaults for integrated-GPU (Steam Deck / 780M class) machines.
// Only overrides values still at their built-in defaults, so explicit user choices win.
// Called once the Vulkan device type is known (see vma.cpp CreateAllocator).
void ApplyIgpuDefaults(bool integrated_gpu);

uint32_t GetScreenWidth();
uint32_t GetScreenHeight();
uint32_t GetGuestRenderWidth();
uint32_t GetGuestRenderHeight();
uint32_t GetFsrOutputWidth();
uint32_t GetFsrOutputHeight();
const std::string& GetUserName();
int32_t  GetUserId();
const std::string& GetAudioInputDevice();
const std::optional<ControllerColor>& GetControllerColor();
uint32_t GetControllerSpeakerVolume();
uint32_t GetControllerVibrationIntensity();
int32_t GetGpuIndex();
bool     FullscreenEnabled();
bool     HdrEnabled();
bool     HideCursorEnabled();
bool     VrEnabled();
bool     AmdCpuEnabled();
bool     IsNetworkConnected();
uint32_t GetVblankFrequency();
uint32_t GetConsoleLanguage();
bool     VulkanValidationEnabled();

bool                   ShaderValidationEnabled();
ShaderOptimizationType GetShaderOptimizationType();
LogDirection           GetShaderLogDirection();
std::filesystem::path  GetShaderLogFolder();

bool                  CommandBufferDumpEnabled();
std::filesystem::path GetCommandBufferDumpFolder();

bool GraphicsDebugDumpEnabled();

LogDirection          GetPrintfDirection();
std::filesystem::path GetPrintfOutputFile();

bool ProfilerEnabled();

bool SpirvDebugPrintfEnabled();

bool GpuAssistedValidationEnabled();

bool RenderDocEnabled();
bool NggRectlistDrawEnabled();
bool ReadbackLinearImagesEnabled();
bool SyncRawImageBuffersEnabled();
bool TessellationEnabled();
bool TrophyEnabled();
uint32_t GetTrophyDurationSeconds();
bool PlayGoHackEnabled();
bool SkipNoticeScreen();
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
bool RedZoneProtectionEnabled();
#endif
const Keymap& GetKeymap();
const std::vector<std::string>& GetGuestArguments();

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
// Set the UMA staging-bypass flag. Used by the VMA allocator to auto-enable the
// bypass once a unified-memory heap is detected at device-creation time.
void             SetUmaStagingBypass(bool value);

// Returns the scale factor for the selected resolution scale (1.0, 0.5, or 0.25).
[[nodiscard]] float GetResolutionScaleFactor();

// Multiplier applied to display-sized guest render targets. Always in (0, 1];
// 1.0 means the guest renders at its native resolution.
[[nodiscard]] float GetRenderScale();

// --- Upscaler accessors ---
UpscalerMethod  GetUpscalerMethod();
UpscalerQuality GetUpscalerQuality();
float           GetUpscalerSharpness();

// Returns the render scale factor for the selected upscaler quality
// (e.g. Quality = 0.67, meaning render at 67% then upscale to full).
float GetUpscalerRenderScale();

// Native DualSense HID driver gating. False (default) keeps the unvalidated
// DualSense driver disabled; the standard SDL controller path is unaffected.
bool DualSenseEnabled();
bool NetworkOnlineEnabled();

// When true, unimplemented graphics paths abort (fail-fast) instead of being
// logged and skipped. False (default) keeps rendering alive past a gap.
bool UnimplementedStrictMode();

} // namespace Config

#endif /* KYTY_COMMON_EMULATOR_CONFIG_H_ */