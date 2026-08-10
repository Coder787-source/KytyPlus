#pragma once

#include <vector>
#include <memory>
#include "kyty_expected.hpp"
#include <span>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace KytyPS5::IO {

    enum class DecompressionError {
        InvalidHeader,
        BufferOverflow,
        CorruptData,
        LibraryNotLoaded,
        UnsupportedCompressionType
    };

    /**
     * @brief Interface for hardware-accelerated or software-based decompression.
     */
    class IDecompressor {
    public:
        virtual ~IDecompressor() = default;
        virtual std::expected<std::vector<uint8_t>, DecompressionError> Decompress(std::span<const uint8_t> compressed_data, size_t original_size) = 0;
        virtual std::string GetAlgorithmName() const = 0;
    };

    /**
     * @brief Bridge for Oodle/Kraken implementation.
     * Wraps proprietary binaries via the Provider pattern.
     * 
     * Supports Oodle 2.7, 2.8, 2.9+ (oo2core_7/8/9_win64.dll)
     */
    class KrakenDecompressor : public IDecompressor {
    public:
        KrakenDecompressor() {
            LoadLibrary();
        }

        ~KrakenDecompressor() {
#ifdef _WIN32
            if (library_handle_) {
                FreeLibrary(library_handle_);
                library_handle_ = nullptr;
            }
#endif
        }

        std::expected<std::vector<uint8_t>, DecompressionError> Decompress(std::span<const uint8_t> compressed_data, size_t original_size) override {
            if (!library_loaded_ || !decompress_fn_) {
                return std::unexpected(DecompressionError::LibraryNotLoaded);
            }

            if (compressed_data.empty()) {
                return std::unexpected(DecompressionError::InvalidHeader);
            }

            std::vector<uint8_t> output(original_size);
            
            // Call OodleLZ_Decompress with single-threaded parameters (safe default)
            // Signature: OodleLZ_Decompress(pSrc, srcSize, pDst, dstSize, 
            //                               nThreadPhase1, nThreadPhase2, nThreadDecode,
            //                               pUnused, pDecoderAllocators, pScratch, scratchSize)
            size_t result = decompress_fn_(
                compressed_data.data(), compressed_data.size(),
                output.data(), original_size,
                0, 0, 0,  // Single-threaded decompression
                nullptr, nullptr, nullptr, 0
            );
            
            if (result == 0 || result > original_size) {
                // Decompression failed - data may be corrupt or not Kraken-compressed
                return std::unexpected(DecompressionError::CorruptData);
            }

            // Resize output to actual decompressed size if smaller than expected
            if (result < original_size) {
                output.resize(result);
            }

            return output;
        }

        std::string GetAlgorithmName() const override { return "Kraken/Oodle"; }

    private:
        void LoadLibrary() {
#ifdef _WIN32
            // Try to load Oodle Kraken DLL
            // Look for common Oodle versions used in PS5 games
            const char* dll_names[] = {
                "oo2core_9_win64.dll",  // Oodle 2.9+ (most common for PS5 games)
                "oo2core_8_win64.dll",  // Oodle 2.8
                "oo2core_7_win64.dll",  // Oodle 2.7
                "oo2core_6_win64.dll",  // Oodle 2.6 (fallback)
                nullptr
            };
            
            for (int i = 0; dll_names[i] != nullptr; i++) {
                library_handle_ = LoadLibraryA(dll_names[i]);
                if (library_handle_) {
                    // Try to get the decompression function
                    // OodleLZ_Decompress is the standard export name across all versions
                    decompress_fn_ = reinterpret_cast<OodleDecompressFunc>(
                        GetProcAddress(library_handle_, "OodleLZ_Decompress")
                    );
                    
                    if (decompress_fn_) {
                        library_loaded_ = true;
                        return;
                    }
                    
                    // Function not found, free library and try next DLL
                    FreeLibrary(library_handle_);
                    library_handle_ = nullptr;
                }
            }
            
            // If we get here, no Oodle DLL was found or function export missing
            library_loaded_ = false;
            library_handle_ = nullptr;
            decompress_fn_ = nullptr;
#else
            // Linux/macOS stub - would need .so loading implementation
            library_loaded_ = false;
            library_handle_ = nullptr;
            decompress_fn_ = nullptr;
#endif
        }

        // OodleLZ_Decompress function pointer type
        // Returns: size_t (actual decompressed size, or 0 on error)
        using OodleDecompressFunc = size_t (*)(
            const uint8_t* pSrc, size_t srcSize,
            uint8_t* pDst, size_t dstSize,
            int nThreadPhase1, int nThreadPhase2, int nThreadDecode,
            void* pUnused, void* pDecoderAllocators, void* pScratch, size_t scratchSize
        );

        bool library_loaded_ = false;
#ifdef _WIN32
        HMODULE library_handle_ = nullptr;
#endif
        OodleDecompressFunc decompress_fn_ = nullptr;
    };

    /**
     * @brief Factory to handle different compression schemes found in PKG assets.
     */
    class DecompressionProvider {
    public:
        static DecompressionProvider& Instance() {
            static DecompressionProvider instance;
            return instance;
        }

        std::expected<std::vector<uint8_t>, DecompressionError> ProcessAsset(uint32_t algo_id, std::span<const uint8_t> data, size_t original_size) {
            std::shared_lock lock(mutex_);
            if (!decompressors_.contains(algo_id)) {
                return std::unexpected(DecompressionError::UnsupportedCompressionType);
            }
            return decompressors_[algo_id]->Decompress(data, original_size);
        }

        void RegisterDecompressor(uint32_t algo_id, std::unique_ptr<IDecompressor> impl) {
            std::unique_lock lock(mutex_);
            decompressors_[algo_id] = std::move(impl);
        }

    private:
        DecompressionProvider() = default;
        std::shared_mutex mutex_;
        std::unordered_map<uint32_t, std::unique_ptr<IDecompressor>> decompressors_;
    };

}
