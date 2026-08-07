// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceSigninDialog HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibSigninDialog {

LIB_VERSION("libSceSigninDialog", 1, "libSceSigninDialog", 1, 1);

LIB_DEFINE(InitSigninDialog_1) {
}

} // namespace LibSigninDialog

} // namespace Libs
