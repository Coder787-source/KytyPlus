// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceVoice HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibVoice {
LIB_VERSION("libSceVoice", 1, "libSceVoice", 1, 1);

static int KYTY_SYSV_ABI sceVoiceGetMuteFlag() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceGetResourceInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceInitHQ() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoicePausePort() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoicePausePortAll() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceResetPort() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceResumePort() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceResumePortAll() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceSetBitRate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceSetMuteFlag() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceSetMuteFlagAll() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceUpdatePort() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceVADAdjustment() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceVoiceVADSetVersion() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitVoice_1) {
	LIB_FUNC("Pc4z1QjForU", LibVoice::sceVoiceGetMuteFlag);
	LIB_FUNC("Z6QV6j7igvE", LibVoice::sceVoiceGetResourceInfo);
	LIB_FUNC("IPHvnM5+g04", LibVoice::sceVoiceInitHQ);
	LIB_FUNC("x0slGBQW+wY", LibVoice::sceVoicePausePort);
	LIB_FUNC("Dinob0yMRl8", LibVoice::sceVoicePausePortAll);
	LIB_FUNC("udAxvCePkUs", LibVoice::sceVoiceResetPort);
	LIB_FUNC("gAgN+HkiEzY", LibVoice::sceVoiceResumePort);
	LIB_FUNC("jbkJFmOZ9U0", LibVoice::sceVoiceResumePortAll);
	LIB_FUNC("TexwmOHQsDg", LibVoice::sceVoiceSetBitRate);
	LIB_FUNC("gwUynkEgNFY", LibVoice::sceVoiceSetMuteFlag);
	LIB_FUNC("oUha0S-Ij9Q", LibVoice::sceVoiceSetMuteFlagAll);
	LIB_FUNC("jSZNP7xJrcw", LibVoice::sceVoiceUpdatePort);
	LIB_FUNC("hg9T73LlRiU", LibVoice::sceVoiceVADAdjustment);
	LIB_FUNC("wFeAxEeEi-8", LibVoice::sceVoiceVADSetVersion);
}

} // namespace LibVoice
} // namespace Libs
