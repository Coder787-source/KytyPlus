// Local, non-interactive commerce dialog compatibility. This does not contact PSN
// or establish ownership of remote content. Stage-loading behavior must be verified
// separately; matching an import does not prove this dialog is the loading gate.

#include "libs/libs.h"
#include "loader/symbolDatabase.h"
#include "libs/errno.h"

#include <common/abi.h>

namespace Libs {

LIB_VERSION("libSceNpCommerce", 1, "libSceNpCommerce", 1, 1);

namespace NpCommerce {

static constexpr int32_t STATUS_NONE        = 0;
static constexpr int32_t STATUS_INITIALIZED = 1;
static constexpr int32_t STATUS_RUNNING     = 2;
static constexpr int32_t STATUS_FINISHED    = 3;

static constexpr int32_t ERROR_ARG_NULL = -2137780209; // 0x80B8000D

static constexpr int32_t RESULT_OK        = 0;
static constexpr int32_t RESULT_PURCHASED = 2;

struct CommonDialogBaseParam {
	uint64_t size;
	uint8_t  reserved[36];
	uint32_t magic;
};

struct OrbisNpCommerceDialogParam {
	CommonDialogBaseParam base_param;
	int32_t               size;
	int32_t               user_id;
	int32_t               mode;
	uint32_t              service_label;
	const char* const*    targets;
	uint32_t              num_targets;
	int32_t               pad0;
	uint64_t              features;
	void*                 user_data;
	uint8_t               reserved[32];
};
static_assert(sizeof(OrbisNpCommerceDialogParam) == 0x80);

struct OrbisNpCommerceDialogResult {
	int32_t  result;
	bool     authorized;
	uint8_t  _reserved_pad[3];
	void*    user_data;
	uint8_t  reserved[32];
};
static_assert(sizeof(OrbisNpCommerceDialogResult) == 0x30);

static int32_t g_status      = STATUS_FINISHED;
static int32_t g_result_code = RESULT_PURCHASED;
static void*   g_user_data   = nullptr;

int KYTY_SYSV_ABI NpCommerceDialogInitialize() {
	PRINT_NAME();
	g_status = STATUS_INITIALIZED;
	return OK;
}

int KYTY_SYSV_ABI NpCommerceDialogInitializeInternal() {
	PRINT_NAME();
	return NpCommerceDialogInitialize();
}

int KYTY_SYSV_ABI NpCommerceDialogTerminate() {
	PRINT_NAME();
	g_status = STATUS_FINISHED;
	return OK;
}

int KYTY_SYSV_ABI NpCommerceDialogOpen(const OrbisNpCommerceDialogParam* param) {
	PRINT_NAME();
	if (param == nullptr) {
		return ERROR_ARG_NULL;
	}
	LOGF("\t mode = %" PRId32 "\n", param->mode);
	g_user_data   = param->user_data;
	g_result_code = RESULT_PURCHASED;
	g_status      = STATUS_FINISHED;
	return OK;
}

int KYTY_SYSV_ABI NpCommerceDialogClose() {
	PRINT_NAME();
	g_status = STATUS_FINISHED;
	return OK;
}

int KYTY_SYSV_ABI NpCommerceDialogGetStatus() {
	PRINT_NAME();
	return g_status;
}

int KYTY_SYSV_ABI NpCommerceDialogUpdateStatus() {
	PRINT_NAME();
	return g_status;
}

int KYTY_SYSV_ABI NpCommerceDialogGetResult(OrbisNpCommerceDialogResult* result) {
	PRINT_NAME();
	if (result == nullptr) {
		return ERROR_ARG_NULL;
	}
	result->result     = g_result_code;
	result->authorized = true;
	result->user_data  = g_user_data;
	return g_result_code;
}

int KYTY_SYSV_ABI NpCommerceShowPsStoreIcon(int32_t pos) {
	PRINT_NAME();
	return OK;
}

int KYTY_SYSV_ABI NpCommerceHidePsStoreIcon() {
	PRINT_NAME();
	return OK;
}

int KYTY_SYSV_ABI NpCommerceSetPsStoreIconLayout() {
	PRINT_NAME();
	return OK;
}

} // namespace NpCommerce

LIB_DEFINE(InitNpCommerce_1) {
	LIB_FUNC("0aR2aWmQal4", NpCommerce::NpCommerceDialogInitialize);
	LIB_FUNC("9ZiLXAGG5rg", NpCommerce::NpCommerceDialogInitializeInternal);
	LIB_FUNC("m-I92Ab50W8", NpCommerce::NpCommerceDialogTerminate);
	LIB_FUNC("DfSCDRA3EjY", NpCommerce::NpCommerceDialogOpen);
	LIB_FUNC("NU3ckGHMFXo", NpCommerce::NpCommerceDialogClose);
	LIB_FUNC("CCbC+lqqvF0", NpCommerce::NpCommerceDialogGetStatus);
	LIB_FUNC("LR5cwFMMCVE", NpCommerce::NpCommerceDialogUpdateStatus);
	LIB_FUNC("r42bWcQbtZY", NpCommerce::NpCommerceDialogGetResult);
	LIB_FUNC("DHmwsa6S8Tc", NpCommerce::NpCommerceShowPsStoreIcon);
	LIB_FUNC("dsqCVsNM0Zg", NpCommerce::NpCommerceHidePsStoreIcon);
	LIB_FUNC("uKTDW8hk-ts", NpCommerce::NpCommerceSetPsStoreIconLayout);
}

} // namespace Libs
