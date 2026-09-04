#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <span>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "kyty_expected.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX // keep std::min/std::max usable (windows.h otherwise defines min/max macros)
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace KytyPS5::IO {

enum class DecompressionError {
	InvalidHeader,
	BufferOverflow,
	CorruptData,
	LibraryNotLoaded,
	UnsupportedCompressionType,
	WrongOutputSize,
};

/**
 * @brief Interface for hardware-accelerated or software-based decompression.
 */
class IDecompressor {
public:
	virtual ~IDecompressor()                                       = default;
	virtual std::expected<std::vector<uint8_t>, DecompressionError> Decompress(std::span<const uint8_t> compressed_data,
	                                                                     size_t original_size) = 0;
	virtual std::string GetAlgorithmName() const                    = 0;
};

/**
 * @brief zlib/deflate decompressor (stock PFSC codec).
 *
 * PFSC blocks may be stored with the zlib wrapper or as raw deflate depending
 * on the tool that produced the image, so both are attempted. Strict: the
 * stream must decode to exactly original_size bytes.
 * Implemented in package/Decompressor.cpp (where zlib is linked).
 */
class ZlibDecompressor : public IDecompressor {
public:
	std::expected<std::vector<uint8_t>, DecompressionError> Decompress(std::span<const uint8_t> compressed_data,
	                                                                   size_t original_size) override;
	std::string GetAlgorithmName() const override { return "zlib/deflate"; }

private:
	// Inflates `src` into `dst`. `raw` = raw deflate stream, otherwise zlib wrapper.
	bool TryInflate(std::span<const uint8_t> src, std::vector<uint8_t>& dst, bool raw);
};

/**
 * @brief Oodle (Kraken et al.) decompressor bridge.
 *
 * Loads a user-supplied proprietary Oodle core at runtime (never redistributed
 * by this project). Search order: $KYTY_OODLE_LIB override, then common names
 * next to the executable.
 *
 * NOTE on ABI: the documented export is
 *   s64 OodleLZ_Decompress(const void* compBuf, s64 compBufSize, void* rawBuf, s64 rawLen,
 *                          OodleLZ_FuzzSafe fuzzSafe, OodleLZ_CheckCRC checkCRC,
 *                          OodleLZ_Verbosity verbosity, void* decoderBase,
 *                          void* decoderMem, s64 decoderMemSize,
 *                          OodleLZ_DecodeBufferSizeLevel decodeBufferSizeLevel,
 *                          s32 tpThreadPhase1, s32 tpThreadPhase2, s32 tpThreadDecode);
 * We declare an s64 return; if a given core actually returns s32 the value
 * still arrives zero-extended in the low half of RAX, and the range check in
 * Decompress() rejects out-of-range results. FuzzSafe=Yes, CheckCRC=No,
 * Verbosity=None, no decoder memory, single-threaded.
 */
class OodleDecompressor : public IDecompressor {
public:
	OodleDecompressor() {
		LoadOodleCore();
	}

	~OodleDecompressor() {
#ifdef _WIN32
		if (library_handle_ != nullptr) {
			FreeLibrary(library_handle_);
			library_handle_ = nullptr;
		}
#else
		if (library_handle_ != nullptr) {
			dlclose(library_handle_);
			library_handle_ = nullptr;
		}
#endif
		library_loaded_ = false;
		decompress_fn_  = nullptr;
	}

	bool IsLoaded() const {
		return library_loaded_ && decompress_fn_ != nullptr;
	}

	std::expected<std::vector<uint8_t>, DecompressionError> Decompress(std::span<const uint8_t> compressed_data,
	                                                                   size_t original_size) override {
		if (!IsLoaded()) {
			return std::unexpected(DecompressionError::LibraryNotLoaded);
		}
		if (compressed_data.empty() || original_size == 0) {
			return std::unexpected(DecompressionError::InvalidHeader);
		}

		std::vector<uint8_t> output(original_size);

		const int64_t result =
		    decompress_fn_(compressed_data.data(), static_cast<int64_t>(compressed_data.size()),
		                   output.data(), static_cast<int64_t>(original_size),
		                   1 /* FuzzSafe_Yes */, 0 /* CheckCRC_No */, 0 /* Verbosity_None */, nullptr, nullptr, 0,
		                   0 /* BufferSizeLevel_Unspecified */, 0, 0, 0 /* single-threaded */);

		if (result < 0 || static_cast<uint64_t>(result) > original_size) {
			return std::unexpected(DecompressionError::CorruptData);
		}
		if (static_cast<size_t>(result) != original_size) {
			// PFSC blocks must decode to exactly the logical block size.
			return std::unexpected(DecompressionError::WrongOutputSize);
		}
		return output;
	}

	std::string GetAlgorithmName() const override { return "Oodle/Kraken"; }

private:
	// NOTE: must not be named LoadLibrary - windows.h expands that identifier to
	// LoadLibraryA, which would shadow the real ::LoadLibraryA inside this class.
	void LoadOodleCore() {
		// User-provided explicit path wins.
		std::vector<std::string> names;
		if (const char* env = std::getenv("KYTY_OODLE_LIB"); env != nullptr && env[0] != '\0') {
			names.emplace_back(env);
		}

#ifdef _WIN32
		const char* defaults[] = {"oo2core_9_win64.dll", "oo2core_8_win64.dll", "oo2core_7_win64.dll",
		                          "oo2core_6_win64.dll", "oo2core_5_win64.dll"};
#else
		const char* defaults[] = {"liboo2core_9_linux64.so", "liboo2ex_9_linux64.so",
		                          "liboo2core_8_linux64.so", "liboo2core_7_linux64.so",
		                          "liboo2core_9.so",         "oo2core_9.dylib", "oo2core_8.dylib"};
#endif
		for (const char* n: defaults) {
			names.emplace_back(n);
		}

		for (const auto& name: names) {
#ifdef _WIN32
			library_handle_ = LoadLibraryA(name.c_str());
#else
			library_handle_ = dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
			if (library_handle_ == nullptr) {
				continue;
			}

			decompress_fn_ = reinterpret_cast<OodleDecompressFunc>(
#ifdef _WIN32
			    GetProcAddress(library_handle_, "OodleLZ_Decompress"));
#else
			    dlsym(library_handle_, "OodleLZ_Decompress"));
#endif
			if (decompress_fn_ != nullptr) {
				library_loaded_ = true;
				return;
			}

#ifdef _WIN32
			FreeLibrary(library_handle_);
#else
			dlclose(library_handle_);
#endif
			library_handle_ = nullptr;
		}

		library_loaded_ = false;
	}

	// s64 return + enum params before decoder-mem params, threads last (see class comment).
	using OodleDecompressFunc = int64_t (*)(const void* pSrc, int64_t srcSize, void* pDst, int64_t dstSize,
	                                        int32_t fuzzSafe, int32_t checkCRC, int32_t verbosity, void* decoderBase,
	                                        void* decoderMem, int64_t decoderMemSize, int32_t bufferLevel,
	                                        int32_t tpPhase1, int32_t tpPhase2, int32_t tpDecode);

	bool library_loaded_ = false;
#ifdef _WIN32
	HMODULE library_handle_ = nullptr;
#else
	void* library_handle_ = nullptr;
#endif
	OodleDecompressFunc decompress_fn_ = nullptr;
};

/**
 * @brief Registry mapping PFSC algorithm ids to decompressor implementations.
 */
class DecompressionProvider {
public:
	static DecompressionProvider& Instance() {
		static DecompressionProvider instance;
		return instance;
	}

	std::expected<std::vector<uint8_t>, DecompressionError> ProcessAsset(uint32_t algo_id, std::span<const uint8_t> data,
	                                                                 size_t original_size) {
		std::shared_lock lock(mutex_);
		auto             it = decompressors_.find(algo_id);
		if (it == decompressors_.end()) {
			return std::unexpected(DecompressionError::UnsupportedCompressionType);
		}
		return it->second->Decompress(data, original_size);
	}

	bool Has(uint32_t algo_id) const {
		std::shared_lock lock(mutex_);
		return decompressors_.find(algo_id) != decompressors_.end();
	}

	void RegisterDecompressor(uint32_t algo_id, std::unique_ptr<IDecompressor> impl) {
		std::unique_lock lock(mutex_);
		decompressors_[algo_id] = std::move(impl);
	}

private:
	DecompressionProvider() = default;
	mutable std::shared_mutex mutex_;
	std::unordered_map<uint32_t, std::unique_ptr<IDecompressor>> decompressors_;
};

// Registers algo 0 (zlib, if compiled in) and algo 1 (Oodle, if a core loads).
// Implemented out-of-line in package/Decompressor.cpp so that targets which
// merely include this header don't need zlib or emit duplicate registrations.
void RegisterBuiltinDecompressors();

} // namespace KytyPS5::IO