#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RT_HARDWARE_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RT_HARDWARE_H_

#include "graphics/host_gpu/graphicContext.h"

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace Libs::Graphics {

// ---------------------------------------------------------------------------
// Ray tracing pipeline infrastructure for KytyPlus.
// Provides BLAS/TLAS building, SBT creation, and RT pipeline creation.
// Gracefully degrades if hardware RT is unavailable.
// ---------------------------------------------------------------------------

struct RtAccelerationStructure {
    vk::AccelerationStructureKHR handle   = nullptr;
    std::unique_ptr<Graphics::VulkanBuffer> buffer;
    vk::DeviceAddress            address  = 0;
    vk::AccelerationStructureTypeKHR type  = vk::AccelerationStructureTypeKHR::eBottomLevel;
    uint64_t                    build_id = 0;
};

struct RtShaderGroup {
    vk::RayTracingShaderGroupTypeKHR type    = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    uint32_t                         general_shader = VK_SHADER_UNUSED_KHR;
    uint32_t                         closest_hit_shader = VK_SHADER_UNUSED_KHR;
    uint32_t                         any_hit_shader     = VK_SHADER_UNUSED_KHR;
    uint32_t                         intersection_shader = VK_SHADER_UNUSED_KHR;
};

struct RtPipeline {
    vk::Pipeline                        pipeline  = nullptr;
    vk::PipelineLayout                  layout    = nullptr;
    vk::StridedDeviceAddressRegionKHR   rgen_region{};
    vk::StridedDeviceAddressRegionKHR   miss_region{};
    vk::StridedDeviceAddressRegionKHR   hit_region{};
    vk::StridedDeviceAddressRegionKHR   callable_region{};
};

// Manages all RT state for a device.
class RayTracingEngine {
public:
    explicit RayTracingEngine(const GraphicContext& ctx);
    ~RayTracingEngine();

    KYTY_CLASS_NO_COPY(RayTracingEngine);

    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    // Build a bottom-level acceleration structure from vertex/index data.
    // Returns a unique build ID that can be used to reference the BLAS.
    uint64_t BuildBlas(std::span<const float>   vertices,
                       std::span<const uint32_t> indices,
                       vk::CommandBuffer         cmd);

    // Build a top-level acceleration structure from instance data.
    uint64_t BuildTlas(std::span<const vk::AccelerationStructureInstanceKHR> instances,
                       vk::CommandBuffer                                     cmd);

    // Create a ray tracing pipeline from shader modules and shader groups.
    // Returns false if the pipeline could not be created.
    bool CreateRayTracingPipeline(std::span<const vk::PipelineShaderStageCreateInfo> stages,
                                  std::span<const RtShaderGroup>                     groups,
                                  uint32_t                                           max_recursion_depth,
                                  vk::PipelineLayout                                 layout,
                                  RtPipeline&                                        out_pipeline);

    // Destroy a previously-created RT pipeline.
    void DestroyPipeline(RtPipeline& pipeline);

    // Get the SBT buffer for a given pipeline. The SBT is laid out as:
    //   [raygen][miss][hit][callable]
    vk::Buffer GetSbtBuffer(const RtPipeline& pipeline) const;

private:
    struct BlasEntry {
        std::unique_ptr<RtAccelerationStructure> as;
        uint64_t                                 build_id = 0;
    };

    struct TlasEntry {
        std::unique_ptr<RtAccelerationStructure> as;
        uint64_t                                 build_id = 0;
    };

    bool CreateAccelerationBuffer(vk::AccelerationStructureBuildGeometryInfoKHR& build_info,
                                  vk::AccelerationStructureBuildSizesInfoKHR&   size_info,
                                  vk::AccelerationStructureTypeKHR              type,
                                  std::unique_ptr<RtAccelerationStructure>&    out_as);

    const GraphicContext& m_ctx;
    bool                  m_enabled = false;

    std::vector<BlasEntry> m_blas_entries;
    std::vector<TlasEntry> m_tlas_entries;
    uint64_t               m_next_build_id = 1;

    // Scratch buffer reused across builds.
    std::unique_ptr<Graphics::VulkanBuffer> m_scratch;
    uint64_t                                m_scratch_size = 0;
};

} // namespace Libs::Graphics

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RT_HARDWARE_H_ */
