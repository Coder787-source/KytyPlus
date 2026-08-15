// bandwidthController.h
//
// Bandwidth-aware adaptive texture LOD bias controller for shared-memory iGPUs.
//
// Problem: integrated GPUs (e.g. Radeon 780M) share system DDR5 (~86 GB/s) with the
// CPU, whereas the PS5 GPU has dedicated GDDR6 (~448 GB/s). When the guest assumes
// the latter's bandwidth, the iGPU stalls waiting on memory and frames drop.
//
// This controller monitors host frame time and, when the iGPU is bandwidth-starved,
// increases the texture LOD bias so samplers select coarser mip levels. Coarser mips
// are smaller and fetch fewer bytes per pixel, reducing texture bandwidth pressure.
// When headroom returns, the bias relaxes back toward the base value.
//
// HONEST STATUS: The mechanism (frame-time monitoring + bias adjustment + sampler
// cache invalidation) is testable on the reference UM870 — the log records every
// decision and the bias generation counter drives cache invalidation. The
// *game-level* framerate improvement is NOT validated without a real game: a game
// is required to observe whether the reduced texture bandwidth actually yields
// smoother frames. Built and wired; not validated on hardware.

#ifndef KYTY_SRC_COMMON_BANDWIDTHCONTROLLER_H_
#define KYTY_SRC_COMMON_BANDWIDTHCONTROLLER_H_

#include "common/threads.h"

#include <cstdint>

namespace Config {

// Adaptive LOD bias controller. Thread-safe: the present thread calls
// OnFrame() every frame; the render thread reads CurrentLodBiasGeneration()
// to decide whether the sampler cache needs a rebuild.
class BandwidthController {
public:
	// Must be called once at startup, after iGPU defaults are applied, so the
	// controller knows the base bias to relax back toward.
	void Initialize(bool integrated_gpu, int32_t base_bias);

	// Called from the present path once per presented frame.
	// frame_time_ms: measured frame time for the just-presented frame.
	void OnFrame(float frame_time_ms);

	// Current effective LOD bias (base + adaptive delta). Render thread reads this
	// when (re)building samplers.
	[[nodiscard]] int32_t CurrentLodBias() const;

	// Monotonically increasing counter that changes whenever the effective LOD bias
	// changes. The sampler cache embeds this in its key so a bias change invalidates
	// all stale samplers.
	[[nodiscard]] uint64_t CurrentLodBiasGeneration() const;

	// Whether adaptive control is active (iGPU only). When false, CurrentLodBias()
	// just returns the base bias and OnFrame() is a no-op.
	[[nodiscard]] bool Active() const;

private:
	bool      m_active = false;
	int32_t   m_base_bias = 0;        // base bias from iGPU defaults / user config
	int32_t   m_adaptive_delta = 0;    // added on top of base when pressure detected
	uint32_t  m_max_adaptive_delta = 0;// clamp, scales with how iGPU-bottlenecked we are
	mutable Common::Mutex m_mutex;
	uint64_t  m_generation = 0;        // bumped on every bias change

	// Hysteresis state
	uint32_t  m_consecutive_over = 0;  // frames in a row above the pressure threshold
	uint32_t  m_consecutive_under = 0; // frames in a row below the relax threshold
};

BandwidthController& BandwidthControllerInstance();

} // namespace Config

#endif // KYTY_SRC_COMMON_BANDWIDTHCONTROLLER_H_