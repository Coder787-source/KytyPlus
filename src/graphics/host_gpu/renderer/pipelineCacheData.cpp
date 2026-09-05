#include "graphics/host_gpu/renderer/pipelineCacheData.h"

#include <cstring>

namespace {

bool HeaderMatchesDevice(std::span<const uint8_t>            data,
                         const vk::PhysicalDeviceProperties& properties) {
	if (data.size() < sizeof(VkPipelineCacheHeaderVersionOne)) {
		return false;
	}

	VkPipelineCacheHeaderVersionOne header {};
	std::memcpy(&header, data.data(), sizeof(header));

	return header.headerSize >= sizeof(header) &&
	       header.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
	       header.vendorID == properties.vendorID && header.deviceID == properties.deviceID;
}

} // namespace

namespace Libs::Graphics {

bool PipelineCacheDataIsCompatible(std::span<const uint8_t>            data,
                                   const vk::PhysicalDeviceProperties& properties,
                                   bool                                require_same_uuid) {
	if (!HeaderMatchesDevice(data, properties)) {
		return false;
	}
	if (!require_same_uuid) {
		return true;
	}
	return PipelineCacheDataUuidMatches(data, properties);
}

bool PipelineCacheDataUuidMatches(std::span<const uint8_t>            data,
                                  const vk::PhysicalDeviceProperties& properties) {
	if (data.size() < sizeof(VkPipelineCacheHeaderVersionOne)) {
		return false;
	}

	VkPipelineCacheHeaderVersionOne header {};
	std::memcpy(&header, data.data(), sizeof(header));

	return std::memcmp(header.pipelineCacheUUID, properties.pipelineCacheUUID.data(),
	                   VK_UUID_SIZE) == 0;
}

} // namespace Libs::Graphics
