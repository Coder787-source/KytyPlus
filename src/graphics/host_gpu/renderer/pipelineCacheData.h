#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINECACHEDATA_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINECACHEDATA_H_

#include "graphics/host_gpu/vulkanCommon.h"

#include <cstdint>
#include <span>

namespace Libs::Graphics {

// Returns true if `data` (a VkPipelineCache serialized blob) may be used to
// seed a pipeline cache on the device described by `properties`.
//
// `require_same_uuid` (default true) additionally demands an exact
// pipelineCacheUUID match. The UUID changes whenever the driver updates its
// internal cache format; a mismatch does NOT make the blob invalid for the
// device (the spec only ties the header to vendor/device), so callers that
// want validate-and-reuse across driver updates pass false and let the
// driver itself decide which entries are reusable.
[[nodiscard]] bool PipelineCacheDataIsCompatible(std::span<const uint8_t>            data,
                                                 const vk::PhysicalDeviceProperties& properties,
                                                 bool                                require_same_uuid = true);

// True if the cache header's pipelineCacheUUID exactly matches the current
// driver. Used for diagnostics ("cache written by another driver version").
[[nodiscard]] bool PipelineCacheDataUuidMatches(std::span<const uint8_t>            data,
                                                const vk::PhysicalDeviceProperties& properties);

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINECACHEDATA_H_
