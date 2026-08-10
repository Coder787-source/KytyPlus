// SPDX-FileCopyrightText: Copyright 2024 KytyPlus Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// CPU instruction timing throttle — limits emulator CPU execution speed
// to match PS5's AMD Zen 2 performance (3.5 GHz, ~4-wide decode).
// Without throttling, x86-on-x86 translation runs 2-5x faster than real hardware,
// which can break game timing, physics, and guest timing heuristics.

#pragma once

#include "common/common.h"

#include <atomic>
#include <chrono>

namespace Libs {
namespace CpuThrottle {

// PS5 Zen 2 theoretical IPC (instructions per cycle) — conservative estimate.
// Real workloads vary, but this provides a baseline for throttling.
static constexpr double PS5_ZEN2_IPC         = 3.5;    // ~3.5 IPC average
static constexpr double PS5_CLOCK_GHZ        = 3.5;    // 3.5 GHz
static constexpr double PS5_IPS              = PS5_CLOCK_GHZ * PS5_ZEN2_IPC * 1e9; // ~12.25 GIPS

// Throttle configuration
struct ThrottleConfig {
	bool     enabled           = true;   // master enable
	double   target_ips        = PS5_IPS; // target instructions per second
	double   window_seconds    = 0.016;    // measurement window (1 frame at 60fps)
	double   sleep_threshold   = 0.1;      // sleep if running >10% faster
	uint32_t max_sleep_us      = 500;      // cap sleep time to 500us
};

// Per-thread throttle state
struct ThreadState {
	uint64_t instructions_executed = 0;
	uint64_t last_check_time_ns    = 0;
	double   current_ips           = 0.0;
	bool     needs_sleep           = false;
};

// Global throttle state
class CpuThrottler {
public:
	static CpuThrottler& Get() {
		static CpuThrottler instance;
		return instance;
	}

	void SetConfig(const ThrottleConfig& config) {
		config_ = config;
	}

	const ThrottleConfig& GetConfig() const {
		return config_;
	}

	// Called periodically from the emulation loop to check if throttling is needed.
	// Returns the number of microseconds to sleep (0 if no sleep needed).
	uint32_t CheckAndThrottle(ThreadState& state, uint64_t instructions_this_frame) {
		if (!config_.enabled) {
			return 0;
		}

		state.instructions_executed += instructions_this_frame;

		auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
		                  std::chrono::steady_clock::now().time_since_epoch())
		                  .count();

		if (state.last_check_time_ns == 0) {
			state.last_check_time_ns = static_cast<uint64_t>(now_ns);
			return 0;
		}

		const double elapsed_seconds =
		    static_cast<double>(now_ns - state.last_check_time_ns) / 1e9;

		if (elapsed_seconds < config_.window_seconds) {
			return 0; // Not enough time has passed for a measurement
		}

		// Calculate actual IPS
		state.current_ips = static_cast<double>(state.instructions_executed) / elapsed_seconds;

		// Reset counters for next window
		state.instructions_executed = 0;
		state.last_check_time_ns    = static_cast<uint64_t>(now_ns);

		// Check if running faster than target
		const double speed_ratio = state.current_ips / config_.target_ips;
		state.needs_sleep        = (speed_ratio > (1.0 + config_.sleep_threshold));

		if (!state.needs_sleep) {
			return 0;
		}

		// Calculate sleep time to bring us back in line
		const double excess_time =
		    (state.current_ips / config_.target_ips - 1.0) * config_.window_seconds;
		const auto sleep_us =
		    static_cast<uint32_t>(std::min(excess_time * 1e6, static_cast<double>(config_.max_sleep_us)));

		return sleep_us;
	}

	// Quick estimate of instructions executed based on elapsed time.
	// Used when we don't have an exact instruction count.
	uint64_t EstimateInstructions(double elapsed_seconds) const {
		return static_cast<uint64_t>(config_.target_ips * elapsed_seconds);
	}

private:
	CpuThrottler() = default;
	ThrottleConfig config_;
};

// Convenience function: call this at the end of each frame
inline void ThrottleFrameEnd(ThreadState& state, uint64_t instructions_this_frame) {
	const uint32_t sleep_us = CpuThrottler::Get().CheckAndThrottle(state, instructions_this_frame);
	if (sleep_us > 0) {
		std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
	}
}

} // namespace CpuThrottle
} // namespace Libs
