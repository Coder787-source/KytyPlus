#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Kyty::Libs {

/**
 * UE4 Mission Natives for GTA 3 DE Final Missions
 * 
 * This module provides additional UE4 native function implementations
 * required for completing late-game missions in GTA 3 DE, including
 * The Exchange and other complex scripted sequences.
 * 
 * Covers: Mission scripting, cutscene triggers, AI behavior,
 *         vehicle spawning, and endgame sequences.
 */

// Mission state types
enum class EMissionState : uint8_t {
    NotStarted = 0,
    InProgress = 1,
    ObjectiveActive = 2,
    ObjectiveCompleted = 3,
    Failed = 4,
    Completed = 5
};

// Objective types
enum class EObjectiveType : uint8_t {
    KillTarget = 0,
    CollectItem = 1,
    DeliverItem = 2,
    EscortNPC = 3,
    DefendLocation = 4,
    StealthKill = 5,
    RaceCheckpoint = 6,
    TimedMission = 7,
    VehicleMission = 8,
    CutsceneOnly = 9
};

// Native function signature
using UENativeFunction = std::function<intptr_t(uintptr_t* params)>;

/**
 * Mission native registration entry
 */
struct MissionNativeEntry {
    std::string name;
    std::string description;
    UENativeFunction function;
    bool isImplemented;
    int callCount;
    
    MissionNativeEntry() : isImplemented(false), callCount(0) {}
};

/**
 * Mission objective descriptor
 */
struct MissionObjective {
    int32_t objectiveId;
    EObjectiveType type;
    std::string description;
    bool isCompleted;
    bool isFailed;
    bool isOptional;
    int32_t targetActorId;
    float targetLocation[3];
    float radius;
    int32_t itemCount;
    int32_t requiredCount;
    float timeLimit;
    float currentTime;
    
    MissionObjective() : objectiveId(0), type(EObjectiveType::KillTarget),
                         isCompleted(false), isFailed(false), isOptional(false),
                         targetActorId(0), radius(5.0f), itemCount(0),
                         requiredCount(1), timeLimit(0.0f), currentTime(0.0f) {}
};

/**
 * Mission descriptor
 */
struct MissionDescriptor {
    int32_t missionId;
    std::string missionName;
    std::string internalName;
    EMissionState state;
    int32_t currentObjectiveIndex;
    std::vector<MissionObjective> objectives;
    bool isUnlocked;
    bool isCompleted;
    int32_t prerequisiteMissionId;
    float rewardMoney;
    int32_t rewardRespect;
    std::string unlockMessage;
    
    MissionDescriptor() : missionId(0), state(EMissionState::NotStarted),
                          currentObjectiveIndex(-1), isUnlocked(false),
                          isCompleted(false), prerequisiteMissionId(-1),
                          rewardMoney(0), rewardRespect(0) {}
};

/**
 * UE4 Mission Natives Manager
 */
class UE4MissionNatives {
public:
    static UE4MissionNatives& Instance();
    
    /**
     * Initialize mission natives system
     * @return true if initialization succeeded
     */
    bool Initialize();
    
    /**
     * Shutdown mission natives system
     */
    void Shutdown();
    
    /**
     * Register a native function
     * @param name Name of the native function
     * @param description Function description
     * @param function Function implementation
     * @return true if registration succeeded
     */
    bool RegisterNative(const std::string& name, const std::string& description,
                        UENativeFunction function);
    
    /**
     * Call a native function
     * @param name Name of the native function
     * @param params Function parameters
     * @return Function return value
     */
    intptr_t CallNative(const std::string& name, uintptr_t* params = nullptr);
    
    /**
     * Check if a native is registered
     * @param name Name of the native
     * @return true if registered
     */
    bool IsNativeRegistered(const std::string& name) const;
    
    /**
     * Get the number of registered natives
     * @return Number of natives
     */
    size_t GetNativeCount() const;
    
    /**
     * Get the number of calls to a native
     * @param name Name of the native
     * @return Call count
     */
    int GetNativeCallCount(const std::string& name) const;
    
    /**
     * Register a mission
     * @param mission Mission descriptor
     * @return true if registration succeeded
     */
    bool RegisterMission(const MissionDescriptor& mission);
    
    /**
     * Start a mission
     * @param missionId ID of the mission
     * @return true if mission started
     */
    bool StartMission(int32_t missionId);
    
    /**
     * Complete current objective
     * @param missionId ID of the mission
     * @param objectiveId ID of the objective
     * @return true if objective completed
     */
    bool CompleteObjective(int32_t missionId, int32_t objectiveId);
    
    /**
     * Fail current objective
     * @param missionId ID of the mission
     * @param objectiveId ID of the objective
     */
    void FailObjective(int32_t missionId, int32_t objectiveId);
    
    /**
     * Complete a mission
     * @param missionId ID of the mission
     */
    void CompleteMission(int32_t missionId);
    
    /**
     * Fail a mission
     * @param missionId ID of the mission
     */
    void FailMission(int32_t missionId);
    
    /**
     * Get mission state
     * @param missionId ID of the mission
     * @return Mission state
     */
    EMissionState GetMissionState(int32_t missionId) const;
    
    /**
     * Get current objective
     * @param missionId ID of the mission
     * @return Current objective, or nullptr if none
     */
    const MissionObjective* GetCurrentObjective(int32_t missionId) const;
    
    /**
     * Update mission (called per frame)
     * @param missionId ID of the mission
     * @param deltaTime Time since last update
     */
    void UpdateMission(int32_t missionId, float deltaTime);
    
    /**
     * Spawn a vehicle for mission
     * @param vehicleClass Vehicle class name
     * @param location Spawn location
     * @param rotation Spawn rotation
     * @param missionId Associated mission ID
     * @return Spawned actor ID, or 0 if failed
     */
    uint32_t SpawnMissionVehicle(const std::string& vehicleClass,
                                  float location[3], float rotation[3],
                                  int32_t missionId);
    
    /**
     * Spawn an NPC for mission
     * @param npcClass NPC class name
     * @param location Spawn location
     * @param missionId Associated mission ID
     * @return Spawned actor ID, or 0 if failed
     */
    uint32_t SpawnMissionNPC(const std::string& npcClass,
                              float location[3], int32_t missionId);
    
    /**
     * Trigger a cutscene
     * @param cutsceneName Name of the cutscene
     * @param missionId Associated mission ID
     * @return true if cutscene triggered
     */
    bool TriggerCutscene(const std::string& cutsceneName, int32_t missionId);
    
    /**
     * Set mission objective marker
     * @param missionId ID of the mission
     * @param objectiveId ID of the objective
     * @param location Marker location
     * @param radius Marker radius
     */
    void SetObjectiveMarker(int32_t missionId, int32_t objectiveId,
                            float location[3], float radius);
    
    /**
     * Play mission dialogue
     * @param dialogueId Dialogue identifier
     * @param speaker Speaker name
     * @param missionId Associated mission ID
     */
    void PlayMissionDialogue(const std::string& dialogueId,
                             const std::string& speaker, int32_t missionId);
    
    /**
     * Check if mission is unlocked
     * @param missionId ID of the mission
     * @return true if unlocked
     */
    bool IsMissionUnlocked(int32_t missionId) const;
    
    /**
     * Check if mission is completed
     * @param missionId ID of the mission
     * @return true if completed
     */
    bool IsMissionCompleted(int32_t missionId) const;
    
    /**
     * Get the number of registered missions
     * @return Number of missions
     */
    size_t GetMissionCount() const;
    
    /**
     * Enable/disable mission natives
     * @param enabled Enable/disable
     */
    void SetEnabled(bool enabled);
    
    /**
     * Check if enabled
     * @return true if enabled
     */
    bool IsEnabled() const;
    
private:
    UE4MissionNatives() = default;
    ~UE4MissionNatives() = default;
    
    UE4MissionNatives(const UE4MissionNatives&) = delete;
    UE4MissionNatives& operator=(const UE4MissionNatives&) = delete;
    
    std::unordered_map<std::string, MissionNativeEntry> m_natives;
    std::unordered_map<int32_t, MissionDescriptor> m_missions;
    bool m_enabled = true;
    bool m_initialized = false;
    
    // Native function implementations
    intptr_t Native_CreateVehicle(uintptr_t* params);
    intptr_t Native_CreatePed(uintptr_t* params);
    intptr_t Native_SetObjective(uintptr_t* params);
    intptr_t Native_CompleteObjective(uintptr_t* params);
    intptr_t Native_FailMission(uintptr_t* params);
    intptr_t Native_PlayCutscene(uintptr_t* params);
    intptr_t Native_SpawnVehicle(uintptr_t* params);
    intptr_t Native_SpawnPed(uintptr_t* params);
    intptr_t Native_SetMarker(uintptr_t* params);
    intptr_t Native_PlaySound(uintptr_t* params);
};

/**
 * Initialize GTA 3 DE final mission natives
 */
void InitializeFinalMissionNatives();

/**
 * Register natives for The Exchange mission
 */
void RegisterExchangeMissionNatives();

/**
 * Register natives for Shoreside Vale missions
 */
void RegisterShoresideValeNatives();

} // namespace Kyty::Libs
