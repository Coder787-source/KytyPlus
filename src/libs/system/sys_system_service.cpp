#include "sys_system_service.h"
#include "logging.h"
#include "kernel/process.h"
#include "kernel/memory.h"
#include <string>

int sceSystemServiceLoadExec(const char* path, void* args) {
    LOG_INFO("GTA V: LoadExec called for %s", path);

    // GTA V expects this to load the main executable
    if (!path || strlen(path) == 0) {
        LOG_ERROR("LoadExec: Invalid path");
        return -1;
    }

    // Simulate process creation (GTA V expects this to succeed)
    Process* proc = Process::Create(path);
    if (!proc) {
        LOG_ERROR("LoadExec: Failed to create process");
        return -1;
    }

    // Pass arguments (GTA V may use this for launch parameters)
    if (args) {
        proc->SetArgs(static_cast<char**>(args));
    }

    return 0;
}

int sceSystemServiceGetStatus() {
    // GTA V checks this to confirm system readiness
    // Return 0 (ready) to allow GTA V to proceed
    return 0;
}

int sceSystemServiceHideSplashScreen() {
    LOG_INFO("GTA V: HideSplashScreen called");
    // GTA V calls this after loading
    // No actual implementation needed for in-game
    return 0;
}

int sceSystemServiceParamGetInt(int id, int* value) {
    if (!value) return -1;

    // GTA V queries system parameters
    switch (id) {
        case 0: *value = 1; break; // System language (English)
        case 1: *value = 1; break; // Region (US)
        case 2: *value = 1; break; // Console type (PS5)
        default: *value = 0; break;
    }

    return 0;
}

int sceSystemServiceParamGetString(int id, char* buffer, size_t len) {
    if (!buffer || len == 0) return -1;

    // GTA V queries system strings
    switch (id) {
        case 0: strncpy(buffer, "en-US", len); break; // Language
        case 1: strncpy(buffer, "US", len); break;    // Region
        default: buffer[0] = '\0'; break;
    }

    return 0;
}