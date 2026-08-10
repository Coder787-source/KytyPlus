#pragma once

// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Shadps4Embedder — turns subprocess delegation into a unified-window
// experience on Windows by reparenting the shadPS4 SDL window into Kyty's
// own window after launch. This addresses the "separate process / separate
// window" limitation of plain delegation WITHOUT requiring shadPS4 to be
// refactored into a library.
//
// Mechanism (Windows):
//   shadPS4 creates its SDL window with the title of the game. After
//   CreateProcess returns a child process handle, we wait for that process
//   to produce a top-level window, then reparent it into Kyty's host
//   window via SetParent + style changes so it appears as a child surface.
//   Kyty then forwards resize/close to the embedded window. Input still
//   flows directly to the embedded window, so shadPS4's own input layer
//   works unchanged.
//
// This is the same technique used by "embedded" frontends for other
// process-isolated engines. It is inherently best-effort: SDL3 windows
// reparent reasonably on Windows; on Wayland/X11 reparenting is fragile
// and not attempted (the subprocess keeps its own window there).
//
// Also provides:
//   - Shadps4SaveBridge: points shadPS4 at a shared user/ directory so
//     save data, settings, and shader caches are unified between a Kyty
//     install and the embedded shadPS4, instead of each maintaining its
//     own isolated user dir.
//   - InProcessLinkage: the build-system seam (macros + forward decls)
//     for the eventual in-process lib path, so the dispatch code can
//     compile in either mode from the same source.

#include "common/common.h"

#include <filesystem>
#include <string>
#include <cstdint>

namespace Emulator::Shadps4Integration {

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	using NativeWindowHandle = struct HWND__; // forward: HWND is HWND__*
	using NativePid          = unsigned long; // DWORD
#else
	using NativeWindowHandle = void;
	using NativePid          = int;
#endif

	// ---- Window embedding -------------------------------------------------

	struct EmbedOptions {
		bool        embed_enabled = true; // disable to keep the subprocess window standalone
		uint32_t    timeout_ms    = 30000; // how long to wait for shadPS4 to create its window
		std::string window_title_hint;     // optional: substring to match shadPS4's window title
	};

	struct EmbedResult {
		bool        embedded  = false;
		NativePid   child_pid = 0;
		std::string message;
	};

	// Reparent the launched shadPS4 process's window into `host_window`.
	// `child_pid` is the PID from the subprocess launch. On non-Windows or
	// when disabled, returns embedded=false (the caller simply lets the
	// subprocess keep its own window).
	EmbedResult EmbedChildWindow(NativeWindowHandle* host_window, NativePid child_pid,
	                             const EmbedOptions& opts);

	// Forward a host resize to the embedded child (no-op if not embedded).
	void ResizeEmbeddedChild(NativeWindowHandle* host_window, uint32_t width,
	                         uint32_t height);

	// Request the embedded child to close (no-op if not embedded).
	void CloseEmbeddedChild();

	// ---- Save / settings unification --------------------------------------

	// Computes the user/ directory shadPS4 should use so its saves, settings,
	// and caches live alongside Kyty's. Returns the absolute path that both
	// the subprocess (via an env hint) and Kyty agree on. Creates it if
	// missing so shadPS4's portable-dir logic adopts it.
	std::filesystem::path ResolveSharedUserDir();

	// The env var shadPS4 reads (via its portable-dir logic) to override its
	// user directory. Set this in the child's environment before launch.
	std::string Shadps4UserDirEnvName();

	// ---- In-process linkage seam (forward declarations only) -------------
	// Implemented when shadPS4 is built as a namespace-isolated library and
	// KYTY_ENABLE_INPROCESS_SHADPS4 is defined at configure time. Keeping the
	// declarations here means DispatchToShadps4() compiles in either mode.
#ifdef KYTY_ENABLE_INPROCESS_SHADPS4
	extern "C" {
	// shadps4_runtime_init / run / shutdown — the C ABI the in-process lib
	// must expose. Symbols are namespace-mangled away by shadPS4's isolation
	// build so they do not clash with Kyty's LOG_INFO / Singleton / ASSERT.
	int shadps4_runtime_init(void);
	int shadps4_runtime_run(const char* eboot_path, const char* user_dir);
	void shadps4_runtime_shutdown(void);
	}
#endif

} // namespace Emulator::Shadps4Integration
