#include "libs/tempest3D.h"
#include "common/log.h"
#include <algorithm>
#include <cstring>

namespace Kyty::Libs {

// Global engine instance
static Tempest3DEngine g_tempestEngine;

Tempest3DEngine& GetTempestEngine() {
    return g_tempestEngine;
}

Tempest3DEngine::Tempest3DEngine() {
    m_audioObjects.reserve(TEMPEST_MAX_OBJECTS);
    m_audioBeds.reserve(TEMPEST_MAX_BEDS);
    m_hrtfTable.reserve(360 * 18); // 360 azimuth x 18 elevation
    m_reverbBuffer.resize(TEMPEST_SAMPLE_RATE * 2); // 2 seconds
    m_reflectionBuffer.resize(TEMPEST_SAMPLE_RATE / 10); // 100ms
    m_tempBuffer.resize(TEMPEST_MAX_OUTPUT_CHANNELS * TEMPEST_SAMPLE_RATE / 100);
}

Tempest3DEngine::~Tempest3DEngine() {
    Shutdown();
}

bool Tempest3DEngine::Initialize(int32_t sampleRate) {
    if (m_initialized) {
        return true;
    }

    m_sampleRate = sampleRate;
    
    // Initialize HRTF table
    InitializeHRTFTable();
    
    // Resize buffers
    m_reverbBuffer.resize(sampleRate * 2);
    m_reflectionBuffer.resize(sampleRate / 10);
    m_tempBuffer.resize(TEMPEST_MAX_OUTPUT_CHANNELS * sampleRate / 100);
    
    m_initialized = true;
    
    LOG_INFO("Tempest3D", "Initialized with sample rate %d Hz", sampleRate);
    return true;
}

void Tempest3DEngine::Shutdown() {
    m_audioObjects.clear();
    m_audioBeds.clear();
    m_hrtfTable.clear();
    m_reverbBuffer.clear();
    m_reflectionBuffer.clear();
    m_tempBuffer.clear();
    m_initialized = false;
    
    LOG_INFO("Tempest3D", "Shutdown complete");
}

void Tempest3DEngine::InitializeHRTFTable() {
    // Generate HRTF filters for all azimuth/elevation combinations
    // This is a simplified model - real HRTF uses measured data
    
    for (int azimuth = 0; azimuth < 360; azimuth += 10) {
        for (int elevation = -90; elevation <= 90; elevation += 10) {
            HRTFFilter filter;
            filter.azimuth = static_cast<float>(azimuth);
            filter.elevation = static_cast<float>(elevation);
            
            // Generate HRTF using simplified ITD/ILD model
            float azimuthRad = azimuth * 3.14159f / 180.0f;
            float elevationRad = elevation * 3.14159f / 180.0f;
            
            // Interaural Time Difference (ITD)
            float itd = 0.00065f * sinf(azimuthRad) * cosf(elevationRad);
            
            // Interaural Level Difference (ILD)
            float ild = 10.0f * sinf(azimuthRad) * cosf(elevationRad);
            
            // Generate frequency-dependent filters
            for (int i = 0; i < TEMPEST_HRTF_SIZE; i++) {
                float freq = static_cast<float>(i) * m_sampleRate / TEMPEST_HRTF_SIZE;
                
                // Phase shift for ITD
                float phase = -2.0f * 3.14159f * freq * itd;
                
                // Amplitude scaling for ILD
                float leftGain = 1.0f;
                float rightGain = powf(10.0f, -ild / 20.0f);
                
                if (azimuth > 180) {
                    std::swap(leftGain, rightGain);
                }
                
                // Apply head shadowing (simplified)
                float shadowFreq = 2000.0f + 3000.0f * (1.0f - cosf(elevationRad));
                if (freq > shadowFreq) {
                    rightGain *= shadowFreq / freq;
                }
                
                filter.left[i] = std::polar(leftGain, phase / 2.0f);
                filter.right[i] = std::polar(rightGain, -phase / 2.0f);
            }
            
            m_hrtfTable.push_back(filter);
        }
    }
    
    LOG_INFO("Tempest3D", "HRTF table initialized with %zu entries", m_hrtfTable.size());
}

const HRTFFilter& Tempest3DEngine::GetHRTFFilter(float azimuth, float elevation) {
    // Normalize azimuth to 0-360
    while (azimuth < 0) azimuth += 360.0f;
    while (azimuth >= 360) azimuth -= 360.0f;
    
    // Clamp elevation to -90 to 90
    elevation = std::max(-90.0f, std::min(90.0f, elevation));
    
    // Find nearest HRTF in table
    int azimuthIdx = static_cast<int>(azimuth / 10.0f) % 36;
    int elevationIdx = static_cast<int>((elevation + 90.0f) / 10.0f);
    elevationIdx = std::max(0, std::min(17, elevationIdx));
    
    int idx = azimuthIdx * 18 + elevationIdx;
    
    if (idx >= 0 && idx < static_cast<int>(m_hrtfTable.size())) {
        return m_hrtfTable[idx];
    }
    
    // Fallback to center
    static HRTFFilter defaultFilter;
    return defaultFilter;
}

uint32_t Tempest3DEngine::CreateAudioObject() {
    if (m_audioObjects.size() >= TEMPEST_MAX_OBJECTS) {
        LOG_ERROR("Tempest3D", "Maximum audio objects reached");
        return 0;
    }
    
    TempestAudioObject obj;
    obj.id = m_nextObjectId++;
    if (m_nextObjectId == 0) m_nextObjectId = 1; // Wrap around
    
    m_audioObjects.push_back(obj);
    return obj.id;
}

void Tempest3DEngine::DestroyAudioObject(uint32_t objectId) {
    auto it = std::find_if(m_audioObjects.begin(), m_audioObjects.end(),
                           [objectId](const TempestAudioObject& obj) {
                               return obj.id == objectId;
                           });
    
    if (it != m_audioObjects.end()) {
        m_audioObjects.erase(it);
    }
}

void Tempest3DEngine::UpdateAudioObject(uint32_t objectId, const TempestAudioObject& obj) {
    auto it = std::find_if(m_audioObjects.begin(), m_audioObjects.end(),
                           [objectId](const TempestAudioObject& o) {
                               return o.id == objectId;
                           });
    
    if (it != m_audioObjects.end()) {
        *it = obj;
        it->id = objectId; // Preserve ID
    }
}

TempestAudioObject* Tempest3DEngine::GetAudioObject(uint32_t objectId) {
    auto it = std::find_if(m_audioObjects.begin(), m_audioObjects.end(),
                           [objectId](const TempestAudioObject& obj) {
                               return obj.id == objectId;
                           });
    
    if (it != m_audioObjects.end()) {
        return &(*it);
    }
    return nullptr;
}

uint32_t Tempest3DEngine::CreateAudioBed(int32_t channelConfig) {
    if (m_audioBeds.size() >= TEMPEST_MAX_BEDS) {
        LOG_ERROR("Tempest3D", "Maximum audio beds reached");
        return 0;
    }
    
    TempestAudioBed bed;
    bed.id = m_nextBedId++;
    if (m_nextBedId == 0) m_nextBedId = 1;
    
    bed.channelConfig = channelConfig;
    
    // Initialize channel gains based on config
    for (int i = 0; i < TEMPEST_MAX_OUTPUT_CHANNELS; i++) {
        bed.channelGains[i] = 0.0f;
    }
    
    // Set default gains for common configs
    if (channelConfig == 2) { // Stereo
        bed.channelGains[0] = 1.0f; // L
        bed.channelGains[1] = 1.0f; // R
    } else if (channelConfig == 6) { // 5.1
        bed.channelGains[0] = 1.0f; // L
        bed.channelGains[1] = 1.0f; // R
        bed.channelGains[2] = 0.707f; // C
        bed.channelGains[3] = 0.707f; // LFE
        bed.channelGains[4] = 0.707f; // LS
        bed.channelGains[5] = 0.707f; // RS
    } else if (channelConfig == 8) { // 7.1
        bed.channelGains[0] = 1.0f; // L
        bed.channelGains[1] = 1.0f; // R
        bed.channelGains[2] = 0.707f; // C
        bed.channelGains[3] = 0.707f; // LFE
        bed.channelGains[4] = 0.707f; // LS
        bed.channelGains[5] = 0.707f; // RS
        bed.channelGains[6] = 0.707f; // LB
        bed.channelGains[7] = 0.707f; // RB
    } else if (channelConfig == 12) { // 7.1.4 Atmos
        // 7.1 base + 4 height channels
        for (int i = 0; i < 8; i++) {
            bed.channelGains[i] = (i < 2) ? 1.0f : 0.707f;
        }
        // Height channels
        bed.channelGains[8] = 0.707f;  // L height
        bed.channelGains[9] = 0.707f;  // R height
        bed.channelGains[10] = 0.707f; // LB height
        bed.channelGains[11] = 0.707f; // RB height
    }
    
    m_audioBeds.push_back(bed);
    return bed.id;
}

void Tempest3DEngine::DestroyAudioBed(uint32_t bedId) {
    auto it = std::find_if(m_audioBeds.begin(), m_audioBeds.end(),
                           [bedId](const TempestAudioBed& bed) {
                               return bed.id == bedId;
                           });
    
    if (it != m_audioBeds.end()) {
        m_audioBeds.erase(it);
    }
}

void Tempest3DEngine::UpdateAudioBed(uint32_t bedId, const TempestAudioBed& bed) {
    auto it = std::find_if(m_audioBeds.begin(), m_audioBeds.end(),
                           [bedId](const TempestAudioBed& b) {
                               return b.id == bedId;
                           });
    
    if (it != m_audioBeds.end()) {
        *it = bed;
        it->id = bedId; // Preserve ID
    }
}

TempestAudioBed* Tempest3DEngine::GetAudioBed(uint32_t bedId) {
    auto it = std::find_if(m_audioBeds.begin(), m_audioBeds.end(),
                           [bedId](const TempestAudioBed& bed) {
                               return bed.id == bedId;
                           });
    
    if (it != m_audioBeds.end()) {
        return &(*it);
    }
    return nullptr;
}

void Tempest3DEngine::SetListenerPosition(float x, float y, float z) {
    m_listenerX = x;
    m_listenerY = y;
    m_listenerZ = z;
}

void Tempest3DEngine::SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
                                              float upX, float upY, float upZ) {
    m_listenerForwardX = forwardX;
    m_listenerForwardY = forwardY;
    m_listenerForwardZ = forwardZ;
    m_listenerUpX = upX;
    m_listenerUpY = upY;
    m_listenerUpZ = upZ;
}

void Tempest3DEngine::SetListenerVelocity(float vx, float vy, float vz) {
    m_listenerVelocityX = vx;
    m_listenerVelocityY = vy;
    m_listenerVelocityZ = vz;
}

void Tempest3DEngine::SetRoomParams(const TempestRoomParams& params) {
    m_roomParams = params;
}

void Tempest3DEngine::CalculateDistanceGain(TempestAudioObject& obj) {
    float dx = obj.x - m_listenerX;
    float dy = obj.y - m_listenerY;
    float dz = obj.z - m_listenerZ;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    
    // Inverse square law with minimum distance clamp
    float minDistance = 1.0f;
    float maxDistance = 1000.0f;
    
    distance = std::max(minDistance, std::min(maxDistance, distance));
    obj.distanceGain = minDistance / distance;
}

void Tempest3DEngine::CalculateObstruction(TempestAudioObject& obj) {
    // Simplified obstruction model
    // In a real implementation, this would raycast against geometry
    obj.obstruction = std::max(0.0f, std::min(1.0f, obj.obstruction));
}

void Tempest3DEngine::ApplyHRTF(float* inputBuffer, float* outputBuffer, int32_t numSamples, 
                                 const TempestAudioObject& obj) {
    // Calculate relative position to listener
    float dx = obj.x - m_listenerX;
    float dy = obj.y - m_listenerY;
    float dz = obj.z - m_listenerZ;
    
    // Convert to spherical coordinates
    float azimuth = atan2f(dx, dz) * 180.0f / 3.14159f;
    float elevation = asinf(dy / sqrtf(dx * dx + dy * dy + dz * dz)) * 180.0f / 3.14159f;
    
    // Get HRTF filter
    const HRTFFilter& hrtf = GetHRTFFilter(azimuth, elevation);
    
    // Apply HRTF (simplified frequency domain processing)
    // In production, this would use FFT for efficiency
    float gain = obj.gain * obj.distanceGain * (1.0f - obj.obstruction * 0.5f);
    
    for (int i = 0; i < numSamples; i++) {
        float sample = inputBuffer[i] * gain;
        outputBuffer[i * 2] = sample * 1.0f;     // Left
        outputBuffer[i * 2 + 1] = sample * 0.9f; // Right (simplified ILD)
    }
}

void Tempest3DEngine::ApplyReverb(float* buffer, int32_t numSamples) {
    // Simplified reverb implementation
    float wetLevel = m_roomParams.wetLevel;
    float dryLevel = m_roomParams.dryLevel;
    float reverbTime = m_roomParams.reverbTime;
    
    // Decay factor
    float decay = expf(-1.0f / (reverbTime * m_sampleRate));
    
    // Apply reverb using delay line
    for (int i = 0; i < numSamples; i++) {
        int delayIdx = (i - static_cast<int>(m_roomParams.reflectionDelay * m_sampleRate) + 
                        m_reverbBuffer.size()) % m_reverbBuffer.size();
        
        float delayed = m_reverbBuffer[delayIdx];
        m_reverbBuffer[i % m_reverbBuffer.size()] = buffer[i] + delayed * decay;
        
        buffer[i] = buffer[i] * dryLevel + delayed * wetLevel;
    }
}

void Tempest3DEngine::ApplyReflections(float* buffer, int32_t numSamples, 
                                        const TempestAudioObject& obj) {
    // Simplified early reflections
    float reflectionGain = obj.reflectionGain * m_roomParams.dryLevel;
    int delaySamples = static_cast<int>(m_roomParams.reflectionDelay * m_sampleRate);
    
    for (int i = delaySamples; i < numSamples; i++) {
        buffer[i] += buffer[i - delaySamples] * reflectionGain;
    }
}

void Tempest3DEngine::MixObjects(float* outputBuffer, int32_t numSamples) {
    // Clear output buffer
    memset(outputBuffer, 0, sizeof(float) * numSamples * 2);
    
    // Mix all active audio objects
    for (auto& obj : m_audioObjects) {
        if (!obj.enabled) continue;
        
        // Calculate distance gain
        CalculateDistanceGain(obj);
        CalculateObstruction(obj);
        
        // Create temporary buffer for this object
        std::vector<float> tempBuffer(numSamples);
        memset(tempBuffer.data(), 0, sizeof(float) * numSamples);
        
        // Apply HRTF and mix into output
        ApplyHRTF(tempBuffer.data(), outputBuffer, numSamples, obj);
    }
}

void Tempest3DEngine::ApplyBedMixing(float* outputBuffer, int32_t numSamples) {
    // Mix all active audio beds
    for (auto& bed : m_audioBeds) {
        if (!bed.enabled) continue;
        
        // Apply bed channel gains to output
        for (int ch = 0; ch < std::min(bed.channelConfig, TEMPEST_MAX_OUTPUT_CHANNELS); ch++) {
            float gain = bed.channelGains[ch] * bed.gain;
            if (gain > 0.0f) {
                for (int i = 0; i < numSamples; i++) {
                    outputBuffer[i * TEMPEST_MAX_OUTPUT_CHANNELS + ch] *= gain;
                }
            }
        }
    }
}

void Tempest3DEngine::ProcessAudio(float* inputBuffer, float* outputBuffer, 
                                    int32_t numSamples, int32_t numChannels) {
    if (!m_initialized || !inputBuffer || !outputBuffer) {
        return;
    }
    
    // Mix audio objects with HRTF
    MixObjects(outputBuffer, numSamples);
    
    // Apply bed mixing
    if (numChannels > 2) {
        ApplyBedMixing(outputBuffer, numSamples);
    }
    
    // Apply room acoustics
    ApplyReverb(outputBuffer, numSamples);
    
    // Apply early reflections for each object
    for (const auto& obj : m_audioObjects) {
        if (obj.enabled) {
            ApplyReflections(outputBuffer, numSamples, obj);
        }
    }
}

int32_t Tempest3DEngine::GetActiveObjectCount() const {
    return static_cast<int32_t>(std::count_if(m_audioObjects.begin(), m_audioObjects.end(),
                                               [](const TempestAudioObject& obj) {
                                                   return obj.enabled;
                                               }));
}

int32_t Tempest3DEngine::GetActiveBedCount() const {
    return static_cast<int32_t>(std::count_if(m_audioBeds.begin(), m_audioBeds.end(),
                                               [](const TempestAudioBed& bed) {
                                                   return bed.enabled;
                                               }));
}

} // namespace Kyty::Libs
