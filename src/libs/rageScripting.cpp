#include "libs/rageScripting.h"
#include "common/logging/log.h"
#include "common/assert.h"
#include <cstring>
#include <algorithm>

namespace Libs {

namespace RageScripting {

RageScriptingManager::~RageScriptingManager() {
    Shutdown();
}

bool RageScriptingManager::Initialize() {
    std::unique_lock lock(mutex_);
    
    if (initialized_.load(std::memory_order_acquire)) {
        LOGF_COLOR(Log::Color::Yellow, "RageScripting: Already initialized\n");
        return true;
    }
    
    LOGF("RageScripting: Initializing scripting system\n");
    
    // Register default stub handlers for common natives
    RegisterDefaultStubs();
    
    initialized_.store(true, std::memory_order_release);
    
    LOGF("RageScripting: Initialization complete (%u natives registered, %u stubs)\n",
         GetNativeCount(), GetStubNativeCount());
    
    return true;
}

void RageScriptingManager::Shutdown() {
    std::unique_lock lock(mutex_);
    
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    LOGF("RageScripting: Shutting down\n");
    
    // Kill all active threads
    for (auto& [id, thread] : threads_) {
        if (thread.state != ScriptState::Killed) {
            thread.state = ScriptState::Killed;
            LOGF("  Killed thread: %s (ID: %u)\n", 
                 thread.script_name.c_str(), id);
        }
    }
    
    threads_.clear();
    natives_.clear();
    mission_triggers_.clear();
    ai_behaviors_.clear();
    
    initialized_.store(false, std::memory_order_release);
    
    LOGF("RageScripting: Shutdown complete\n");
}

bool RageScriptingManager::RegisterNative(uint64_t hash, const std::string& name,
                                           NativeHandler handler, 
                                           uint32_t param_count, bool is_stub) {
    std::unique_lock lock(mutex_);
    
    NativeInfo info;
    info.hash = hash;
    info.name = name;
    info.handler = handler;
    info.param_count = param_count;
    info.is_stub = is_stub;
    
    natives_[hash] = info;
    
    if (is_stub) {
        stub_native_count_++;
    }
    
    LOGF("RageScripting: Registered native 0x%016llx (%s) - %s\n",
         hash, name.c_str(), is_stub ? "STUB" : "IMPLEMENTED");
    
    return true;
}

uint64_t RageScriptingManager::CallNative(uint64_t hash, uint64_t* args, uint32_t arg_count) {
    if (!initialized_.load(std::memory_order_acquire)) {
        LOGF_COLOR(Log::Color::Red, "RageScripting: Not initialized\n");
        return 0;
    }
    
    std::shared_lock lock(mutex_);
    
    auto it = natives_.find(hash);
    if (it == natives_.end()) {
        // Unknown native - log once per hash to avoid spam
        static std::unordered_map<uint64_t, uint32_t> log_counts;
        auto& count = log_counts[hash];
        
        if (count < 10) {  // Log first 10 occurrences
            LOGF_COLOR(Log::Color::Yellow,
                       "RageScripting: Unknown native 0x%016llx (args=%u)\n",
                       hash, arg_count);
            count++;
            
            if (count == 10) {
                LOGF_COLOR(Log::Color::Yellow,
                           "RageScripting: Suppressing further logs for native 0x%016llx\n", hash);
            }
        }
        
        return 0;
    }
    
    const NativeInfo& info = it->second;
    
    // Validate parameter count
    if (arg_count != info.param_count) {
        LOGF_COLOR(Log::Color::Yellow,
                   "RageScripting: Native %s parameter mismatch (expected %u, got %u)\n",
                   info.name.c_str(), info.param_count, arg_count);
    }
    
    // Call the handler
    return info.handler(args, arg_count);
}

uint32_t RageScriptingManager::CreateThread(uint64_t script_hash, const std::string& script_name,
                                             bool is_mission) {
    if (!initialized_.load(std::memory_order_acquire)) {
        LOGF_COLOR(Log::Color::Red, "RageScripting: Not initialized\n");
        return 0;
    }
    
    std::unique_lock lock(mutex_);
    
    const uint32_t thread_id = next_thread_id_.fetch_add(1, std::memory_order_relaxed);
    
    ScriptThread thread;
    thread.thread_id = thread_id;
    thread.state = ScriptState::Running;
    thread.script_hash = script_hash;
    thread.script_name = script_name;
    thread.stack_pointer = 0;
    thread.instruction_pointer = 0;
    thread.wakeup_time = 0;
    thread.is_mission = is_mission;
    thread.is_priority = is_mission;  // Missions are high priority
    
    threads_[thread_id] = thread;
    
    LOGF("RageScripting: Created thread %u (%s) - %s\n",
         thread_id, script_name.c_str(), is_mission ? "MISSION" : "SCRIPT");
    
    return thread_id;
}

bool RageScriptingManager::KillThread(uint32_t thread_id) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = threads_.find(thread_id);
    if (it == threads_.end()) {
        LOGF_COLOR(Log::Color::Yellow, "RageScripting: Thread %u not found\n", thread_id);
        return false;
    }
    
    it->second.state = ScriptState::Killed;
    
    LOGF("RageScripting: Killed thread %u (%s)\n", thread_id, it->second.script_name.c_str());
    
    return true;
}

bool RageScriptingManager::SuspendThread(uint32_t thread_id, uint32_t wakeup_time) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = threads_.find(thread_id);
    if (it == threads_.end()) {
        return false;
    }
    
    it->second.state = ScriptState::Suspended;
    it->second.wakeup_time = wakeup_time;
    
    LOGF("RageScripting: Suspended thread %u (wakeup: %u ms)\n", thread_id, wakeup_time);
    
    return true;
}

bool RageScriptingManager::ResumeThread(uint32_t thread_id) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = threads_.find(thread_id);
    if (it == threads_.end()) {
        return false;
    }
    
    if (it->second.state == ScriptState::Suspended || 
        it->second.state == ScriptState::Paused) {
        it->second.state = ScriptState::Running;
        it->second.wakeup_time = 0;
        
        LOGF("RageScripting: Resumed thread %u\n", thread_id);
        return true;
    }
    
    return false;
}

const ScriptThread* RageScriptingManager::GetThread(uint32_t thread_id) const {
    if (!initialized_.load(std::memory_order_acquire)) {
        return nullptr;
    }
    
    std::shared_lock lock(mutex_);
    
    auto it = threads_.find(thread_id);
    if (it == threads_.end()) {
        return nullptr;
    }
    
    return &it->second;
}

uint64_t RageScriptingManager::RegisterMissionTrigger(const std::string& mission_name,
                                                       const std::string& trigger_name,
                                                       uint32_t priority) {
    std::unique_lock lock(mutex_);
    
    const uint64_t trigger_hash = next_trigger_hash_.fetch_add(1, std::memory_order_relaxed);
    
    MissionTrigger trigger;
    trigger.mission_name = mission_name;
    trigger.trigger_name = trigger_name;
    trigger.trigger_hash = trigger_hash;
    trigger.is_active = false;
    trigger.is_completed = false;
    trigger.priority = priority;
    
    mission_triggers_[trigger_hash] = trigger;
    
    LOGF("RageScripting: Registered mission trigger '%s' -> '%s' (hash: 0x%016llx)\n",
         mission_name.c_str(), trigger_name.c_str(), trigger_hash);
    
    return trigger_hash;
}

bool RageScriptingManager::ActivateMissionTrigger(uint64_t trigger_hash) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = mission_triggers_.find(trigger_hash);
    if (it == mission_triggers_.end()) {
        return false;
    }
    
    it->second.is_active = true;
    
    LOGF("RageScripting: Activated mission trigger '%s' (%s)\n",
         it->second.mission_name.c_str(), it->second.trigger_name.c_str());
    
    return true;
}

bool RageScriptingManager::IsMissionTriggerActive(uint64_t trigger_hash) const {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::shared_lock lock(mutex_);
    
    auto it = mission_triggers_.find(trigger_hash);
    if (it == mission_triggers_.end()) {
        return false;
    }
    
    return it->second.is_active;
}

bool RageScriptingManager::CompleteMission(uint64_t trigger_hash) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = mission_triggers_.find(trigger_hash);
    if (it == mission_triggers_.end()) {
        return false;
    }
    
    it->second.is_completed = true;
    it->second.is_active = false;
    
    LOGF("RageScripting: Completed mission '%s' (%s)\n",
         it->second.mission_name.c_str(), it->second.trigger_name.c_str());
    
    return true;
}

uint64_t RageScriptingManager::RegisterAIBehavior(const std::string& behavior_name,
                                                   float aggression,
                                                   float accuracy,
                                                   float mobility) {
    std::unique_lock lock(mutex_);
    
    const uint64_t behavior_hash = next_behavior_hash_.fetch_add(1, std::memory_order_relaxed);
    
    AIBehavior behavior;
    behavior.behavior_name = behavior_name;
    behavior.behavior_hash = behavior_hash;
    behavior.is_active = false;
    behavior.aggression = std::clamp(aggression, 0.0f, 1.0f);
    behavior.accuracy = std::clamp(accuracy, 0.0f, 1.0f);
    behavior.mobility = std::clamp(mobility, 0.0f, 1.0f);
    
    ai_behaviors_[static_cast<uint32_t>(behavior_hash)] = behavior;
    
    LOGF("RageScripting: Registered AI behavior '%s' (agg=%.2f, acc=%.2f, mob=%.2f)\n",
         behavior_name.c_str(), aggression, accuracy, mobility);
    
    return behavior_hash;
}

bool RageScriptingManager::SetAIBehavior(uint32_t entity_id, uint64_t behavior_hash) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = ai_behaviors_.find(static_cast<uint32_t>(behavior_hash));
    if (it == ai_behaviors_.end()) {
        return false;
    }
    
    it->second.is_active = true;
    
    LOGF("RageScripting: Set AI behavior for entity %u: %s\n",
         entity_id, it->second.behavior_name.c_str());
    
    return true;
}

uint32_t RageScriptingManager::GetActiveThreadCount() const {
    if (!initialized_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    std::shared_lock lock(mutex_);
    
    uint32_t count = 0;
    for (const auto& [id, thread] : threads_) {
        if (thread.state == ScriptState::Running || 
            thread.state == ScriptState::Suspended) {
            count++;
        }
    }
    
    return count;
}

uint32_t RageScriptingManager::GetNativeCount() const {
    std::shared_lock lock(mutex_);
    return static_cast<uint32_t>(natives_.size());
}

uint32_t RageScriptingManager::GetStubNativeCount() const {
    return stub_native_count_;
}

// Default stub handlers for common RAGE natives

uint64_t RageScriptingManager::StubNativeHandler(uint64_t* args, uint32_t arg_count) {
    // Generic stub - returns 0 or 1 depending on expected return type
    // Most natives return 0 (false/nullptr) or 1 (true) for success
    return 1;  // Default to success
}

void RageScriptingManager::RegisterDefaultStubs() {
    LOGF("RageScripting: Registering default stub handlers\n");
    
    // Common GTA V natives (hashes are examples - real hashes would come from game)
    // These are stub implementations that return success but don't actually do anything
    
    // Script control natives
    RegisterNative(0x7C35800000000001ULL, "TERMINATE_THIS_SCRIPT", StubNativeHandler, 0, true);
    RegisterNative(0x7C35800000000002ULL, "WAIT", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000003ULL, "START_NEW_SCRIPT", CreateThreadWrapper, 2, true);
    RegisterNative(0x7C35800000000004ULL, "GET_THREAD_ID", StubNativeHandler, 0, true);
    
    // Entity natives
    RegisterNative(0x7C35800000000010ULL, "CREATE_ENTITY", StubNativeHandler, 3, true);
    RegisterNative(0x7C35800000000011ULL, "DELETE_ENTITY", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000012ULL, "DOES_ENTITY_EXIST", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000013ULL, "SET_ENTITY_COORDS", StubNativeHandler, 4, true);
    RegisterNative(0x7C35800000000014ULL, "GET_ENTITY_COORDS", StubNativeHandler, 1, true);
    
    // Ped (character) natives
    RegisterNative(0x7C35800000000020ULL, "CREATE_PED", StubNativeHandler, 4, true);
    RegisterNative(0x7C35800000000021ULL, "IS_PED_INJURED", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000022ULL, "SET_PED_HEALTH", StubNativeHandler, 2, true);
    RegisterNative(0x7C35800000000023ULL, "GET_PED_HEALTH", StubNativeHandler, 1, true);
    
    // Vehicle natives
    RegisterNative(0x7C35800000000030ULL, "CREATE_VEHICLE", StubNativeHandler, 4, true);
    RegisterNative(0x7C35800000000031ULL, "SET_VEHICLE_ENGINE_ON", StubNativeHandler, 3, true);
    RegisterNative(0x7C35800000000032ULL, "SET_VEHICLE_FORWARD_SPEED", StubNativeHandler, 2, true);
    
    // Mission natives
    RegisterNative(0x7C35800000000040ULL, "SET_MISSION_FLAG", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000041ULL, "HAS_MISSION_COMPLETED", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000042ULL, "SET_MISSION_TRIGGER", ActivateMissionTriggerWrapper, 1, true);
    
    // AI natives
    RegisterNative(0x7C35800000000050ULL, "TASK_GO_TO_COORD", StubNativeHandler, 3, true);
    RegisterNative(0x7C35800000000051ULL, "TASK_FOLLOW_NAV_MESH", StubNativeHandler, 4, true);
    RegisterNative(0x7C35800000000052ULL, "TASK_COMBAT_PED", StubNativeHandler, 2, true);
    RegisterNative(0x7C35800000000053ULL, "CLEAR_PED_TASKS", StubNativeHandler, 1, true);
    
    // Audio natives
    RegisterNative(0x7C35800000000060ULL, "PLAY_SOUND", StubNativeHandler, 2, true);
    RegisterNative(0x7C35800000000061ULL, "STOP_SOUND", StubNativeHandler, 1, true);
    RegisterNative(0x7C35800000000062ULL, "SET_RADIO_STATION", StubNativeHandler, 1, true);
    
    // World natives
    RegisterNative(0x7C35800000000070ULL, "GET_GROUND_Z", StubNativeHandler, 3, true);
    RegisterNative(0x7C35800000000071ULL, "IS_POINT_OBSCURED_BY_A_MISSION_ENTITY", StubNativeHandler, 4, true);
    
    LOGF("RageScripting: Registered %u default stub natives\n", stub_native_count_);
}

// Wrapper functions for natives that need special handling

uint64_t RageScriptingManager::CreateThreadWrapper(uint64_t* args, uint32_t arg_count) {
    if (arg_count < 2) {
        return 0;
    }
    
    const uint64_t script_hash = args[0];
    const char* script_name = reinterpret_cast<const char*>(args[1]);
    
    const uint32_t thread_id = Instance().CreateThread(script_hash, 
                                                        script_name ? script_name : "unknown",
                                                        false);
    
    return thread_id;
}

uint64_t RageScriptingManager::ActivateMissionTriggerWrapper(uint64_t* args, uint32_t arg_count) {
    if (arg_count < 1) {
        return 0;
    }
    
    const uint64_t trigger_hash = args[0];
    return Instance().ActivateMissionTrigger(trigger_hash) ? 1 : 0;
}

} // namespace RageScripting

} // namespace Libs
