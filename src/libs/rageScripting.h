#pragma once

#include "common/common.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace Libs {

/**
 * @brief RAGE Scripting System HLE Implementation
 * 
 * Provides HLE stubs for Rockstar Advanced Game Engine (RAGE) scripting system.
 * Used by GTA V, Red Dead Redemption 2, and other Rockstar titles.
 * 
 * Features:
 * - Script thread management
 * - Native function stubs
 * - Mission trigger handling
 * - AI behavior stubs
 * - Radio/audio script hooks
 */
namespace RageScripting {

/**
 * @brief Script thread states
 */
enum class ScriptState : uint32_t {
    Idle        = 0,
    Running     = 1,
    Suspended   = 2,
    Killed      = 3,
    Paused      = 4,
    Completed   = 5
};

/**
 * @brief Script thread information
 */
struct ScriptThread {
    uint32_t    thread_id;
    ScriptState state;
    uint64_t    script_hash;
    std::string script_name;
    uint64_t    stack_pointer;
    uint64_t    instruction_pointer;
    uint32_t    wakeup_time;
    bool        is_mission;
    bool        is_priority;
    
    ScriptThread() 
        : thread_id(0), state(ScriptState::Idle), script_hash(0), 
          stack_pointer(0), instruction_pointer(0), wakeup_time(0),
          is_mission(false), is_priority(false) {}
};

/**
 * @brief Native function handler type
 */
using NativeHandler = uint64_t (*)(uint64_t* args, uint32_t arg_count);

/**
 * @brief Native function registration info
 */
struct NativeInfo {
    uint64_t      hash;
    std::string   name;
    NativeHandler handler;
    uint32_t      param_count;
    bool          is_stub;
};

/**
 * @brief Mission trigger information
 */
struct MissionTrigger {
    std::string mission_name;
    std::string trigger_name;
    uint64_t    trigger_hash;
    bool        is_active;
    bool        is_completed;
    uint32_t    priority;
    
    MissionTrigger() 
        : trigger_hash(0), is_active(false), is_completed(false), priority(0) {}
};

/**
 * @brief AI behavior profile
 */
struct AIBehavior {
    std::string behavior_name;
    uint64_t    behavior_hash;
    bool        is_active;
    float       aggression;
    float       accuracy;
    float       mobility;
    
    AIBehavior() 
        : behavior_hash(0), is_active(false), 
          aggression(0.5f), accuracy(0.5f), mobility(0.5f) {}
};

/**
 * @brief RAGE Scripting Manager Singleton
 */
class RageScriptingManager {
public:
    static RageScriptingManager& Instance() {
        static RageScriptingManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize scripting system
     * @return true if successful
     */
    bool Initialize();
    
    /**
     * @brief Shutdown scripting system
     */
    void Shutdown();
    
    /**
     * @brief Register a native function handler
     * @param hash Native function hash
     * @param name Native function name
     * @param handler Handler function
     * @param param_count Expected parameter count
     * @param is_stub True if this is a stub implementation
     * @return true if registered successfully
     */
    bool RegisterNative(uint64_t hash, const std::string& name, 
                        NativeHandler handler, uint32_t param_count, bool is_stub = true);
    
    /**
     * @brief Call a native function
     * @param hash Native function hash
     * @param args Function arguments
     * @param arg_count Argument count
     * @return Return value (0 if not found or stub)
     */
    uint64_t CallNative(uint64_t hash, uint64_t* args, uint32_t arg_count);
    
    /**
     * @brief Create a new script thread
     * @param script_hash Script hash
     * @param script_name Script name
     * @param is_mission True if this is a mission script
     * @return Thread ID or 0 on failure
     */
    uint32_t CreateThread(uint64_t script_hash, const std::string& script_name, 
                          bool is_mission = false);
    
    /**
     * @brief Kill a script thread
     * @param thread_id Thread ID
     * @return true if successful
     */
    bool KillThread(uint32_t thread_id);
    
    /**
     * @brief Suspend a script thread
     * @param thread_id Thread ID
     * @param wakeup_time Time to wake up (0 = indefinite)
     * @return true if successful
     */
    bool SuspendThread(uint32_t thread_id, uint32_t wakeup_time = 0);
    
    /**
     * @brief Resume a script thread
     * @param thread_id Thread ID
     * @return true if successful
     */
    bool ResumeThread(uint32_t thread_id);
    
    /**
     * @brief Get thread information
     * @param thread_id Thread ID
     * @return Thread info or nullptr if not found
     */
    const ScriptThread* GetThread(uint32_t thread_id) const;
    
    /**
     * @brief Register a mission trigger
     * @param mission_name Mission name
     * @param trigger_name Trigger name
     * @param priority Trigger priority
     * @return Trigger hash
     */
    uint64_t RegisterMissionTrigger(const std::string& mission_name, 
                                     const std::string& trigger_name,
                                     uint32_t priority = 0);
    
    /**
     * @brief Activate a mission trigger
     * @param trigger_hash Trigger hash
     * @return true if activated
     */
    bool ActivateMissionTrigger(uint64_t trigger_hash);
    
    /**
     * @brief Check if mission trigger is active
     * @param trigger_hash Trigger hash
     * @return true if active
     */
    bool IsMissionTriggerActive(uint64_t trigger_hash) const;
    
    /**
     * @brief Mark mission as completed
     * @param trigger_hash Trigger hash
     * @return true if successful
     */
    bool CompleteMission(uint64_t trigger_hash);
    
    /**
     * @brief Register AI behavior profile
     * @param behavior_name Behavior name
     * @param aggression Aggression level (0.0 - 1.0)
     * @param accuracy Accuracy level (0.0 - 1.0)
     * @param mobility Mobility level (0.0 - 1.0)
     * @return Behavior hash
     */
    uint64_t RegisterAIBehavior(const std::string& behavior_name,
                                 float aggression = 0.5f,
                                 float accuracy = 0.5f,
                                 float mobility = 0.5f);
    
    /**
     * @brief Set AI behavior for entity
     * @param entity_id Entity ID
     * @param behavior_hash Behavior hash
     * @return true if successful
     */
    bool SetAIBehavior(uint32_t entity_id, uint64_t behavior_hash);
    
    /**
     * @brief Get number of active script threads
     */
    uint32_t GetActiveThreadCount() const;
    
    /**
     * @brief Get number of registered natives
     */
    uint32_t GetNativeCount() const;
    
    /**
     * @brief Get number of stub natives
     */
    uint32_t GetStubNativeCount() const;
    
    /**
     * @brief Check if scripting system is initialized
     */
    bool IsInitialized() const { return initialized_; }

    /**
     * @brief Wrapper for CreateThread native
     */
    static uint64_t CreateThreadWrapper(uint64_t* args, uint32_t arg_count);

    /**
     * @brief Wrapper for ActivateMissionTrigger native
     */
    static uint64_t ActivateMissionTriggerWrapper(uint64_t* args, uint32_t arg_count);

private:
    RageScriptingManager() = default;
    ~RageScriptingManager();
    
    // Prevent copying
    RageScriptingManager(const RageScriptingManager&) = delete;
    RageScriptingManager& operator=(const RageScriptingManager&) = delete;
    
    /**
     * @brief Create default stub handlers for common natives
     */
    void RegisterDefaultStubs();
    
    /**
     * @brief Stub handler for common natives
     */
    static uint64_t StubNativeHandler(uint64_t* args, uint32_t arg_count);
    
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_ {false};
    
    std::unordered_map<uint64_t, NativeInfo> natives_;
    std::unordered_map<uint32_t, ScriptThread> threads_;
    std::unordered_map<uint64_t, MissionTrigger> mission_triggers_;
    std::unordered_map<uint32_t, AIBehavior> ai_behaviors_;
    
    std::atomic<uint32_t> next_thread_id_ {1};
    std::atomic<uint32_t> next_trigger_hash_ {0x1000};
    std::atomic<uint32_t> next_behavior_hash_ {0x2000};
    
    uint32_t stub_native_count_ = 0;
};

/**
 * @brief Helper macro to register native functions
 */
#define RAGE_REGISTER_NATIVE(hash, name, handler, params) \
    Libs::RageScripting::RageScriptingManager::Instance().RegisterNative( \
        hash, name, handler, params, true)

/**
 * @brief Helper macro to define a native handler
 */
#define RAGE_NATIVE_HANDLER(name) \
    uint64_t name##_handler(uint64_t* args, uint32_t arg_count)

} // namespace RageScripting

} // namespace Libs
