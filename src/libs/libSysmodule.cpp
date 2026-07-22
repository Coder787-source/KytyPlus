#include "common/abi.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/logging/log.h"
#include "common/stringUtils.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Libs {

LIB_VERSION("Sysmodule", 1, "Sysmodule", 1, 1);

namespace LibKernel {
struct ModuleInfoForUnwind;
int KYTY_SYSV_ABI KernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info);
} // namespace LibKernel

namespace Sysmodule {

// Sony sysmodule error codes (libSceSysmodule).
constexpr int SYSMODULE_ERROR_INVALID_ID = static_cast<int>(0x805A1000);
constexpr int SYSMODULE_ERROR_NOT_LOADED = static_cast<int>(0x805A1001);

// HLE catalog entry: a firmware SPRX is never loaded. The matching host NIDs are
// registered by Libs::InitAll; Sysmodule only tracks the guest-visible load state.
struct HleModule {
	uint32_t    id          = 0;
	const char* name        = nullptr;
	bool        always_loaded = false; // present from process start on real Prospero
};

// Public (uint16-range) and internal (0x800000xx) modules that KytyPS5 already
// implements as host HLE. Unknown IDs still succeed on Load so titles never need
// a firmware dump; they just log once that the module is an empty HLE shell.
static constexpr HleModule kHleModules[] = {
    // Always-resident / internal modules (registered at InitAll, marked loaded at boot).
    {0x80000001u, "libSceAudioOut", true},
    {0x80000002u, "libSceAudioIn", true},
    {0x80000009u, "libSceNetCtl", true},
    {0x8000000Au, "libSceHttp", true},
    {0x8000000Bu, "libSceSsl", true},
    {0x8000000Du, "libSceNpManager", true},
    {0x8000000Fu, "libSceSaveData", true},
    {0x80000010u, "libSceSystemService", true},
    {0x80000011u, "libSceUserService", true},
    {0x80000018u, "libSceCommonDialog", true},
    {0x8000001Cu, "libSceNet", true},
    {0x80000020u, "libSceRtc", true},
    {0x80000022u, "libSceVideoOut", true},
    {0x80000023u, "libSceAjm", true},
    {0x80000024u, "libScePad", true},
    {0x8000008Cu, "libSceHttp2", true},
    {0x8000008Fu, "libSceNpWebApi2", true},

    // Dynamically loaded public modules with HLE backends.
    {0x0006u, "libSceFiber", false},
    {0x0007u, "libSceUlt", false},
    {0x000Bu, "libSceNgs2", false},
    {0x001Au, "libSceVoice", false},
    {0x001Bu, "libSceVoiceQoS", false},
    {0x0021u, "libSceRudp", false},
    {0x0080u, "libSceJson", false},
    {0x0081u, "libSceGameLiveStreaming", false},
    {0x0083u, "libScePlayGo", false},
    {0x0084u, "libSceFont", false},
    {0x0088u, "libSceAudiodec", false},
    {0x008Cu, "libScePngDec", false},
    {0x0095u, "libSceIme", false},
    {0x0096u, "libSceImeDialog", false},
    {0x0098u, "libSceFontFt", false},
    {0x009Du, "libSceNpAuth", false},
    {0x00A0u, "libSceSaveDataDialog", false},
    {0x00A3u, "libSceKeyboard", false},
    {0x00A4u, "libSceMsgDialog", false},
    {0x00A5u, "libSceAvPlayer", false},
    {0x00A6u, "libSceContentExport", false},
    {0x00A7u, "libSceAudio3d", false},
    {0x00A9u, "libSceMouse", false},
    {0x00ABu, "libSceWebBrowserDialog", false},
    {0x00ACu, "libSceErrorDialog", false},
    {0x00ADu, "libSceNpTrophy", false}, // maps to NpTrophy2 HLE
    {0x00B4u, "libSceAppContent", false},
    {0x00B5u, "libSceNpSignaling", false},
    {0x00B6u, "libSceRemoteplay", false},
    {0x00BAu, "libSceRandom", false},
    {0x00C7u, "libSceContentSearch", false},
    {0x00C8u, "libSceShareUtility", false},
    {0x00CCu, "libSceGameUpdate", false},
    {0x00CEu, "libSceSystemGesture", false},
    {0x00CFu, "libSceVideodec2", false},
    {0x00D3u, "libSceSharePlay", false},
    {0x00E2u, "libSceLoginDialog", false},
    {0x00E4u, "libSceSigninDialog", false},
    {0x00E7u, "libSceJson2", false},
    {0x00EEu, "libSceContentDelete", false},
    {0x0105u, "libSceNpUniversalDataSystem", false},
    {0x0106u, "libSceKeyboard", false},
    {0x0112u, "libSceNpSessionSignaling", false},
    {0x0113u, "libSceNpEntitlementAccess", false},
    {0x8000008Du, "libSceNpGameIntent", false},
};

struct ModuleState {
	const char* name   = "unknown";
	bool        loaded = false;
	bool        hle    = false; // true when we have a host implementation catalogued
};

static std::mutex                                g_mutex;
static std::unordered_map<uint32_t, ModuleState>  g_modules;
static std::unordered_set<uint32_t>               g_unknown_logged;
static std::unordered_set<std::string>           g_unknown_names_logged;
static std::atomic_bool                          g_bootstrapped {false};

static const HleModule* FindCatalogEntry(uint32_t id) {
	for (const auto& entry: kHleModules) {
		if (entry.id == id) {
			return &entry;
		}
	}
	return nullptr;
}

static void EnsureBootstrapLocked() {
	if (g_bootstrapped.load(std::memory_order_relaxed)) {
		return;
	}
	for (const auto& entry: kHleModules) {
		ModuleState state {};
		state.name = entry.name;
		state.hle  = true;
		// Host NIDs for every catalogued module are already registered by Libs::InitAll, so from
		// the guest's point of view the library is present — mark it loaded up front. always_loaded
		// only controls whether Unload is allowed to clear that state.
		state.loaded = true;
		g_modules.emplace(entry.id, state);
	}
	g_bootstrapped.store(true, std::memory_order_relaxed);
}

static ModuleState& EnsureModuleLocked(uint32_t id) {
	EnsureBootstrapLocked();
	auto it = g_modules.find(id);
	if (it != g_modules.end()) {
		return it->second;
	}
	const auto* catalog = FindCatalogEntry(id);
	ModuleState state {};
	if (catalog != nullptr) {
		state.name   = catalog->name;
		state.hle    = true;
		state.loaded = true;
	} else {
		state.name   = "unknown";
		state.hle    = false;
		state.loaded = false;
	}
	return g_modules.emplace(id, state).first->second;
}

static void LogUnknownOnce(uint32_t id, const char* action) {
	if (g_unknown_logged.insert(id).second) {
		LOGF_COLOR(Log::Color::Yellow,
		           "Sysmodule: %s unknown module id=0x%08" PRIx32
		           " as empty HLE (no firmware SPRX required)\n",
		           action, id);
	}
}

static void LogUnknownNameOnce(const char* name) {
	const char* safe = name != nullptr ? name : "(null)";
	if (g_unknown_names_logged.insert(safe).second) {
		LOGF_COLOR(Log::Color::Yellow,
		           "Sysmodule: loading unknown module name=%s as empty HLE "
		           "(no firmware SPRX required)\n",
		           safe);
	}
}

static bool IsValidPublicId(uint32_t id) {
	return id != 0 && (id & 0x80000000u) == 0;
}

static bool IsValidInternalId(uint32_t id) {
	return (id & 0x80000000u) != 0 && (id & 0x7fffffffu) != 0;
}

static int LoadModuleCommon(uint32_t id, bool internal_id, int* result_out) {
	if (internal_id ? !IsValidInternalId(id) : !IsValidPublicId(id)) {
		return SYSMODULE_ERROR_INVALID_ID;
	}

	std::lock_guard lock(g_mutex);
	auto&           state = EnsureModuleLocked(id);
	if (!state.hle) {
		LogUnknownOnce(id, "loading");
	} else {
		LOGF("Sysmodule: load HLE module id=0x%08" PRIx32 " name=%s%s\n", id, state.name,
		     state.loaded ? " (already loaded)" : "");
	}
	state.loaded = true;
	if (result_out != nullptr) {
		*result_out = OK;
	}
	return OK;
}

static int UnloadModuleCommon(uint32_t id, bool internal_id) {
	if (internal_id ? !IsValidInternalId(id) : !IsValidPublicId(id)) {
		return SYSMODULE_ERROR_INVALID_ID;
	}

	std::lock_guard lock(g_mutex);
	auto&           state = EnsureModuleLocked(id);
	if (!state.loaded) {
		return SYSMODULE_ERROR_NOT_LOADED;
	}
	// Always-resident modules stay loaded; unload is a no-op success on hardware for many of
	// these, and tearing down HLE NIDs mid-process would just crash the guest.
	const auto* catalog = FindCatalogEntry(id);
	if (catalog != nullptr && catalog->always_loaded) {
		return OK;
	}
	state.loaded = false;
	LOGF("Sysmodule: unload HLE module id=0x%08" PRIx32 " name=%s\n", id, state.name);
	return OK;
}

static int IsLoadedCommon(uint32_t id, bool internal_id) {
	if (internal_id ? !IsValidInternalId(id) : !IsValidPublicId(id)) {
		return SYSMODULE_ERROR_INVALID_ID;
	}

	std::lock_guard lock(g_mutex);
	const auto&     state = EnsureModuleLocked(id);
	return state.loaded ? OK : SYSMODULE_ERROR_NOT_LOADED;
}

static KYTY_SYSV_ABI int SysmoduleGetModuleInfoForUnwind(uint64_t addr, int flags,
                                                         LibKernel::ModuleInfoForUnwind* info) {
	return LibKernel::KernelGetModuleInfoForUnwind(addr, flags, info);
}

static KYTY_SYSV_ABI int SysmoduleGetModuleHandleInternal(uint32_t id, int* handle) {
	PRINT_NAME();
	if (!IsValidInternalId(id) || handle == nullptr) {
		return SYSMODULE_ERROR_INVALID_ID;
	}
	std::lock_guard lock(g_mutex);
	const auto&     state = EnsureModuleLocked(id);
	if (!state.loaded) {
		return SYSMODULE_ERROR_NOT_LOADED;
	}
	// Fabricate a stable non-zero handle from the module id; guests only compare / pass it around.
	*handle = static_cast<int>(id);
	return OK;
}

static KYTY_SYSV_ABI int SysmoduleIsCalledFromSysModule() {
	PRINT_NAME();
	return OK;
}

static KYTY_SYSV_ABI int SysmoduleIsCameraPreloaded() {
	PRINT_NAME();
	return OK;
}

static KYTY_SYSV_ABI int SysmoduleLoadModule(uint32_t id) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	return LoadModuleCommon(id, false, nullptr);
}

static KYTY_SYSV_ABI int SysmoduleUnloadModule(uint32_t id) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	return UnloadModuleCommon(id, false);
}

static KYTY_SYSV_ABI int SysmoduleIsLoaded(uint32_t id) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	return IsLoadedCommon(id, false);
}

static KYTY_SYSV_ABI int SysmoduleLoadModuleInternal(uint32_t id) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	return LoadModuleCommon(id, true, nullptr);
}

static KYTY_SYSV_ABI int SysmoduleUnloadModuleInternal(uint32_t id) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	return UnloadModuleCommon(id, true);
}

static KYTY_SYSV_ABI int SysmoduleIsLoadedInternal(uint32_t id) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	return IsLoadedCommon(id, true);
}

static KYTY_SYSV_ABI int SysmoduleLoadModuleInternalWithArg(uint32_t id, int argc, const void* argv,
                                                            uint64_t unk, int* result_out) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 " argc=%d argv=0x%016" PRIx64 " unk=0x%016" PRIx64 "\n", id, argc,
	     reinterpret_cast<uint64_t>(argv), unk);
	if (unk != 0) {
		return SYSMODULE_ERROR_INVALID_ID;
	}
	(void)argc;
	(void)argv;
	return LoadModuleCommon(id, true, result_out);
}

static KYTY_SYSV_ABI int SysmoduleUnloadModuleInternalWithArg(uint32_t id, int argc,
                                                              const void* argv, uint64_t unk,
                                                              int* result_out) {
	PRINT_NAME();
	LOGF("\t id = 0x%08" PRIx32 "\n", id);
	if (unk != 0) {
		return SYSMODULE_ERROR_INVALID_ID;
	}
	(void)argc;
	(void)argv;
	const int result = UnloadModuleCommon(id, true);
	if (result_out != nullptr) {
		*result_out = result;
	}
	return result;
}

static KYTY_SYSV_ABI int SysmoduleLoadModuleByNameInternal(const char* name, uint64_t a1,
                                                           uint64_t a2, uint64_t a3, uint64_t a4,
                                                           uint64_t a5) {
	PRINT_NAME();
	LOGF("\t name = %s\n", name != nullptr ? name : "(null)");
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	// Name-based loads are rare; treat as soft HLE success so a missing firmware SPRX never aborts.
	if (name == nullptr || name[0] == '\0') {
		return SYSMODULE_ERROR_INVALID_ID;
	}
	std::lock_guard lock(g_mutex);
	EnsureBootstrapLocked();
	for (auto& [id, state]: g_modules) {
		if (state.name != nullptr && std::strcmp(state.name, name) == 0) {
			state.loaded = true;
			return OK;
		}
	}
	LogUnknownNameOnce(name);
	return OK;
}

static KYTY_SYSV_ABI int SysmoduleUnloadModuleByNameInternal(const char* name, uint64_t a1,
                                                             uint64_t a2, uint64_t a3, uint64_t a4,
                                                             uint64_t a5) {
	PRINT_NAME();
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	if (name == nullptr || name[0] == '\0') {
		return SYSMODULE_ERROR_INVALID_ID;
	}
	std::lock_guard lock(g_mutex);
	EnsureBootstrapLocked();
	for (auto& [id, state]: g_modules) {
		if (state.name != nullptr && std::strcmp(state.name, name) == 0) {
			const auto* catalog = FindCatalogEntry(id);
			if (catalog == nullptr || !catalog->always_loaded) {
				state.loaded = false;
			}
			return OK;
		}
	}
	return SYSMODULE_ERROR_NOT_LOADED;
}

static KYTY_SYSV_ABI int SysmoduleMapLibcForLibkernel() {
	PRINT_NAME();
	return OK;
}

static KYTY_SYSV_ABI int SysmodulePreloadModuleForLibkernel() {
	PRINT_NAME();
	std::lock_guard lock(g_mutex);
	EnsureBootstrapLocked();
	for (auto& [id, state]: g_modules) {
		const auto* catalog = FindCatalogEntry(id);
		if (catalog != nullptr && catalog->always_loaded) {
			state.loaded = true;
		}
	}
	LOGF("Sysmodule: preloaded always-resident HLE modules for libkernel\n");
	return OK;
}

} // namespace Sysmodule

LIB_DEFINE(InitSysmodule_1) {
	// Mark always-resident HLE modules loaded before any guest code runs.
	{
		std::lock_guard lock(Sysmodule::g_mutex);
		Sysmodule::EnsureBootstrapLocked();
	}

	LIB_FUNC("D8cuU4d72xM", Sysmodule::SysmoduleGetModuleHandleInternal);
	LIB_FUNC("4fU5yvOkVG4", Sysmodule::SysmoduleGetModuleInfoForUnwind);
	LIB_FUNC("ctfO7dQ7geg", Sysmodule::SysmoduleIsCalledFromSysModule);
	LIB_FUNC("no6T3EfiS3E", Sysmodule::SysmoduleIsCameraPreloaded);
	LIB_FUNC("eR2bZFAAU0Q", Sysmodule::SysmoduleUnloadModule);
	LIB_FUNC("hHrGoGoNf+s", Sysmodule::SysmoduleLoadModuleInternalWithArg);
	LIB_FUNC("g8cM39EUZ6o", Sysmodule::SysmoduleLoadModule);
	LIB_FUNC("fMP5NHUOaMk", Sysmodule::SysmoduleIsLoaded);
	LIB_FUNC("ynFKQ5bfGks", Sysmodule::SysmoduleIsLoadedInternal);
	LIB_FUNC("39iV5E1HoCk", Sysmodule::SysmoduleLoadModuleInternal);
	LIB_FUNC("CU8m+Qs+HN4", Sysmodule::SysmoduleLoadModuleByNameInternal);
	LIB_FUNC("lZ6RvVl0vo0", Sysmodule::SysmoduleMapLibcForLibkernel);
	LIB_FUNC("DOO+zuW1lrE", Sysmodule::SysmodulePreloadModuleForLibkernel);
	LIB_FUNC("vpTHmA6Knvg", Sysmodule::SysmoduleUnloadModuleByNameInternal);
	LIB_FUNC("vXZhrtJxkGc", Sysmodule::SysmoduleUnloadModuleInternal);
	LIB_FUNC("aKa6YfBKZs4", Sysmodule::SysmoduleUnloadModuleInternalWithArg);
}

} // namespace Libs
