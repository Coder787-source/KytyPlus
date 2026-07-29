#pragma once

#include <vector>
#include <memory>
#include "kyty_expected.hpp"
#include <span>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

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
     */
    class KrakenDecompressor : public IDecompressor {
    public:
        KrakenDecompressor() {
            LoadLibrary();
        }

        std::expected<std::vector<uint8_t>, DecompressionError> Decompress(std::span<const uint8_t> compressed_data, size_t original_size) override {
            if (!library_loaded_) return std::unexpected(DecompressionError::LibraryNotLoaded);

            std::vector<uint8_t> output(original_size);
            
            // Simulating Kraken call: 
            // int result = oodle_kraken_decompress(output.data(), original_size, compressed_data.data(), compressed_data.size());
            
            bool success = true; // Mock result from DLL
            if (!success) return std::unexpected(DecompressionError::CorruptData);

            return output;
        }

        std::string GetAlgorithmName() const override { return "Kraken/Oodle"; }

    private:
        void LoadLibrary() {
            // Logic to load oodle_kraken.dll / .so
            library_loaded_ = true;
        }

        bool library_loaded_ = false;
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
