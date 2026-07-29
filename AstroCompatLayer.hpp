#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

#include "IODecompressor.hpp"

/**
 * Scaffolding-only AstroCompatLayer. Not wired into the real Kyty HLE path.
 * Provides default construction so headers that include it can parse cleanly.
 */
class AstroCompatLayer {
public:
	struct CompatConfig {
		bool forceSycnhronousIO = true;
		uint64_t iommuOffsetAdjustment = 0x1000;
		bool bypassHapticSanityCheck = false;
	};

	AstroCompatLayer() : m_decompressor(std::make_shared<IODecompressor>()), m_config{} {}

	explicit AstroCompatLayer(std::shared_ptr<IODecompressor> decompressor)
	    : m_decompressor(std::move(decompressor)), m_config{} {
		if (!m_decompressor) {
			m_decompressor = std::make_shared<IODecompressor>();
		}
	}

	void ApplyAssetPatches(uint8_t* /*buffer*/, size_t /*size*/) {}

	bool DecompressAssetSafe(uint64_t address, size_t size) {
		if (m_config.forceSycnhronousIO) {
			return m_decompressor->DecompressSync(address, size);
		}
		return m_decompressor->DecompressAsync(address, size);
	}

	void SetConfig(const CompatConfig& config) { m_config = config; }

	int HandleIdc(uint32_t /*id*/, void* /*buf*/, size_t /*len*/) { return 0; }

private:
	std::shared_ptr<IODecompressor> m_decompressor;
	CompatConfig m_config;
};
