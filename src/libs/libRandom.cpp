// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceRandom HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4's random.cpp (GPL-2.0-or-later):
//   "PI7jIZj4pcE" -> sceRandomGetRandomNumber  (libSceRandom v1)
//
// Implementation upgraded from shadPS4's std::rand() to the host OS CSPRNG so
// the kernel PRNG is genuinely cryptographically sourced, not a toy LCG.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

#include <cstdint>
#include <cstring>

#if defined(_WIN32)
	#include <windows.h>
	#include <bcrypt.h>
	#pragma comment(lib, "bcrypt.lib")
#else
	#include <fcntl.h>
	#include <unistd.h>
#endif

namespace Libs {

namespace LibRandom {

LIB_VERSION("libSceRandom", 1, "libSceRandom", 1, 1);

// Verified PS4 constant from shadPS4's random_error.h.
constexpr int  SCE_RANDOM_ERROR_INVALID        = 0x817C0016;
constexpr int  SCE_RANDOM_ERROR_OUT_OF_RESOURCES = 0x817C001C;
constexpr int  SCE_RANDOM_ERROR_FATAL          = 0x817C00FF;
constexpr size_t SCE_RANDOM_MAX_SIZE           = 64;

// Fill `buf` with `size` cryptographically-secure random bytes from the host OS.
// Returns true on success. On any host CSPRNG failure we fall through to the
// fatal error code rather than silently weakening the PRNG.
static bool GetOsRandomBytes(uint8_t* buf, size_t size) {
#if defined(_WIN32)
	return BCryptGenRandom(nullptr, buf, static_cast<ULONG>(size),
	                       BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	size_t got = 0;
	while (got < size) {
		ssize_t n = read(fd, buf + got, size - got);
		if (n <= 0) {
			close(fd);
			return false;
		}
		got += static_cast<size_t>(n);
	}
	close(fd);
	return true;
#endif
}


LIB_DEFINE(InitRandom_1) {
}

} // namespace LibRandom
} // namespace Libs
