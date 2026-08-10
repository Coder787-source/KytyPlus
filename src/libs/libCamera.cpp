// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceCamera HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibCamera {
LIB_VERSION("libSceCamera", 1, "libSceCamera", 1, 1);

static int KYTY_SYSV_ABI sceCameraAccGetData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraAudioClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraAudioGetData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraAudioGetData2() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraAudioOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraAudioReset() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraChangeAppModuleState() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraCloseByHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraDeviceOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetAttribute() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetAutoExposureGain() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetAutoWhiteBalance() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetCalibData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetCalibDataFromDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetCalibrationData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetConfig() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetContrast() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetDefectivePixelCancellation() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetDeviceConfig() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetDeviceConfigWithoutHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetDeviceID() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetDeviceIDWithoutOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetDeviceInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetExposureGain() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetFrameData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetGamma() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetHue() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetLensCorrection() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetMmapConnectedCount() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetProductInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetRegister() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetRegistryInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetSaturation() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetSharpness() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetVrCaptureInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraGetWhiteBalance() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraInitializeRegistryCalibData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraIsAttached() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraIsConfigChangeDone() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraIsValidFrameData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraOpenByModuleId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraRemoveAppModuleFocus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetAppModuleFocus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetAttribute() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetAttributeInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetAutoExposureGain() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetAutoWhiteBalance() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetCalibData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetConfig() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetConfigInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetContrast() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetDebugStop() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetDefectivePixelCancellation() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetDefectivePixelCancellationInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetExposureGain() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetForceActivate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetGamma() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetHue() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetLensCorrection() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetLensCorrectionInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetProcessFocus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetProcessFocusByHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetRegister() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetSaturation() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetSharpness() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetTrackerMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetUacModeInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetVideoSync() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetVideoSyncInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraSetWhiteBalance() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraStart() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraStartByHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraStop() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceCameraStopByHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitCamera_1) {
	LIB_FUNC("QhjrPkRPUZQ", LibCamera::sceCameraAccGetData);
	LIB_FUNC("UFonL7xopFM", LibCamera::sceCameraAudioClose);
	LIB_FUNC("fkZE7Hup2ro", LibCamera::sceCameraAudioGetData);
	LIB_FUNC("hftC5A1C8OQ", LibCamera::sceCameraAudioGetData2);
	LIB_FUNC("DhqqFiBU+6g", LibCamera::sceCameraAudioOpen);
	LIB_FUNC("wyU98EXAYxU", LibCamera::sceCameraAudioReset);
	LIB_FUNC("Y0pCDajzkVQ", LibCamera::sceCameraChangeAppModuleState);
	LIB_FUNC("OMS9LlcrvBo", LibCamera::sceCameraClose);
	LIB_FUNC("ztqH5qNTpTk", LibCamera::sceCameraCloseByHandle);
	LIB_FUNC("nBH6i2s4Glc", LibCamera::sceCameraDeviceOpen);
	LIB_FUNC("0btIPD5hg5A", LibCamera::sceCameraGetAttribute);
	LIB_FUNC("oEi6vM-3E2c", LibCamera::sceCameraGetAutoExposureGain);
	LIB_FUNC("qTPRMh4eY60", LibCamera::sceCameraGetAutoWhiteBalance);
	LIB_FUNC("hHA1frlMxYE", LibCamera::sceCameraGetCalibData);
	LIB_FUNC("5Oie5RArfWs", LibCamera::sceCameraGetCalibDataFromDevice);
	LIB_FUNC("RHYJ7GKOSMg", LibCamera::sceCameraGetCalibrationData);
	LIB_FUNC("ZaqmGEtYuL0", LibCamera::sceCameraGetConfig);
	LIB_FUNC("a5xFueMZIMs", LibCamera::sceCameraGetContrast);
	LIB_FUNC("tslCukqFE+E", LibCamera::sceCameraGetDefectivePixelCancellation);
	LIB_FUNC("DSOLCrc3Kh8", LibCamera::sceCameraGetDeviceConfig);
	LIB_FUNC("n+rFeP1XXyM", LibCamera::sceCameraGetDeviceConfigWithoutHandle);
	LIB_FUNC("jTJCdyv9GLU", LibCamera::sceCameraGetDeviceID);
	LIB_FUNC("-H3UwGQvNZI", LibCamera::sceCameraGetDeviceIDWithoutOpen);
	LIB_FUNC("WZpxnSAM-ds", LibCamera::sceCameraGetDeviceInfo);
	LIB_FUNC("ObIste7hqdk", LibCamera::sceCameraGetExposureGain);
	LIB_FUNC("mxgMmR+1Kr0", LibCamera::sceCameraGetFrameData);
	LIB_FUNC("WVox2rwGuSc", LibCamera::sceCameraGetGamma);
	LIB_FUNC("zrIUDKZx0iE", LibCamera::sceCameraGetHue);
	LIB_FUNC("XqYRHc4aw3w", LibCamera::sceCameraGetLensCorrection);
	LIB_FUNC("B260o9pSzM8", LibCamera::sceCameraGetMmapConnectedCount);
	LIB_FUNC("ULxbwqiYYuU", LibCamera::sceCameraGetProductInfo);
	LIB_FUNC("olojYZKYiYs", LibCamera::sceCameraGetRegister);
	LIB_FUNC("hawKak+Auw4", LibCamera::sceCameraGetRegistryInfo);
	LIB_FUNC("RTDOsWWqdME", LibCamera::sceCameraGetSaturation);
	LIB_FUNC("c6Fp9M1EXXc", LibCamera::sceCameraGetSharpness);
	LIB_FUNC("IAz2HgZQWzE", LibCamera::sceCameraGetVrCaptureInfo);
	LIB_FUNC("HX5524E5tMY", LibCamera::sceCameraGetWhiteBalance);
	LIB_FUNC("0wnf2a60FqI", LibCamera::sceCameraInitializeRegistryCalibData);
	LIB_FUNC("p6n3Npi3YY4", LibCamera::sceCameraIsAttached);
	LIB_FUNC("wQfd7kfRZvo", LibCamera::sceCameraIsConfigChangeDone);
	LIB_FUNC("U3BVwQl2R5Q", LibCamera::sceCameraIsValidFrameData);
	LIB_FUNC("BHn83xrF92E", LibCamera::sceCameraOpen);
	LIB_FUNC("eTywOSWsEiI", LibCamera::sceCameraOpenByModuleId);
	LIB_FUNC("py8p6kZcHmA", LibCamera::sceCameraRemoveAppModuleFocus);
	LIB_FUNC("j5isFVIlZLk", LibCamera::sceCameraSetAppModuleFocus);
	LIB_FUNC("doPlf33ab-U", LibCamera::sceCameraSetAttribute);
	LIB_FUNC("96F7zp1Xo+k", LibCamera::sceCameraSetAttributeInternal);
	LIB_FUNC("yfSdswDaElo", LibCamera::sceCameraSetAutoExposureGain);
	LIB_FUNC("zIKL4kZleuc", LibCamera::sceCameraSetAutoWhiteBalance);
	LIB_FUNC("LEMk5cTHKEA", LibCamera::sceCameraSetCalibData);
	LIB_FUNC("VQ+5kAqsE2Q", LibCamera::sceCameraSetConfig);
	LIB_FUNC("9+SNhbctk64", LibCamera::sceCameraSetConfigInternal);
	LIB_FUNC("3i5MEzrC1pg", LibCamera::sceCameraSetContrast);
	LIB_FUNC("vejouEusC7g", LibCamera::sceCameraSetDebugStop);
	LIB_FUNC("jMv40y2A23g", LibCamera::sceCameraSetDefectivePixelCancellation);
	LIB_FUNC("vER3cIMBHqI", LibCamera::sceCameraSetDefectivePixelCancellationInternal);
	LIB_FUNC("wgBMXJJA6K4", LibCamera::sceCameraSetExposureGain);
	LIB_FUNC("jeTpU0MqKU0", LibCamera::sceCameraSetForceActivate);
	LIB_FUNC("lhEIsHzB8r4", LibCamera::sceCameraSetGamma);
	LIB_FUNC("QI8GVJUy2ZY", LibCamera::sceCameraSetHue);
	LIB_FUNC("K7W7H4ZRwbc", LibCamera::sceCameraSetLensCorrection);
	LIB_FUNC("eHa3vhGu2rQ", LibCamera::sceCameraSetLensCorrectionInternal);
	LIB_FUNC("lS0tM6n+Q5E", LibCamera::sceCameraSetProcessFocus);
	LIB_FUNC("NVITuK83Z7o", LibCamera::sceCameraSetProcessFocusByHandle);
	LIB_FUNC("8MjO05qk5hA", LibCamera::sceCameraSetRegister);
	LIB_FUNC("bSKEi2PzzXI", LibCamera::sceCameraSetSaturation);
	LIB_FUNC("P-7MVfzvpsM", LibCamera::sceCameraSetSharpness);
	LIB_FUNC("3VJOpzKoIeM", LibCamera::sceCameraSetTrackerMode);
	LIB_FUNC("nnR7KAIDPv8", LibCamera::sceCameraSetUacModeInternal);
	LIB_FUNC("wpeyFwJ+UEI", LibCamera::sceCameraSetVideoSync);
	LIB_FUNC("8WtmqmE4edw", LibCamera::sceCameraSetVideoSyncInternal);
	LIB_FUNC("k3zPIcgFNv0", LibCamera::sceCameraSetWhiteBalance);
	LIB_FUNC("9EpRYMy7rHU", LibCamera::sceCameraStart);
	LIB_FUNC("cLxF1QtHch0", LibCamera::sceCameraStartByHandle);
	LIB_FUNC("2G2C0nmd++M", LibCamera::sceCameraStop);
	LIB_FUNC("+X1Kgnn3bzg", LibCamera::sceCameraStopByHandle);
}

} // namespace LibCamera
} // namespace Libs
