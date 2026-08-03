#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Kyty::Libs {

/**
 * Texture Streaming Optimization Module
 * 
 * This module provides texture streaming fixes to eliminate texture pop-in
 * and improve asset loading performance in GTA 3 DE and other games.
 * 
 * Covers: Texture priority management, streaming budget control,
 *         NVMe integration, and LOD management.
 */

// Texture priority levels
enum class ETexturePriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// Texture streaming states
enum class ETextureStreamState : uint8_t {
    NotLoaded = 0,
    Queued = 1,
    Streaming = 2,
    Loaded = 3,
    Resident = 4,
    Error = 5
};

// LOD levels
enum class ELODLevel : uint8_t {
    LOD_0 = 0,  // Highest quality
    LOD_1 = 1,
    LOD_2 = 2,
    LOD_3 = 3,  // Lowest quality
    LOD_Auto = 255
};

/**
 * Texture descriptor
 */
struct TextureDesc {
    uint32_t textureId;
    std::string name;
    std::string path;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    ETexturePriority priority;
    ETextureStreamState state;
    ELODLevel currentLOD;
    size_t memorySize;
    uint64_t lastAccessTime;
    uint32_t frameSinceLoad;
    float distanceFromCamera;
    bool isStreaming;
    bool isResident;
    
    TextureDesc() : textureId(0), width(0), height(0), mipLevels(1),
                    priority(ETexturePriority::Normal), state(ETextureStreamState::NotLoaded),
                    currentLOD(ELODLevel::LOD_0), memorySize(0), lastAccessTime(0),
                    frameSinceLoad(0), distanceFromCamera(0.0f),
                    isStreaming(false), isResident(false) {}
};

/**
 * Streaming budget configuration
 */
struct StreamingBudget {
    size_t maxMemoryMB;
    size_t targetMemoryMB;
    size_t minMemoryMB;
    float streamBandwidthMBps;
    uint32_t maxConcurrentStreams;
    uint32_t preloadFrameCount;
    float lodTransitionDistance;
    bool enableDynamicLOD;
    bool enablePreloading;
    bool enableCompression;
    
    StreamingBudget() : maxMemoryMB(2048), targetMemoryMB(1536), minMemoryMB(512),
                        streamBandwidthMBps(100.0f), maxConcurrentStreams(8),
                        preloadFrameCount(3), lodTransitionDistance(50.0f),
                        enableDynamicLOD(true), enablePreloading(true),
                        enableCompression(true) {}
};

/**
 * Texture Streaming System
 */
class TextureStreaming {
public:
    static TextureStreaming& Instance();
    
    /**
     * Initialize texture streaming system
     * @return true if initialization succeeded
     */
    bool Initialize();
    
    /**
     * Shutdown texture streaming system
     */
    void Shutdown();
    
    /**
     * Register a texture for streaming
     * @param name Texture name
     * @param path Texture file path
     * @param width Texture width
     * @param height Texture height
     * @param priority Texture priority
     * @return Texture ID, or 0 if failed
     */
    uint32_t RegisterTexture(const std::string& name, const std::string& path,
                             uint32_t width, uint32_t height,
                             ETexturePriority priority = ETexturePriority::Normal);
    
    /**
     * Unregister a texture
     * @param textureId ID of the texture
     */
    void UnregisterTexture(uint32_t textureId);
    
    /**
     * Request texture load
     * @param textureId ID of the texture
     * @param lodLevel LOD level to load
     * @return true if load request was queued
     */
    bool RequestLoad(uint32_t textureId, ELODLevel lodLevel = ELODLevel::LOD_Auto);
    
    /**
     * Request texture unload
     * @param textureId ID of the texture
     * @param immediate If true, unload immediately
     */
    void RequestUnload(uint32_t textureId, bool immediate = false);
    
    /**
     * Set texture priority
     * @param textureId ID of the texture
     * @param priority New priority
     */
    void SetPriority(uint32_t textureId, ETexturePriority priority);
    
    /**
     * Update texture distance from camera
     * @param textureId ID of the texture
     * @param distance Distance in meters
     */
    void UpdateTextureDistance(uint32_t textureId, float distance);
    
    /**
     * Update all textures (called per frame)
     * @param deltaTime Time since last update
     * @param currentFrame Current frame number
     */
    void Update(float deltaTime, uint32_t currentFrame);
    
    /**
     * Force texture to be resident in memory
     * @param textureId ID of the texture
     * @param resident true to make resident
     */
    void SetResident(uint32_t textureId, bool resident);
    
    /**
     * Check if texture is loaded
     * @param textureId ID of the texture
     * @return true if loaded
     */
    bool IsTextureLoaded(uint32_t textureId) const;
    
    /**
     * Check if texture is streaming
     * @param textureId ID of the texture
     * @return true if streaming
     */
    bool IsTextureStreaming(uint32_t textureId) const;
    
    /**
     * Get texture memory usage
     * @return Current memory usage in MB
     */
    float GetMemoryUsageMB() const;
    
    /**
     * Get streaming budget
     * @return Current streaming budget
     */
    const StreamingBudget& GetBudget() const;
    
    /**
     * Set streaming budget
     * @param budget New streaming budget
     */
    void SetBudget(const StreamingBudget& budget);
    
    /**
     * Enable/disable streaming fixes
     * @param enabled Enable/disable
     */
    void SetFixEnabled(bool enabled);
    
    /**
     * Check if fixes are enabled
     * @return true if fixes are enabled
     */
    bool IsFixEnabled() const;
    
    /**
     * Get the number of registered textures
     * @return Number of textures
     */
    size_t GetTextureCount() const;
    
    /**
     * Get the number of textures currently streaming
     * @return Number of streaming textures
     */
    size_t GetStreamingCount() const;
    
    /**
     * Clear texture cache
     * @param keepResident If true, keep resident textures
     */
    void ClearCache(bool keepResident = true);
    
    /**
     * Preload textures for upcoming area
     * @param areaId Area identifier
     * @param priority Priority for preload
     */
    void PreloadArea(const std::string& areaId, ETexturePriority priority = ETexturePriority::High);
    
private:
    TextureStreaming() = default;
    ~TextureStreaming() = default;
    
    TextureStreaming(const TextureStreaming&) = delete;
    TextureStreaming& operator=(const TextureStreaming&) = delete;
    
    std::unordered_map<uint32_t, std::unique_ptr<TextureDesc>> m_textures;
    std::vector<uint32_t> m_streamQueue;
    StreamingBudget m_budget;
    uint32_t m_nextTextureId = 1;
    uint32_t m_currentFrame = 0;
    bool m_fixEnabled = true;
    bool m_initialized = false;
    float m_currentMemoryMB = 0.0f;
    
    void ProcessStreamQueue();
    void UpdateLODs();
    void EvictTextures();
    void PreloadTextures();
};

/**
 * Initialize texture streaming fixes for GTA 3 DE
 */
void InitializeTextureStreamingFixes();

/**
 * Apply pop-in reduction fix
 */
void ApplyPopInReductionFix();

/**
 * Optimize streaming for NVMe
 */
void OptimizeForNVMe();

} // namespace Kyty::Libs
