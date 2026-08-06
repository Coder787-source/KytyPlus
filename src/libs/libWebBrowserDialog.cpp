// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceWebBrowserDialog HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibWebBrowserDialog {

static int KYTY_SYSV_ABI sceWebBrowserDialogClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogGetEvent() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogGetResult() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogGetStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogNavigate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogOpenForPredeterminedContent() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogResetCookie() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogSetCookie() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogSetZoom() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceWebBrowserDialogUpdateStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_F2BE042771625F8C() {
	PRINT_NAME();
	return OK; // STUBBED
}

LIB_VERSION("libSceWebBrowserDialog", 1, "libSceWebBrowserDialog", 1, 1);

} // namespace LibWebBrowserDialog

LIB_DEFINE(InitWebBrowserDialog_1) {
	LIB_FUNC("PSK+Eik919Q", LibWebBrowserDialog::sceWebBrowserDialogClose);
	LIB_FUNC("Wit4LjeoeX4", LibWebBrowserDialog::sceWebBrowserDialogGetEvent);
	LIB_FUNC("vCaW0fgVQmc", LibWebBrowserDialog::sceWebBrowserDialogGetResult);
	LIB_FUNC("CFTG6a8TjOU", LibWebBrowserDialog::sceWebBrowserDialogGetStatus);
	LIB_FUNC("uYELOMVnmNQ", LibWebBrowserDialog::sceWebBrowserDialogNavigate);
	LIB_FUNC("FraP7debcdg", LibWebBrowserDialog::sceWebBrowserDialogOpen);
	LIB_FUNC("O7dIZQrwVFY", LibWebBrowserDialog::sceWebBrowserDialogOpenForPredeterminedContent);
	LIB_FUNC("Cya+jvTtPqg", LibWebBrowserDialog::sceWebBrowserDialogResetCookie);
	LIB_FUNC("TZnDVkP91Rg", LibWebBrowserDialog::sceWebBrowserDialogSetCookie);
	LIB_FUNC("RLhKBOoNyXY", LibWebBrowserDialog::sceWebBrowserDialogSetZoom);
	LIB_FUNC("h1dR-t5ISgg", LibWebBrowserDialog::sceWebBrowserDialogUpdateStatus);
	LIB_FUNC("8r4EJ3FiX4w", LibWebBrowserDialog::Func_F2BE042771625F8C);
}

} // namespace Libs
