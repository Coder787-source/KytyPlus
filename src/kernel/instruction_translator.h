#pragma once
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <iostream>

namespace Emulator {

enum class OpCode {
    MOV_REG_REG,
    MOV_REG_IMM,
    ADD_REG_REG,
    SUB_REG_REG,
    PUSH_REG,
    POP_REG,
    CALL,
    RET,
    NOP,
    UNKNOWN
};

struct DecodedInstruction {
    OpCode op;
    uint8_t dest_reg;
    uint8_t src_reg;
    int64_t immediate;
    uint64_t original_address;
    size_t length;
};

class InstructionTranslator {
public:
    InstructionTranslator();
    ~InstructionTranslator() = default;

    // Translates a block of guest x86-64 code into host executable code
    std::vector<uint8_t> TranslateBlock(const std::vector<uint8_t>& guest_code, uint64_t start_address);

private:
    // Simple decoder to identify the opcode
    DecodedInstruction Decode(const uint8_t* code, size_t& offset, uint64_t addr);
    
    // Translates a single decoded instruction into host machine code
    void EmitHostCode(const DecodedInstruction& inst, std::vector<uint8_t>& buffer);

    // Helper to map guest registers to host registers (simplified)
    uint8_t MapRegister(uint8_t guest_reg);
};

} // namespace Emulator
