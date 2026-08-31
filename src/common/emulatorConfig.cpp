#include "common/emulatorConfig.h"

#include "common/assert.h"
#include "common/logging/log.h"

#include <algorithm>
#include <memory>

namespace Config {

static std::unique_ptr<ConfigOptions> g_config;

void Initialize() {
	EXIT_IF(g_config != nullptr);

	g_config = std::make_unique<ConfigOptions>();
}

void Shutdown() {
	g_config.reset();
}

void Load(const ConfigOptions& cfg) {
	EXIT_IF(g_config == nullptr);
	EXIT_IF(cfg.user_name.empty() || cfg.user_name.size() > MAX_USER_NAME_LENGTH);
	EXIT_IF(!IsConfiguredUserIdValid(cfg.user_id));

	*g_config = cfg;
}

void ApplyIgpuDefaults(bool integrated_gpu) {
	EXIT_IF(g_config == nullptr);

	if (!integrated_gpu) {
		return;
	}

	// Performance-floor defaults for integrated-GPU machines (Steam Deck / 780M class):
	// FSR 1.0 presentation upscaling and a texture LOD bias that skips the highest mip
	// levels, reducing texture bandwidth. Applied only when the value is still at its
	// built-in default, so an explicit user choice always wins.
	bool changed = false;
	if (g_config->upscaler_method == UpscalerMethod::Off) {
		g_config->upscaler_method = UpscalerMethod::Fsr1;
		changed                   = true;
	}
	if (g_config->texture_lod_bias == 0) {
		g_config->texture_lod_bias = 1;
		changed                    = true;
	}
	if (changed) {
		LOGF("Config: integrated GPU detected — applied floor defaults: "
		     "FSR 1.0 upscaler, texture LOD bias 1\n");
	}
}

uint32_t GetScreenWidth() {
	return g_config->screen_width;
}

uint32_t GetScreenHeight() {
	return g_config->screen_height;
}

const std::string& GetUserName() {
	return g_config->user_name;
}

int32_t GetUserId() {
	return g_config->user_id;
}

PresentMode GetPresentMode() {
	return g_config->present_mode;
}

bool FullscreenEnabled() {
	return g_config->fullscreen_enabled;
}

uint32_t GetVblankFrequency() {
	return std::clamp(g_config->vblank_frequency, 30u, 360u);
}

uint32_t GetConsoleLanguage() {
	return g_config->console_language;
}

bool VulkanValidationEnabled() {
	return g_config->vulkan_validation_enabled;
}

bool ShaderValidationEnabled() {
	return g_config->shader_validation_enabled;
}

ShaderOptimizationType GetShaderOptimizationType() {
	return g_config->shader_optimization_type;
}

ShaderLogDirection GetShaderLogDirection() {
	return g_config->shader_log_direction;
}

std::filesystem::path GetShaderLogFolder() {
	return g_config->shader_log_folder;
}

bool CommandBufferDumpEnabled() {
	return g_config->command_buffer_dump_enabled;
}

std::filesystem::path GetCommandBufferDumpFolder() {
	return g_config->command_buffer_dump_folder;
}

bool GraphicsDebugDumpEnabled() {
	return g_config->graphics_debug_dump_enabled;
}

OutputDirection GetPrintfDirection() {
	return g_config->printf_direction;
}

std::filesystem::path GetPrintfOutputFile() {
	return g_config->printf_output_file;
}

ProfilerDirection GetProfilerDirection() {
	return g_config->profiler_direction;
}

bool SpirvDebugPrintfEnabled() {
	return g_config->spirv_debug_printf_enabled;
}

bool GpuAssistedValidationEnabled() {
	return g_config->gpu_assisted_validation_enabled && g_config->vulkan_validation_enabled;
}

bool RenderDocEnabled() {
	return g_config->renderdoc_enabled;
}

bool NggRectlistDrawEnabled() {
	return g_config->ngg_rectlist_draw_enabled;
}

bool ReadbackLinearImagesEnabled() {
	return g_config->readback_linear_images;
}

bool PlayGoHackEnabled() {
	return g_config->playgo_hack_enabled;
}

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
bool RedZoneProtectionEnabled() {
	return g_config->red_zone_protection_enabled;
}
#endif

const Keymap& GetKeymap() {
	return g_config->keymap;
}

PresentFilter GetPresentFilter() {
	return g_config->present_filter;
}

AspectRatio GetAspectRatio() {
	return g_config->aspect_ratio;
}

uint32_t GetScreenshotHotkey() {
	return g_config->screenshot_hotkey;
}

std::filesystem::path GetScreenshotFolder() {
	return g_config->screenshot_folder;
}

bool AsyncPipelineCompilationEnabled() {
	return g_config->async_pipeline_compilation;
}

// --- iGPU optimization accessors ---

bool ForceIgpuMode() {
	return g_config->force_igpu_mode;
}

ResolutionScale GetResolutionScale() {
	return g_config->resolution_scale;
}

int32_t GetTextureLodBias() {
	return g_config->texture_lod_bias;
}

bool UmaStagingBypass() {
	return g_config->uma_staging_bypass;
}

void SetUmaStagingBypass(bool value) {
	EXIT_IF(g_config == nullptr);
	g_config->uma_staging_bypass = value;
}

float GetResolutionScaleFactor() {
	switch (g_config->resolution_scale) {
		case ResolutionScale::Native:  return 1.0f;
		case ResolutionScale::Half:    return 0.5f;
		case ResolutionScale::Quarter: return 0.25f;
	}
	return 1.0f;
}

// --- Upscaler accessors ---

UpscalerMethod GetUpscalerMethod() {
	return g_config->upscaler_method;
}

UpscalerQuality GetUpscalerQuality() {
	return g_config->upscaler_quality;
}

float GetUpscalerSharpness() {
	return std::clamp(g_config->upscaler_sharpness, 0.0f, 1.0f);
}

float GetUpscalerRenderScale() {
	switch (g_config->upscaler_quality) {
		case UpscalerQuality::UltraQuality: return 0.77f;
		case UpscalerQuality::Quality:      return 0.67f;
		case UpscalerQuality::Balanced:     return 0.59f;
		case UpscalerQuality::Performance:  return 0.50f;
	}
	return 0.67f;
}

} // namespace Config