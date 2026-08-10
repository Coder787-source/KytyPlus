#include "loader/nullPageFaultLog.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace Loader::X64InstructionEmulator {

namespace {

constexpr size_t kRingCapacity = 64;
std::mutex       g_mutex;
std::array<NullPageFaultFingerprint, kRingCapacity> g_ring {};
size_t           g_ring_next  = 0;
size_t           g_ring_count = 0;
std::atomic<uint64_t> g_total {0};

} // namespace

void RecordNullPageFault(const NullPageFaultFingerprint& fault) {
	g_total.fetch_add(1, std::memory_order_relaxed);
	std::lock_guard lock(g_mutex);
	g_ring[g_ring_next] = fault;
	g_ring_next         = (g_ring_next + 1) % kRingCapacity;
	if (g_ring_count < kRingCapacity) {
		++g_ring_count;
	}
}

void ResetNullPageFaultLog() noexcept {
	std::lock_guard lock(g_mutex);
	g_ring_next  = 0;
	g_ring_count = 0;
	g_total.store(0, std::memory_order_relaxed);
	g_ring.fill(NullPageFaultFingerprint {});
}

size_t NullPageFaultCount() noexcept {
	return static_cast<size_t>(g_total.load(std::memory_order_relaxed));
}

size_t CopyNullPageFaults(NullPageFaultFingerprint* out, size_t capacity) noexcept {
	if (out == nullptr || capacity == 0) {
		return 0;
	}
	std::lock_guard lock(g_mutex);
	const size_t    n = std::min(capacity, g_ring_count);
	for (size_t i = 0; i < n; ++i) {
		const size_t idx =
		    (g_ring_next + kRingCapacity - g_ring_count + i) % kRingCapacity;
		out[i] = g_ring[idx];
	}
	return n;
}

std::string FormatNullPageFaultFingerprint(const NullPageFaultFingerprint& fault) {
	char opcode_hex[48] {};
	size_t pos = 0;
	for (uint8_t i = 0; i < fault.opcode_len && pos + 3 < sizeof(opcode_hex); ++i) {
		pos += static_cast<size_t>(
		    std::snprintf(opcode_hex + pos, sizeof(opcode_hex) - pos, "%02x", fault.opcode[i]));
	}
	char line[192];
	std::snprintf(line, sizeof(line),
	              "null-page-fault rip=0x%016llx vaddr=0x%016llx skip=%u fallback=%u opcode=%s",
	              static_cast<unsigned long long>(fault.rip),
	              static_cast<unsigned long long>(fault.access_vaddr), fault.skip_length,
	              fault.fallback_1b ? 1u : 0u, opcode_hex);
	return std::string(line);
}

} // namespace Loader::X64InstructionEmulator
