// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project


// SPDX-License-Identifier: GPL-2.0-or-later


//


// PS4 libSceRazorCpu HLE, ported into KytyPlus's LIB_FUNC framework.


// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are


// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs


// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.





#include "libs/libs.h"


#include "loader/symbolDatabase.h"

#include "libs/errno.h"





#include <common/abi.h>





namespace Libs {





namespace LibRazorCpu {


LIB_VERSION("libSceRazorCpu", 1, "libSceRazorCpu", 1, 1);






static int KYTY_SYSV_ABI sceRazorCpuBeginLogicalFileAccess() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuDisableFiberUserMarkers() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuEndLogicalFileAccess() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuFiberLogNameChange() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuFiberSwitch() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuFlushOccurred() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuGetDataTagStorageSize() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuGpuMarkerSync() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuInitDataTags() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuInitializeGpuMarkerContext() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuInitializeInternal() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuJobManagerDispatch() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuJobManagerJob() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuJobManagerSequence() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuNamedSync() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuPlotValue() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuPopMarker() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuPushMarker() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuPushMarkerStatic() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuResizeTaggedBuffer() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuSetPopMarkerCallback() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuSetPushMarkerCallback() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuSetPushMarkerStaticCallback() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuShutdownDataTags() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuStartCaptureInternal() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuStopCaptureInternal() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuSync() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuTagArray() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuTagBuffer() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuUnTagBuffer() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuWorkloadRunBegin() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuWorkloadRunEnd() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuWorkloadSubmit() {


	PRINT_NAME();


	return OK; // STUBBED


}





static int KYTY_SYSV_ABI sceRazorCpuWriteBookmark() {


	PRINT_NAME();


	return OK; // STUBBED


}







LIB_DEFINE(InitRazorCpu_1) {


	LIB_FUNC("JFzLJBlYIJE", LibRazorCpu::sceRazorCpuBeginLogicalFileAccess);


	LIB_FUNC("SfRTRZ1Sh+U", LibRazorCpu::sceRazorCpuDisableFiberUserMarkers);


	LIB_FUNC("gVioM9cbiDs", LibRazorCpu::sceRazorCpuEndLogicalFileAccess);


	LIB_FUNC("G90IIOtgFQ0", LibRazorCpu::sceRazorCpuFiberLogNameChange);


	LIB_FUNC("PAytDtFGpqY", LibRazorCpu::sceRazorCpuFiberSwitch);


	LIB_FUNC("sPhrQD31ClM", LibRazorCpu::sceRazorCpuFlushOccurred);


	LIB_FUNC("B782NptkGUc", LibRazorCpu::sceRazorCpuGetDataTagStorageSize);


	LIB_FUNC("EH9Au2RlSrE", LibRazorCpu::sceRazorCpuGpuMarkerSync);


	LIB_FUNC("A7oRMdaOJP8", LibRazorCpu::sceRazorCpuInitDataTags);


	LIB_FUNC("NFwh-J-BrI0", LibRazorCpu::sceRazorCpuInitializeGpuMarkerContext);


	LIB_FUNC("ElNyedXaa4o", LibRazorCpu::sceRazorCpuInitializeInternal);


	LIB_FUNC("dnEdyY4+klQ", LibRazorCpu::sceRazorCpuJobManagerDispatch);


	LIB_FUNC("KP+TBWGHlgs", LibRazorCpu::sceRazorCpuJobManagerJob);


	LIB_FUNC("9FowWFMEIM8", LibRazorCpu::sceRazorCpuJobManagerSequence);


	LIB_FUNC("XCuZoBSVFG8", LibRazorCpu::sceRazorCpuNamedSync);


	LIB_FUNC("njGikRrxkC0", LibRazorCpu::sceRazorCpuPlotValue);


	LIB_FUNC("YpkGsMXP3ew", LibRazorCpu::sceRazorCpuPopMarker);


	LIB_FUNC("zw+celG7zSI", LibRazorCpu::sceRazorCpuPushMarker);


	LIB_FUNC("uZrOwuNJX-M", LibRazorCpu::sceRazorCpuPushMarkerStatic);


	LIB_FUNC("D0yUjM33QqU", LibRazorCpu::sceRazorCpuResizeTaggedBuffer);


	LIB_FUNC("jqYWaTfgZs0", LibRazorCpu::sceRazorCpuSetPopMarkerCallback);


	LIB_FUNC("DJsHcEb94n0", LibRazorCpu::sceRazorCpuSetPushMarkerCallback);


	LIB_FUNC("EZtqozPTS4M", LibRazorCpu::sceRazorCpuSetPushMarkerStaticCallback);


	LIB_FUNC("emklx7RK-LY", LibRazorCpu::sceRazorCpuShutdownDataTags);


	LIB_FUNC("TIytAjYeaik", LibRazorCpu::sceRazorCpuStartCaptureInternal);


	LIB_FUNC("jWpkVWdMrsM", LibRazorCpu::sceRazorCpuStopCaptureInternal);


	LIB_FUNC("Ax7NjOzctIM", LibRazorCpu::sceRazorCpuSync);


	LIB_FUNC("we3oTKSPSTw", LibRazorCpu::sceRazorCpuTagArray);


	LIB_FUNC("vyjdThnQfQQ", LibRazorCpu::sceRazorCpuTagBuffer);


	LIB_FUNC("0yNHPIkVTmw", LibRazorCpu::sceRazorCpuUnTagBuffer);


	LIB_FUNC("Crha9LvwvJM", LibRazorCpu::sceRazorCpuWorkloadRunBegin);


	LIB_FUNC("q1GxBfGHO0s", LibRazorCpu::sceRazorCpuWorkloadRunEnd);


	LIB_FUNC("6rUvx-6QmYc", LibRazorCpu::sceRazorCpuWorkloadSubmit);


	LIB_FUNC("G3brhegfyNg", LibRazorCpu::sceRazorCpuWriteBookmark);


}


} // namespace LibRazorCpu





} // namespace Libs


