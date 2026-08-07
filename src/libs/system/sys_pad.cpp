#include "sys_pad.h"
#include "logging.h"
#include "input/controller.h"
#include <vector>

static Controller* g_controller = nullptr;

int scePadInit() {
    LOG_INFO("GTA V: PadInit called");

    if (!g_controller) {
        g_controller = new Controller();
        if (!g_controller->Initialize()) {
            delete g_controller;
            g_controller = nullptr;
            return -1;
        }
    }

    return 0;
}

int scePadOpen(int userId, int type, int index, const void* param) {
    LOG_INFO("GTA V: PadOpen called (userId=%d, type=%d)", userId, type);

    if (!g_controller) {
        return -1;
    }

    // GTA V expects DualSense controller
    int handle = g_controller->OpenPort(userId);
    if (handle < 0) {
        LOG_ERROR("PadOpen: Failed to open controller port");
        return -1;
    }

    return handle;
}

int scePadRead(int handle, ScePadData* data, int count) {
    if (!g_controller || handle < 0 || !data || count == 0) {
        return -1;
    }

    // GTA V reads controller input (DualSense features)
    return g_controller->Read(handle, data, count) ? 0 : -1;
}

int scePadClose(int handle) {
    if (!g_controller || handle < 0) {
        return -1;
    }

    return g_controller->ClosePort(handle) ? 0 : -1;
}

int scePadSetVibration(int handle, const ScePadVibrationParam* param) {
    if (!g_controller || handle < 0 || !param) {
        return -1;
    }

    // GTA V uses DualSense haptics
    return g_controller->SetVibration(handle, param) ? 0 : -1;
}

int scePadSetLightBar(int handle, const ScePadLightBarParam* param) {
    if (!g_controller || handle < 0 || !param) {
        return -1;
    }

    // GTA V uses DualSense light bar
    return g_controller->SetLightBar(handle, param) ? 0 : -1;
}