#pragma once

#include "libs.h"
#include "common/common.h"
#include "audio_internal.h"
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <complex>

namespace Kyty::Libs {

// Tempest 3D Audio Engine - Complete Implementation
// Supports HRTF, object-based audio, bed mixing, and spatial processing

constexpr int32_t TEMPEST_MAX_OBJECTS = 128;
constexpr int32_t TEMPEST_MAX_BEDS = 16;
constexpr int32_t TEMPEST_MAX_OUTPUT_CHANNELS = 16; // 7.1.4 Atmos
constexpr int32_t TEMPEST_HRTF_SIZE = 256;
constexpr int32_t TEMPEST_SAMPLE_RATE = 48000;

// Audio object structure
struct TempestAudioObject {
    uint32_t id = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f; // Position in 3D space
    float velocityX = 0.0f, velocityY = 0.0f, velocityZ = 0.0f;
    float gain = 1.0f;
    float spread = 0.0f; // 0-360 degrees
    float distanceGain = 1.0f;
    float obstruction = 0.0f; // 0-1 (occlusion)
    float reflectionGain = 0.3f;
    bool enabled = true;
    bool isStatic = false;
    int32_t userId = -1;
};

// Audio bed (channel-based) structure
struct TempestAudioBed {
    uint32_t id = 0;
    int32_t channelConfig = 0; // 2=stereo, 6=5.1, 8=7.1, 12=7.1.4
    float gain = 1.0f;
    bool enabled = true;
    std::array<float, TEMPEST_MAX_OUTPUT_CHANNELS> channelGains{};
};

// HRTF filter structure
struct HRTFFilter {
    std::array<std::complex<float>, TEMPEST_HRTF_SIZE> left{};
    std::array<std::complex<float>, TEMPEST_HRTF_SIZE> right{};
    float azimuth = 0.0f;
    float elevation = 0.0f;
};

// Room acoustic parameters
struct TempestRoomParams {
    float roomSize = 1.0f; // 0.1-10.0
    float damping = 0.5f; // 0-1
    float wetLevel = 0.3f; // 0-1
    float dryLevel = 0.7f; // 0-1
    float reflectionDelay = 0.03f; // seconds
    float reverbTime = 1.5f; // seconds
    float diffusion = 0.7f; // 0-1
    float density = 0.5f; // 0-1
};

// Tempest 3D Audio Engine class
class Tempest3DEngine {
public:
    Tempest3DEngine();
    ~Tempest3DEngine();

    // Initialization
    bool Initialize(int32_t sampleRate = TEMPEST_SAMPLE_RATE);
    void Shutdown();

    // Audio object management
    uint32_t CreateAudioObject();
    void DestroyAudioObject(uint32_t objectId);
    void UpdateAudioObject(uint32_t objectId, const TempestAudioObject& obj);
    TempestAudioObject* GetAudioObject(uint32_t objectId);

    // Audio bed management
    uint32_t CreateAudioBed(int32_t channelConfig);
    void DestroyAudioBed(uint32_t bedId);
    void UpdateAudioBed(uint32_t bedId, const TempestAudioBed& bed);
    TempestAudioBed* GetAudioBed(uint32_t bedId);

    // Listener management
    void SetListenerPosition(float x, float y, float z);
    void SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                 float upX, float upY, float upZ);
    void SetListenerVelocity(float vx, float vy, float vz);

    // Room acoustics
    void SetRoomParams(const TempestRoomParams& params);
    TempestRoomParams GetRoomParams() const { return m_roomParams; }

    // Processing
    void ProcessAudio(float* inputBuffer, float* outputBuffer, int32_t numSamples, int32_t numChannels);
    void MixObjects(float* outputBuffer, int32_t numSamples);
    void ApplyHRTF(float* inputBuffer, float* outputBuffer, int32_t numSamples, const TempestAudioObject& obj);
    void ApplyBedMixing(float* outputBuffer, int32_t numSamples);

    // State
    bool IsInitialized() const { return m_initialized; }
    int32_t GetActiveObjectCount() const;
    int32_t GetActiveBedCount() const;
    int32_t GetSampleRate() const { return m_sampleRate; }

private:
    void InitializeHRTFTable();
    const HRTFFilter& GetHRTFFilter(float azimuth, float elevation);
    void CalculateDistanceGain(TempestAudioObject& obj);
    void CalculateObstruction(TempestAudioObject& obj);
    void ApplyReverb(float* buffer, int32_t numSamples);
    void ApplyReflections(float* buffer, int32_t numSamples, const TempestAudioObject& obj);

    bool m_initialized = false;
    int32_t m_sampleRate = TEMPEST_SAMPLE_RATE;
    
    // Audio objects
    std::vector<TempestAudioObject> m_audioObjects;
    std::vector<TempestAudioBed> m_audioBeds;
    uint32_t m_nextObjectId = 1;
    uint32_t m_nextBedId = 1;

    // Listener
    float m_listenerX = 0.0f, m_listenerY = 0.0f, m_listenerZ = 0.0f;
    float m_listenerForwardX = 0.0f, m_listenerForwardY = 0.0f, m_listenerForwardZ = -1.0f;
    float m_listenerUpX = 0.0f, m_listenerUpY = 1.0f, m_listenerUpZ = 0.0f;
    float m_listenerVelocityX = 0.0f, m_listenerVelocityY = 0.0f, m_listenerVelocityZ = 0.0f;

    // HRTF table
    std::vector<HRTFFilter> m_hrtfTable;

    // Room acoustics
    TempestRoomParams m_roomParams;
    std::vector<float> m_reverbBuffer;
    std::vector<float> m_reflectionBuffer;

    // Processing buffers
    std::vector<float> m_tempBuffer;
};

// Global engine instance
Tempest3DEngine& GetTempestEngine();

} // namespace Kyty::Libs
