#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Kyty::Libs {

/**
 * Tempest Audio Crackling Fix Module
 * 
 * This module provides audio buffer management and timing fixes to eliminate
 * audio crackling, popping, and stuttering in games using Tempest 3D audio.
 * 
 * Covers: Buffer underrun prevention, sample rate conversion, timing synchronization,
 *         and audio stream management.
 */

// Audio buffer states
enum class EAudioBufferState : uint8_t {
    Empty = 0,
    Buffering = 1,
    Ready = 2,
    Playing = 3,
    Error = 4
};

// Audio quality levels
enum class EAudioQuality : uint8_t {
    Low = 0,      // 22050 Hz
    Medium = 1,   // 44100 Hz
    High = 2,     // 48000 Hz
    Tempest = 3   // 96000 Hz (Tempest 3D)
};

/**
 * Audio buffer descriptor
 */
struct AudioBufferDesc {
    uint32_t bufferId;
    EAudioBufferState state;
    EAudioQuality quality;
    size_t sampleCount;
    size_t channelCount;
    float sampleRate;
    size_t readPosition;
    size_t writePosition;
    size_t bufferSize;
    uint64_t lastUpdateTime;
    
    AudioBufferDesc() : bufferId(0), state(EAudioBufferState::Empty),
                        quality(EAudioQuality::Medium), sampleCount(0), channelCount(2),
                        sampleRate(44100.0f), readPosition(0), writePosition(0),
                        bufferSize(0), lastUpdateTime(0) {}
};

/**
 * Audio stream descriptor
 */
struct AudioStreamDesc {
    uint32_t streamId;
    std::string name;
    bool is3D;
    bool isLooping;
    float volume;
    float pitch;
    float pan;
    uint32_t bufferCount;
    uint32_t activeBuffer;
    bool isPlaying;
    bool isPaused;
    uint64_t startTime;
    uint64_t pausedTime;
    
    AudioStreamDesc() : streamId(0), is3D(false), isLooping(false),
                        volume(1.0f), pitch(1.0f), pan(0.0f),
                        bufferCount(0), activeBuffer(0), isPlaying(false),
                        isPaused(false), startTime(0), pausedTime(0) {}
};

/**
 * Tempest Audio Fix System
 */
class TempestAudioFix {
public:
    static TempestAudioFix& Instance();
    
    /**
     * Initialize audio fix system
     * @return true if initialization succeeded
     */
    bool Initialize();
    
    /**
     * Shutdown audio fix system
     */
    void Shutdown();
    
    /**
     * Create an audio buffer
     * @param sampleCount Number of samples
     * @param channelCount Number of channels
     * @param sampleRate Sample rate
     * @return Buffer ID, or 0 if failed
     */
    uint32_t CreateBuffer(size_t sampleCount, size_t channelCount, float sampleRate);
    
    /**
     * Destroy an audio buffer
     * @param bufferId ID of the buffer to destroy
     */
    void DestroyBuffer(uint32_t bufferId);
    
    /**
     * Write data to audio buffer
     * @param bufferId ID of the buffer
     * @param data Audio data
     * @param sampleCount Number of samples to write
     * @return Number of samples written
     */
    size_t WriteToBuffer(uint32_t bufferId, const float* data, size_t sampleCount);
    
    /**
     * Read data from audio buffer
     * @param bufferId ID of the buffer
     * @param outData Output buffer
     * @param sampleCount Number of samples to read
     * @return Number of samples read
     */
    size_t ReadFromBuffer(uint32_t bufferId, float* outData, size_t sampleCount);
    
    /**
     * Create an audio stream
     * @param name Stream name
     * @param is3D Is 3D audio stream
     * @param isLooping Is looping stream
     * @return Stream ID, or 0 if failed
     */
    uint32_t CreateStream(const std::string& name, bool is3D, bool isLooping);
    
    /**
     * Destroy an audio stream
     * @param streamId ID of the stream to destroy
     */
    void DestroyStream(uint32_t streamId);
    
    /**
     * Play an audio stream
     * @param streamId ID of the stream
     * @return true if playback started
     */
    bool PlayStream(uint32_t streamId);
    
    /**
     * Pause an audio stream
     * @param streamId ID of the stream
     */
    void PauseStream(uint32_t streamId);
    
    /**
     * Resume an audio stream
     * @param streamId ID of the stream
     */
    void ResumeStream(uint32_t streamId);
    
    /**
     * Stop an audio stream
     * @param streamId ID of the stream
     */
    void StopStream(uint32_t streamId);
    
    /**
     * Set stream volume
     * @param streamId ID of the stream
     * @param volume Volume (0.0 - 1.0)
     */
    void SetStreamVolume(uint32_t streamId, float volume);
    
    /**
     * Set stream pitch
     * @param streamId ID of the stream
     * @param pitch Pitch (0.5 - 2.0)
     */
    void SetStreamPitch(uint32_t streamId, float pitch);
    
    /**
     * Set stream 3D position
     * @param streamId ID of the stream
     * @param x X position
     * @param y Y position
     * @param z Z position
     */
    void SetStream3DPosition(uint32_t streamId, float x, float y, float z);
    
    /**
     * Update audio system
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);
    
    /**
     * Get available samples in buffer
     * @param bufferId ID of the buffer
     * @return Number of available samples
     */
    size_t GetAvailableSamples(uint32_t bufferId) const;
    
    /**
     * Get buffer state
     * @param bufferId ID of the buffer
     * @return Buffer state
     */
    EAudioBufferState GetBufferState(uint32_t bufferId) const;
    
    /**
     * Check if stream is playing
     * @param streamId ID of the stream
     * @return true if playing
     */
    bool IsStreamPlaying(uint32_t streamId) const;
    
    /**
     * Enable/disable crackling fixes
     * @param enabled Enable/disable
     */
    void SetFixEnabled(bool enabled);
    
    /**
     * Check if fixes are enabled
     * @return true if fixes are enabled
     */
    bool IsFixEnabled() const;
    
    /**
     * Set audio quality
     * @param quality Audio quality level
     */
    void SetAudioQuality(EAudioQuality quality);
    
    /**
     * Get current audio quality
     * @return Current audio quality
     */
    EAudioQuality GetAudioQuality() const;
    
    /**
     * Get the number of active buffers
     * @return Number of buffers
     */
    size_t GetBufferCount() const;
    
    /**
     * Get the number of active streams
     * @return Number of streams
     */
    size_t GetStreamCount() const;
    
private:
    TempestAudioFix() = default;
    ~TempestAudioFix() = default;
    
    TempestAudioFix(const TempestAudioFix&) = delete;
    TempestAudioFix& operator=(const TempestAudioFix&) = delete;
    
    std::unordered_map<uint32_t, std::unique_ptr<AudioBufferDesc>> m_buffers;
    std::unordered_map<uint32_t, std::unique_ptr<AudioStreamDesc>> m_streams;
    std::vector<float> m_tempBuffer; // For mixing
    uint32_t m_nextBufferId = 1;
    uint32_t m_nextStreamId = 1;
    bool m_fixEnabled = true;
    bool m_initialized = false;
    EAudioQuality m_audioQuality = EAudioQuality::High;
    
    void PreventBufferUnderrun(uint32_t bufferId);
    void SynchronizeBuffers(uint32_t streamId);
};

/**
 * Initialize Tempest audio fixes for GTA 3 DE
 */
void InitializeTempestAudioFixes();

/**
 * Apply crackling fix for specific audio issue
 * @param issueType Type of audio issue
 */
void ApplyAudioCracklingFix(int issueType);

} // namespace Kyty::Libs
