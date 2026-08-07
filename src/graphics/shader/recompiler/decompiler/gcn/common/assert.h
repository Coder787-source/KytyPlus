// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compatibility shim mapping shadPS4's assert macros onto Kyty's logging.
//
// IMPORTANT DEVIATION FROM UPSTREAM: shadPS4's ASSERT/UNREACHABLE/UNIMPLEMENTED
// hard-abort the process. This ported GCN decoder is used *diagnostically* —
// Kyty decodes PS4 shaders to surface coverage gaps (see ReportPs4CompatGap),
// not to run them to completion yet. Aborting on the first unsupported opcode
// would crash the emulator on virtually every real PS4 shader and destroy the
// diagnostic value. Therefore these macros LOG a critical message and
// continue instead of aborting. This is intentional and documented; if Kyty
// later promotes the GCN path to full execution, re-evaluate whether true
// aborts are wanted for genuine invariant violations.
//
// All message-bearing macros use brace-style fmt args ({}, {:#x}, ...).
#pragma once

// Use explicit relative paths so this shim is order-independent: it must
// pull Kyty's REAL common.h (no shim exists for it) and the shim log.h
// (which defines the LOG_* macros and in turn pulls Kyty's real ::Log).
#include "../../../../../../common/common.h"
#include "logging/log.h"

#include <fmt/format.h>
#include <cstdlib>
#include <string_view>

// Keep the SHAD_NO_INLINE marker (used on a couple of helper decls upstream).
#ifdef _MSC_VER
#define SHAD_NO_INLINE __declspec(noinline)
#else
#define SHAD_NO_INLINE __attribute__((noinline))
#endif

#define ASSERT(cond)                                                                              \
	do {                                                                                          \
		if (!(cond)) [[unlikely]] {                                                               \
			if (!::Log::IsSilent()) {                                                             \
				::Log::Write(::Log::Color::BrightRed,                                             \
				             ::fmt::format("GCN decoder assertion failed: {} ({}:{})", #cond,      \
			                            __FILE__, __LINE__));                                     \
			}                                                                                     \
		}                                                                                         \
	} while (false)

#define ASSERT_MSG(cond, ...)                                                                     \
	do {                                                                                          \
		if (!(cond)) [[unlikely]] {                                                               \
			if (!::Log::IsSilent()) {                                                             \
				::Log::Write(::Log::Color::BrightRed,                                             \
				             ::fmt::format("GCN decoder assertion failed: {} ({}:{}): {}", #cond,  \
			                            __FILE__, __LINE__, ::fmt::format(__VA_ARGS__)));          \
			}                                                                                     \
		}                                                                                         \
	} while (false)

// Non-aborting: log and fall through. Callers that need a return value rely
// on the surrounding logic producing a defined (often default) result.
#define UNREACHABLE()                                                                             \
	do {                                                                                          \
		if (!::Log::IsSilent()) {                                                                \
			::Log::Write(::Log::Color::BrightRed,                                                \
			             ::fmt::format("GCN decoder: unreachable hit at {}:{}", __FILE__,         \
			                        __LINE__));                                                  \
		}                                                                                         \
	} while (false)

#define UNREACHABLE_MSG(...)                                                                      \
	do {                                                                                          \
		if (!::Log::IsSilent()) {                                                                \
			::Log::Write(::Log::Color::BrightRed,                                                \
			             ::fmt::format("GCN decoder: unreachable at {}:{}: {}", __FILE__,         \
			                        __LINE__, ::fmt::format(__VA_ARGS__)));                      \
		}                                                                                         \
	} while (false)

#define UNIMPLEMENTED()                                                                           \
	do {                                                                                          \
		if (!::Log::IsSilent()) {                                                                \
			::Log::Write(::Log::Color::BrightRed,                                                \
			             ::fmt::format("GCN decoder: unimplemented at {}:{}", __FILE__,           \
			                        __LINE__));                                                   \
		}                                                                                         \
	} while (false)

#define UNIMPLEMENTED_MSG(...)                                                                    \
	do {                                                                                          \
		if (!::Log::IsSilent()) {                                                                \
			::Log::Write(::Log::Color::BrightRed,                                                \
			             ::fmt::format("GCN decoder: unimplemented at {}:{}: {}", __FILE__,      \
			                        __LINE__, ::fmt::format(__VA_ARGS__)));                      \
		}                                                                                         \
	} while (false)

#define UNIMPLEMENTED_IF(cond)                                                                    \
	do {                                                                                          \
		if (cond) {                                                                              \
			UNIMPLEMENTED();                                                                      \
		}                                                                                         \
	} while (false)

#define UNIMPLEMENTED_IF_MSG(cond, ...)                                                            \
	do {                                                                                          \
		if (cond) {                                                                              \
			UNIMPLEMENTED_MSG(__VA_ARGS__);                                                       \
		}                                                                                         \
	} while (false)

#define DEBUG_ASSERT(cond) ASSERT(cond)
#define DEBUG_ASSERT_MSG(cond, ...) ASSERT_MSG(cond, __VA_ARGS__)

#define ASSERT_OR_EXECUTE(cond, block)                                                            \
	do {                                                                                          \
		ASSERT(cond);                                                                             \
		if (!(cond)) [[unlikely]] {                                                               \
			block                                                                                \
		}                                                                                         \
	} while (false)

#define ASSERT_OR_EXECUTE_MSG(cond, block, ...)                                                   \
	do {                                                                                          \
		ASSERT_MSG(cond, __VA_ARGS__);                                                            \
		if (!(cond)) [[unlikely]] {                                                               \
			block                                                                                \
		}                                                                                         \
	} while (false)