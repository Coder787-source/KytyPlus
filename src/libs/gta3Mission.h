#pragma once

#include "common/common.h"
#include <string>
#include <vector>
#include <functional>

namespace Kyty::Libs {

// GTA 3 Definitive Edition Mission System
// Handles mission triggers, progression, and state

constexpr int32_t GTA3_MAX_MISSIONS = 120; // All story missions
constexpr int32_t GTA3_MAX_ACTIVE_MISSIONS = 10;

enum class GTAMissionState {
    NotAvailable,
    Available,
    Active,
    Completed,
    Failed
};

enum class GTAMissionType {
    Story,
    Side,
    Rampage,
    Vigilante,
    Taxi,
    Paramedic,
    Firefighter,
    StreetRace,
    UniqueJump,
    PackageDelivery
};

struct GTAMissionInfo {
    uint32_t id = 0;
    std::string name;
    std::string description;
    GTAMissionType type = GTAMissionType::Story;
    std::string giverName;
    std::string locationName;
    float locationX = 0.0f;
    float locationY = 0.0f;
    float locationZ = 0.0f;
    uint32_t prerequisiteMission = 0;
    uint32_t rewardMoney = 0;
    uint32_t rewardRespect = 0;
    bool isUnlocked = false;
};

struct GTAMissionObjective {
    std::string description;
    bool isCompleted = false;
    bool isOptional = false;
    uint32_t targetCount = 0;
    uint32_t currentCount = 0;
};

struct GTAMissionState {
    uint32_t missionId = 0;
    GTAMissionState state = GTAMissionState::NotAvailable;
    float startTime = 0.0f;
    float currentTime = 0.0f;
    int32_t failures = 0;
    std::vector<GTAMissionObjective> objectives;
    uint32_t currentObjectiveIndex = 0;
    bool missionPassed = false;
    bool missionFailed = false;
    std::string failReason;
};

// Mission callbacks
using MissionStartCallback = std::function<void(uint32_t missionId)>;
using MissionCompleteCallback = std::function<void(uint32_t missionId, bool success)>;
using MissionUpdateCallback = std::function<void(uint32_t missionId, const GTAMissionState&)>;

// Mission system for GTA 3 DE
class GTA3MissionSystem {
public:
    GTA3MissionSystem();
    ~GTA3MissionSystem();

    // Initialization
    bool Initialize();
    void Shutdown();

    // Mission data
    bool LoadMissionDefinitions();
    const GTAMissionInfo* GetMissionInfo(uint32_t missionId) const;
    int32_t GetTotalMissionCount() const;
    int32_t GetCompletedMissionCount() const;
    float GetCompletionPercentage() const;

    // Mission availability
    bool IsMissionAvailable(uint32_t missionId) const;
    bool IsMissionUnlocked(uint32_t missionId) const;
    std::vector<uint32_t> GetAvailableMissions() const;
    
    // Mission control
    bool StartMission(uint32_t missionId);
    bool CompleteMission(uint32_t missionId, bool success);
    bool FailMission(uint32_t missionId, const std::string& reason);
    bool AbortMission();
    
    // Active mission
    bool HasActiveMission() const;
    uint32_t GetActiveMissionId() const;
    const GTAMissionState* GetActiveMissionState() const;
    GTAMissionState* GetActiveMissionState();
    
    // Objectives
    bool AddObjective(const std::string& description, bool optional = false);
    bool CompleteObjective(uint32_t objectiveIndex);
    bool UpdateObjectiveCount(uint32_t objectiveIndex, uint32_t count);
    uint32_t GetCurrentObjectiveIndex() const;
    int32_t GetObjectiveCount() const;
    
    // Progress tracking
    void SetMissionCompleted(uint32_t missionId);
    void SetMissionFailed(uint32_t missionId);
    bool IsMissionCompleted(uint32_t missionId) const;
    bool IsMissionFailed(uint32_t missionId) const;
    int32_t GetMissionFailures(uint32_t missionId) const;
    
    // Unlock progression
    void UnlockMission(uint32_t missionId);
    void LockMission(uint32_t missionId);
    void UnlockNextMissions(uint32_t completedMissionId);
    
    // Stats
    uint32_t GetTotalPlayTime() const;
    uint32_t GetMissionPlayTime() const;
    uint32_t GetFreeRoamPlayTime() const;
    
    // Callbacks
    void SetMissionStartCallback(MissionStartCallback callback);
    void SetMissionCompleteCallback(MissionCompleteCallback callback);
    void SetMissionUpdateCallback(MissionUpdateCallback callback);

    // Debug
    void DumpMissionProgress() const;
    void DebugPrintActiveMission() const;

private:
    void InitializeStoryMissions();
    void InitializeSideMissions();
    bool CheckPrerequisites(uint32_t missionId) const;
    void UpdateMissionState();
    void TriggerCallbacks();

    bool m_initialized = false;
    std::vector<GTAMissionInfo> m_missions;
    std::vector<bool> m_missionCompleted;
    std::vector<bool> m_missionFailed;
    std::vector<int32_t> m_missionFailures;
    std::vector<bool> m_missionUnlocked;
    
    GTAMissionState m_activeMission;
    bool m_hasActiveMission = false;
    
    uint32_t m_totalPlayTime = 0;
    uint32_t m_missionPlayTime = 0;
    
    MissionStartCallback m_onMissionStart;
    MissionCompleteCallback m_onMissionComplete;
    MissionUpdateCallback m_onMissionUpdate;
};

// Global mission system
GTA3MissionSystem& GetGTA3MissionSystem();

} // namespace Kyty::Libs
