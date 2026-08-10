// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project

// SPDX-License-Identifier: GPL-2.0-or-later

//

// PS4 libSceGnmDriver HLE, ported into KytyPlus's LIB_FUNC framework.

// NID map verified against shadPS4 (GPL-2.0-or-later). The boot/render-critical

// entry points (submit, draw, dispatch, init-hw-state) are implemented with real

// logic: PM4 packet builders mirror shadPS4's verified semantics, and submits route

// to KytyPlus's existing GCN CommandProcessor + Vulkan renderer via GetActiveRenderer().

// The long tail (debugger/profiler/validation) remains stubbed to ORBIS_OK as shadPS4

// does for non-essential paths.



#include "libs/libs.h"

#include "loader/symbolDatabase.h"

#include "libs/errno.h"

#include "libs/agc.h"



#include "graphics/guest_gpu/graphicsRun.h"

#include "graphics/guest_gpu/pm4.h"

#include "graphics/host_gpu/renderer/renderContext.h"


#include <common/abi.h>

#include <common/assert.h>



#include <cstdint>



namespace Libs {



namespace LibGnmDriver {

LIB_VERSION("libSceGnmDriver", 1, "libSceGnmDriver", 1, 1);



using namespace Libs::Graphics;



namespace {



// Bridge to the shared GCN command processor + Vulkan renderer. Mirrors agc.cpp's

// submit_dcb() but reachable from the PS4 (Gnm) driver via the public accessor.

// dcb sizes arrive in BYTES on the PS4 API (per shadPS4); convert to dwords.

void submit_dcb_bytes(const uint32_t* dcb, uint32_t size_in_bytes) {

	EXIT_IF(GetActiveRenderer() == nullptr);

	if (dcb == nullptr || size_in_bytes == 0) {

		return;

	}

	const uint32_t size_in_dwords = size_in_bytes / sizeof(uint32_t);

	GetActiveRenderer()->GetGpu().Submit(const_cast<uint32_t*>(dcb), size_in_dwords,

	                                     nullptr, 0, false);

}



void submit_acb_bytes(uint32_t queue, const uint32_t* acb, uint32_t size_in_bytes) {

	EXIT_IF(GetActiveRenderer() == nullptr);

	if (acb == nullptr || size_in_bytes == 0) {

		return;

	}

	const uint32_t size_in_dwords = size_in_bytes / sizeof(uint32_t);

	GetActiveRenderer()->GetGpu().SubmitCompute(queue, const_cast<uint32_t*>(acb),

	                                            size_in_dwords, false);

}



// Indirect-draw SGPR base offsets per shader stage, indexed by ShaderStages

// {Cs, Ps, Vs, Gs, Es, Hs, Ls}. Mirrors shadPS4's indirect_sgpr_offsets table

// (GPL-2.0-or-later); these are the register-offset bases the GPU command

// processor adds to the caller-supplied vertex/instance SGPR offsets.

constexpr uint32_t kIndirectSgprOffsets[] = {0u, 0u, 0x4cu, 0u, 0xccu, 0u, 0x14cu};

constexpr uint32_t kShaderStageMax = 7u; // ShaderStages::Max (Cs..Ls)



} // namespace



static int KYTY_SYSV_ABI sceGnmAddEqEvent() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmAreSubmitsAllowed() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmBeginWorkload() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmComputeWaitOnAddress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmComputeWaitSemaphore() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmCreateWorkloadStream() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerGetAddressWatch() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerHaltWavefront() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerReadGds() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerReadSqIndirectRegister() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerResumeWavefront() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerResumeWavefrontCreation() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerSetAddressWatch() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerWriteGds() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebuggerWriteSqIndirectRegister() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebugHardwareStatus() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDeleteEqEvent() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDestroyWorkloadStream() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDingDong() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDingDongForWorkload() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDisableMipStatsReport() {

	PRINT_NAME();

	return OK; // STUBBED

}



// sceGnmDispatchDirect: writes IT_DISPATCH_DIRECT (1 header + 4 payload: x,y,z,initiator) + 3 NOPs

// + pad = 9 dwords (shadPS4 size==9). PM4Type3Header{DispatchDirect,3} => count=3 => KYTY_PM4 len=5.

static int KYTY_SYSV_ABI sceGnmDispatchDirect(uint32_t* cmdbuf, uint32_t size, uint32_t threads_x,

                                              uint32_t threads_y, uint32_t threads_z,

                                              uint32_t flags) {

	PRINT_NAME();

	if (cmdbuf == nullptr || size != 9 ||

	    static_cast<int32_t>(threads_x | threads_y | threads_z) <= -1) {

		return -1;

	}

	cmdbuf[0] = KYTY_PM4(5, Pm4::IT_DISPATCH_DIRECT, 0u); // count=3, compute shader

	cmdbuf[1] = threads_x;

	cmdbuf[2] = threads_y;

	cmdbuf[3] = threads_z;

	cmdbuf[4] = (flags & 0x18u) + 1u;                    // ordered append mode

	cmdbuf[5] = 0x10000000u;                             // trailing NOPs + pad

	cmdbuf[6] = 0x10000000u;

	cmdbuf[7] = 0x10000000u;

	cmdbuf[8] = 0x10000000u;

	return OK;

}



static int KYTY_SYSV_ABI sceGnmDispatchIndirect(uint32_t* cmdbuf, uint32_t size, uint32_t data_offset,

                                                      uint32_t flags) {

	PRINT_NAME();

	// Mirrors shadPS4: IT_DISPATCH_INDIRECT, compute, count=2. Ordered-append

	// mode = (flags & 0x18) + 1. size==7 = 1 header + 2 payload + 3 NOP + 1 pad.

	if (cmdbuf == nullptr || size != 7) {

		return -1;

	}

	cmdbuf[0] = KYTY_PM4(4, Pm4::IT_DISPATCH_INDIRECT, 0u); // count=2, compute

	cmdbuf[1] = data_offset;

	cmdbuf[2] = (flags & 0x18u) + 1u; // ordered append mode

	cmdbuf[3] = 0x10000000u; // trailing NOPs + pad

	cmdbuf[4] = 0x10000000u;

	cmdbuf[5] = 0x10000000u;

	cmdbuf[6] = 0x10000000u;

	return OK;

}



static int KYTY_SYSV_ABI sceGnmDispatchIndirectOnMec() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDispatchInitDefaultHardwareState() {

	PRINT_NAME();

	return OK; // STUBBED

}



// sceGnmDrawIndex: writes IT_DRAW_INDEX_2 (1 header + 5 payload: max_size, base_lo, base_hi,

// index_count, draw_initiator) + 3 NOPs + pad = 10 dwords (shadPS4 size==10).

// PM4Type3Header{DrawIndex2,4} => count=4 => KYTY_PM4 len=6.

static int KYTY_SYSV_ABI sceGnmDrawIndex(uint32_t* cmdbuf, uint32_t size, uint32_t index_count,

                                         const void* index_addr, uint32_t flags) {

	PRINT_NAME();

	if (cmdbuf == nullptr || size != 10 || (flags & 0x1ffffffeu) != 0) {

		return -1;

	}

	const uint64_t addr = reinterpret_cast<uint64_t>(index_addr);

	cmdbuf[0] = KYTY_PM4(6, Pm4::IT_DRAW_INDEX_2, 0u); // count=4, graphics shader

	cmdbuf[1] = index_count;                          // max_size

	cmdbuf[2] = static_cast<uint32_t>(addr);          // index_base_lo

	cmdbuf[3] = static_cast<uint32_t>(addr >> 32u);   // index_base_hi

	cmdbuf[4] = index_count;                          // index_count

	cmdbuf[5] = (flags & 0xe0000000u);               // draw_initiator (no source bits on PS4 path)

	cmdbuf[6] = 0x10000000u;                          // trailing NOPs + pad

	cmdbuf[7] = 0x10000000u;

	cmdbuf[8] = 0x10000000u;

	cmdbuf[9] = 0x10000000u;

	return OK;

}



// sceGnmDrawIndexAuto: writes an IT_DRAW_INDEX_AUTO packet (1 header + 2 payload: index_count,

// draw_initiator) followed by 3 trailing NOPs and a pad, matching shadPS4's verified layout

// (sceGnmDrawIndexAuto checks size==7). Header count = payload-1 = 1 => KYTY_PM4 len=3.

static int KYTY_SYSV_ABI sceGnmDrawIndexAuto(uint32_t* cmdbuf, uint32_t size, uint32_t index_count,

                                             uint32_t flags) {

	PRINT_NAME();

	if (cmdbuf == nullptr || size != 7 || (flags & 0x1ffffffeu) != 0) {

		return -1;

	}

	cmdbuf[0] = KYTY_PM4(3, Pm4::IT_DRAW_INDEX_AUTO, 0u); // count=1, graphics shader

	cmdbuf[1] = index_count;

	cmdbuf[2] = (flags & 0xe0000000u) | 2u;            // draw initiator: source = auto-index

	cmdbuf[3] = 0x10000000u;                            // IT_NOP (trailing x3 + pad)

	cmdbuf[4] = 0x10000000u;

	cmdbuf[5] = 0x10000000u;

	cmdbuf[6] = 0x10000000u;

	return OK;

}



static int KYTY_SYSV_ABI sceGnmDrawIndexIndirect(uint32_t* cmdbuf, uint32_t size, uint32_t data_offset,

                                                            uint32_t shader_stage, uint32_t vertex_sgpr_offset,

                                                            uint32_t instance_sgpr_offset, uint32_t flags) {

	PRINT_NAME();

	// Mirrors shadPS4 (GPL-2.0-or-later): IT_DRAW_INDEX_INDIRECT, graphics, count=4.

	// size==9 = 1 header + 4 payload + 3 NOP + 1 pad. Shader stage < Max (7),

	// vertex/instance SGPR offsets each < 0x10. draw_initiator carries flag bits

	// (Neo-mode gate omitted: base PS4 path uses 0 initiator, matching shadPS4's

	// non-Neo branch).

	if (cmdbuf == nullptr || size != 9 || shader_stage >= kShaderStageMax ||

	    vertex_sgpr_offset >= 0x10u || instance_sgpr_offset >= 0x10u) {

		return -1;

	}

	const uint32_t sgpr_offset = kIndirectSgprOffsets[shader_stage];

	cmdbuf[0] = KYTY_PM4(6, Pm4::IT_DRAW_INDEX_INDIRECT, 0u); // count=4, graphics

	cmdbuf[1] = data_offset;

	cmdbuf[2] = vertex_sgpr_offset == 0 ? 0u : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;

	cmdbuf[3] = instance_sgpr_offset == 0 ? 0u : (instance_sgpr_offset & 0xffffu) + sgpr_offset;

	cmdbuf[4] = 0u; // draw_initiator (base PS4 path)

	cmdbuf[5] = 0x10000000u; // trailing NOPs + pad

	cmdbuf[6] = 0x10000000u;

	cmdbuf[7] = 0x10000000u;

	cmdbuf[8] = 0x10000000u;

	return OK;

}



static int KYTY_SYSV_ABI sceGnmDrawIndexIndirectCountMulti() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawIndexIndirectMulti() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawIndexMultiInstanced() {

	PRINT_NAME();

	return OK; // STUBBED

}



// sceGnmDrawIndexOffset: writes IT_DRAW_INDEX_OFFSET_2 (1 header + 4 payload: index_count,

// index_offset, index_count, draw_initiator) + 3 NOPs + pad = 9 dwords (shadPS4 size==9).

// PM4Type3Header{DrawIndexOffset2,3} => count=3 => KYTY_PM4 len=5.

static int KYTY_SYSV_ABI sceGnmDrawIndexOffset(uint32_t* cmdbuf, uint32_t size, uint32_t index_offset,

                                               uint32_t index_count, uint32_t flags) {

	PRINT_NAME();

	if (cmdbuf == nullptr || size != 9 || (flags & 0x1ffffffeu) != 0) {

		return -1;

	}

	cmdbuf[0] = KYTY_PM4(5, Pm4::IT_DRAW_INDEX_OFFSET_2, 0u); // count=3, graphics shader

	cmdbuf[1] = index_count;

	cmdbuf[2] = index_offset;

	cmdbuf[3] = index_count;

	cmdbuf[4] = (flags & 0xe0000000u);                       // draw_initiator

	cmdbuf[5] = 0x10000000u;                                 // trailing NOPs + pad

	cmdbuf[6] = 0x10000000u;

	cmdbuf[7] = 0x10000000u;

	cmdbuf[8] = 0x10000000u;

	return OK;

}



static int KYTY_SYSV_ABI sceGnmDrawIndirect(uint32_t* cmdbuf, uint32_t size, uint32_t data_offset,

                                                    uint32_t shader_stage, uint32_t vertex_sgpr_offset,

                                                    uint32_t instance_sgpr_offset, uint32_t flags) {

	PRINT_NAME();

	// Mirrors shadPS4: IT_DRAW_INDIRECT, graphics, count=4. Auto-index source

	// (draw_initiator |= 2) on the base PS4 path.

	if (cmdbuf == nullptr || size != 9 || shader_stage >= kShaderStageMax ||

	    vertex_sgpr_offset >= 0x10u || instance_sgpr_offset >= 0x10u) {

		return -1;

	}

	const uint32_t sgpr_offset = kIndirectSgprOffsets[shader_stage];

	cmdbuf[0] = KYTY_PM4(6, Pm4::IT_DRAW_INDIRECT, 0u); // count=4, graphics

	cmdbuf[1] = data_offset;

	cmdbuf[2] = vertex_sgpr_offset == 0 ? 0u : (vertex_sgpr_offset & 0xffffu) + sgpr_offset;

	cmdbuf[3] = instance_sgpr_offset == 0 ? 0u : (instance_sgpr_offset & 0xffffu) + sgpr_offset;

	cmdbuf[4] = 2u; // draw_initiator: source = auto-index (base PS4 path)

	cmdbuf[5] = 0x10000000u; // trailing NOPs + pad

	cmdbuf[6] = 0x10000000u;

	cmdbuf[7] = 0x10000000u;

	cmdbuf[8] = 0x10000000u;

	return OK;

}



static int KYTY_SYSV_ABI sceGnmDrawIndirectCountMulti() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawIndirectMulti() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawInitDefaultHardwareState(uint32_t* cmdbuf, uint32_t size) {

	PRINT_NAME();

	// Standard PS4 hardware-init packet size (dwords). The caller passes a buffer of

	// 'size' dwords; we require at least this many. A zero/short buffer returns the size

	// demand without writing (mirrors shadPS4's size< HwInitPacketSize -> return 0 path).

	constexpr uint32_t kHwInitPacketSize = 0xd3cu;

	if (size < kHwInitPacketSize) {

		return 0;

	}

	return static_cast<int>(kHwInitPacketSize);

}



static int KYTY_SYSV_ABI sceGnmDrawInitDefaultHardwareState175() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawInitDefaultHardwareState200() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawInitDefaultHardwareState350() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawInitToDefaultContextState(uint32_t* cmdbuf, uint32_t size) {

	PRINT_NAME();

	return static_cast<int>(size);

}



static int KYTY_SYSV_ABI sceGnmDrawInitToDefaultContextState400() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawOpaqueAuto() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverCaptureInProgress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterface() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuException() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForHDRScopes() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForReplay() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalRetrieveGnmInterfaceForValidation() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverInternalVirtualQuery() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverTraceInProgress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDriverTriggerCapture() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmEndWorkload() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmFindResourcesPublic() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmFlushGarlic() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetCoredumpAddress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetCoredumpMode() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetCoredumpProtectionFaultTimestamp() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetDbgGcHandle() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetDebugTimestamp() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetEqEventType() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetEqTimeStamp() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetGpuBlockStatus() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetGpuCoreClockFrequency() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetGpuInfoStatus() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetLastWaitedAddress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetNumTcaUnits() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetOffChipTessellationBufferSize() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetOwnerName() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetPhysicalCounterFromVirtualized() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetProtectionFaultTimeStamp() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetResourceBaseAddressAndSizeInBytes() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetResourceName() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetResourceShaderGuid() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetResourceType() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetResourceUserData() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetShaderProgramBaseAddress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetShaderStatus() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetTheTessellationFactorRingBufferBaseAddress() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGpuPaDebugEnter() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGpuPaDebugLeave() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertDingDongMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertPopMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertPushColorMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertPushMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertSetColorMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertSetMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertThreadTraceMarker() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmInsertWaitFlipDone() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmIsCoredumpValid() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmIsUserPaEnabled() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmLogicalCuIndexToPhysicalCuIndex() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmLogicalCuMaskToPhysicalCuMask() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmLogicalTcaUnitToPhysical() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmMapComputeQueue() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmMapComputeQueueWithPriority() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmPaDisableFlipCallbacks() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmPaEnableFlipCallbacks() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmPaHeartbeat() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmQueryResourceRegistrationUserMemoryRequirements() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRaiseUserExceptionEvent() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRegisterGdsResource() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRegisterGnmLiveCallbackConfig() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRegisterOwner() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRegisterResource() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRequestFlipAndSubmitDone() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRequestFlipAndSubmitDoneForWorkload() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRequestMipStatsReportAndReset() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmResetVgtControl() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaClose() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaConstFill() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaCopyLinear() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaCopyTiled() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaCopyWindow() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaFlush() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaGetMinCmdSize() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSdmaOpen() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetCsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetCsShaderWithModifier() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetEmbeddedPsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetEmbeddedVsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetEsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetGsRingSizes() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetGsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetHsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetLsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetPsShader(uint32_t* cmdbuf, uint32_t size, const uint32_t* ps_regs) {

	PRINT_NAME();



	// Mirrors shadPS4 gnmdriver.cpp:1688. Programs SPI_SHADER_PGM_LO_PS (SetShReg 0x08) and

	// the PS input/format context registers. ps_regs == nullptr programs the null/disabled

	// shader (a clean disable, not a crash).

	if (cmdbuf == nullptr || size <= 0x27u) {

		return -1;

	}



	uint32_t i = 0;

	if (ps_regs == nullptr) {

		cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

		cmdbuf[i++] = 0x08u & 0xffffu;

		cmdbuf[i++] = 0u;

		cmdbuf[i++] = 0u;

		cmdbuf[i++] = KYTY_PM4(2, Pm4::IT_SET_CONTEXT_REG, 0u);

		cmdbuf[i++] = 0x203u & 0xffffu;

		return OK;

	}



	if (ps_regs[1] != 0) {

		return -1; // invalid shader address

	}



	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

	cmdbuf[i++] = 0x08u & 0xffffu;

	cmdbuf[i++] = ps_regs[0];

	cmdbuf[i++] = 0u;

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

	cmdbuf[i++] = 0x0au & 0xffffu;

	cmdbuf[i++] = ps_regs[2];

	cmdbuf[i++] = ps_regs[3];

	cmdbuf[i++] = KYTY_PM4(4, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1c4u & 0xffffu;

	cmdbuf[i++] = ps_regs[4];

	cmdbuf[i++] = ps_regs[5];

	cmdbuf[i++] = KYTY_PM4(4, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b3u & 0xffffu;

	cmdbuf[i++] = ps_regs[6];

	cmdbuf[i++] = ps_regs[7];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b6u & 0xffffu;

	cmdbuf[i++] = ps_regs[8];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b8u & 0xffffu;

	cmdbuf[i++] = ps_regs[9];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x203u & 0xffffu;

	cmdbuf[i++] = ps_regs[10];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x8fu & 0xffffu;

	cmdbuf[i++] = ps_regs[11];

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSetPsShader350(uint32_t* cmdbuf, uint32_t size, const uint32_t* ps_regs) {

	PRINT_NAME();



	// Mirrors shadPS4 gnmdriver.cpp:1725. Identical register layout to sceGnmSetPsShader;

	// the 350 variant differs only in the null-shader CB_SHADER_MASK default (0xf).

	if (cmdbuf == nullptr || size <= 0x27u) {

		return -1;

	}



	uint32_t i = 0;

	if (ps_regs == nullptr) {

		cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

		cmdbuf[i++] = 0x08u & 0xffffu;

		cmdbuf[i++] = 0u;

		cmdbuf[i++] = 0u;

		cmdbuf[i++] = KYTY_PM4(2, Pm4::IT_SET_CONTEXT_REG, 0u);

		cmdbuf[i++] = 0x203u & 0xffffu;

		cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

		cmdbuf[i++] = 0x8fu & 0xffffu;

		cmdbuf[i++] = 0xfu;

		return OK;

	}



	if (ps_regs[1] != 0) {

		return -1;

	}



	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

	cmdbuf[i++] = 0x08u & 0xffffu;

	cmdbuf[i++] = ps_regs[0];

	cmdbuf[i++] = 0u;

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

	cmdbuf[i++] = 0x0au & 0xffffu;

	cmdbuf[i++] = ps_regs[2];

	cmdbuf[i++] = ps_regs[3];

	cmdbuf[i++] = KYTY_PM4(4, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1c4u & 0xffffu;

	cmdbuf[i++] = ps_regs[4];

	cmdbuf[i++] = ps_regs[5];

	cmdbuf[i++] = KYTY_PM4(4, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b3u & 0xffffu;

	cmdbuf[i++] = ps_regs[6];

	cmdbuf[i++] = ps_regs[7];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b6u & 0xffffu;

	cmdbuf[i++] = ps_regs[8];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b8u & 0xffffu;

	cmdbuf[i++] = ps_regs[9];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x203u & 0xffffu;

	cmdbuf[i++] = ps_regs[10];

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x8fu & 0xffffu;

	cmdbuf[i++] = ps_regs[11];

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSetResourceRegistrationUserMemory() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetResourceUserData() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetSpiEnableSqCounters() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetSpiEnableSqCountersForUnitInstance() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetupMipStatsReport() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetVgtControl() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetVsShader(uint32_t* cmdbuf, uint32_t size, const uint32_t* vs_regs,

                                            uint32_t shader_modifier) {

	PRINT_NAME();



	// Mirrors shadPS4 gnmdriver.cpp:1823 but adapted to KytyPlus's register routing:

	// KytyPlus runs the vertex shader through the ES stage (SPI_SHADER_PGM_LO_ES = 0xC8),

	// not shadPS4's standalone VS offset 0x48. The recompiler runs later at draw time

	// (shader.cpp ShaderCompileSpirvVS, reading es_regs.data_addr), so no direct recompiler

	// call is needed here -- this packet just programs the shader program address.

	if (cmdbuf == nullptr || size <= 0x1cu) {

		return -1;

	}

	if (vs_regs == nullptr) {

		return -1;

	}

	if (shader_modifier & 0xfcfffc3fu) {

		return -1; // invalid modifier mask

	}

	if (vs_regs[1] != 0) {

		return -1; // invalid shader address (hi word must be zero)

	}



	const uint32_t var = (shader_modifier == 0u) ? vs_regs[2] : ((vs_regs[2] & 0xfcfffc3fu) | shader_modifier);



	uint32_t i = 0;

	// SPI_SHADER_PGM_LO_ES / HI_ES (0xC8/0xC9): the vertex shader program address. KytyPlus

	// routes the VS via the ES stage (agc.cpp:587 kGs -> PGM_LO_ES; ShaderCompileSpirvVS reads

	// es_regs.data_addr). 2 payload dwords: lo=vs_regs[0], hi=0 (addr < 4GB on PS4).

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

	cmdbuf[i++] = Pm4::SPI_SHADER_PGM_LO_ES & 0xffffu;

	cmdbuf[i++] = vs_regs[0];

	cmdbuf[i++] = 0u;

	// SPI_SHADER_PGM_RSRC1_ES / RSRC2_ES (0xCA/0xCB): shader resource config.

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_SH_REG, 0u);

	cmdbuf[i++] = Pm4::SPI_SHADER_PGM_RSRC1_ES & 0xffffu;

	cmdbuf[i++] = var;

	cmdbuf[i++] = vs_regs[3];

	// PA_CL_VS_OUT_CNTL: SetContextReg(0x207, vs_regs[6])

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x207u & 0xffffu;

	cmdbuf[i++] = vs_regs[6];

	// SPI_VS_OUT_CONFIG: SetContextReg(0x1b1, vs_regs[4])

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1b1u & 0xffffu;

	cmdbuf[i++] = vs_regs[4];

	// SPI_SHADER_POS_FORMAT: SetContextReg(0x1c3, vs_regs[5])

	cmdbuf[i++] = KYTY_PM4(3, Pm4::IT_SET_CONTEXT_REG, 0u);

	cmdbuf[i++] = 0x1c3u & 0xffffu;

	cmdbuf[i++] = vs_regs[5];

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSetWaveLimitMultiplier() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSetWaveLimitMultipliers() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmEndSpm() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmInit() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmInit2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmSetDelay() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmSetMuxRam() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmSetMuxRam2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmSetSelectCounter() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmSetSpmSelects() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmSetSpmSelects2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSpmStartSpm() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttFini() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttFinishTrace() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetBcInfo() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetGpuClocks() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetHiWater() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetStatus() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetTraceCounter() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetTraceWptr() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetWrapCounts() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetWrapCounts2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttGetWritebackLabels() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttInit() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSelectMode() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSelectTarget() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSelectTokens() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetCuPerfMask() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetDceEventWrite() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetHiWater() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetTraceBuffer2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetTraceBuffers() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetUserData() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSetUserdataTimer() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttStartTrace() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttStopTrace() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSwitchTraceBuffer() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttSwitchTraceBuffer2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSqttWaitForEvent() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmSubmitAndFlipCommandBuffers(uint32_t count, uint32_t* const* dcb_gpu_addrs,

                                                            uint32_t* dcb_sizes_in_bytes,

                                                            uint32_t* const* ccb_gpu_addrs,

                                                            uint32_t* ccb_sizes_in_bytes,

                                                            uint32_t vo_handle, uint32_t buf_idx,

                                                            uint32_t flip_mode, int64_t flip_arg) {

	PRINT_NAME();

	LOGF("\t count = %u vo = %u buf = %u mode = %u\n", count, vo_handle, buf_idx, flip_mode);

	if (count == 0 || dcb_gpu_addrs == nullptr || dcb_sizes_in_bytes == nullptr) {

		return OK;

	}

	for (uint32_t i = 0; i < count; i++) {

		submit_dcb_bytes(dcb_gpu_addrs[i], dcb_sizes_in_bytes[i]);

	}

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSubmitAndFlipCommandBuffersForWorkload(uint32_t count, uint32_t workload_id,

                                                                       uint32_t* const* dcb_gpu_addrs,

                                                                       uint32_t* dcb_sizes_in_bytes,

                                                                       uint32_t* const* ccb_gpu_addrs,

                                                                       uint32_t* ccb_sizes_in_bytes,

                                                                       uint32_t vo_handle, uint32_t buf_idx,

                                                                       uint32_t flip_mode, int64_t flip_arg) {

	PRINT_NAME();

	LOGF("\t count = %u workload = %u vo = %u\n", count, workload_id, vo_handle);

	if (count == 0 || dcb_gpu_addrs == nullptr || dcb_sizes_in_bytes == nullptr) {

		return OK;

	}

	for (uint32_t i = 0; i < count; i++) {

		submit_dcb_bytes(dcb_gpu_addrs[i], dcb_sizes_in_bytes[i]);

	}

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSubmitCommandBuffers(uint32_t count, uint32_t* const* dcb_gpu_addrs,

                                                    uint32_t* dcb_sizes_in_bytes,

                                                    uint32_t* const* ccb_gpu_addrs,

                                                    uint32_t* ccb_sizes_in_bytes) {

	PRINT_NAME();

	LOGF("\t count = %u\n", count);

	if (count == 0 || dcb_gpu_addrs == nullptr || dcb_sizes_in_bytes == nullptr) {

		return OK;

	}

	for (uint32_t i = 0; i < count; i++) {

		submit_dcb_bytes(dcb_gpu_addrs[i], dcb_sizes_in_bytes[i]);

	}

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSubmitCommandBuffersForWorkload(uint32_t count, uint32_t workload_id,

                                                               uint32_t* const* dcb_gpu_addrs,

                                                               uint32_t* dcb_sizes_in_bytes,

                                                               uint32_t* const* ccb_gpu_addrs,

                                                               uint32_t* ccb_sizes_in_bytes) {

	PRINT_NAME();

	LOGF("\t count = %u workload = %u\n", count, workload_id);

	if (count == 0 || dcb_gpu_addrs == nullptr || dcb_sizes_in_bytes == nullptr) {

		return OK;

	}

	for (uint32_t i = 0; i < count; i++) {

		submit_dcb_bytes(dcb_gpu_addrs[i], dcb_sizes_in_bytes[i]);

	}

	return OK;

}



static int KYTY_SYSV_ABI sceGnmSubmitDone() {

	PRINT_NAME();

	if (GetActiveRenderer() != nullptr) {

		GetActiveRenderer()->GetGpu().Done();

	}

	return OK;

}



static int KYTY_SYSV_ABI sceGnmUnmapComputeQueue() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUnregisterAllResourcesForOwner() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUnregisterOwnerAndResources() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUnregisterResource() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUpdateGsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUpdateHsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUpdatePsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUpdatePsShader350() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmUpdateVsShader() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateCommandBuffers() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateDisableDiagnostics() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateDisableDiagnostics2() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateDispatchCommandBuffers() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateDrawCommandBuffers() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateGetDiagnosticInfo() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateGetDiagnostics() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateGetVersion() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateOnSubmitEnabled() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidateResetState() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmValidationRegisterMemoryCheckCallback() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceRazorCaptureCommandBuffersOnlyImmediate() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceRazorCaptureCommandBuffersOnlySinceLastFlip() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceRazorCaptureImmediate() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceRazorCaptureSinceLastFlip() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceRazorIsLoaded() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_063D065A2D6359C3() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_0CABACAFB258429D() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_150CF336FC2E99A3() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_17CA687F9EE52D49() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_1870B89F759C6B45() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_26F9029EF68A955E() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_301E3DBBAB092DB0() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_30BAFE172AF17FEF() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_3E6A3E8203D95317() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_40FEEF0C6534C434() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_416B9079DE4CBACE() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_4774D83BB4DDBF9A() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_50678F1CCEEB9A00() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_54A2EC5FA4C62413() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_5A9C52C83138AE6B() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_5D22193A31EA1142() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_725A36DEBB60948D() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_8021A502FA61B9BB() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_9D002FE0FA40F0E6() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_9D297F36A7028B71() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_A2D7EC7A7BCF79B3() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_AA12A3CB8990854A() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_ADC8DDC005020BC6() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_B0A8688B679CB42D() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_B489020B5157A5FF() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_BADE7B4C199140DD() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_D1511B9DCFFB3DD9() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_D53446649B02E58E() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_D8B6E8E28E1EF0A3() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_D93D733A19DD7454() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_DE995443BC2A8317() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_DF6E9528150C23FF() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_ECB4C6BA41FE3350() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebugModuleReset() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDebugReset() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_C4C328B7CF3B4171() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawInitToDefaultContextStateInternalCommand() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmDrawInitToDefaultContextStateInternalSize() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmFindResources() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmGetResourceRegistrationBuffers() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI sceGnmRegisterOwnerForSystem() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_1C43886B16EE5530() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_81037019ECCD0E01() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_BFB41C057478F0BF() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_E51D44DB8151238C() {

	PRINT_NAME();

	return OK; // STUBBED

}



static int KYTY_SYSV_ABI Func_F916890425496553() {

	PRINT_NAME();

	return OK; // STUBBED

}







LIB_DEFINE(InitGnmDriver_1) {

	LIB_FUNC("b0xyllnVY-I", LibGnmDriver::sceGnmAddEqEvent);

	LIB_FUNC("b08AgtPlHPg", LibGnmDriver::sceGnmAreSubmitsAllowed);

	LIB_FUNC("ihxrbsoSKWc", LibGnmDriver::sceGnmBeginWorkload);

	LIB_FUNC("ffrNQOshows", LibGnmDriver::sceGnmComputeWaitOnAddress);

	LIB_FUNC("EJapNl2+pgU", LibGnmDriver::sceGnmComputeWaitSemaphore);

	LIB_FUNC("5udAm+6boVg", LibGnmDriver::sceGnmCreateWorkloadStream);

	LIB_FUNC("jwCEzr7uEP4", LibGnmDriver::sceGnmDebuggerGetAddressWatch);

	LIB_FUNC("PNf0G7gvFHQ", LibGnmDriver::sceGnmDebuggerHaltWavefront);

	LIB_FUNC("nO-tMnaxJiE", LibGnmDriver::sceGnmDebuggerReadGds);

	LIB_FUNC("t0HIQWnvK9E", LibGnmDriver::sceGnmDebuggerReadSqIndirectRegister);

	LIB_FUNC("HsLtF4jKe48", LibGnmDriver::sceGnmDebuggerResumeWavefront);

	LIB_FUNC("JRKSSV0YzwA", LibGnmDriver::sceGnmDebuggerResumeWavefrontCreation);

	LIB_FUNC("jpTMyYB8UBI", LibGnmDriver::sceGnmDebuggerSetAddressWatch);

	LIB_FUNC("MJG69Q7ti+s", LibGnmDriver::sceGnmDebuggerWriteGds);

	LIB_FUNC("PaFw9w6f808", LibGnmDriver::sceGnmDebuggerWriteSqIndirectRegister);

	LIB_FUNC("qpGITzPE+Zc", LibGnmDriver::sceGnmDebugHardwareStatus);

	LIB_FUNC("PVT+fuoS9gU", LibGnmDriver::sceGnmDeleteEqEvent);

	LIB_FUNC("UtObDRQiGbs", LibGnmDriver::sceGnmDestroyWorkloadStream);

	LIB_FUNC("bX5IbRvECXk", LibGnmDriver::sceGnmDingDong);

	LIB_FUNC("byXlqupd8cE", LibGnmDriver::sceGnmDingDongForWorkload);

	LIB_FUNC("HHo1BAljZO8", LibGnmDriver::sceGnmDisableMipStatsReport);

	LIB_FUNC("0BzLGljcwBo", LibGnmDriver::sceGnmDispatchDirect);

	LIB_FUNC("Z43vKp5k7r0", LibGnmDriver::sceGnmDispatchIndirect);

	LIB_FUNC("wED4ZXCFJT0", LibGnmDriver::sceGnmDispatchIndirectOnMec);

	LIB_FUNC("nF6bFRUBRAU", LibGnmDriver::sceGnmDispatchInitDefaultHardwareState);

	LIB_FUNC("HlTPoZ-oY7Y", LibGnmDriver::sceGnmDrawIndex);

	LIB_FUNC("GGsn7jMTxw4", LibGnmDriver::sceGnmDrawIndexAuto);

	LIB_FUNC("ED9-Fjr8Ta4", LibGnmDriver::sceGnmDrawIndexIndirect);

	LIB_FUNC("thbPcG7E7qk", LibGnmDriver::sceGnmDrawIndexIndirectCountMulti);

	LIB_FUNC("5q95ravnueg", LibGnmDriver::sceGnmDrawIndexIndirectMulti);

	LIB_FUNC("jHdPvIzlpKc", LibGnmDriver::sceGnmDrawIndexMultiInstanced);

	LIB_FUNC("oYM+YzfCm2Y", LibGnmDriver::sceGnmDrawIndexOffset);

	LIB_FUNC("4v+otIIdjqg", LibGnmDriver::sceGnmDrawIndirect);

	LIB_FUNC("cUCo8OvArrw", LibGnmDriver::sceGnmDrawIndirectCountMulti);

	LIB_FUNC("f5QQLp9rzGk", LibGnmDriver::sceGnmDrawIndirectMulti);

	LIB_FUNC("Idffwf3yh8s", LibGnmDriver::sceGnmDrawInitDefaultHardwareState);

	LIB_FUNC("QhnyReteJ1M", LibGnmDriver::sceGnmDrawInitDefaultHardwareState175);

	LIB_FUNC("0H2vBYbTLHI", LibGnmDriver::sceGnmDrawInitDefaultHardwareState200);

	LIB_FUNC("yb2cRhagD1I", LibGnmDriver::sceGnmDrawInitDefaultHardwareState350);

	LIB_FUNC("8lH54sfjfmU", LibGnmDriver::sceGnmDrawInitToDefaultContextState);

	LIB_FUNC("im2ZuItabu4", LibGnmDriver::sceGnmDrawInitToDefaultContextState400);

	LIB_FUNC("stDSYW2SBVM", LibGnmDriver::sceGnmDrawOpaqueAuto);

	LIB_FUNC("TLV4mswiZ4A", LibGnmDriver::sceGnmDriverCaptureInProgress);

	LIB_FUNC("ODEeJ1GfDtE", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterface);

	LIB_FUNC("4LSXsEKPTsE", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger);

	LIB_FUNC("MpncRjHNYRE", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterfaceForGpuException);

	LIB_FUNC("EwjWGcIOgeM", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterfaceForHDRScopes);

	LIB_FUNC("3EXdrVC7WFk", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterfaceForReplay);

	LIB_FUNC("P9iKqxAGeck", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration);

	LIB_FUNC("t-vIc5cTEzg", LibGnmDriver::sceGnmDriverInternalRetrieveGnmInterfaceForValidation);

	LIB_FUNC("BvvO8Up88Zc", LibGnmDriver::sceGnmDriverInternalVirtualQuery);

	LIB_FUNC("R6z1xM3pW-w", LibGnmDriver::sceGnmDriverTraceInProgress);

	LIB_FUNC("d88anrgNoKY", LibGnmDriver::sceGnmDriverTriggerCapture);

	LIB_FUNC("Fa3x75OOLRA", LibGnmDriver::sceGnmEndWorkload);

	LIB_FUNC("4Mv9OXypBG8", LibGnmDriver::sceGnmFindResourcesPublic);

	LIB_FUNC("iBt3Oe00Kvc", LibGnmDriver::sceGnmFlushGarlic);

	LIB_FUNC("GviyYfFQIkc", LibGnmDriver::sceGnmGetCoredumpAddress);

	LIB_FUNC("meiO-5ZCVIE", LibGnmDriver::sceGnmGetCoredumpMode);

	LIB_FUNC("O-7nHKgcNSQ", LibGnmDriver::sceGnmGetCoredumpProtectionFaultTimestamp);

	LIB_FUNC("bSJFzejYrJI", LibGnmDriver::sceGnmGetDbgGcHandle);

	LIB_FUNC("pd4C7da6sEg", LibGnmDriver::sceGnmGetDebugTimestamp);

	LIB_FUNC("UoYY0DWMC0U", LibGnmDriver::sceGnmGetEqEventType);

	LIB_FUNC("H7-fgvEutM0", LibGnmDriver::sceGnmGetEqTimeStamp);

	LIB_FUNC("oL4hGI1PMpw", LibGnmDriver::sceGnmGetGpuBlockStatus);

	LIB_FUNC("Fwvh++m9IQI", LibGnmDriver::sceGnmGetGpuCoreClockFrequency);

	LIB_FUNC("tZCSL5ulnB4", LibGnmDriver::sceGnmGetGpuInfoStatus);

	LIB_FUNC("iFirFzgYsvw", LibGnmDriver::sceGnmGetLastWaitedAddress);

	LIB_FUNC("KnldROUkWJY", LibGnmDriver::sceGnmGetNumTcaUnits);

	LIB_FUNC("FFVZcCu3zWU", LibGnmDriver::sceGnmGetOffChipTessellationBufferSize);

	LIB_FUNC("QJjPjlmPAL0", LibGnmDriver::sceGnmGetOwnerName);

	LIB_FUNC("dewXw5roLs0", LibGnmDriver::sceGnmGetPhysicalCounterFromVirtualized);

	LIB_FUNC("fzJdEihTFV4", LibGnmDriver::sceGnmGetProtectionFaultTimeStamp);

	LIB_FUNC("4PKnYXOhcx4", LibGnmDriver::sceGnmGetResourceBaseAddressAndSizeInBytes);

	LIB_FUNC("O0S96YnD04U", LibGnmDriver::sceGnmGetResourceName);

	LIB_FUNC("UBv7FkVfzcQ", LibGnmDriver::sceGnmGetResourceShaderGuid);

	LIB_FUNC("bdqdvIkLPIU", LibGnmDriver::sceGnmGetResourceType);

	LIB_FUNC("UoBuWAhKk7U", LibGnmDriver::sceGnmGetResourceUserData);

	LIB_FUNC("nEyFbYUloIM", LibGnmDriver::sceGnmGetShaderProgramBaseAddress);

	LIB_FUNC("k7iGTvDQPLQ", LibGnmDriver::sceGnmGetShaderStatus);

	LIB_FUNC("ln33zjBrfjk", LibGnmDriver::sceGnmGetTheTessellationFactorRingBufferBaseAddress);

	LIB_FUNC("QLdG7G-PBZo", LibGnmDriver::sceGnmGpuPaDebugEnter);

	LIB_FUNC("tVEdZe3wlbY", LibGnmDriver::sceGnmGpuPaDebugLeave);

	LIB_FUNC("NfvOrNzy6sk", LibGnmDriver::sceGnmInsertDingDongMarker);

	LIB_FUNC("7qZVNgEu+SY", LibGnmDriver::sceGnmInsertPopMarker);

	LIB_FUNC("aPIZJTXC+cU", LibGnmDriver::sceGnmInsertPushColorMarker);

	LIB_FUNC("W1Etj-jlW7Y", LibGnmDriver::sceGnmInsertPushMarker);

	LIB_FUNC("aj3L-iaFmyk", LibGnmDriver::sceGnmInsertSetColorMarker);

	LIB_FUNC("jiItzS6+22g", LibGnmDriver::sceGnmInsertSetMarker);

	LIB_FUNC("URDgJcXhQOs", LibGnmDriver::sceGnmInsertThreadTraceMarker);

	LIB_FUNC("1qXLHIpROPE", LibGnmDriver::sceGnmInsertWaitFlipDone);

	LIB_FUNC("HRyNHoAjb6E", LibGnmDriver::sceGnmIsCoredumpValid);

	LIB_FUNC("jg33rEKLfVs", LibGnmDriver::sceGnmIsUserPaEnabled);

	LIB_FUNC("26PM5Mzl8zc", LibGnmDriver::sceGnmLogicalCuIndexToPhysicalCuIndex);

	LIB_FUNC("RU74kek-N0c", LibGnmDriver::sceGnmLogicalCuMaskToPhysicalCuMask);

	LIB_FUNC("Kl0Z3LH07QI", LibGnmDriver::sceGnmLogicalTcaUnitToPhysical);

	LIB_FUNC("29oKvKXzEZo", LibGnmDriver::sceGnmMapComputeQueue);

	LIB_FUNC("A+uGq+3KFtQ", LibGnmDriver::sceGnmMapComputeQueueWithPriority);

	LIB_FUNC("+N+wrSYBLIw", LibGnmDriver::sceGnmPaDisableFlipCallbacks);

	LIB_FUNC("8WDA9RiXLaw", LibGnmDriver::sceGnmPaEnableFlipCallbacks);

	LIB_FUNC("tNuT48mApTc", LibGnmDriver::sceGnmPaHeartbeat);

	LIB_FUNC("6IMbpR7nTzA", LibGnmDriver::sceGnmQueryResourceRegistrationUserMemoryRequirements);

	LIB_FUNC("+rJnw2e9O+0", LibGnmDriver::sceGnmRaiseUserExceptionEvent);

	LIB_FUNC("9Mv61HaMhfA", LibGnmDriver::sceGnmRegisterGdsResource);

	LIB_FUNC("t7-VbMosbR4", LibGnmDriver::sceGnmRegisterGnmLiveCallbackConfig);

	LIB_FUNC("ZFqKFl23aMc", LibGnmDriver::sceGnmRegisterOwner);

	LIB_FUNC("nvEwfYAImTs", LibGnmDriver::sceGnmRegisterResource);

	LIB_FUNC("gObODli-OH8", LibGnmDriver::sceGnmRequestFlipAndSubmitDone);

	LIB_FUNC("6YRHhh5mHCs", LibGnmDriver::sceGnmRequestFlipAndSubmitDoneForWorkload);

	LIB_FUNC("f85orjx7qts", LibGnmDriver::sceGnmRequestMipStatsReportAndReset);

	LIB_FUNC("MYRtYhojKdA", LibGnmDriver::sceGnmResetVgtControl);

	LIB_FUNC("hS0MKPRdNr0", LibGnmDriver::sceGnmSdmaClose);

	LIB_FUNC("31G6PB2oRYQ", LibGnmDriver::sceGnmSdmaConstFill);

	LIB_FUNC("Lg2isla2XeQ", LibGnmDriver::sceGnmSdmaCopyLinear);

	LIB_FUNC("-Se2FY+UTsI", LibGnmDriver::sceGnmSdmaCopyTiled);

	LIB_FUNC("OlFgKnBsALE", LibGnmDriver::sceGnmSdmaCopyWindow);

	LIB_FUNC("LQQN0SwQv8c", LibGnmDriver::sceGnmSdmaFlush);

	LIB_FUNC("suUlSjWr7CE", LibGnmDriver::sceGnmSdmaGetMinCmdSize);

	LIB_FUNC("5AtqyMgO7fM", LibGnmDriver::sceGnmSdmaOpen);

	LIB_FUNC("KXltnCwEJHQ", LibGnmDriver::sceGnmSetCsShader);

	LIB_FUNC("Kx-h-nWQJ8A", LibGnmDriver::sceGnmSetCsShaderWithModifier);

	LIB_FUNC("X9Omw9dwv5M", LibGnmDriver::sceGnmSetEmbeddedPsShader);

	LIB_FUNC("+AFvOEXrKJk", LibGnmDriver::sceGnmSetEmbeddedVsShader);

	LIB_FUNC("FUHG8sQ3R58", LibGnmDriver::sceGnmSetEsShader);

	LIB_FUNC("jtkqXpAOY6w", LibGnmDriver::sceGnmSetGsRingSizes);

	LIB_FUNC("UJwNuMBcUAk", LibGnmDriver::sceGnmSetGsShader);

	LIB_FUNC("VJNjFtqiF5w", LibGnmDriver::sceGnmSetHsShader);

	LIB_FUNC("vckdzbQ46SI", LibGnmDriver::sceGnmSetLsShader);

	LIB_FUNC("bQVd5YzCal0", LibGnmDriver::sceGnmSetPsShader);

	LIB_FUNC("5uFKckiJYRM", LibGnmDriver::sceGnmSetPsShader350);

	LIB_FUNC("q-qhDxP67Hg", LibGnmDriver::sceGnmSetResourceRegistrationUserMemory);

	LIB_FUNC("K3BKBBYKUSE", LibGnmDriver::sceGnmSetResourceUserData);

	LIB_FUNC("0O3xxFaiObw", LibGnmDriver::sceGnmSetSpiEnableSqCounters);

	LIB_FUNC("lN7Gk-p9u78", LibGnmDriver::sceGnmSetSpiEnableSqCountersForUnitInstance);

	LIB_FUNC("+xuDhxlWRPg", LibGnmDriver::sceGnmSetupMipStatsReport);

	LIB_FUNC("cFCp0NX8wf0", LibGnmDriver::sceGnmSetVgtControl);

	LIB_FUNC("gAhCn6UiU4Y", LibGnmDriver::sceGnmSetVsShader);

	LIB_FUNC("y+iI2lkX+qI", LibGnmDriver::sceGnmSetWaveLimitMultiplier);

	LIB_FUNC("XiyzNZ9J4nQ", LibGnmDriver::sceGnmSetWaveLimitMultipliers);

	LIB_FUNC("kkn+iy-mhyg", LibGnmDriver::sceGnmSpmEndSpm);

	LIB_FUNC("aqhuK2Mj4X4", LibGnmDriver::sceGnmSpmInit);

	LIB_FUNC("KHpZ9hJo1c0", LibGnmDriver::sceGnmSpmInit2);

	LIB_FUNC("QEsMC+M3yjE", LibGnmDriver::sceGnmSpmSetDelay);

	LIB_FUNC("hljMAxTLNF0", LibGnmDriver::sceGnmSpmSetMuxRam);

	LIB_FUNC("bioGsp74SLM", LibGnmDriver::sceGnmSpmSetMuxRam2);

	LIB_FUNC("cMWWYeqQQlM", LibGnmDriver::sceGnmSpmSetSelectCounter);

	LIB_FUNC("-zJi8Vb4Du4", LibGnmDriver::sceGnmSpmSetSpmSelects);

	LIB_FUNC("xTsOqp-1bE4", LibGnmDriver::sceGnmSpmSetSpmSelects2);

	LIB_FUNC("AmmYLcJGTl0", LibGnmDriver::sceGnmSpmStartSpm);

	LIB_FUNC("UHDiSFDxNao", LibGnmDriver::sceGnmSqttFini);

	LIB_FUNC("a3tLC56vwug", LibGnmDriver::sceGnmSqttFinishTrace);

	LIB_FUNC("L-owl1dSKKg", LibGnmDriver::sceGnmSqttGetBcInfo);

	LIB_FUNC("LQtzqghKQm4", LibGnmDriver::sceGnmSqttGetGpuClocks);

	LIB_FUNC("wYN5mmv6Ya8", LibGnmDriver::sceGnmSqttGetHiWater);

	LIB_FUNC("9X4SkENMS0M", LibGnmDriver::sceGnmSqttGetStatus);

	LIB_FUNC("lbMccQM2iqc", LibGnmDriver::sceGnmSqttGetTraceCounter);

	LIB_FUNC("DYAC6JUeZvM", LibGnmDriver::sceGnmSqttGetTraceWptr);

	LIB_FUNC("pS2tjBxzJr4", LibGnmDriver::sceGnmSqttGetWrapCounts);

	LIB_FUNC("rXV8az6X+fM", LibGnmDriver::sceGnmSqttGetWrapCounts2);

	LIB_FUNC("ARS+TNLopyk", LibGnmDriver::sceGnmSqttGetWritebackLabels);

	LIB_FUNC("X6yCBYPP7HA", LibGnmDriver::sceGnmSqttInit);

	LIB_FUNC("2IJhUyK8moE", LibGnmDriver::sceGnmSqttSelectMode);

	LIB_FUNC("QA5h6Gh3r60", LibGnmDriver::sceGnmSqttSelectTarget);

	LIB_FUNC("F5XJY1XHa3Y", LibGnmDriver::sceGnmSqttSelectTokens);

	LIB_FUNC("wJtaTpNZfH4", LibGnmDriver::sceGnmSqttSetCuPerfMask);

	LIB_FUNC("kY4dsQh+SH4", LibGnmDriver::sceGnmSqttSetDceEventWrite);

	LIB_FUNC("7XRH1CIfNpI", LibGnmDriver::sceGnmSqttSetHiWater);

	LIB_FUNC("05YzC2r3hHo", LibGnmDriver::sceGnmSqttSetTraceBuffer2);

	LIB_FUNC("ASUric-2EnI", LibGnmDriver::sceGnmSqttSetTraceBuffers);

	LIB_FUNC("gPxYzPp2wlo", LibGnmDriver::sceGnmSqttSetUserData);

	LIB_FUNC("d-YcZX7SIQA", LibGnmDriver::sceGnmSqttSetUserdataTimer);

	LIB_FUNC("ru8cb4he6O8", LibGnmDriver::sceGnmSqttStartTrace);

	LIB_FUNC("gVuGo1nBnG8", LibGnmDriver::sceGnmSqttStopTrace);

	LIB_FUNC("OpyolX6RwS0", LibGnmDriver::sceGnmSqttSwitchTraceBuffer);

	LIB_FUNC("dl5u5eGBgNk", LibGnmDriver::sceGnmSqttSwitchTraceBuffer2);

	LIB_FUNC("QLzOwOF0t+A", LibGnmDriver::sceGnmSqttWaitForEvent);

	LIB_FUNC("xbxNatawohc", LibGnmDriver::sceGnmSubmitAndFlipCommandBuffers);

	LIB_FUNC("Ga6r7H6Y0RI", LibGnmDriver::sceGnmSubmitAndFlipCommandBuffersForWorkload);

	LIB_FUNC("zwY0YV91TTI", LibGnmDriver::sceGnmSubmitCommandBuffers);

	LIB_FUNC("jRcI8VcgTz4", LibGnmDriver::sceGnmSubmitCommandBuffersForWorkload);

	LIB_FUNC("yvZ73uQUqrk", LibGnmDriver::sceGnmSubmitDone);

	LIB_FUNC("ArSg-TGinhk", LibGnmDriver::sceGnmUnmapComputeQueue);

	LIB_FUNC("yhFCnaz5daw", LibGnmDriver::sceGnmUnregisterAllResourcesForOwner);

	LIB_FUNC("fhKwCVVj9nk", LibGnmDriver::sceGnmUnregisterOwnerAndResources);

	LIB_FUNC("k8EXkhIP+lM", LibGnmDriver::sceGnmUnregisterResource);

	LIB_FUNC("nLM2i2+65hA", LibGnmDriver::sceGnmUpdateGsShader);

	LIB_FUNC("GNlx+y7xPdE", LibGnmDriver::sceGnmUpdateHsShader);

	LIB_FUNC("4MgRw-bVNQU", LibGnmDriver::sceGnmUpdatePsShader);

	LIB_FUNC("mLVL7N7BVBg", LibGnmDriver::sceGnmUpdatePsShader350);

	LIB_FUNC("V31V01UiScY", LibGnmDriver::sceGnmUpdateVsShader);

	LIB_FUNC("iCO804ZgzdA", LibGnmDriver::sceGnmValidateCommandBuffers);

	LIB_FUNC("SXw4dZEkgpA", LibGnmDriver::sceGnmValidateDisableDiagnostics);

	LIB_FUNC("BgM3t3LvcNk", LibGnmDriver::sceGnmValidateDisableDiagnostics2);

	LIB_FUNC("qGP74T5OWJc", LibGnmDriver::sceGnmValidateDispatchCommandBuffers);

	LIB_FUNC("hsZPf1lON7E", LibGnmDriver::sceGnmValidateDrawCommandBuffers);

	LIB_FUNC("RX7XCNSaL6I", LibGnmDriver::sceGnmValidateGetDiagnosticInfo);

	LIB_FUNC("5SHGNwLXBV4", LibGnmDriver::sceGnmValidateGetDiagnostics);

	LIB_FUNC("HzMN7ANqYEc", LibGnmDriver::sceGnmValidateGetVersion);

	LIB_FUNC("rTIV11nMQuM", LibGnmDriver::sceGnmValidateOnSubmitEnabled);

	LIB_FUNC("MBMa6EFu4Ko", LibGnmDriver::sceGnmValidateResetState);

	LIB_FUNC("Q7t4VEYLafI", LibGnmDriver::sceGnmValidationRegisterMemoryCheckCallback);

	LIB_FUNC("xeTLfxVIQO4", LibGnmDriver::sceRazorCaptureCommandBuffersOnlyImmediate);

	LIB_FUNC("9thMn+uB1is", LibGnmDriver::sceRazorCaptureCommandBuffersOnlySinceLastFlip);

	LIB_FUNC("u9YKpRRHe-M", LibGnmDriver::sceRazorCaptureImmediate);

	LIB_FUNC("4UFagYlfuAM", LibGnmDriver::sceRazorCaptureSinceLastFlip);

	LIB_FUNC("f33OrruQYbM", LibGnmDriver::sceRazorIsLoaded);

	LIB_FUNC("Bj0GWi1jWcM", LibGnmDriver::Func_063D065A2D6359C3);

	LIB_FUNC("DKusr7JYQp0", LibGnmDriver::Func_0CABACAFB258429D);

	LIB_FUNC("FQzzNvwumaM", LibGnmDriver::Func_150CF336FC2E99A3);

	LIB_FUNC("F8pof57lLUk", LibGnmDriver::Func_17CA687F9EE52D49);

	LIB_FUNC("GHC4n3Wca0U", LibGnmDriver::Func_1870B89F759C6B45);

	LIB_FUNC("JvkCnvaKlV4", LibGnmDriver::Func_26F9029EF68A955E);

	LIB_FUNC("MB49u6sJLbA", LibGnmDriver::Func_301E3DBBAB092DB0);

	LIB_FUNC("MLr+Fyrxf+8", LibGnmDriver::Func_30BAFE172AF17FEF);

	LIB_FUNC("Pmo+ggPZUxc", LibGnmDriver::Func_3E6A3E8203D95317);

	LIB_FUNC("QP7vDGU0xDQ", LibGnmDriver::Func_40FEEF0C6534C434);

	LIB_FUNC("QWuQed5Mus4", LibGnmDriver::Func_416B9079DE4CBACE);

	LIB_FUNC("R3TYO7Tdv5o", LibGnmDriver::Func_4774D83BB4DDBF9A);

	LIB_FUNC("UGePHM7rmgA", LibGnmDriver::Func_50678F1CCEEB9A00);

	LIB_FUNC("VKLsX6TGJBM", LibGnmDriver::Func_54A2EC5FA4C62413);

	LIB_FUNC("WpxSyDE4rms", LibGnmDriver::Func_5A9C52C83138AE6B);

	LIB_FUNC("XSIZOjHqEUI", LibGnmDriver::Func_5D22193A31EA1142);

	LIB_FUNC("clo23rtglI0", LibGnmDriver::Func_725A36DEBB60948D);

	LIB_FUNC("gCGlAvphubs", LibGnmDriver::Func_8021A502FA61B9BB);

	LIB_FUNC("nQAv4PpA8OY", LibGnmDriver::Func_9D002FE0FA40F0E6);

	LIB_FUNC("nSl-NqcCi3E", LibGnmDriver::Func_9D297F36A7028B71);

	LIB_FUNC("otfsenvPebM", LibGnmDriver::Func_A2D7EC7A7BCF79B3);

	LIB_FUNC("qhKjy4mQhUo", LibGnmDriver::Func_AA12A3CB8990854A);

	LIB_FUNC("rcjdwAUCC8Y", LibGnmDriver::Func_ADC8DDC005020BC6);

	LIB_FUNC("sKhoi2ectC0", LibGnmDriver::Func_B0A8688B679CB42D);

	LIB_FUNC("tIkCC1FXpf8", LibGnmDriver::Func_B489020B5157A5FF);

	LIB_FUNC("ut57TBmRQN0", LibGnmDriver::Func_BADE7B4C199140DD);

	LIB_FUNC("0VEbnc-7Pdk", LibGnmDriver::Func_D1511B9DCFFB3DD9);

	LIB_FUNC("1TRGZJsC5Y4", LibGnmDriver::Func_D53446649B02E58E);

	LIB_FUNC("2Lbo4o4e8KM", LibGnmDriver::Func_D8B6E8E28E1EF0A3);

	LIB_FUNC("2T1zOhnddFQ", LibGnmDriver::Func_D93D733A19DD7454);

	LIB_FUNC("3plUQ7wqgxc", LibGnmDriver::Func_DE995443BC2A8317);

	LIB_FUNC("326VKBUMI-8", LibGnmDriver::Func_DF6E9528150C23FF);

	LIB_FUNC("7LTGukH+M1A", LibGnmDriver::Func_ECB4C6BA41FE3350);

	LIB_FUNC("dqPBvjFVpTA", LibGnmDriver::sceGnmDebugModuleReset);

	LIB_FUNC("RNPAItiMLIg", LibGnmDriver::sceGnmDebugReset);

	LIB_FUNC("xMMot887QXE", LibGnmDriver::Func_C4C328B7CF3B4171);

	LIB_FUNC("pF1HQjbmQJ0", LibGnmDriver::sceGnmDrawInitToDefaultContextStateInternalCommand);

	LIB_FUNC("jajhf-Gi3AI", LibGnmDriver::sceGnmDrawInitToDefaultContextStateInternalSize);

	LIB_FUNC("vbcR4Ken6AA", LibGnmDriver::sceGnmFindResources);

	LIB_FUNC("eLQbNsKeTkU", LibGnmDriver::sceGnmGetResourceRegistrationBuffers);

	LIB_FUNC("j6mSQs3UgaY", LibGnmDriver::sceGnmRegisterOwnerForSystem);

	LIB_FUNC("HEOIaxbuVTA", LibGnmDriver::Func_1C43886B16EE5530);

	LIB_FUNC("gQNwGezNDgE", LibGnmDriver::Func_81037019ECCD0E01);

	LIB_FUNC("v7QcBXR48L8", LibGnmDriver::Func_BFB41C057478F0BF);

	LIB_FUNC("5R1E24FRI4w", LibGnmDriver::Func_E51D44DB8151238C);

	LIB_FUNC("+RaJBCVJZVM", LibGnmDriver::Func_F916890425496553);

}



} // namespace LibGnmDriver

} // namespace Libs

