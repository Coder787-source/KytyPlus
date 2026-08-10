// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Lightweight guest-platform detection for the launcher. Reads the eboot's
// ELF/SELF header and inspects EI_ABIVERSION (byte 7 of the ELF header):
//   0 -> PlayStation 4
//   2 -> PlayStation 5
// anything else (or unreadable / not found) -> Unknown.
//
// This mirrors Emulator::PlatformDispatch::DetectPlatform in the emulator core
// (src/platformDispatch.cpp) so the launcher and the emulator agree on the same
// game's platform. The launcher uses it only to badge the game list; it never
// executes anything, so a wrong guess here is cosmetic, not fatal.

#include "configuration.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace {

// SELF magic "SELF", ELF magic \x7FELF.
constexpr unsigned char kSelfMagic[4] = {0x53, 0x45, 0x4C, 0x46};
constexpr unsigned char kElfMagic[4]  = {0x7F, 0x45, 0x4C, 0x46};

// In a SELF file the ELF header is embedded; the minimal decrypted-dump SELF
// header is 0x20 bytes, so we scan a small window rather than hard-coding.
constexpr qsizetype kSelfScanWindow = 4096;

// Returns the byte offset of the ELF header within buf, or -1 if not found.
qsizetype FindElfHeaderOffset(const QByteArray& buf) {
	if (buf.size() < 4) {
		return -1;
	}
	if (static_cast<unsigned char>(buf[0]) == kElfMagic[0] &&
	    static_cast<unsigned char>(buf[1]) == kElfMagic[1] &&
	    static_cast<unsigned char>(buf[2]) == kElfMagic[2] &&
	    static_cast<unsigned char>(buf[3]) == kElfMagic[3]) {
		return 0;
	}
	const qsizetype limit = qMin(buf.size(), kSelfScanWindow);
	for (qsizetype i = 4; i + 4 <= limit; ++i) {
		if (static_cast<unsigned char>(buf[i]) == kElfMagic[0] &&
		    static_cast<unsigned char>(buf[i + 1]) == kElfMagic[1] &&
		    static_cast<unsigned char>(buf[i + 2]) == kElfMagic[2] &&
		    static_cast<unsigned char>(buf[i + 3]) == kElfMagic[3]) {
			return i;
		}
	}
	return -1;
}

} // namespace

GuestPlatform DetectGamePlatform(const QString& game_dir, const QString& elf_name) {
	const QString eboot_name = elf_name.isEmpty() ? QStringLiteral("eboot.bin") : elf_name;
	const QString eboot_path = QDir(game_dir).filePath(eboot_name);
	const QFileInfo info(eboot_path);
	if (!info.exists() || !info.isFile()) {
		return GuestPlatform::Unknown;
	}

	QFile file(eboot_path);
	// Read at most the scan window + the 16-byte ELF e_ident. SELF headers
	// put the ELF header within the first few KiB, so a small read suffices.
	if (!file.open(QIODevice::ReadOnly)) {
		return GuestPlatform::Unknown;
	}
	const QByteArray buf = file.read(kSelfScanWindow + 64);
	if (buf.size() < 16) {
		return GuestPlatform::Unknown;
	}

	// If it starts with the SELF magic, the ELF header is embedded further in.
	if (buf.size() >= 4 &&
	    static_cast<unsigned char>(buf[0]) == kSelfMagic[0] &&
	    static_cast<unsigned char>(buf[1]) == kSelfMagic[1] &&
	    static_cast<unsigned char>(buf[2]) == kSelfMagic[2] &&
	    static_cast<unsigned char>(buf[3]) == kSelfMagic[3]) {
		const qsizetype elf_off = FindElfHeaderOffset(buf);
		if (elf_off < 0 || elf_off + 16 > buf.size()) {
			return GuestPlatform::Unknown;
		}
		const unsigned char abi = static_cast<unsigned char>(buf[elf_off + 7]);
		switch (abi) {
			case 0: return GuestPlatform::Ps4;
			case 2: return GuestPlatform::Ps5;
			default: return GuestPlatform::Unknown;
		}
	}

	// Plain ELF (decrypted/raw dump).
	if (static_cast<unsigned char>(buf[0]) == kElfMagic[0] &&
	    static_cast<unsigned char>(buf[1]) == kElfMagic[1] &&
	    static_cast<unsigned char>(buf[2]) == kElfMagic[2] &&
	    static_cast<unsigned char>(buf[3]) == kElfMagic[3]) {
		const unsigned char abi = static_cast<unsigned char>(buf[7]);
		switch (abi) {
			case 0: return GuestPlatform::Ps4;
			case 2: return GuestPlatform::Ps5;
			default: return GuestPlatform::Unknown;
		}
	}

	return GuestPlatform::Unknown;
}
