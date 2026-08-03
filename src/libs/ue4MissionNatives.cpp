#include "ue4MissionNatives.h"
#include "ue4HLE.h"
#include "common/logging.hpp"
#include <algorithm>

namespace Kyty::Libs {

using namespace Kyty::Common;

//=============================================================================
// UE4MissionNatives Implementation
//=============================================================================

UE4MissionNatives& UE4MissionNatives::Instance() {
    static UE4MissionNatives instance;
    return instance;
}

bool UE4MissionNatives::Initialize() {
    if (m_initialized) {
        Log::Warning("[UE4Mission] Already initialized");
        return true;
    }
    
    Log::Info("[UE4Mission] Initializing mission natives system...");
    
    // Register core native functions
    RegisterNative("CREATE_VEHICLE", "Create a vehicle for mission", 
                   [this](uintptr_t* params) { return Native_CreateVehicle(params); });
    RegisterNative("CREATE_PED", "Create a ped for mission",
                   [this](uintptr_t* params) { return Native_CreatePed(params); });
    RegisterNative("SET_OBJECTIVE", "Set mission objective",
                   [this](uintptr_t* params) { return Native_SetObjective(params); });
    RegisterNative("COMPLETE_OBJECTIVE", "Complete mission objective",
                   [this](uintptr_t* params) { return Native_CompleteObjective(params); });
    RegisterNative("FAIL_MISSION", "Fail the current mission",
                   [this](uintptr_t* params) { return Native_FailMission(params); });
    RegisterNative("PLAY_CUTSCENE", "Play a cutscene",
                   [this](uintptr_t* params) { return Native_PlayCutscene(params); });
    RegisterNative("SPAWN_VEHICLE", "Spawn a vehicle at location",
                   [this](uintptr_t* params) { return Native_SpawnVehicle(params); });
    RegisterNative("SPAWN_PED", "Spawn a ped at location",
                   [this](uintptr_t* params) { return Native_SpawnPed(params); });
    RegisterNative("SET_MARKER", "Set objective marker",
                   [this](uintptr_t* params) { return Native_SetMarker(params); });
    RegisterNative("PLAY_SOUND", "Play mission sound",
                   [this](uintptr_t* params) { return Native_PlaySound(params); });
    
    // Initialize GTA 3 DE final mission natives
    InitializeFinalMissionNatives();
    
    m_initialized = true;
    
    Log::Info("[UE4Mission] Mission natives system initialized (%zu natives, %zu missions)",
              m_natives.size(), m_missions.size());
    return true;
}

void UE4MissionNatives::Shutdown() {
    Log::Info("[UE4Mission] Shutting down mission natives system...");
    
    m_natives.clear();
    m_missions.clear();
    m_initialized = false;
    
    Log::Info("[UE4Mission] Mission natives system shut down");
}

bool UE4MissionNatives::RegisterNative(const std::string& name, const std::string& description,
                                        UENativeFunction function) {
    if (m_natives.find(name) != m_natives.end()) {
        Log::Warning("[UE4Mission] Native already registered: %s", name.c_str());
        return false;
    }
    
    MissionNativeEntry entry;
    entry.name = name;
    entry.description = description;
    entry.function = std::move(function);
    entry.isImplemented = true;
    entry.callCount = 0;
    
    m_natives[name] = entry;
    Log::Debug("[UE4Mission] Registered native: %s", name.c_str());
    
    return true;
}

intptr_t UE4MissionNatives::CallNative(const std::string& name, uintptr_t* params) {
    if (!m_enabled) {
        Log::Warning("[UE4Mission] Mission natives disabled");
        return 0;
    }
    
    auto it = m_natives.find(name);
    if (it == m_natives.end()) {
        Log::Warning("[UE4Mission] Native not found: %s", name.c_str());
        return 0;
    }
    
    it->second.callCount++;
    
    if (it->second.function) {
        return it->second.function(params);
    }
    
    return 0;
}

bool UE4MissionNatives::IsNativeRegistered(const std::string& name) const {
    return m_natives.find(name) != m_natives.end();
}

size_t UE4MissionNatives::GetNativeCount() const {
    return m_natives.size();
}

int UE4MissionNatives::GetNativeCallCount(const std::string& name) const {
    auto it = m_natives.find(name);
    if (it == m_natives.end()) {
        return 0;
    }
    
    return it->second.callCount;
}

bool UE4MissionNatives::RegisterMission(const MissionDescriptor& mission) {
    if (m_missions.find(mission.missionId) != m_missions.end()) {
        Log::Warning("[UE4Mission] Mission already registered: %d", mission.missionId);
        return false;
    }
    
    m_missions[mission.missionId] = mission;
    Log::Debug("[UE4Mission] Registered mission: %s (ID=%d)",
               mission.missionName.c_str(), mission.missionId);
    
    return true;
}

bool UE4MissionNatives::StartMission(int32_t missionId) {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        Log::Warning("[UE4Mission] Mission not found: %d", missionId);
        return false;
    }
    
    if (!it->second.isUnlocked) {
        Log::Warning("[UE4Mission] Mission not unlocked: %d", missionId);
        return false;
    }
    
    it->second.state = EMissionState::InProgress;
    it->second.currentObjectiveIndex = 0;
    
    Log::Info("[UE4Mission] Started mission: %s", it->second.missionName.c_str());
    return true;
}

bool UE4MissionNatives::CompleteObjective(int32_t missionId, int32_t objectiveId) {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    for (auto& objective : it->second.objectives) {
        if (objective.objectiveId == objectiveId) {
            objective.isCompleted = true;
            Log::Info("[UE4Mission] Completed objective %d in mission %d",
                     objectiveId, missionId);
            
            // Move to next objective
            it->second.currentObjectiveIndex++;
            
            if (it->second.currentObjectiveIndex >= static_cast<int>(it->second.objectives.size())) {
                CompleteMission(missionId);
            }
            
            return true;
        }
    }
    
    return false;
}

void UE4MissionNatives::FailObjective(int32_t missionId, int32_t objectiveId) {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return;
    }
    
    for (auto& objective : it->second.objectives) {
        if (objective.objectiveId == objectiveId) {
            objective.isFailed = true;
            
            if (!objective.isOptional) {
                FailMission(missionId);
            }
            
            Log::Info("[UE4Mission] Failed objective %d in mission %d",
                     objectiveId, missionId);
            return;
        }
    }
}

void UE4MissionNatives::CompleteMission(int32_t missionId) {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return;
    }
    
    it->second.state = EMissionState::Completed;
    it->second.isCompleted = true;
    
    Log::Info("[UE4Mission] Completed mission: %s (reward: $%d, %d respect)",
              it->second.missionName.c_str(), it->second.rewardMoney, it->second.rewardRespect);
}

void UE4MissionNatives::FailMission(int32_t missionId) {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return;
    }
    
    it->second.state = EMissionState::Failed;
    
    Log::Info("[UE4Mission] Failed mission: %s", it->second.missionName.c_str());
}

EMissionState UE4MissionNatives::GetMissionState(int32_t missionId) const {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return EMissionState::NotStarted;
    }
    
    return it->second.state;
}

const MissionObjective* UE4MissionNatives::GetCurrentObjective(int32_t missionId) const {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end() || it->second.currentObjectiveIndex < 0) {
        return nullptr;
    }
    
    if (it->second.currentObjectiveIndex >= static_cast<int>(it->second.objectives.size())) {
        return nullptr;
    }
    
    return &it->second.objectives[it->second.currentObjectiveIndex];
}

void UE4MissionNatives::UpdateMission(int32_t missionId, float deltaTime) {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end() || it->second.state != EMissionState::InProgress) {
        return;
    }
    
    // Update current objective timer
    if (it->second.currentObjectiveIndex >= 0 &&
        it->second.currentObjectiveIndex < static_cast<int>(it->second.objectives.size())) {
        auto& objective = it->second.objectives[it->second.currentObjectiveIndex];
        
        if (objective.timeLimit > 0.0f) {
            objective.currentTime += deltaTime;
            
            if (objective.currentTime >= objective.timeLimit) {
                FailObjective(missionId, objective.objectiveId);
            }
        }
    }
}

uint32_t UE4MissionNatives::SpawnMissionVehicle(const std::string& vehicleClass,
                                                 float location[3], float rotation[3],
                                                 int32_t missionId) {
    Log::Info("[UE4Mission] Spawning mission vehicle: %s at (%.2f, %.2f, %.2f)",
              vehicleClass.c_str(), location[0], location[1], location[2]);
    
    // In production, this would spawn the actual vehicle
    // Return a mock ID for now
    static uint32_t nextVehicleId = 1000;
    return nextVehicleId++;
}

uint32_t UE4MissionNatives::SpawnMissionNPC(const std::string& npcClass,
                                             float location[3], int32_t missionId) {
    Log::Info("[UE4Mission] Spawning mission NPC: %s at (%.2f, %.2f, %.2f)",
              npcClass.c_str(), location[0], location[1], location[2]);
    
    // In production, this would spawn the actual NPC
    static uint32_t nextNPCId = 2000;
    return nextNPCId++;
}

bool UE4MissionNatives::TriggerCutscene(const std::string& cutsceneName, int32_t missionId) {
    Log::Info("[UE4Mission] Triggering cutscene: %s for mission %d",
              cutsceneName.c_str(), missionId);
    
    // In production, this would trigger the actual cutscene
    return true;
}

void UE4MissionNatives::SetObjectiveMarker(int32_t missionId, int32_t objectiveId,
                                            float location[3], float radius) {
    Log::Debug("[UE4Mission] Set marker for objective %d: (%.2f, %.2f, %.2f) r=%.2f",
               objectiveId, location[0], location[1], location[2], radius);
}

void UE4MissionNatives::PlayMissionDialogue(const std::string& dialogueId,
                                             const std::string& speaker, int32_t missionId) {
    Log::Info("[UE4Mission] Playing dialogue: %s (%s) for mission %d",
              dialogueId.c_str(), speaker.c_str(), missionId);
}

bool UE4MissionNatives::IsMissionUnlocked(int32_t missionId) const {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    return it->second.isUnlocked;
}

bool UE4MissionNatives::IsMissionCompleted(int32_t missionId) const {
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    return it->second.isCompleted;
}

size_t UE4MissionNatives::GetMissionCount() const {
    return m_missions.size();
}

void UE4MissionNatives::SetEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        Log::Info("[UE4Mission] Mission natives %s", enabled ? "enabled" : "disabled");
    }
}

bool UE4MissionNatives::IsEnabled() const {
    return m_enabled;
}

// Native function implementations
intptr_t UE4MissionNatives::Native_CreateVehicle(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: CREATE_VEHICLE called");
    return 1;
}

intptr_t UE4MissionNatives::Native_CreatePed(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: CREATE_PED called");
    return 1;
}

intptr_t UE4MissionNatives::Native_SetObjective(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: SET_OBJECTIVE called");
    return 1;
}

intptr_t UE4MissionNatives::Native_CompleteObjective(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: COMPLETE_OBJECTIVE called");
    return 1;
}

intptr_t UE4MissionNatives::Native_FailMission(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: FAIL_MISSION called");
    return 1;
}

intptr_t UE4MissionNatives::Native_PlayCutscene(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: PLAY_CUTSCENE called");
    return 1;
}

intptr_t UE4MissionNatives::Native_SpawnVehicle(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: SPAWN_VEHICLE called");
    return 1;
}

intptr_t UE4MissionNatives::Native_SpawnPed(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: SPAWN_PED called");
    return 1;
}

intptr_t UE4MissionNatives::Native_SetMarker(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: SET_MARKER called");
    return 1;
}

intptr_t UE4MissionNatives::Native_PlaySound(uintptr_t* params) {
    Log::Debug("[UE4Mission] Native: PLAY_SOUND called");
    return 1;
}

//=============================================================================
// Final Mission Natives Initialization
//=============================================================================

void InitializeFinalMissionNatives() {
    Log::Info("[UE4Mission] Initializing final mission natives...");
    
    RegisterExchangeMissionNatives();
    RegisterShoresideValeNatives();
    
    Log::Info("[UE4Mission] Final mission natives initialized");
}

void RegisterExchangeMissionNatives() {
    auto& natives = UE4MissionNatives::Instance();
    
    // The Exchange mission (final mission)
    MissionDescriptor exchange;
    exchange.missionId = 100;
    exchange.missionName = "The Exchange";
    exchange.internalName = "MISSION_EXCHANGE";
    exchange.isUnlocked = true;
    exchange.prerequisiteMissionId = 99;
    exchange.rewardMoney = 500000;
    exchange.rewardRespect = 1000;
    
    // Objective 1: Reach the stadium
    MissionObjective obj1;
    obj1.objectiveId = 1;
    obj1.type = EObjectiveType::DeliverItem;
    obj1.description = "Reach the stadium";
    obj1.radius = 10.0f;
    exchange.objectives.push_back(obj1);
    
    // Objective 2: Eliminate targets
    MissionObjective obj2;
    obj2.objectiveId = 2;
    obj2.type = EObjectiveType::KillTarget;
    obj2.description = "Eliminate the targets";
    obj2.requiredCount = 3;
    exchange.objectives.push_back(obj2);
    
    // Objective 3: Escape
    MissionObjective obj3;
    obj3.objectiveId = 3;
    obj3.type = EObjectiveType::TimedMission;
    obj3.description = "Escape before the helicopter arrives";
    obj3.timeLimit = 120.0f; // 2 minutes
    exchange.objectives.push_back(obj3);
    
    natives.RegisterMission(exchange);
    
    Log::Info("[UE4Mission] Registered The Exchange mission");
}

void RegisterShoresideValeNatives() {
    auto& natives = UE4MissionNatives::Instance();
    
    // A Drop In The Ocean
    MissionDescriptor dropIn;
    dropIn.missionId = 95;
    dropIn.missionName = "A Drop In The Ocean";
    dropIn.internalName = "MISSION_DROP_OCEAN";
    dropIn.isUnlocked = true;
    dropIn.rewardMoney = 50000;
    dropIn.rewardRespect = 100;
    
    MissionObjective obj;
    obj.objectiveId = 1;
    obj.type = EObjectiveType::CollectItem;
    obj.description = "Collect the package";
    dropIn.objectives.push_back(obj);
    
    natives.RegisterMission(dropIn);
    
    // Uzi Money
    MissionDescriptor uziMoney;
    uziMoney.missionId = 96;
    uziMoney.missionName = "Uzi Money";
    uziMoney.internalName = "MISSION_UZI_MONEY";
    uziMoney.isUnlocked = true;
    uziMoney.prerequisiteMissionId = 95;
    uziMoney.rewardMoney = 75000;
    uziMoney.rewardRespect = 150;
    
    obj.objectiveId = 1;
    obj.type = EObjectiveType::KillTarget;
    obj.description = "Collect protection money";
    obj.requiredCount = 5;
    uziMoney.objectives.push_back(obj);
    
    natives.RegisterMission(uziMoney);
    
    // Toyminator
    MissionDescriptor toyminator;
    toyminator.missionId = 97;
    toyminator.missionName = "Toyminator";
    toyminator.internalName = "MISSION_TOYMINATOR";
    toyminator.isUnlocked = true;
    toyminator.prerequisiteMissionId = 96;
    toyminator.rewardMoney = 100000;
    toyminator.rewardRespect = 200;
    
    obj.objectiveId = 1;
    obj.type = EObjectiveType::VehicleMission;
    obj.description = "Destroy targets with RC vehicle";
    obj.requiredCount = 10;
    toyminator.objectives.push_back(obj);
    
    natives.RegisterMission(toyminator);
    
    Log::Info("[UE4Mission] Registered Shoreside Vale missions");
}

} // namespace Kyty::Libs
