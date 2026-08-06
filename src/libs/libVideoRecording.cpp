// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceVideoRecording HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibVideoRecording {
LIB_VERSION("libSceVideoRecording", 1, "libSceVideoRecording", 1, 1);


static int KYTY_SYSV_ABI sceVideoRecordingSetInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}


LIB_DEFINE(InitVideoRecording_1) {
	LIB_FUNC("Fc8qxlKINYQ", LibVideoRecording::sceVideoRecordingSetInfo);
}
} // namespace LibVideoRecording

} // namespace Libs
