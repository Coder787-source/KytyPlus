#include "textureStreaming.h"
#include "common/logging/log.h"
#include <algorithm>
#include <cmath>

namespace Kyty::Libs {

using namespace Kyty::Common;

//=============================================================================
// TextureStreaming Implementation
//=============================================================================

TextureStreaming& TextureStreaming::Instance() {
    static TextureStreaming instance;
    return instance;
}

bool TextureStreaming::Initialize() {
    if (m_initialized) {
        Log::Warning("[TexStream] Already initialized");
        return true;
    }
    
    Log::Info("[TexStream] Initializing texture streaming system...");
    
    // Initialize with default budget
    m_budget = StreamingBudget();
    
    // Initialize GTA 3 DE specific fixes
    InitializeTextureStreamingFixes();
    
    m_initialized = true;
    
    Log::Info("[TexStream] Texture streaming system initialized");
    return true;
}

void TextureStreaming::Shutdown() {
    Log::Info("[TexStream] Shutting down texture streaming system...");
    
    m_textures.clear();
    m_streamQueue.clear();
    m_nextTextureId = 1;
    m_currentFrame = 0;
    m_currentMemoryMB = 0.0f;
    m_initialized = false;
    
    Log::Info("[TexStream] Texture streaming system shut down");
}

uint32_t TextureStreaming::RegisterTexture(const std::string& name, const std::string& path,
                                            uint32_t width, uint32_t height,
                                            ETexturePriority priority) {
    auto texture = std::make_unique<TextureDesc>();
    texture->textureId = m_nextTextureId++;
    texture->name = name;
    texture->path = path;
    texture->width = width;
    texture->height = height;
    texture->priority = priority;
    texture->state = ETextureStreamState::NotLoaded;
    texture->currentLOD = ELODLevel::LOD_0;
    
    // Calculate approximate memory size (RGBA8, with mips)
    texture->memorySize = width * height * 4; // Base level
    for (uint32_t i = 1; i < texture->mipLevels; i++) {
        texture->memorySize += (width >> i) * (height >> i) * 4;
    }
    
    uint32_t textureId = texture->textureId;
    m_textures[textureId] = std::move(texture);
    
    Log::Debug("[TexStream] Registered texture %u: %s (%ux%u, priority=%d)",
               textureId, name.c_str(), width, height, static_cast<int>(priority));
    
    return textureId;
}

void TextureStreaming::UnregisterTexture(uint32_t textureId) {
    auto it = m_textures.find(textureId);
    if (it != m_textures.end()) {
        m_currentMemoryMB -= it->second->memorySize / (1024.0f * 1024.0f);
        m_textures.erase(it);
        Log::Debug("[TexStream] Unregistered texture %u", textureId);
    }
}

bool TextureStreaming::RequestLoad(uint32_t textureId, ELODLevel lodLevel) {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return false;
    }
    
    auto& texture = it->second;
    
    if (texture->state == ETextureStreamState::Loaded ||
        texture->state == ETextureStreamState::Resident) {
        return true; // Already loaded
    }
    
    if (lodLevel == ELODLevel::LOD_Auto) {
        // Auto-select LOD based on distance
        if (texture->distanceFromCamera > 100.0f) {
            lodLevel = ELODLevel::LOD_3;
        } else if (texture->distanceFromCamera > 50.0f) {
            lodLevel = ELODLevel::LOD_2;
        } else if (texture->distanceFromCamera > 20.0f) {
            lodLevel = ELODLevel::LOD_1;
        } else {
            lodLevel = ELODLevel::LOD_0;
        }
    }
    
    texture->currentLOD = lodLevel;
    texture->state = ETextureStreamState::Queued;
    
    // Add to stream queue
    m_streamQueue.push_back(textureId);
    
    Log::Debug("[TexStream] Requested load for texture %u (LOD=%d)", textureId, static_cast<int>(lodLevel));
    return true;
}

void TextureStreaming::RequestUnload(uint32_t textureId, bool immediate) {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return;
    }
    
    auto& texture = it->second;
    
    if (immediate) {
        texture->state = ETextureStreamState::NotLoaded;
        m_currentMemoryMB -= texture->memorySize / (1024.0f * 1024.0f);
        Log::Debug("[TexStream] Immediately unloaded texture %u", textureId);
    } else {
        // Mark for later unload
        texture->state = ETextureStreamState::Loaded; // Will be evicted if needed
    }
}

void TextureStreaming::SetPriority(uint32_t textureId, ETexturePriority priority) {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return;
    }
    
    it->second->priority = priority;
    
    // Re-sort stream queue based on priority
    std::sort(m_streamQueue.begin(), m_streamQueue.end(),
              [this](uint32_t a, uint32_t b) {
                  auto itA = m_textures.find(a);
                  auto itB = m_textures.find(b);
                  if (itA == m_textures.end() || itB == m_textures.end()) return false;
                  return static_cast<int>(itA->second->priority) > static_cast<int>(itB->second->priority);
              });
}

void TextureStreaming::UpdateTextureDistance(uint32_t textureId, float distance) {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return;
    }
    
    it->second->distanceFromCamera = distance;
}

void TextureStreaming::Update(float deltaTime, uint32_t currentFrame) {
    if (!m_initialized) {
        return;
    }
    
    m_currentFrame = currentFrame;
    
    // Process streaming queue
    ProcessStreamQueue();
    
    // Update LODs based on distance
    if (m_budget.enableDynamicLOD) {
        UpdateLODs();
    }
    
    // Preload textures for upcoming areas
    if (m_budget.enablePreloading) {
        PreloadTextures();
    }
    
    // Evict textures if over budget
    if (m_currentMemoryMB > m_budget.targetMemoryMB) {
        EvictTextures();
    }
}

void TextureStreaming::SetResident(uint32_t textureId, bool resident) {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return;
    }
    
    auto& texture = it->second;
    texture->isResident = resident;
    
    if (resident) {
        texture->state = ETextureStreamState::Resident;
        texture->priority = ETexturePriority::Critical;
    } else if (texture->state == ETextureStreamState::Resident) {
        texture->state = ETextureStreamState::Loaded;
    }
}

bool TextureStreaming::IsTextureLoaded(uint32_t textureId) const {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return false;
    }
    
    auto state = it->second->state;
    return state == ETextureStreamState::Loaded ||
           state == ETextureStreamState::Resident;
}

bool TextureStreaming::IsTextureStreaming(uint32_t textureId) const {
    auto it = m_textures.find(textureId);
    if (it == m_textures.end()) {
        return false;
    }
    
    return it->second->isStreaming;
}

float TextureStreaming::GetMemoryUsageMB() const {
    return m_currentMemoryMB;
}

const StreamingBudget& TextureStreaming::GetBudget() const {
    return m_budget;
}

void TextureStreaming::SetBudget(const StreamingBudget& budget) {
    m_budget = budget;
    Log::Info("[TexStream] Streaming budget updated: max=%zuMB, target=%zuMB",
              budget.maxMemoryMB, budget.targetMemoryMB);
}

void TextureStreaming::SetFixEnabled(bool enabled) {
    if (m_fixEnabled != enabled) {
        m_fixEnabled = enabled;
        Log::Info("[TexStream] Streaming fixes %s", enabled ? "enabled" : "disabled");
    }
}

bool TextureStreaming::IsFixEnabled() const {
    return m_fixEnabled;
}

size_t TextureStreaming::GetTextureCount() const {
    return m_textures.size();
}

size_t TextureStreaming::GetStreamingCount() const {
    size_t count = 0;
    for (const auto& [id, texture] : m_textures) {
        if (texture->isStreaming) {
            count++;
        }
    }
    return count;
}

void TextureStreaming::ClearCache(bool keepResident) {
    Log::Info("[TexStream] Clearing texture cache (keepResident=%d)", keepResident);
    
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (keepResident && it->second->isResident) {
            ++it;
        } else {
            m_currentMemoryMB -= it->second->memorySize / (1024.0f * 1024.0f);
            it = m_textures.erase(it);
        }
    }
    
    m_streamQueue.clear();
}

void TextureStreaming::PreloadArea(const std::string& areaId, ETexturePriority priority) {
    Log::Info("[TexStream] Preloading area: %s (priority=%d)", areaId.c_str(), static_cast<int>(priority));
    
    // In production, this would load textures specific to the area
    // For now, just increase priority of nearby textures
    for (auto& [id, texture] : m_textures) {
        if (texture->distanceFromCamera < 100.0f) {
            texture->priority = priority;
            RequestLoad(id, ELODLevel::LOD_0);
        }
    }
}

void TextureStreaming::ProcessStreamQueue() {
    if (m_streamQueue.empty()) {
        return;
    }
    
    // Process up to maxConcurrentStreams textures per frame
    uint32_t processed = 0;
    auto it = m_streamQueue.begin();
    
    while (it != m_streamQueue.end() && processed < m_budget.maxConcurrentStreams) {
        uint32_t textureId = *it;
        auto textureIt = m_textures.find(textureId);
        
        if (textureIt != m_textures.end()) {
            auto& texture = textureIt->second;
            
            if (texture->state == ETextureStreamState::Queued) {
                texture->isStreaming = true;
                texture->state = ETextureStreamState::Streaming;
                
                // Simulate streaming completion (would be async in production)
                texture->state = ETextureStreamState::Loaded;
                texture->isStreaming = false;
                texture->frameSinceLoad = m_currentFrame;
                
                m_currentMemoryMB += texture->memorySize / (1024.0f * 1024.0f);
                
                Log::Debug("[TexStream] Loaded texture %u (%.2f MB)",
                          textureId, texture->memorySize / (1024.0f * 1024.0f));
                
                processed++;
            }
        }
        
        it = m_streamQueue.erase(it);
    }
}

void TextureStreaming::UpdateLODs() {
    for (auto& [id, texture] : m_textures) {
        if (texture->isResident) continue;
        
        ELODLevel newLOD = texture->currentLOD;
        
        if (texture->distanceFromCamera > 100.0f) {
            newLOD = ELODLevel::LOD_3;
        } else if (texture->distanceFromCamera > 50.0f) {
            newLOD = ELODLevel::LOD_2;
        } else if (texture->distanceFromCamera > 20.0f) {
            newLOD = ELODLevel::LOD_1;
        } else {
            newLOD = ELODLevel::LOD_0;
        }
        
        if (newLOD != texture->currentLOD) {
            texture->currentLOD = newLOD;
            Log::Debug("[TexStream] Updated LOD for texture %u to %d",
                      id, static_cast<int>(newLOD));
        }
    }
}

void TextureStreaming::EvictTextures() {
    Log::Debug("[TexStream] Evicting textures (current=%.2f MB, target=%zu MB)",
              m_currentMemoryMB, m_budget.targetMemoryMB);
    
    // Sort textures by priority and last access time
    std::vector<std::pair<uint32_t, float>> evictionCandidates;
    
    for (const auto& [id, texture] : m_textures) {
        if (texture->isResident || texture->isStreaming) continue;
        
        float evictionScore = static_cast<float>(texture->priority);
        evictionScore += (m_currentFrame - texture->frameSinceLoad) * 0.01f;
        
        evictionCandidates.emplace_back(id, evictionScore);
    }
    
    // Sort by eviction score (lower = evict first)
    std::sort(evictionCandidates.begin(), evictionCandidates.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Evict until under budget
    for (const auto& [id, score] : evictionCandidates) {
        if (m_currentMemoryMB <= m_budget.targetMemoryMB) break;
        
        auto it = m_textures.find(id);
        if (it != m_textures.end()) {
            m_currentMemoryMB -= it->second->memorySize / (1024.0f * 1024.0f);
            it->second->state = ETextureStreamState::NotLoaded;
            
            Log::Debug("[TexStream] Evicted texture %u", id);
        }
    }
}

void TextureStreaming::PreloadTextures() {
    // Preload textures that will be needed soon
    for (auto& [id, texture] : m_textures) {
        if (texture->state == ETextureStreamState::NotLoaded &&
            texture->distanceFromCamera < m_budget.lodTransitionDistance * 2.0f) {
            RequestLoad(id, ELODLevel::LOD_Auto);
        }
    }
}

//=============================================================================
// Texture Streaming Fixes Initialization
//=============================================================================

void InitializeTextureStreamingFixes() {
    Log::Info("[TexStream] Initializing texture streaming fixes...");
    
    ApplyPopInReductionFix();
    OptimizeForNVMe();
    
    Log::Info("[TexStream] Texture streaming fixes initialized");
}

void ApplyPopInReductionFix() {
    auto& stream = TextureStreaming::Instance();
    
    // Reduce LOD transition distance to minimize pop-in
    StreamingBudget budget = stream.GetBudget();
    budget.lodTransitionDistance = 30.0f; // Reduced from 50.0f
    budget.preloadFrameCount = 5; // Increased from 3
    stream.SetBudget(budget);
    
    Log::Info("[TexStream] Applied pop-in reduction fix");
}

void OptimizeForNVMe() {
    auto& stream = TextureStreaming::Instance();
    
    // Optimize for PS5 NVMe speeds
    StreamingBudget budget = stream.GetBudget();
    budget.streamBandwidthMBps = 500.0f; // PS5 NVMe speed
    budget.maxConcurrentStreams = 16; // Increased for parallel loading
    budget.enableCompression = true;
    stream.SetBudget(budget);
    
    Log::Info("[TexStream] Optimized for NVMe streaming");
}

} // namespace Kyty::Libs
