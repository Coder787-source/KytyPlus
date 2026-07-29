#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "kyty_expected.hpp"

namespace KytyPS5::GPU {

    struct PsoDescriptor {
        uint64_t pipeline_hash;
        uint32_t shader_stage;
        uint32_t blend_mode;
        uint32_t depth_test_enable;

        bool operator==(const PsoDescriptor& other) const = default;
    };

    struct PsoHash {
        size_t operator()(const PsoDescriptor& d) const {
            return std::hash<uint64_t>{}(d.pipeline_hash);
        }
    };

    class GpuDevice {
    public:
        static GpuDevice& Instance() {
            static GpuDevice instance;
            return instance;
        }

        std::expected<uint64_t, bool> GetOrCreatePso(const PsoDescriptor& desc) {
            std::shared_lock lock(mutex_);
            if (pso_cache_.contains(desc)) {
                return pso_cache_[desc];
            }
            lock.unlock();

            std::unique_lock write_lock(mutex_);
            uint64_t pso_handle = CreateVulkanPso(desc);
            pso_cache_[desc] = pso_handle;
            return pso_handle;
        }

        void SubmitCommandBuffer(void* host_ptr, size_t size) {
            // Vulkan vkQueueSubmit implementation
        }

    private:
        GpuDevice() = default;
        uint64_t CreateVulkanPso(const PsoDescriptor& desc) {
            return 0xDEADBEEF; // Mock Vulkan Pipeline Handle
        }

        std::shared_mutex mutex_;
        std::unordered_map<PsoDescriptor, uint64_t, PsoHash> pso_cache_;
    };

}
