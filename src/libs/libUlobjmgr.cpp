// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 ulobjmgr HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibUlobjmgr {
LIB_VERSION("ulobjmgr", 1, "ulobjmgr", 1, 1);


static int KYTY_SYSV_ABI _sceUlobjmgrRegisterObject() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI _sceUlobjmgrUnregisterObject() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_1D9F50D9CFB8054E() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_4B07893BBB77A649() {
	PRINT_NAME();
	return OK; // STUBBED
}


LIB_DEFINE(InitUlobjmgr_1) {
	LIB_FUNC("BG26hBGiNlw", LibUlobjmgr::_sceUlobjmgrRegisterObject);
	LIB_FUNC("Smf+fUNblPc", LibUlobjmgr::_sceUlobjmgrUnregisterObject);
	LIB_FUNC("HZ9Q2c+4BU4", LibUlobjmgr::Func_1D9F50D9CFB8054E);
	LIB_FUNC("SweJO7t3pkk", LibUlobjmgr::Func_4B07893BBB77A649);
}
} // namespace LibUlobjmgr

} // namespace Libs
