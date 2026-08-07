// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KYTY_LIBS_RAGE_SCRIPTING_H_
#define KYTY_LIBS_RAGE_SCRIPTING_H_

#include "common/common.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::RageScripting {

// ─── RAGE script thread state ────────────────────────────────────────────────

enum class ScriptThreadState : uint32_t {
	Idle       = 0,
	Running    = 1,
	Waiting    = 2,
	Killed     = 3,
	Paused     = 4,
};

// ─── Bytecode VM opcodes ─────────────────────────────────────────────────────
// RAGE .ysc/.xsc scripts use a stack-based bytecode VM. Opcodes are 1 byte,
// followed by 0-4 bytes of operands depending on the instruction.
//
// Encoding reference (community-reverse-engineered, GTA V era):
//   [opcode]                       — 1 byte, no operands
//   [opcode][u8]                   — 1 byte + 1-byte operand
//   [opcode][lo][hi]               — 1 byte + 2-byte little-endian operand
//   [opcode][b0][b1][b2][b3]       — 1 byte + 4-byte operand (NATIVE_CALL)

enum class RageOpcode : uint8_t {
	NOP            = 0x00,

	// ── Arithmetic ──────────────────────────────────────────────────────
	ADD_I          = 0x01,  // pop b, a → push(a + b)
	SUB_I          = 0x02,  // pop b, a → push(a - b)
	MUL_I          = 0x03,  // pop b, a → push(a * b)
	DIV_I          = 0x04,  // pop b, a → push(a / b)  (guarded ÷0)
	MOD_I          = 0x05,  // pop b, a → push(a % b)
	NEG_I          = 0x06,  // pop a → push(-a)

	// ── Bitwise ─────────────────────────────────────────────────────────
	AND_I          = 0x07,
	OR_I           = 0x08,
	XOR_I          = 0x09,
	NOT_I          = 0x0A,
	SHL_I          = 0x0B,
	SHR_I          = 0x0C,

	// ── Comparison ──────────────────────────────────────────────────────
	CMP_EQ         = 0x0D,  // pop b, a → push(a == b ? 1 : 0)
	CMP_NE         = 0x0E,
	CMP_GT         = 0x0F,
	CMP_GE         = 0x10,
	CMP_LT         = 0x11,
	CMP_LE         = 0x12,

	// ── Float arithmetic (reinterpreted as float bits) ──────────────────
	ADD_F          = 0x13,
	SUB_F          = 0x14,
	MUL_F          = 0x15,
	DIV_F          = 0x16,
	NEG_F          = 0x17,

	// ── Stack manipulation ──────────────────────────────────────────────
	DUP            = 0x1E,  // duplicate TOS
	DUP_N          = 0x1F,  // [u8 n] duplicate stack[n]
	DROP           = 0x20,  // pop and discard TOS
	SWAP           = 0x21,  // swap top two values

	// ── Load constants ──────────────────────────────────────────────────
	PUSH_I8        = 0x22,  // [i8]  push sign-extended 8-bit immediate
	PUSH_I16       = 0x23,  // [i16] push sign-extended 16-bit immediate
	PUSH_I32       = 0x24,  // [i32] push 32-bit immediate
	PUSH_0         = 0x25,  // push integer 0
	PUSH_1         = 0x26,  // push integer 1
	PUSH_M1        = 0x27,  // push integer -1
	PUSH_F         = 0x28,  // [f32] push float immediate (as uint32 bits)
	PUSH_STR       = 0x29,  // [u16 offset] push string pointer from string pool

	// ── Variable access ─────────────────────────────────────────────────
	LOAD_LOCAL     = 0x30,  // [u8 idx] push locals[idx]
	STORE_LOCAL    = 0x31,  // [u8 idx] pop → locals[idx]
	LOAD_PARAM     = 0x32,  // [u8 idx] push params[idx]
	STORE_PARAM    = 0x33,  // [u8 idx] pop → params[idx]
	LOAD_STATIC    = 0x34,  // [u16 idx] push globals[idx]
	STORE_STATIC   = 0x35,  // [u16 idx] pop → globals[idx]

	// ── Control flow ────────────────────────────────────────────────────
	JUMP           = 0x40,  // [i16 offset] pc += offset
	JUMP_FALSE     = 0x41,  // [i16 offset] pop; if 0: pc += offset
	JUMP_TRUE      = 0x42,  // [i16 offset] pop; if !0: pc += offset
	SWITCH         = 0x43,  // [u16 count][table...] multi-way branch

	// ── Functions ───────────────────────────────────────────────────────
	CALL           = 0x50,  // [i16 offset] push pc; pc += offset
	RET            = 0x51,  // pop return address; pc = addr
	NATIVE_CALL    = 0x52,  // [u32 hash][u8 argc] dispatch native

	// ── Thread control ──────────────────────────────────────────────────
	WAIT           = 0x60,  // pop ms; yield thread
	EXIT_THREAD    = 0x61,  // terminate this script thread

	// ── Type conversions ────────────────────────────────────────────────
	I2F            = 0x70,  // pop int → push float (reinterpreted)
	F2I            = 0x71,  // pop float → push int (truncated)
	I2F_CVT       = 0x72,  // pop int → push float (proper conversion)
	F2I_CVT       = 0x73,  // pop float → push int (proper conversion)

	// ── Misc ────────────────────────────────────────────────────────────
	LOAD_CONST_STR = 0x7A,  // [u16] load from string table
	LOAD_NUL       = 0x7B,  // push null/0 handle
	IS_NUL         = 0x7C,  // pop → push(val == 0 ? 1 : 0)
	STR_CMP        = 0x7D,  // pop two string ptrs, push comparison result

	// Sentinel: any opcode not in this table.
	UNKNOWN        = 0xFF,
};

// ─── Code page (one function / script body) ──────────────────────────────────

struct CodePage {
	std::vector<uint8_t>  code;            // raw bytecode
	uint32_t              local_count = 0;  // number of local variable slots
	uint32_t              param_count = 0;  // number of parameter slots
	uint32_t              flags       = 0;
};

// ─── Parsed program (one .ysc file) ──────────────────────────────────────────

struct ScriptProgram {
	uint32_t                 handle       = 0;
	std::string              name;
	uint32_t                 version      = 0;
	std::vector<CodePage>    pages;
	std::vector<uint8_t>     global_data; // static variable initial values
	std::vector<uint8_t>     raw_bytecode; // original blob for re-parsing
	uint32_t                 global_count = 0;
	uint32_t                 native_count = 0;
	uint32_t                 string_table_offset = 0;
};

// ─── VM call stack frame ─────────────────────────────────────────────────────

struct VmCallFrame {
	uint32_t page_index      = 0;    // which code page we're executing
	uint32_t return_pc       = 0;    // PC to return to after RET
	uint32_t base_pointer    = 0;    // stack base for this frame
	uint32_t local_base      = 0;    // offset into locals array
};

// ─── Script thread (VM execution context) ────────────────────────────────────

struct ScriptThread {
	uint32_t         thread_id        = 0;
	std::string      name;
	ScriptThreadState state           = ScriptThreadState::Idle;
	uint32_t         program_handle   = 0;
	uint32_t         wake_time_ms     = 0;
	float            frame_time_slice = 0.0f;

	// VM state
	uint32_t         current_page     = 0;
	uint32_t         pc               = 0;   // program counter within current page

	std::vector<uint64_t>       data_stack;    // operand stack
	std::vector<uint32_t>       locals;        // local variables (flat array)
	std::vector<VmCallFrame>    call_stack;    // function call frames

	// Execution stats
	uint64_t         instructions_executed = 0;
	uint32_t         native_calls_made     = 0;
};

// ─── Native function table ───────────────────────────────────────────────────

using NativeHandler = void (*)(uint64_t* args, uint32_t arg_count, uint64_t* ret);

struct NativeEntry {
	uint64_t      hash        = 0;
	const char*   name        = nullptr;
	NativeHandler handler     = nullptr;
	bool          stub        = true;
};

// ─── Public API ──────────────────────────────────────────────────────────────

void Initialize();
void Shutdown();

void RegisterNative(uint64_t hash, const char* name, NativeHandler handler = nullptr);
void RegisterKnownNatives();

// Load a compiled RAGE script program (.ysc / .xsc bytecode blob).
// Parses the header and code pages. Returns a program handle, or 0 on failure.
uint32_t LoadProgram(const uint8_t* bytecode, uint32_t size, const char* name);

// Start a script thread executing the given program.
uint32_t StartThread(uint32_t program_handle, const char* name, uint32_t stack_size = 8192);

// Advance all running script threads by one tick (called from the game loop).
// Each thread executes up to `max_instructions` bytecodes before yielding.
void TickScripts(float dt_ms, uint32_t max_instructions_per_thread = 100000);

void KillThread(uint32_t thread_id);
ScriptThreadState GetThreadState(uint32_t thread_id);
const NativeEntry* FindNative(uint64_t hash);
uint32_t GetNativeCount();

// Access the shared global variable pool (indexed by STORE_STATIC/LOAD_STATIC).
uint64_t GetGlobal(uint32_t index);
void     SetGlobal(uint32_t index, uint64_t value);

} // namespace Libs::RageScripting

#endif // KYTY_LIBS_RAGE_SCRIPTING_H_
