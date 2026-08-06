// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceHmd HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibHmd {
LIB_VERSION("libSceHmd", 1, "libSceHmd", 1, 1);

static int KYTY_SYSV_ABI sceHmdClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdGet2DEyeOffset() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdGetAssyError() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdGetDeviceInformation() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdGetDeviceInformationByHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdGetFieldOfView() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdGetInertialSensorData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInitialize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInitialize315() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternal3dAudioClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternal3dAudioOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternal3dAudioSendData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenGetAudioStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenGetFadeState() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenGetVideoStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenSendAudio() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenSendVideo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalAnotherScreenSetFadeAndSwitch() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalBindDeviceWithUserId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCheckDeviceModelMk3() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCheckS3dPassModeAvailable() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCrashReportCancel() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCrashReportClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCrashReportOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCrashReportReadData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCrashReportReadDataSize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalCreateSharedMemory() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuCheckAfterPvt() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuCheckPartialUpdateAvailable() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuGetStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuReset() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuSend() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuSendSize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuSetMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalDfuStart() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalEventInitialize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetBrightness() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetCrashDumpInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDebugMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDebugSocialScreenMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDebugTextMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDefaultLedData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDemoMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDeviceInformation() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDeviceInformationByHandle() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetDeviceStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetEyeStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetHmuOpticalParam() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetHmuPowerStatusForDebug() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetHmuSerialNumber() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_69383B2B4E3AEABF() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetIPD() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetIpdSettingEnableForSystemService() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetPuBuildNumber() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetPuPositionParam() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetPuRevision() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetPUSerialNumber() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetPUVersion() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetRequiredPUPVersion() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetStatusReport() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetTv4kCapability() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetVirtualDisplayDepth() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetVirtualDisplayHeight() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetVirtualDisplaySize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalGetVr2dData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalIsCommonDlgMiniAppVr2d() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalIsCommonDlgVr2d() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalIsGameVr2d() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalIsMiniAppVr2d() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMapSharedMemory() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMirroringModeSetAspect() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMirroringModeSetAspectDebug() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMmapGetCount() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMmapGetModeId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMmapGetSensorCalibrationData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalMmapIsConnect() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalPushVr2dData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalRegisterEventCallback() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalResetInertialSensor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalResetLedForVrTracker() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalResetLedForVsh() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSeparateModeClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSeparateModeGetAudioStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSeparateModeGetVideoStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSeparateModeOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSeparateModeSendAudio() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSeparateModeSendVideo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetBrightness() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetCrashReportCommand() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDebugGpo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDebugMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDebugSocialScreenMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDebugTextMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDefaultLedData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDemoMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetDeviceConnection() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetForcedCrash() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetHmuPowerControl() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetHmuPowerControlForDebug() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetIPD() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetIpdSettingEnableForSystemService() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetLedOn() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetM2LedBrightness() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetM2LedOn() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetPortConnection() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetPortStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetS3dPassMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetSidetone() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetUserType() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetVirtualDisplayDepth() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetVirtualDisplayHeight() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetVirtualDisplaySize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSetVRMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSocialScreenGetFadeState() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSocialScreenSetFadeAndSwitch() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdInternalSocialScreenSetOutput() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceHmdTerminate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_202D0D1A687FCD2F() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_358DBF818A3D8A12() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_5CCBADA76FE8F40E() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_63D403167DC08CF0() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_791560C32F4F6D68() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_7C955961EA85B6D3() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_9952277839236BA7() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_9A276E739E54EEAF() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_9E501994E289CBE7() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_A31F4DA8B3BD2E12() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_A92D7C23AC364993() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_ADCCC25CB876FDBE() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_B16652641FE69F0E() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_FC193BD653F2AF2E() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_FF2E0E53015FE231() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitHmd_1) {
	LIB_FUNC("6biw1XHTSqQ", LibHmd::sceHmdClose);
	LIB_FUNC("BWY-qKM5hxE", LibHmd::sceHmdGet2DEyeOffset);
	LIB_FUNC("Yx+CuF11D3Q", LibHmd::sceHmdGetAssyError);
	LIB_FUNC("thDt9upZlp8", LibHmd::sceHmdGetDeviceInformation);
	LIB_FUNC("1pxQfif1rkE", LibHmd::sceHmdGetDeviceInformationByHandle);
	LIB_FUNC("NPQwYFqi0bs", LibHmd::sceHmdGetFieldOfView);
	LIB_FUNC("rU3HK9Q0r8o", LibHmd::sceHmdGetInertialSensorData);
	LIB_FUNC("K4KnH0QkT2c", LibHmd::sceHmdInitialize);
	LIB_FUNC("s-J66ar9g50", LibHmd::sceHmdInitialize315);
	LIB_FUNC("riPQfAdebHk", LibHmd::sceHmdInternal3dAudioClose);
	LIB_FUNC("wHnZU1qtiqw", LibHmd::sceHmdInternal3dAudioOpen);
	LIB_FUNC("NuEjeN8WCBA", LibHmd::sceHmdInternal3dAudioSendData);
	LIB_FUNC("QasPTUPWVZE", LibHmd::sceHmdInternalAnotherScreenClose);
	LIB_FUNC("Wr5KVtyVDG0", LibHmd::sceHmdInternalAnotherScreenGetAudioStatus);
	LIB_FUNC("whRxl6Hhrzg", LibHmd::sceHmdInternalAnotherScreenGetFadeState);
	LIB_FUNC("w8BEUsIYn8w", LibHmd::sceHmdInternalAnotherScreenGetVideoStatus);
	LIB_FUNC("0cQDAbkOt2A", LibHmd::sceHmdInternalAnotherScreenOpen);
	LIB_FUNC("Asczi8gw1NM", LibHmd::sceHmdInternalAnotherScreenSendAudio);
	LIB_FUNC("6+v7m1vwE+0", LibHmd::sceHmdInternalAnotherScreenSendVideo);
	LIB_FUNC("E0BLvy57IiQ", LibHmd::sceHmdInternalAnotherScreenSetFadeAndSwitch);
	LIB_FUNC("UTqrWB+1+SU", LibHmd::sceHmdInternalBindDeviceWithUserId);
	LIB_FUNC("ego1YdqNGpI", LibHmd::sceHmdInternalCheckDeviceModelMk3);
	LIB_FUNC("WR7XsLdjcqQ", LibHmd::sceHmdInternalCheckS3dPassModeAvailable);
	LIB_FUNC("eMI1Hq+NEwY", LibHmd::sceHmdInternalCrashReportCancel);
	LIB_FUNC("dI3StPLQlMM", LibHmd::sceHmdInternalCrashReportClose);
	LIB_FUNC("lqPT-Bf1s4I", LibHmd::sceHmdInternalCrashReportOpen);
	LIB_FUNC("QxhJs6zHUmU", LibHmd::sceHmdInternalCrashReportReadData);
	LIB_FUNC("A2jWOLPzHHE", LibHmd::sceHmdInternalCrashReportReadDataSize);
	LIB_FUNC("E9scVxt0DNg", LibHmd::sceHmdInternalCreateSharedMemory);
	LIB_FUNC("6RclvsKxr3I", LibHmd::sceHmdInternalDfuCheckAfterPvt);
	LIB_FUNC("cE99PJR6b8w", LibHmd::sceHmdInternalDfuCheckPartialUpdateAvailable);
	LIB_FUNC("SuE90Qscg0s", LibHmd::sceHmdInternalDfuClose);
	LIB_FUNC("5f-6lp7L5cY", LibHmd::sceHmdInternalDfuGetStatus);
	LIB_FUNC("dv2RqD7ZBd4", LibHmd::sceHmdInternalDfuOpen);
	LIB_FUNC("pN0HjRU86Jo", LibHmd::sceHmdInternalDfuReset);
	LIB_FUNC("mdc++HCXSsQ", LibHmd::sceHmdInternalDfuSend);
	LIB_FUNC("gjyqnphjGZE", LibHmd::sceHmdInternalDfuSendSize);
	LIB_FUNC("bl4MkWNLxKs", LibHmd::sceHmdInternalDfuSetMode);
	LIB_FUNC("a1LmvXhZ6TM", LibHmd::sceHmdInternalDfuStart);
	LIB_FUNC("+UzzSnc0z9A", LibHmd::sceHmdInternalEventInitialize);
	LIB_FUNC("uQc9P8Hrr6U", LibHmd::sceHmdInternalGetBrightness);
	LIB_FUNC("nK1g+MwMV10", LibHmd::sceHmdInternalGetCrashDumpInfo);
	LIB_FUNC("L5WZgOTw41Y", LibHmd::sceHmdInternalGetDebugMode);
	LIB_FUNC("3w8SkMfCHY0", LibHmd::sceHmdInternalGetDebugSocialScreenMode);
	LIB_FUNC("1Xmb76MHXug", LibHmd::sceHmdInternalGetDebugTextMode);
	LIB_FUNC("S0ITgPRkfUg", LibHmd::sceHmdInternalGetDefaultLedData);
	LIB_FUNC("mxjolbeBa78", LibHmd::sceHmdInternalGetDemoMode);
	LIB_FUNC("RFIi20Wp9j0", LibHmd::sceHmdInternalGetDeviceInformation);
	LIB_FUNC("P04LQJQZ43Y", LibHmd::sceHmdInternalGetDeviceInformationByHandle);
	LIB_FUNC("PPCqsD8B5uM", LibHmd::sceHmdInternalGetDeviceStatus);
	LIB_FUNC("-u82z1UhOq4", LibHmd::sceHmdInternalGetEyeStatus);
	LIB_FUNC("iINSFzCIaB8", LibHmd::sceHmdInternalGetHmuOpticalParam);
	LIB_FUNC("Csuvq2MMXHU", LibHmd::sceHmdInternalGetHmuPowerStatusForDebug);
	LIB_FUNC("UhFPniZvm8U", LibHmd::sceHmdInternalGetHmuSerialNumber);
	LIB_FUNC("aTg7K0466r8", LibHmd::Func_69383B2B4E3AEABF);
	LIB_FUNC("9exeDpk7JU8", LibHmd::sceHmdInternalGetIPD);
	LIB_FUNC("yNtYRsxZ6-A", LibHmd::sceHmdInternalGetIpdSettingEnableForSystemService);
	LIB_FUNC("EKn+IFVsz0M", LibHmd::sceHmdInternalGetPuBuildNumber);
	LIB_FUNC("AxQ6HtktYfQ", LibHmd::sceHmdInternalGetPuPositionParam);
	LIB_FUNC("ynKv9QCSbto", LibHmd::sceHmdInternalGetPuRevision);
	LIB_FUNC("3jcyx7XOm7A", LibHmd::sceHmdInternalGetPUSerialNumber);
	LIB_FUNC("+PDyXnclP5w", LibHmd::sceHmdInternalGetPUVersion);
	LIB_FUNC("67q17ERGBuw", LibHmd::sceHmdInternalGetRequiredPUPVersion);
	LIB_FUNC("uGyN1CkvwYU", LibHmd::sceHmdInternalGetStatusReport);
	LIB_FUNC("p9lSvZujLuo", LibHmd::sceHmdInternalGetTv4kCapability);
	LIB_FUNC("-Z+-9u98m9o", LibHmd::sceHmdInternalGetVirtualDisplayDepth);
	LIB_FUNC("df+b0FQnnVQ", LibHmd::sceHmdInternalGetVirtualDisplayHeight);
	LIB_FUNC("i6yROd9ygJs", LibHmd::sceHmdInternalGetVirtualDisplaySize);
	LIB_FUNC("Aajiktl6JXU", LibHmd::sceHmdInternalGetVr2dData);
	LIB_FUNC("GwFVF2KkIT4", LibHmd::sceHmdInternalIsCommonDlgMiniAppVr2d);
	LIB_FUNC("LWQpWHOSUvk", LibHmd::sceHmdInternalIsCommonDlgVr2d);
	LIB_FUNC("YiIVBPLxmfE", LibHmd::sceHmdInternalIsGameVr2d);
	LIB_FUNC("LMlWs+oKHTg", LibHmd::sceHmdInternalIsMiniAppVr2d);
	LIB_FUNC("nBv4CKUGX0Y", LibHmd::sceHmdInternalMapSharedMemory);
	LIB_FUNC("4hTD8I3CyAk", LibHmd::sceHmdInternalMirroringModeSetAspect);
	LIB_FUNC("EJwPtSSZykY", LibHmd::sceHmdInternalMirroringModeSetAspectDebug);
	LIB_FUNC("r7f7M5q3snU", LibHmd::sceHmdInternalMmapGetCount);
	LIB_FUNC("gCjTEtEsOOw", LibHmd::sceHmdInternalMmapGetModeId);
	LIB_FUNC("HAr740Mt9Hs", LibHmd::sceHmdInternalMmapGetSensorCalibrationData);
	LIB_FUNC("1PNiQR-7L6k", LibHmd::sceHmdInternalMmapIsConnect);
	LIB_FUNC("9-jaAXUNG-A", LibHmd::sceHmdInternalPushVr2dData);
	LIB_FUNC("1gkbLH5+kxU", LibHmd::sceHmdInternalRegisterEventCallback);
	LIB_FUNC("6kHBllapJas", LibHmd::sceHmdInternalResetInertialSensor);
	LIB_FUNC("k1W6RPkd0mc", LibHmd::sceHmdInternalResetLedForVrTracker);
	LIB_FUNC("dp1wu22jSGc", LibHmd::sceHmdInternalResetLedForVsh);
	LIB_FUNC("d2TeoKeqM5U", LibHmd::sceHmdInternalSeparateModeClose);
	LIB_FUNC("WxsnAsjPF7Q", LibHmd::sceHmdInternalSeparateModeGetAudioStatus);
	LIB_FUNC("eOOeG9SpEuc", LibHmd::sceHmdInternalSeparateModeGetVideoStatus);
	LIB_FUNC("gA4Xnn+NSGk", LibHmd::sceHmdInternalSeparateModeOpen);
	LIB_FUNC("stQ7AsondmE", LibHmd::sceHmdInternalSeparateModeSendAudio);
	LIB_FUNC("jfnS-OoDayM", LibHmd::sceHmdInternalSeparateModeSendVideo);
	LIB_FUNC("roHN4ml+tB8", LibHmd::sceHmdInternalSetBrightness);
	LIB_FUNC("0z2qLqedQH0", LibHmd::sceHmdInternalSetCrashReportCommand);
	LIB_FUNC("xhx5rVZEpnw", LibHmd::sceHmdInternalSetDebugGpo);
	LIB_FUNC("e7laRxRGCHc", LibHmd::sceHmdInternalSetDebugMode);
	LIB_FUNC("CRyJ7Q-ap3g", LibHmd::sceHmdInternalSetDebugSocialScreenMode);
	LIB_FUNC("dG4XPW4juU4", LibHmd::sceHmdInternalSetDebugTextMode);
	LIB_FUNC("rAXmGoO-VmE", LibHmd::sceHmdInternalSetDefaultLedData);
	LIB_FUNC("lu9I7jnUvWQ", LibHmd::sceHmdInternalSetDemoMode);
	LIB_FUNC("hyATMTuQSoQ", LibHmd::sceHmdInternalSetDeviceConnection);
	LIB_FUNC("c4mSi64bXUw", LibHmd::sceHmdInternalSetForcedCrash);
	LIB_FUNC("U9kPT4g1mFE", LibHmd::sceHmdInternalSetHmuPowerControl);
	LIB_FUNC("dX-MVpXIPwQ", LibHmd::sceHmdInternalSetHmuPowerControlForDebug);
	LIB_FUNC("4KIjvAf8PCA", LibHmd::sceHmdInternalSetIPD);
	LIB_FUNC("NbxTfUKO184", LibHmd::sceHmdInternalSetIpdSettingEnableForSystemService);
	LIB_FUNC("NnRKjf+hxW4", LibHmd::sceHmdInternalSetLedOn);
	LIB_FUNC("4AP0X9qGhqw", LibHmd::sceHmdInternalSetM2LedBrightness);
	LIB_FUNC("Mzzz2HPWM+8", LibHmd::sceHmdInternalSetM2LedOn);
	LIB_FUNC("LkBkse9Pit0", LibHmd::sceHmdInternalSetPortConnection);
	LIB_FUNC("v243mvYg0Y0", LibHmd::sceHmdInternalSetPortStatus);
	LIB_FUNC("EwXvkZpo9Go", LibHmd::sceHmdInternalSetS3dPassMode);
	LIB_FUNC("g3DKNOy1tYw", LibHmd::sceHmdInternalSetSidetone);
	LIB_FUNC("mjMsl838XM8", LibHmd::sceHmdInternalSetUserType);
	LIB_FUNC("8IS0KLkDNQY", LibHmd::sceHmdInternalSetVirtualDisplayDepth);
	LIB_FUNC("afhK5KcJOJY", LibHmd::sceHmdInternalSetVirtualDisplayHeight);
	LIB_FUNC("+zPvzIiB+BU", LibHmd::sceHmdInternalSetVirtualDisplaySize);
	LIB_FUNC("9z8Lc64NF1c", LibHmd::sceHmdInternalSetVRMode);
	LIB_FUNC("s5EqYh5kbwM", LibHmd::sceHmdInternalSocialScreenGetFadeState);
	LIB_FUNC("a1LMFZtK9b0", LibHmd::sceHmdInternalSocialScreenSetFadeAndSwitch);
	LIB_FUNC("-6FjKlMA+Yc", LibHmd::sceHmdInternalSocialScreenSetOutput);
	LIB_FUNC("d2g5Ij7EUzo", LibHmd::sceHmdOpen);
	LIB_FUNC("z-RMILqP6tE", LibHmd::sceHmdTerminate);
	LIB_FUNC("IC0NGmh-zS8", LibHmd::Func_202D0D1A687FCD2F);
	LIB_FUNC("NY2-gYo9ihI", LibHmd::Func_358DBF818A3D8A12);
	LIB_FUNC("XMutp2-o9A4", LibHmd::Func_5CCBADA76FE8F40E);
	LIB_FUNC("Y9QDFn3AjPA", LibHmd::Func_63D403167DC08CF0);
	LIB_FUNC("eRVgwy9PbWg", LibHmd::Func_791560C32F4F6D68);
	LIB_FUNC("fJVZYeqFttM", LibHmd::Func_7C955961EA85B6D3);
	LIB_FUNC("mVIneDkja6c", LibHmd::Func_9952277839236BA7);
	LIB_FUNC("miduc55U7q8", LibHmd::Func_9A276E739E54EEAF);
	LIB_FUNC("nlAZlOKJy+c", LibHmd::Func_9E501994E289CBE7);
	LIB_FUNC("ox9NqLO9LhI", LibHmd::Func_A31F4DA8B3BD2E12);
	LIB_FUNC("qS18I6w2SZM", LibHmd::Func_A92D7C23AC364993);
	LIB_FUNC("rczCXLh2-b4", LibHmd::Func_ADCCC25CB876FDBE);
	LIB_FUNC("sWZSZB-mnw4", LibHmd::Func_B16652641FE69F0E);
	LIB_FUNC("-Bk71lPyry4", LibHmd::Func_FC193BD653F2AF2E);
	LIB_FUNC("-y4OUwFf4jE", LibHmd::Func_FF2E0E53015FE231);
}

} // namespace LibHmd
} // namespace Libs
