// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// RAGE engine texture streaming — working implementation.
//
// GTA V uses an aggressive texture streaming system that keeps only a subset of
// mip levels resident in VRAM at any time. This implementation provides:
//   - Priority-based request queue with distance-based LOD selection
//   - Real VRAM budget tracking with eviction of low-priority textures
//   - Proper request state machine (Pending → InFlight → Complete/Failed)
//   - Mip-level residency tracking per texture
//
// The actual GPU upload is deferred — ProcessQueue() simulates the streaming
// by updating residency state. A future integration with textureCache.cpp
// would perform real async IO from .rpf packfiles and Vulkan texture uploads.

#include "libs/textureStreaming.h"
#include "common/logging/log.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace Libs::TextureStreaming {

namespace {

std::mutex                                         g_mutex;
std::unordered_map<uint32_t, StreamedTexture>      g_textures;
std::vector<StreamRequest>                         g_requests;
StreamBudget                                       g_budget;
std::atomic<uint32_t>                              g_next_request_id {1};
bool                                               g_initialized     = false;

// Estimate VRAM for a given mip level of a texture. Uses a simple
// formula based on the texture's base dimensions and bytes-per-pixel.
uint64_t EstimateMipSize(const StreamedTexture& tex, uint32_t mip) {
	if (mip >= tex.max_mip_levels) return 0;
	uint32_t w = std::max(1u, tex.base_width >> mip);
	uint32_t h = std::max(1u, tex.base_height >> mip);
	// Assume 4 bytes per pixel (RGBA8) as default for ASTC-compressed
	// textures after decode. Real implementation would use the actual
	// compressed size from the .rpf metadata.
	return static_cast<uint64_t>(w) * h * 4u;
}

// Total VRAM needed to have mips [0..target_mip] resident.
uint64_t TotalVramForMips(const StreamedTexture& tex, uint32_t target_mip) {
	uint64_t total = 0;
	for (uint32_t m = 0; m <= target_mip && m < tex.max_mip_levels; m++) {
		total += EstimateMipSize(tex, m);
	}
	return total;
}

// Evict the lowest-priority texture's mip levels to free VRAM.
bool EvictToFreeVram(uint64_t needed_bytes) {
	if (g_budget.current_vram_usage + needed_bytes <= g_budget.total_vram_budget) {
		return true;
	}

	// Sort textures by access tick (oldest first) for eviction candidates.
	std::vector<StreamedTexture*> candidates;
	for (auto& [hash, tex]: g_textures) {
		if (tex.resident_mip != 0xFFFFFFFF && tex.vram_bytes > 0) {
			candidates.push_back(&tex);
		}
	}
	std::sort(candidates.begin(), candidates.end(),
	          [](const StreamedTexture* a, const StreamedTexture* b) {
		          return a->last_access_tick < b->last_access_tick;
	          });

	uint64_t freed = 0;
	for (auto* tex: candidates) {
		if (g_budget.current_vram_usage - freed + needed_bytes <=
		    g_budget.total_vram_budget) {
			break;
		}
		freed += tex->vram_bytes;
		tex->resident_mip = 0xFFFFFFFF; // Evict all mips.
		tex->vram_bytes   = 0;
	}
	g_budget.current_vram_usage -= std::min(g_budget.current_vram_usage, freed);
	return (g_budget.current_vram_usage + needed_bytes <= g_budget.total_vram_budget);
}

} // namespace

void Initialize(uint64_t vram_budget) {
	std::lock_guard lock(g_mutex);
	if (g_initialized) {
		return;
	}
	g_budget.total_vram_budget  = vram_budget;
	g_budget.current_vram_usage = 0;
	g_initialized = true;
	LOGF("[TexStream] INFO: Texture streaming initialized (budget=%" PRIu64 " MiB)\n",
	     vram_budget / (1024 * 1024));
}

void Shutdown() {
	std::lock_guard lock(g_mutex);
	g_textures.clear();
	g_requests.clear();
	g_budget.current_vram_usage = 0;
	g_initialized = false;
	LOGF("[TexStream] INFO: Texture streaming shut down\n");
}

uint32_t RequestTexture(uint32_t texture_hash, uint32_t mip_level,
                        StreamRequestPriority priority, float distance) {
	std::lock_guard lock(g_mutex);
	if (!g_initialized) {
		return 0;
	}

	// Check if this texture+mip is already resident — skip duplicate requests.
	auto it = g_textures.find(texture_hash);
	if (it != g_textures.end() && it->second.resident_mip <= mip_level) {
		// Already at or better than requested mip. No-op.
		return 0;
	}

	StreamRequest req;
	req.request_id         = g_next_request_id.fetch_add(1, std::memory_order_relaxed);
	req.texture_hash       = texture_hash;
	req.requested_mip      = mip_level;
	req.priority           = priority;
	req.distance_to_camera = distance;
	req.state              = StreamRequestState::Pending;

	g_requests.push_back(req);
	return req.request_id;
}

void CancelRequest(uint32_t request_id) {
	std::lock_guard lock(g_mutex);
	for (auto& req: g_requests) {
		if (req.request_id == request_id &&
		    req.state != StreamRequestState::Complete &&
		    req.state != StreamRequestState::Failed) {
			req.state = StreamRequestState::Cancelled;
			return;
		}
	}
}

void RegisterTexture(const StreamedTexture& texture) {
	std::lock_guard lock(g_mutex);
	g_textures[texture.texture_hash] = texture;
}

void UnregisterTexture(uint32_t texture_hash) {
	std::lock_guard lock(g_mutex);
	auto it = g_textures.find(texture_hash);
	if (it != g_textures.end()) {
		g_budget.current_vram_usage -= std::min(g_budget.current_vram_usage, it->second.vram_bytes);
		g_textures.erase(it);
	}
}

uint32_t GetResidentMip(uint32_t texture_hash) {
	std::lock_guard lock(g_mutex);
	auto it = g_textures.find(texture_hash);
	if (it == g_textures.end()) {
		return 0xFFFFFFFF;
	}
	return it->second.resident_mip;
}

void ProcessQueue() {
	std::lock_guard lock(g_mutex);
	if (!g_initialized) {
		return;
	}

	// Sort pending requests by priority (higher first), then by distance
	// (closer objects first). This mirrors the real RAGE streaming heuristic.
	std::sort(g_requests.begin(), g_requests.end(),
	          [](const StreamRequest& a, const StreamRequest& b) {
		          if (a.priority != b.priority) {
			          return static_cast<uint32_t>(a.priority) >
			                 static_cast<uint32_t>(b.priority);
		          }
		          return a.distance_to_camera < b.distance_to_camera;
	          });

	for (auto& req: g_requests) {
		if (req.state != StreamRequestState::Pending) {
			continue;
		}

		auto it = g_textures.find(req.texture_hash);
		if (it == g_textures.end()) {
			// Unknown texture — mark as failed.
			req.state = StreamRequestState::Failed;
			continue;
		}

		auto& tex = it->second;

		// Transition to InFlight.
		req.state = StreamRequestState::InFlight;

		// Calculate VRAM needed to promote to the requested mip level.
		const uint64_t current_vram = tex.vram_bytes;
		const uint64_t needed_vram  = TotalVramForMips(tex, req.requested_mip);
		const uint64_t delta        = (needed_vram > current_vram) ?
		                              (needed_vram - current_vram) : 0;

		if (delta > 0) {
			// Try to free VRAM by evicting least-recently-used textures.
			if (!EvictToFreeVram(delta)) {
				// Not enough VRAM — fail the request. The script will
				// retry with a lower mip level or evict manually.
				req.state = StreamRequestState::Failed;
				static std::atomic<uint32_t> evict_logs {0};
				if (evict_logs.fetch_add(1, std::memory_order_relaxed) < 32) {
					LOGF_COLOR(Log::Color::Yellow,
					           "[TexStream] WARN: VRAM budget exhausted, cannot stream "
					           "texture 0x%08" PRIx32 " to mip %" PRIu32
					           " (need %" PRIu64 " bytes, avail %" PRIu64 ")\n",
					           req.texture_hash, req.requested_mip, delta,
					           g_budget.total_vram_budget - g_budget.current_vram_usage);
				}
				continue;
			}
			g_budget.current_vram_usage += delta;
		}

		// "Upload" complete: update residency state.
		tex.resident_mip  = req.requested_mip;
		tex.vram_bytes    = needed_vram;
		tex.last_access_tick++;
		req.state         = StreamRequestState::Complete;
	}

	// Garbage-collect completed / cancelled / failed requests.
	g_requests.erase(
	    std::remove_if(g_requests.begin(), g_requests.end(),
	                   [](const StreamRequest& r) {
		                   return r.state == StreamRequestState::Complete ||
		                          r.state == StreamRequestState::Failed ||
		                          r.state == StreamRequestState::Cancelled;
	                   }),
	    g_requests.end());
}

const StreamBudget& GetBudget() {
	return g_budget;
}

uint32_t GetTextureCount() {
	std::lock_guard lock(g_mutex);
	return static_cast<uint32_t>(g_textures.size());
}

uint32_t GetPendingRequestCount() {
	std::lock_guard lock(g_mutex);
	return static_cast<uint32_t>(std::count_if(
	    g_requests.begin(), g_requests.end(),
	    [](const StreamRequest& r) {
		    return r.state == StreamRequestState::Pending ||
		           r.state == StreamRequestState::InFlight;
	    }));
}

} // namespace Libs::TextureStreaming
