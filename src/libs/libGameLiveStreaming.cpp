// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libSceGameLiveStreaming_direct_streaming HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Entry points are
// stubbed (return ORBIS_OK) - real logic for complex modules (GPU/NP) needs
// adaptation to KytyPlus's Core/Memory infrastructure and is tracked separately.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

namespace LibGameLiveStreaming {
LIB_VERSION("libSceGameLiveStreaming", 1, "libSceGameLiveStreaming", 1, 1);


static int KYTY_SYSV_ABI sceGameLiveStreamingStartDebugBroadcast() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingStopDebugBroadcast() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingApplySocialFeedbackMessageFilter() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingCheckCallback() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingClearPresetSocialFeedbackCommands() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingClearSocialFeedbackMessages() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingClearSpoilerTag() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingEnableLiveStreaming() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingEnableSocialFeedback() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingGetCurrentBroadcastScreenLayout() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingGetCurrentStatus() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingGetCurrentStatus2() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingGetProgramInfo() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingGetSocialFeedbackMessages() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingGetSocialFeedbackMessagesCount() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingLaunchLiveViewer() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingLaunchLiveViewerA() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingPermitLiveStreaming() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingPermitServerSideRecording() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingPostSocialMessage() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingRegisterCallback() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenCloseSeparateMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenConfigureSeparateMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenInitialize() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenInitializeSeparateModeParameter() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenOpenSeparateMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenSetMode() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingScreenTerminate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetCameraFrameSetting() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetDefaultServiceProviderPermission() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetGuardAreas() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetInvitationSessionId() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetLinkCommentPreset() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetMaxBitrate() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetMetadata() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetPresetSocialFeedbackCommands() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetServiceProviderPermission() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetSpoilerTag() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingSetStandbyScreenResource() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingStartGenerateStandbyScreenResource() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingStartSocialFeedbackMessageFiltering() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingStopGenerateStandbyScreenResource() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingStopSocialFeedbackMessageFiltering() {
	PRINT_NAME();
	return OK; // STUBBED
}

static int KYTY_SYSV_ABI sceGameLiveStreamingUnregisterCallback() {
	PRINT_NAME();
	return OK; // STUBBED
}


LIB_DEFINE(InitGameLiveStreaming_1) {
	LIB_FUNC("caqgDl+V9qA", LibGameLiveStreaming::sceGameLiveStreamingStartDebugBroadcast);
	LIB_FUNC("0i8Lrllxwow", LibGameLiveStreaming::sceGameLiveStreamingStopDebugBroadcast);
	LIB_FUNC("NqkTzemliC0", LibGameLiveStreaming::sceGameLiveStreamingApplySocialFeedbackMessageFilter);
	LIB_FUNC("PC4jq87+YQI", LibGameLiveStreaming::sceGameLiveStreamingCheckCallback);
	LIB_FUNC("FcHBfHjFXkA", LibGameLiveStreaming::sceGameLiveStreamingClearPresetSocialFeedbackCommands);
	LIB_FUNC("lZ2Sd0uEvpo", LibGameLiveStreaming::sceGameLiveStreamingClearSocialFeedbackMessages);
	LIB_FUNC("6c2zGtThFww", LibGameLiveStreaming::sceGameLiveStreamingClearSpoilerTag);
	LIB_FUNC("dWM80AX39o4", LibGameLiveStreaming::sceGameLiveStreamingEnableLiveStreaming);
	LIB_FUNC("wBOQWjbWMfU", LibGameLiveStreaming::sceGameLiveStreamingEnableSocialFeedback);
	LIB_FUNC("aRSQNqbats4", LibGameLiveStreaming::sceGameLiveStreamingGetCurrentBroadcastScreenLayout);
	LIB_FUNC("CoPMx369EqM", LibGameLiveStreaming::sceGameLiveStreamingGetCurrentStatus);
	LIB_FUNC("lK8dLBNp9OE", LibGameLiveStreaming::sceGameLiveStreamingGetCurrentStatus2);
	LIB_FUNC("OIIm19xu+NM", LibGameLiveStreaming::sceGameLiveStreamingGetProgramInfo);
	LIB_FUNC("PMx7N4WqNdo", LibGameLiveStreaming::sceGameLiveStreamingGetSocialFeedbackMessages);
	LIB_FUNC("yeQKjHETi40", LibGameLiveStreaming::sceGameLiveStreamingGetSocialFeedbackMessagesCount);
	LIB_FUNC("ysWfX5PPbfc", LibGameLiveStreaming::sceGameLiveStreamingLaunchLiveViewer);
	LIB_FUNC("cvRCb7DTAig", LibGameLiveStreaming::sceGameLiveStreamingLaunchLiveViewerA);
	LIB_FUNC("K0QxEbD7q+c", LibGameLiveStreaming::sceGameLiveStreamingPermitLiveStreaming);
	LIB_FUNC("-EHnU68gExU", LibGameLiveStreaming::sceGameLiveStreamingPermitServerSideRecording);
	LIB_FUNC("hggKhPySVgI", LibGameLiveStreaming::sceGameLiveStreamingPostSocialMessage);
	LIB_FUNC("nFP8qT9YXbo", LibGameLiveStreaming::sceGameLiveStreamingRegisterCallback);
	LIB_FUNC("b5RaMD2J0So", LibGameLiveStreaming::sceGameLiveStreamingScreenCloseSeparateMode);
	LIB_FUNC("hBdd8n6kuvE", LibGameLiveStreaming::sceGameLiveStreamingScreenConfigureSeparateMode);
	LIB_FUNC("uhCmn81s-mU", LibGameLiveStreaming::sceGameLiveStreamingScreenInitialize);
	LIB_FUNC("fo5B8RUaBxQ", LibGameLiveStreaming::sceGameLiveStreamingScreenInitializeSeparateModeParameter);
	LIB_FUNC("iorzW0pKOiA", LibGameLiveStreaming::sceGameLiveStreamingScreenOpenSeparateMode);
	LIB_FUNC("gDSvt78H3Oo", LibGameLiveStreaming::sceGameLiveStreamingScreenSetMode);
	LIB_FUNC("HE93dr-5rx4", LibGameLiveStreaming::sceGameLiveStreamingScreenTerminate);
	LIB_FUNC("3PSiwAzFISE", LibGameLiveStreaming::sceGameLiveStreamingSetCameraFrameSetting);
	LIB_FUNC("TwuUzTKKeek", LibGameLiveStreaming::sceGameLiveStreamingSetDefaultServiceProviderPermission);
	LIB_FUNC("Gw6S4oqlY7E", LibGameLiveStreaming::sceGameLiveStreamingSetGuardAreas);
	LIB_FUNC("QmQYwQ7OTJI", LibGameLiveStreaming::sceGameLiveStreamingSetInvitationSessionId);
	LIB_FUNC("Sb5bAXyUt5c", LibGameLiveStreaming::sceGameLiveStreamingSetLinkCommentPreset);
	LIB_FUNC("q-kxuaF7URU", LibGameLiveStreaming::sceGameLiveStreamingSetMaxBitrate);
	LIB_FUNC("hUY-mSOyGL0", LibGameLiveStreaming::sceGameLiveStreamingSetMetadata);
	LIB_FUNC("ycodiP2I0xo", LibGameLiveStreaming::sceGameLiveStreamingSetPresetSocialFeedbackCommands);
	LIB_FUNC("x6deXUpQbBo", LibGameLiveStreaming::sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription);
	LIB_FUNC("mCoz3k3zPmA", LibGameLiveStreaming::sceGameLiveStreamingSetServiceProviderPermission);
	LIB_FUNC("ZuX+zzz2DkA", LibGameLiveStreaming::sceGameLiveStreamingSetSpoilerTag);
	LIB_FUNC("MLvYI86FFAo", LibGameLiveStreaming::sceGameLiveStreamingSetStandbyScreenResource);
	LIB_FUNC("y0KkAydy9xE", LibGameLiveStreaming::sceGameLiveStreamingStartGenerateStandbyScreenResource);
	LIB_FUNC("Y1WxX7dPMCw", LibGameLiveStreaming::sceGameLiveStreamingStartSocialFeedbackMessageFiltering);
	LIB_FUNC("D7dg5QJ4FlE", LibGameLiveStreaming::sceGameLiveStreamingStopGenerateStandbyScreenResource);
	LIB_FUNC("bYuGUBuIsaY", LibGameLiveStreaming::sceGameLiveStreamingStopSocialFeedbackMessageFiltering);
	LIB_FUNC("5XHaH3kL+bA", LibGameLiveStreaming::sceGameLiveStreamingUnregisterCallback);
}
} // namespace LibGameLiveStreaming

} // namespace Libs
