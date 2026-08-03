#include "libs/gta3Mission.h"
#include "common/logging/log.h"
#include <algorithm>
#include <cstring>

namespace Kyty::Libs {

// Global mission system
static GTA3MissionSystem g_gta3MissionSystem;

GTA3MissionSystem& GetGTA3MissionSystem() {
    return g_gta3MissionSystem;
}

GTA3MissionSystem::GTA3MissionSystem() {
    m_missions.reserve(GTA3_MAX_MISSIONS);
    m_missionCompleted.resize(GTA3_MAX_MISSIONS, false);
    m_missionFailed.resize(GTA3_MAX_MISSIONS, false);
    m_missionFailures.resize(GTA3_MAX_MISSIONS, 0);
    m_missionUnlocked.resize(GTA3_MAX_MISSIONS, false);
}

GTA3MissionSystem::~GTA3MissionSystem() {
    Shutdown();
}

bool GTA3MissionSystem::Initialize() {
    if (m_initialized) {
        LOGF("[GTA3Mission] WARNING: " "Already initialized");
        return true;
    }

    LOGF("[GTA3Mission] INFO: " "Initializing mission system...");

    // Initialize mission definitions
    InitializeStoryMissions();
    InitializeSideMissions();

    // Unlock first mission by default
    if (!m_missions.empty()) {
        UnlockMission(1); // First story mission
    }

    m_initialized = true;
    
    LOGF("[GTA3Mission] INFO: " "Initialized %zu missions", m_missions.size());
    LOGF("[GTA3Mission] INFO: " "Unlocked missions: %zu", 
             std::count(m_missionUnlocked.begin(), m_missionUnlocked.end(), true));

    return true;
}

void GTA3MissionSystem::Shutdown() {
    m_missions.clear();
    m_missionCompleted.clear();
    m_missionFailed.clear();
    m_missionFailures.clear();
    m_missionUnlocked.clear();
    m_hasActiveMission = false;
    m_initialized = false;
    
    LOGF("[GTA3Mission] INFO: " "Shutdown complete");
}

void GTA3MissionSystem::InitializeStoryMissions() {
    LOGF("[GTA3Mission] DEBUG: " "Initializing story missions...");

    // GTA 3 Story Missions (simplified list)
    // Liberty City Stories
    
    // Portland Island
    m_missions.push_back({1, "Give Me Liberty", "Meet 8-Ball in the courthouse", 
                          GTAMissionType::Story, "8-Ball", "Courthouse", -638.0f, 892.0f, 0.0f, 0, 0, 0, true});
    
    m_missions.push_back({2, "Luigi's Girls", "Meet Luigi at the club", 
                          GTAMissionType::Story, "Luigi", "Paulie's Revue Bar", -580.0f, 920.0f, 0.0f, 1, 100, 0, false});
    
    m_missions.push_back({3, "Don't Spank Ma Bitch Up", "Defend the brothel", 
                          GTAMissionType::Story, "Luigi", "Paulie's Revue Bar", -580.0f, 920.0f, 0.0f, 2, 200, 0, false});
    
    m_missions.push_back({4, "Drive Misty For Me", "Pick up Misty", 
                          GTAMissionType::Story, "Luigi", "Paulie's Revue Bar", -580.0f, 920.0f, 0.0f, 3, 300, 0, false});
    
    m_missions.push_back({5, "Pump Action Pimps", "Eliminate rival pimps", 
                          GTAMissionType::Story, "Luigi", "Paulie's Revue Bar", -580.0f, 920.0f, 0.0f, 4, 500, 0, false});
    
    m_missions.push_back({6, "Mike Lips Last Lunch", "Meet Joey", 
                          GTAMissionType::Story, "Joey", "Joey's Garage", -620.0f, 850.0f, 0.0f, 5, 400, 0, false});
    
    m_missions.push_back({7, "Frightened Fares", "Scare the cabbie", 
                          GTAMissionType::Story, "Joey", "Joey's Garage", -620.0f, 850.0f, 0.0f, 6, 500, 0, false});
    
    m_missions.push_back({8, "The Getaway", "Getaway driver job", 
                          GTAMissionType::Story, "Joey", "Joey's Garage", -620.0f, 850.0f, 0.0f, 7, 600, 0, false});
    
    m_missions.push_back({9, "Cutting The Grass", "Meet Toni", 
                          GTAMissionType::Story, "Toni", "Tavern", -700.0f, 900.0f, 0.0f, 8, 700, 0, false});
    
    m_missions.push_back({10, "Blow Fish", "Eliminate target", 
                          GTAMissionType::Story, "Toni", "Tavern", -700.0f, 900.0f, 0.0f, 9, 800, 0, false});
    
    // Staunton Island missions
    m_missions.push_back({11, "Silence The Sneak", "First Staunton mission", 
                          GTAMissionType::Story, "Asuka", "Asuka's Hideout", 1200.0f, 400.0f, 0.0f, 10, 1000, 0, false});
    
    m_missions.push_back({12, "Plaster Blaster", "Protect the informant", 
                          GTAMissionType::Story, "Asuka", "Asuka's Hideout", 1200.0f, 400.0f, 0.0f, 11, 1200, 0, false});
    
    m_missions.push_back({13, "Liberty Rumours", "Investigate rumors", 
                          GTAMissionType::Story, "Asuka", "Asuka's Hideout", 1200.0f, 400.0f, 0.0f, 12, 1400, 0, false});
    
    m_missions.push_back({14, "Uzi Rider", "Eliminate rival gang", 
                          GTAMissionType::Story, "Asuka", "Asuka's Hideout", 1200.0f, 400.0f, 0.0f, 13, 1600, 0, false});
    
    m_missions.push_back({15, "Grand Theft Auto", "Steal cars", 
                          GTAMissionType::Story, "Asuka", "Asuka's Hideout", 1200.0f, 400.0f, 0.0f, 14, 1800, 0, false});
    
    // Shoreside Vale missions
    m_missions.push_back({16, "A Drop In The Ocean", "First Shoreside mission", 
                          GTAMissionType::Story, "Donald", "Airport", 2400.0f, -800.0f, 0.0f, 15, 2000, 0, false});
    
    m_missions.push_back({17, "Uzi Money", "Collect money", 
                          GTAMissionType::Story, "Donald", "Airport", 2400.0f, -800.0f, 0.0f, 16, 2200, 0, false});
    
    m_missions.push_back({18, "Toyminator", "Destroy the van", 
                          GTAMissionType::Story, "Donald", "Airport", 2400.0f, -800.0f, 0.0f, 17, 2400, 0, false});
    
    m_missions.push_back({19, "Rigged To Blow", "Final mission setup", 
                          GTAMissionType::Story, "Donald", "Airport", 2400.0f, -800.0f, 0.0f, 18, 2600, 0, false});
    
    m_missions.push_back({20, "The Exchange", "Final mission", 
                          GTAMissionType::Story, "Donald", "Airport", 2400.0f, -800.0f, 0.0f, 19, 10000, 0, false});

    LOGF("[GTA3Mission] INFO: " "Loaded %zu story missions", m_missions.size());
}

void GTA3MissionSystem::InitializeSideMissions() {
    LOGF("[GTA3Mission] DEBUG: " "Initializing side missions...");

    // Side activities (simplified)
    size_t sideStart = m_missions.size();
    
    // Vigilante missions
    for (int i = 0; i < 12; i++) {
        char name[64], desc[128];
        snprintf(name, sizeof(name), "Vigilante %d", i + 1);
        snprintf(desc, sizeof(desc), "Stop criminals as a vigilante - Level %d", i + 1);
        
        GTAMissionInfo mission;
        mission.id = static_cast<uint32_t>(m_missions.size() + 1);
        mission.name = name;
        mission.description = desc;
        mission.type = GTAMissionType::Vigilante;
        mission.giverName = "Police Radio";
        mission.locationName = "Anywhere";
        mission.locationX = 0.0f;
        mission.locationY = 0.0f;
        mission.locationZ = 0.0f;
        mission.prerequisiteMission = 0;
        mission.rewardMoney = 100 * (i + 1);
        mission.rewardRespect = 10;
        mission.isUnlocked = true;
        
        m_missions.push_back(mission);
    }
    
    // Taxi missions
    for (int i = 0; i < 5; i++) {
        char name[64], desc[128];
        snprintf(name, sizeof(name), "Taxi Driver %d", i + 1);
        snprintf(desc, sizeof(desc), "Complete taxi fares - Level %d", i + 1);
        
        GTAMissionInfo mission;
        mission.id = static_cast<uint32_t>(m_missions.size() + 1);
        mission.name = name;
        mission.description = desc;
        mission.type = GTAMissionType::Taxi;
        mission.giverName = "Taxi Dispatcher";
        mission.locationName = "Taxi Stand";
        mission.rewardMoney = 50 * (i + 1);
        mission.isUnlocked = true;
        
        m_missions.push_back(mission);
    }
    
    // Unique jumps
    for (int i = 0; i < 20; i++) {
        char name[64], desc[128];
        snprintf(name, sizeof(name), "Unique Jump %d", i + 1);
        snprintf(desc, sizeof(desc), "Perform a unique stunt jump");
        
        GTAMissionInfo mission;
        mission.id = static_cast<uint32_t>(m_missions.size() + 1);
        mission.name = name;
        mission.description = desc;
        mission.type = GTAMissionType::UniqueJump;
        mission.giverName = "None";
        mission.locationName = "Various";
        mission.rewardMoney = 100;
        mission.isUnlocked = true;
        
        m_missions.push_back(mission);
    }

    LOGF("[GTA3Mission] INFO: " "Loaded %zu side missions", m_missions.size() - sideStart);
}

bool GTA3MissionSystem::LoadMissionDefinitions() {
    // Already loaded in Initialize()
    return m_initialized;
}

const GTAMissionInfo* GTA3MissionSystem::GetMissionInfo(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return nullptr;
    }
    
    return &m_missions[missionId - 1];
}

int32_t GTA3MissionSystem::GetTotalMissionCount() const {
    return static_cast<int32_t>(m_missions.size());
}

int32_t GTA3MissionSystem::GetCompletedMissionCount() const {
    return static_cast<int32_t>(std::count(m_missionCompleted.begin(), 
                                            m_missionCompleted.end(), true));
}

float GTA3MissionSystem::GetCompletionPercentage() const {
    if (m_missions.empty()) return 0.0f;
    
    return static_cast<float>(GetCompletedMissionCount()) / m_missions.size() * 100.0f;
}

bool GTA3MissionSystem::IsMissionAvailable(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return false;
    }
    
    return m_missionUnlocked[missionId - 1] && 
           !m_missionCompleted[missionId - 1] &&
           !m_hasActiveMission;
}

bool GTA3MissionSystem::IsMissionUnlocked(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return false;
    }
    
    return m_missionUnlocked[missionId - 1];
}

std::vector<uint32_t> GTA3MissionSystem::GetAvailableMissions() const {
    std::vector<uint32_t> available;
    
    for (size_t i = 0; i < m_missions.size(); i++) {
        if (m_missionUnlocked[i] && !m_missionCompleted[i] && !m_hasActiveMission) {
            available.push_back(static_cast<uint32_t>(i + 1));
        }
    }
    
    return available;
}

bool GTA3MissionSystem::StartMission(uint32_t missionId) {
    if (!IsMissionAvailable(missionId)) {
        LOGF("[GTA3Mission] WARNING: " "Cannot start mission %u: not available", missionId);
        return false;
    }
    
    if (m_hasActiveMission) {
        LOGF("[GTA3Mission] WARNING: " "Cannot start mission %u: another mission is active", missionId);
        return false;
    }
    
    const GTAMissionInfo* info = GetMissionInfo(missionId);
    if (!info) {
        LOGF("[GTA3Mission] ERROR: " "Invalid mission ID: %u", missionId);
        return false;
    }
    
    LOGF("[GTA3Mission] INFO: " "Starting mission %u: %s", missionId, info->name.c_str());
    
    // Initialize active mission state
    m_activeMission.missionId = missionId;
    m_activeMission.state = GTAMissionState::Active;
    m_activeMission.startTime = 0.0f; // Would get from game clock
    m_activeMission.currentTime = 0.0f;
    m_activeMission.failures = m_missionFailures[missionId - 1];
    m_activeMission.objectives.clear();
    m_activeMission.currentObjectiveIndex = 0;
    m_activeMission.missionPassed = false;
    m_activeMission.missionFailed = false;
    m_hasActiveMission = true;
    
    // Add default objective
    AddObjective(info->description);
    
    // Trigger callback
    if (m_onMissionStart) {
        m_onMissionStart(missionId);
    }
    
    return true;
}

bool GTA3MissionSystem::CompleteMission(uint32_t missionId, bool success) {
    if (!m_hasActiveMission || m_activeMission.missionId != missionId) {
        LOGF("[GTA3Mission] WARNING: " "Cannot complete mission %u: not active", missionId);
        return false;
    }
    
    const GTAMissionInfo* info = GetMissionInfo(missionId);
    
    if (success) {
        LOGF("[GTA3Mission] INFO: " "Mission %u PASSED: %s", missionId, info->name.c_str());
        
        m_missionCompleted[missionId - 1] = true;
        m_activeMission.missionPassed = true;
        m_activeMission.state = GTAMissionState::Completed;
        
        // Unlock next missions
        UnlockNextMissions(missionId);
        
        m_missionPlayTime += static_cast<uint32_t>(m_activeMission.currentTime);
    } else {
        LOGF("[GTA3Mission] INFO: " "Mission %u FAILED: %s", missionId, info->name.c_str());
        
        m_missionFailed[missionId - 1] = true;
        m_missionFailures[missionId - 1]++;
        m_activeMission.failures++;
        m_activeMission.missionFailed = true;
        m_activeMission.state = GTAMissionState::Failed;
    }
    
    m_hasActiveMission = false;
    
    // Trigger callback
    if (m_onMissionComplete) {
        m_onMissionComplete(missionId, success);
    }
    
    return true;
}

bool GTA3MissionSystem::FailMission(uint32_t missionId, const std::string& reason) {
    if (!m_hasActiveMission || m_activeMission.missionId != missionId) {
        return false;
    }
    
    m_activeMission.failReason = reason;
    return CompleteMission(missionId, false);
}

bool GTA3MissionSystem::AbortMission() {
    if (!m_hasActiveMission) {
        return false;
    }
    
    uint32_t missionId = m_activeMission.missionId;
    m_hasActiveMission = false;
    m_activeMission = GTAMissionState{};
    
    LOGF("[GTA3Mission] INFO: " "Mission %u aborted", missionId);
    return true;
}

bool GTA3MissionSystem::HasActiveMission() const {
    return m_hasActiveMission;
}

uint32_t GTA3MissionSystem::GetActiveMissionId() const {
    return m_hasActiveMission ? m_activeMission.missionId : 0;
}

const GTAMissionState* GTA3MissionSystem::GetActiveMissionState() const {
    return m_hasActiveMission ? &m_activeMission : nullptr;
}

GTAMissionState* GTA3MissionSystem::GetActiveMissionState() {
    return m_hasActiveMission ? &m_activeMission : nullptr;
}

bool GTA3MissionSystem::AddObjective(const std::string& description, bool optional) {
    if (!m_hasActiveMission) {
        return false;
    }
    
    GTAMissionObjective obj;
    obj.description = description;
    obj.isCompleted = false;
    obj.isOptional = optional;
    obj.targetCount = 1;
    obj.currentCount = 0;
    
    m_activeMission.objectives.push_back(obj);
    
    LOGF("[GTA3Mission] DEBUG: " "Added objective: %s", description.c_str());
    return true;
}

bool GTA3MissionSystem::CompleteObjective(uint32_t objectiveIndex) {
    if (!m_hasActiveMission || objectiveIndex >= m_activeMission.objectives.size()) {
        return false;
    }
    
    m_activeMission.objectives[objectiveIndex].isCompleted = true;
    m_activeMission.currentObjectiveIndex++;
    
    LOGF("[GTA3Mission] DEBUG: " "Objective %u completed", objectiveIndex);
    
    // Check if all required objectives are complete
    bool allComplete = true;
    for (const auto& obj : m_activeMission.objectives) {
        if (!obj.isCompleted && !obj.isOptional) {
            allComplete = false;
            break;
        }
    }
    
    if (allComplete) {
        LOGF("[GTA3Mission] INFO: " "All objectives complete for mission %u", m_activeMission.missionId);
        CompleteMission(m_activeMission.missionId, true);
    }
    
    return true;
}

bool GTA3MissionSystem::UpdateObjectiveCount(uint32_t objectiveIndex, uint32_t count) {
    if (!m_hasActiveMission || objectiveIndex >= m_activeMission.objectives.size()) {
        return false;
    }
    
    m_activeMission.objectives[objectiveIndex].currentCount = count;
    
    if (count >= m_activeMission.objectives[objectiveIndex].targetCount) {
        return CompleteObjective(objectiveIndex);
    }
    
    return true;
}

uint32_t GTA3MissionSystem::GetCurrentObjectiveIndex() const {
    return m_hasActiveMission ? m_activeMission.currentObjectiveIndex : 0;
}

int32_t GTA3MissionSystem::GetObjectiveCount() const {
    return m_hasActiveMission ? static_cast<int32_t>(m_activeMission.objectives.size()) : 0;
}

void GTA3MissionSystem::SetMissionCompleted(uint32_t missionId) {
    if (missionId > 0 && missionId <= m_missions.size()) {
        m_missionCompleted[missionId - 1] = true;
        UnlockNextMissions(missionId);
    }
}

void GTA3MissionSystem::SetMissionFailed(uint32_t missionId) {
    if (missionId > 0 && missionId <= m_missions.size()) {
        m_missionFailed[missionId - 1] = true;
    }
}

bool GTA3MissionSystem::IsMissionCompleted(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return false;
    }
    return m_missionCompleted[missionId - 1];
}

bool GTA3MissionSystem::IsMissionFailed(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return false;
    }
    return m_missionFailed[missionId - 1];
}

int32_t GTA3MissionSystem::GetMissionFailures(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return 0;
    }
    return m_missionFailures[missionId - 1];
}

void GTA3MissionSystem::UnlockMission(uint32_t missionId) {
    if (missionId > 0 && missionId <= m_missions.size()) {
        m_missionUnlocked[missionId - 1] = true;
        LOGF("[GTA3Mission] DEBUG: " "Unlocked mission %u: %s", missionId, 
                  m_missions[missionId - 1].name.c_str());
    }
}

void GTA3MissionSystem::LockMission(uint32_t missionId) {
    if (missionId > 0 && missionId <= m_missions.size()) {
        m_missionUnlocked[missionId - 1] = false;
    }
}

void GTA3MissionSystem::UnlockNextMissions(uint32_t completedMissionId) {
    LOGF("[GTA3Mission] DEBUG: " "Checking for missions to unlock after %u", completedMissionId);
    
    for (size_t i = 0; i < m_missions.size(); i++) {
        if (m_missions[i].prerequisiteMission == completedMissionId) {
            UnlockMission(static_cast<uint32_t>(i + 1));
            LOGF("[GTA3Mission] INFO: " "Unlocked follow-up mission %u: %s", 
                     static_cast<uint32_t>(i + 1), m_missions[i].name.c_str());
        }
    }
}

uint32_t GTA3MissionSystem::GetTotalPlayTime() const {
    return m_totalPlayTime;
}

uint32_t GTA3MissionSystem::GetMissionPlayTime() const {
    return m_missionPlayTime;
}

uint32_t GTA3MissionSystem::GetFreeRoamPlayTime() const {
    return m_totalPlayTime - m_missionPlayTime;
}

void GTA3MissionSystem::SetMissionStartCallback(MissionStartCallback callback) {
    m_onMissionStart = std::move(callback);
}

void GTA3MissionSystem::SetMissionCompleteCallback(MissionCompleteCallback callback) {
    m_onMissionComplete = std::move(callback);
}

void GTA3MissionSystem::SetMissionUpdateCallback(MissionUpdateCallback callback) {
    m_onMissionUpdate = std::move(callback);
}

void GTA3MissionSystem::DumpMissionProgress() const {
    LOGF("[GTA3Mission] INFO: " "=== Mission Progress ===");
    LOGF("[GTA3Mission] INFO: " "Total Missions: %zu", m_missions.size());
    LOGF("[GTA3Mission] INFO: " "Completed: %d", GetCompletedMissionCount());
    LOGF("[GTA3Mission] INFO: " "Completion: %.1f%%", GetCompletionPercentage());
    
    int completed = 0;
    for (size_t i = 0; i < m_missions.size() && completed < 20; i++) {
        if (m_missionCompleted[i]) {
            LOGF("[GTA3Mission] INFO: " "  [✓] %s", m_missions[i].name.c_str());
            completed++;
        }
    }
    
    if (GetCompletedMissionCount() > 20) {
        LOGF("[GTA3Mission] INFO: " "  ... and %d more", GetCompletedMissionCount() - 20);
    }
}

void GTA3MissionSystem::DebugPrintActiveMission() const {
    if (!m_hasActiveMission) {
        LOGF("[GTA3Mission] INFO: " "No active mission");
        return;
    }
    
    const GTAMissionInfo* info = GetMissionInfo(m_activeMission.missionId);
    if (!info) return;
    
    LOGF("[GTA3Mission] INFO: " "Active Mission: %s", info->name.c_str());
    LOGF("[GTA3Mission] INFO: " "  Description: %s", info->description.c_str());
    LOGF("[GTA3Mission] INFO: " "  Type: %d", static_cast<int>(info->type));
    LOGF("[GTA3Mission] INFO: " "  Giver: %s", info->giverName.c_str());
    LOGF("[GTA3Mission] INFO: " "  Location: %s", info->locationName.c_str());
    LOGF("[GTA3Mission] INFO: " "  Objectives: %zu", m_activeMission.objectives.size());
    LOGF("[GTA3Mission] INFO: " "  Current Objective: %u", m_activeMission.currentObjectiveIndex);
    LOGF("[GTA3Mission] INFO: " "  Failures: %d", m_activeMission.failures);
}

bool GTA3MissionSystem::CheckPrerequisites(uint32_t missionId) const {
    if (missionId == 0 || missionId > m_missions.size()) {
        return false;
    }
    
    const GTAMissionInfo& mission = m_missions[missionId - 1];
    
    if (mission.prerequisiteMission == 0) {
        return true; // No prerequisites
    }
    
    // Check if prerequisite is completed
    return m_missionCompleted[mission.prerequisiteMission - 1];
}

void GTA3MissionSystem::UpdateMissionState() {
    // Called each frame to update mission timers
    if (m_hasActiveMission) {
        m_activeMission.currentTime += 0.016f; // Assume 60fps
    }
}

void GTA3MissionSystem::TriggerCallbacks() {
    // Internal callback trigger
    if (m_hasActiveMission && m_onMissionUpdate) {
        m_onMissionUpdate(m_activeMission.missionId, m_activeMission);
    }
}

} // namespace Kyty::Libs
