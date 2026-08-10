#pragma once

#include "graphics/guest_gpu/pm4.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Libs::Graphics::Pm4SoftIgnore {

enum class Bank : uint8_t { Cx, Sh, Uc };

struct Hit {
	Bank     bank   = Bank::Cx;
	uint32_t offset = 0;
};

struct ProbeReport {
	std::vector<Hit> packet_soft_ignores;
	std::vector<Hit> uncovered_cx;
	std::vector<Hit> uncovered_sh;
	// UC space is large; only offsets touched by the packet stream are reported
	// unless include_full_uc_coverage is requested.
	std::vector<Hit> uncovered_uc_touched;
	uint32_t         packets_parsed = 0;
	uint32_t         regs_checked   = 0;
};

// Ensure indirect jmp tables are initialized (direct tables are constinit).
void EnsureDispatchReady();

[[nodiscard]] bool IsCxHandled(uint32_t offset) noexcept;
[[nodiscard]] bool IsShHandled(uint32_t offset) noexcept;
[[nodiscard]] bool IsUcHandled(uint32_t offset) noexcept;

// Dry-run a PM4 dword stream: walk Type-3 SET_*_REG packets and record offsets
// that would soft-ignore (no dispatch handler). Does not mutate GPU state.
[[nodiscard]] ProbeReport ProbePacketStream(std::span<const uint32_t> dwords,
                                            bool include_full_table_gaps = false);

[[nodiscard]] std::string FormatReport(const ProbeReport& report);

} // namespace Libs::Graphics::Pm4SoftIgnore
