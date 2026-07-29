#include "graphics/host_gpu/renderer/pipelineCacheData.h"

#include <array>
#include <cassert>
#include <cstring>
#include <vector>

using Libs::Graphics::PipelineCacheDataIsCompatible;

int main() {
  vk::PhysicalDeviceProperties properties{};
  properties.vendorID = 0x1234;
  properties.deviceID = 0x5678;
  for (uint32_t i = 0; i < VK_UUID_SIZE; i++) {
    properties.pipelineCacheUUID[i] = static_cast<uint8_t>(i + 1);
  }

  VkPipelineCacheHeaderVersionOne header{};
  header.headerSize = sizeof(header);
  header.headerVersion = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
  header.vendorID = properties.vendorID;
  header.deviceID = properties.deviceID;
  std::memcpy(header.pipelineCacheUUID, properties.pipelineCacheUUID.data(),
              VK_UUID_SIZE);

  std::vector<uint8_t> data(sizeof(header) + 4);
  std::memcpy(data.data(), &header, sizeof(header));
  assert(PipelineCacheDataIsCompatible(data, properties));

  auto incompatible = properties;
  incompatible.deviceID++;
  assert(!PipelineCacheDataIsCompatible(data, incompatible));

  data.resize(sizeof(header) - 1);
  assert(!PipelineCacheDataIsCompatible(data, properties));
  return 0;
}
