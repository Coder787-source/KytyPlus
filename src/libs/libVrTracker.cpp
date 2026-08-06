// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceVrTrackerFourDeviceAllowed HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibVrTracker {
LIB_VERSION("libSceVrTracker", 1, "libSceVrTracker", 1, 1);


static int KYTY_SYSV_ABI sceVrTrackerRegisterDevice2() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerCpuProcess() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGetPlayAreaWarningInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGetResult() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGetTime() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGpuSubmit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGpuWait() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGpuWaitAndCpuProcess() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerInit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerNotifyEndOfCpuProcess() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerQueryMemory() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerRecalibrate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerRegisterDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerRegisterDeviceInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerResetAll() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerResetOrientationRelative() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerSaveInternalBuffers() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerSetDurationUntilStatusNotTracking() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerSetExtendedMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerSetLEDBrightness() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerSetRestingMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerTerm() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerUnregisterDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerUpdateMotionSensorData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_0FA4C949F8D3024E() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_285C6AFC09C42F7E() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_9A6CDB2103664F8A() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_B4D26B7D8B18DF06() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerSetDeviceRejection() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_1119B0BE399F37E7() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_4928B43816BC440D() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_863EF32EFCB0FA9C() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_E6E726CBC85C48F9() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_F6407E46C66DF383() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerCpuPopMarker() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerCpuPushMarker() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerGetLiveCaptureId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerStartLiveCapture() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVrTrackerStopLiveCapture() {
	PRINT_NAME();
	return OK; // STUBBED
}


LIB_DEFINE(InitVrTracker_1) {
	LIB_FUNC("24kDA+A0Ox0", LibVrTracker::sceVrTrackerRegisterDevice2);
	LIB_FUNC("5IFOAYv-62g", LibVrTracker::sceVrTrackerCpuProcess);
	LIB_FUNC("zvyKP0Z3UvU", LibVrTracker::sceVrTrackerGetPlayAreaWarningInfo);
	LIB_FUNC("76OBvrrQXUc", LibVrTracker::sceVrTrackerGetResult);
	LIB_FUNC("XoeWzXlrnMw", LibVrTracker::sceVrTrackerGetTime);
	LIB_FUNC("TVegDMLaBB8", LibVrTracker::sceVrTrackerGpuSubmit);
	LIB_FUNC("gkGuO9dd57M", LibVrTracker::sceVrTrackerGpuWait);
	LIB_FUNC("ARhgpXvwoR0", LibVrTracker::sceVrTrackerGpuWaitAndCpuProcess);
	LIB_FUNC("QkRl7pART9M", LibVrTracker::sceVrTrackerInit);
	LIB_FUNC("VItTwN8DmS8", LibVrTracker::sceVrTrackerNotifyEndOfCpuProcess);
	LIB_FUNC("K7yhYrsIBPc", LibVrTracker::sceVrTrackerQueryMemory);
	LIB_FUNC("EUCaQtXXYNI", LibVrTracker::sceVrTrackerRecalibrate);
	LIB_FUNC("sIh8GwcevaQ", LibVrTracker::sceVrTrackerRegisterDevice);
	LIB_FUNC("ufexf4aNiwg", LibVrTracker::sceVrTrackerRegisterDeviceInternal);
	LIB_FUNC("CtWUbFgmq+I", LibVrTracker::sceVrTrackerResetAll);
	LIB_FUNC("E0P0sN-wy+4", LibVrTracker::sceVrTrackerResetOrientationRelative);
	LIB_FUNC("bDGZVTwwZ1A", LibVrTracker::sceVrTrackerSaveInternalBuffers);
	LIB_FUNC("qBjnR0HtMYI", LibVrTracker::sceVrTrackerSetDurationUntilStatusNotTracking);
	LIB_FUNC("NhPkY3V8E+8", LibVrTracker::sceVrTrackerSetExtendedMode);
	LIB_FUNC("vpsLLotiSUg", LibVrTracker::sceVrTrackerSetLEDBrightness);
	LIB_FUNC("lgWSHQ8p4i4", LibVrTracker::sceVrTrackerSetRestingMode);
	LIB_FUNC("IBv4P3q1pQ0", LibVrTracker::sceVrTrackerTerm);
	LIB_FUNC("Q8skQqEwn5c", LibVrTracker::sceVrTrackerUnregisterDevice);
	LIB_FUNC("9fvHMUbsom4", LibVrTracker::sceVrTrackerUpdateMotionSensorData);
	LIB_FUNC("D6TJSfjTAk4", LibVrTracker::Func_0FA4C949F8D3024E);
	LIB_FUNC("KFxq-AnEL34", LibVrTracker::Func_285C6AFC09C42F7E);
	LIB_FUNC("mmzbIQNmT4o", LibVrTracker::Func_9A6CDB2103664F8A);
	LIB_FUNC("tNJrfYsY3wY", LibVrTracker::Func_B4D26B7D8B18DF06);
	LIB_FUNC("jGqEkPy0iLU", LibVrTracker::sceVrTrackerSetDeviceRejection);
	LIB_FUNC("ERmwvjmfN+c", LibVrTracker::Func_1119B0BE399F37E7);
	LIB_FUNC("SSi0OBa8RA0", LibVrTracker::Func_4928B43816BC440D);
	LIB_FUNC("hj7zLvyw+pw", LibVrTracker::Func_863EF32EFCB0FA9C);
	LIB_FUNC("5ucmy8hcSPk", LibVrTracker::Func_E6E726CBC85C48F9);
	LIB_FUNC("9kB+RsZt84M", LibVrTracker::Func_F6407E46C66DF383);
	LIB_FUNC("sBkAqyF5Gns", LibVrTracker::sceVrTrackerCpuPopMarker);
	LIB_FUNC("rvCywCbc7Pk", LibVrTracker::sceVrTrackerCpuPushMarker);
	LIB_FUNC("lm6T1Ur6JRk", LibVrTracker::sceVrTrackerGetLiveCaptureId);
	LIB_FUNC("qa1+CeXKDPc", LibVrTracker::sceVrTrackerStartLiveCapture);
	LIB_FUNC("3YCwwpHkHIg", LibVrTracker::sceVrTrackerStopLiveCapture);
}
} // namespace LibVrTracker

} // namespace Libs
