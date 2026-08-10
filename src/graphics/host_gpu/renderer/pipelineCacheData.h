#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINECACHEDATA_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINECACHEDATA_H_

#include "graphics/host_gpu/vulkanCommon.h"

#include <cstdint>
#include <span>

namespace Libs::Graphics {

[[nodiscard]] bool PipelineCacheDataIsCompatible(std::span<const uint8_t>            data,
                                                 const vk::PhysicalDeviceProperties& properties);

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINECACHEDATA_H_
