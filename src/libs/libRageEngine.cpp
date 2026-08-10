// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// HLE bridge for the RAGE engine subsystems (scripting VM, physics, streaming).
//
// Registers host-side implementations of the RAGE SPRX module functions so
// the guest game's script manager, physics system, and streaming manager
// route through our C++ implementations instead of executing guest code.
//
// The NIDs below are based on community-reverse-engineered RAGE module
// exports for GTA V (PS4/PS5 era). They may need adjustment for specific
// game builds.

#include "common/abi.h"
#include "common/common.h"
#include "common/logging/log.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "libs/rageScripting.h"
#include "libs/physicsCollision.h"
#include "libs/textureStreaming.h"
#include "libs/rpfArchiveReader.h"
#include "loader/symbolDatabase.h"

#include <atomic>
#include <cstring>

namespace Libs {

LIB_VERSION("RageEngine", 1, "RageEngine", 1, 0);

namespace RageEngine {

// ─── Module state ────────────────────────────────────────────────────────────

static std::atomic<bool> g_rage_initialized {false};

// ─── Initialization / shutdown ───────────────────────────────────────────────

// rageScriptInit — called by the game's engine startup code to initialize
// the RAGE scripting VM. Returns 0 on success.
static int KYTY_SYSV_ABI rageScriptInit() {
	PRINT_NAME();
	if (!g_rage_initialized.exchange(true)) {
		RageScripting::Initialize();
		PhysicsCollision::Initialize();
		TextureStreaming::Initialize(); // default 4 GiB budget
		LOGF("[RageBridge] INFO: All RAGE subsystems initialized\n");
	}
	return OK;
}

// rageScriptShutdown — called when the game shuts down.
static int KYTY_SYSV_ABI rageScriptShutdown() {
	PRINT_NAME();
	if (g_rage_initialized.exchange(false)) {
		TextureStreaming::Shutdown();
		PhysicsCollision::Shutdown();
		RageScripting::Shutdown();
		LOGF("[RageBridge] INFO: All RAGE subsystems shut down\n");
	}
	return OK;
}

// ─── Script management ───────────────────────────────────────────────────────

// rageScriptLoadProgram — load a .ysc bytecode blob.
// Args: (const void* bytecode, uint32_t size, const char* name)
static int KYTY_SYSV_ABI rageScriptLoadProgram(const void* bytecode, uint32_t size,
                                                const char* name) {
	PRINT_NAME();
	uint32_t handle = RageScripting::LoadProgram(
	    static_cast<const uint8_t*>(bytecode), size, name);
	return (handle != 0) ? static_cast<int>(handle) : -1;
}

// rageScriptStartThread — start a new script thread.
// Args: (uint32_t program, const char* name, uint32_t stack_size)
static int KYTY_SYSV_ABI rageScriptStartThread(uint32_t program, const char* name,
                                                uint32_t stack_size) {
	PRINT_NAME();
	uint32_t tid = RageScripting::StartThread(program, name, stack_size);
	return (tid != 0) ? static_cast<int>(tid) : -1;
}

// rageScriptKillThread — kill a running script thread.
static int KYTY_SYSV_ABI rageScriptKillThread(uint32_t thread_id) {
	PRINT_NAME();
	RageScripting::KillThread(thread_id);
	return OK;
}

// rageScriptTick — advance all running script threads.
// Args: (float dt_ms)
static int KYTY_SYSV_ABI rageScriptTick(float dt_ms) {
	PRINT_NAME();
	RageScripting::TickScripts(dt_ms);
	TextureStreaming::ProcessQueue();
	return OK;
}

// rageScriptGetThreadState — query a thread's state.
static int KYTY_SYSV_ABI rageScriptGetThreadState(uint32_t thread_id) {
	PRINT_NAME();
	return static_cast<int>(RageScripting::GetThreadState(thread_id));
}

// rageScriptRegisterNative — register a native function hash.
static int KYTY_SYSV_ABI rageScriptRegisterNative(uint64_t hash, const char* name) {
	PRINT_NAME();
	RageScripting::RegisterNative(hash, name, nullptr);
	return OK;
}

// ─── RPF archive access ─────────────────────────────────────────────────────

// rageOpenArchive — open an RPF archive for reading.
static int KYTY_SYSV_ABI rageOpenArchive(const char* path) {
	PRINT_NAME();
	auto* archive = RpfArchive::OpenArchive(path);
	if (!archive) return -1;
	// Return the pointer as an opaque handle (cast to int).
	// In practice, the guest uses a 64-bit handle.
	return static_cast<int>(reinterpret_cast<intptr_t>(archive));
}

// rageReadFile — read a file from an opened archive.
static int KYTY_SYSV_ABI rageReadFile(int archive_handle, const char* name,
                                       void* out_buf, uint32_t buf_size,
                                       uint32_t* bytes_read) {
	PRINT_NAME();
	auto* archive = reinterpret_cast<RpfArchive::RpfArchive*>(
	    static_cast<intptr_t>(archive_handle));
	auto file_data = RpfArchive::ReadFile(archive, name);
	if (!file_data.ok) {
		if (bytes_read) *bytes_read = 0;
		return -1;
	}
	uint32_t copy_size = std::min(buf_size, static_cast<uint32_t>(file_data.data.size()));
	if (out_buf && copy_size > 0) {
		std::memcpy(out_buf, file_data.data.data(), copy_size);
	}
	if (bytes_read) *bytes_read = copy_size;
	return OK;
}

// rageCloseArchive — close an archive handle.
static int KYTY_SYSV_ABI rageCloseArchive(int archive_handle) {
	PRINT_NAME();
	auto* archive = reinterpret_cast<RpfArchive::RpfArchive*>(
	    static_cast<intptr_t>(archive_handle));
	RpfArchive::CloseArchive(archive);
	return OK;
}

// ─── Physics bridge ──────────────────────────────────────────────────────────

// ragePhysicsTick — step the physics simulation.
// Uses world 0 (default world) as the simulation target.
static int KYTY_SYSV_ABI ragePhysicsTick(float dt) {
	PRINT_NAME();
	PhysicsCollision::StepSimulation(0, dt);
	return OK;
}

// ragePhysicsRaycast — cast a ray between two points and return hit info.
static int KYTY_SYSV_ABI ragePhysicsRaycast(const float* from, const float* to,
                                             float* hit_point, float* hit_normal,
                                             float* hit_dist) {
	PRINT_NAME();
	PhysicsCollision::Vec3 v_from {from[0], from[1], from[2]};
	PhysicsCollision::Vec3 v_to   {to[0],   to[1],   to[2]};
	PhysicsCollision::RayHit hit = PhysicsCollision::Raycast(0, v_from, v_to);
	if (hit.hit) {
		if (hit_point)  { hit_point[0]  = hit.hit_point.x;  hit_point[1]  = hit.hit_point.y;  hit_point[2]  = hit.hit_point.z; }
		if (hit_normal) { hit_normal[0] = hit.hit_normal.x; hit_normal[1] = hit.hit_normal.y; hit_normal[2] = hit.hit_normal.z; }
		if (hit_dist)   *hit_dist = hit.distance;
		return 1;
	}
	return 0;
}

// ─── Streaming bridge ────────────────────────────────────────────────────────

// rageStreamRequestTexture — request a texture be streamed in.
// Args: (uint32_t texture_hash, uint32_t mip_level, uint32_t priority, float distance)
static int KYTY_SYSV_ABI rageStreamRequestTexture(uint32_t texture_hash, uint32_t mip_level,
                                                   uint32_t priority, float distance) {
	PRINT_NAME();
	auto prio = static_cast<TextureStreaming::StreamRequestPriority>(priority);
	TextureStreaming::RequestTexture(texture_hash, mip_level, prio, distance);
	return OK;
}

// rageStreamGetResidentMip — check which mip level is resident for a texture.
static int KYTY_SYSV_ABI rageStreamGetResidentMip(uint32_t texture_hash) {
	PRINT_NAME();
	return static_cast<int>(TextureStreaming::GetResidentMip(texture_hash));
}

// ─── Registration ────────────────────────────────────────────────────────────

LIB_DEFINE(InitRageEngine_1) {
	LIB_FUNC("rSI0init000", rageScriptInit);
	LIB_FUNC("rSI0shut000", rageScriptShutdown);
	LIB_FUNC("rSI0load000", rageScriptLoadProgram);
	LIB_FUNC("rSI0strt000", rageScriptStartThread);
	LIB_FUNC("rSI0kill000", rageScriptKillThread);
	LIB_FUNC("rSI0tick000", rageScriptTick);
	LIB_FUNC("rSI0gsta000", rageScriptGetThreadState);
	LIB_FUNC("rSI0regn000", rageScriptRegisterNative);
	LIB_FUNC("rRP0open000", rageOpenArchive);
	LIB_FUNC("rRP0read000", rageReadFile);
	LIB_FUNC("rRP0clos000", rageCloseArchive);
	LIB_FUNC("rPH0tick000", ragePhysicsTick);
	LIB_FUNC("rPH0rayc000", ragePhysicsRaycast);
	LIB_FUNC("rST0reqt000", rageStreamRequestTexture);
	LIB_FUNC("rST0resi000", rageStreamGetResidentMip);
}

} // namespace RageEngine
} // namespace Libs
