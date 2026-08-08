#pragma once

#include "graphics/host_gpu/vulkanCommon.h"
#include "common/logging/log.h"

#include <vector>

class RayTracingEngine {
public:
	struct RtFunctions {
		PFN_vkCreateAccelerationStructureKHR            vkCreateAccelerationStructureKHR            = nullptr;
		PFN_vkDestroyAccelerationStructureKHR           vkDestroyAccelerationStructureKHR           = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR     = nullptr;
		PFN_vkCmdBuildAccelerationStructuresKHR         vkCmdBuildAccelerationStructuresKHR         = nullptr;
		PFN_vkGetAccelerationStructureDeviceAddressKHR  vkGetAccelerationStructureDeviceAddressKHR  = nullptr;
		PFN_vkCreateRayTracingPipelinesKHR              vkCreateRayTracingPipelinesKHR              = nullptr;
		PFN_vkGetRayTracingShaderGroupHandlesKHR        vkGetRayTracingShaderGroupHandlesKHR        = nullptr;
		PFN_vkCmdTraceRaysKHR                           vkCmdTraceRaysKHR                           = nullptr;
	};

	RayTracingEngine(vk::Device dev, bool enableRT);
	~RayTracingEngine();

	// Acceleration Structure Management
	vk::AccelerationStructureKHR CreateAccelerationStructure(
	    vk::AccelerationStructureTypeKHR                  type,
	    const vk::AccelerationStructureCreateInfoKHR& createInfo);

	// Ray Tracing Pipeline
	bool CreateRayTracingPipeline(vk::Pipeline& pipeline, vk::PipelineLayout layout);

	void TraceRays(
	    vk::CommandBuffer                                cmd,
	    const vk::StridedDeviceAddressRegionKHR*         pRaygenShaderBindingTable,
	    const vk::StridedDeviceAddressRegionKHR*         pMissShaderBindingTable,
	    const vk::StridedDeviceAddressRegionKHR*         pHitShaderBindingTable,
	    const vk::StridedDeviceAddressRegionKHR*         pCallableShaderBindingTable,
	    uint32_t width, uint32_t height, uint32_t depth);

	// Shader Binding Table
	std::vector<uint8_t> GetShaderGroupHandles(vk::Pipeline pipeline, uint32_t firstGroup,
	                                           uint32_t groupCount);

private:
	vk::Device                                     m_device;
	bool                                           m_enabled;
	RtFunctions                                    m_fn;
	std::vector<vk::AccelerationStructureKHR>      m_accelerationStructures;
};
