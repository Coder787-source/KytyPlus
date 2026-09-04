// DecompressionProvider backend registration + zlib implementation.
//
// The stock PFSC codec is zlib/deflate. Oodle/Kraken-family codecs are served
// by user-supplied proprietary cores loaded at runtime (never redistributed).
// This file contains no proprietary code and no decryption keys.

#include "IO/Decompressor.hpp"

#include "common/logging/log.h"

#ifdef KYTY_HAS_ZLIB
#include <zlib.h>
#endif

namespace KytyPS5::IO {

#ifdef KYTY_HAS_ZLIB

std::expected<std::vector<uint8_t>, DecompressionError>
ZlibDecompressor::Decompress(std::span<const uint8_t> compressed_data, size_t original_size) {
	if (compressed_data.empty() || original_size == 0) {
		return std::unexpected(DecompressionError::InvalidHeader);
	}

	std::vector<uint8_t> output(original_size);

	// PFSC streams are produced either with the zlib wrapper or as raw
	// deflate depending on the tool that wrote the image; try both.
	if (TryInflate(compressed_data, output, false) || TryInflate(compressed_data, output, true)) {
		return output;
	}
	return std::unexpected(DecompressionError::CorruptData);
}

bool ZlibDecompressor::TryInflate(std::span<const uint8_t> src, std::vector<uint8_t>& dst, bool raw) {
	z_stream zs {};
	zs.next_in   = const_cast<Bytef*>(src.data());
	zs.avail_in  = static_cast<uInt>(src.size());
	zs.next_out  = dst.data();
	zs.avail_out = static_cast<uInt>(dst.size());

	// 15 = zlib wrapper, -15 = raw deflate.
	if (inflateInit2(&zs, raw ? -15 : 15) != Z_OK) {
		return false;
	}

	const int  rc = inflate(&zs, Z_FINISH);
	const bool ok = (rc == Z_STREAM_END) && zs.avail_out == 0 && zs.total_in > 0;
	inflateEnd(&zs);
	return ok;
}

#else

// zlib not linked in this build: report Unsupported so callers can log loudly
// instead of silently handing back raw compressed bytes.
std::expected<std::vector<uint8_t>, DecompressionError>
ZlibDecompressor::Decompress(std::span<const uint8_t> /*compressed_data*/, size_t /*original_size*/) {
	return std::unexpected(DecompressionError::UnsupportedCompressionType);
}

bool ZlibDecompressor::TryInflate(std::span<const uint8_t>, std::vector<uint8_t>&, bool) {
	return false;
}

#endif // KYTY_HAS_ZLIB

void RegisterBuiltinDecompressors() {
	auto& provider = DecompressionProvider::Instance();

#ifdef KYTY_HAS_ZLIB
	provider.RegisterDecompressor(0, std::make_unique<ZlibDecompressor>());
	LOGF("Decompressor: registered algo 0 (zlib/deflate)");
#else
	LOGF("Decompressor: zlib not linked - algo 0 unavailable (build with ZLIB_FOUND)");
#endif

	// algo 1: user-supplied Oodle core (Kraken et al.)
	auto oodle = std::make_unique<OodleDecompressor>();
	if (oodle->IsLoaded()) {
		LOGF("Decompressor: registered algo 1 (Oodle/Kraken, user-supplied core)");
		provider.RegisterDecompressor(1, std::move(oodle));
	} else {
		LOGF("Decompressor: Oodle core not found (set KYTY_OODLE_LIB or place oo2core_*.dll/so "
		     "next to the emulator) - Kraken-family codecs unavailable");
	}
}

} // namespace KytyPS5::IO