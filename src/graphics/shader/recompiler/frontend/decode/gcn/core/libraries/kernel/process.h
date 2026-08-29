// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compatibility shim: shadPS4's decode.cpp transitively includes
// core/libraries/kernel/process.h but uses no symbol from it. This empty stub
// satisfies the include in isolation without pulling Kyty's full kernel
// headers into the GCN decoder translation unit.
#pragma once

#include "common/types.h"
