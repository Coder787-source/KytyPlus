// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compatibility shim mapping shadPS4's logging macros onto Kyty's Log
// subsystem. The ported decoder uses brace-style fmt args ({:#x}, {}, ...),
// so we route through fmt::format (not Kyty's printf-style LOGF) and feed the
// result to Log::Write. This keeps the upstream file bodies byte-identical.
#pragma once

#include "common/common.h"

// IMPORTANT: include Kyty's REAL logging header explicitly via a relative
// path that climbs out of the gcn/ shim directory. With gcn/ on the include
// path BEFORE src/ (see gcn/CMakeLists.txt), the bare #include "common/logging/log.h"
// would resolve back to THIS shim file (infinite recursion, skipped by #pragma
// once) and ::Log would never be defined. The explicit relative path forces the
// real header (which declares namespace Log, Log::Color, Log::IsSilent,
// Log::Write) to be pulled in unambiguously.
#include "../../../../../../../common/logging/log.h"

#include <fmt/format.h>
#include <string>

// The shadPS4 LOG_* macros take (Class, fmt, args...). We discard the class
// tag (Kyty has no per-class logging) and format with brace syntax.
#define LOG_CRITICAL(class_tag, ...)                                                               \
	do {                                                                                           \
		if (!::Log::IsSilent()) {                                                                  \
			::Log::Write(::Log::Color::BrightRed, ::fmt::format(__VA_ARGS__));                     \
		}                                                                                          \
	} while (false)
#define LOG_ERROR(class_tag, ...)                                                                  \
	do {                                                                                           \
		if (!::Log::IsSilent()) {                                                                  \
			::Log::Write(::Log::Color::Red, ::fmt::format(__VA_ARGS__));                           \
		}                                                                                          \
	} while (false)
#define LOG_WARNING(class_tag, ...)                                                                \
	do {                                                                                           \
		if (!::Log::IsSilent()) {                                                                  \
			::Log::Write(::Log::Color::Yellow, ::fmt::format(__VA_ARGS__));                         \
		}                                                                                          \
	} while (false)
#define LOG_INFO(class_tag, ...)                                                                   \
	do {                                                                                           \
		if (!::Log::IsSilent()) {                                                                  \
			::Log::Write(::fmt::format(__VA_ARGS__));                                               \
		}                                                                                          \
	} while (false)
#define LOG_DEBUG(class_tag, ...) LOG_INFO(class_tag, __VA_ARGS__)
#define LOG_TRACE(class_tag, ...) LOG_INFO(class_tag, __VA_ARGS__)