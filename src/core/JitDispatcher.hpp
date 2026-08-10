#pragma once

#include <cstdint>
#include "kyty_expected.hpp"
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "Avx512Emitter.hpp"

namespace KytyPS5::Core {

struct alignas(64) ThreadContext {
	uint64_t gprs[16]; // x86-64 has 16 GPRs (rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8-r15)
	uint64_t sp = 0;
	uint64_t pc = 0;
	uint64_t rflags = 0;
};

struct OpMov {
	uint8_t reg_dest = 0;
	uint8_t reg_src = 0;
	uint64_t offset = 0;
};
struct OpAdd {
	uint8_t reg_dest = 0;
	uint8_t reg_src = 0;
	uint64_t offset = 0;
};
struct OpLdr {
	uint8_t reg_dest = 0;
	uint8_t base_reg = 0;
	uint64_t offset = 0;
};
struct OpStr {
	uint8_t reg_src = 0;
	uint8_t base_reg = 0;
	uint64_t offset = 0;
};
struct OpJmp {
	uint64_t target_addr = 0;
};
struct OpSyscall {
	uint32_t call_id = 0;
};
struct OpAvx512 {
	uint32_t opcode = 0;
	uint8_t zmm_reg = 0;
};

using Instruction = std::variant<OpMov, OpAdd, OpLdr, OpStr, OpJmp, OpSyscall, OpAvx512>;

struct DispatchResult {
	bool is_syscall = false;
	uint32_t call_id = 0;
	uint64_t next_pc = 0;
	std::vector<uint64_t> args;
};

class JitDispatcher {
public:
	JitDispatcher() = default;

	// Scaffolding stub used by SystemOrchestrator.
	std::expected<DispatchResult, std::string> Dispatch(uint64_t entry_point) {
		DispatchResult result;
		result.next_pc = entry_point;
		return result;
	}

	std::vector<uint8_t> TranslateBlock(const std::vector<Instruction>& block) {
		code_buffer_.clear();
		for (const auto& instr : block) {
			Emit(instr);
		}
		return code_buffer_;
	}

	void Emit(const Instruction& instr) {
		std::visit(
		    [this](auto&& arg) {
			    using T = std::decay_t<decltype(arg)>;
			    if constexpr (std::is_same_v<T, OpMov>) {
				    EmitMov(arg);
			    } else if constexpr (std::is_same_v<T, OpAdd>) {
				    EmitAdd(arg);
			    } else if constexpr (std::is_same_v<T, OpLdr>) {
				    EmitLdr(arg);
			    } else if constexpr (std::is_same_v<T, OpStr>) {
				    EmitStr(arg);
			    } else if constexpr (std::is_same_v<T, OpJmp>) {
				    EmitJmp(arg);
			    } else if constexpr (std::is_same_v<T, OpSyscall>) {
				    EmitSyscall(arg);
			    } else if constexpr (std::is_same_v<T, OpAvx512>) {
				    EmitAvx512(arg);
			    }
		    },
		    instr);
	}

private:
	static constexpr uint8_t HOT_MAP[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12};
	static constexpr uint8_t CONTEXT_BASE_REG = 11;

	void PushByte(uint8_t b) { code_buffer_.push_back(b); }

	void PushModRM(uint8_t modrm_reg, uint8_t rm_reg, uint32_t offset = 0) {
		if (offset == 0) {
			PushByte(static_cast<uint8_t>((0x00 << 6) | ((modrm_reg & 7) << 3) | (rm_reg & 7)));
		} else if (offset < 128) {
			PushByte(static_cast<uint8_t>((0x01 << 6) | ((modrm_reg & 7) << 3) | (rm_reg & 7)));
			PushByte(static_cast<uint8_t>(offset));
		} else {
			PushByte(static_cast<uint8_t>((0x02 << 6) | ((modrm_reg & 7) << 3) | (rm_reg & 7)));
			for (int i = 0; i < 4; ++i) {
				PushByte(static_cast<uint8_t>((offset >> (i * 8)) & 0xFF));
			}
		}
	}

	void EmitFillMove(uint8_t guest_reg, uint8_t target_hot_reg) {
		if (guest_reg < 12) {
			return;
		}
		PushByte(0x48);
		PushByte(0x8B);
		PushModRM(target_hot_reg, CONTEXT_BASE_REG, guest_reg * 8u);
	}

	void EmitSpillMove(uint8_t guest_reg, uint8_t source_hot_reg) {
		if (guest_reg < 12) {
			return;
		}
		PushByte(0x48);
		PushByte(0x89);
		PushModRM(CONTEXT_BASE_REG, source_hot_reg, guest_reg * 8u);
	}

	void EmitMov(const OpMov& op) {
		const uint8_t d = op.reg_dest < 12 ? HOT_MAP[op.reg_dest] : 10;
		const uint8_t s = op.reg_src < 12 ? HOT_MAP[op.reg_src] : 10;
		if (op.reg_src >= 12) {
			EmitFillMove(op.reg_src, 10);
		}
		PushByte(0x48);
		PushByte(0x89);
		PushModRM(d, s);
		if (op.reg_dest >= 12) {
			EmitSpillMove(op.reg_dest, d);
		}
	}

	void EmitAdd(const OpAdd& op) {
		const uint8_t d = op.reg_dest < 12 ? HOT_MAP[op.reg_dest] : 10;
		const uint8_t s = op.reg_src < 12 ? HOT_MAP[op.reg_src] : 10;
		if (op.reg_src >= 12) {
			EmitFillMove(op.reg_src, 10);
		}
		PushByte(0x48);
		PushByte(0x01);
		PushModRM(d, s);
		if (op.reg_dest >= 12) {
			EmitSpillMove(op.reg_dest, d);
		}
	}

	void EmitLdr(const OpLdr& op) {
		const uint8_t d = op.reg_dest < 12 ? HOT_MAP[op.reg_dest] : 10;
		const uint8_t b = op.base_reg < 12 ? HOT_MAP[op.base_reg] : 10;
		if (op.base_reg >= 12) {
			EmitFillMove(op.base_reg, 10);
		}
		// MOV d, [b + offset]
		PushByte(0x48);
		PushByte(0x8B);
		PushModRM(d, b, static_cast<uint32_t>(op.offset));
		if (op.reg_dest >= 12) {
			EmitSpillMove(op.reg_dest, d);
		}
	}

	void EmitStr(const OpStr& op) {
		const uint8_t s = op.reg_src < 12 ? HOT_MAP[op.reg_src] : 10;
		const uint8_t b = op.base_reg < 12 ? HOT_MAP[op.base_reg] : 10;
		if (op.reg_src >= 12) {
			EmitFillMove(op.reg_src, 10);
		}
		if (op.base_reg >= 12) {
			EmitFillMove(op.base_reg, 10);
		}
		// MOV [b + offset], s
		PushByte(0x48);
		PushByte(0x89);
		PushModRM(s, b, static_cast<uint32_t>(op.offset));
	}

	void EmitJmp(const OpJmp& op) {
		// JMP rel32 — caller must compute relative offset from end of this instruction
		// For scaffolding, emit placeholder that will be patched later
		PushByte(0xE9);
		const uint32_t rel = static_cast<uint32_t>(op.target_addr);
		for (int i = 0; i < 4; ++i) {
			PushByte(static_cast<uint8_t>((rel >> (i * 8)) & 0xFF));
		}
	}

	void EmitSyscall(const OpSyscall& op) {
		// MOV RAX, call_id
		PushByte(0x48);
		PushByte(0xB8);
		const uint64_t id = op.call_id;
		for (int i = 0; i < 8; ++i) {
			PushByte(static_cast<uint8_t>((id >> (i * 8)) & 0xFF));
		}
		// SYSCALL
		PushByte(0x0F);
		PushByte(0x05);
	}

	void EmitAvx512(const OpAvx512& op) {
		// Delegate to the AVX-512 emitter for proper encoding
		avx_emitter_->EmitRaw(op.opcode, op.zmm_reg, code_buffer_);
	}

	std::vector<uint8_t> code_buffer_;
	std::unique_ptr<KytyPS5::JIT::Avx512Emitter> avx_emitter_ =
	    std::make_unique<KytyPS5::JIT::Avx512Emitter>();
};

} // namespace KytyPS5::Core
