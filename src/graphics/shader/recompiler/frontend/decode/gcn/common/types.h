// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compatibility shim: provides the fixed-width type aliases that the ported
// shadPS4 GCN decoder expects from "common/types.h". Isolated to the gcn/
// include root so it does not shadow Kyty's own src/common headers elsewhere.
#pragma once

#include <array>
#include <cstdint>

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;
using u128 = std::array<std::uint64_t, 2>;
static_assert(sizeof(u128) == 16, "u128 must be 128 bits wide");

using VAddr = uintptr_t;
using PAddr = uintptr_t;

#ifndef PS4_SYSV_ABI
#if defined(_MSC_VER)
#define PS4_SYSV_ABI
#else
#define PS4_SYSV_ABI __attribute__((sysv_abi))
#endif
#endif
