#ifndef KYTY_COMMON_HOST_EXCEPTION_H_
#define KYTY_COMMON_HOST_EXCEPTION_H_

#include "common/common.h"

#include <cstddef>
#include <cstdint>

namespace Common::HostException {

enum class ExceptionType { Unknown, AccessViolation, IllegalInstruction };

enum class AccessViolationType { Unknown, Read, Write, Execute };

struct ExceptionInfo {
	ExceptionType       type                   = ExceptionType::Unknown;
	AccessViolationType access_violation_type  = AccessViolationType::Unknown;
	uint64_t            access_violation_vaddr = 0;
	uint64_t            exception_address      = 0;
	uint64_t            rax                    = 0;
	uint64_t            rbx                    = 0;
	uint64_t            rcx                    = 0;
	uint64_t            rdx                    = 0;
	uint64_t            rsi                    = 0;
	uint64_t            rdi                    = 0;
	uint64_t            rbp                    = 0;
	uint64_t            rsp                    = 0;
	uint64_t            r8                     = 0;
	uint64_t            r9                     = 0;
	uint64_t            r10                    = 0;
	uint64_t            r11                    = 0;
	uint64_t            r12                    = 0;
	uint64_t            r13                    = 0;
	uint64_t            r14                    = 0;
	uint64_t            r15                    = 0;
	uint32_t            native_code            = 0;
	// Platform-specific mutable context, valid only for the duration of the handler call.
	void* native_context = nullptr;
};

using Handler = bool (*)(const ExceptionInfo&);

bool InstallHandler(Handler handler);

// ---------------------------------------------------------------------------
// Async-signal-safe raw formatter.
//
// The fault path must never itself fault. RawLog is restricted to raw OS write
// primitives and a fixed stack buffer: no CRT heap, no iostreams, no std::string,
// no locks, no exceptions. A fault inside the CRT (heap corruption, iostream
// state) must not be able to re-enter the formatter. This is the ONLY sanctioned
// sink for the register dump, which runs inside the exception filter.
// ---------------------------------------------------------------------------
class RawLog final {
public:
	RawLog() noexcept = default;

	RawLog(const RawLog&)            = delete;
	RawLog& operator=(const RawLog&) = delete;

	void Append(const char* s) noexcept {
		while (*s != '\0' && m_len < kCapacity - 1) {
			m_buf[m_len++] = *s++;
		}
	}

	void AppendChar(char c) noexcept {
		if (m_len < kCapacity - 1) {
			m_buf[m_len++] = c;
		}
	}

	void AppendHex(uint64_t value) noexcept {
		char tmp[17];
		int  n = 0;
		if (value == 0) {
			tmp[n++] = '0';
		} else {
			for (int shift = 60; shift >= 0; shift -= 4) {
				const auto nibble = static_cast<unsigned>((value >> shift) & 0xF);
				if (n != 0 || nibble != 0) {
					tmp[n++] = "0123456789abcdef"[nibble];
				}
			}
		}
		Append("0x");
		for (int i = 0; i < n; ++i) {
			AppendChar(tmp[i]);
		}
	}

	void AppendHex32(uint32_t value) noexcept {
		AppendHex(static_cast<uint64_t>(value));
	}

	void AppendByte(uint8_t value) noexcept {
		char tmp[3];
		tmp[0] = "0123456789abcdef"[(value >> 4) & 0xF];
		tmp[1] = "0123456789abcdef"[value & 0xF];
		tmp[2] = '\0';
		Append(tmp);
	}

	void Flush() noexcept {
		RawWriteStderr(m_buf, m_len);
		m_len = 0;
	}

private:
	static void RawWriteStderr(const char* data, size_t len) noexcept;

	static constexpr size_t kCapacity = 2048;
	char                    m_buf[kCapacity];
	size_t                  m_len = 0;
};

// Emit a complete, allocation-free register dump for the given exception.
// Safe to call from inside the exception filter. Never throws, never allocates,
// never locks. Degrades (prints "<unmapped>") instead of crashing on bad ranges.
void DumpExceptionInfo(const ExceptionInfo& info) noexcept;

} // namespace Common::HostException

#endif /* KYTY_COMMON_HOST_EXCEPTION_H_ */
