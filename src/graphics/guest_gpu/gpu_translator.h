#ifndef KYTY_GPU_TRANSLATOR_H
#define KYTY_GPU_TRANSLATOR_H

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <iostream>

namespace Emulator {

/**
 * @brief Represents a single command in the PS5 GPU command stream.
 */
struct GPUCommand {
    uint32_t opcode;
    std::vector<uint8_t> payload;
};

/**
 * @brief Abstract base class for a Host Graphics API backend (e.g., Vulkan, DX12).
 */
class IHostGraphicsBackend {
public:
    virtual ~IHostGraphicsBackend() = default;
    virtual void Initialize() = 0;
    virtual void SubmitCommandBuffer(const std::vector<GPUCommand>& commands) = 0;
    virtual void CreateBuffer(uint64_t guest_addr, size_t size) = 0;
    virtual void MapShader(const std::vector<uint8_t>& guest_shader, const std::string& target_api) = 0;
};

/**
 * @brief GPUTranslator translates guest GPU packets into host API calls.
 */
class NullHostGraphicsBackend : public IHostGraphicsBackend {
public:
    void Initialize() override {}
    void SubmitCommandBuffer(const std::vector<GPUCommand>&) override {}
    void CreateBuffer(uint64_t, size_t) override {}
    void MapShader(const std::vector<uint8_t>&, const std::string&) override {}
};

class GPUTranslator {
public:
    GPUTranslator() : GPUTranslator(std::make_unique<NullHostGraphicsBackend>()) {}
    explicit GPUTranslator(std::unique_ptr<IHostGraphicsBackend> backend);
    ~GPUTranslator() = default;

    /**
     * @brief Parses a block of guest memory and translates it into GPU commands.
     */
    void ProcessCommandStream(uint64_t guest_addr, size_t length);

    /**
     * @brief Translates PS5 PSSL shaders to SPIR-V/DXIL.
     */
    void TranslateShader(const std::vector<uint8_t>& ps5_shader);

private:
    std::unique_ptr<IHostGraphicsBackend> backend_;
    std::unordered_map<uint64_t, size_t> gpu_memory_map_;

    void DecodePacket(const uint8_t* data, size_t size);
};

} // namespace Emulator

#endif // KYTY_GPU_TRANSLATOR_H
