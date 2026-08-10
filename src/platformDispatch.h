#pragma once

// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Platform dispatch seam.
//
// A PS4 (Orbis) eboot and a PS5 (Prospero) eboot share the same SELF/ELF
// container format, but differ in ABI version and — critically — in the
// entire runtime below the loader: PS4 is GCN + Orbis kernel, PS5 is
// RDNA2/AGC + Prospero kernel. Kyty's native path targets PS5 only.
//
// Rather than re-derive a full PS4 runtime (GCN->IR->SPIR-V, PM4->Vulkan,
// Orbis kernel HLE, GPU state), the PS4 case is delegated to shadPS4,
// which already implements that entire stack. This header is the single
// routing point: detect the guest platform from the eboot and dispatch
// to the appropriate backend.
//
// Two backend modes are supported:
//   Mode::Subprocess  - launch an external shadPS4 binary on the eboot.
//                       Works immediately, requires a built shadps4.exe
//                       discoverable via the SHADPS4_BIN env var or the
//                       --shadps4-bin option, or a sibling install dir.
//   Mode::InProcess   - call the shadPS4 runtime as a linked library via
//                       the shadps4_runtime_* C ABI (see Shadps4Runtime.h).
//                       Requires building shadPS4 as a lib and resolving
//                       its symbol clashes (LOG_INFO/Singleton/ASSERT)
//                       via a namespace-isolation build; not the default.
//
// Only one backend is ever active per process. The dispatch point is
// reached before Kyty initializes its own Vulkan/memory subsystems, so
// no PS5 resources are wasted on a PS4 title (and vice versa).

#include "common/common.h"

#include <filesystem>
#include <string>

namespace Emulator::PlatformDispatch {

	// Mirrors Loader::Platform so this seam does not pull in the full ELF
	// parser. Keep in sync with loader/elf.h.
	enum class GuestPlatform : uint8_t {
		Unknown = 0,
		Ps4     = 1, // Orbis  (EI_ABIVVERSION == 0)
		Ps5     = 2, // Prospero (EI_ABIVVERSION == 2)
	};

	enum class BackendMode : uint8_t {
		Subprocess = 0,
		InProcess  = 1,
	};

	struct DispatchResult {
		bool        delegated  = false; // true => handled by shadPS4, Kyty must NOT continue
		int         exit_code  = 0;     // meaningful when delegated == true
		std::string message;            // diagnostic on failure
	};

	// Resolve the eboot path from Kyty's RunOptions layout. app0_dir is the
	// host directory; elf is the guest-style path (e.g. "/app0/eboot.bin").
	// Returns the absolute host path to the eboot file, or empty on failure.
	std::filesystem::path ResolveEbootHostPath(const std::filesystem::path& app0_dir,
	                                           const std::filesystem::path& elf);

	// Read EI_ABIVERSION (ELF header byte 7) directly from the eboot file
	// without constructing a full Elf64. Returns GuestPlatform::Unknown if
	// the file cannot be opened or is not a valid SELF/ELF.
	//
	// SELF files begin with the 4-byte SELF magic (0x53 0x45 0x4C 0x46 =
	// "SELF"); the embedded ELF header follows at a fixed offset. For raw
	// ELF files the ELF magic (0x7F 'E' 'L' 'F') is at offset 0. This
	// helper handles both so detection works on decrypted dumps and raw
	// ELFs alike.
	GuestPlatform DetectPlatform(const std::filesystem::path& eboot_host_path);

	// Locate a usable shadPS4 binary for subprocess delegation. Search
	// order: the explicit `bin_override` argument, the SHADPS4_BIN env
	// var, then a set of sibling install locations. Returns empty if none
	// found.
	std::filesystem::path FindShadps4Binary(const std::string& bin_override);

	// Delegate a PS4 title to shadPS4. In Subprocess mode this execs the
	// resolved binary and blocks until it exits. In InProcess mode it
	// calls the linked shadps4_runtime_run entry (if available). On any
	// failure, delegated is false and message describes the problem so
	// the caller can fall back / report cleanly.
	DispatchResult DispatchToShadps4(const std::filesystem::path& eboot_host_path,
	                                 BackendMode                   mode,
	                                 const std::filesystem::path&  shadps4_bin);

} // namespace Emulator::PlatformDispatch
