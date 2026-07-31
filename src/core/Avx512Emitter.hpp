#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "kyty_expected.hpp"
#include <immintrin.h>

namespace KytyPS5::JIT {

    enum class JitError {
        EmissionFailed,
        InvalidOpcode,
        AlignmentError
    };

    class Avx512Emitter {
    public:
        Avx512Emitter() = default;

        // Emits VADDPS (Vector Add Packed Single Precision)
        std::expected<void, JitError> EmitVAddPs(uint8_t zmm_dest, uint8_t zmm_src1, uint8_t zmm_src2) {
            // EVEX prefix encoding for AVX-512
            // Format: [Prefix] [Opcode] [ModRM]
            std::vector<uint8_t> bytes = {
                0x62, 
                static_cast<uint8_t>(0x00 | (zmm_dest << 3)), // EVEX prefix (simplified)
                0xC0, 
                static_cast<uint8_t>(0x10 | (zmm_src1 << 3))  // Opcode + ModRM
            };
            return AppendBytes(bytes);
        }

        // Emits VMOVAPS (Vector Move Aligned Packed Single Precision)
        std::expected<void, JitError> EmitVMoveAps(uint8_t zmm_dest, uint8_t zmm_src) {
            std::vector<uint8_t> bytes = {
                0x62, 
                static_cast<uint8_t>(0x00 | (zmm_dest << 3)),
                0x28, 
                static_cast<uint8_t>(0x00 | (zmm_src << 3))
            };
            return AppendBytes(bytes);
        }

        // Emits SFENCE (Store Fence) for memory consistency
        std::expected<void, JitError> EmitStoreFence() {
            // SFENCE = 0x0F 0xAE 0xF8
            return AppendBytes({ 0x0F, 0xAE, 0xF8 });
        }

        // Emits LFENCE (Load Fence)
        std::expected<void, JitError> EmitLoadFence() {
            // LFENCE = 0x0F 0xAE 0xE8
            return AppendBytes({ 0x0F, 0xAE, 0xE8 });
        }

        // Emits MFENCE (Memory Fence)
        std::expected<void, JitError> EmitMemFence() {
            // MFENCE = 0x0F 0xAE 0xF0
            return AppendBytes({ 0x0F, 0xAE, 0xF0 });
        }

        // Raw emission callback for JitDispatcher — appends raw bytes to an external buffer
        void EmitRaw(uint32_t opcode, uint8_t zmm_reg, std::vector<uint8_t>& out_buffer) {
            // Simplified: emit a placeholder EVEX-encoded NOP-like instruction
            // Real implementation would decode opcode and emit proper EVEV-encoded instruction
            out_buffer.push_back(0x62);
            out_buffer.push_back(static_cast<uint8_t>(0xF0 | (zmm_reg & 0x0F)));
            out_buffer.push_back(0x00);
            out_buffer.push_back(0x00);
        }

    private:
        std::expected<void, JitError> AppendBytes(const std::vector<uint8_t>& bytes) {
            code_buffer_.insert(code_buffer_.end(), bytes.begin(), bytes.end());
            return {};
        }

        std::vector<uint8_t> code_buffer_;
    };

}
