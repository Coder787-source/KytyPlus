#include "graphics/rt/hardware.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/vma.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Libs::Graphics {
namespace {

bool ExtensionAvailable(const std::vector<vk::ExtensionProperties>& available, const char* name) {
    return std::ranges::any_of(available, [name](const auto& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

void AppendExtensionIfMissing(std::vector<const char*>& extensions, const char* name) {
    if (std::ranges::none_of(extensions, [name](const char* extension) {
            return std::strcmp(extension, name) == 0;
        })) {
        extensions.push_back(name);
    }
}

// Helper: get buffer device address using the KHR dispatcher directly.
// Avoids vk::BufferDeviceAddressInfo which requires VK_KHR_buffer_device_address
// at compile time (not available on macOS MoltenVK).
static vk::DeviceAddress GetBufferAddress(const Graphics::VulkanBuffer& buf, vk::Device device) {
    if (!buf.buffer) { return 0; }
    VkBufferDeviceAddressInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
    info.buffer = static_cast<VkBuffer>(buf.buffer);
    return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetBufferDeviceAddressKHR(device, &info);
}

} // namespace

void GraphicContext::AppendHardwareRayTracingDeviceExtensions(
    const std::vector<vk::ExtensionProperties>& available_extensions,
    std::vector<const char*>&                   device_extensions) {

    const char* required[] = {
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
    };
    for (const auto* extension: required) {
        if (!ExtensionAvailable(available_extensions, extension)) {
            LOGF("Vulkan RT: extension %s is unavailable; hardware ray tracing disabled\n",
                 extension);
            rt_extensions_enabled = false;
            return;
        }
    }
    for (const auto* extension: required) {
        AppendExtensionIfMissing(device_extensions, extension);
    }
    if (ExtensionAvailable(available_extensions, VK_KHR_SPIRV_1_4_EXTENSION_NAME)) {
        AppendExtensionIfMissing(device_extensions, VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    }
    if (ExtensionAvailable(available_extensions, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME)) {
        AppendExtensionIfMissing(device_extensions, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    }

    rt_extensions_enabled = true;
    LOGF("Vulkan RT: enabling hardware ray query extensions\n");
}

void GraphicContext::LoadHardwareRayTracingFunctions() const {
    EXIT_IF(device == nullptr);
    if (!rt_extensions_enabled) {
        return;
    }

    const auto& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;
    if (dispatcher.vkGetBufferDeviceAddressKHR == nullptr ||
        dispatcher.vkCreateAccelerationStructureKHR == nullptr ||
        dispatcher.vkDestroyAccelerationStructureKHR == nullptr ||
        dispatcher.vkGetAccelerationStructureBuildSizesKHR == nullptr ||
        dispatcher.vkCmdBuildAccelerationStructuresKHR == nullptr ||
        dispatcher.vkGetAccelerationStructureDeviceAddressKHR == nullptr) {
        EXIT("Vulkan RT: failed to load required device functions\n");
    }
}

// ---------------------------------------------------------------------------
// RayTracingEngine implementation
// ---------------------------------------------------------------------------

RayTracingEngine::RayTracingEngine(const GraphicContext& ctx)
    : m_ctx(ctx), m_enabled(ctx.rt_extensions_enabled) {
    if (m_enabled) {
        LOGF("RayTracingEngine: initialized\n");
    }
}

RayTracingEngine::~RayTracingEngine() {
    if (!m_enabled) { return; }

    auto* device = m_ctx.device;
    for (auto& entry : m_blas_entries) {
        if (entry.as && entry.as->handle) {
            device->destroyAccelerationStructureKHR(entry.as->handle);
        }
    }
    for (auto& entry : m_tlas_entries) {
        if (entry.as && entry.as->handle) {
            device->destroyAccelerationStructureKHR(entry.as->handle);
        }
    }
    if (m_scratch) {
        m_ctx.DeleteBuffer(*m_scratch);
    }
}

bool RayTracingEngine::CreateAccelerationBuffer(
    vk::AccelerationStructureBuildGeometryInfoKHR& build_info,
    vk::AccelerationStructureBuildSizesInfoKHR&    size_info,
    vk::AccelerationStructureTypeKHR               type,
    std::unique_ptr<RtAccelerationStructure>&      out_as) {

    auto* device = m_ctx.device;

    auto as = std::make_unique<RtAccelerationStructure>();
    as->type = type;

    // Create the acceleration structure buffer.
    auto as_buffer = std::make_unique<Graphics::VulkanBuffer>();
    m_ctx.CreateBuffer(size_info.accelerationStructureSize, *as_buffer);
    if (!as_buffer->buffer) {
        LOGF("RayTracingEngine: failed to create AS buffer\n");
        return false;
    }

    // Create the acceleration structure.
    vk::AccelerationStructureCreateInfoKHR as_info{};
    as_info.buffer  = as_buffer->buffer;
    as_info.size    = size_info.accelerationStructureSize;
    as_info.type    = type;

    auto result = device->createAccelerationStructureKHR(&as_info, nullptr, &as->handle);
    if (result != vk::Result::eSuccess) {
        LOGF("RayTracingEngine: createAccelerationStructureKHR failed: %s\n",
             vk::to_string(result).c_str());
        m_ctx.DeleteBuffer(*as_buffer);
        return false;
    }

    // Get the device address.
    vk::AccelerationStructureDeviceAddressInfoKHR addr_info{};
    addr_info.accelerationStructure = as->handle;
    as->address = device->getAccelerationStructureDeviceAddressKHR(addr_info);
    as->buffer  = std::move(as_buffer);
    as->build_id = m_next_build_id++;

    // Ensure scratch buffer is large enough.
    if (size_info.buildScratchSize > m_scratch_size) {
        if (m_scratch) {
            m_ctx.DeleteBuffer(*m_scratch);
        }
        m_scratch = std::make_unique<Graphics::VulkanBuffer>();
        m_ctx.CreateBuffer(size_info.buildScratchSize, *m_scratch);
        if (!m_scratch->buffer) {
            LOGF("RayTracingEngine: failed to create scratch buffer (%" PRIu64 " bytes)\n",
                 size_info.buildScratchSize);
            return false;
        }
        m_scratch_size = size_info.buildScratchSize;
    }

    build_info.scratchData.deviceAddress =
        GetBufferAddress(*m_scratch, m_ctx.device);
    build_info.dstAccelerationStructure = as->handle;

    out_as = std::move(as);
    return true;
}

uint64_t RayTracingEngine::BuildBlas(std::span<const float>   vertices,
                                     std::span<const uint32_t> indices,
                                     vk::CommandBuffer         cmd) {
    if (!m_enabled) { return 0; }

    auto* device = m_ctx.device;

    // Create vertex buffer.
    auto vertex_buffer = std::make_unique<Graphics::VulkanBuffer>();
    m_ctx.CreateBuffer(vertices.size() * sizeof(float), *vertex_buffer);
    if (!vertex_buffer->buffer) { return 0; }

    void* mapped = nullptr;
    m_ctx.MapMemory(vertex_buffer->memory, mapped);
    if (mapped) {
        std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(float));
        m_ctx.UnmapMemory(vertex_buffer->memory);
    }

    // Create index buffer.
    auto index_buffer = std::make_unique<Graphics::VulkanBuffer>();
    m_ctx.CreateBuffer(indices.size() * sizeof(uint32_t), *index_buffer);
    if (!index_buffer->buffer) { return 0; }

    m_ctx.MapMemory(index_buffer->memory, mapped);
    if (mapped) {
        std::memcpy(mapped, indices.data(), indices.size() * sizeof(uint32_t));
        m_ctx.UnmapMemory(index_buffer->memory);
    }

    // Geometry info.
    vk::AccelerationStructureGeometryKHR geometry{};
    geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    geometry.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
    geometry.geometry.triangles.vertexData.deviceAddress =
        GetBufferAddress(*vertex_buffer, m_ctx.device);
    geometry.geometry.triangles.vertexStride   = sizeof(float) * 3;
    geometry.geometry.triangles.maxVertex       = static_cast<uint32_t>(vertices.size() / 3) - 1;
    geometry.geometry.triangles.indexType      = vk::IndexType::eUint32;
    geometry.geometry.triangles.indexData.deviceAddress =
        GetBufferAddress(*index_buffer, m_ctx.device);
    geometry.geometry.triangles.indexCount     = static_cast<uint32_t>(indices.size());
    geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;

    vk::AccelerationStructureBuildGeometryInfoKHR build_info{};
    build_info.type          = vk::AccelerationStructureTypeKHR::eBottomLevel;
    build_info.flags         = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    build_info.geometryCount = 1;
    build_info.pGeometries   = &geometry;

    // Get build sizes.
    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    uint32_t primitive_count = static_cast<uint32_t>(indices.size() / 3);
    device->getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        &build_info, &primitive_count, &size_info);

    // Create the AS.
    std::unique_ptr<RtAccelerationStructure> as;
    if (!CreateAccelerationBuffer(build_info, size_info,
                                   vk::AccelerationStructureTypeKHR::eBottomLevel, as)) {
        return 0;
    }

    // Build.
    vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount  = primitive_count;
    range_info.primitiveOffset = 0;
    range_info.firstVertex     = 0;
    range_info.transformOffset = 0;
    const auto* p_range_info = &range_info;

    cmd.buildAccelerationStructuresKHR(build_info, &p_range_info);

    // Store the entry.
    BlasEntry entry;
    entry.as       = std::move(as);
    entry.build_id = entry.as->build_id;
    m_blas_entries.push_back(std::move(entry));

    return m_blas_entries.back().build_id;
}

uint64_t RayTracingEngine::BuildTlas(
    std::span<const vk::AccelerationStructureInstanceKHR> instances,
    vk::CommandBuffer                                     cmd) {
    if (!m_enabled) { return 0; }

    auto* device = m_ctx.device;

    // Create instance buffer.
    auto instance_buffer = std::make_unique<Graphics::VulkanBuffer>();
    const auto instance_buffer_size = instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
    m_ctx.CreateBuffer(instance_buffer_size, *instance_buffer);
    if (!instance_buffer->buffer) { return 0; }

    void* mapped = nullptr;
    m_ctx.MapMemory(instance_buffer->memory, mapped);
    if (mapped) {
        std::memcpy(mapped, instances.data(), instance_buffer_size);
        m_ctx.UnmapMemory(instance_buffer->memory);
    }

    // Geometry info for TLAS.
    vk::AccelerationStructureGeometryKHR geometry{};
    geometry.geometryType = vk::GeometryTypeKHR::eInstances;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress =
        GetBufferAddress(*instance_buffer, m_ctx.device);

    vk::AccelerationStructureBuildGeometryInfoKHR build_info{};
    build_info.type          = vk::AccelerationStructureTypeKHR::eTopLevel;
    build_info.flags         = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    build_info.geometryCount = 1;
    build_info.pGeometries   = &geometry;

    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    uint32_t instance_count = static_cast<uint32_t>(instances.size());
    device->getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        &build_info, &instance_count, &size_info);

    std::unique_ptr<RtAccelerationStructure> as;
    if (!CreateAccelerationBuffer(build_info, size_info,
                                   vk::AccelerationStructureTypeKHR::eTopLevel, as)) {
        return 0;
    }

    vk::AccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount  = instance_count;
    range_info.primitiveOffset = 0;
    range_info.firstVertex     = 0;
    range_info.transformOffset = 0;
    const auto* p_range_info = &range_info;

    cmd.buildAccelerationStructuresKHR(build_info, &p_range_info);

    TlasEntry entry;
    entry.as       = std::move(as);
    entry.build_id = entry.as->build_id;
    m_tlas_entries.push_back(std::move(entry));

    return m_tlas_entries.back().build_id;
}

bool RayTracingEngine::CreateRayTracingPipeline(
    std::span<const vk::PipelineShaderStageCreateInfo> stages,
    std::span<const RtShaderGroup>                     groups,
    uint32_t                                           max_recursion_depth,
    vk::PipelineLayout                                 layout,
    RtPipeline&                                        out_pipeline) {
    if (!m_enabled) { return false; }

    auto* device = m_ctx.device;

    // Convert shader groups.
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> vk_groups;
    vk_groups.reserve(groups.size());
    for (const auto& g : groups) {
        vk::RayTracingShaderGroupCreateInfoKHR vk_group{};
        vk_group.type              = g.type;
        vk_group.generalShader     = g.general_shader;
        vk_group.closestHitShader  = g.closest_hit_shader;
        vk_group.anyHitShader     = g.any_hit_shader;
        vk_group.intersectionShader = g.intersection_shader;
        vk_groups.push_back(vk_group);
    }

    // Pipeline layout (must have push constants for SBT index).
    vk::RayTracingPipelineCreateInfoKHR pipeline_info{};
    pipeline_info.stageCount                   = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages                      = stages.data();
    pipeline_info.groupCount                   = static_cast<uint32_t>(vk_groups.size());
    pipeline_info.pGroups                      = vk_groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = max_recursion_depth;
    pipeline_info.layout                       = layout;

    auto result = device->createRayTracingPipelineKHR(
        VK_NULL_HANDLE, VK_NULL_HANDLE, pipeline_info, nullptr, &out_pipeline.pipeline);
    if (result != vk::Result::eSuccess) {
        LOGF("RayTracingEngine: createRayTracingPipelineKHR failed: %s\n",
             vk::to_string(result).c_str());
        return false;
    }

    out_pipeline.layout = layout;

    // Query SBT sizes.
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
    vk::PhysicalDeviceProperties2 props2{};
    props2.pNext = &rt_props;
    m_ctx.physical_device.getProperties2(&props2);

    const uint32_t handle_size      = rt_props.shaderGroupHandleSize;
    const uint32_t handle_alignment = rt_props.shaderGroupHandleAlignment;
    const uint32_t base_alignment   = rt_props.shaderGroupBaseAlignment;

    // Get shader group handles.
    std::vector<uint8_t> handles(handle_size * groups.size());
    result = device->getRayTracingShaderGroupHandlesKHR(
        out_pipeline.pipeline, 0, static_cast<uint32_t>(groups.size()),
        handles.size(), handles.data());
    if (result != vk::Result::eSuccess) {
        LOGF("RayTracingEngine: getRayTracingShaderGroupHandlesKHR failed\n");
        return false;
    }

    // Count groups per region.
    uint32_t rgen_count = 0, miss_count = 0, hit_count = 0, callable_count = 0;
    for (const auto& g : groups) {
        switch (g.type) {
            case vk::RayTracingShaderGroupTypeKHR::eGeneral:        rgen_count++; break;
            case vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup:
            case vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup: hit_count++; break;
            default: break;
        }
    }

    // Compute SBT sizes.
    auto align_up = [](uint32_t val, uint32_t align) {
        return (val + align - 1) & ~(align - 1);
    };

    const uint32_t rgen_size    = align_up(handle_size, handle_alignment);
    const uint32_t miss_size    = align_up(handle_size, handle_alignment);
    const uint32_t hit_size     = align_up(handle_size, handle_alignment);
    const uint32_t callable_size = align_up(handle_size, handle_alignment);

    const uint32_t rgen_stride    = align_up(rgen_size, base_alignment);
    const uint32_t miss_stride    = align_up(miss_size, base_alignment);
    const uint32_t hit_stride     = align_up(hit_size, base_alignment);
    const uint32_t callable_stride = align_up(callable_size, base_alignment);

    const uint32_t total_size = rgen_stride * rgen_count +
                                miss_stride * miss_count +
                                hit_stride * hit_count +
                                callable_stride * callable_count;

    // Create SBT buffer.
    auto sbt_buffer = std::make_unique<Graphics::VulkanBuffer>();
    m_ctx.CreateBuffer(total_size, *sbt_buffer);
    if (!sbt_buffer->buffer) { return false; }

    void* mapped = nullptr;
    m_ctx.MapMemory(sbt_buffer->memory, mapped);
    if (!mapped) { return false; }

    auto* dst = static_cast<uint8_t*>(mapped);
    uint32_t handle_index = 0;

    for (uint32_t g = 0; g < groups.size(); g++) {
        const auto& group = groups[g];
        uint8_t* region_base = nullptr;
        uint32_t stride = 0;

        switch (group.type) {
            case vk::RayTracingShaderGroupTypeKHR::eGeneral:
                region_base = dst;
                stride = rgen_stride;
                break;
            case vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup:
            case vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup:
                region_base = dst + rgen_stride * rgen_count + miss_stride * miss_count;
                stride = hit_stride;
                break;
            default:
                region_base = dst + rgen_stride * rgen_count + miss_stride * miss_count +
                              hit_stride * hit_count;
                stride = callable_stride;
                break;
        }

        std::memcpy(region_base + stride * handle_index,
                    handles.data() + handle_index * handle_size, handle_size);
        handle_index++;
    }

    m_ctx.UnmapMemory(sbt_buffer->memory);

    // Set up SBT regions.
    out_pipeline.rgen_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, m_ctx.device),
        rgen_size, rgen_stride
    };
    out_pipeline.miss_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, m_ctx.device) + rgen_stride * rgen_count,
        miss_size, miss_stride
    };
    out_pipeline.hit_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, m_ctx.device) + rgen_stride * rgen_count + miss_stride * miss_count,
        hit_size, hit_stride
    };
    out_pipeline.callable_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, m_ctx.device) + rgen_stride * rgen_count + miss_stride * miss_count + hit_stride * hit_count,
        callable_size, callable_stride
    };

    return true;
}

void RayTracingEngine::DestroyPipeline(RtPipeline& pipeline) {
    if (!m_enabled || !pipeline.pipeline) { return; }
    m_ctx.device->destroyPipeline(pipeline.pipeline);
    pipeline.pipeline = nullptr;
}

vk::Buffer RayTracingEngine::GetSbtBuffer(const RtPipeline& pipeline) const {
    // The SBT is stored at the rgen_region's device address.
    // We don't track the buffer handle directly; callers use the address regions.
    return VK_NULL_HANDLE;
}

} // namespace Libs::Graphics
