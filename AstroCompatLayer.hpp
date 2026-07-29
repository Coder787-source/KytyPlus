#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include "IODecompressor.hpp"

/**
 * @brief AstroCompatLayer handles specific quirks for Astro's Playroom.
 * Focuses on I/O synchronization and memory alignment for the Kraken decompressor.
 */
class AstroCompatLayer {
public:
    struct CompatConfig {
        bool forceSycnhronousIO = true;
        uint64_t iommuOffsetAdjustment = 0x1000;
        bool bypassHapticSanityCheck = false;
    };

    explicit AstroCompatLayer(std::shared_ptr<IODecompressor> decompressor)
        : m_decompressor(std::move(decompressor)), m_config{} {}

    // Patches the memory stream for Astro's specific asset layout
    void ApplyAssetPatches(uint8_t* buffer, size_t size) {
        // Implementation of specific byte-patches for Astro's Playroom 
        // to prevent decompression failure on certain host CPU architectures
    }

    // Wraps decompression to ensure the game doesn't hang waiting for the GPU
    bool DecompressAssetSafe(uint64_t address, size_t size) {
        if (m_config.forceSycnhronousIO) {
            return m_decompressor->DecompressSync(address, size);
        }
        return m_decompressor->DecompressAsync(address, size);
    }

    void SetConfig(const CompatConfig& config) { m_config = config; }

private:
    std::shared_ptr<IODecompressor> m_decompressor;
    CompatConfig m_config;
};
