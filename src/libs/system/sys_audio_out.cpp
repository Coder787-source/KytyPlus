#include "sys_audio_out.h"
#include "logging.h"
#include "audio/tempest_audio.h"
#include <vector>

static TempestAudio* g_tempestAudio = nullptr;

int sceAudioOutOpen(int userId, int type, int index, int len, int freq, int param) {
    LOG_INFO("GTA V: AudioOutOpen called (userId=%d, type=%d, freq=%d)", userId, type, freq);

    if (!g_tempestAudio) {
        g_tempestAudio = new TempestAudio();
        if (!g_tempestAudio->Initialize()) {
            delete g_tempestAudio;
            g_tempestAudio = nullptr;
            return -1;
        }
    }

    // GTA V expects stereo audio at 48kHz
    int handle = g_tempestAudio->OpenPort(userId, len, freq);
    if (handle < 0) {
        LOG_ERROR("AudioOutOpen: Failed to open audio port");
        return -1;
    }

    return handle;
}

int sceAudioOutOutput(int handle, const void* audioData, int size) {
    if (!g_tempestAudio || handle < 0) {
        return -1;
    }

    // GTA V outputs 3D audio - route to Tempest engine
    return g_tempestAudio->Output(handle, audioData, size);
}

int sceAudioOutClose(int handle) {
    if (!g_tempestAudio || handle < 0) {
        return -1;
    }

    return g_tempestAudio->ClosePort(handle);
}

int sceAudioOutGetSystemState(int* state) {
    if (!state) return -1;

    // GTA V checks this for audio system readiness
    *state = 1; // Ready
    return 0;
}