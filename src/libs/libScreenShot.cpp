// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceScreenShot / libSceScreenShotDrc HLE, ported into KytyPlus's
// LIB_FUNC framework. NID map verified against shadPS4's screenshot.cpp
// (GPL-2.0-or-later). All entry points are stubbed (return ORBIS_OK), matching
// shadPS4's behavior for this non-rendering-path module.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <common/abi.h>

namespace Libs {

namespace LibScreenShot {

LIB_VERSION("libSceScreenShot", 1, "libSceScreenShot", 1, 1);

static int KYTY_SYSV_ABI ScreenShotDummy() {
	PRINT_NAME();
	return 0; // ORBIS_OK
}

static int KYTY_SYSV_ABI sceScreenShotCapture() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotDisable() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotDisableNotification() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotEnable() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotEnableNotification() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotGetAppInfo() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotGetDrcParam() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotIsDisabled() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotIsVshScreenCaptureDisabled() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotSetOverlayImage() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotSetOverlayImageWithOrigin() {
	PRINT_NAME();
	return 0;
}

static int KYTY_SYSV_ABI sceScreenShotSetParam() {
	PRINT_NAME();
	return 0;
}

// NOTE: SetDrcParam belongs to the DRC variant library (libSceScreenShotDrc) but
// exports under the libSceScreenShot module name, per the verified shadPS4 map.
static int KYTY_SYSV_ABI sceScreenShotSetDrcParam() {
	PRINT_NAME();
	return 0;
}


LIB_DEFINE(InitScreenShot_1) {
	LIB_FUNC("AS45QoYHjc4",    LibScreenShot::ScreenShotDummy);
	LIB_FUNC("JuMLLmmvRgk",    LibScreenShot::sceScreenShotCapture);
	LIB_FUNC("tIYf0W5VTi8",    LibScreenShot::sceScreenShotDisable);
	LIB_FUNC("ysfza71rm9M",    LibScreenShot::sceScreenShotDisableNotification);
	LIB_FUNC("2xxUtuC-RzE",    LibScreenShot::sceScreenShotEnable);
	LIB_FUNC("BDUaqlVdSAY",    LibScreenShot::sceScreenShotEnableNotification);
	LIB_FUNC("hNmK4SdhPT0",    LibScreenShot::sceScreenShotGetAppInfo);
	LIB_FUNC("VlAQIgXa2R0",    LibScreenShot::sceScreenShotGetDrcParam);
	LIB_FUNC("-SV-oTNGFQk",    LibScreenShot::sceScreenShotIsDisabled);
	LIB_FUNC("ICNJ-1POs84",    LibScreenShot::sceScreenShotIsVshScreenCaptureDisabled);
	LIB_FUNC("ahHhOf+QNkQ",    LibScreenShot::sceScreenShotSetOverlayImage);
	LIB_FUNC("73WQ4Jj0nJI",    LibScreenShot::sceScreenShotSetOverlayImageWithOrigin);
	LIB_FUNC("G7KlmIYFIZc",    LibScreenShot::sceScreenShotSetParam);
	LIB_FUNC("itlWFWV3Tzc",    LibScreenShot::sceScreenShotSetDrcParam);
}

} // namespace LibScreenShot
} // namespace Libs