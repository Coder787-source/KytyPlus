#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace KytyPS5::GPU {

/**
 * @brief PsoCacheManager
 * Manages Pipeline State Objects (PSO) to eliminate shader stutter and vertex stretching.
 */
struct PsoKey {
    uint64_t pipelineHash;
    uint32_t renderPassId;

    bool operator==(const PsoKey& other) const {
        return pipelineHash == other.pipelineHash && renderPassId == other.renderPassId;
    }
};

struct PsoKeyHash {
    std::size_t operator()(const PsoKey& k) const {
        return std::hash<uint64_t>{}(k.pipelineHash) ^ (std::hash<uint32_t>{}(k.renderPassId) << 1);
    }
};

class PsoCacheManager {
public:
    PsoCacheManager() = default;

    // Retrieves or creates a PSO for a specific draw call
    uint32_t GetOrCreatePso(const PsoKey& key) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        
        if (auto it = m_psoMap.find(key); it != m_psoMap.end()) {
            return it->second;
        }

        uint32_t newPso = GeneratePsoOnHost();
        m_psoMap[key] = newPso;
        return newPso;
    }

    void ClearCache() {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_psoMap.clear();
    }

private:
    uint32_t GeneratePsoOnHost() {
        // Logic to compile Vulkan Pipeline State based on PS5 descriptors
        static uint32_t psoCounter = 0;
        return ++psoCounter;
    }

    std::unordered_map<PsoKey, uint32_t, PsoKeyHash> m_psoMap;
    std::mutex m_cacheMutex;
};

} // namespace KytyPS5::GPU
