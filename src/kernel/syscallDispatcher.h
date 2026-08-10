#ifndef EMULATOR_INCLUDE_EMULATOR_KERNEL_SYSCALLDISPATCHER_H_
#define EMULATOR_INCLUDE_EMULATOR_KERNEL_SYSCALLDISPATCHER_H_

#include "common/abi.h"
#include "common/common.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Kyty::Libs::LibKernel {

// ---------------------------------------------------------------------------
// Guest syscall dispatcher.
//
// KytyPlus runs guest x86-64 code natively on the host CPU (see loader/jit.h),
// so a guest `syscall` instruction has no implicit trap target. PS4 (Orbis) and
// PS5 (Prospero) both use the x86-64 `syscall` instruction with the syscall
// number in rax and arguments in rdi/rsi/rdx/r10/r8/r9 (System V AMD64).
//
// This dispatcher is the single place that resolves a guest syscall number to
// a host HLE implementation. It is platform-aware: PS4 and PS5 share most of
// the kernel surface but have divergent syscall numbering in places, so each
// table is keyed separately.
//
// A handler returns the value placed in rax. Return value semantics follow the
// Orbis/Prospero convention: negative values are errno-encoded kernel errors
// (see libs/errno.h), non-negative values are success.
// ---------------------------------------------------------------------------

enum class GuestPlatform : uint8_t {
	Unknown = 0,
	Ps4     = 1, // Orbis  (GCN,  ABI version 0)
	Ps5     = 2, // Prospero (RDNA2/AGC, ABI version 2)
};

// A guest syscall handler. Up to six System V AMD64 integer arguments are
// forwarded as void* (the handler is responsible for interpreting them). This
// keeps the table uniform without a per-syscall signature explosion.
using SyscallHandler = std::function<int64_t(uint64_t a0, uint64_t a1, uint64_t a2,
                                             uint64_t a3, uint64_t a4, uint64_t a5)>;

class SyscallDispatcher {
public:
	static SyscallDispatcher& Instance() {
		static SyscallDispatcher instance;
		return instance;
	}

	// Register a handler for a given platform + syscall number. Idempotent.
	void Register(GuestPlatform platform, uint64_t number, SyscallHandler handler);

	// Dispatch a guest syscall. Returns the value to write into rax. If no
	// handler is registered, logs an UnresolvedSyscall diagnostic and returns
	// KERNEL_ERROR_ENOSYS so the guest sees a clean, traceable failure instead
	// of an undefined host fault.
	int64_t Dispatch(GuestPlatform platform, uint64_t number,
	                 uint64_t a0, uint64_t a1, uint64_t a2,
	                 uint64_t a3, uint64_t a4, uint64_t a5) const;

	// Report unresolved syscalls observed during the current run. Used by the
	// PS4 compat-gap analyzer to surface a real work order.
	struct UnresolvedRecord {
		GuestPlatform platform;
		uint64_t      number;
		uint64_t      hit_count;
	};
	[[nodiscard]] std::vector<UnresolvedRecord> UnresolvedSnapshot() const;

	// Test/inspection helpers.
	[[nodiscard]] bool IsRegistered(GuestPlatform platform, uint64_t number) const;
	[[nodiscard]] size_t RegisteredCount(GuestPlatform platform) const;

	void Clear();

private:
	SyscallDispatcher() = default;

	struct Key {
		GuestPlatform platform = GuestPlatform::Unknown;
		uint64_t      number   = 0;
		bool operator==(const Key& other) const noexcept {
			return platform == other.platform && number == other.number;
		}
	};
	struct KeyHash {
		size_t operator()(const Key& k) const noexcept {
			return std::hash<uint64_t>{}(k.number) ^
			       (static_cast<uint64_t>(k.platform) << 56);
		}
	};

	std::unordered_map<Key, SyscallHandler, KeyHash>        m_handlers;
	mutable std::unordered_map<Key, uint64_t, KeyHash>     m_unresolved;
	mutable std::mutex                                    m_mutex;
};

// One-time registration of the shared PS4/PS5 kernel syscall surface. Maps
// Orbis/Prospero syscall numbers onto the existing Libs::LibKernel::FileSystem
// and pthread implementations instead of the no-op stubs in syscallExtended.cpp.
// Implemented in syscallDispatcher.cpp.
void RegisterKernelSyscalls(SyscallDispatcher& dispatcher);

} // namespace Kyty::Libs::LibKernel

#endif /* EMULATOR_INCLUDE_EMULATOR_KERNEL_SYSCALLDISPATCHER_H_ */
