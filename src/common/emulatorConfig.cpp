#include "common/emulatorConfig.h"

#include "common/assert.h"

#include <algorithm>
#include <memory>

namespace Config {

static std::unique_ptr<ConfigOptions> g_config;

KYTY_SUBSYSTEM_INIT(Config) {
	EXIT_IF(g_config != nullptr);

	g_config = std::make_unique<ConfigOptions>();
}

KYTY_SUBSYSTEM_UNEXPECTED_SHUTDOWN(Config) {}

KYTY_SUBSYSTEM_DESTROY(Config) {}

void Load(const ConfigOptions& cfg) {
	EXIT_IF(g_config == nullptr);

	*g_config = cfg;
}

uint32_t GetScreenWidth() {
	return g_config->screen_width;
}

uint32_t GetScreenHeight() {
	return g_config->screen_height;
}

uint32_t GetVblankFrequency() {
	return std::clamp(g_config->vblank_frequency, 30u, 360u);
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

bool RenderDocEnabled() {
	return g_config->renderdoc_enabled;
}

bool NggRectlistDrawEnabled() {
	return g_config->ngg_rectlist_draw_enabled;
}

bool ReadbackLinearImagesEnabled() {
	return g_config->readback_linear_images;
}

bool FullscreenEnabled() {
	return g_config->fullscreen;
}

PresentFilter GetPresentFilter() {
	return g_config->present_filter;
}

PresentMode GetPresentMode() {
	return g_config->present_mode;
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
