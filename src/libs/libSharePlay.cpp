// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceSharePlay HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibSharePlay {
LIB_VERSION("libSceSharePlay", 1, "libSceSharePlay", 1, 1);

static int KYTY_SYSV_ABI sceSharePlayCrashDaemon() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayGetCurrentConnectionInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayGetCurrentConnectionInfoA() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayGetCurrentInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayGetEvent() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayNotifyDialogOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayNotifyForceCloseForCdlg() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayNotifyOpenQuickMenu() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayResumeScreenForCdlg() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayServerLock() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayServerUnLock() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlaySetMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlaySetProhibition() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlaySetProhibitionModeWithAppId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayStartStandby() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayStartStreaming() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayStopStandby() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceSharePlayStopStreaming() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_2E93C0EA6A6B67C4() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_C1C236728D88E177() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_E9E80C474781F115() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_F3DD6199DA15ED44() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitSharePlay_1) {
	LIB_FUNC("ggnCfalLU-8", LibSharePlay::sceSharePlayCrashDaemon);
	LIB_FUNC("OOrLKB0bSDs", LibSharePlay::sceSharePlayGetCurrentConnectionInfo);
	LIB_FUNC("+MCXJlWdi+s", LibSharePlay::sceSharePlayGetCurrentConnectionInfoA);
	LIB_FUNC("vUMkWXQff3w", LibSharePlay::sceSharePlayGetCurrentInfo);
	LIB_FUNC("Md7Mdkr8LBc", LibSharePlay::sceSharePlayGetEvent);
	LIB_FUNC("9zwJpai7jGc", LibSharePlay::sceSharePlayNotifyDialogOpen);
	LIB_FUNC("VUW2V9cUTP4", LibSharePlay::sceSharePlayNotifyForceCloseForCdlg);
	LIB_FUNC("XL0WwUJoQPg", LibSharePlay::sceSharePlayNotifyOpenQuickMenu);
	LIB_FUNC("6-1fKaa5HlY", LibSharePlay::sceSharePlayResumeScreenForCdlg);
	LIB_FUNC("U28jAuLHj6c", LibSharePlay::sceSharePlayServerLock);
	LIB_FUNC("3Oaux9ITEtY", LibSharePlay::sceSharePlayServerUnLock);
	LIB_FUNC("QZy+KmyqKPU", LibSharePlay::sceSharePlaySetMode);
	LIB_FUNC("co2NCj--pnc", LibSharePlay::sceSharePlaySetProhibition);
	LIB_FUNC("KADsbjNCgPo", LibSharePlay::sceSharePlaySetProhibitionModeWithAppId);
	LIB_FUNC("-F6NddfUsa4", LibSharePlay::sceSharePlayStartStandby);
	LIB_FUNC("rWVNHNnEx6g", LibSharePlay::sceSharePlayStartStreaming);
	LIB_FUNC("zEDkUWLVwFI", LibSharePlay::sceSharePlayStopStandby);
	LIB_FUNC("aGlema+JxUU", LibSharePlay::sceSharePlayStopStreaming);
	LIB_FUNC("LpPA6mprZ8Q", LibSharePlay::Func_2E93C0EA6A6B67C4);
	LIB_FUNC("wcI2co2I4Xc", LibSharePlay::Func_C1C236728D88E177);
	LIB_FUNC("6egMR0eB8RU", LibSharePlay::Func_E9E80C474781F115);
	LIB_FUNC("891hmdoV7UQ", LibSharePlay::Func_F3DD6199DA15ED44);
}

} // namespace LibSharePlay
} // namespace Libs
