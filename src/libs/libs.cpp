#include "libs/libs.h"

#include "common/logging/log.h"
#include "libs/errno.h"
#include "loader/symbolDatabase.h"

namespace Libs {

namespace LibcInternal {
LIB_DEFINE(InitLibcInternal_1);
} // namespace LibcInternal

namespace LibContentDelete {
LIB_DEFINE(InitContentDelete_1);
} // namespace LibContentDelete

namespace LibContentExport {
LIB_DEFINE(InitContentExport_1);
} // namespace LibContentExport

namespace LibContentSearch {
LIB_DEFINE(InitContentSearch_1);
} // namespace LibContentSearch

namespace VideoDec2 {
LIB_DEFINE(InitVideoDec2_1);
} // namespace VideoDec2

namespace LibMouse {
LIB_DEFINE(InitMouse_1);
} // namespace LibMouse

namespace LibKeyboard {
LIB_DEFINE(InitKeyboard_1);
} // namespace LibKeyboard

namespace LibUlt {
LIB_DEFINE(InitUlt_1);
} // namespace LibUlt

namespace LibPsml {
LIB_DEFINE(InitPsml_1);
} // namespace LibPsml

namespace LibRandom {
LIB_DEFINE(InitRandom_1);
} // namespace LibRandom

namespace LibScreenShot {
LIB_DEFINE(InitScreenShot_1);
} // namespace LibScreenShot

LIB_DEFINE(InitLibC_1);
LIB_DEFINE(InitAppContent_1);
LIB_DEFINE(InitAmpr_1);
LIB_DEFINE(InitAudio_1);
LIB_DEFINE(InitCoredump_1);
LIB_DEFINE(InitDbgAddressSanitizer_1);
LIB_DEFINE(InitDebug_1);
LIB_DEFINE(InitDialog_1);
LIB_DEFINE(InitFiber_1);
LIB_DEFINE(InitFont_1);
LIB_DEFINE(InitFontFt_1);
LIB_DEFINE(InitGraphicsDriver_1);
LIB_DEFINE(InitLibKernel_1);
LIB_DEFINE(InitNet_1);
LIB_DEFINE(InitPad_1);
LIB_DEFINE(InitPlayGo_1);
LIB_DEFINE(InitPngDec_1);
LIB_DEFINE(InitPlatform_1);
LIB_DEFINE(InitRudp_1);
LIB_DEFINE(InitRtc_1);
LIB_DEFINE(InitSaveData_1);
LIB_DEFINE(InitShare_1);
LIB_DEFINE(InitSysmodule_1);
LIB_DEFINE(InitSystemService_1);
LIB_DEFINE(InitUserService_1);
LIB_DEFINE(InitVideoOut_1);


// --- PS4 modules ported from shadPS4 (GPL-2.0-or-later), full NID coverage ---
// Each Init function is defined inside its own inner namespace in the matching
// lib*.cpp file (e.g. namespace LibCamera { LIB_DEFINE(InitCamera_1); ... }),
// so the declaration here must be wrapped in the same inner namespace, matching
// the pre-existing LibMouse/LibUlt/LibRandom/VideoDec2 pattern above.
namespace LibCamera { LIB_DEFINE(InitCamera_1); }
namespace LibDiscMap { LIB_DEFINE(InitDiscMap_1); }
namespace LibGameLiveStreaming { LIB_DEFINE(InitGameLiveStreaming_1); }
namespace LibGnmDriver { LIB_DEFINE(InitGnmDriver_1); }
namespace LibHmd { LIB_DEFINE(InitHmd_1); }
namespace LibIme { LIB_DEFINE(InitIme_1); }
namespace LibInvitationDialog { LIB_DEFINE(InitInvitationDialog_1); }
namespace LibJpeg { LIB_DEFINE(InitJpeg_1); }
namespace LibMove { LIB_DEFINE(InitMove_1); }
namespace LibNgs2 { LIB_DEFINE(InitNgs2_1); }
namespace LibRazorCpu { LIB_DEFINE(InitRazorCpu_1); }
namespace LibRemotePlay { LIB_DEFINE(InitRemotePlay_1); }
namespace LibSharePlay { LIB_DEFINE(InitSharePlay_1); }
namespace LibSigninDialog { LIB_DEFINE(InitSigninDialog_1); }
namespace LibSystemGesture { LIB_DEFINE(InitSystemGesture_1); }
namespace LibUlobjmgr { LIB_DEFINE(InitUlobjmgr_1); }
namespace LibUsbd { LIB_DEFINE(InitUsbd_1); }
namespace LibVideoRecording { LIB_DEFINE(InitVideoRecording_1); }
namespace LibVoice { LIB_DEFINE(InitVoice_1); }
namespace LibVrTracker { LIB_DEFINE(InitVrTracker_1); }
namespace LibWebBrowserDialog { LIB_DEFINE(InitWebBrowserDialog_1); }
namespace LibZlib { LIB_DEFINE(InitZlib_1); }
namespace RageEngine { LIB_DEFINE(InitRageEngine_1); }
namespace CompanionHttpd { LIB_DEFINE(InitCompanionHttpd_1); }
namespace CompanionUtil { LIB_DEFINE(InitCompanionUtil_1); }
namespace NpAuth { LIB_DEFINE(InitNpAuth_1); }
namespace NpCommon { LIB_DEFINE(InitNpCommon_1); }
namespace NpManager { LIB_DEFINE(InitNpManager_1); }
namespace NpPartner { LIB_DEFINE(InitNpPartner_1); }
namespace NpParty { LIB_DEFINE(InitNpParty_1); }
namespace NpSnsFacebookDialog { LIB_DEFINE(InitNpSnsFacebookDialog_1); }
namespace NpTrophy { LIB_DEFINE(InitNpTrophy_1); }
namespace NpTus { LIB_DEFINE(InitNpTus_1); }

void InitAll(Loader::SymbolDatabase* s) {
	LIB_LOAD(InitAudio_1);
	LIB_LOAD(InitAmpr_1);
	LIB_LOAD(InitAppContent_1);
	LIB_LOAD(InitCoredump_1);
	LIB_LOAD(LibContentDelete::InitContentDelete_1);
	LIB_LOAD(LibContentExport::InitContentExport_1);
	LIB_LOAD(LibContentSearch::InitContentSearch_1);
	LIB_LOAD(InitLibC_1);
	LIB_LOAD(InitDbgAddressSanitizer_1);
	LIB_LOAD(InitDebug_1);
	LIB_LOAD(InitDialog_1);
	LIB_LOAD(InitFiber_1);
	LIB_LOAD(InitFont_1);
	LIB_LOAD(InitFontFt_1);
	LIB_LOAD(InitGraphicsDriver_1);
	LIB_LOAD(InitLibKernel_1);
	LIB_LOAD(LibMouse::InitMouse_1);
	LIB_LOAD(LibKeyboard::InitKeyboard_1);
	LIB_LOAD(InitNet_1);
	LIB_LOAD(InitPad_1);
	LIB_LOAD(InitPlayGo_1);
	LIB_LOAD(LibRandom::InitRandom_1);
	LIB_LOAD(LibPsml::InitPsml_1);
	LIB_LOAD(InitPngDec_1);
	LIB_LOAD(InitPlatform_1);
	LIB_LOAD(InitRudp_1);
	LIB_LOAD(InitRtc_1);
	LIB_LOAD(InitSaveData_1);
	LIB_LOAD(LibScreenShot::InitScreenShot_1);
	LIB_LOAD(InitShare_1);
	LIB_LOAD(InitSysmodule_1);
	LIB_LOAD(InitSystemService_1);
	LIB_LOAD(LibUlt::InitUlt_1);
	LIB_LOAD(InitUserService_1);
	LIB_LOAD(VideoDec2::InitVideoDec2_1);
	LIB_LOAD(InitVideoOut_1);
// --- RAGE engine HLE bridge ---
	LIB_LOAD(RageEngine::InitRageEngine_1);
// --- PS4 module loads (shadPS4 port) ---
	LIB_LOAD(LibCamera::InitCamera_1);
	LIB_LOAD(LibDiscMap::InitDiscMap_1);
	LIB_LOAD(LibGameLiveStreaming::InitGameLiveStreaming_1);
	LIB_LOAD(LibGnmDriver::InitGnmDriver_1);
	LIB_LOAD(LibHmd::InitHmd_1);
	LIB_LOAD(LibIme::InitIme_1);
	LIB_LOAD(LibInvitationDialog::InitInvitationDialog_1);
	LIB_LOAD(LibJpeg::InitJpeg_1);
	LIB_LOAD(LibMove::InitMove_1);
	LIB_LOAD(LibNgs2::InitNgs2_1);
	LIB_LOAD(LibRazorCpu::InitRazorCpu_1);
	LIB_LOAD(LibRemotePlay::InitRemotePlay_1);
	LIB_LOAD(LibSharePlay::InitSharePlay_1);
	LIB_LOAD(LibSigninDialog::InitSigninDialog_1);
	LIB_LOAD(LibSystemGesture::InitSystemGesture_1);
	LIB_LOAD(LibUlobjmgr::InitUlobjmgr_1);
	LIB_LOAD(LibUsbd::InitUsbd_1);
	LIB_LOAD(LibVideoRecording::InitVideoRecording_1);
	LIB_LOAD(LibVoice::InitVoice_1);
	LIB_LOAD(LibVrTracker::InitVrTracker_1);
	LIB_LOAD(LibWebBrowserDialog::InitWebBrowserDialog_1);
	LIB_LOAD(LibZlib::InitZlib_1);
	LIB_LOAD(CompanionHttpd::InitCompanionHttpd_1);
	LIB_LOAD(CompanionUtil::InitCompanionUtil_1);
	LIB_LOAD(NpAuth::InitNpAuth_1);
	LIB_LOAD(NpCommon::InitNpCommon_1);
	LIB_LOAD(NpManager::InitNpManager_1);
	LIB_LOAD(NpPartner::InitNpPartner_1);
	LIB_LOAD(NpParty::InitNpParty_1);
	LIB_LOAD(NpSnsFacebookDialog::InitNpSnsFacebookDialog_1);
	LIB_LOAD(NpTrophy::InitNpTrophy_1);
	LIB_LOAD(NpTus::InitNpTus_1);
}

namespace LibContentExport {

LIB_VERSION("ContentExport", 1, "ContentExport", 1, 1);

namespace ContentExport {

constexpr int CONTENT_EXPORT_ERROR_INVALID_PARAM = -2137182186; /* 0x809D3016 */

struct ContentExportInitParam2 {
	void*   malloc_func;
	void*   free_func;
	void*   user_data;
	size_t  buffer_size;
	int64_t reserved0;
	int64_t reserved1;
};

static bool g_initialized = false;

static int KYTY_SYSV_ABI ContentExportInit2(const ContentExportInitParam2* init_param) {
	PRINT_NAME();

	LOGF("\t init_param  = 0x%016" PRIx64 "\n", reinterpret_cast<uint64_t>(init_param));

	if (init_param == nullptr) {
		return CONTENT_EXPORT_ERROR_INVALID_PARAM;
	}

	LOGF("\t malloc_func = 0x%016" PRIx64 "\n"
	     "\t free_func   = 0x%016" PRIx64 "\n"
	     "\t user_data   = 0x%016" PRIx64 "\n"
	     "\t buffer_size = %" PRIu64 "\n",
	     reinterpret_cast<uint64_t>(init_param->malloc_func),
	     reinterpret_cast<uint64_t>(init_param->free_func),
	     reinterpret_cast<uint64_t>(init_param->user_data),
	     static_cast<uint64_t>(init_param->buffer_size));

	g_initialized = true;

	return OK;
}

} // namespace ContentExport

LIB_DEFINE(InitContentExport_1) {
	LIB_FUNC("0GnN4QCgIfs", ContentExport::ContentExportInit2);
}

} // namespace LibContentExport

namespace LibContentSearch {

LIB_VERSION("ContentSearch", 1, "ContentSearch", 1, 0);

namespace ContentSearch {

constexpr int CONTENT_SEARCH_ERROR_INVALID_PARAM = -2137190397; /* 0x809D1003 */

struct ContentSearchInitParam {
	size_t memory_size;
};

static bool g_initialized = false;

static int KYTY_SYSV_ABI ContentSearchInit(const ContentSearchInitParam* init_param) {
	PRINT_NAME();

	LOGF("\t init_param  = 0x%016" PRIx64 "\n", reinterpret_cast<uint64_t>(init_param));

	if (init_param == nullptr) {
		return CONTENT_SEARCH_ERROR_INVALID_PARAM;
	}

	LOGF("\t memory_size = %" PRIu64 "\n", static_cast<uint64_t>(init_param->memory_size));

	g_initialized = true;

	return OK;
}

} // namespace ContentSearch

LIB_DEFINE(InitContentSearch_1) {
	LIB_FUNC("dPj4ZtRcIWk", ContentSearch::ContentSearchInit);
}

} // namespace LibContentSearch

namespace LibContentDelete {

LIB_VERSION("ContentDelete", 1, "ContentDelete", 1, 1);

namespace ContentDelete {

constexpr int CONTENT_DELETE_ERROR_INVALID_PARAM = -2137174015; /* 0x809D5001 */

struct ContentDeleteInitParam {
	char   reserved1[4];
	size_t heap_size;
	char   reserved2[32];
};

static bool g_initialized = false;

static int KYTY_SYSV_ABI ContentDeleteInitialize(const ContentDeleteInitParam* init_param) {
	PRINT_NAME();

	LOGF("\t init_param = 0x%016" PRIx64 "\n", reinterpret_cast<uint64_t>(init_param));

	if (init_param == nullptr) {
		return CONTENT_DELETE_ERROR_INVALID_PARAM;
	}

	LOGF("\t heap_size  = %" PRIu64 "\n", static_cast<uint64_t>(init_param->heap_size));

	g_initialized = true;

	return OK;
}

} // namespace ContentDelete

LIB_DEFINE(InitContentDelete_1) {
	LIB_FUNC("zoxb0wEChEM", ContentDelete::ContentDeleteInitialize);
}

} // namespace LibContentDelete

} // namespace Libs
