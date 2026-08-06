// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceIme HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibIme {

static int KYTY_SYSV_ABI FinalizeImeModule() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI InitializeImeModule() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeCheckFilterText() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeCheckRemoteEventParam() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeCheckUpdateTextInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeConfigGet() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeConfigSet() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeConfirmCandidate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDicAddWord() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDicDeleteLearnDics() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDicDeleteUserDics() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDicDeleteWord() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDicGetWords() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDicReplaceWord() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeDisableController() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeFilterText() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeForTestFunction() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeGetPanelPositionAndForm() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeGetPanelSize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeKeyboardOpenInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeKeyboardUpdate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeOpenInternal() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeParamInit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeSetCandidateIndex() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeSetCaret() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeSetText() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeSetTextGeometry() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshClearPreedit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshConfirmPreedit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshDisableController() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshGetPanelPositionAndForm() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshInformConfirmdString() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshInformConfirmdString2() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSendTextInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetCaretGeometry() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetCaretIndexInPreedit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetPanelPosition() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetParam() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetPreeditGeometry() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetSelectGeometry() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshSetSelectionText() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshUpdate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshUpdateContext() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceImeVshUpdateContext2() {
	PRINT_NAME();
	return OK; // STUBBED
}

LIB_VERSION("libSceIme", 1, "libSceIme", 1, 1);

} // namespace LibIme

LIB_DEFINE(InitIme_1) {
	LIB_FUNC("mN+ZoSN-8hQ", LibIme::FinalizeImeModule);
	LIB_FUNC("uTW+63goeJs", LibIme::InitializeImeModule);
	LIB_FUNC("Lf3DeGWC6xg", LibIme::sceImeCheckFilterText);
	LIB_FUNC("zHuMUGb-AQI", LibIme::sceImeCheckRemoteEventParam);
	LIB_FUNC("OTb0Mg+1i1k", LibIme::sceImeCheckUpdateTextInfo);
	LIB_FUNC("TmVP8LzcFcY", LibIme::sceImeClose);
	LIB_FUNC("Ho5NVQzpKHo", LibIme::sceImeConfigGet);
	LIB_FUNC("P5dPeiLwm-M", LibIme::sceImeConfigSet);
	LIB_FUNC("tKLmVIUkpyM", LibIme::sceImeConfirmCandidate);
	LIB_FUNC("NYDsL9a0oEo", LibIme::sceImeDicAddWord);
	LIB_FUNC("l01GKoyiQrY", LibIme::sceImeDicDeleteLearnDics);
	LIB_FUNC("E2OcGgi-FPY", LibIme::sceImeDicDeleteUserDics);
	LIB_FUNC("JAiMBkOTYKI", LibIme::sceImeDicDeleteWord);
	LIB_FUNC("JoPdCUXOzMU", LibIme::sceImeDicGetWords);
	LIB_FUNC("FuEl46uHDyo", LibIme::sceImeDicReplaceWord);
	LIB_FUNC("E+f1n8e8DAw", LibIme::sceImeDisableController);
	LIB_FUNC("evjOsE18yuI", LibIme::sceImeFilterText);
	LIB_FUNC("wVkehxutK-U", LibIme::sceImeForTestFunction);
	LIB_FUNC("T6FYjZXG93o", LibIme::sceImeGetPanelPositionAndForm);
	LIB_FUNC("ziPDcIjO0Vk", LibIme::sceImeGetPanelSize);
	LIB_FUNC("oYkJlMK51SA", LibIme::sceImeKeyboardOpenInternal);
	LIB_FUNC("3Hx2Uw9xnv8", LibIme::sceImeKeyboardUpdate);
	LIB_FUNC("RPydv-Jr1bc", LibIme::sceImeOpen);
	LIB_FUNC("16UI54cWRQk", LibIme::sceImeOpenInternal);
	LIB_FUNC("WmYDzdC4EHI", LibIme::sceImeParamInit);
	LIB_FUNC("TQaogSaqkEk", LibIme::sceImeSetCandidateIndex);
	LIB_FUNC("WLxUN2WMim8", LibIme::sceImeSetCaret);
	LIB_FUNC("ieCNrVrzKd4", LibIme::sceImeSetText);
	LIB_FUNC("TXYHFRuL8UY", LibIme::sceImeSetTextGeometry);
	LIB_FUNC("oOwl47ouxoM", LibIme::sceImeVshClearPreedit);
	LIB_FUNC("gtoTsGM9vEY", LibIme::sceImeVshClose);
	LIB_FUNC("wTKF4mUlSew", LibIme::sceImeVshConfirmPreedit);
	LIB_FUNC("rM-1hkuOhh0", LibIme::sceImeVshDisableController);
	LIB_FUNC("42xMaQ+GLeQ", LibIme::sceImeVshGetPanelPositionAndForm);
	LIB_FUNC("ZmmV6iukhyo", LibIme::sceImeVshInformConfirmdString);
	LIB_FUNC("EQBusz6Uhp8", LibIme::sceImeVshInformConfirmdString2);
	LIB_FUNC("LBicRa-hj3A", LibIme::sceImeVshOpen);
	LIB_FUNC("-IAOwd2nO7g", LibIme::sceImeVshSendTextInfo);
	LIB_FUNC("qDagOjvJdNk", LibIme::sceImeVshSetCaretGeometry);
	LIB_FUNC("tNOlmxee-Nk", LibIme::sceImeVshSetCaretIndexInPreedit);
	LIB_FUNC("rASXozKkQ9g", LibIme::sceImeVshSetPanelPosition);
	LIB_FUNC("idvMaIu5H+k", LibIme::sceImeVshSetParam);
	LIB_FUNC("ga5GOgThbjo", LibIme::sceImeVshSetPreeditGeometry);
	LIB_FUNC("RuSca8rS6yA", LibIme::sceImeVshSetSelectGeometry);
	LIB_FUNC("J7COZrgSFRA", LibIme::sceImeVshSetSelectionText);
	LIB_FUNC("WqAayyok5p0", LibIme::sceImeVshUpdate);
	LIB_FUNC("O7Fdd+Oc-qQ", LibIme::sceImeVshUpdateContext);
	LIB_FUNC("fwcPR7+7Rks", LibIme::sceImeVshUpdateContext2);
}

} // namespace Libs
