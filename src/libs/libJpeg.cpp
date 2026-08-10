// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceJpegEnc HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibJpeg {
LIB_VERSION("libSceJpegEnc", 1, "libSceJpegEnc", 1, 1);

static int KYTY_SYSV_ABI sceJpegEncCreate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceJpegEncDelete() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceJpegEncEncode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceJpegEncQueryMemorySize() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitJpeg_1) {
	LIB_FUNC("K+rocojkr-I", LibJpeg::sceJpegEncCreate);
	LIB_FUNC("j1LyMdaM+C0", LibJpeg::sceJpegEncDelete);
	LIB_FUNC("QbrU0cUghEM", LibJpeg::sceJpegEncEncode);
	LIB_FUNC("o6ZgXfFdWXQ", LibJpeg::sceJpegEncQueryMemorySize);
}

} // namespace LibJpeg
} // namespace Libs
