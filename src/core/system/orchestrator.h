#pragma once
#include <memory>
#include "kyty_expected.hpp"
#include <vector>
#include <span]
#include <filesystem>
#include <iostream>
#include <variant>
#include <unordered_map>
#include <string>
#include <concepts>

namespace kyty {

enum class SystemError {
    ImageLoadFailure,
    MemoryMapFailure,
    CpuExecutionError,
    GpuInitializationError,
    DecompressionFailure
};

template<typename T>
using Result = std::expected<T, SystemError>;

// ============================================================================
// DECOMPRESSION ENGINE
// ============================================================================

class DecompressionEngine {
public:
    static Result<std::vector<std::byte>> DecompressKraken(std::span<const std::byte> compressed_data) {
        if (compressed_data.empty()) return std::unexpected(SystemError::DecompressionFailure);
        
        // Integration point for actual Kraken/Oodle SDK
        std::vector<std::byte> decompressed(compressed_data.size() * 2); 
        return decompressed;
    }
};

// ============================================================================
// GPU PIPELINE
// ============================================================================

struct PsoDescriptor {
    std::string vs_id;
    std::string fs_id;
    uint32_t blend_state;
    uint32_t depth_state;

    bool operator==(const PsoDescriptor&) const = default;
};

struct PsoHash {
    size_t operator()(const PsoDescriptor& d) const {
        return std::hash<std::string>{}(d.vs_id) ^ std::hash<std::string>{}(d.fs_id) ^ d.blend_state;
    }
};

class GpuDevice {
public:
    void Init() { /* Vulkan Instance/Device Setup */ }
};

class GpuTranslator {
public:
    explicit GpuTranslator(GpuDevice& device) : device_(device) {}

    void BindPipeline(const PsoDescriptor& desc) {
        if (!pso_cache_.contains(desc)) {
            pso_cache_[desc] = CreateVulkanPipeline(desc);
        }
        active_pipeline_ = pso_cache_[desc];
    }

private:
    uint64_t CreateVulkanPipeline(const PsoDescriptor& desc) {
        return 0x12345678; 
    }

    GpuDevice& device_;
    uint64_t active_pipeline_ = 0;
    std::unordered_map<PsoDescriptor, uint64_t, PsoHash> pso_cache_;
};

// ============================================================================
// CPU JIT TRANSLATOR
// ============================================================================

struct MovRegReg { uint8_t dst, src; };
struct AddRegImm { uint8_t dst; int32_t imm; };
struct JumpRel   { int32_t offset; };

using Instruction = std::variant<MovRegReg, AddRegImm, JumpRel>;

class JitTranslator {
public:
    void Translate(const Instruction& inst) {
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, MovRegReg>) EmitMov(arg.dst, arg.src);
            else if constexpr (std::is_same_v<T, AddRegImm>) EmitAddImm(arg.dst, arg.imm);
            else if constexpr (std::is_same_v<T, JumpRel>) EmitJmp(arg.offset);
        }, inst);
    }

    std::span<const uint8_t> GetHostCode() const { return host_code_; }

private:
    void EmitMov(uint8_t dst, uint8_t src) {
        host_code_.push_back(0x48); 
        host_code_.push_back(0x89);
        host_code_.push_back(static_cast<uint8_t>(src | 0xD0));
    }

    void EmitAddImm(uint8_t dst, int32_t imm) {
        host_code_.push_back(0x48);
        host_code_.push_back(0x05);
        for (int i = 0; i < 4; ++i) host_code_.push_back((imm >> (i * 8)) & 0xFF);
    }

    void EmitJmp(int32_t offset) {
        host_code_.push_back(0xE9);
        for (int i = 0; i < 4; ++i) host_code_.push_back((offset >> (i * 8)) & 0xFF);
    }

    std::vector<uint8_t> host_code_;
};

// ============================================================================
// IMAGE PROVIDER & LOADERS
// ============================================================================

class ImageProvider {
public:
    virtual ~ImageProvider() = default;
    virtual Result<std::vector<std::byte>> LoadImage(const std::filesystem::path& path) = 0;
};

class ElfLoader : public ImageProvider {
public:
    Result<std::vector<std::byte>> LoadImage(const std::filesystem::path& path) override {
        return std::vector<std::byte>{}; 
    }
};

class PkgLoader : public ImageProvider {
public:
    Result<std::vector<std::byte>> LoadImage(const std::filesystem::path& path) override {
        return std::vector<std::byte>{};
    }
};

// ============================================================================
// SYSTEM ORCHESTRATOR
// ============================================================================

class SystemOrchestrator {
public:
    SystemOrchestrator(
        std::unique_ptr<ImageProvider> provider,
        std::unique_ptr<JitTranslator> jit,
        std::unique_ptr<GpuDevice> gpu
    ) : provider_(std::move(provider)), jit_(std::move(jit)), gpu_(std::move(gpu)) {}

    Result<void> Boot(const std::filesystem::path& game_path) {
        gpu_->Init();
        
        auto image = provider_->LoadImage(game_path);
        if (!image) return std::unexpected(image.error());

        Instruction entry_inst = MovRegReg{0, 1}; 
        jit_->Translate(entry_inst);

        return {};
    }

private:
    std::unique_ptr<ImageProvider> provider_;
    std::unique_ptr<JitTranslator> jit_;
    std::unique_ptr<GpuDevice> gpu_;
};

} // namespace kyty
