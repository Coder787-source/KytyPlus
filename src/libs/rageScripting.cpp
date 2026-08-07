// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// RAGE engine bytecode VM interpreter.
//
// Implements a full fetch/decode/execute loop for RAGE .ysc/.xsc script bytecode
// used by GTA V and RDR2. The VM is stack-based with local variables, global
// (static) variables, a call stack, and native function dispatch.
//
// Program layout (RAGE v7/v8 .ysc header, community reverse-engineered):
//   Bytes [0..3]   : magic / identifier
//   Bytes [4..7]   : version
//   Bytes [8..11]  : code page count
//   Bytes [12..15] : global count
//   Bytes [16..19] : native count
//   Bytes [20..23] : string table offset
//   Bytes [24..27] : reserved
//   Bytes [28..31] : global data size
//   [global data blob]
//   For each code page:
//     [4 bytes] code_size
//     [4 bytes] local_count
//     [4 bytes] param_count
//     [4 bytes] flags
//     [code_size bytes] bytecode
//
// Each bytecode instruction is 1 byte opcode + 0-4 bytes operands.

#include "libs/rageScripting.h"

#include "common/logging/log.h"
#include "libs/errno.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_set>

namespace Libs::RageScripting {

namespace {

// ─── Internal state ──────────────────────────────────────────────────────────

std::mutex                                    g_mutex;
std::vector<ScriptProgram>                    g_programs;
std::vector<ScriptThread>                     g_threads;
std::unordered_map<uint64_t, NativeEntry>     g_natives;
std::vector<uint64_t>                         g_globals; // shared static variable pool
std::atomic<uint32_t>                         g_next_thread_id {1};
std::atomic<uint32_t>                         g_next_program_id {1};
bool                                          g_initialized = false;

// ─── Global simulation state ──────────────────────────────────────────────

struct SimulationState {
	uint32_t  game_timer_ms    = 0;
	float     frame_time_s     = 1.0f / 60.0f;
	uint32_t  player_id        = 0;
	uint32_t  player_ped       = 1;
	float     player_x         = 0.0f;
	float     player_y         = 0.0f;
	float     player_z         = 0.0f;
	float     player_heading   = 0.0f;
	int32_t   wanted_level     = 0;
	bool      player_playing   = true;
	std::chrono::steady_clock::time_point start_time =
	    std::chrono::steady_clock::now();
} g_sim_state;

std::unordered_set<uint32_t> g_loaded_models;
std::unordered_set<uint32_t> g_loaded_ptfx;

// One-shot logger for unhandled natives / opcodes.
struct LogGuard {
	static bool ShouldLog(uint64_t key) {
		std::lock_guard lock(s_mutex);
		return s_logged.insert(key).second;
	}
private:
	static std::mutex                      s_mutex;
	static std::unordered_set<uint64_t>    s_logged;
};
std::mutex                   LogGuard::s_mutex;
std::unordered_set<uint64_t> LogGuard::s_logged;

// ─── VM helpers ──────────────────────────────────────────────────────────────

inline uint8_t  ReadU8 (const uint8_t* code, uint32_t pc) { return code[pc]; }
inline uint16_t ReadU16(const uint8_t* code, uint32_t pc) {
	return static_cast<uint16_t>(code[pc]) | (static_cast<uint16_t>(code[pc + 1]) << 8u);
}
inline uint32_t ReadU32(const uint8_t* code, uint32_t pc) {
	return static_cast<uint32_t>(code[pc])      | (static_cast<uint32_t>(code[pc + 1]) << 8u) |
	       (static_cast<uint32_t>(code[pc + 2]) << 16u) | (static_cast<uint32_t>(code[pc + 3]) << 24u);
}
inline int8_t   ReadI8 (const uint8_t* code, uint32_t pc) { return static_cast<int8_t>(code[pc]); }
inline int16_t  ReadI16(const uint8_t* code, uint32_t pc) { return static_cast<int16_t>(ReadU16(code, pc)); }
inline int32_t  ReadI32(const uint8_t* code, uint32_t pc) { return static_cast<int32_t>(ReadU32(code, pc)); }

inline void StackPush(ScriptThread& t, uint64_t v) { t.data_stack.push_back(v); }
inline uint64_t StackPop(ScriptThread& t) {
	if (t.data_stack.empty()) return 0;
	uint64_t v = t.data_stack.back();
	t.data_stack.pop_back();
	return v;
}
inline uint64_t StackPeek(ScriptThread& t) {
	return t.data_stack.empty() ? 0 : t.data_stack.back();
}

// Safe local variable access — auto-grows the locals array.
inline uint64_t GetLocal(ScriptThread& t, uint32_t idx) {
	if (idx >= t.locals.size()) t.locals.resize(idx + 1, 0);
	return t.locals[idx];
}
inline void SetLocal(ScriptThread& t, uint32_t idx, uint64_t val) {
	if (idx >= t.locals.size()) t.locals.resize(idx + 1, 0);
	t.locals[idx] = val;
}

// Safe global variable access — auto-grows the global pool.
inline uint64_t GetStatic(uint32_t idx) {
	if (idx >= g_globals.size()) g_globals.resize(idx + 1, 0);
	return g_globals[idx];
}
inline void SetStatic(uint32_t idx, uint64_t val) {
	if (idx >= g_globals.size()) g_globals.resize(idx + 1, 0);
	g_globals[idx] = val;
}

// Find the program for a thread.
const ScriptProgram* FindProgram(uint32_t handle) {
	for (const auto& prog : g_programs) {
		if (prog.handle == handle) return &prog;
	}
	return nullptr;
}

// Get the code page currently being executed by a thread.
const CodePage* CurrentPage(const ScriptThread& t, const ScriptProgram& prog) {
	if (t.current_page < prog.pages.size()) return &prog.pages[t.current_page];
	return nullptr;
}

// ─── Stateful native implementations ──────────────────────────────────────

void NativeWait(uint64_t* /*args*/, uint32_t /*arg_count*/, uint64_t* ret) {
	if (ret) *ret = 0;
}

void NativeTimerA(uint64_t* /*args*/, uint32_t /*arg_count*/, uint64_t* ret) {
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - g_sim_state.start_time).count();
	if (ret) *ret = static_cast<uint64_t>(ms) & 0xFFFFFFFFu;
}

void NativeTimerB(uint64_t* /*args*/, uint32_t /*arg_count*/, uint64_t* ret) {
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - g_sim_state.start_time).count();
	if (ret) *ret = (static_cast<uint64_t>(ms) + 1000u) & 0xFFFFFFFFu;
}

void NativeGetGameTimer(uint64_t* /*args*/, uint32_t /*arg_count*/, uint64_t* ret) {
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - g_sim_state.start_time).count();
	g_sim_state.game_timer_ms = static_cast<uint32_t>(ms);
	if (ret) *ret = g_sim_state.game_timer_ms;
}

void NativeGetFrameTime(uint64_t* /*args*/, uint32_t /*arg_count*/, uint64_t* ret) {
	if (ret) { uint32_t f; std::memcpy(&f, &g_sim_state.frame_time_s, sizeof(f)); *ret = f; }
}

void NativeGetHashKey(uint64_t* args, uint32_t /*arg_count*/, uint64_t* ret) {
	if (ret) {
		if (args && args[0]) {
			const char* s = reinterpret_cast<const char*>(args[0]);
			uint32_t h = 0;
			while (*s) { h += static_cast<uint8_t>(*s); h += (h << 10); h ^= (h >> 6); s++; }
			h += (h << 3); h ^= (h >> 11); h += (h << 15);
			*ret = h;
		} else *ret = 0;
	}
}

void NativePlayerId(uint64_t*, uint32_t, uint64_t* ret) { if (ret) *ret = g_sim_state.player_id; }
void NativePlayerPedId(uint64_t*, uint32_t, uint64_t* ret) { if (ret) *ret = g_sim_state.player_ped; }
void NativeIsPlayerPlaying(uint64_t*, uint32_t, uint64_t* ret) { if (ret) *ret = g_sim_state.player_playing ? 1u : 0u; }
void NativeGetWantedLevel(uint64_t*, uint32_t, uint64_t* ret) { if (ret) *ret = static_cast<uint64_t>(g_sim_state.wanted_level); }

void NativeSetWantedLevel(uint64_t* args, uint32_t, uint64_t* ret) {
	if (args) g_sim_state.wanted_level = static_cast<int32_t>(args[1]);
	if (ret) *ret = 0;
}

void NativeDoesEntityExist(uint64_t* args, uint32_t, uint64_t* ret) {
	if (ret) *ret = (args && args[0]) ? 1u : 0u;
}

void NativeGetEntityCoords(uint64_t*, uint32_t, uint64_t* ret) {
	if (ret) { uint32_t f; std::memcpy(&f, &g_sim_state.player_x, sizeof(f)); *ret = f; }
}

void NativeSetEntityCoords(uint64_t* args, uint32_t, uint64_t* ret) {
	if (args) {
		std::memcpy(&g_sim_state.player_x, &args[1], sizeof(float));
		std::memcpy(&g_sim_state.player_y, &args[2], sizeof(float));
		std::memcpy(&g_sim_state.player_z, &args[3], sizeof(float));
	}
	if (ret) *ret = 0;
}

void NativeGetEntityHeading(uint64_t*, uint32_t, uint64_t* ret) {
	if (ret) { uint32_t f; std::memcpy(&f, &g_sim_state.player_heading, sizeof(f)); *ret = f; }
}

void NativeSetEntityHeading(uint64_t* args, uint32_t, uint64_t* ret) {
	if (args) std::memcpy(&g_sim_state.player_heading, &args[1], sizeof(float));
	if (ret) *ret = 0;
}

void NativeIsEntityDead(uint64_t*, uint32_t, uint64_t* ret) { if (ret) *ret = 0; }

void NativeRequestModel(uint64_t* args, uint32_t, uint64_t* ret) {
	if (args) g_loaded_models.insert(static_cast<uint32_t>(args[0]));
	if (ret) *ret = 0;
}

void NativeHasModelLoaded(uint64_t* args, uint32_t, uint64_t* ret) {
	if (ret) *ret = (args && g_loaded_models.count(static_cast<uint32_t>(args[0]))) ? 1u : 0u;
}

void NativeSetModelNoLongerNeeded(uint64_t* args, uint32_t, uint64_t* ret) {
	if (args) g_loaded_models.erase(static_cast<uint32_t>(args[0]));
	if (ret) *ret = 0;
}

void NativeRequestPtfx(uint64_t* args, uint32_t, uint64_t* ret) {
	if (args) g_loaded_ptfx.insert(static_cast<uint32_t>(args[0]));
	if (ret) *ret = 0;
}

void NativeHasPtfxLoaded(uint64_t* args, uint32_t, uint64_t* ret) {
	if (ret) *ret = (args && g_loaded_ptfx.count(static_cast<uint32_t>(args[0]))) ? 1u : 0u;
}

void NativeIsStringNullOrEmpty(uint64_t* args, uint32_t, uint64_t* ret) {
	if (ret) {
		if (!args || !args[0]) *ret = 1;
		else *ret = (reinterpret_cast<const char*>(args[0])[0] == '\0') ? 1u : 0u;
	}
}

void DefaultStubNative(uint64_t*, uint32_t, uint64_t* ret) { if (ret) *ret = 0; }

// ─── Native dispatch from VM ─────────────────────────────────────────────────

bool DispatchNative(uint64_t hash, uint64_t* args, uint32_t argc, uint64_t* ret) {
	auto it = g_natives.find(hash);
	if (it != g_natives.end() && it->second.handler) {
		it->second.handler(args, argc, ret);
		return true;
	}
	// Unknown native — log once and return 0.
	if (LogGuard::ShouldLog(hash | 0x100000000ULL)) {
		LOGF("[RageVM] WARN: unknown native hash 0x%08X (returning 0)\n",
		     static_cast<uint32_t>(hash));
	}
	if (ret) *ret = 0;
	return false;
}

// ─── VM execution engine ─────────────────────────────────────────────────────
// Execute up to `budget` instructions on one thread. Returns false if the
// thread should stop (EXIT, error, or WAIT).

bool ExecuteThread(ScriptThread& thread, const ScriptProgram& prog, uint32_t budget) {
	constexpr uint32_t kMaxStackDepth = 4096;
	constexpr uint32_t kMaxCallDepth  = 256;

	for (uint32_t step = 0; step < budget; step++) {
		if (thread.state != ScriptThreadState::Running) return false;

		const CodePage* page = CurrentPage(thread, prog);
		if (!page || page->code.empty()) {
			LOGF("[RageVM] ERR: thread '%s' has invalid page %u\n",
			     thread.name.c_str(), thread.current_page);
			thread.state = ScriptThreadState::Killed;
			return false;
		}

		const uint8_t* code = page->code.data();
		const uint32_t code_size = static_cast<uint32_t>(page->code.size());

		if (thread.pc >= code_size) {
			// Fell off end of page — treat as implicit return.
			if (thread.call_stack.empty()) {
				thread.state = ScriptThreadState::Killed;
				return false;
			}
			auto& frame = thread.call_stack.back();
			thread.current_page = frame.page_index;
			thread.pc = frame.return_pc;
			thread.call_stack.pop_back();
			continue;
		}

		const auto opcode = static_cast<RageOpcode>(code[thread.pc]);
		uint32_t next_pc = thread.pc + 1;

		switch (opcode) {

		// ── No-op ─────────────────────────────────────────────────────────
		case RageOpcode::NOP:
			break;

		// ── Integer arithmetic ────────────────────────────────────────────
		case RageOpcode::ADD_I: {
			int32_t b = static_cast<int32_t>(StackPop(thread));
			int32_t a = static_cast<int32_t>(StackPop(thread));
			StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(a + b)));
			break;
		}
		case RageOpcode::SUB_I: {
			int32_t b = static_cast<int32_t>(StackPop(thread));
			int32_t a = static_cast<int32_t>(StackPop(thread));
			StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(a - b)));
			break;
		}
		case RageOpcode::MUL_I: {
			int32_t b = static_cast<int32_t>(StackPop(thread));
			int32_t a = static_cast<int32_t>(StackPop(thread));
			StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(a * b)));
			break;
		}
		case RageOpcode::DIV_I: {
			int32_t b = static_cast<int32_t>(StackPop(thread));
			int32_t a = static_cast<int32_t>(StackPop(thread));
			StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(b != 0 ? a / b : 0)));
			break;
		}
		case RageOpcode::MOD_I: {
			int32_t b = static_cast<int32_t>(StackPop(thread));
			int32_t a = static_cast<int32_t>(StackPop(thread));
			StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(b != 0 ? a % b : 0)));
			break;
		}
		case RageOpcode::NEG_I: {
			int32_t a = static_cast<int32_t>(StackPop(thread));
			StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(-a)));
			break;
		}

		// ── Bitwise ──────────────────────────────────────────────────────
		case RageOpcode::AND_I: { uint64_t b = StackPop(thread); StackPush(thread, StackPop(thread) & b); break; }
		case RageOpcode::OR_I:  { uint64_t b = StackPop(thread); StackPush(thread, StackPop(thread) | b); break; }
		case RageOpcode::XOR_I: { uint64_t b = StackPop(thread); StackPush(thread, StackPop(thread) ^ b); break; }
		case RageOpcode::NOT_I: { StackPush(thread, ~StackPop(thread)); break; }
		case RageOpcode::SHL_I: { uint32_t s = static_cast<uint32_t>(StackPop(thread)); StackPush(thread, StackPop(thread) << (s & 31)); break; }
		case RageOpcode::SHR_I: { uint32_t s = static_cast<uint32_t>(StackPop(thread)); StackPush(thread, StackPop(thread) >> (s & 31)); break; }

		// ── Comparison ───────────────────────────────────────────────────
		case RageOpcode::CMP_EQ: { int32_t b = static_cast<int32_t>(StackPop(thread)); int32_t a = static_cast<int32_t>(StackPop(thread)); StackPush(thread, a == b ? 1u : 0u); break; }
		case RageOpcode::CMP_NE: { int32_t b = static_cast<int32_t>(StackPop(thread)); int32_t a = static_cast<int32_t>(StackPop(thread)); StackPush(thread, a != b ? 1u : 0u); break; }
		case RageOpcode::CMP_GT: { int32_t b = static_cast<int32_t>(StackPop(thread)); int32_t a = static_cast<int32_t>(StackPop(thread)); StackPush(thread, a >  b ? 1u : 0u); break; }
		case RageOpcode::CMP_GE: { int32_t b = static_cast<int32_t>(StackPop(thread)); int32_t a = static_cast<int32_t>(StackPop(thread)); StackPush(thread, a >= b ? 1u : 0u); break; }
		case RageOpcode::CMP_LT: { int32_t b = static_cast<int32_t>(StackPop(thread)); int32_t a = static_cast<int32_t>(StackPop(thread)); StackPush(thread, a <  b ? 1u : 0u); break; }
		case RageOpcode::CMP_LE: { int32_t b = static_cast<int32_t>(StackPop(thread)); int32_t a = static_cast<int32_t>(StackPop(thread)); StackPush(thread, a <= b ? 1u : 0u); break; }

		// ── Float arithmetic (bit-reinterpret) ───────────────────────────
		case RageOpcode::ADD_F: { uint32_t rb = static_cast<uint32_t>(StackPop(thread)); uint32_t ra = static_cast<uint32_t>(StackPop(thread)); float fa, fb; std::memcpy(&fa,&ra,4); std::memcpy(&fb,&rb,4); float r=fa+fb; uint32_t rr; std::memcpy(&rr,&r,4); StackPush(thread,rr); break; }
		case RageOpcode::SUB_F: { uint32_t rb = static_cast<uint32_t>(StackPop(thread)); uint32_t ra = static_cast<uint32_t>(StackPop(thread)); float fa, fb; std::memcpy(&fa,&ra,4); std::memcpy(&fb,&rb,4); float r=fa-fb; uint32_t rr; std::memcpy(&rr,&r,4); StackPush(thread,rr); break; }
		case RageOpcode::MUL_F: { uint32_t rb = static_cast<uint32_t>(StackPop(thread)); uint32_t ra = static_cast<uint32_t>(StackPop(thread)); float fa, fb; std::memcpy(&fa,&ra,4); std::memcpy(&fb,&rb,4); float r=fa*fb; uint32_t rr; std::memcpy(&rr,&r,4); StackPush(thread,rr); break; }
		case RageOpcode::DIV_F: { uint32_t rb = static_cast<uint32_t>(StackPop(thread)); uint32_t ra = static_cast<uint32_t>(StackPop(thread)); float fa, fb; std::memcpy(&fa,&ra,4); std::memcpy(&fb,&rb,4); float r=(fb!=0.0f)?fa/fb:0.0f; uint32_t rr; std::memcpy(&rr,&r,4); StackPush(thread,rr); break; }
		case RageOpcode::NEG_F: { uint32_t ra = static_cast<uint32_t>(StackPop(thread)); float fa; std::memcpy(&fa,&ra,4); fa=-fa; uint32_t rr; std::memcpy(&rr,&fa,4); StackPush(thread,rr); break; }

		// ── Stack manipulation ───────────────────────────────────────────
		case RageOpcode::DUP:
			if (thread.data_stack.size() > kMaxStackDepth) { thread.state = ScriptThreadState::Killed; return false; }
			StackPush(thread, StackPeek(thread));
			break;
		case RageOpcode::DUP_N: {
			uint8_t n = ReadU8(code, next_pc); next_pc += 1;
			if (n < thread.data_stack.size()) StackPush(thread, thread.data_stack[thread.data_stack.size() - 1 - n]);
			else StackPush(thread, 0);
			break;
		}
		case RageOpcode::DROP: StackPop(thread); break;
		case RageOpcode::SWAP: {
			if (thread.data_stack.size() >= 2) {
				uint64_t a = StackPop(thread);
				uint64_t b = StackPop(thread);
				StackPush(thread, a);
				StackPush(thread, b);
			}
			break;
		}

		// ── Load constants ───────────────────────────────────────────────
		case RageOpcode::PUSH_I8: { int8_t v = ReadI8(code, next_pc); next_pc += 1; StackPush(thread, static_cast<uint64_t>(static_cast<int64_t>(v))); break; }
		case RageOpcode::PUSH_I16: { int16_t v = ReadI16(code, next_pc); next_pc += 2; StackPush(thread, static_cast<uint64_t>(static_cast<int64_t>(v))); break; }
		case RageOpcode::PUSH_I32: { int32_t v = ReadI32(code, next_pc); next_pc += 4; StackPush(thread, static_cast<uint64_t>(static_cast<uint32_t>(v))); break; }
		case RageOpcode::PUSH_0:  StackPush(thread, 0); break;
		case RageOpcode::PUSH_1:  StackPush(thread, 1); break;
		case RageOpcode::PUSH_M1: StackPush(thread, static_cast<uint64_t>(0xFFFFFFFFu)); break;
		case RageOpcode::PUSH_F: { uint32_t v = ReadU32(code, next_pc); next_pc += 4; StackPush(thread, v); break; }
		case RageOpcode::PUSH_STR: {
			uint16_t off = ReadU16(code, next_pc); next_pc += 2;
			// Push offset into string table as a handle; the native that
			// reads it will resolve it against the program's string pool.
			StackPush(thread, off);
			break;
		}

		// ── Variable access ──────────────────────────────────────────────
		case RageOpcode::LOAD_LOCAL:  { uint8_t i = ReadU8(code, next_pc); next_pc += 1; StackPush(thread, GetLocal(thread, i)); break; }
		case RageOpcode::STORE_LOCAL: { uint8_t i = ReadU8(code, next_pc); next_pc += 1; SetLocal(thread, i, StackPop(thread)); break; }
		case RageOpcode::LOAD_PARAM:  { uint8_t i = ReadU8(code, next_pc); next_pc += 1; StackPush(thread, GetLocal(thread, i)); break; }
		case RageOpcode::STORE_PARAM: { uint8_t i = ReadU8(code, next_pc); next_pc += 1; SetLocal(thread, i, StackPop(thread)); break; }
		case RageOpcode::LOAD_STATIC:  { uint16_t i = ReadU16(code, next_pc); next_pc += 2; StackPush(thread, GetStatic(i)); break; }
		case RageOpcode::STORE_STATIC: { uint16_t i = ReadU16(code, next_pc); next_pc += 2; SetStatic(i, StackPop(thread)); break; }

		// ── Control flow ─────────────────────────────────────────────────
		case RageOpcode::JUMP: {
			int16_t off = ReadI16(code, next_pc);
			next_pc = static_cast<uint32_t>(static_cast<int32_t>(next_pc + 2) + off);
			break;
		}
		case RageOpcode::JUMP_FALSE: {
			int16_t off = ReadI16(code, next_pc);
			next_pc += 2;
			uint64_t cond = StackPop(thread);
			if (cond == 0) next_pc = static_cast<uint32_t>(static_cast<int32_t>(next_pc) + off);
			break;
		}
		case RageOpcode::JUMP_TRUE: {
			int16_t off = ReadI16(code, next_pc);
			next_pc += 2;
			uint64_t cond = StackPop(thread);
			if (cond != 0) next_pc = static_cast<uint32_t>(static_cast<int32_t>(next_pc) + off);
			break;
		}
		case RageOpcode::SWITCH: {
			// SWITCH [u16 count] [table of (i32 value, i16 offset) pairs]
			// Pop the switch value, search the table, jump to matching offset.
			uint16_t count = ReadU16(code, next_pc); next_pc += 2;
			int32_t val = static_cast<int32_t>(StackPop(thread));
			int16_t default_off = 0;
			bool found = false;
			for (uint32_t i = 0; i < count; i++) {
				int32_t case_val = ReadI32(code, next_pc); next_pc += 4;
				int16_t case_off = ReadI16(code, next_pc); next_pc += 2;
				if (case_val == val) { default_off = case_off; found = true; break; }
			}
			if (!found) {
				// Skip remaining entries.
				for (uint32_t i = 0; i < count && !found; i++) { next_pc += 6; }
			} else {
				// Skip remaining entries.
			}
			next_pc = static_cast<uint32_t>(static_cast<int32_t>(next_pc) + default_off);
			break;
		}

		// ── Functions ────────────────────────────────────────────────────
		case RageOpcode::CALL: {
			int16_t off = ReadI16(code, next_pc);
			if (thread.call_stack.size() >= kMaxCallDepth) {
				LOGF("[RageVM] ERR: thread '%s' call stack overflow\n", thread.name.c_str());
				thread.state = ScriptThreadState::Killed;
				return false;
			}
			VmCallFrame frame;
			frame.page_index   = thread.current_page;
			frame.return_pc    = next_pc + 2; // after the operand
			frame.base_pointer = static_cast<uint32_t>(thread.data_stack.size());
			frame.local_base   = static_cast<uint32_t>(thread.locals.size());
			thread.call_stack.push_back(frame);
			next_pc = static_cast<uint32_t>(static_cast<int32_t>(next_pc + 2) + off);
			break;
		}
		case RageOpcode::RET: {
			if (thread.call_stack.empty()) {
				thread.state = ScriptThreadState::Killed;
				return false;
			}
			auto& frame = thread.call_stack.back();
			thread.current_page = frame.page_index;
			next_pc = frame.return_pc;
			// Restore locals size (discard callee locals).
			if (thread.locals.size() > frame.local_base)
				thread.locals.resize(frame.local_base);
			thread.call_stack.pop_back();
			break;
		}

		// ── Native call ──────────────────────────────────────────────────
		case RageOpcode::NATIVE_CALL: {
			uint32_t hash = ReadU32(code, next_pc); next_pc += 4;
			uint8_t  argc = ReadU8(code, next_pc);  next_pc += 1;

			// Pop arguments (right-to-left).
			uint64_t args[32] = {};
			for (int32_t i = static_cast<int32_t>(argc) - 1; i >= 0; i--) {
				args[i] = StackPop(thread);
			}

			uint64_t result = 0;
			DispatchNative(hash, args, argc, &result);
			StackPush(thread, result);
			thread.native_calls_made++;
			break;
		}

		// ── Thread control ───────────────────────────────────────────────
		case RageOpcode::WAIT: {
			uint32_t ms = static_cast<uint32_t>(StackPop(thread));
			auto now = std::chrono::steady_clock::now();
			auto cur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			    now - g_sim_state.start_time).count();
			thread.wake_time_ms = static_cast<uint32_t>(cur_ms) + ms;
			thread.state = ScriptThreadState::Waiting;
			thread.pc = next_pc; // resume after WAIT on wake
			return false;
		}
		case RageOpcode::EXIT_THREAD:
			thread.state = ScriptThreadState::Killed;
			return false;

		// ── Type conversions ─────────────────────────────────────────────
		case RageOpcode::I2F: {
			int32_t iv = static_cast<int32_t>(StackPop(thread));
			float fv = static_cast<float>(iv);
			uint32_t bits; std::memcpy(&bits, &fv, 4);
			StackPush(thread, bits);
			break;
		}
		case RageOpcode::F2I: {
			uint32_t bits = static_cast<uint32_t>(StackPop(thread));
			float fv; std::memcpy(&fv, &bits, 4);
			StackPush(thread, static_cast<uint64_t>(static_cast<int32_t>(fv)));
			break;
		}
		case RageOpcode::I2F_CVT: {
			int32_t iv = static_cast<int32_t>(StackPop(thread));
			float fv = static_cast<float>(iv);
			uint32_t bits; std::memcpy(&bits, &fv, 4);
			StackPush(thread, bits);
			break;
		}
		case RageOpcode::F2I_CVT: {
			uint32_t bits = static_cast<uint32_t>(StackPop(thread));
			float fv; std::memcpy(&fv, &bits, 4);
			StackPush(thread, static_cast<uint64_t>(static_cast<int32_t>(fv)));
			break;
		}

		// ── Misc ─────────────────────────────────────────────────────────
		case RageOpcode::LOAD_NUL: StackPush(thread, 0); break;
		case RageOpcode::IS_NUL:   StackPush(thread, StackPop(thread) == 0 ? 1u : 0u); break;
		case RageOpcode::STR_CMP: {
			uint64_t b = StackPop(thread);
			uint64_t a = StackPop(thread);
			if (a == 0 && b == 0) StackPush(thread, 1);
			else if (a == 0 || b == 0) StackPush(thread, 0);
			else {
				const char* sa = reinterpret_cast<const char*>(a);
				const char* sb = reinterpret_cast<const char*>(b);
				StackPush(thread, std::strcmp(sa, sb) == 0 ? 1u : 0u);
			}
			break;
		}
		case RageOpcode::LOAD_CONST_STR: {
			uint16_t off = ReadU16(code, next_pc); next_pc += 2;
			StackPush(thread, off); // string table offset handle
			break;
		}

		// ── Unknown / unhandled opcode ───────────────────────────────────
		default: {
			if (LogGuard::ShouldLog(static_cast<uint8_t>(opcode) | 0x200000000ULL)) {
				LOGF("[RageVM] WARN: unknown opcode 0x%02X at pc=%u in thread '%s' page %u (skipping)\n",
				     static_cast<uint8_t>(opcode), thread.pc,
				     thread.name.c_str(), thread.current_page);
			}
			// Skip: assume single-byte opcode with no operands.
			break;
		}
		} // end switch

		// Guard against runaway PC.
		if (next_pc > code_size + 64) {
			LOGF("[RageVM] ERR: thread '%s' PC out of bounds (%u > %u)\n",
			     thread.name.c_str(), next_pc, code_size);
			thread.state = ScriptThreadState::Killed;
			return false;
		}

		thread.pc = next_pc;
		thread.instructions_executed++;
	}
	return true;
}

} // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────────────────

void Initialize() {
	std::lock_guard lock(g_mutex);
	if (g_initialized) return;
	g_initialized = true;
	g_globals.resize(4096, 0);
	RegisterKnownNatives();
	LOGF("[RageVM] INFO: RAGE bytecode VM initialized (%zu natives)\n", g_natives.size());
}

void Shutdown() {
	std::lock_guard lock(g_mutex);
	g_threads.clear();
	g_programs.clear();
	g_natives.clear();
	g_globals.clear();
	g_initialized = false;
	LOGF("[RageVM] INFO: RAGE bytecode VM shut down\n");
}

void RegisterNative(uint64_t hash, const char* name, NativeHandler handler) {
	std::lock_guard lock(g_mutex);
	NativeEntry entry;
	entry.hash    = hash;
	entry.name    = name;
	entry.handler = handler != nullptr ? handler : DefaultStubNative;
	entry.stub    = (handler == nullptr);
	g_natives[hash] = entry;
}

void RegisterKnownNatives() {
	struct NativeDef { uint64_t hash; const char* name; NativeHandler handler; };
	static constexpr NativeDef kStateful[] = {
		{0x489E2820, "SYSTEM::WAIT",                  NativeWait},
		{0xE3D967E4, "SYSTEM::TIMERA",                 NativeTimerA},
		{0x7F95E0C8, "SYSTEM::TIMERB",                 NativeTimerB},
		{0x9CD27B30, "GAMEPLAY::GET_GAME_TIMER",       NativeGetGameTimer},
		{0x3F4A0D10, "GAMEPLAY::GET_FRAME_TIME",       NativeGetFrameTime},
		{0xE19B7F10, "GAMEPLAY::GET_HASH_KEY",         NativeGetHashKey},
		{0x14D913B8, "GAMEPLAY::IS_STRING_NULL_OR_EMPTY", NativeIsStringNullOrEmpty},
		{0xD80958FC, "PLAYER::PLAYER_ID",              NativePlayerId},
		{0x4F8644AF, "PLAYER::PLAYER_PED_ID",          NativePlayerPedId},
		{0x98DA48B3, "PLAYER::IS_PLAYER_PLAYING",      NativeIsPlayerPlaying},
		{0x5E9564D8, "PLAYER::GET_PLAYER_WANTED_LEVEL", NativeGetWantedLevel},
		{0x3F78E69F, "PLAYER::SET_PLAYER_WANTED_LEVEL", NativeSetWantedLevel},
		{0xD5037BA8, "ENTITY::DOES_ENTITY_EXIST",      NativeDoesEntityExist},
		{0x5A9C3D92, "ENTITY::GET_ENTITY_COORDS",      NativeGetEntityCoords},
		{0x6E31B987, "ENTITY::SET_ENTITY_COORDS",      NativeSetEntityCoords},
		{0x1794B4FC, "ENTITY::GET_ENTITY_HEADING",     NativeGetEntityHeading},
		{0xC21B30A1, "ENTITY::SET_ENTITY_HEADING",     NativeSetEntityHeading},
		{0x9F279008, "ENTITY::IS_ENTITY_DEAD",          NativeIsEntityDead},
		{0x444CB644, "STREAMING::REQUEST_MODEL",       NativeRequestModel},
		{0x98A4EB5D, "STREAMING::HAS_MODEL_LOADED",    NativeHasModelLoaded},
		{0xE532F5D7, "STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED", NativeSetModelNoLongerNeeded},
		{0x5C182568, "GRAPHICS::REQUEST_NAMED_PTFX_ASSET", NativeRequestPtfx},
		{0x8702416E, "GRAPHICS::HAS_NAMED_PTFX_ASSET_LOADED", NativeHasPtfxLoaded},
	};
	struct StubDef { uint64_t hash; const char* name; };
	static constexpr StubDef kStubs[] = {
		{0x2892D2B7, "ENTITY::GET_ENTITY_MODEL"},
		{0x8D68C8FD, "ENTITY::SET_ENTITY_VISIBLE"},
		{0xD7421A4F, "PED::IS_PED_IN_ANY_VEHICLE"},
		{0xA3EE4A07, "PED::CREATE_PED"},
		{0xB736A491, "PED::SET_PED_COMPONENT_VARIATION"},
		{0xAF35D0D2, "VEHICLE::CREATE_VEHICLE"},
		{0xB41B7848, "VEHICLE::SET_VEHICLE_ON_GROUND_PROPERLY"},
		{0x24C28B38, "VEHICLE::IS_VEHICLE_DRIVEABLE"},
		{0x62DFAFBD, "VEHICLE::SET_VEHICLE_MOD_KIT"},
		{0x96D70B68, "HUD::BEGIN_TEXT_COMMAND_DISPLAY_TEXT"},
		{0xCD015E5B, "HUD::END_TEXT_COMMAND_DISPLAY_TEXT"},
		{0x6C188BE1, "HUD::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME"},
		{0x61BB1D9B, "GRAPHICS::DRAW_RECT"},
		{0xBF3F6BFF, "GRAPHICS::DRAW_LINE"},
	};
	for (const auto& d : kStateful) RegisterNative(d.hash, d.name, d.handler);
	for (const auto& d : kStubs)    RegisterNative(d.hash, d.name, nullptr);
	LOGF("[RageVM] INFO: Registered %zu stateful + %zu stub natives\n",
	     std::size(kStateful), std::size(kStubs));
}

uint32_t LoadProgram(const uint8_t* bytecode, uint32_t size, const char* name) {
	std::lock_guard lock(g_mutex);
	if (!bytecode || size < 32) {
		LOGF("[RageVM] WARN: LoadProgram: invalid bytecode (size=%u)\n", size);
		return 0;
	}

	ScriptProgram prog;
	prog.handle = g_next_program_id.fetch_add(1, std::memory_order_relaxed);
	prog.name   = name ? name : "unnamed";
	prog.raw_bytecode.assign(bytecode, bytecode + size);

	// Parse the RAGE .ysc header (32 bytes).
	// Bytes 0..3:   magic
	// Bytes 4..7:   version
	// Bytes 8..11:  page_count
	// Bytes 12..15: global_count
	// Bytes 16..19: native_count
	// Bytes 20..23: string_table_offset
	// Bytes 24..27: reserved
	// Bytes 28..31: global_data_size

	prog.version          = ReadU32(bytecode, 4);
	uint32_t page_count   = ReadU32(bytecode, 8);
	prog.global_count     = ReadU32(bytecode, 12);
	prog.native_count     = ReadU32(bytecode, 16);
	prog.string_table_offset = ReadU32(bytecode, 20);
	uint32_t global_data_size = ReadU32(bytecode, 28);

	LOGF("[RageVM] INFO: LoadProgram '%s' v%u, %u pages, %u globals, %u natives, %u bytes global data\n",
	     prog.name.c_str(), prog.version, page_count, prog.global_count, prog.native_count, global_data_size);

	// Copy global data.
	uint32_t offset = 32;
	if (global_data_size > 0 && offset + global_data_size <= size) {
		prog.global_data.assign(bytecode + offset, bytecode + offset + global_data_size);
		// Initialize the global pool from this data.
		uint32_t n_globals = global_data_size / sizeof(uint64_t);
		if (n_globals > g_globals.size()) g_globals.resize(n_globals, 0);
		std::memcpy(g_globals.data(), prog.global_data.data(),
		            std::min<size_t>(prog.global_data.size(), n_globals * sizeof(uint64_t)));
		offset += global_data_size;
	}

	// Parse code pages.
	for (uint32_t i = 0; i < page_count && offset + 16 <= size; i++) {
		CodePage page;
		uint32_t code_size   = ReadU32(bytecode, offset);      offset += 4;
		page.local_count     = ReadU32(bytecode, offset);      offset += 4;
		page.param_count     = ReadU32(bytecode, offset);      offset += 4;
		page.flags           = ReadU32(bytecode, offset);      offset += 4;

		if (code_size > 0 && offset + code_size <= size) {
			page.code.assign(bytecode + offset, bytecode + offset + code_size);
			offset += code_size;
		}

		LOGF("[RageVM] INFO:   page[%u]: %u bytes code, %u locals, %u params\n",
		     i, code_size, page.local_count, page.param_count);
		prog.pages.push_back(std::move(page));
	}

	g_programs.push_back(std::move(prog));
	return g_programs.back().handle;
}

uint32_t StartThread(uint32_t program_handle, const char* name, uint32_t stack_size) {
	std::lock_guard lock(g_mutex);
	const auto* prog = FindProgram(program_handle);
	if (!prog || prog->pages.empty()) {
		LOGF("[RageVM] WARN: StartThread: invalid program handle %u\n", program_handle);
		return 0;
	}

	ScriptThread thread;
	thread.thread_id      = g_next_thread_id.fetch_add(1, std::memory_order_relaxed);
	thread.name           = name ? name : "script";
	thread.state          = ScriptThreadState::Running;
	thread.program_handle = program_handle;
	thread.current_page   = 0; // start executing page 0 (main)
	thread.pc             = 0;

	// Allocate locals for the first page.
	uint32_t initial_locals = prog->pages[0].local_count;
	thread.locals.resize(std::max(initial_locals, 64u), 0);

	g_threads.push_back(std::move(thread));
	LOGF("[RageVM] INFO: Started thread '%s' (id=%u, program=%u, page0=%u bytes)\n",
	     name ? name : "script", g_threads.back().thread_id,
	     program_handle, static_cast<uint32_t>(prog->pages[0].code.size()));
	return g_threads.back().thread_id;
}

void TickScripts(float dt_ms, uint32_t max_instructions_per_thread) {
	std::lock_guard lock(g_mutex);
	if (!g_initialized) return;

	// Update frame time.
	g_sim_state.frame_time_s = dt_ms / 1000.0f;

	// Get current time for waking sleeping threads.
	auto now = std::chrono::steady_clock::now();
	uint32_t now_ms = static_cast<uint32_t>(
	    std::chrono::duration_cast<std::chrono::milliseconds>(
	        now - g_sim_state.start_time).count());

	for (auto& thread : g_threads) {
		// Wake sleeping threads.
		if (thread.state == ScriptThreadState::Waiting && now_ms >= thread.wake_time_ms) {
			thread.state = ScriptThreadState::Running;
		}
		if (thread.state != ScriptThreadState::Running) continue;

		const auto* prog = FindProgram(thread.program_handle);
		if (!prog) {
			thread.state = ScriptThreadState::Killed;
			continue;
		}

		ExecuteThread(thread, *prog, max_instructions_per_thread);
	}
}

void KillThread(uint32_t thread_id) {
	std::lock_guard lock(g_mutex);
	for (auto& t : g_threads) {
		if (t.thread_id == thread_id) {
			t.state = ScriptThreadState::Killed;
			LOGF("[RageVM] INFO: Killed thread '%s' (id=%u, executed %" PRIu64 " instructions, %u native calls)\n",
			     t.name.c_str(), thread_id, t.instructions_executed, t.native_calls_made);
			return;
		}
	}
}

ScriptThreadState GetThreadState(uint32_t thread_id) {
	std::lock_guard lock(g_mutex);
	for (const auto& t : g_threads) {
		if (t.thread_id == thread_id) return t.state;
	}
	return ScriptThreadState::Killed;
}

const NativeEntry* FindNative(uint64_t hash) {
	std::lock_guard lock(g_mutex);
	auto it = g_natives.find(hash);
	return it != g_natives.end() ? &it->second : nullptr;
}

uint32_t GetNativeCount() {
	std::lock_guard lock(g_mutex);
	return static_cast<uint32_t>(g_natives.size());
}

uint64_t GetGlobal(uint32_t index) {
	std::lock_guard lock(g_mutex);
	return GetStatic(index);
}

void SetGlobal(uint32_t index, uint64_t value) {
	std::lock_guard lock(g_mutex);
	SetStatic(index, value);
}

} // namespace Libs::RageScripting
