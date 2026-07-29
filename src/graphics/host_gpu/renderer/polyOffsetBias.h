#pragma once

#include "graphics/guest_gpu/hardwareContext.h"

#include <cmath>

namespace Libs::Graphics {

struct PolyOffsetBias {
	bool  enable   = false;
	float constant = 0.0f;
	float slope    = 0.0f;
	float clamp    = 0.0f;
};

enum class PolyOffsetBiasResult {
	Disabled,
	Enabled,
	UnsupportedPerFace,
	NonFinite,
};

// Maps Prospero PA_SU_POLY_OFFSET_* + mode-control enables onto Vulkan core
// depth-bias parameters.
[[nodiscard]] inline PolyOffsetBiasResult ResolvePolyOffsetBias(const HW::ModeControl& mc,
                                                                const HW::PolyOffset&  po,
                                                                PolyOffsetBias&        out) {
	out = {};
	const bool visible_front = !mc.cull_front;
	const bool visible_back  = !mc.cull_back;
	const bool bias_front    = visible_front && mc.poly_offset_front_enable;
	const bool bias_back     = visible_back && mc.poly_offset_back_enable;
	if (visible_front && visible_back &&
	    (mc.poly_offset_front_enable != mc.poly_offset_back_enable ||
	     (bias_front &&
	      (po.front_scale != po.back_scale || po.front_offset != po.back_offset)))) {
		return PolyOffsetBiasResult::UnsupportedPerFace;
	}
	if (!bias_front && !bias_back) {
		return PolyOffsetBiasResult::Disabled;
	}
	const bool use_front = bias_front;
	out.constant         = use_front ? po.front_offset : po.back_offset;
	out.slope            = use_front ? po.front_scale : po.back_scale;
	out.clamp            = po.clamp;
	if (!std::isfinite(out.constant) || !std::isfinite(out.slope) || !std::isfinite(out.clamp)) {
		out = {};
		return PolyOffsetBiasResult::NonFinite;
	}
	out.enable = true;
	return PolyOffsetBiasResult::Enabled;
}

} // namespace Libs::Graphics
