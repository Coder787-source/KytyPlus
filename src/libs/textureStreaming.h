// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KYTY_LIBS_TEXTURE_STREAMING_H_
#define KYTY_LIBS_TEXTURE_STREAMING_H_

#include "common/common.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Libs::TextureStreaming {

// ─── RAGE texture streaming manager ──────────────────────────────────────────
//
// GTA V streams textures aggressively: at any given moment only a subset of the
// world's texture pool is resident in VRAM. The streaming manager decides which
// textures to load, which to evict, and at what mip level. This scaffolding
// provides the data structures so the guest's streaming initialisation path
// completes and so future work can hook real streaming into the texture cache.

enum class StreamRequestPriority : uint32_t {
	Low      = 0,
	Normal   = 1,
	High     = 2,
	Critical = 3,
};

enum class StreamRequestState : uint32_t {
	Pending    = 0,
	InFlight   = 1,
	Complete   = 2,
	Failed     = 3,
	Cancelled  = 4,
};

struct StreamRequest {
	uint32_t             request_id = 0;
	uint32_t             texture_hash = 0;
	uint32_t             requested_mip = 0;
	uint32_t             resident_mip = 0xFFFFFFFF;
	StreamRequestPriority priority = StreamRequestPriority::Normal;
	StreamRequestState    state = StreamRequestState::Pending;
	float                distance_to_camera = 0.0f;
	uint64_t             texture_offset = 0; // offset in the packed .rpf
	uint32_t             texture_size = 0;
};

struct StreamedTexture {
	uint32_t    texture_hash  = 0;
	std::string name;
	uint32_t    width         = 0;      // alias: base_width
	uint32_t    height        = 0;      // alias: base_height
	uint32_t    mip_count     = 1;      // alias: max_mip_levels
	uint32_t    resident_mip  = 0xFFFFFFFF; // 0xFFFFFFFF = not resident
	uint32_t    format        = 0;      // guest format enum
	uint64_t    vram_bytes    = 0;
	bool        pinned        = false;  // if true, never evict
	uint32_t    base_width    = 0;      // base mip0 width in pixels
	uint32_t    base_height   = 0;      // base mip0 height in pixels
	uint32_t    max_mip_levels = 1;
	uint64_t    last_access_tick = 0;   // for LRU eviction
};

struct StreamBudget {
	uint64_t total_vram_budget    = 4ULL * 1024 * 1024 * 1024; // 4 GiB default
	uint64_t current_vram_usage   = 0;
	uint64_t streaming_bandwidth  = 256ULL * 1024 * 1024; // bytes/sec estimate
	uint32_t max_concurrent_ios   = 4;
};

// ─── Public API ──────────────────────────────────────────────────────────────

void Initialize(uint64_t vram_budget = 4ULL * 1024 * 1024 * 1024);
void Shutdown();

// Submit a streaming request for a texture at a given mip level.
uint32_t RequestTexture(uint32_t texture_hash, uint32_t mip_level,
                        StreamRequestPriority priority, float distance);

// Cancel a pending/in-flight request.
void CancelRequest(uint32_t request_id);

// Register a texture with the streaming manager.
void RegisterTexture(const StreamedTexture& texture);

// Unregister a texture (e.g. when the world sector is unloaded).
void UnregisterTexture(uint32_t texture_hash);

// Query the current resident mip level for a texture.
uint32_t GetResidentMip(uint32_t texture_hash);

// Process pending requests: decide which to promote to in-flight, which to
// cancel, and which to evict to free budget. Called once per frame.
void ProcessQueue();

// Get current streaming budget / usage.
const StreamBudget& GetBudget();

// Total number of registered textures.
uint32_t GetTextureCount();

// Total number of pending/in-flight requests.
uint32_t GetPendingRequestCount();

} // namespace Libs::TextureStreaming

#endif // KYTY_LIBS_TEXTURE_STREAMING_H_
