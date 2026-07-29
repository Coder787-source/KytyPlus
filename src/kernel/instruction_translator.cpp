#include "instruction_translator.h"
#include <stdexcept>

namespace Emulator {

InstructionTranslator::InstructionTranslator() {
    std::cout << "[JIT] Instruction Translator initialized." << std::endl;
}

DecodedInstruction InstructionTranslator::Decode(const uint8_t* code, size_t& offset, uint64_t addr) {
    DecodedInstruction inst;
    inst.original_address = addr;
    uint8_t opcode = code[offset];
    
    // This is a highly simplified x86-64 decoder for demonstration.
    // In a production emulator, we would use a library like Zydis or Capstone.
    switch (opcode) {
        case 0x90: // NOP
            inst.op = OpCode::NOP;
            inst.length = 1;
            break;
        case 0xB8: // MOV EAX, imm32
            inst.op = OpCode::MOV_REG_IMM;
            inst.dest_reg = 0; // RAX
            inst.immediate = *reinterpret_cast<const int32_t*>(&code[offset + 1]);
            inst.length = 5;
            break;
        case 0x89: // MOV reg, reg (simplified)
            inst.op = OpCode::MOV_REG_REG;
            inst.dest_reg = (code[offset + 1] >> 3) & 0x7;
            inst.src_reg = code[offset + 1] & 0x7;
            inst.length = 2;
            break;
        default:
            inst.op = OpCode::UNKNOWN;
            inst.length = 1;
            break;
    }
    
    offset += inst.length;
    return inst;
}

void InstructionTranslator::EmitHostCode(const DecodedInstruction& inst, std::vector<uint8_t>& buffer) {
    switch (inst.op) {
        case OpCode::NOP:
            buffer.push_back(0x90); 
            break;
        case OpCode::MOV_REG_IMM:
            // For a real JIT, we would emit the host's equivalent machine code.
            // Here, we simulate a "Pass-through" for compatible x86-64 instructions.
            buffer.push_back(0xB8);
            for (int i = 0; i < 4; ++i) buffer.push_back((inst.immediate >> (i * 8)) & 0xFF);
            break;
        case OpCode::MOV_REG_REG:
            buffer.push_back(0x89);
            buffer.push_back(inst.src_reg | (inst.dest_reg << 3));
            break;
        case OpCode::UNKNOWN:
            std::cerr << "[JIT] Unknown opcode at 0x" << std::hex << inst.original_address << std::dec << ". Inserting trap." << std::endl;
            // Insert an INT3 or a call to the emulator's exception handler
            buffer.push_back(0xCC); 
            break;
        default:
            break;
    }
}

std::vector<uint8_t> InstructionTranslator::TranslateBlock(const std::vector<uint8_t>& guest_code, uint64_t start_address) {
    std::vector<uint8_t> host_code;
    size_t offset = 0;

    while (offset < guest_code.size()) {
        DecodedInstruction inst = Decode(guest_code.data(), offset, start_address + offset);
        EmitHostCode(inst, host_code);
    }

    return host_code;
}

} // namespace Emulator
