#include "hardware.h"

#include <cstring>

template <typename T>
static T LoadDeviceProc(vk::Device device, const char* name) {
	return reinterpret_cast<T>(
	    static_cast<PFN_vkVoidFunction>(device.getProcAddr(name)));
}

RayTracingEngine::RayTracingEngine(vk::Device dev, bool enableRT)
    : m_device(dev), m_enabled(enableRT) {
	if (!m_enabled) return;

	m_fn.vkCreateAccelerationStructureKHR =
	    LoadDeviceProc<PFN_vkCreateAccelerationStructureKHR>(m_device, "vkCreateAccelerationStructureKHR");
	m_fn.vkDestroyAccelerationStructureKHR =
	    LoadDeviceProc<PFN_vkDestroyAccelerationStructureKHR>(m_device, "vkDestroyAccelerationStructureKHR");
	m_fn.vkGetAccelerationStructureBuildSizesKHR =
	    LoadDeviceProc<PFN_vkGetAccelerationStructureBuildSizesKHR>(m_device, "vkGetAccelerationStructureBuildSizesKHR");
	m_fn.vkCmdBuildAccelerationStructuresKHR =
	    LoadDeviceProc<PFN_vkCmdBuildAccelerationStructuresKHR>(m_device, "vkCmdBuildAccelerationStructuresKHR");
	m_fn.vkGetAccelerationStructureDeviceAddressKHR =
	    LoadDeviceProc<PFN_vkGetAccelerationStructureDeviceAddressKHR>(m_device, "vkGetAccelerationStructureDeviceAddressKHR");
	m_fn.vkCreateRayTracingPipelinesKHR =
	    LoadDeviceProc<PFN_vkCreateRayTracingPipelinesKHR>(m_device, "vkCreateRayTracingPipelinesKHR");
	m_fn.vkGetRayTracingShaderGroupHandlesKHR =
	    LoadDeviceProc<PFN_vkGetRayTracingShaderGroupHandlesKHR>(m_device, "vkGetRayTracingShaderGroupHandlesKHR");
	m_fn.vkCmdTraceRaysKHR =
	    LoadDeviceProc<PFN_vkCmdTraceRaysKHR>(m_device, "vkCmdTraceRaysKHR");

	if (!m_fn.vkCmdTraceRaysKHR) {
		LOGF("RayTracingEngine: Failed to load vkCmdTraceRaysKHR\n");
		m_enabled = false;
	}
}

RayTracingEngine::~RayTracingEngine() {
	for (auto& as : m_accelerationStructures) {
		if (m_fn.vkDestroyAccelerationStructureKHR) {
			m_fn.vkDestroyAccelerationStructureKHR(m_device, as, nullptr);
		}
	}
}

vk::AccelerationStructureKHR RayTracingEngine::CreateAccelerationStructure(
    vk::AccelerationStructureTypeKHR              type,
    const vk::AccelerationStructureCreateInfoKHR& createInfo) {

	vk::AccelerationStructureKHR accelerationStructure {};
	if (m_fn.vkCreateAccelerationStructureKHR) {
		m_fn.vkCreateAccelerationStructureKHR(m_device, &createInfo, nullptr,
		                                      reinterpret_cast<VkAccelerationStructureKHR*>(&accelerationStructure));
	}
	m_accelerationStructures.push_back(accelerationStructure);
	return accelerationStructure;
}

bool RayTracingEngine::CreateRayTracingPipeline(vk::Pipeline& pipeline, vk::PipelineLayout layout) {
	if (!m_fn.vkCreateRayTracingPipelinesKHR) {
		return false;
	}

	// Three shader groups: RayGen, Miss, and Hit
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups(3);

	groups[0].setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
	groups[0].setGeneralShader(0);

	groups[1].setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
	groups[1].setGeneralShader(1);

	groups[2].setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
	groups[2].setClosestHitShader(2);

	vk::RayTracingPipelineCreateInfoKHR pipelineInfo {};
	pipelineInfo.setGroups(groups);
	pipelineInfo.setMaxPipelineRayRecursionDepth(1);
	pipelineInfo.setLayout(layout);

	vk::Pipeline result {};
	auto         res = m_fn.vkCreateRayTracingPipelinesKHR(
                m_device, nullptr, nullptr, 1, &pipelineInfo, nullptr,
                reinterpret_cast<VkPipeline*>(&result));
	if (res == VK_SUCCESS) {
		pipeline = result;
		return true;
	}
	LOGF("RayTracingEngine: failed to create RT pipeline (VkResult=%d)\n",
	     static_cast<int>(res));
	return false;
}

void RayTracingEngine::TraceRays(
    vk::CommandBuffer                        cmd,
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
	    reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(pRaygenShaderBindingTable),
	    reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(pMissShaderBindingTable),
	    reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(pHitShaderBindingTable),
	    reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(pCallableShaderBindingTable),
	    width, height, depth);
}

std::vector<uint8_t> RayTracingEngine::GetShaderGroupHandles(vk::Pipeline pipeline,
                                                             uint32_t firstGroup,
                                                             uint32_t groupCount) {
	if (!m_enabled || !m_fn.vkGetRayTracingShaderGroupHandlesKHR) {
		return {};
	}

	constexpr uint32_t         handleSize = 32;
	std::vector<uint8_t>       handles(groupCount * handleSize);
	m_fn.vkGetRayTracingShaderGroupHandlesKHR(
	    m_device, pipeline, firstGroup, groupCount,
	    static_cast<uint32_t>(handles.size()), handles.data());
	return handles;
}
