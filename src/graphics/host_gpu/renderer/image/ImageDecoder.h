#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_HOST_GPU_RENDERER_IMAGE_IMAGE_DECODER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_HOST_GPU_RENDERER_IMAGE_IMAGE_DECODER_H_

#include "graphics/guest_gpu/gpu_defs.h"

#include <cstdint>
#include <vector>

namespace Libs::Graphics {

// Decode ASTC-compressed guest texture data into RGBA8_UNORM so hosts without
// native ASTC support (the common case on desktop GPUs) can still sample textures
// that rely on ASTC, which GTA V uses for its highest-quality texture packs.
//
// `block_width`/`block_height` are the ASTC block footprint (e.g. 4x4, 6x6, 8x8,
// 10x10, 12x12, and the non-square variants 5x4, 6x5, 8x5, 8x6, 10x5, 10x6,
// 12x10). Returns the decoded RGBA8 tight buffer sized width*height*4.
std::vector<uint8_t> DecodeASTC(const uint8_t* data, uint32_t width, uint32_t height,
                               uint32_t block_width, uint32_t block_height);

// Convenience: true if the guest format is an ASTC format owned by the decoder.
bool IsASTCFormat(Prospero::BufferFormat format);

// Resolve the ASTC block footprint for a guest format, or {0,0} if not ASTC.
struct AstcBlockFootprint {
	uint32_t width;
	uint32_t height;
};
AstcBlockFootprint AstcBlockSize(Prospero::BufferFormat format);

} // namespace Libs::Graphics

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_HOST_GPU_RENDERER_IMAGE_IMAGE_DECODER_H_ */
