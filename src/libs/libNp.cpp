// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 libNp HLE, ported into KytyPlus's LIB_FUNC framework.
// NID map verified against shadPS4 (GPL-2.0-or-later). Exports span multiple
// PS4 module names; each gets its own sub-namespace + LIB_DEFINE, mirroring
// libKernel.cpp's structure. Entry points are stubbed (return ORBIS_OK).

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

// PSN is not available in the emulator. Network-requiring stubs return this
// error so games do not proceed as if online services are connected.
constexpr int NP_NOT_SIGNED_IN = static_cast<int>(0x80550005u);

namespace NpAuth {
LIB_VERSION("libSceNpAuth", 1, "libSceNpAuth", 1, 1);


static int KYTY_SYSV_ABI sceNpAuthGetAuthorizationCode() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpAuthGetAuthorizationCodeA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpAuthGetIdToken() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpAuthGetIdTokenA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpAuthSetTimeout() {
	PRINT_NAME();
	return OK;
}


LIB_DEFINE(InitNpAuth_1) {
	LIB_FUNC("KxGkOrQJTqY", NpAuth::sceNpAuthGetAuthorizationCode);
	LIB_FUNC("qAUXQ9GdWp8", NpAuth::sceNpAuthGetAuthorizationCodeA);
	LIB_FUNC("uaB-LoJqHis", NpAuth::sceNpAuthGetIdToken);
	LIB_FUNC("CocbHVIKPE8", NpAuth::sceNpAuthGetIdTokenA);
	LIB_FUNC("PM3IZCw-7m0", NpAuth::sceNpAuthSetTimeout);
}

} // namespace NpAuth
namespace NpCommon {

LIB_VERSION("libSceNpCommon", 1, "libSceNpCommon", 1, 1);

static int KYTY_SYSV_ABI sceNpCmpNpId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCmpNpIdInOrder() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCmpOnlineId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCondDestroy() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCalloutInitCtx() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCalloutTermCtx() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpLwMutexLock() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpLwMutexInit() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpLwMutexDestroy() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpLwMutexUnlock() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpMutexTryLock() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpJoinThread() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCalloutStartOnCtx() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCreateThread() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpIntIsValidOnlineId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpLwMutexTryLock() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCalloutStopOnCtx() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpMutexDestroy() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCalloutStartOnCtx64() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpMutexUnlock() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetSystemClockUsec() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCondInit() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpMutexLock() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetPlatformType() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCondTimedwait() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpMutexInit() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCondSignal() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetSdkVersion() {
	PRINT_NAME();
	return OK;
}


LIB_DEFINE(InitNpCommon_1) {
	LIB_FUNC("i8UmXTSq7N4", NpCommon::sceNpCmpNpId);
	LIB_FUNC("TcwEFnakiSc", NpCommon::sceNpCmpNpIdInOrder);
	LIB_FUNC("dj+O5aD2a0Q", NpCommon::sceNpCmpOnlineId);
	LIB_FUNC("1a+iY5YUJcI", NpCommon::sceNpCondDestroy);
	LIB_FUNC("9+m5nRdJ-wQ", NpCommon::sceNpCalloutInitCtx);
	LIB_FUNC("AqJ4xkWsV+I", NpCommon::sceNpCalloutTermCtx);
	LIB_FUNC("18j+qk6dRwk", NpCommon::sceNpLwMutexLock);
	LIB_FUNC("1CiXI-MyEKs", NpCommon::sceNpLwMutexInit);
	LIB_FUNC("4zxevggtYrQ", NpCommon::sceNpLwMutexDestroy);
	LIB_FUNC("CQG2oyx1-nM", NpCommon::sceNpLwMutexUnlock);
	LIB_FUNC("DuslmoqQ+nk", NpCommon::sceNpMutexTryLock);
	LIB_FUNC("EjMsfO3GCIA", NpCommon::sceNpJoinThread);
	LIB_FUNC("fClnlkZmA6k", NpCommon::sceNpCalloutStartOnCtx);
	LIB_FUNC("fhJ5uKzcn0w", NpCommon::sceNpCreateThread);
	LIB_FUNC("hkeX9iuCwlI", NpCommon::sceNpIntIsValidOnlineId);
	LIB_FUNC("hp0kVgu5Fxw", NpCommon::sceNpLwMutexTryLock);
	LIB_FUNC("in19gH7G040", NpCommon::sceNpCalloutStopOnCtx);
	LIB_FUNC("lQ11BpMM4LU", NpCommon::sceNpMutexDestroy);
	LIB_FUNC("lpr66Gby8dQ", NpCommon::sceNpCalloutStartOnCtx64);
	LIB_FUNC("oZyb9ktuCpA", NpCommon::sceNpMutexUnlock);
	LIB_FUNC("PVVsRmMkO1g", NpCommon::sceNpGetSystemClockUsec);
	LIB_FUNC("q2tsVO3lM4A", NpCommon::sceNpCondInit);
	LIB_FUNC("r9Bet+s6fKc", NpCommon::sceNpMutexLock);
	LIB_FUNC("sXVQUIGmk2U", NpCommon::sceNpGetPlatformType);
	LIB_FUNC("ss2xO9IJxKQ", NpCommon::sceNpCondTimedwait);
	LIB_FUNC("uEwag-0YZPc", NpCommon::sceNpMutexInit);
	LIB_FUNC("uMJFOA62mVU", NpCommon::sceNpCondSignal);
	LIB_FUNC("Pglk7zFj0DI", NpCommon::sceNpGetSdkVersion);
}

} // namespace NpCommon
namespace NpManager {

LIB_VERSION("libSceNpManager", 1, "libSceNpManager", 1, 1);

static int KYTY_SYSV_ABI sceNpCheckNpAvailabilityA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCheckPlus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetAccountLanguage() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetAccountLanguageA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetParentalControlInfo() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetParentalControlInfoA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpSetTimeout() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpWaitAsync() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetAccountCountry() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetAccountDateOfBirth() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetAccountDateOfBirthA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetGamePresenceStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetGamePresenceStatusA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpSetGamePresenceOnline() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpSetGamePresenceOnlineA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpRegisterGamePresenceCallbackA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpUnregisterGamePresenceCallbackA() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpIsPlusMember() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpUnregisterPlusEventCallback() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetAccountId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetUserIdByAccountId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpGetUserIdByOnlineId() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpCheckCallbackForLib() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpUnregisterStateCallback() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpUnregisterNpReachabilityStateCallback() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpRegisterStateCallbackForToolkit() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpUnregisterStateCallbackForToolkit() {
	PRINT_NAME();
	return OK;
}


LIB_DEFINE(InitNpManager_1) {
	LIB_FUNC("8Z2Jc5GvGDI", NpManager::sceNpCheckNpAvailabilityA);
	LIB_FUNC("r6MyYJkryz8", NpManager::sceNpCheckPlus);
	LIB_FUNC("KZ1Mj9yEGYc", NpManager::sceNpGetAccountLanguage);
	LIB_FUNC("TPMbgIxvog0", NpManager::sceNpGetAccountLanguageA);
	LIB_FUNC("ilwLM4zOmu4", NpManager::sceNpGetParentalControlInfo);
	LIB_FUNC("m9L3O6yst-U", NpManager::sceNpGetParentalControlInfoA);
	LIB_FUNC("-QglDeRr8D8", NpManager::sceNpSetTimeout);
	LIB_FUNC("jyi5p9XWUSs", NpManager::sceNpWaitAsync);
	LIB_FUNC("Ghz9iWDUtC4", NpManager::sceNpGetAccountCountry);
	LIB_FUNC("8VBTeRf1ZwI", NpManager::sceNpGetAccountDateOfBirth);
	LIB_FUNC("q3M7XzBKC3s", NpManager::sceNpGetAccountDateOfBirthA);
	LIB_FUNC("IPb1hd1wAGc", NpManager::sceNpGetGamePresenceStatus);
	LIB_FUNC("oPO9U42YpgI", NpManager::sceNpGetGamePresenceStatusA);
	LIB_FUNC("KO+11cgC7N0", NpManager::sceNpSetGamePresenceOnline);
	LIB_FUNC("C0gNCiRIi4U", NpManager::sceNpSetGamePresenceOnlineA);
	LIB_FUNC("KswxLxk4c1Y", NpManager::sceNpRegisterGamePresenceCallbackA);
	LIB_FUNC("aJZyCcHxzu4", NpManager::sceNpUnregisterGamePresenceCallbackA);
	LIB_FUNC("Ybu6AxV6S0o", NpManager::sceNpIsPlusMember);
	LIB_FUNC("xViqJdDgKl0", NpManager::sceNpUnregisterPlusEventCallback);
	LIB_FUNC("a8R9-75u4iM", NpManager::sceNpGetAccountId);
	LIB_FUNC("VgYczPGB5ss", NpManager::sceNpGetUserIdByAccountId);
	LIB_FUNC("F6E4ycq9Dbg", NpManager::sceNpGetUserIdByOnlineId);
	LIB_FUNC("JELHf4xPufo", NpManager::sceNpCheckCallbackForLib);
	LIB_FUNC("mjjTXh+NHWY", NpManager::sceNpUnregisterStateCallback);
	LIB_FUNC("cRILAEvn+9M", NpManager::sceNpUnregisterNpReachabilityStateCallback);
	LIB_FUNC("0c7HbXRKUt4", NpManager::sceNpRegisterStateCallbackForToolkit);
	LIB_FUNC("YIvqqvJyjEc", NpManager::sceNpUnregisterStateCallbackForToolkit);
}

} // namespace NpManager
namespace NpPartner {

LIB_VERSION("libSceNpPartner001", 1, "libSceNpPartner001", 1, 1);

static int KYTY_SYSV_ABI sceNpEAAccessTerminate() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpHasEAAccessSubscriptionAbortRequest() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpEAAccessInitialize() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpHasEAAccessSubscription() {
	PRINT_NAME();
	return OK;
}


LIB_DEFINE(InitNpPartner_1) {
	LIB_FUNC("pMxXhNozUX8", NpPartner::sceNpEAAccessTerminate);
	LIB_FUNC("pQfYTZHznMc", NpPartner::sceNpHasEAAccessSubscriptionAbortRequest);
	LIB_FUNC("7CxI50-xlCk", NpPartner::sceNpEAAccessInitialize);
	LIB_FUNC("+OnbUs1CV0M", NpPartner::sceNpHasEAAccessSubscription);
}

} // namespace NpPartner
namespace NpParty {

LIB_VERSION("libSceNpParty", 1, "libSceNpParty", 1, 1);

static int KYTY_SYSV_ABI sceNpPartyCheckCallback() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyCreate() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyCreateA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetId() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetMemberInfo() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetMemberInfoA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetMembers() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetMembersA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetMemberSessionInfo() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetMemberVoiceInfo() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetState() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetStateAsUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetStateAsUserA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyGetVoiceChatPriority() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyInitialize() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyJoin() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyLeave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyRegisterHandler() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyRegisterHandlerA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyRegisterPrivateHandler() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartySendBinaryMessage() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartySetVoiceChatPriority() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyShowInvitationList() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyShowInvitationListA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyTerminate() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpPartyUnregisterPrivateHandler() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}


LIB_DEFINE(InitNpParty_1) {
	LIB_FUNC("3e4k2mzLkmc", NpParty::sceNpPartyCheckCallback);
	LIB_FUNC("nOZRy-slBoA", NpParty::sceNpPartyCreate);
	LIB_FUNC("XQSUbbnpPBA", NpParty::sceNpPartyCreateA);
	LIB_FUNC("DRA3ay-1DFQ", NpParty::sceNpPartyGetId);
	LIB_FUNC("F1P+-wpxQow", NpParty::sceNpPartyGetMemberInfo);
	LIB_FUNC("v2RYVGrJDkM", NpParty::sceNpPartyGetMemberInfoA);
	LIB_FUNC("T2UOKf00ZN0", NpParty::sceNpPartyGetMembers);
	LIB_FUNC("TaNw7W25QJw", NpParty::sceNpPartyGetMembersA);
	LIB_FUNC("4gOMfNYzllw", NpParty::sceNpPartyGetMemberSessionInfo);
	LIB_FUNC("EKi1jx59SP4", NpParty::sceNpPartyGetMemberVoiceInfo);
	LIB_FUNC("aEzKdJzATZ0", NpParty::sceNpPartyGetState);
	LIB_FUNC("o7grRhiGHYI", NpParty::sceNpPartyGetStateAsUser);
	LIB_FUNC("EjyAI+QNgFw", NpParty::sceNpPartyGetStateAsUserA);
	LIB_FUNC("-lc6XZnQXvM", NpParty::sceNpPartyGetVoiceChatPriority);
	LIB_FUNC("lhYCTQmBkds", NpParty::sceNpPartyInitialize);
	LIB_FUNC("RXNCDw2GDEg", NpParty::sceNpPartyJoin);
	LIB_FUNC("J8jAi-tfJHc", NpParty::sceNpPartyLeave);
	LIB_FUNC("kA88gbv71ao", NpParty::sceNpPartyRegisterHandler);
	LIB_FUNC("+v4fVHMwFWc", NpParty::sceNpPartyRegisterHandlerA);
	LIB_FUNC("zo4G5WWYpKg", NpParty::sceNpPartyRegisterPrivateHandler);
	LIB_FUNC("U6VdUe-PNAY", NpParty::sceNpPartySendBinaryMessage);
	LIB_FUNC("nazKyHygHhY", NpParty::sceNpPartySetVoiceChatPriority);
	LIB_FUNC("-MFiL7hEnPE", NpParty::sceNpPartyShowInvitationList);
	LIB_FUNC("yARHEYLajs0", NpParty::sceNpPartyShowInvitationListA);
	LIB_FUNC("oLYkibiHqRA", NpParty::sceNpPartyTerminate);
	LIB_FUNC("zQ7gIvt11Pc", NpParty::sceNpPartyUnregisterPrivateHandler);
}

} // namespace NpParty
namespace NpSnsFacebookDialog {

LIB_VERSION("libSceNpSnsFacebookDialog", 1, "libSceNpSnsFacebookDialog", 1, 1);

static int KYTY_SYSV_ABI sceNpSnsFacebookDialogUpdateStatus() {
	PRINT_NAME();
	return OK;
}


LIB_DEFINE(InitNpSnsFacebookDialog_1) {
	LIB_FUNC("fjV7C8H0Y8k", NpSnsFacebookDialog::sceNpSnsFacebookDialogUpdateStatus);
}

} // namespace NpSnsFacebookDialog
namespace NpTrophy {

LIB_VERSION("libSceNpTrophy", 1, "libSceNpTrophy", 1, 1);

static int KYTY_SYSV_ABI sceNpTrophyAbortHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyCaptureScreenshot() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophyDetails() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophyFlagArray() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophyGroupArray() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophyGroupDetails() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophySetInfo() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophySetInfoInGroup() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophySetVersion() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigGetTrophyTitleDetails() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyConfigHasGroupFeature() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyCreateContext() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyCreateHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyDestroyContext() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyDestroyHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetGameIcon() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetGameInfo() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetGroupIcon() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetGroupInfo() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetTrophyIcon() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetTrophyInfo() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGetTrophyUnlockState() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyGroupArrayGetNum() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntAbortHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntCheckNetSyncTitles() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntCreateHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntDestroyHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntGetLocalTrophySummary() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntGetProgress() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntGetRunningTitle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntGetRunningTitles() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntGetTrpIconByUri() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntNetSyncTitle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyIntNetSyncTitles() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyNumInfoGetTotal() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyRegisterContext() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTrophySetInfoGetTrophyFlagArray() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySetInfoGetTrophyNum() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyShowTrophyList() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemAbortHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemBuildGroupIconUri() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemBuildNetTrophyIconUri() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemBuildTitleIconUri() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemBuildTrophyIconUri() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemCheckNetSyncTitles() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemCheckRecoveryRequired() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemCloseStorage() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemCreateContext() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemCreateHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemDbgCtl() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemDebugLockTrophy() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemDebugUnlockTrophy() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemDestroyContext() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemDestroyHandle() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemDestroyTrophyConfig() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetDbgParam() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetDbgParamInt() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetGroupIcon() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetLocalTrophySummary() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetNextTitleFileEntryStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetProgress() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTitleFileStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTitleIcon() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTitleSyncStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTrophyConfig() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTrophyData() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTrophyGroupData() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTrophyIcon() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTrophyTitleData() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetTrophyTitleIds() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetUserFileInfo() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemGetUserFileStatus() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemIsServerAvailable() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemNetSyncTitle() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTrophySystemNetSyncTitles() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTrophySystemOpenStorage() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemPerformRecovery() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemRemoveAll() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemRemoveTitleData() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemRemoveUserData() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemSetDbgParam() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophySystemSetDbgParamInt() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI sceNpTrophyUnlockTrophy() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI Func_149656DA81D41C59() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI Func_9F80071876FFA5F6() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI Func_F8EF6F5350A91990() {
	PRINT_NAME();
	return OK;
}

static int KYTY_SYSV_ABI Func_FA7A2DD770447552() {
	PRINT_NAME();
	return OK;
}


LIB_DEFINE(InitNpTrophy_1) {
	LIB_FUNC("aTnHs7W-9Uk", NpTrophy::sceNpTrophyAbortHandle);
	LIB_FUNC("cqGkYAN-gRw", NpTrophy::sceNpTrophyCaptureScreenshot);
	LIB_FUNC("lhE4XS9OJXs", NpTrophy::sceNpTrophyConfigGetTrophyDetails);
	LIB_FUNC("qJ3IvrOoXg0", NpTrophy::sceNpTrophyConfigGetTrophyFlagArray);
	LIB_FUNC("zDjF2G+6tI0", NpTrophy::sceNpTrophyConfigGetTrophyGroupArray);
	LIB_FUNC("7Kh86vJqtxw", NpTrophy::sceNpTrophyConfigGetTrophyGroupDetails);
	LIB_FUNC("ndLeNWExeZE", NpTrophy::sceNpTrophyConfigGetTrophySetInfo);
	LIB_FUNC("6EOfS5SDgoo", NpTrophy::sceNpTrophyConfigGetTrophySetInfoInGroup);
	LIB_FUNC("MW5ygoZqEBs", NpTrophy::sceNpTrophyConfigGetTrophySetVersion);
	LIB_FUNC("3tWKpNKn5+I", NpTrophy::sceNpTrophyConfigGetTrophyTitleDetails);
	LIB_FUNC("iqYfxC12sak", NpTrophy::sceNpTrophyConfigHasGroupFeature);
	LIB_FUNC("XbkjbobZlCY", NpTrophy::sceNpTrophyCreateContext);
	LIB_FUNC("q7U6tEAQf7c", NpTrophy::sceNpTrophyCreateHandle);
	LIB_FUNC("E1Wrwd07Lr8", NpTrophy::sceNpTrophyDestroyContext);
	LIB_FUNC("GNcF4oidY0Y", NpTrophy::sceNpTrophyDestroyHandle);
	LIB_FUNC("HLwz1fRIycA", NpTrophy::sceNpTrophyGetGameIcon);
	LIB_FUNC("YYP3f2W09og", NpTrophy::sceNpTrophyGetGameInfo);
	LIB_FUNC("w4uMPmErD4I", NpTrophy::sceNpTrophyGetGroupIcon);
	LIB_FUNC("wTUwGfspKic", NpTrophy::sceNpTrophyGetGroupInfo);
	LIB_FUNC("eBL+l6HG9xk", NpTrophy::sceNpTrophyGetTrophyIcon);
	LIB_FUNC("qqUVGDgQBm0", NpTrophy::sceNpTrophyGetTrophyInfo);
	LIB_FUNC("LHuSmO3SLd8", NpTrophy::sceNpTrophyGetTrophyUnlockState);
	LIB_FUNC("Ht6MNTl-je4", NpTrophy::sceNpTrophyGroupArrayGetNum);
	LIB_FUNC("u9plkqa2e0k", NpTrophy::sceNpTrophyIntAbortHandle);
	LIB_FUNC("pE5yhroy9m0", NpTrophy::sceNpTrophyIntCheckNetSyncTitles);
	LIB_FUNC("edPIOFpEAvU", NpTrophy::sceNpTrophyIntCreateHandle);
	LIB_FUNC("DSh3EXpqAQ4", NpTrophy::sceNpTrophyIntDestroyHandle);
	LIB_FUNC("sng98qULzPA", NpTrophy::sceNpTrophyIntGetLocalTrophySummary);
	LIB_FUNC("t3CQzag7-zs", NpTrophy::sceNpTrophyIntGetProgress);
	LIB_FUNC("jF-mCgGuvbQ", NpTrophy::sceNpTrophyIntGetRunningTitle);
	LIB_FUNC("PeAyBjC5kp8", NpTrophy::sceNpTrophyIntGetRunningTitles);
	LIB_FUNC("PEo09Dkqv0o", NpTrophy::sceNpTrophyIntGetTrpIconByUri);
	LIB_FUNC("kF9zjnlAzIA", NpTrophy::sceNpTrophyIntNetSyncTitle);
	LIB_FUNC("UXiyfabxFNQ", NpTrophy::sceNpTrophyIntNetSyncTitles);
	LIB_FUNC("hvdThnVvwdY", NpTrophy::sceNpTrophyNumInfoGetTotal);
	LIB_FUNC("TJCAxto9SEU", NpTrophy::sceNpTrophyRegisterContext);
	LIB_FUNC("ITUmvpBPaG0", NpTrophy::sceNpTrophySetInfoGetTrophyFlagArray);
	LIB_FUNC("BSoSgiMVHnY", NpTrophy::sceNpTrophySetInfoGetTrophyNum);
	LIB_FUNC("d9jpdPz5f-8", NpTrophy::sceNpTrophyShowTrophyList);
	LIB_FUNC("JzJdh-JLtu0", NpTrophy::sceNpTrophySystemAbortHandle);
	LIB_FUNC("z8RCP536GOM", NpTrophy::sceNpTrophySystemBuildGroupIconUri);
	LIB_FUNC("Rd2FBOQE094", NpTrophy::sceNpTrophySystemBuildNetTrophyIconUri);
	LIB_FUNC("Q182x0rT75I", NpTrophy::sceNpTrophySystemBuildTitleIconUri);
	LIB_FUNC("lGnm5Kg-zpA", NpTrophy::sceNpTrophySystemBuildTrophyIconUri);
	LIB_FUNC("20wAMbXP-u0", NpTrophy::sceNpTrophySystemCheckNetSyncTitles);
	LIB_FUNC("sKGFFY59ksY", NpTrophy::sceNpTrophySystemCheckRecoveryRequired);
	LIB_FUNC("JMSapEtDH9Q", NpTrophy::sceNpTrophySystemCloseStorage);
	LIB_FUNC("dk27olS4CEE", NpTrophy::sceNpTrophySystemCreateContext);
	LIB_FUNC("cBzXEdzVzvs", NpTrophy::sceNpTrophySystemCreateHandle);
	LIB_FUNC("8aLlLHKP+No", NpTrophy::sceNpTrophySystemDbgCtl);
	LIB_FUNC("NobVwD8qcQY", NpTrophy::sceNpTrophySystemDebugLockTrophy);
	LIB_FUNC("yXJlgXljItk", NpTrophy::sceNpTrophySystemDebugUnlockTrophy);
	LIB_FUNC("U0TOSinfuvw", NpTrophy::sceNpTrophySystemDestroyContext);
	LIB_FUNC("-LC9hudmD+Y", NpTrophy::sceNpTrophySystemDestroyHandle);
	LIB_FUNC("q6eAMucXIEM", NpTrophy::sceNpTrophySystemDestroyTrophyConfig);
	LIB_FUNC("WdCUUJLQodM", NpTrophy::sceNpTrophySystemGetDbgParam);
	LIB_FUNC("4QYFwC7tn4U", NpTrophy::sceNpTrophySystemGetDbgParamInt);
	LIB_FUNC("OcllHFFcQkI", NpTrophy::sceNpTrophySystemGetGroupIcon);
	LIB_FUNC("tQ3tXfVZreU", NpTrophy::sceNpTrophySystemGetLocalTrophySummary);
	LIB_FUNC("g0dxBNTspC0", NpTrophy::sceNpTrophySystemGetNextTitleFileEntryStatus);
	LIB_FUNC("sJSDnJRJHhI", NpTrophy::sceNpTrophySystemGetProgress);
	LIB_FUNC("X47s4AamPGg", NpTrophy::sceNpTrophySystemGetTitleFileStatus);
	LIB_FUNC("7WPj4KCF3D8", NpTrophy::sceNpTrophySystemGetTitleIcon);
	LIB_FUNC("pzL+aAk0tQA", NpTrophy::sceNpTrophySystemGetTitleSyncStatus);
	LIB_FUNC("Ro4sI9xgYl4", NpTrophy::sceNpTrophySystemGetTrophyConfig);
	LIB_FUNC("7+OR1TU5QOA", NpTrophy::sceNpTrophySystemGetTrophyData);
	LIB_FUNC("aXhvf2OmbiE", NpTrophy::sceNpTrophySystemGetTrophyGroupData);
	LIB_FUNC("Rkt0bVyaa4Y", NpTrophy::sceNpTrophySystemGetTrophyIcon);
	LIB_FUNC("nXr5Rho8Bqk", NpTrophy::sceNpTrophySystemGetTrophyTitleData);
	LIB_FUNC("eV1rtLr+eys", NpTrophy::sceNpTrophySystemGetTrophyTitleIds);
	LIB_FUNC("SsGLKTfWfm0", NpTrophy::sceNpTrophySystemGetUserFileInfo);
	LIB_FUNC("XqLLsvl48kA", NpTrophy::sceNpTrophySystemGetUserFileStatus);
	LIB_FUNC("-qjm2fFE64M", NpTrophy::sceNpTrophySystemIsServerAvailable);
	LIB_FUNC("50BvYYzPTsY", NpTrophy::sceNpTrophySystemNetSyncTitle);
	LIB_FUNC("yDJ-r-8f4S4", NpTrophy::sceNpTrophySystemNetSyncTitles);
	LIB_FUNC("mWtsnHY8JZg", NpTrophy::sceNpTrophySystemOpenStorage);
	LIB_FUNC("tAxnXpzDgFw", NpTrophy::sceNpTrophySystemPerformRecovery);
	LIB_FUNC("tV18n8OcheI", NpTrophy::sceNpTrophySystemRemoveAll);
	LIB_FUNC("kV4DP0OTMNo", NpTrophy::sceNpTrophySystemRemoveTitleData);
	LIB_FUNC("lZSZoN8BstI", NpTrophy::sceNpTrophySystemRemoveUserData);
	LIB_FUNC("nytN-3-pdvI", NpTrophy::sceNpTrophySystemSetDbgParam);
	LIB_FUNC("JsRnDKRzvRw", NpTrophy::sceNpTrophySystemSetDbgParamInt);
	LIB_FUNC("28xmRUFao68", NpTrophy::sceNpTrophyUnlockTrophy);
	LIB_FUNC("FJZW2oHUHFk", NpTrophy::Func_149656DA81D41C59);
	LIB_FUNC("n4AHGHb-pfY", NpTrophy::Func_9F80071876FFA5F6);
	LIB_FUNC("+O9vU1CpGZA", NpTrophy::Func_F8EF6F5350A91990);
	LIB_FUNC("+not13BEdVI", NpTrophy::Func_FA7A2DD770447552);
}

} // namespace NpTrophy
namespace NpTus {

LIB_VERSION("libSceNpTus", 1, "libSceNpTus", 1, 1);

static int KYTY_SYSV_ABI sceNpTssCreateNpTitleCtx() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusCreateNpTitleCtx() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotData() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotDataAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetData() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsDataStatus() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsDataStatusAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatus() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatus() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetData() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetMultiSlotVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetMultiSlotVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariable() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssCreateNpTitleCtxA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssGetData() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssGetDataAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssGetSmallStorage() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssGetSmallStorageAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssGetStorage() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTssGetStorageAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAbortRequest() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusAddAndGetVariableForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusChangeModeForOtherSaveDataOwners() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusCreateNpTitleCtxA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusCreateRequest() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusCreateTitleCtx() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotDataA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotDataAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotDataVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotDataVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotVariableVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteMultiSlotVariableVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteNpTitleCtx() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusDeleteRequest() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetDataForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsDataStatusA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsDataStatusAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsDataStatusForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsDataStatusForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsVariableForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetFriendsVariableForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiSlotVariableForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusGetMultiUserVariableForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusPollAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetDataAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetMultiSlotVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetMultiSlotVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetMultiSlotVariableVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetMultiSlotVariableVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetThreadParam() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusSetTimeout() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableA() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableAAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableAVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableAVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableForCrossSave() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableForCrossSaveAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableForCrossSaveVUser() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusTryAndSetVariableForCrossSaveVUserAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}

static int KYTY_SYSV_ABI sceNpTusWaitAsync() {
	PRINT_NAME();
	return NP_NOT_SIGNED_IN;
}


LIB_DEFINE(InitNpTus_1) {
	LIB_FUNC("sRVb2Cf0GHg", NpTus::sceNpTssCreateNpTitleCtx);
	LIB_FUNC("cRVmNrJDbG8", NpTus::sceNpTusAddAndGetVariable);
	LIB_FUNC("Q2UmHdK04c8", NpTus::sceNpTusAddAndGetVariableAsync);
	LIB_FUNC("ukr6FBSrkJw", NpTus::sceNpTusAddAndGetVariableVUser);
	LIB_FUNC("lliK9T6ylJg", NpTus::sceNpTusAddAndGetVariableVUserAsync);
	LIB_FUNC("BIkMmUfNKWM", NpTus::sceNpTusCreateNpTitleCtx);
	LIB_FUNC("0DT5bP6YzBo", NpTus::sceNpTusDeleteMultiSlotData);
	LIB_FUNC("OCozl1ZtxRY", NpTus::sceNpTusDeleteMultiSlotDataAsync);
	LIB_FUNC("mYhbiRtkE1Y", NpTus::sceNpTusDeleteMultiSlotVariable);
	LIB_FUNC("0nDVqcYECoM", NpTus::sceNpTusDeleteMultiSlotVariableAsync);
	LIB_FUNC("XOzszO4ONWU", NpTus::sceNpTusGetData);
	LIB_FUNC("uHtKS5V1T5k", NpTus::sceNpTusGetDataAsync);
	LIB_FUNC("GQHCksS7aLs", NpTus::sceNpTusGetDataVUser);
	LIB_FUNC("5R6kI-8f+Hk", NpTus::sceNpTusGetDataVUserAsync);
	LIB_FUNC("DXigwIBTjWE", NpTus::sceNpTusGetFriendsDataStatus);
	LIB_FUNC("LUwvy0MOSqw", NpTus::sceNpTusGetFriendsDataStatusAsync);
	LIB_FUNC("cy+pAALkHp8", NpTus::sceNpTusGetFriendsVariable);
	LIB_FUNC("YFYWOwYI6DY", NpTus::sceNpTusGetFriendsVariableAsync);
	LIB_FUNC("pgcNwFHoOL4", NpTus::sceNpTusGetMultiSlotDataStatus);
	LIB_FUNC("Qyek420uZmM", NpTus::sceNpTusGetMultiSlotDataStatusAsync);
	LIB_FUNC("NGCeFUl5ckM", NpTus::sceNpTusGetMultiSlotDataStatusVUser);
	LIB_FUNC("bHWFSg6jvXc", NpTus::sceNpTusGetMultiSlotDataStatusVUserAsync);
	LIB_FUNC("F+eQlfcka98", NpTus::sceNpTusGetMultiSlotVariable);
	LIB_FUNC("bcPB2rnhQqo", NpTus::sceNpTusGetMultiSlotVariableAsync);
	LIB_FUNC("uFxVYJEkcmc", NpTus::sceNpTusGetMultiSlotVariableVUser);
	LIB_FUNC("qp-rTrq1klk", NpTus::sceNpTusGetMultiSlotVariableVUserAsync);
	LIB_FUNC("NvHjFkx2rnU", NpTus::sceNpTusGetMultiUserDataStatus);
	LIB_FUNC("0zkr0T+NYvI", NpTus::sceNpTusGetMultiUserDataStatusAsync);
	LIB_FUNC("xwJIlK0bHgA", NpTus::sceNpTusGetMultiUserDataStatusVUser);
	LIB_FUNC("I5dlIKkHNkQ", NpTus::sceNpTusGetMultiUserDataStatusVUserAsync);
	LIB_FUNC("6G9+4eIb+cY", NpTus::sceNpTusGetMultiUserVariable);
	LIB_FUNC("YRje5yEXS0U", NpTus::sceNpTusGetMultiUserVariableAsync);
	LIB_FUNC("zB0vaHTzA6g", NpTus::sceNpTusGetMultiUserVariableVUser);
	LIB_FUNC("xZXQuNSTC6o", NpTus::sceNpTusGetMultiUserVariableVUserAsync);
	LIB_FUNC("4NrufkNCkiE", NpTus::sceNpTusSetData);
	LIB_FUNC("G68xdfQuiyU", NpTus::sceNpTusSetDataAsync);
	LIB_FUNC("+RhzSuuXwxo", NpTus::sceNpTusSetDataVUser);
	LIB_FUNC("E4BCVfx-YfM", NpTus::sceNpTusSetDataVUserAsync);
	LIB_FUNC("c6aYoa47YgI", NpTus::sceNpTusSetMultiSlotVariable);
	LIB_FUNC("5J9GGMludxY", NpTus::sceNpTusSetMultiSlotVariableAsync);
	LIB_FUNC("ukC55HsotJ4", NpTus::sceNpTusTryAndSetVariable);
	LIB_FUNC("xQfR51i4kck", NpTus::sceNpTusTryAndSetVariableAsync);
	LIB_FUNC("ZbitD262GhY", NpTus::sceNpTusTryAndSetVariableVUser);
	LIB_FUNC("trZ6QGW6jHs", NpTus::sceNpTusTryAndSetVariableVUserAsync);
	LIB_FUNC("lBtrk+7lk14", NpTus::sceNpTssCreateNpTitleCtxA);
	LIB_FUNC("-SUR+UoLS6c", NpTus::sceNpTssGetData);
	LIB_FUNC("DS2yu3Sjj1o", NpTus::sceNpTssGetDataAsync);
	LIB_FUNC("lL+Z3zCKNTs", NpTus::sceNpTssGetSmallStorage);
	LIB_FUNC("f2Pe4LGS2II", NpTus::sceNpTssGetSmallStorageAsync);
	LIB_FUNC("IVSbAEOxJ6I", NpTus::sceNpTssGetStorage);
	LIB_FUNC("k5NZIzggbuk", NpTus::sceNpTssGetStorageAsync);
	LIB_FUNC("2eq1bMwgZYo", NpTus::sceNpTusAbortRequest);
	LIB_FUNC("wPFah4-5Xec", NpTus::sceNpTusAddAndGetVariableA);
	LIB_FUNC("2dB427dT3Iw", NpTus::sceNpTusAddAndGetVariableAAsync);
	LIB_FUNC("Nt1runsPVJc", NpTus::sceNpTusAddAndGetVariableAVUser);
	LIB_FUNC("GjlEgLCh4DY", NpTus::sceNpTusAddAndGetVariableAVUserAsync);
	LIB_FUNC("EPeq43CQKxY", NpTus::sceNpTusAddAndGetVariableForCrossSave);
	LIB_FUNC("mXZi1D2xwZE", NpTus::sceNpTusAddAndGetVariableForCrossSaveAsync);
	LIB_FUNC("4VLlu7EIjzk", NpTus::sceNpTusAddAndGetVariableForCrossSaveVUser);
	LIB_FUNC("6Lu9geO5TiA", NpTus::sceNpTusAddAndGetVariableForCrossSaveVUserAsync);
	LIB_FUNC("wjNhItL2wzg", NpTus::sceNpTusChangeModeForOtherSaveDataOwners);
	LIB_FUNC("1n-dGukBgnY", NpTus::sceNpTusCreateNpTitleCtxA);
	LIB_FUNC("3bh2aBvvmvM", NpTus::sceNpTusCreateRequest);
	LIB_FUNC("hhy8+oecGac", NpTus::sceNpTusCreateTitleCtx);
	LIB_FUNC("iXzUOM9sXU0", NpTus::sceNpTusDeleteMultiSlotDataA);
	LIB_FUNC("6-+Yqc-NppQ", NpTus::sceNpTusDeleteMultiSlotDataAAsync);
	LIB_FUNC("xutwCvsydkk", NpTus::sceNpTusDeleteMultiSlotDataVUser);
	LIB_FUNC("zDeH4tr+0cQ", NpTus::sceNpTusDeleteMultiSlotDataVUserAsync);
	LIB_FUNC("pwnE9Oa1uF8", NpTus::sceNpTusDeleteMultiSlotVariableA);
	LIB_FUNC("NQIw7tzo0Ow", NpTus::sceNpTusDeleteMultiSlotVariableAAsync);
	LIB_FUNC("o02Mtf8G6V0", NpTus::sceNpTusDeleteMultiSlotVariableVUser);
	LIB_FUNC("WCzd3cxhubo", NpTus::sceNpTusDeleteMultiSlotVariableVUserAsync);
	LIB_FUNC("H3uq7x0sZOI", NpTus::sceNpTusDeleteNpTitleCtx);
	LIB_FUNC("CcIH40dYS88", NpTus::sceNpTusDeleteRequest);
	LIB_FUNC("yWEHUFkY1qI", NpTus::sceNpTusGetDataA);
	LIB_FUNC("xzG8mG9YlKY", NpTus::sceNpTusGetDataAAsync);
	LIB_FUNC("iaH+Sxlw32k", NpTus::sceNpTusGetDataAVUser);
	LIB_FUNC("uoFvgzwawAY", NpTus::sceNpTusGetDataAVUserAsync);
	LIB_FUNC("1TE3OvH61qo", NpTus::sceNpTusGetDataForCrossSave);
	LIB_FUNC("CFPx3eyaT34", NpTus::sceNpTusGetDataForCrossSaveAsync);
	LIB_FUNC("-LxFGYCJwww", NpTus::sceNpTusGetDataForCrossSaveVUser);
	LIB_FUNC("B7rBR0CoYLI", NpTus::sceNpTusGetDataForCrossSaveVUserAsync);
	LIB_FUNC("yixh7HDKWfk", NpTus::sceNpTusGetFriendsDataStatusA);
	LIB_FUNC("OheijxY5RYE", NpTus::sceNpTusGetFriendsDataStatusAAsync);
	LIB_FUNC("TDoqRD+CE+M", NpTus::sceNpTusGetFriendsDataStatusForCrossSave);
	LIB_FUNC("68B6XDgSANk", NpTus::sceNpTusGetFriendsDataStatusForCrossSaveAsync);
	LIB_FUNC("C8TY-UnQoXg", NpTus::sceNpTusGetFriendsVariableA);
	LIB_FUNC("wrImtTqUSGM", NpTus::sceNpTusGetFriendsVariableAAsync);
	LIB_FUNC("mD6s8HtMdpk", NpTus::sceNpTusGetFriendsVariableForCrossSave);
	LIB_FUNC("FabW3QpY3gQ", NpTus::sceNpTusGetFriendsVariableForCrossSaveAsync);
	LIB_FUNC("833Y2TnyonE", NpTus::sceNpTusGetMultiSlotDataStatusA);
	LIB_FUNC("7uLPqiNvNLc", NpTus::sceNpTusGetMultiSlotDataStatusAAsync);
	LIB_FUNC("azmjx3jBAZA", NpTus::sceNpTusGetMultiSlotDataStatusAVUser);
	LIB_FUNC("668Ij9MYKEU", NpTus::sceNpTusGetMultiSlotDataStatusAVUserAsync);
	LIB_FUNC("DgpRToHWN40", NpTus::sceNpTusGetMultiSlotDataStatusForCrossSave);
	LIB_FUNC("LQ6CoHcp+ug", NpTus::sceNpTusGetMultiSlotDataStatusForCrossSaveAsync);
	LIB_FUNC("KBfBmtxCdmI", NpTus::sceNpTusGetMultiSlotDataStatusForCrossSaveVUser);
	LIB_FUNC("4UF2uu2eDCo", NpTus::sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync);
	LIB_FUNC("GDXlRTxgd+M", NpTus::sceNpTusGetMultiSlotVariableA);
	LIB_FUNC("2BnPSY1Oxd8", NpTus::sceNpTusGetMultiSlotVariableAAsync);
	LIB_FUNC("AsziNQ9X2uk", NpTus::sceNpTusGetMultiSlotVariableAVUser);
	LIB_FUNC("y-DJK+d+leg", NpTus::sceNpTusGetMultiSlotVariableAVUserAsync);
	LIB_FUNC("m9XZnxw9AmE", NpTus::sceNpTusGetMultiSlotVariableForCrossSave);
	LIB_FUNC("DFlBYT+Lm2I", NpTus::sceNpTusGetMultiSlotVariableForCrossSaveAsync);
	LIB_FUNC("wTuuw4-6HI8", NpTus::sceNpTusGetMultiSlotVariableForCrossSaveVUser);
	LIB_FUNC("DPcu0qWsd7Q", NpTus::sceNpTusGetMultiSlotVariableForCrossSaveVUserAsync);
	LIB_FUNC("lxNDPDnWfMc", NpTus::sceNpTusGetMultiUserDataStatusA);
	LIB_FUNC("kt+k6jegYZ8", NpTus::sceNpTusGetMultiUserDataStatusAAsync);
	LIB_FUNC("fJU2TZId210", NpTus::sceNpTusGetMultiUserDataStatusAVUser);
	LIB_FUNC("WBh3zfrjS38", NpTus::sceNpTusGetMultiUserDataStatusAVUserAsync);
	LIB_FUNC("cVeBif6zdZ4", NpTus::sceNpTusGetMultiUserDataStatusForCrossSave);
	LIB_FUNC("lq0Anwhj0wY", NpTus::sceNpTusGetMultiUserDataStatusForCrossSaveAsync);
	LIB_FUNC("w-c7U0MW2KY", NpTus::sceNpTusGetMultiUserDataStatusForCrossSaveVUser);
	LIB_FUNC("H6sQJ99usfE", NpTus::sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync);
	LIB_FUNC("Gjixv5hqRVY", NpTus::sceNpTusGetMultiUserVariableA);
	LIB_FUNC("eGunerNP9n0", NpTus::sceNpTusGetMultiUserVariableAAsync);
	LIB_FUNC("fVvocpq4mG4", NpTus::sceNpTusGetMultiUserVariableAVUser);
	LIB_FUNC("V8ZA3hHrAbw", NpTus::sceNpTusGetMultiUserVariableAVUserAsync);
	LIB_FUNC("Q5uQeScvTPE", NpTus::sceNpTusGetMultiUserVariableForCrossSave);
	LIB_FUNC("oZ8DMeTU-50", NpTus::sceNpTusGetMultiUserVariableForCrossSaveAsync);
	LIB_FUNC("Djuj2+1VNL0", NpTus::sceNpTusGetMultiUserVariableForCrossSaveVUser);
	LIB_FUNC("82RP7itI-zI", NpTus::sceNpTusGetMultiUserVariableForCrossSaveVUserAsync);
	LIB_FUNC("t7b6dmpQNiI", NpTus::sceNpTusPollAsync);
	LIB_FUNC("VzxN3tOouj8", NpTus::sceNpTusSetDataA);
	LIB_FUNC("4u58d6g6uwU", NpTus::sceNpTusSetDataAAsync);
	LIB_FUNC("kbWqOt3QjKU", NpTus::sceNpTusSetDataAVUser);
	LIB_FUNC("Fmx4tapJGzo", NpTus::sceNpTusSetDataAVUserAsync);
	LIB_FUNC("cf-WMA0jYCc", NpTus::sceNpTusSetMultiSlotVariableA);
	LIB_FUNC("ypMObSwfcns", NpTus::sceNpTusSetMultiSlotVariableAAsync);
	LIB_FUNC("1Cz0hTJFyh4", NpTus::sceNpTusSetMultiSlotVariableVUser);
	LIB_FUNC("CJAxTxQdwHM", NpTus::sceNpTusSetMultiSlotVariableVUserAsync);
	LIB_FUNC("6GKDdRCFx8c", NpTus::sceNpTusSetThreadParam);
	LIB_FUNC("KMlHj+tgfdQ", NpTus::sceNpTusSetTimeout);
	LIB_FUNC("0up4MP1wNtc", NpTus::sceNpTusTryAndSetVariableA);
	LIB_FUNC("bGTjTkHPHTE", NpTus::sceNpTusTryAndSetVariableAAsync);
	LIB_FUNC("oGIcxlUabSA", NpTus::sceNpTusTryAndSetVariableAVUser);
	LIB_FUNC("uf77muc5Bog", NpTus::sceNpTusTryAndSetVariableAVUserAsync);
	LIB_FUNC("MGvSJEHwyL8", NpTus::sceNpTusTryAndSetVariableForCrossSave);
	LIB_FUNC("JKGYZ2F1yT8", NpTus::sceNpTusTryAndSetVariableForCrossSaveAsync);
	LIB_FUNC("fcCwKpi4CbU", NpTus::sceNpTusTryAndSetVariableForCrossSaveVUser);
	LIB_FUNC("CjVIpztpTNc", NpTus::sceNpTusTryAndSetVariableForCrossSaveVUserAsync);
	LIB_FUNC("hYPJFWzFPjA", NpTus::sceNpTusWaitAsync);
}

} // namespace NpTus
} // namespace Libs
