#include "common/abi.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/logging/log.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "loader/symbolDatabase.h"

// Non-interactive local-install dialog compatibility; the title may implement
// its own download UI independently of this system dialog.

namespace Libs {

LIB_VERSION("PlayGoDialog", 1, "PlayGoDialog", 1, 1);

namespace PlayGoDialog {

// CommonDialog::Status
static constexpr int32_t ORBIS_COMMON_DIALOG_STATUS_NONE = 0;

// CommonDialog::Result (value 3 allows games to proceed; matches shadPS4 scePlayGoDialogGetResult)
static constexpr int32_t ORBIS_COMMON_DIALOG_RESULT_PROCEED = 3;

// CommonDialog::Error (0x80B8000D as signed 32-bit)
static constexpr int32_t ORBIS_COMMON_DIALOG_ERROR_ARG_NULL = -2137780209;

struct CommonDialogBaseParam {
	uint64_t size; // std::size_t in shadPS4 commondialog.h
	uint8_t  reserved[36];
	uint32_t magic;
}; // 0x30, 8-byte alignment preserved

struct OrbisPlayGoDialogResult {
	uint8_t unk1[0x4];
	int32_t result;
	uint8_t unk2[0x20];
};
static_assert(sizeof(OrbisPlayGoDialogResult) == 0x28);

struct OrbisPlayGoDialogParam {
	CommonDialogBaseParam base_param;
	int32_t               size;
	uint8_t               unk[0x30];
};
static_assert(sizeof(OrbisPlayGoDialogParam) == 0x68);

int KYTY_SYSV_ABI PlayGoDialogInitialize() {
	PRINT_NAME();
	return OK;
}

int KYTY_SYSV_ABI PlayGoDialogTerminate() {
	PRINT_NAME();
	return OK;
}

int KYTY_SYSV_ABI PlayGoDialogOpen(const OrbisPlayGoDialogParam* param) {
	PRINT_NAME();
	LOGF("PlayGoDialogOpen: local-install compatibility\n");

	if (param == nullptr) {
		return ORBIS_COMMON_DIALOG_ERROR_ARG_NULL;
	}

	LOGF("\t size = %" PRId32 "\n", param->size);

	return OK;
}

int KYTY_SYSV_ABI PlayGoDialogClose() {
	PRINT_NAME();
	return OK;
}

int KYTY_SYSV_ABI PlayGoDialogGetStatus() {
	PRINT_NAME();
	// No dialog is ever actually shown, so report NONE.
	return ORBIS_COMMON_DIALOG_STATUS_NONE;
}

int KYTY_SYSV_ABI PlayGoDialogUpdateStatus() {
	PRINT_NAME();
	// Never RUNNING; immediately report NONE so the game treats the download as done.
	return ORBIS_COMMON_DIALOG_STATUS_NONE;
}

int KYTY_SYSV_ABI PlayGoDialogGetResult(OrbisPlayGoDialogResult* result) {
	PRINT_NAME();

	if (result == nullptr) {
		return ORBIS_COMMON_DIALOG_ERROR_ARG_NULL;
	}

	result->result = ORBIS_COMMON_DIALOG_RESULT_PROCEED;

	return OK;
}

} // namespace PlayGoDialog

LIB_DEFINE(InitPlayGoDialog_1) {
	LIB_FUNC("fECamTJKpsM", PlayGoDialog::PlayGoDialogInitialize);
	LIB_FUNC("okgIGdr5Iz0", PlayGoDialog::PlayGoDialogTerminate);
	LIB_FUNC("kHd72ukqbxw", PlayGoDialog::PlayGoDialogOpen);
	LIB_FUNC("fbigNQiZpm0", PlayGoDialog::PlayGoDialogClose);
	LIB_FUNC("wx9TDplJKB4", PlayGoDialog::PlayGoDialogGetResult);
	LIB_FUNC("NOAMxY2EGS0", PlayGoDialog::PlayGoDialogGetStatus);
	LIB_FUNC("Yb60K7BST48", PlayGoDialog::PlayGoDialogUpdateStatus);
}

} // namespace Libs
