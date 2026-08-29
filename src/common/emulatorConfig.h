#ifndef KYTY_COMMON_EMULATOR_CONFIG_H_
#define KYTY_COMMON_EMULATOR_CONFIG_H_

#include "common/common.h"

#include <cstddef>
#include <filesystem>
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

enum class ShaderLogDirection { Silent, Console, File };

enum class ProfilerDirection { None, Network };

enum class OutputDirection { Silent, Console, File };

enum class PresentMode { Fifo, Mailbox, Immediate };

// Filter used by the presenter to scale the guest frame to the swapchain/window extent.
enum class PresentFilter { Nearest, Linear, Cubic };

// How the guest frame is fitted into the window extent.
enum class AspectRatio { Stretch, Fit16x9, Fit4x3, Integer };

// Internal resolution scale for iGPU optimization.
enum class ResolutionScale { Native, Half, Quarter };

// Upscaling method applied during the final guest->swapchain presentation.
enum class UpscalerMethod { Off, Fsr1 };

// Quality preset for the upscaler. Controls the internal render scale.
enum class UpscalerQuality { UltraQuality, Quality, Balanced, Performance };



using Keymap = std::vector<std::string>;

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
	PresentMode            present_mode                = PresentMode::Fifo;
	bool                   fullscreen_enabled          = false;
	uint32_t               vblank_frequency            = 60;
	uint32_t               console_language            = DEFAULT_CONSOLE_LANGUAGE;
	bool                   vulkan_validation_enabled   = false;
	bool                   shader_validation_enabled   = false;
	ShaderOptimizationType shader_optimization_type    = ShaderOptimizationType::None;
	ShaderLogDirection     shader_log_direction        = ShaderLogDirection::Silent;
	std::filesystem::path  shader_log_folder           = "_Shaders";
	bool                   command_buffer_dump_enabled = false;
	std::filesystem::path  command_buffer_dump_folder  = "_Buffers";
	bool                   graphics_debug_dump_enabled = false;
	OutputDirection        printf_direction            = OutputDirection::Silent;
	std::filesystem::path  printf_output_file          = "_kyty.txt";
	ProfilerDirection      profiler_direction          = ProfilerDirection::None;
	bool                   spirv_debug_printf_enabled  = false;
	bool                   gpu_assisted_validation_enabled = false;
	bool                   renderdoc_enabled           = false;
	bool                   readback_linear_images      = false;
	bool                   playgo_hack_enabled         = false;
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	bool red_zone_protection_enabled = false;
#endif
	// --- KytyPlus fork options ---
	// Upscaling filter applied during the final guest->swapchain blit.
	PresentFilter          present_filter              = PresentFilter::Linear;
	PresentMode            present_mode_fork           = PresentMode::Fifo;
	AspectRatio            aspect_ratio                = AspectRatio::Stretch;
	// First-encounter graphics pipelines compile on a worker thread pool; draws
	// hitting a pending pipeline skip recording instead of blocking.
	bool                   async_pipeline_compilation  = true;
	bool                   ngg_rectlist_draw_enabled   = true;
	uint32_t               screenshot_hotkey           = 0x5b;
	std::filesystem::path  screenshot_folder           = "_Screenshots";
	// --- iGPU optimization ---
	bool                   force_igpu_mode             = false;
	ResolutionScale        resolution_scale            = ResolutionScale::Native;
	int32_t                texture_lod_bias            = 0;
	bool                   uma_staging_bypass          = false;
	// --- Upscaler ---
	UpscalerMethod         upscaler_method             = UpscalerMethod::Off;
	UpscalerQuality        upscaler_quality            = UpscalerQuality::Quality;
	float                  upscaler_sharpness          = 0.5f;
	
	Keymap keymap;
};

void Load(const ConfigOptions& cfg);

uint32_t GetScreenWidth();
uint32_t GetScreenHeight();
const std::string& GetUserName();
int32_t  GetUserId();
PresentMode GetPresentMode();
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

bool GpuAssistedValidationEnabled();

bool RenderDocEnabled();
bool ReadbackLinearImagesEnabled();
bool PlayGoHackEnabled();
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
bool RedZoneProtectionEnabled();
#endif

const Keymap& GetKeymap();
// --- KytyPlus fork accessors ---
bool NggRectlistDrawEnabled();
PresentFilter GetPresentFilter();
AspectRatio   GetAspectRatio();
uint32_t      GetScreenshotHotkey();
std::filesystem::path GetScreenshotFolder();
bool          AsyncPipelineCompilationEnabled();
bool          ForceIgpuMode();
ResolutionScale GetResolutionScale();
int32_t       GetTextureLodBias();
bool          UmaStagingBypass();
void          SetUmaStagingBypass(bool value);
float         GetResolutionScaleFactor();
UpscalerMethod GetUpscalerMethod();
UpscalerQuality GetUpscalerQuality();
float         GetUpscalerSharpness();
float         GetUpscalerRenderScale();
void ApplyIgpuDefaults(bool integrated_gpu);

} // namespace Config

#endif /* KYTY_COMMON_EMULATOR_CONFIG_H_ */
