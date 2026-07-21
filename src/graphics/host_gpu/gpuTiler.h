#pragma once

#include "graphics/guest_gpu/tile.h"

#include <cstdint>
#include <functional>
#include <span>

namespace Libs::Graphics {

struct GraphicContext;
struct VulkanBuffer;
class CommandBuffer;

struct GpuTileInfo {
	TileBlockFamily family              = TileBlockFamily::Count;
	uint32_t        bytes_per_element   = 0;
	uint64_t        linear_offset       = 0;
	uint64_t        linear_size         = 0;
	uint64_t        tiled_offset        = 0;
	uint64_t        tiled_size          = 0;
	uint64_t        linear_slice_stride = 0;
	uint32_t        width               = 0;
	uint32_t        height              = 0;
	uint32_t        depth               = 1;
	uint32_t        pitch               = 0;
	uint32_t        tail_x              = 0;
	uint32_t        tail_y              = 0;
	bool            tail                = false;
	uint32_t        tiled_width         = 0;
	uint32_t        tiled_height        = 0;
	uint32_t        surface_z           = 0;
};

using GpuTileRecord = std::function<void(CommandBuffer&, VulkanBuffer&)>;

void GpuDetile(const void* tiled, void* linear, uint64_t tiled_capacity,
               uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
               const GpuTileRecord& after = {});
void GpuTile(const void* linear, void* tiled, uint64_t tiled_capacity,
             uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
             const GpuTileRecord& before = {});
void GpuTileRelease();

} // namespace Libs::Graphics
