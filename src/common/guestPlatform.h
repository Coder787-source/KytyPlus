#ifndef EMULATOR_INCLUDE_EMULATOR_COMMON_GUESTPLATFORM_H_
#define EMULATOR_INCLUDE_EMULATOR_COMMON_GUESTPLATFORM_H_

#include "common/common.h"

#include <atomic>
#include <cstdint>

namespace Kyty::Common {

// ---------------------------------------------------------------------------
// Active guest platform accessor.
//
// Platform detection (PS4 vs PS5) lives in the ELF loader (loader/elf.cpp:
// Elf64::GetPlatform via EI_ABIVERSION). The GPU and shader recompiler paths
// are invoked far from the loader and have no direct handle to the loaded
// program, so they read the active platform here.
//
// Set once by the runtime linker when the main eboot is loaded, before any
// GPU work begins. Reading it before it is set returns Ps5 (the project's
// native/default target) so existing PS5-only behavior is unchanged.
// ---------------------------------------------------------------------------

enum class GuestPlatform : uint8_t {
	Unknown = 0,
	Ps4     = 1, // Orbis    — GCN (GFX8/GFX9)
	Ps5     = 2, // Prospero — RDNA2 / AGC
};

class GuestPlatformState {
public:
	static GuestPlatformState& Instance() {
		static GuestPlatformState state;
		return state;
	}

	void Set(GuestPlatform platform) {
		platform_.store(platform, std::memory_order_release);
	}

	[[nodiscard]] GuestPlatform Get() const {
		return platform_.load(std::memory_order_acquire);
	}

	[[nodiscard]] bool IsPs4() const { return Get() == GuestPlatform::Ps4; }
	[[nodiscard]] bool IsPs5() const { return Get() == GuestPlatform::Ps5; }

private:
	GuestPlatformState() = default;
	std::atomic<GuestPlatform> platform_{GuestPlatform::Ps5};
};

} // namespace Kyty::Common

#endif /* EMULATOR_INCLUDE_EMULATOR_COMMON_GUESTPLATFORM_H_ */
