#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SHADERRECOMPILER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SHADERRECOMPILER_H_

#include "common/common.h"
#include "common/stringUtils.h"
#include "graphics/shader/recompiler/ir/ResourceMaterialization.h"
#include "graphics/shader/shader.h"

#include <optional>
#include <span>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler {

// IsaFamily is fully defined in ShaderDecoder.h (reached transitively via
// ResourceMaterialization.h -> SrtWalker.h -> ShaderIR.h -> ShaderCFG.h).
// Keeping the enum definition there avoids a circular include between this
// upper-layer header and the decoder leaf.

struct CompileOptions {
	// Graphics ISA family the shader binary targets. PS5 (Prospero) ships RDNA2 /
	// AGC shaders; PS4 (Orbis) ships GCN (GFX8/GFX9) shaders. The decoder must know
	// which family it is parsing because the two share the word-family dispatch shape
	// but diverge in encoding details and opcode tables.
	IsaFamily                     isa_family      = IsaFamily::Rdna2;
	ShaderType                    stage           = ShaderType::Compute;
	ShaderLaneMaskMode            lane_mask_mode  = ShaderLaneMaskMode::NativeWave;
	uint32_t                      wave_size       = 64;
	uint32_t                      user_data_base  = 0;
	uint32_t                      user_data_count = 64;
	uint64_t                      shader_hash     = 0;
	uint64_t                      shader_base     = 0;
	std::optional<uint64_t>       flat_memory_base;
	uint32_t                      descriptor_set       = 0;
	uint32_t                      push_constant_offset = 0;
	bool                          dump_ir              = true;
	bool                          early_dump           = false;
	bool                          force_dispatcher     = false;
	const char*                   dump_label           = nullptr;
	const uint32_t*               user_data            = nullptr;
	IR::SrtMemoryReader           read_memory          = nullptr;
	void*                         read_memory_data     = nullptr;
	const IR::ResourceSnapshot*   resource_snapshot    = nullptr;
	const ShaderVertexInputInfo*  vertex_input_info    = nullptr;
	const ShaderPixelInputInfo*   pixel_input_info     = nullptr;
	const ShaderComputeInputInfo* compute_input_info   = nullptr;
};

struct CompileResult {
	std::vector<uint32_t> spirv;
	std::string           decoded_dump;
	std::string           ir_dump;
	IR::Program           program;
	IR::ResourceSnapshot  resources;
};

bool TryRecompile(std::span<const uint32_t> code, const CompileOptions& options,
	              CompileResult& result, std::string* error);

} // namespace Libs::Graphics::ShaderRecompiler

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SHADERRECOMPILER_H_ */
