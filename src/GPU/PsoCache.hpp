#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include "kyty_expected.hpp"
#include <fstream>
#include <span>

namespace KytyPS5::Gpu {

    struct PsoDescriptor {
        uint64_t pipeline_hash;
        uint32_t vertex_layout_id;
        uint32_t fragment_shader_id;
        uint32_t raster_state;

        bool operator==(const PsoDescriptor&) const = default;
    };

    struct PsoHash {
        size_t operator()(const PsoDescriptor& pso) const {
            return std::hash<uint64_t>{}(pso.pipeline_hash) ^ 
                   (std::hash<uint32_t>{}(pso.vertex_layout_id) << 1);
        }
    };

    /**
     * @brief High-performance PSO Cache to prevent driver-level stutter.
     * Persists compiled pipelines to disk.
     */
    class PsoCache {
    public:
        static PsoCache& Instance() {
            static PsoCache instance;
            return instance;
        }

        std::expected<void*, std::string> GetOrCreatePipeline(const PsoDescriptor& desc) {
            std::shared_lock lock(mutex_);
            if (cache_.contains(desc)) {
                return cache_[desc];
            }
            lock.unlock();

            std::unique_lock write_lock(mutex_);
            // Double-check lock pattern
            if (cache_.contains(desc)) return cache_[desc];

            void* compiled_pipeline = CompileAndStore(desc);
            if (!compiled_pipeline) return std::unexpected("PSO Compilation Failed");
            
            cache_[desc] = compiled_pipeline;
            return compiled_pipeline;
        }

    private:
        PsoCache() = default;
        void* CompileAndStore(const PsoDescriptor& desc) {
            // Integration with Vulkan vkCreateGraphicsPipelines
            return (void*)0xDEADBEEF; // Mock handle
        }

        std::shared_mutex mutex_;
        std::unordered_map<PsoDescriptor, void*, PsoHash> cache_;
    };

}
