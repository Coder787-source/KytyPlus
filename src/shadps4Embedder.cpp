// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4Embedder.h"

#include "common/common.h"
#include "common/file.h"
#include "common/logging/log.h"

#include <thread>
#include <chrono>

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace Emulator::Shadps4Integration {

// ---------------------------------------------------------------------------
// Save / settings unification
// ---------------------------------------------------------------------------

std::string Shadps4UserDirEnvName() {
	// shadPS4's portable-dir logic keys off current_path()/PORTABLE_DIR.
	// We communicate the shared user dir by launching the subprocess with
	// its working directory set to the shared parent (handled in the
	// dispatcher). This env name is reserved for any future shadPS4
	// override hook that reads it directly.
	return "SHADPS4_USER_DIR";
}

std::filesystem::path ResolveSharedUserDir() {
	// Co-locate shadPS4's user/ tree with Kyty's own data so saves,
	// settings, shader caches, and game data are unified. Kyty keeps its
	// data next to the executable; mirror that.
	std::filesystem::path base;
#ifdef _WIN32
	char module_path[MAX_PATH] = {0};
	if (GetModuleFileNameA(nullptr, module_path, MAX_PATH) > 0) {
		base = std::filesystem::path(module_path).parent_path();
	} else {
		base = std::filesystem::current_path();
	}
#else
	base = std::filesystem::current_path();
#endif

	auto user_dir = base / "user";
	std::error_code ec;
	std::filesystem::create_directories(user_dir, ec);

	// Pre-create the subdirs shadPS4 expects so its first run does not
	// scatter them unpredictably. Names mirror shadPS4's path_util.cpp.
	const char* subdirs[] = {"savedata", "games",    "shader",  "cache",
	                         "logs",     "screenshots", "trophy",  "config"};
	for (const auto* s : subdirs) {
		std::filesystem::create_directories(user_dir / s, ec);
	}
	return user_dir.lexically_normal();
}

// ---------------------------------------------------------------------------
// Window embedding (Windows only; no-op elsewhere)
// ---------------------------------------------------------------------------

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS

// File-scope handle to the reparented shadPS4 window, so resize/close can
// address it without re-enumerating.
HWND g_embedded_child = nullptr;

namespace {

struct EnumCtx {
	DWORD       pid       = 0;
	HWND        found     = nullptr;
	std::string title_hint;
};

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
	auto* ctx = reinterpret_cast<EnumCtx*>(lParam);
	DWORD wpid = 0;
	GetWindowThreadProcessId(hwnd, &wpid);
	if (wpid != ctx->pid) {
		return TRUE;
	}
	// Skip invisible / message-only transient windows shadPS4 may create.
	if (!IsWindowVisible(hwnd)) {
		return TRUE;
	}
	char title[512] = {0};
	GetWindowTextA(hwnd, title, sizeof(title));
	if (title[0] == '\0') {
		return TRUE; // not ready yet — keep waiting for the SDL window
	}
	// If a title hint is supplied, prefer a window whose title contains it;
	// otherwise accept the first visible titled top-level window for the PID.
	if (!ctx->title_hint.empty()) {
		std::string t(title);
		if (t.find(ctx->title_hint) == std::string::npos) {
			return TRUE; // not our window yet — keep looking
		}
	}
	ctx->found = hwnd;
	return FALSE; // stop enumerating
}

HWND FindChildWindowByPid(DWORD pid, const std::string& title_hint) {
	EnumCtx ctx{pid, nullptr, title_hint};
	EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));
	return ctx.found;
}

void RemoveDecorationsAndReparent(HWND child, HWND host) {
	// Strip shadPS4's frame/borders so it looks native inside Kyty, then
	// reparent. We do not need to restore later (the child process owns the
	// window lifetime and exits with the game).
	LONG_PTR style   = GetWindowLongPtrW(child, GWL_STYLE);
	LONG_PTR exstyle = GetWindowLongPtrW(child, GWL_EXSTYLE);
	style   &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
	             WS_MAXIMIZEBOX | WS_SYSMENU);
	exstyle &= ~(WS_EX_APPWINDOW);
	SetWindowLongPtrW(child, GWL_STYLE, style);
	SetWindowLongPtrW(child, GWL_EXSTYLE, exstyle);
	SetParent(child, host);
}

} // namespace

EmbedResult EmbedChildWindow(NativeWindowHandle* host_window, NativePid child_pid,
                             const EmbedOptions& opts) {
	EmbedResult r;
	r.child_pid = child_pid;
	if (!opts.embed_enabled || host_window == nullptr || child_pid == 0) {
		r.embedded = false;
		r.message  = "embedding disabled or missing host window";
		return r;
	}

	HWND host  = reinterpret_cast<HWND>(host_window);
	HWND child = nullptr;

	const auto deadline =
	    std::chrono::steady_clock::now() + std::chrono::milliseconds(opts.timeout_ms);
	while (std::chrono::steady_clock::now() < deadline) {
		child = FindChildWindowByPid(static_cast<DWORD>(child_pid), opts.window_title_hint);
		if (child) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (!child) {
		r.embedded = false;
		r.message  = "timed out waiting for shadPS4 window (PID " +
		             std::to_string(child_pid) + ")";
		return r;
	}

	RemoveDecorationsAndReparent(child, host);

	// Size the child to the host's client area.
	RECT rc{};
	if (GetClientRect(host, &rc)) {
		SetWindowPos(child, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
		             SWP_NOZORDER | SWP_SHOWWINDOW);
	}

	g_embedded_child = child;
	r.embedded = true;
	r.message  = "embedded";
	LOGF("Shadps4Embedder: reparented shadPS4 window (PID %lu) into host window",
	     static_cast<unsigned long>(child_pid));
	return r;
}

void ResizeEmbeddedChild(NativeWindowHandle* host_window, uint32_t width, uint32_t height) {
	if (!host_window) {
		return;
	}
	HWND host  = reinterpret_cast<HWND>(host_window);
	HWND child = FindWindowExW(host, nullptr, nullptr, nullptr);
	if (child) {
		SetWindowPos(child, nullptr, 0, 0, static_cast<int>(width),
		             static_cast<int>(height), SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

void CloseEmbeddedChild() {
	// WM_CLOSE lets shadPS4 shut down cleanly through its own SDL event loop
	// rather than being killed mid-frame.
	if (g_embedded_child != nullptr) {
		PostMessageW(g_embedded_child, WM_CLOSE, 0, 0);
		g_embedded_child = nullptr;
	}
}

#else // non-Windows: reparenting is fragile on Wayland/X11, so we no-op
       // and let the subprocess keep its own window.

EmbedResult EmbedChildWindow(NativeWindowHandle* /*host_window*/, NativePid child_pid,
                             const EmbedOptions& /*opts*/) {
	EmbedResult r;
	r.child_pid = child_pid;
	r.embedded  = false;
	r.message   = "window embedding is only supported on Windows";
	return r;
}

void ResizeEmbeddedChild(NativeWindowHandle*, uint32_t, uint32_t) {}
void CloseEmbeddedChild() {}

#endif

} // namespace Emulator::Shadps4Integration
