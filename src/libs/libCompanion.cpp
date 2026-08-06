// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libCompanion HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Exports span multiple
// PS4 module names; each gets its own sub-namespace + LIB_DEFINE, mirroring
// libKernel.cpp's structure. Entry points are stubbed (return ORBIS_OK).

#include "libs/libs.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace CompanionHttpd {

LIB_VERSION("libSceCompanionHttpd", 1, "libSceCompanionHttpd", 1, 1);

static int KYTY_SYSV_ABI sceCompanionHttpdAddHeader() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdGet2ndScreenStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdGetEvent() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdGetUserId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdInitialize() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdInitialize2() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdOptParamInitialize() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdRegisterRequestBodyReceptionCallback() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdRegisterRequestCallback() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdRegisterRequestCallback2() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdSetBody() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdSetStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdStart() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdStop() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdTerminate() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionHttpdUnregisterRequestCallback() {
	PRINT_NAME();
	return OK;
}

} // namespace CompanionHttpd

LIB_DEFINE(InitCompanionHttpd_1) {
	LIB_FUNC("8pWltDG7h6A", CompanionHttpd::sceCompanionHttpdAddHeader);
	LIB_FUNC("B-QBMeFdNgY", CompanionHttpd::sceCompanionHttpdGet2ndScreenStatus);
	LIB_FUNC("Vku4big+IYM", CompanionHttpd::sceCompanionHttpdGetEvent);
	LIB_FUNC("0SySxcuVNG0", CompanionHttpd::sceCompanionHttpdGetUserId);
	LIB_FUNC("ykNpWs3ktLY", CompanionHttpd::sceCompanionHttpdInitialize);
	LIB_FUNC("OA6FbORefbo", CompanionHttpd::sceCompanionHttpdInitialize2);
	LIB_FUNC("r-2-a0c7Kfc", CompanionHttpd::sceCompanionHttpdOptParamInitialize);
	LIB_FUNC("fHNmij7kAUM", CompanionHttpd::sceCompanionHttpdRegisterRequestBodyReceptionCallback);
	LIB_FUNC("OaWw+IVEdbI", CompanionHttpd::sceCompanionHttpdRegisterRequestCallback);
	LIB_FUNC("-0c9TCTwnGs", CompanionHttpd::sceCompanionHttpdRegisterRequestCallback2);
	LIB_FUNC("h3OvVxzX4qM", CompanionHttpd::sceCompanionHttpdSetBody);
	LIB_FUNC("w7oz0AWHpT4", CompanionHttpd::sceCompanionHttpdSetStatus);
	LIB_FUNC("k7F0FcDM-Xc", CompanionHttpd::sceCompanionHttpdStart);
	LIB_FUNC("0SCgzfVQHpo", CompanionHttpd::sceCompanionHttpdStop);
	LIB_FUNC("+-du9tWgE9s", CompanionHttpd::sceCompanionHttpdTerminate);
	LIB_FUNC("ZSHiUfYK+QI", CompanionHttpd::sceCompanionHttpdUnregisterRequestBodyReceptionCallback);
	LIB_FUNC("xweOi2QT-BE", CompanionHttpd::sceCompanionHttpdUnregisterRequestCallback);
}

namespace CompanionUtil {

LIB_VERSION("libSceCompanionUtil", 1, "libSceCompanionUtil", 1, 1);

static int KYTY_SYSV_ABI sceCompanionUtilGetEvent() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionUtilGetRemoteOskEvent() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionUtilInitialize() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionUtilOptParamInitialize() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceCompanionUtilTerminate() {
	PRINT_NAME();
	return OK;
}

} // namespace CompanionUtil

LIB_DEFINE(InitCompanionUtil_1) {
	LIB_FUNC("cE5Msy11WhU", CompanionUtil::sceCompanionUtilGetEvent);
	LIB_FUNC("MaVrz79mT5o", CompanionUtil::sceCompanionUtilGetRemoteOskEvent);
	LIB_FUNC("xb1xlIhf0QY", CompanionUtil::sceCompanionUtilInitialize);
	LIB_FUNC("IPN-FRSrafk", CompanionUtil::sceCompanionUtilOptParamInitialize);
	LIB_FUNC("H1fYQd5lFAI", CompanionUtil::sceCompanionUtilTerminate);
}

} // namespace Libs
