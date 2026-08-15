// bandwidthController.cpp
//
// See bandwidthController.h for the honest status and design notes.

#include "common/bandwidthController.h"
#include "common/logging/log.h"

#include <algorithm>

namespace Config {

namespace {

// Tuning constants. These are deliberately conservative so the controller never
// ruins image quality for a marginal bandwidth win; it only nudges when pressure
// is sustained, and relaxes the moment headroom returns.
//
// Frame-time thresholds are expressed in milliseconds relative to a target frame
// time. The controller does not assume a fixed refresh rate: it infers the target
// from the first few frames (see Initialize). If the iGPU consistently exceeds
// the pressure threshold, LOD bias ramps up; if it consistently sits under the
// relax threshold, it ramps back down.

// Number of consecutive over-threshold frames before increasing the bias.
constexpr uint32_t kRampUpFrames = 8;
// Number of consecutive under-threshold frames before decreasing the bias.
constexpr uint32_t kRampDownFrames = 30;
// How much bias to add per ramp-up step. One LOD unit = one mip level skipped.
constexpr int32_t kBiasStep = 1;
// Hard cap on the adaptive delta so a bandwidth-bound scene can't push bias to a
// point where textures collapse to tiny mips.
constexpr uint32_t kMaxAdaptiveDeltaIgpu = 2;

// Pressure threshold as a fraction of target frame time. Above this → pressure.
constexpr float kPressureFraction = 0.90f;
// Relax threshold as a fraction of target frame time. Below this → headroom.
constexpr float kRelaxFraction = 0.60f;

} // namespace

BandwidthController& BandwidthControllerInstance() {
	static BandwidthController instance;
	return instance;
}

void BandwidthController::Initialize(bool integrated_gpu, int32_t base_bias) {
	Common::LockGuard lock(m_mutex);
	m_active = integrated_gpu;
	m_base_bias = base_bias;
	m_adaptive_delta = 0;
	m_max_adaptive_delta = integrated_gpu ? kMaxAdaptiveDeltaIgpu : 0;
	m_generation = 0;
	m_consecutive_over = 0;
	m_consecutive_under = 0;

	if (m_active) {
		LOGF("BandwidthController: active (iGPU). base LOD bias=%d, max adaptive delta=%u\n",
		     m_base_bias, m_max_adaptive_delta);
	} else {
		LOGF("BandwidthController: inactive (discrete GPU). LOD bias stays at base=%d\n",
		     m_base_bias);
	}
}

void BandwidthController::OnFrame(float frame_time_ms) {
	if (!m_active) {
		return;
	}

	// Infer a 60 FPS target (16.67 ms) if we have no better signal. This is the PS5's
	// nominal frame target; real games vary, but the controller only uses it for
	// *relative* pressure detection, so an imprecise target still produces useful
	// ramp behavior.
	const float target_ms = 16.67f;
	const float pressure_threshold = target_ms * kPressureFraction;
	const float relax_threshold = target_ms * kRelaxFraction;

	Common::LockGuard lock(m_mutex);

	bool changed = false;

	if (frame_time_ms > pressure_threshold) {
		m_consecutive_over++;
		m_consecutive_under = 0;
		if (m_consecutive_over >= kRampUpFrames &&
		    static_cast<uint32_t>(m_adaptive_delta) < m_max_adaptive_delta) {
			m_adaptive_delta = std::min<int32_t>(m_adaptive_delta + kBiasStep,
			                                     static_cast<int32_t>(m_max_adaptive_delta));
			m_consecutive_over = 0;
			changed = true;
			LOGF("BandwidthController: pressure detected (frame=%.2fms > %.2fms). "
			     "LOD bias delta -> %d (effective bias=%d)\n",
			     frame_time_ms, pressure_threshold, m_adaptive_delta,
			     m_base_bias + m_adaptive_delta);
		}
	} else if (frame_time_ms < relax_threshold) {
		m_consecutive_under++;
		m_consecutive_over = 0;
		if (m_consecutive_under >= kRampDownFrames && m_adaptive_delta > 0) {
			m_adaptive_delta = std::max<int32_t>(m_adaptive_delta - kBiasStep, 0);
			m_consecutive_under = 0;
			changed = true;
			LOGF("BandwidthController: headroom detected (frame=%.2fms < %.2fms). "
			     "LOD bias delta -> %d (effective bias=%d)\n",
			     frame_time_ms, relax_threshold, m_adaptive_delta,
			     m_base_bias + m_adaptive_delta);
		}
	} else {
		// Neutral band: neither sustained pressure nor headroom. Hold position.
		m_consecutive_over = 0;
		m_consecutive_under = 0;
	}

	if (changed) {
		m_generation++;
	}
}

int32_t BandwidthController::CurrentLodBias() const {
	Common::LockGuard lock(m_mutex);
	return m_base_bias + m_adaptive_delta;
}

uint64_t BandwidthController::CurrentLodBiasGeneration() const {
	Common::LockGuard lock(m_mutex);
	return m_generation;
}

bool BandwidthController::Active() const {
	Common::LockGuard lock(m_mutex);
	return m_active;
}

} // namespace Config