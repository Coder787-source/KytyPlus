#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/threads.h"
#include "graphics/presentation/window/windowInternal.h"
#include "graphics/shader/recompiler/ShaderRecompiler.h"
#include "spirv-tools/libspirv.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

Libs::Graphics::ShaderRecompiler::CompileResult CheckLayerShader(bool layer, bool user_data) {
	using namespace Libs::Graphics;
	// v0..v3 form POS0; POS1.z carries the raw layer index in the folded NGG path.
	std::vector<uint32_t> code {
	    0x7e0002ffu, 0u, 0x7e0202ffu, 0u,
	    0x7e0402ffu, 0u, 0x7e0602ffu, 0x3f800000u};
	if (user_data) {
		code.push_back(0x7e040200u); // v_mov_b32 v2, s0 (push constant)
	}
	code.insert(code.end(), {0xf80000cfu, 0x03020100u}); // exp pos0 v0,v1,v2,v3
	if (layer) {
		code.insert(code.end(), {0xf80008d4u, 0x02020202u}); // exp pos1.z, done
	}
	code.push_back(0xbf810000u); // s_endpgm

	ShaderVertexInputInfo vertex {};
	vertex.folded_triangle_copy = layer;
	vertex.export_layer         = layer;
	const uint32_t data[1] {0};
	ShaderRecompiler::CompileOptions options;
	options.stage             = ShaderType::Vertex;
	options.lane_mask_mode    = ShaderLaneMaskMode::PerInvocation;
	options.vertex_input_info = &vertex;
	options.user_data         = data;
	options.user_data_count   = 1;
	ShaderRecompiler::CompileResult result;
	std::string error;
	const bool compiled = ShaderRecompiler::TryRecompile(code, options, result, &error);
	Require(compiled, error.c_str());
	Require(result.spirv.size() >= 5, "empty shader module");
	Require(result.spirv[1] == (layer ? 0x00010500u : 0x00010300u),
	        "only layer-output shaders should require SPIR-V 1.5");

	spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_2);
	tools.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&,
	                           const char* message) { std::fprintf(stderr, "%s\n", message); });
	Require(tools.Validate(result.spirv), "layer shader or its resource interface is invalid");
	std::string text;
	Require(tools.Disassemble(result.spirv, &text), "cannot disassemble shader");
	Require((text.find("OpCapability ShaderLayer") != std::string::npos) == layer,
	        "layer capability does not match output");
	Require(text.find("ShaderViewportIndexLayerEXT") == std::string::npos &&
	            text.find("SPV_EXT_shader_viewport_index_layer") == std::string::npos,
	        "layer output still requires the unenabled viewport/layer extension");
	Require(!user_data || result.program.bindings.push_constant_size != 0,
	        "resource-interface regression did not exercise a push constant");
	std::printf("PASS: layer=%d user_data=%d\n", layer, user_data);
	return result;
}

void CheckPipeline(vk::Device device, const Libs::Graphics::ShaderRecompiler::CompileResult& shader) {
	vk::ShaderModuleCreateInfo module_info {};
	module_info.codeSize = shader.spirv.size() * sizeof(uint32_t);
	module_info.pCode    = shader.spirv.data();
	vk::ShaderModule module;
	Require(device.createShaderModule(&module_info, nullptr, &module) == vk::Result::eSuccess,
	        "driver rejected shader module");
	vk::PushConstantRange push {vk::ShaderStageFlagBits::eVertex, 0,
	                            shader.program.bindings.push_constant_size};
	vk::PipelineLayoutCreateInfo layout_info {};
	layout_info.pushConstantRangeCount = push.size != 0 ? 1 : 0;
	layout_info.pPushConstantRanges    = &push;
	vk::PipelineLayout layout;
	Require(device.createPipelineLayout(&layout_info, nullptr, &layout) == vk::Result::eSuccess,
	        "driver rejected pipeline layout");
	vk::PipelineShaderStageCreateInfo stage {};
	stage.stage  = vk::ShaderStageFlagBits::eVertex;
	stage.module = module;
	stage.pName  = "main";
	vk::PipelineVertexInputStateCreateInfo vertex_input {};
	vk::PipelineInputAssemblyStateCreateInfo assembly {};
	assembly.topology = vk::PrimitiveTopology::eTriangleList;
	vk::PipelineRasterizationStateCreateInfo rasterization {};
	rasterization.rasterizerDiscardEnable = VK_TRUE;
	rasterization.lineWidth = 1.0f;
	vk::PipelineRenderingCreateInfo rendering {};
	vk::GraphicsPipelineCreateInfo info {};
	info.pNext               = &rendering;
	info.stageCount          = 1;
	info.pStages             = &stage;
	info.pVertexInputState   = &vertex_input;
	info.pInputAssemblyState = &assembly;
	info.pRasterizationState = &rasterization;
	info.layout              = layout;
	vk::Pipeline pipeline;
	Require(device.createGraphicsPipelines(nullptr, 1, &info, nullptr, &pipeline) == vk::Result::eSuccess,
	        "driver rejected layer-output graphics pipeline");
	device.destroyPipeline(pipeline, nullptr);
	device.destroyPipelineLayout(layout, nullptr);
	device.destroyShaderModule(module, nullptr);
	std::printf("PASS: driver pipeline creation\n");
}
} // namespace

int main(int argc, char** argv) {
	Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "--device"),
	        "usage: vulkan_shader_compatibility_tests [--device]");
	Common::ThreadsSubsystem::Instance()->Init(nullptr);
	Config::ConfigSubsystem::Instance()->Init(nullptr);
	Config::ConfigOptions options;
	options.vulkan_validation_enabled = true;
	options.shader_validation_enabled = true;
	// Validation errors must fail this check even when normal gameplay uses soft exits.
	options.strict_unimplemented_enabled = true;
	Config::Load(options);
	Log::LogSubsystem::Instance()->Init(nullptr);
	const auto shaders = std::array {CheckLayerShader(false, false), CheckLayerShader(false, true),
	                                 CheckLayerShader(true, false), CheckLayerShader(true, true)};
	if (argc == 2 && std::string(argv[1]) == "--device") {
		Require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
		{
			Libs::Graphics::WindowContext window;
			window.window = SDL_CreateWindow("Vulkan compatibility test", SDL_WINDOWPOS_UNDEFINED,
			                                 SDL_WINDOWPOS_UNDEFINED, 64, 64,
			                                 SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
			Require(window.window != nullptr, SDL_GetError());
			window.graphic_ctx.screen_width = 64;
			window.graphic_ctx.screen_height = 64;
			window.CreateVulkan();
			Require(window.graphic_ctx.device != nullptr, "production device creation failed");
			Require(window.graphic_ctx.debug_messenger != nullptr,
			        "device check requires the installed Vulkan validation layer");
			for (const auto& shader: shaders) {
				CheckPipeline(window.graphic_ctx.device, shader);
			}
			std::printf("PASS: validation-enabled production device on %s\n", window.device_name);
		}
		SDL_Quit();
	}
	return 0;
}
