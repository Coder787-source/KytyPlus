#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

// Minimal decompressor interface used by AstroCompatLayer scaffolding.
class IODecompressor {
public:
	virtual ~IODecompressor() = default;
	virtual bool DecompressSync(uint64_t /*address*/, size_t /*size*/) { return false; }
	virtual bool DecompressAsync(uint64_t /*address*/, size_t /*size*/) { return false; }
};
