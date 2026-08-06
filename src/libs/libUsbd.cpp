// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceUsbd HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibUsbd {
LIB_VERSION("libSceUsbd", 1, "libSceUsbd", 1, 1);


static int KYTY_SYSV_ABI sceUsbdAllocTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdAttachKernelDriver() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdBulkTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdCancelTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdCheckConnected() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdClaimInterface() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdClearHalt() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdClose() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdControlTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdControlTransferGetData() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdControlTransferGetSetup() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdDetachKernelDriver() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdEventHandlerActive() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdEventHandlingOk() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdExit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFillBulkTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFillControlSetup() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFillControlTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFillInterruptTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFillIsoTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFreeConfigDescriptor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFreeDeviceList() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdFreeTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetActiveConfigDescriptor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetBusNumber() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetConfigDescriptor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetConfigDescriptorByValue() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetConfiguration() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetDescriptor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetDeviceAddress() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetDeviceDescriptor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetDeviceList() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetDeviceSpeed() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetIsoPacketBuffer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetMaxIsoPacketSize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetMaxPacketSize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetStringDescriptor() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdGetStringDescriptorAscii() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdHandleEvents() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdHandleEventsLocked() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdHandleEventsTimeout() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdInit() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdInterruptTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdKernelDriverActive() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdLockEvents() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdLockEventWaiters() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdOpen() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdOpenDeviceWithVidPid() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdRefDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdReleaseInterface() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdResetDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdSetConfiguration() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdSetInterfaceAltSetting() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdSetIsoPacketLengths() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdSubmitTransfer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdTryLockEvents() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdUnlockEvents() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdUnlockEventWaiters() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdUnrefDevice() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceUsbdWaitForEvent() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_65F6EF33E38FFF50() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_97F056BAD90AADE7() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_C55104A33B35B264() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI Func_D56B43060720B1E0() {
	PRINT_NAME();
	return OK; // STUBBED
}


LIB_DEFINE(InitUsbd_1) {
	LIB_FUNC("0ktE1PhzGFU", LibUsbd::sceUsbdAllocTransfer);
	LIB_FUNC("BKMEGvfCPyU", LibUsbd::sceUsbdAttachKernelDriver);
	LIB_FUNC("fotb7DzeHYw", LibUsbd::sceUsbdBulkTransfer);
	LIB_FUNC("-KNh1VFIzlM", LibUsbd::sceUsbdCancelTransfer);
	LIB_FUNC("MlW6deWfPp0", LibUsbd::sceUsbdCheckConnected);
	LIB_FUNC("AE+mHBHneyk", LibUsbd::sceUsbdClaimInterface);
	LIB_FUNC("3tPPMo4QRdY", LibUsbd::sceUsbdClearHalt);
	LIB_FUNC("HarYYlaFGJY", LibUsbd::sceUsbdClose);
	LIB_FUNC("RRKFcKQ1Ka4", LibUsbd::sceUsbdControlTransfer);
	LIB_FUNC("XUWtxI31YEY", LibUsbd::sceUsbdControlTransferGetData);
	LIB_FUNC("SEdQo8CFmus", LibUsbd::sceUsbdControlTransferGetSetup);
	LIB_FUNC("Y5go+ha6eDs", LibUsbd::sceUsbdDetachKernelDriver);
	LIB_FUNC("Vw8Hg1CN028", LibUsbd::sceUsbdEventHandlerActive);
	LIB_FUNC("e7gp1xhu6RI", LibUsbd::sceUsbdEventHandlingOk);
	LIB_FUNC("Fq6+0Fm55xU", LibUsbd::sceUsbdExit);
	LIB_FUNC("oHCade-0qQ0", LibUsbd::sceUsbdFillBulkTransfer);
	LIB_FUNC("8KrqbaaPkE0", LibUsbd::sceUsbdFillControlSetup);
	LIB_FUNC("7VGfMerK6m0", LibUsbd::sceUsbdFillControlTransfer);
	LIB_FUNC("t3J5pXxhJlI", LibUsbd::sceUsbdFillInterruptTransfer);
	LIB_FUNC("xqmkjHCEOSY", LibUsbd::sceUsbdFillIsoTransfer);
	LIB_FUNC("Hvd3S--n25w", LibUsbd::sceUsbdFreeConfigDescriptor);
	LIB_FUNC("EQ6SCLMqzkM", LibUsbd::sceUsbdFreeDeviceList);
	LIB_FUNC("-sgi7EeLSO8", LibUsbd::sceUsbdFreeTransfer);
	LIB_FUNC("S1o1C6yOt5g", LibUsbd::sceUsbdGetActiveConfigDescriptor);
	LIB_FUNC("t7WE9mb1TB8", LibUsbd::sceUsbdGetBusNumber);
	LIB_FUNC("Dkm5qe8j3XE", LibUsbd::sceUsbdGetConfigDescriptor);
	LIB_FUNC("GQsAVJuy8gM", LibUsbd::sceUsbdGetConfigDescriptorByValue);
	LIB_FUNC("L7FoTZp3bZs", LibUsbd::sceUsbdGetConfiguration);
	LIB_FUNC("-JBoEtvTxvA", LibUsbd::sceUsbdGetDescriptor);
	LIB_FUNC("rsl9KQ-agyA", LibUsbd::sceUsbdGetDevice);
	LIB_FUNC("GjlCrU4GcIY", LibUsbd::sceUsbdGetDeviceAddress);
	LIB_FUNC("bhomgbiQgeo", LibUsbd::sceUsbdGetDeviceDescriptor);
	LIB_FUNC("8qB9Ar4P5nc", LibUsbd::sceUsbdGetDeviceList);
	LIB_FUNC("e1UWb8cWPJM", LibUsbd::sceUsbdGetDeviceSpeed);
	LIB_FUNC("vokkJ0aDf54", LibUsbd::sceUsbdGetIsoPacketBuffer);
	LIB_FUNC("nuIRlpbxauM", LibUsbd::sceUsbdGetMaxIsoPacketSize);
	LIB_FUNC("YJ0cMAlLuxQ", LibUsbd::sceUsbdGetMaxPacketSize);
	LIB_FUNC("g2oYm1DitDg", LibUsbd::sceUsbdGetStringDescriptor);
	LIB_FUNC("t4gUfGsjk+g", LibUsbd::sceUsbdGetStringDescriptorAscii);
	LIB_FUNC("EkqGLxWC-S0", LibUsbd::sceUsbdHandleEvents);
	LIB_FUNC("rt-WeUGibfg", LibUsbd::sceUsbdHandleEventsLocked);
	LIB_FUNC("+wU6CGuZcWk", LibUsbd::sceUsbdHandleEventsTimeout);
	LIB_FUNC("TOhg7P6kTH4", LibUsbd::sceUsbdInit);
	LIB_FUNC("rxi1nCOKWc8", LibUsbd::sceUsbdInterruptTransfer);
	LIB_FUNC("RLf56F-WjKQ", LibUsbd::sceUsbdKernelDriverActive);
	LIB_FUNC("u9yKks02-rA", LibUsbd::sceUsbdLockEvents);
	LIB_FUNC("AeGaY8JrAV4", LibUsbd::sceUsbdLockEventWaiters);
	LIB_FUNC("VJ6oMq-Di2U", LibUsbd::sceUsbdOpen);
	LIB_FUNC("vrQXYRo1Gwk", LibUsbd::sceUsbdOpenDeviceWithVidPid);
	LIB_FUNC("U1t1SoJvV-A", LibUsbd::sceUsbdRefDevice);
	LIB_FUNC("REfUTmTchMw", LibUsbd::sceUsbdReleaseInterface);
	LIB_FUNC("hvMn0QJXj5g", LibUsbd::sceUsbdResetDevice);
	LIB_FUNC("FhU9oYrbXoA", LibUsbd::sceUsbdSetConfiguration);
	LIB_FUNC("DVCQW9o+ki0", LibUsbd::sceUsbdSetInterfaceAltSetting);
	LIB_FUNC("dJxro8Nzcjk", LibUsbd::sceUsbdSetIsoPacketLengths);
	LIB_FUNC("L0EHgZZNVas", LibUsbd::sceUsbdSubmitTransfer);
	LIB_FUNC("TcXVGc-LPbQ", LibUsbd::sceUsbdTryLockEvents);
	LIB_FUNC("RA2D9rFH-Uw", LibUsbd::sceUsbdUnlockEvents);
	LIB_FUNC("1DkGvUQYFKI", LibUsbd::sceUsbdUnlockEventWaiters);
	LIB_FUNC("OULgIo1zAsA", LibUsbd::sceUsbdUnrefDevice);
	LIB_FUNC("ys2e9VRBPrY", LibUsbd::sceUsbdWaitForEvent);
	LIB_FUNC("ZfbvM+OP-1A", LibUsbd::Func_65F6EF33E38FFF50);
	LIB_FUNC("l-BWutkKrec", LibUsbd::Func_97F056BAD90AADE7);
	LIB_FUNC("xVEEozs1smQ", LibUsbd::Func_C55104A33B35B264);
	LIB_FUNC("1WtDBgcgseA", LibUsbd::Func_D56B43060720B1E0);
}
} // namespace LibUsbd

} // namespace Libs
