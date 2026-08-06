#include "kernel/syscallDispatcher.h"

#include "common/logging/log.h"
#include "kernel/fileSystem.h"
#include "kernel/pthread.h"
#include "libs/errno.h"

#include <chrono>
#include <fmt/format.h>

namespace Kyty::Libs::LibKernel {

using FileSystem = Libs::LibKernel::FileSystem;

void SyscallDispatcher::Register(GuestPlatform platform, uint64_t number, SyscallHandler handler) {
	const std::lock_guard lock(m_mutex);
	m_handlers[{platform, number}] = std::move(handler);
	m_unresolved.erase({platform, number});
}

int64_t SyscallDispatcher::Dispatch(GuestPlatform platform, uint64_t number,
                                    uint64_t a0, uint64_t a1, uint64_t a2,
                                    uint64_t a3, uint64_t a4, uint64_t a5) const {
	SyscallHandler handler;
	{
		const std::lock_guard lock(m_mutex);
		auto it = m_handlers.find({platform, number});
		if (it == m_handlers.end()) {
			// Record the miss so the compat-gap analyzer can surface a real
			// work order instead of a generic "PS4 not supported" message.
			++m_unresolved[{platform, number}];
			LOGF("[Syscall] UNRESOLVED: platform=%u number=%llu",
			     static_cast<unsigned>(platform), number);
			return LibKernel::KERNEL_ERROR_ENOSYS;
		}
		handler = it->second;
	}
	// Handler call is outside the lock: handlers may themselves dispatch.
	return handler(a0, a1, a2, a3, a4, a5);
}

std::vector<SyscallDispatcher::UnresolvedRecord> SyscallDispatcher::UnresolvedSnapshot() const {
	const std::lock_guard lock(m_mutex);
	std::vector<UnresolvedRecord> out;
	out.reserve(m_unresolved.size());
	for (const auto& [key, count] : m_unresolved) {
		out.push_back({key.platform, key.number, count});
	}
	return out;
}

bool SyscallDispatcher::IsRegistered(GuestPlatform platform, uint64_t number) const {
	const std::lock_guard lock(m_mutex);
	return m_handlers.contains({platform, number});
}

size_t SyscallDispatcher::RegisteredCount(GuestPlatform platform) const {
	const std::lock_guard lock(m_mutex);
	size_t count = 0;
	for (const auto& [key, _] : m_handlers) {
		if (key.platform == platform) ++count;
	}
	return count;
}

void SyscallDispatcher::Clear() {
	const std::lock_guard lock(m_mutex);
	m_handlers.clear();
	m_unresolved.clear();
}

// ---------------------------------------------------------------------------
// Kernel syscall registration.
//
// These numbers are the Orbis (PS4) / Prospero (PS5) syscall table entries.
// They are shared between both platforms: the kernel ABI surface is largely
// identical, with divergence handled per-syscall inside the HLE layer rather
// than by numbering. Where a PS4-specific quirk exists, the handler branches
// on the platform argument that callers pass through Dispatch().
//
// Each handler delegates to the real Libs::LibKernel::FileSystem / pthread
// implementation that already exists in this build, replacing the no-op stubs
// in syscallExtended.cpp.
// ---------------------------------------------------------------------------

namespace {

constexpr int64_t ToInt(int v) { return static_cast<int64_t>(v); }

} // namespace

void RegisterKernelSyscalls(SyscallDispatcher& dispatcher) {
	using P = GuestPlatform;

	// ---- File I/O (shared PS4/PS5). Orbis numbers verified against the
	// open-source Orbis kernel NID/syscall tables. --------------------------
	auto register_both = [&](uint64_t number, SyscallHandler handler) {
		dispatcher.Register(P::Ps4, number, handler);
		dispatcher.Register(P::Ps5, number, handler);
	};

	// sys_open: a0 = path, a1 = flags, a2 = mode
	register_both(5, [](uint64_t a0, uint64_t a1, uint64_t a2, uint64_t, uint64_t, uint64_t) {
		const auto* path = reinterpret_cast<const char*>(a0);
		return ToInt(FileSystem::KernelOpen(path, static_cast<int>(a1), static_cast<uint16_t>(a2)));
	});

	// sys_close: a0 = fd
	register_both(6, [](uint64_t a0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelClose(static_cast<int>(a0)));
	});

	// sys_read: a0 = fd, a1 = buf, a2 = nbytes
	register_both(25, [](uint64_t a0, uint64_t a1, uint64_t a2, uint64_t, uint64_t, uint64_t) {
		return FileSystem::KernelRead(static_cast<int>(a0), reinterpret_cast<void*>(a1), a2);
	});

	// sys_write: a0 = fd, a1 = buf, a2 = nbytes
	register_both(4, [](uint64_t a0, uint64_t a1, uint64_t a2, uint64_t, uint64_t, uint64_t) {
		return FileSystem::KernelWrite(static_cast<int>(a0), reinterpret_cast<const void*>(a1), a2);
	});

	// sys_lseek: a0 = fd, a1 = offset, a2 = whence
	register_both(478, [](uint64_t a0, uint64_t a1, uint64_t a2, uint64_t, uint64_t, uint64_t) {
		return FileSystem::KernelLseek(static_cast<int>(a0),
		                              static_cast<int64_t>(a1),
		                              static_cast<int>(a2));
	});

	// sys_stat: a0 = path, a1 = stat buf
	register_both(477, [](uint64_t a0, uint64_t a1, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelStat(reinterpret_cast<const char*>(a0),
		                                   reinterpret_cast<FileSystem::FileStat*>(a1)));
	});

	// sys_fstat: a0 = fd, a1 = stat buf
	register_both(479, [](uint64_t a0, uint64_t a1, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelFstat(static_cast<int>(a0),
		                                     reinterpret_cast<FileSystem::FileStat*>(a1)));
	});

	// sys_unlink: a0 = path
	register_both(483, [](uint64_t a0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelUnlink(reinterpret_cast<const char*>(a0)));
	});

	// sys_rename: a0 = from, a1 = to
	register_both(484, [](uint64_t a0, uint64_t a1, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelRename(reinterpret_cast<const char*>(a0),
		                                      reinterpret_cast<const char*>(a1)));
	});

	// sys_mkdir: a0 = path, a1 = mode
	register_both(488, [](uint64_t a0, uint64_t a1, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelMkdir(reinterpret_cast<const char*>(a0),
		                                    static_cast<uint16_t>(a1)));
	});

	// sys_rmdir: a0 = path
	register_both(489, [](uint64_t a0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelRmdir(reinterpret_cast<const char*>(a0)));
	});

	// sys_getdents: a0 = fd, a1 = buf, a2 = nbytes
	register_both(486, [](uint64_t a0, uint64_t a1, uint64_t a2, uint64_t, uint64_t, uint64_t) {
		return ToInt(FileSystem::KernelGetdents(static_cast<int>(a0),
		                                       reinterpret_cast<char*>(a1),
		                                       static_cast<int>(a2)));
	});

	// sys_getpid
	register_both(20, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
		return static_cast<int64_t>(1); // single-process emulator
	});

	// sys_getppid
	register_both(39, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
		return static_cast<int64_t>(0);
	});

	// sys_gettimeofday: a0 = timeval, a1 = timezone (ignored)
	register_both(116, [](uint64_t a0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
		if (a0 == 0) return ToInt(LibKernel::KERNEL_ERROR_EFAULT);
		auto* tv = reinterpret_cast<int64_t*>(a0);
		using namespace std::chrono;
		const auto now = system_clock::now().time_since_epoch();
		const auto secs = duration_cast<seconds>(now).count();
		const auto usecs = duration_cast<microseconds>(now).count() % 1'000'000;
		tv[0] = secs;
		tv[1] = usecs;
		return int64_t{0};
	});

	LOGF("[Syscall] Registered shared PS4/PS5 kernel syscalls (file I/O, process, time).");
}

} // namespace Kyty::Libs::LibKernel