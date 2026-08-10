// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceNgs2 HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibNgs2 {
LIB_VERSION("libSceNgs2", 1, "libSceNgs2", 1, 1);

static int KYTY_SYSV_ABI sceNgs2CustomRackGetModuleInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2FftInit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2FftProcess() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2FftQuerySize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2GetWaveformFrameInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2JobSchedulerResetOption() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ModuleArrayEnumItems() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ModuleEnumConfigs() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ModuleQueueEnumItems() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ParseWaveformFile() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ParseWaveformUser() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2RackGetInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2RackGetUserData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2RackQueryInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2RackRunCommands() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2RackSetUserData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ReportRegisterHandler() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2ReportUnregisterHandler() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemEnumHandles() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemEnumRackHandles() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemGetInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemGetUserData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemLock() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemQueryInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemRunCommands() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemSetLoudThreshold() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemSetSampleRate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemSetUserData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2SystemUnlock() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamCreate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamCreateWithAllocator() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamDestroy() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamQueryBufferSize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamQueryInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamResetOption() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2StreamRunCommands() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2VoiceGetMatrixInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2VoiceGetOwner() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2VoiceGetPortInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceNgs2VoiceQueryInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}



LIB_DEFINE(InitNgs2_1) {
	LIB_FUNC("6qN1zaEZuN0", LibNgs2::sceNgs2CustomRackGetModuleInfo);
	LIB_FUNC("Kg1MA5j7KFk", LibNgs2::sceNgs2FftInit);
	LIB_FUNC("D8eCqBxSojA", LibNgs2::sceNgs2FftProcess);
	LIB_FUNC("-YNfTO6KOMY", LibNgs2::sceNgs2FftQuerySize);
	LIB_FUNC("ekGJmmoc8j4", LibNgs2::sceNgs2GetWaveformFrameInfo);
	LIB_FUNC("BcoPfWfpvVI", LibNgs2::sceNgs2JobSchedulerResetOption);
	LIB_FUNC("EEemGEQCjO8", LibNgs2::sceNgs2ModuleArrayEnumItems);
	LIB_FUNC("TaoNtmMKkXQ", LibNgs2::sceNgs2ModuleEnumConfigs);
	LIB_FUNC("ve6bZi+1sYQ", LibNgs2::sceNgs2ModuleQueueEnumItems);
	LIB_FUNC("iprCTXPVWMI", LibNgs2::sceNgs2ParseWaveformFile);
	LIB_FUNC("t9T0QM17Kvo", LibNgs2::sceNgs2ParseWaveformUser);
	LIB_FUNC("M4LYATRhRUE", LibNgs2::sceNgs2RackGetInfo);
	LIB_FUNC("Mn4XNDg03XY", LibNgs2::sceNgs2RackGetUserData);
	LIB_FUNC("TZqb8E-j3dY", LibNgs2::sceNgs2RackQueryInfo);
	LIB_FUNC("MI2VmBx2RbM", LibNgs2::sceNgs2RackRunCommands);
	LIB_FUNC("JNTMIaBIbV4", LibNgs2::sceNgs2RackSetUserData);
	LIB_FUNC("uBIN24Tv2MI", LibNgs2::sceNgs2ReportRegisterHandler);
	LIB_FUNC("nPzb7Ly-VjE", LibNgs2::sceNgs2ReportUnregisterHandler);
	LIB_FUNC("vubFP0T6MP0", LibNgs2::sceNgs2SystemEnumHandles);
	LIB_FUNC("U-+7HsswcIs", LibNgs2::sceNgs2SystemEnumRackHandles);
	LIB_FUNC("vU7TQ62pItw", LibNgs2::sceNgs2SystemGetInfo);
	LIB_FUNC("4lFaRxd-aLs", LibNgs2::sceNgs2SystemGetUserData);
	LIB_FUNC("gThZqM5PYlQ", LibNgs2::sceNgs2SystemLock);
	LIB_FUNC("3oIK7y7O4k0", LibNgs2::sceNgs2SystemQueryInfo);
	LIB_FUNC("gXiormHoZZ4", LibNgs2::sceNgs2SystemRunCommands);
	LIB_FUNC("Wdlx0ZFTV9s", LibNgs2::sceNgs2SystemSetLoudThreshold);
	LIB_FUNC("-tbc2SxQD60", LibNgs2::sceNgs2SystemSetSampleRate);
	LIB_FUNC("GZB2v0XnG0k", LibNgs2::sceNgs2SystemSetUserData);
	LIB_FUNC("JXRC5n0RQls", LibNgs2::sceNgs2SystemUnlock);
	LIB_FUNC("sU2St3agdjg", LibNgs2::sceNgs2StreamCreate);
	LIB_FUNC("I+RLwaauggA", LibNgs2::sceNgs2StreamCreateWithAllocator);
	LIB_FUNC("bfoMXnTRtwE", LibNgs2::sceNgs2StreamDestroy);
	LIB_FUNC("dxulc33msHM", LibNgs2::sceNgs2StreamQueryBufferSize);
	LIB_FUNC("rfw6ufRsmow", LibNgs2::sceNgs2StreamQueryInfo);
	LIB_FUNC("q+2W8YdK0F8", LibNgs2::sceNgs2StreamResetOption);
	LIB_FUNC("qQHCi9pjDps", LibNgs2::sceNgs2StreamRunCommands);
	LIB_FUNC("jjBVvPN9964", LibNgs2::sceNgs2VoiceGetMatrixInfo);
	LIB_FUNC("W-Z8wWMBnhk", LibNgs2::sceNgs2VoiceGetOwner);
	LIB_FUNC("WCayTgob7-o", LibNgs2::sceNgs2VoiceGetPortInfo);
	LIB_FUNC("9eic4AmjGVI", LibNgs2::sceNgs2VoiceQueryInfo);
}

} // namespace LibNgs2
} // namespace Libs
