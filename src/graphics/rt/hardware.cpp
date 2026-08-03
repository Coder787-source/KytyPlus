#include "graphics/rt/hardware.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/vma.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <limits>

namespace Libs::Graphics {
namespace {

// Helper: get buffer device address using the KHR dispatcher directly.
// Avoids vk::BufferDeviceAddressInfo which requires VK_KHR_buffer_device_address
// at compile time (not available on macOS MoltenVK).
static vk::DeviceAddress GetBufferAddress(const Graphics::VulkanBuffer& buf, VkDevice device) {
    if (!buf.buffer) { return 0; }
    VkBufferDeviceAddressInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
    info.buffer = static_cast<VkBuffer>(buf.buffer);
    return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetBufferDeviceAddressKHR(device, &info);
}

// Load a device function pointer from vkGetDeviceProcAddr.
template <typename T>
static T LoadDeviceFn(VkDevice device, const char* name) {
    auto* ptr = reinterpret_cast<T>(VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr(device, name));
    EXIT_IF(ptr == nullptr && "Failed to load Vulkan device function");
    return ptr;
}

} // namespace

// ---------------------------------------------------------------------------
// RayTracingEngine implementation
// ---------------------------------------------------------------------------

RayTracingEngine::RayTracingEngine(GraphicContext& ctx)
    : m_ctx(ctx), m_enabled(ctx.rt_extensions_enabled) {
    if (m_enabled) {
        VkDevice dev = static_cast<VkDevice>(ctx.device);

        // Load all RT function pointers at construction time.
        m_fn.vkCreateAccelerationStructureKHR = LoadDeviceFn<PFN_vkCreateAccelerationStructureKHR>(
            dev, "vkCreateAccelerationStructureKHR");
        m_fn.vkDestroyAccelerationStructureKHR = LoadDeviceFn<PFN_vkDestroyAccelerationStructureKHR>(
            dev, "vkDestroyAccelerationStructureKHR");
        m_fn.vkGetAccelerationStructureBuildSizesKHR = LoadDeviceFn<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            dev, "vkGetAccelerationStructureBuildSizesKHR");
        m_fn.vkCmdBuildAccelerationStructuresKHR = LoadDeviceFn<PFN_vkCmdBuildAccelerationStructuresKHR>(
            dev, "vkCmdBuildAccelerationStructuresKHR");
        m_fn.vkGetAccelerationStructureDeviceAddressKHR = LoadDeviceFn<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            dev, "vkGetAccelerationStructureDeviceAddressKHR");
        m_fn.vkCreateRayTracingPipelinesKHR = LoadDeviceFn<PFN_vkCreateRayTracingPipelinesKHR>(
            dev, "vkCreateRayTracingPipelineKHR");
        m_fn.vkGetRayTracingShaderGroupHandlesKHR = LoadDeviceFn<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            dev, "vkGetRayTracingShaderGroupHandlesKHR");

        LOGF("RayTracingEngine: initialized with %zu function pointers\n", sizeof(m_fn));
    }
}

RayTracingEngine::~RayTracingEngine() {
    if (!m_enabled) { return; }

    VkDevice dev = static_cast<VkDevice>(m_ctx.device);
    for (auto& entry : m_blas_entries) {
        if (entry.as && entry.as->handle) {
            m_fn.vkDestroyAccelerationStructureKHR(
                dev, static_cast<VkAccelerationStructureKHR>(entry.as->handle), nullptr);
        }
    }
    for (auto& entry : m_tlas_entries) {
        if (entry.as && entry.as->handle) {
            m_fn.vkDestroyAccelerationStructureKHR(
                dev, static_cast<VkAccelerationStructureKHR>(entry.as->handle), nullptr);
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

    VkDevice dev = static_cast<VkDevice>(m_ctx.device);

    auto as = std::make_unique<RtAccelerationStructure>();
    as->type = type;

    // Create the acceleration structure buffer.
    auto as_buffer = std::make_unique<Graphics::VulkanBuffer>();
    m_ctx.CreateBuffer(size_info.accelerationStructureSize, *as_buffer);
    if (!as_buffer->buffer) {
        LOGF("RayTracingEngine: failed to create AS buffer\n");
        return false;
    }

    // Create the acceleration structure (raw C API).
    VkAccelerationStructureCreateInfoKHR as_info_c{};
    as_info_c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    as_info_c.buffer = static_cast<VkBuffer>(as_buffer->buffer);
    as_info_c.size = size_info.accelerationStructureSize;
    as_info_c.type = static_cast<VkAccelerationStructureTypeKHR>(static_cast<int>(type));

    VkAccelerationStructureKHR vk_as = VK_NULL_HANDLE;
    auto result_c = m_fn.vkCreateAccelerationStructureKHR(dev, &as_info_c, nullptr, &vk_as);
    if (result_c != VK_SUCCESS) {
        LOGF("RayTracingEngine: createAccelerationStructureKHR failed: %d\n", static_cast<int>(result_c));
        m_ctx.DeleteBuffer(*as_buffer);
        return false;
    }
    as->handle = vk_as;

    // Get the device address (raw C API).
    VkAccelerationStructureDeviceAddressInfoKHR addr_info_c{};
    addr_info_c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addr_info_c.accelerationStructure = vk_as;
    as->address = m_fn.vkGetAccelerationStructureDeviceAddressKHR(dev, &addr_info_c);
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
        GetBufferAddress(*m_scratch, dev);
    build_info.dstAccelerationStructure = vk_as;

    out_as = std::move(as);
    return true;
}

uint64_t RayTracingEngine::BuildBlas(std::span<const float>   vertices,
                                     std::span<const uint32_t> indices,
                                     vk::CommandBuffer         cmd) {
    if (!m_enabled) { return 0; }

    VkDevice dev = static_cast<VkDevice>(m_ctx.device);

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
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress =
        GetBufferAddress(*vertex_buffer, dev);
    geometry.geometry.triangles.vertexStride   = sizeof(float) * 3;
    geometry.geometry.triangles.maxVertex       = static_cast<uint32_t>(vertices.size() / 3) - 1;
    geometry.geometry.triangles.indexType      = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress =
        GetBufferAddress(*index_buffer, dev);
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    // Get build sizes.
    VkAccelerationStructureBuildGeometryInfoKHR build_info_c{};
    build_info_c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_info_c.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_info_c.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_info_c.geometryCount = 1;
    build_info_c.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR size_info_c{};
    size_info_c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    uint32_t primitive_count = static_cast<uint32_t>(indices.size() / 3);
    m_fn.vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_info_c, &primitive_count, &size_info_c);

    // Convert to C++ struct for CreateAccelerationBuffer.
    vk::AccelerationStructureBuildGeometryInfoKHR build_info{};
    build_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    build_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    build_info.geometryCount = 1;
    build_info.pGeometries = reinterpret_cast<const vk::AccelerationStructureGeometryKHR*>(&geometry);

    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    size_info.accelerationStructureSize = size_info_c.accelerationStructureSize;
    size_info.buildScratchSize = size_info_c.buildScratchSize;
    size_info.updateScratchSize = size_info_c.updateScratchSize;

    // Create the AS.
    std::unique_ptr<RtAccelerationStructure> as;
    if (!CreateAccelerationBuffer(build_info, size_info,
                                   vk::AccelerationStructureTypeKHR::eBottomLevel, as)) {
        return 0;
    }

    // Build using raw C API.
    VkAccelerationStructureBuildGeometryInfoKHR build_cmd_info{};
    build_cmd_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_cmd_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_cmd_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_cmd_info.geometryCount = 1;
    build_cmd_info.pGeometries = &geometry;
    build_cmd_info.scratchData.deviceAddress = build_info.scratchData.deviceAddress;
    build_cmd_info.dstAccelerationStructure = static_cast<VkAccelerationStructureKHR>(build_info.dstAccelerationStructure);
    build_cmd_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    VkAccelerationStructureBuildRangeInfoKHR range_info_c{};
    range_info_c.primitiveCount  = primitive_count;
    range_info_c.primitiveOffset = 0;
    range_info_c.firstVertex     = 0;
    range_info_c.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* p_range_info_c = &range_info_c;

    m_fn.vkCmdBuildAccelerationStructuresKHR(
        static_cast<VkCommandBuffer>(cmd), 1, &build_cmd_info, &p_range_info_c);

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

    VkDevice dev = static_cast<VkDevice>(m_ctx.device);

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
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress =
        GetBufferAddress(*instance_buffer, dev);

    // Get build sizes.
    VkAccelerationStructureBuildGeometryInfoKHR build_info_c{};
    build_info_c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_info_c.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build_info_c.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_info_c.geometryCount = 1;
    build_info_c.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR size_info_c{};
    size_info_c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    uint32_t instance_count = static_cast<uint32_t>(instances.size());
    m_fn.vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_info_c, &instance_count, &size_info_c);

    // Convert to C++ struct for CreateAccelerationBuffer.
    vk::AccelerationStructureBuildGeometryInfoKHR build_info{};
    build_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    build_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    build_info.geometryCount = 1;
    build_info.pGeometries = reinterpret_cast<const vk::AccelerationStructureGeometryKHR*>(&geometry);

    vk::AccelerationStructureBuildSizesInfoKHR size_info{};
    size_info.accelerationStructureSize = size_info_c.accelerationStructureSize;
    size_info.buildScratchSize = size_info_c.buildScratchSize;
    size_info.updateScratchSize = size_info_c.updateScratchSize;

    std::unique_ptr<RtAccelerationStructure> as;
    if (!CreateAccelerationBuffer(build_info, size_info,
                                   vk::AccelerationStructureTypeKHR::eTopLevel, as)) {
        return 0;
    }

    // Build TLAS using raw C API.
    VkAccelerationStructureBuildGeometryInfoKHR tlas_cmd_info{};
    tlas_cmd_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlas_cmd_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_cmd_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlas_cmd_info.geometryCount = 1;
    tlas_cmd_info.pGeometries = &geometry;
    tlas_cmd_info.scratchData.deviceAddress = build_info.scratchData.deviceAddress;
    tlas_cmd_info.dstAccelerationStructure = static_cast<VkAccelerationStructureKHR>(build_info.dstAccelerationStructure);
    tlas_cmd_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    VkAccelerationStructureBuildRangeInfoKHR tlas_range_info_c{};
    tlas_range_info_c.primitiveCount  = instance_count;
    tlas_range_info_c.primitiveOffset = 0;
    tlas_range_info_c.firstVertex     = 0;
    tlas_range_info_c.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* p_tlas_range_info_c = &tlas_range_info_c;

    m_fn.vkCmdBuildAccelerationStructuresKHR(
        static_cast<VkCommandBuffer>(cmd), 1, &tlas_cmd_info, &p_tlas_range_info_c);

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

    VkDevice dev = static_cast<VkDevice>(m_ctx.device);

    // Convert shader groups to C structs.
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> vk_groups;
    vk_groups.reserve(groups.size());
    for (const auto& g : groups) {
        VkRayTracingShaderGroupCreateInfoKHR vk_group{};
        vk_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        vk_group.type = static_cast<VkRayTracingShaderGroupTypeKHR>(static_cast<int>(g.type));
        vk_group.generalShader = g.general_shader;
        vk_group.closestHitShader = g.closest_hit_shader;
        vk_group.anyHitShader = g.any_hit_shader;
        vk_group.intersectionShader = g.intersection_shader;
        vk_groups.push_back(vk_group);
    }

    // Create the pipeline using raw C API.
    VkRayTracingPipelineCreateInfoKHR pipeline_info_c{};
    pipeline_info_c.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipeline_info_c.stageCount = static_cast<uint32_t>(stages.size());
    pipeline_info_c.pStages = reinterpret_cast<const VkPipelineShaderStageCreateInfo*>(stages.data());
    pipeline_info_c.groupCount = static_cast<uint32_t>(vk_groups.size());
    pipeline_info_c.pGroups = vk_groups.data();
    pipeline_info_c.maxPipelineRayRecursionDepth = max_recursion_depth;
    pipeline_info_c.layout = static_cast<VkPipelineLayout>(layout);

    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    auto result_c = m_fn.vkCreateRayTracingPipelinesKHR(
        dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeline_info_c, nullptr, &vk_pipeline);
    if (result_c != VK_SUCCESS) {
        LOGF("RayTracingEngine: createRayTracingPipelineKHR failed: %d\n", static_cast<int>(result_c));
        return false;
    }
    out_pipeline.pipeline = vk_pipeline;
    out_pipeline.layout = layout;

    // Query SBT properties.
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{};
    rt_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rt_props;
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceProperties2(
        static_cast<VkPhysicalDevice>(m_ctx.physical_device), &props2);

    const uint32_t handle_size      = rt_props.shaderGroupHandleSize;
    const uint32_t handle_alignment = rt_props.shaderGroupHandleAlignment;
    const uint32_t base_alignment   = rt_props.shaderGroupBaseAlignment;

    // Get shader group handles (raw C API).
    std::vector<uint8_t> handles(handle_size * groups.size());
    auto result_c2 = m_fn.vkGetRayTracingShaderGroupHandlesKHR(
        dev, vk_pipeline, 0, static_cast<uint32_t>(groups.size()),
        handles.size(), handles.data());
    if (result_c2 != VK_SUCCESS) {
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
        GetBufferAddress(*sbt_buffer, dev),
        rgen_size, rgen_stride
    };
    out_pipeline.miss_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, dev) + rgen_stride * rgen_count,
        miss_size, miss_stride
    };
    out_pipeline.hit_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, dev) + rgen_stride * rgen_count + miss_stride * miss_count,
        hit_size, hit_stride
    };
    out_pipeline.callable_region = vk::StridedDeviceAddressRegionKHR{
        GetBufferAddress(*sbt_buffer, dev) + rgen_stride * rgen_count + miss_stride * miss_count + hit_stride * hit_count,
        callable_size, callable_stride
    };

    return true;
}

void RayTracingEngine::DestroyPipeline(RtPipeline& pipeline) {
    if (!m_enabled || !pipeline.pipeline) { return; }
    VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyPipeline(
        static_cast<VkDevice>(m_ctx.device), static_cast<VkPipeline>(pipeline.pipeline), nullptr);
    pipeline.pipeline = nullptr;
}

vk::Buffer RayTracingEngine::GetSbtBuffer(const RtPipeline& pipeline) const {
    return VK_NULL_HANDLE;
}

} // namespace Libs::Graphics
