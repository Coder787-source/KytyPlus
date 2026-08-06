// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compatibility shim mapping shadPS4's logging macros onto Kyty's Log
// subsystem. The ported decoder uses brace-style fmt args ({:#x}, {}, ...),
// so we route through fmt::format (not Kyty's printf-style LOGF) and feed the
// result to Log::Write. This keeps the upstream file bodies byte-identical.
#pragma once

#include "common/common.h"
#include "common/logging/log.h"

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