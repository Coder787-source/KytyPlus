// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceRemoteplay HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibRemotePlay {
LIB_VERSION("libSceRemoteplay", 1, "libSceRemoteplay", 1, 1);

static int KYTY_SYSV_ABI sceRemoteplayApprove() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayChangeEnterKey() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayClearAllRegistData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayClearConnectHistory() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayConfirmDeviceRegist() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayDisconnect() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGeneratePinCode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetApMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetConnectHistory() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetConnectUserId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetMbusDeviceInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetOperationStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetRemoteplayStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayGetRpMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeFilterResult() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeGetEvent() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeNotify() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeNotifyEventResult() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeSetCaret() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayImeSetText() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayIsRemoteOskReady() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayIsRemotePlaying() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayNotifyMbusDeviceRegistComplete() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayNotifyNpPushWakeup() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayNotifyPinCodeError() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayNotifyUserDelete() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayPrintAllRegistData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayProhibit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayProhibitStreaming() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayServerLock() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplayServerUnLock() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplaySetApMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplaySetLogLevel() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplaySetProhibition() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplaySetProhibitionForVsh() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceRemoteplaySetRpMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_1D5EE365ED5FADB3() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitRemotePlay_1) {
	LIB_FUNC("xQeIryTX7dY", LibRemotePlay::sceRemoteplayApprove);
	LIB_FUNC("IYZ+Mu+8tPo", LibRemotePlay::sceRemoteplayChangeEnterKey);
	LIB_FUNC("ZYUsJtcAnqA", LibRemotePlay::sceRemoteplayClearAllRegistData);
	LIB_FUNC("cCheyCbF7qw", LibRemotePlay::sceRemoteplayClearConnectHistory);
	LIB_FUNC("tPYT-kGbZh8", LibRemotePlay::sceRemoteplayConfirmDeviceRegist);
	LIB_FUNC("6Lg4BNleJWc", LibRemotePlay::sceRemoteplayDisconnect);
	LIB_FUNC("j98LdSGy4eY", LibRemotePlay::sceRemoteplayGeneratePinCode);
	LIB_FUNC("L+cL-M-DP3w", LibRemotePlay::sceRemoteplayGetApMode);
	LIB_FUNC("g4K51cY+PEw", LibRemotePlay::sceRemoteplayGetConnectHistory);
	LIB_FUNC("3eBNV9A0BUM", LibRemotePlay::sceRemoteplayGetConnectUserId);
	LIB_FUNC("ufesWMVX6iU", LibRemotePlay::sceRemoteplayGetMbusDeviceInfo);
	LIB_FUNC("DxU4JGh4S2k", LibRemotePlay::sceRemoteplayGetOperationStatus);
	LIB_FUNC("n5OxFJEvPlc", LibRemotePlay::sceRemoteplayGetRemoteplayStatus);
	LIB_FUNC("Cekhs6LSHC0", LibRemotePlay::sceRemoteplayGetRpMode);
	LIB_FUNC("ig1ocbR7Ptw", LibRemotePlay::sceRemoteplayImeClose);
	LIB_FUNC("gV9-8cJPM3I", LibRemotePlay::sceRemoteplayImeFilterResult);
	LIB_FUNC("cMk57DZXe6c", LibRemotePlay::sceRemoteplayImeGetEvent);
	LIB_FUNC("-gwkQpOCl68", LibRemotePlay::sceRemoteplayImeNotify);
	LIB_FUNC("58v9tSlRxc8", LibRemotePlay::sceRemoteplayImeNotifyEventResult);
	LIB_FUNC("C3r2zT5ebMg", LibRemotePlay::sceRemoteplayImeOpen);
	LIB_FUNC("oB730zwoz0s", LibRemotePlay::sceRemoteplayImeSetCaret);
	LIB_FUNC("rOTg1Nljp8w", LibRemotePlay::sceRemoteplayImeSetText);
	LIB_FUNC("R8RZC1ZIkzU", LibRemotePlay::sceRemoteplayIsRemoteOskReady);
	LIB_FUNC("uYhiELUtLgA", LibRemotePlay::sceRemoteplayIsRemotePlaying);
	LIB_FUNC("d-BBSEq1nfc", LibRemotePlay::sceRemoteplayNotifyMbusDeviceRegistComplete);
	LIB_FUNC("Yytq7NE38R8", LibRemotePlay::sceRemoteplayNotifyNpPushWakeup);
	LIB_FUNC("Wg-w8xjMZA4", LibRemotePlay::sceRemoteplayNotifyPinCodeError);
	LIB_FUNC("yheulqylKwI", LibRemotePlay::sceRemoteplayNotifyUserDelete);
	LIB_FUNC("t5ZvUiZ1hpE", LibRemotePlay::sceRemoteplayPrintAllRegistData);
	LIB_FUNC("mrNh78tBpmg", LibRemotePlay::sceRemoteplayProhibit);
	LIB_FUNC("7QLrixwVHcU", LibRemotePlay::sceRemoteplayProhibitStreaming);
	LIB_FUNC("-ThIlThsN80", LibRemotePlay::sceRemoteplayServerLock);
	LIB_FUNC("0Z-Pm5rZJOI", LibRemotePlay::sceRemoteplayServerUnLock);
	LIB_FUNC("xSrhtSLIjOc", LibRemotePlay::sceRemoteplaySetApMode);
	LIB_FUNC("5-2agAeaE+c", LibRemotePlay::sceRemoteplaySetLogLevel);
	LIB_FUNC("Rf0XMVR7xPw", LibRemotePlay::sceRemoteplaySetProhibition);
	LIB_FUNC("n4l3FTZtNQM", LibRemotePlay::sceRemoteplaySetProhibitionForVsh);
	LIB_FUNC("-BPcEQ1w8xc", LibRemotePlay::sceRemoteplaySetRpMode);
	LIB_FUNC("HV7jZe1frbM", LibRemotePlay::Func_1D5EE365ED5FADB3);
}

} // namespace LibRemotePlay
} // namespace Libs
