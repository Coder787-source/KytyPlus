#include "hardware.h"
#include "graphics/vulkanCommon.h"
#include "logging.h"

RayTracingEngine::RayTracingEngine(vk::Device dev, bool enableRT) : m_device(dev), m_enabled(enableRT) {
    if (!m_enabled) return;

    // Load Vulkan RT functions
    m_fn.vkCreateAccelerationStructureKHR = LoadDeviceFn<PFN_vkCreateAccelerationStructureKHR>(m_device, "vkCreateAccelerationStructureKHR");
    m_fn.vkDestroyAccelerationStructureKHR = LoadDeviceFn<PFN_vkDestroyAccelerationStructureKHR>(m_device, "vkDestroyAccelerationStructureKHR");
    m_fn.vkGetAccelerationStructureBuildSizesKHR = LoadDeviceFn<PFN_vkGetAccelerationStructureBuildSizesKHR>(m_device, "vkGetAccelerationStructureBuildSizesKHR");
    m_fn.vkCmdBuildAccelerationStructuresKHR = LoadDeviceFn<PFN_vkCmdBuildAccelerationStructuresKHR>(m_device, "vkCmdBuildAccelerationStructuresKHR");
    m_fn.vkGetAccelerationStructureDeviceAddressKHR = LoadDeviceFn<PFN_vkGetAccelerationStructureDeviceAddressKHR>(m_device, "vkGetAccelerationStructureDeviceAddressKHR");
    m_fn.vkCreateRayTracingPipelinesKHR = LoadDeviceFn<PFN_vkCreateRayTracingPipelinesKHR>(m_device, "vkCreateRayTracingPipelinesKHR");
    m_fn.vkGetRayTracingShaderGroupHandlesKHR = LoadDeviceFn<PFN_vkGetRayTracingShaderGroupHandlesKHR>(m_device, "vkGetRayTracingShaderGroupHandlesKHR");
    m_fn.vkCmdTraceRaysKHR = LoadDeviceFn<PFN_vkCmdTraceRaysKHR>(m_device, "vkCmdTraceRaysKHR");

    if (!m_fn.vkCmdTraceRaysKHR) {
        LOGF("RayTracingEngine: Failed to load vkCmdTraceRaysKHR\n");
        m_enabled = false;
    }
}

RayTracingEngine::~RayTracingEngine() {
    for (auto& as : m_accelerationStructures) {
        m_device.destroyAccelerationStructureKHR(as);
    }
}

vk::AccelerationStructureKHR RayTracingEngine::CreateAccelerationStructure(
    vk::AccelerationStructureTypeKHR type,
    const vk::AccelerationStructureCreateInfoKHR& createInfo) {

    vk::AccelerationStructureKHR accelerationStructure = m_device.createAccelerationStructureKHR(createInfo);
    m_accelerationStructures.push_back(accelerationStructure);
    return accelerationStructure;
}

bool RayTracingEngine::CreateGTAVPipeline(vk::Pipeline& pipeline, vk::PipelineLayout layout) {
    // GTA V uses 3 shader groups: RayGen, Miss, and Hit
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups(3);

    // RayGen group (GTA V's primary ray shader)
    groups[0].setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    groups[0].setGeneralShader(0); // Index of RayGen shader in shaderStages

    // Miss group (GTA V's sky/environment shader)
    groups[1].setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    groups[1].setGeneralShader(1); // Index of Miss shader in shaderStages

    // Hit group (GTA V's material shader)
    groups[2].setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
    groups[2].setClosestHitShader(2); // Index of ClosestHit shader in shaderStages

    // Create the pipeline
    vk::RayTracingPipelineCreateInfoKHR pipelineInfo;
    pipelineInfo.setStages(shaderStages); // Assume shaderStages is populated
    pipelineInfo.setGroups(groups);
    pipelineInfo.setMaxPipelineRayRecursionDepth(1); // GTA V uses single-bounce RT
    pipelineInfo.setLayout(layout);

    try {
        pipeline = m_device.createRayTracingPipelineKHR({}, pipelineInfo).value;
        return true;
    } catch (const vk::SystemError& err) {
        LOGF("Failed to create GTA V RT pipeline: %s\n", err.what());
        return false;
    }
}

void RayTracingEngine::TraceRays(
    vk::CommandBuffer cmd,
    const vk::StridedDeviceAddressRegionKHR* pRaygenShaderBindingTable,
    const vk::StridedDeviceAddressRegionKHR* pMissShaderBindingTable,
    const vk::StridedDeviceAddressRegionKHR* pHitShaderBindingTable,
    const vk::StridedDeviceAddressRegionKHR* pCallableShaderBindingTable,
    uint32_t width, uint32_t height, uint32_t depth) {

    if (!m_enabled || !m_fn.vkCmdTraceRaysKHR) {
        LOGF("RayTracingEngine: vkCmdTraceRaysKHR not available\n");
        return;
    }

    m_fn.vkCmdTraceRaysKHR(
        cmd,
        pRaygenShaderBindingTable,
        pMissShaderBindingTable,
        pHitShaderBindingTable,
        pCallableShaderBindingTable,
        width, height, depth);
}

std::vector<uint8_t> RayTracingEngine::GetShaderGroupHandles(vk::Pipeline pipeline, uint32_t firstGroup, uint32_t groupCount) {
    if (!m_enabled || !m_fn.vkGetRayTracingShaderGroupHandlesKHR) {
        return {};
    }

    uint32_t handleSize = 32; // GTA V uses 32-byte handles
    std::vector<uint8_t> handles(groupCount * handleSize);
    m_fn.vkGetRayTracingShaderGroupHandlesKHR(
        m_device, pipeline, firstGroup, groupCount, handles.size(), handles.data());
    return handles;
}