#pragma once

#include <variant>
#include <vector>
#include <cstdint>
#include <memory>
#include <span>
#include "Core\Avx512Emitter.hpp"

namespace KytyPS5::Core {

    /**
     * @brief ARM64 Guest State. Aligned to cache line.
     */
    struct alignas(64) ThreadContext {
        uint64_t gprs[32];      // X0-X31
        uint64_t sp;            // Stack Pointer
        uint64_t pc;            // Program Counter
        uint64_t nzcv;          // Condition flags
    };

    struct OpMov { uint8_t reg_dest; uint8_t reg_src; uint64_t offset = 0; };
    struct OpAdd { uint8_t reg_dest; uint8_t reg_src; uint64_t offset = 0; };
    struct OpLdr { uint8_t reg_dest; uint8_t base_reg; uint64_t offset = 0; };
    struct OpStr { uint8_t reg_src; uint8_t base_reg; uint64_t offset = 0; };
    struct OpJmp { uint64_t target_addr; };
    struct OpSyscall { uint32_t call_id; };
    struct OpAvx512 { uint32_t opcode; uint8_t zmm_reg; };

    using Instruction = std::variant<OpMov, OpAdd, OpLdr, OpStr, OpJmp, OpSyscall, OpAvx512>;

    class JitDispatcher {
    public:
        JitDispatcher() = default;

        std::vector<uint8_t> TranslateBlock(const std::vector<Instruction>& block) {
            code_buffer_.clear();
            for (const auto& instr : block) {
                Emit(instr);
            }
            return code_buffer_;
        }

        void Emit(const Instruction& instr) {
            std::visit([this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, OpMov>) this->EmitMov(arg);
                else if constexpr (std::is_same_v<T, OpAdd>) this->EmitAdd(arg);
                else if constexpr (std::is_same_v<T, OpLdr>) this->EmitLdr(arg);
                else if constexpr (std::is_same_v<T, OpStr>) this->EmitStr(arg);
                else if constexpr (std::is_same_v<T, OpJmp>) this->EmitJmp(arg);
                else if constexpr (std::is_same_v<T, OpSyscall>) this->EmitSyscall(arg);
                else if constexpr (std::is_same_v<T, OpAvx512>) this->EmitAvx512(arg);
            }, instr);
        }

    private:
        // X86-64 Hot Register Map (X0-X11)
        static constexpr uint8_t HOT_MAP[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 }; 
        static constexpr uint8_t CONTEXT_BASE_REG = 11; // R11 points to ThreadContext

        void PushByte(uint8_t b) { code_buffer_.push_back(b); }

        void PushModRM(uint8_t modrm_reg, uint8_t rm_reg, uint32_t offset = 0) {
            // Simplified ModRM: [mod:2][reg:3][rm:3]
            // Mod 00 = register, 01 = [reg + disp8], 10 = [reg + disp32]
            uint8_t modrm = 0x00;
            if (offset == 0) {
                modrm = (0x00 << 6) | ((modrm_reg & 7) << 3) | (rm_reg & 7);
            } else if (offset < 128) {
                modrm = (0x01 << 6) | ((modrm_reg & 7) << 3) | (rm_reg & 7);
                PushByte(static_cast<uint8_t>(offset));
            } else {
                modrm = (0x10 << 6) | ((modrm_reg & 7) << 3) | (rm_reg & 7);
                for(int i=0; i<4; ++i) PushByte((offset >> (i*8)) & 0xFF);
            }
            PushByte(modrm);
        }

        void EmitFillMove(uint8_t guest_reg, uint8_t target_hot_reg) {
            if (guest_reg < 12) return; // Already in hot reg
            uint32_t offset = guest_reg * 8;
            PushByte(0x48); PushByte(0x8B); // MOV RAX, [R11 + offset]
            PushModRM(target_hot_reg, CONTEXT_BASE_REG, offset);
        }

        void EmitSpillMove(uint8_t guest_reg, uint8_t source_hot_reg) {
            if (guest_reg < 12) return;
            uint32_t offset = guest_reg * 8;
            PushByte(0x48); PushByte(0x89); // MOV [R11 + offset], RAX
            PushModRM(CONTEXT_BASE_REG, source_hot_reg, offset);
        }

        void EmitMov(const OpMov& op) {
            uint8_t d = op.reg_dest < 12 ? HOT_MAP[op.reg_dest] : 10; // R10 as scratch
            uint8_t s = op.reg_src < 12 ? HOT_MAP[op.reg_src] : 10;
            
            if (op.reg_src >= 12) EmitFillMove(op.reg_src, 10);
            if (op.reg_dest >= 12) EmitFillMove(op.reg_dest, 10); // Logic simplified for sketch
            
            PushByte(0x48); PushByte(0x89);
            PushModRM(d, s);
            if (op.reg_dest >= 12) EmitSpillMove(op.reg_dest, d);
        }

        void EmitAdd(const OpAdd& op) {
            uint8_t d = op.reg_dest < 12 ? HOT_MAP[op.reg_dest] : 10;
            uint8_t s = op.reg_src < 12 ? HOT_MAP[op.reg_src] : 10;
            
            if (op.reg_src >= 12) EmitFillMove(op.reg_src, 10);
            
            PushByte(0x48); PushByte(0x01); // ADD
            PushModRM(d, s);
            if (op.reg_dest >= 12) EmitSpillMove(op.reg_dest, d);
        }

        void EmitLdr(const OpLdr& op) {
            uint8_t d = op.reg_dest < 12 ? HOT_MAP[op.reg_dest] : 10;
            uint8_t b = op.base_reg < 12 ? HOT_MAP[op.base_reg] : 10;

            if (op.base_reg >= 12) EmitFillMove(op.base_reg, 10);
            
            // To be game-ready, we must handle GVA -> HVA.
            // We emit a call to a runtime helper that resolves the address via VirtualMemoryManager.
            // 1. Move BaseReg to RAX
            // 2. Add Offset
            // 3. Call ResolveHelper(address)
            // 4. Move result to DestReg
            
            PushByte(0x48); PushByte(0x89); // MOV RAX, BaseReg
            PushModRM(0, b); 
            
            if (op.offset != 0) {
                PushByte(0x48); PushByte(0x05); // ADD RAX, imm32
                uint32_t off = static_cast<uint32_t>(op.offset);
                for(int i=0; i<4; ++i) PushByte((off >> (i*8)) & 0xFF);
            }

            // Call External Runtime Resolve
            PushByte(0xE8); // CALL <offset_to_resolve_helper>
            for(int i=0; i<4; ++i) PushByte(0x00); // Placeholder for linker

            PushByte(0x48); PushByte(0x89); // MOV DestReg, RAX
            PushModRM(d, 0);
            if (op.reg_dest >= 12) EmitSpillMove(op.reg_dest, d);
        }

        void EmitStr(const OpStr& op) {
            uint8_t s = op.reg_src < 12 ? HOT_MAP[op.reg_src] : 10;
            uint8_t b = op.base_reg < 12 ? HOT_MAP[op.base_reg] : 10;

            if (op.reg_src >= 12) EmitFillMove(op.reg_src, 10);
            if (op.base_reg >= 12) EmitFillMove(op.base_reg, 10);

            PushByte(0x48); PushByte(0x89); // MOV [base + off], src
            PushModRM(b, s, static_cast<uint32_t>(op.offset));
        }

        void EmitJmp(const OpJmp& op) {
            PushByte(0xE9);
            uint32_t rel = static_cast<uint32_t>(op.target_addr); // Simplified
            for(int i=0; i<4; ++i) PushByte((rel >> (i*8)) & 0xFF);
        }

        void EmitSyscall(const OpSyscall& op) {
            PushByte(0x48); PushByte(0xB8); // MOV RAX, imm64
            uint64_t id = op.call_id;
            for(int i=0; i<8; ++i) PushByte((id >> (i*8)) & 0xFF);
            PushByte(0x0F); PushByte(0x05); // SYSCALL
        }

        void EmitAvx512(const OpAvx512& op) {
            avx_emitter_->EmitInstruction(op.opcode, op.zmm_reg);
            PushByte(0x62); PushByte(0x00); PushByte(0x00); PushByte(0x00);
        }

        std::vector<uint8_t> code_buffer_;
        std::unique_ptr<Avx512Emitter> avx_emitter_ = std::make_unique<Avx512Emitter>();
    };
}
