#include "loader/x64InstructionEmulator.h"

#include "common/common.h"
#include "common/logging/log.h"

#include <atomic>
#include <cstddef>

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#include <windows.h> // IWYU pragma: keep
#endif

namespace Loader::X64InstructionEmulator {

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS

static M128A* GetContextXmm(PCONTEXT context, uint8_t index) {
	if (context == nullptr || index >= 16) {
		return nullptr;
	}

	return &context->Xmm0 + index;
}

static uint64_t ExtractBitField(uint64_t value, uint32_t length, uint32_t index) {
	length &= 0x3fu;
	index &= 0x3fu;

	if (length == 0) {
		length = 64;
	}

	if (index >= 64) {
		return 0;
	}

	auto available = 64u - index;
	if (length > available) {
		length = available;
	}

	const uint64_t mask = (length == 64 ? UINT64_MAX : ((uint64_t {1} << length) - 1u));
	return (value >> index) & mask;
}

static uint64_t InsertBitField(uint64_t dst, uint64_t src, uint32_t length, uint32_t index) {
	length &= 0x3fu;
	index &= 0x3fu;

	if (length == 0) {
		length = 64;
	}

	if (index >= 64) {
		return dst;
	}

	auto available = 64u - index;
	if (length > available) {
		length = available;
	}

	const uint64_t mask        = (length == 64 ? UINT64_MAX : ((uint64_t {1} << length) - 1u));
	const uint64_t shifted     = (index == 0 ? mask : (mask << index));
	const uint64_t src_shifted = (src & mask) << index;

	return (dst & ~shifted) | src_shifted;
}

static bool TryEmulateSse4a(PCONTEXT context) {
	if (context == nullptr) {
		return false;
	}

	const auto* rip = reinterpret_cast<const uint8_t*>(context->Rip);

	const uint8_t prefix = rip[0];
	if (prefix != 0x66 && prefix != 0xf2) {
		return false;
	}

	size_t  offset = 1;
	uint8_t rex    = 0;
	if ((rip[offset] & 0xf0u) == 0x40u) {
		rex = rip[offset];
		offset++;
	}

	if (rip[offset] != 0x0f || rip[offset + 1] != 0x78) {
		return false;
	}

	auto modrm = rip[offset + 2];
	if ((modrm & 0xc0u) != 0xc0u) {
		return false;
	}

	const uint8_t reg    = ((modrm >> 3u) & 0x07u) | ((rex & 0x04u) << 1u);
	const uint8_t rm     = (modrm & 0x07u) | ((rex & 0x01u) << 3u);
	const uint8_t length = rip[offset + 3];
	const uint8_t index  = rip[offset + 4];

	// AMD SSE4a immediate-form EXTRQ/INSERTQ. PS5 code can execute these natively on AMD hardware,
	// while Intel hosts raise an illegal-instruction exception.
	if (prefix == 0x66) {
		auto* dst = GetContextXmm(context, rm);
		if (dst == nullptr) {
			return false;
		}

		dst->Low  = ExtractBitField(dst->Low, length, index);
		dst->High = 0;
		context->Rip += offset + 5;
		return true;
	}

	auto* dst = GetContextXmm(context, reg);
	auto* src = GetContextXmm(context, rm);
	if (dst == nullptr || src == nullptr) {
		return false;
	}

	dst->Low = InsertBitField(dst->Low, src->Low, length, index);
	context->Rip += offset + 5;
	return true;
}

static bool TryEmulateMonitorxMwaitx(PCONTEXT context) {
	if (context == nullptr) {
		return false;
	}

	const auto* rip = reinterpret_cast<const uint8_t*>(context->Rip);
	if (rip[0] != 0x0f || rip[1] != 0x01 || (rip[2] != 0xfa && rip[2] != 0xfb)) {
		return false;
	}

	// AMD MONITORX/MWAITX are used by PS5 code in wait loops. Intel hosts can raise an illegal-
	// instruction exception, so approximate them as a no-op/yield pair.
	if (rip[2] == 0xfb) {
		SwitchToThread();
	}
	context->Rip += 3;
	return true;
}

#endif

bool TryEmulate(void* native_context) {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	auto* context = static_cast<PCONTEXT>(native_context);
	return TryEmulateMonitorxMwaitx(context) || TryEmulateSse4a(context);
#else
	(void)native_context;
	return false;
#endif
}

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS

// Compact length decoder covering the common store/load forms used by guest fault skips.
// Returns 0 when the instruction cannot be sized safely.
static size_t GetX64InstructionLength(const uint8_t* code) {
	if (code == nullptr) {
		return 0;
	}

	size_t  offset   = 0;
	uint8_t prefixes = 0;
	bool    has_rex  = false;
	uint8_t rex      = 0;
	bool    op_size  = false;
	bool    addr_size = false;

	for (; offset < 15; ++offset) {
		const uint8_t b = code[offset];
		if (b == 0x66) {
			op_size = true;
			++prefixes;
			continue;
		}
		if (b == 0x67) {
			addr_size = true;
			++prefixes;
			continue;
		}
		if (b == 0xf0 || b == 0xf2 || b == 0xf3 || b == 0x2e || b == 0x36 || b == 0x3e ||
		    b == 0x26 || b == 0x64 || b == 0x65) {
			++prefixes;
			continue;
		}
		break;
	}
	if (offset >= 15) {
		return 0;
	}
	if ((code[offset] & 0xf0u) == 0x40u) {
		has_rex = true;
		rex     = code[offset];
		++offset;
		++prefixes;
	}
	if (offset >= 15) {
		return 0;
	}

	const auto read_modrm = [&](size_t at, uint8_t* modrm_out, size_t* disp_out,
	                            bool* has_sib_out) -> bool {
		if (at >= 15) {
			return false;
		}
		const uint8_t modrm = code[at];
		*modrm_out          = modrm;
		const uint8_t mod   = static_cast<uint8_t>(modrm >> 6u);
		const uint8_t rm    = static_cast<uint8_t>(modrm & 0x7u);
		*has_sib_out        = false;
		*disp_out           = 0;
		size_t cursor       = at + 1;
		if (mod != 3 && rm == 4 && !addr_size) {
			*has_sib_out = true;
			if (cursor >= 15) {
				return false;
			}
			const uint8_t sib = code[cursor++];
			if (mod == 0 && (sib & 0x7u) == 5) {
				*disp_out = 4;
			}
		}
		if (mod == 1) {
			*disp_out = 1;
		} else if (mod == 2) {
			*disp_out = 4;
		} else if (mod == 0 && rm == 5 && !addr_size) {
			*disp_out = 4;
		}
		(void)has_rex;
		(void)rex;
		(void)op_size;
		(void)prefixes;
		return cursor + *disp_out <= 15;
	};

	const uint8_t op = code[offset++];
	uint8_t       modrm = 0;
	size_t        disp  = 0;
	bool          sib   = false;

	auto finish_modrm = [&](size_t imm) -> size_t {
		if (!read_modrm(offset, &modrm, &disp, &sib)) {
			return 0;
		}
		size_t len = offset + 1 + (sib ? 1u : 0u) + disp + imm;
		return len > 0 && len <= 15 ? len : 0;
	};

	// MOV r/m, r and MOV r, r/m (8/16/32/64)
	if (op == 0x88 || op == 0x89 || op == 0x8a || op == 0x8b || op == 0x86 || op == 0x87) {
		return finish_modrm(0);
	}
	// MOV r/m, imm8 / imm16/32
	if (op == 0xc6) {
		return finish_modrm(1);
	}
	if (op == 0xc7) {
		return finish_modrm(op_size ? 2u : 4u);
	}
	// MOV [rip+disp32], rAX family and absolute moffs
	if (op == 0xa0 || op == 0xa1 || op == 0xa2 || op == 0xa3) {
		const size_t moffs = addr_size ? 4u : (has_rex && (rex & 0x8u) != 0 ? 8u : 4u);
		const size_t len   = offset + moffs;
		return len > 0 && len <= 15 ? len : 0;
	}
	// ALU r/m, r / r, r/m
	if ((op & 0xc4u) == 0x00 && (op & 0x3u) <= 0x3u) {
		return finish_modrm(0);
	}
	// Group 1 immediate
	if (op == 0x80 || op == 0x82) {
		return finish_modrm(1);
	}
	if (op == 0x81) {
		return finish_modrm(op_size ? 2u : 4u);
	}
	if (op == 0x83) {
		return finish_modrm(1);
	}
	// MOVZX / MOVSX / MOVSXD with 0F prefix already consumed? handle 0F escape
	if (op == 0x0f) {
		if (offset >= 15) {
			return 0;
		}
		const uint8_t op2 = code[offset++];
		if (op2 == 0xb6 || op2 == 0xb7 || op2 == 0xbe || op2 == 0xbf || op2 == 0xb0 || op2 == 0xb1) {
			return finish_modrm(0);
		}
		return 0;
	}
	// PUSH/POP r/m
	if (op == 0xff || op == 0x8f) {
		return finish_modrm(0);
	}
	// XCHG rax,r and single-byte ops are not memory ops — reject
	return 0;
}

bool TrySkipNullPageAccess(void* native_context, uint64_t access_vaddr) {
	constexpr uint64_t kNullPageSize = 0x1000;
	if (native_context == nullptr || access_vaddr >= kNullPageSize) {
		return false;
	}
	auto* context = static_cast<PCONTEXT>(native_context);
	if (context == nullptr || context->Rip == 0) {
		return false;
	}
	const auto* code = reinterpret_cast<const uint8_t*>(context->Rip);
	MEMORY_BASIC_INFORMATION mbi = {};
	if (VirtualQuery(code, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT ||
	    (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
		return false;
	}
	const size_t length = GetX64InstructionLength(code);
	if (length == 0 || length > 15) {
		return false;
	}
	static std::atomic<uint32_t> skip_logs {0};
	if (skip_logs.fetch_add(1, std::memory_order_relaxed) < 32) {
		LOGF_COLOR(Log::Color::Yellow,
		           "soft-skip null-page access: rip=0x%016" PRIx64 " vaddr=0x%016" PRIx64
		           " len=%zu\n",
		           static_cast<uint64_t>(context->Rip), access_vaddr, length);
	}
	context->Rip += length;
	return true;
}

#else

bool TrySkipNullPageAccess(void* /*native_context*/, uint64_t /*access_vaddr*/) {
	return false;
}

#endif

} // namespace Loader::X64InstructionEmulator
