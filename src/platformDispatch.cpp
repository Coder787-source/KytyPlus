// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "platformDispatch.h"

#include "common/common.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "shadps4Embedder.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL.h>

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace Emulator::PlatformDispatch {

namespace {

// SELF magic "SELF"
constexpr std::array<uint8_t, 4> kSelfMagic = {0x53, 0x45, 0x4C, 0x46};
// ELF magic \x7FELF
constexpr std::array<uint8_t, 4> kElfMagic = {0x7F, 0x45, 0x4C, 0x46};

// In a SELF file the ELF header is embedded at a fixed offset. shadPS4 /
// the Orbis SELF layout place the ELF header immediately after the SELF
// header. The SELF header is 0x20 bytes in the minimal form used by
// decrypted dumps; we scan a small window to find the ELF magic rather
// than hard-coding, which is robust across SELF variants.
constexpr size_t kSelfScanWindow = 4096;

size_t FindElfHeaderOffset(const std::vector<uint8_t>& buf) {
	if (buf.size() < 4) {
		return SIZE_MAX;
	}
	if (std::equal(kElfMagic.begin(), kElfMagic.end(), buf.begin())) {
		return 0;
	}
	const size_t limit = std::min(buf.size(), kSelfScanWindow);
	for (size_t i = 4; i + 4 <= limit; ++i) {
		if (std::equal(kElfMagic.begin(), kElfMagic.end(), buf.begin() + i)) {
			return i;
		}
	}
	return SIZE_MAX;
}

} // namespace

std::filesystem::path ResolveEbootHostPath(const std::filesystem::path& app0_dir,
                                           const std::filesystem::path& elf) {
	std::string guest = elf.generic_string();
	const std::string prefix = "/app0/";
	std::string rel;
	if (guest.rfind(prefix, 0) == 0) {
		rel = guest.substr(prefix.size());
	} else {
		rel = guest;
	}
	if (rel.empty()) {
		rel = "eboot.bin";
	}
	auto out = app0_dir / rel;
	return out.lexically_normal();
}

GuestPlatform DetectPlatform(const std::filesystem::path& eboot_host_path) {
	if (eboot_host_path.empty() || !Common::File::IsFileExisting(eboot_host_path)) {
		return GuestPlatform::Unknown;
	}

	std::ifstream f(eboot_host_path, std::ios::binary);
	if (!f) {
		return GuestPlatform::Unknown;
	}

	std::vector<uint8_t> buf(std::min<std::streamoff>(
	    static_cast<std::streamoff>(kSelfScanWindow),
	    static_cast<std::streamoff>(
	        std::filesystem::file_size(eboot_host_path))));
	if (buf.empty()) {
		return GuestPlatform::Unknown;
	}
	f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
	const auto got = static_cast<size_t>(f.gcount());
	buf.resize(got);

	const size_t elf_off = FindElfHeaderOffset(buf);
	if (elf_off == SIZE_MAX) {
		return GuestPlatform::Unknown;
	}
	if (elf_off + 16 > buf.size()) {
		return GuestPlatform::Unknown;
	}

	const uint8_t abi_version = buf[elf_off + 7];
	switch (abi_version) {
		case 0:  return GuestPlatform::Ps4;
		case 2:  return GuestPlatform::Ps5;
		default: return GuestPlatform::Unknown;
	}
}

namespace {
// Resolve a path relative to the kyty_emulator executable directory so the
// BUNDLED shadps4 binary (copied next to it by shadps4_bundle.cmake) is
// found regardless of the user's working directory.
std::filesystem::path ModuleDir() {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	char buf[MAX_PATH] = {0};
	if (GetModuleFileNameA(nullptr, buf, MAX_PATH) > 0) {
		return std::filesystem::path(buf).parent_path();
	}
#endif
	return std::filesystem::current_path();
}
} // namespace

std::filesystem::path FindShadps4Binary(const std::string& bin_override) {
	auto check = [](const std::string& p) -> std::filesystem::path {
		if (p.empty()) {
			return {};
		}
		std::filesystem::path candidate(p);
		if (std::filesystem::exists(candidate) && !std::filesystem::is_directory(candidate)) {
			return candidate.lexically_normal();
		}
		return {};
	};

	if (!bin_override.empty()) {
		if (auto p = check(bin_override); !p.empty()) {
			return p;
		}
	}

	if (const char* env = std::getenv("SHADPS4_BIN")) {
		if (auto p = check(env); !p.empty()) {
			return p;
		}
	}

	// Prefer the bundled binary next to kyty_emulator (built by
	// shadps4_bundle.cmake when KYTY_BUNDLE_SHADPS4=ON).
	const auto mod_dir = ModuleDir();
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	for (const auto* name : {"shadps4.exe", "shadPS4.exe"}) {
		auto p = mod_dir / name;
		if (std::filesystem::exists(p)) {
			return p.lexically_normal();
		}
	}
#else
	{
		auto p = mod_dir / "shadps4";
		if (std::filesystem::exists(p)) {
			return p.lexically_normal();
		}
	}
#endif

	const std::vector<std::string> candidates = {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	    "shadps4.exe",
	    "shadPS4.exe",
	    "../shadps4.exe",
	    "../shadPS4.exe",
	    "shadps4/shadps4.exe",
	    "../shadps4/shadps4.exe",
#else
	    "shadps4",
	    "../shadps4",
	    "shadps4/shadps4",
#endif
	};

	for (const auto& c : candidates) {
		if (auto p = check(c); !p.empty()) {
			return p;
		}
	}
	return {};
}

DispatchResult DispatchToShadps4(const std::filesystem::path& eboot_host_path,
                                 BackendMode                   mode,
                                 const std::filesystem::path&  shadps4_bin) {
	DispatchResult result;

	if (eboot_host_path.empty() || !std::filesystem::exists(eboot_host_path)) {
		result.message = "PS4 dispatch: eboot not found: " + eboot_host_path.string();
		return result;
	}

	if (mode == BackendMode::InProcess) {
#ifdef KYTY_ENABLE_INPROCESS_SHADPS4
		// Linked-library path. shadPS4 must be built as a namespace-isolated
		// lib exporting this C ABI (see shadps4Embedder.h). Symbol clashes
		// (LOG_INFO / Common::Singleton / ASSERT) are resolved by the
		// isolation build, not here.
		auto user_dir = Emulator::Shadps4Integration::ResolveSharedUserDir();
		const int rc_init = ::shadps4_runtime_init();
		if (rc_init != 0) {
			result.message = "PS4 dispatch: shadps4_runtime_init failed (" +
			                 std::to_string(rc_init) + ")";
			return result;
		}
		result.delegated = true;
		result.exit_code =
		    ::shadps4_runtime_run(eboot_host_path.string().c_str(), user_dir.string().c_str());
		::shadps4_runtime_shutdown();
		return result;
#else
		result.message = "PS4 dispatch: in-process backend not enabled (build with "
		                 "KYTY_ENABLE_INPROCESS_SHADPS4 and link the shadPS4 isolation lib)";
		return result;
#endif
	}

	// --- Subprocess mode (embed-aware on Windows) ---
	//
	// Kyty's own host window is created deep inside Run(), which a PS4 title
	// never reaches (we return from here with the child's exit code). So for
	// the embedded experience the dispatcher creates its own minimal SDL host
	// window, launches shadPS4, reparents shadPS4's window into it, and runs a
	// tiny event loop forwarding resize/close until the child exits. This gives
	// a unified single-window PS4 experience without refactoring shadPS4 into
	// a lib. On non-Windows the child keeps its own window (reparenting is
	// fragile there).

	std::filesystem::path bin = shadps4_bin;
	if (bin.empty()) {
		bin = FindShadps4Binary({});
	}
	if (bin.empty()) {
		result.message =
		    "PS4 dispatch: shadPS4 binary not found. Set SHADPS4_BIN or pass --shadps4-bin. "
		    "Download from https://github.com/shadps4-emu/shadPS4/releases";
		return result;
	}

	LOGF("PlatformDispatch: PS4 title detected, delegating to shadPS4: %s (eboot=%s)",
	     bin.string().c_str(), eboot_host_path.string().c_str());

	const std::string eboot_str = eboot_host_path.string();

	// Resolve a shared user dir so saves/settings/caches are unified between
	// the Kyty install and the embedded shadPS4. shadPS4's portable-dir logic
	// adopts a `user/` dir next to its working directory, so we launch it with
	// cwd set to a parent that already contains the unified `user/` tree.
	auto shared_user = Emulator::Shadps4Integration::ResolveSharedUserDir();
	auto shad_parent = shared_user.parent_path();

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	std::string cmd = std::string("\"") + bin.string() + "\" \"" + eboot_str + "\"";

	STARTUPINFOA si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};

	std::vector<char> cmd_buf(cmd.begin(), cmd.end());
	cmd_buf.push_back('\0');

	// Create the host window BEFORE launching so the child can be reparented.
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* host = SDL_CreateWindow("KytyPlus - PS4 (shadPS4)",
	                                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                     1280, 720, SDL_WINDOW_RESIZABLE);
	HWND host_hwnd = nullptr;
	if (host) {
		SDL_SysWMinfo wm{};
		SDL_VERSION(&wm.version);
		if (SDL_GetWindowWMInfo(host, &wm) && wm.subsystem == SDL_SYSWM_WINDOWS) {
			host_hwnd = wm.info.win.window;
		}
	}

	std::string cwd_w = shad_parent.string();
	if (!CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, 0, nullptr,
	                     cwd_w.empty() ? nullptr : cwd_w.c_str(), &si, &pi)) {
		result.message = "PS4 dispatch: CreateProcess failed for shadPS4 (err=" +
		                 std::to_string(GetLastError()) + ")";
		if (host) {
			SDL_DestroyWindow(host);
		}
		SDL_Quit();
		return result;
	}

	// Reparent the child window into the host (best-effort; on failure the
	// child keeps its own standalone window, which is still functional).
	if (host_hwnd) {
		Emulator::Shadps4Integration::EmbedOptions eo;
		eo.embed_enabled = true;
		eo.timeout_ms    = 30000;
		auto er = Emulator::Shadps4Integration::EmbedChildWindow(
		    reinterpret_cast<Emulator::Shadps4Integration::NativeWindowHandle*>(host_hwnd),
		    static_cast<Emulator::Shadps4Integration::NativePid>(pi.dwProcessId), eo);
		LOGF("PlatformDispatch: embed result: %s (%s)", er.embedded ? "embedded" : "standalone",
		     er.message.c_str());
	}

	// Drive a minimal event loop until the child exits, forwarding resize/close.
	for (;;) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
				int w = 0, h = 0;
				SDL_GetWindowSize(host, &w, &h);
				Emulator::Shadps4Integration::ResizeEmbeddedChild(
				    reinterpret_cast<Emulator::Shadps4Integration::NativeWindowHandle*>(host_hwnd),
				    static_cast<uint32_t>(w), static_cast<uint32_t>(h));
			} else if (ev.type == SDL_QUIT || (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)) {
				Emulator::Shadps4Integration::CloseEmbeddedChild();
			}
		}
		if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) {
			break;
		}
	}

	DWORD code = 0;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	if (host) {
		SDL_DestroyWindow(host);
	}
	SDL_Quit();

	result.delegated = true;
	result.exit_code = static_cast<int>(code);
	return result;
#else
	// Non-Windows: launch with the shared user dir as cwd, no embedding.
	pid_t pid = fork();
	if (pid < 0) {
		result.message = "PS4 dispatch: fork failed";
		return result;
	}
	if (pid == 0) {
		if (!shad_parent.empty()) {
			(void)chdir(shad_parent.string().c_str());
		}
		execl(bin.c_str(), bin.c_str(), eboot_str.c_str(), static_cast<char*>(nullptr));
		_exit(127);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	result.delegated = true;
	result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	return result;
#endif
}

} // namespace Emulator::PlatformDispatch