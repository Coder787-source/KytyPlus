// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceMove HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibMove {
LIB_VERSION("libSceMove", 1, "libSceMove", 1, 1);

static int KYTY_SYSV_ABI sceMoveInit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveGetDeviceInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveReadStateLatest() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveReadStateRecent() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveGetExtensionPortInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveSetVibration() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveSetLightSphere() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveResetLightSphere() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceMoveTerm() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitMove_1) {
	LIB_FUNC("j1ITE-EoJmE", LibMove::sceMoveInit);
	LIB_FUNC("HzC60MfjJxU", LibMove::sceMoveOpen);
	LIB_FUNC("GWXTyxs4QbE", LibMove::sceMoveGetDeviceInfo);
	LIB_FUNC("ttU+JOhShl4", LibMove::sceMoveReadStateLatest);
	LIB_FUNC("f2bcpK6kJfg", LibMove::sceMoveReadStateRecent);
	LIB_FUNC("y5h7f8H1Jnk", LibMove::sceMoveGetExtensionPortInfo);
	LIB_FUNC("IFQwtT2CeY0", LibMove::sceMoveSetVibration);
	LIB_FUNC("T8KYHPs1JE8", LibMove::sceMoveSetLightSphere);
	LIB_FUNC("zuxWAg3HAac", LibMove::sceMoveResetLightSphere);
	LIB_FUNC("XX6wlxpHyeo", LibMove::sceMoveClose);
	LIB_FUNC("tsZi60H4ypY", LibMove::sceMoveTerm);
}

} // namespace LibMove
} // namespace Libs
