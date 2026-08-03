#pragma once

#include "common/types.h"
#include <string>
#include <vector>
#include <array>

namespace Kyty::Libs {

// GTA 3 Definitive Edition Save Data Handler
// Supports binary save format used by UE4 GTA 3 DE

constexpr int32_t GTA3_MAX_SAVE_SLOTS = 10;
constexpr int32_t GTA3_SAVE_VERSION = 3; // Definitive Edition version
constexpr size_t GTA3_SAVE_SIZE = 262144; // 256KB per save slot
constexpr size_t GTA3_SAVE_HEADER_SIZE = 256;

// Save file header structure
struct GTA3SaveHeader {
    uint32_t magic = 0x47544133; // "GTA3"
    uint32_t version = GTA3_SAVE_VERSION;
    uint32_t slotId = 0;
    uint32_t checksum = 0;
    char saveName[32];
    char locationName[32];
    uint32_t playTimeSeconds = 0;
    uint32_t missionId = 0;
    uint32_t lastModified = 0; // Unix timestamp
    uint32_t reserved[6] = {0};
};

// Player state in save
struct GTA3PlayerState {
    float position[3]; // x, y, z
    float rotation[3]; // pitch, yaw, roll
    uint32_t health = 100;
    uint32_t armor = 0;
    uint32_t money = 0;
    uint32_t wantedLevel = 0;
    uint32_t currentWeapon = 0;
    uint32_t weaponsOwned = 0; // Bitmask
    uint32_t maxHealth = 100;
    uint32_t maxArmor = 100;
    float stamina = 100.0f;
    float breath = 100.0f;
    uint32_t respect = 0; // For SA compatibility
    uint8_t reserved[48];
};

// Vehicle state
struct GTA3VehicleState {
    uint32_t modelId = 0;
    float position[3];
    float rotation[3];
    uint32_t color[2];
    uint32_t health = 1000;
    uint32_t flags = 0;
    uint8_t reserved[32];
};

// Mission progress
struct GTA3MissionState {
    uint32_t currentMission = 0;
    uint32_t completedMissions = 0; // Bitmask for first 32
    uint32_t completedMissions2 = 0; // Bitmask for next 32
    uint32_t failedMissions = 0;
    uint32_t lastMissionResult = 0; // 0=none, 1=passed, 2=failed
    uint8_t reserved[48];
};

// World state
struct GTA3WorldState {
    uint32_t gameTime = 0; // In-game minutes
    uint32_t weatherType = 0;
    float timeScale = 1.0f;
    uint32_t pedKills = 0;
    uint32_t vehicleKills = 0;
    uint32_t damageTaken = 0;
    uint32_t damageDealt = 0;
    uint32_t shotsFired = 0;
    uint32_t shotsHit = 0;
    uint32_t vehiclesStolen = 0;
    uint32_t packagesCollected = 0;
    uint32_t hiddenPackagesFound = 0;
    uint32_t rampagesPassed = 0;
    uint32_t uniqueJumpsFound = 0;
    uint8_t reserved[64];
};

// Complete save data structure
struct GTA3SaveData {
    GTA3SaveHeader header;
    GTA3PlayerState player;
    GTA3MissionState mission;
    GTA3WorldState world;
    
    // Vehicle array (up to 10 saved vehicles)
    std::array<GTA3VehicleState, 10> vehicles;
    uint32_t vehicleCount = 0;
    
    // Inventory
    std::array<uint32_t, 16> weaponAmmo;
    uint32_t itemCounts[32];
    
    // Flags and unlocks
    uint32_t unlockFlags = 0;
    uint32_t cheatFlags = 0;
    uint32_t stats[128];
    
    // Padding to reach 256KB
    uint8_t padding[GTA3_SAVE_SIZE - GTA3_SAVE_HEADER_SIZE - 
                    sizeof(GTA3PlayerState) - sizeof(GTA3MissionState) - 
                    sizeof(GTA3WorldState) - sizeof(GTA3VehicleState) * 10 -
                    sizeof(uint32_t) * 16 - sizeof(uint32_t) * 32 - 
                    sizeof(uint32_t) * 3 - sizeof(uint32_t) * 128];
};

// Save data manager
class GTA3SaveManager {
public:
    GTA3SaveManager();
    ~GTA3SaveManager();

    // Initialization
    bool Initialize(const std::string& saveDirectory);
    void Shutdown();

    // Save/Load operations
    bool SaveGame(int32_t slotId, const GTA3SaveData& data);
    bool LoadGame(int32_t slotId, GTA3SaveData& data);
    bool DeleteGame(int32_t slotId);
    
    // Save info (without loading full data)
    bool GetSaveInfo(int32_t slotId, GTA3SaveHeader& header);
    bool HasSave(int32_t slotId) const;
    
    // List saves
    std::vector<int32_t> GetSaveSlots() const;
    int32_t GetFirstEmptySlot() const;
    
    // Utilities
    uint32_t CalculateChecksum(const GTA3SaveData& data);
    bool ValidateChecksum(const GTA3SaveData& data);
    
    // Conversion
    bool ExportToJson(int32_t slotId, const std::string& jsonPath);
    bool ImportFromJson(const std::string& jsonPath, int32_t slotId);
    
    // State accessors
    std::string GetSaveDirectory() const { return m_saveDirectory; }
    int32_t GetSaveCount() const;
    size_t GetTotalSaveSize() const;

private:
    std::string GetSaveFilePath(int32_t slotId) const;
    bool EnsureDirectoryExists() const;
    void CompressSave(GTA3SaveData& data);
    void DecompressSave(GTA3SaveData& data);

    bool m_initialized = false;
    std::string m_saveDirectory;
    std::array<bool, GTA3_MAX_SAVE_SLOTS> m_slotExists;
};

// Global save manager
GTA3SaveManager& GetGTA3SaveManager();

} // namespace Kyty::Libs
