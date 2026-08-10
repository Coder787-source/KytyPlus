#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Loader::X64InstructionEmulator {

struct NullPageFaultFingerprint {
	uint64_t rip          = 0;
	uint64_t access_vaddr = 0;
	uint32_t skip_length  = 0;
	uint8_t  opcode[16]   = {};
	uint8_t  opcode_len   = 0;
	bool     fallback_1b  = false;
};

// Rolling ring of recent #66-class near-null faults for post-mortem / dump matching.
// Real root-cause fix still requires the guest eboot + exact fault RIP.
void RecordNullPageFault(const NullPageFaultFingerprint& fault);
void ResetNullPageFaultLog() noexcept;
[[nodiscard]] size_t NullPageFaultCount() noexcept;
[[nodiscard]] size_t CopyNullPageFaults(NullPageFaultFingerprint* out, size_t capacity) noexcept;
[[nodiscard]] std::string FormatNullPageFaultFingerprint(const NullPageFaultFingerprint& fault);

} // namespace Loader::X64InstructionEmulator
