#include "ue4Animation.h"
#include "ue4HLE.h"
#include "common/logging.hpp"
#include <algorithm>
#include <cmath>

namespace Kyty::Libs {

using namespace Kyty::Common;

//=============================================================================
// AnimationInstance Internal Structure
//=============================================================================

struct UE4Animation::AnimationInstance {
    std::string animClass;
    std::string currentState;
    std::string currentMontage;
    float currentTime;
    float currentMontageTime;
    float playRate;
    bool isPlaying;
    bool isMontagePlaying;
    std::unordered_map<std::string, float> blendParams;
    std::vector<AnimNotifyDesc> pendingNotifies;
    
    AnimationInstance() : currentTime(0.0f), currentMontageTime(0.0f), 
                          playRate(1.0f), isPlaying(false), isMontagePlaying(false) {}
};

//=============================================================================
// UE4Animation Implementation
//=============================================================================

UE4Animation& UE4Animation::Instance() {
    static UE4Animation instance;
    return instance;
}

bool UE4Animation::Initialize() {
    if (m_initialized) {
        Log::Warning("[UE4Anim] Already initialized");
        return true;
    }
    
    Log::Info("[UE4Anim] Initializing animation system...");
    
    // Register all animation categories
    RegisterGTA3Animations();
    RegisterCharacterAnimations();
    RegisterVehicleAnimations();
    RegisterWeaponAnimations();
    
    m_initialized = true;
    
    Log::Info("[UE4Anim] Animation system initialized (%zu states registered)", m_states.size());
    return true;
}

void UE4Animation::Shutdown() {
    Log::Info("[UE4Anim] Shutting down animation system...");
    
    m_instances.clear();
    m_states.clear();
    m_transitions.clear();
    m_nextInstanceId = 1;
    m_initialized = false;
    
    Log::Info("[UE4Anim] Animation system shut down");
}

bool UE4Animation::RegisterAnimation(const std::string& name, float duration, size_t boneCount) {
    // Create a default state for this animation
    AnimStateDesc desc;
    desc.name = name;
    desc.animationSequence = name;
    desc.looping = true;
    desc.playRate = 1.0f;
    
    // Add basic notifies based on duration
    if (duration > 0.5f) {
        AnimNotifyDesc footstep;
        footstep.name = "Footstep";
        footstep.type = EAnimNotifyType::PlaySound;
        footstep.time = duration * 0.25f;
        footstep.duration = 0.1f;
        desc.notifies.push_back(footstep);
        
        AnimNotifyDesc footstep2;
        footstep2.name = "Footstep2";
        footstep2.type = EAnimNotifyType::PlaySound;
        footstep2.time = duration * 0.75f;
        footstep2.duration = 0.1f;
        desc.notifies.push_back(footstep2);
    }
    
    m_states[name] = desc;
    Log::Debug("[UE4Anim] Registered animation: %s (duration=%.2fs, bones=%zu)", 
               name.c_str(), duration, boneCount);
    
    return true;
}

bool UE4Animation::RegisterState(const std::string& stateName, const AnimStateDesc& desc) {
    if (m_states.find(stateName) != m_states.end()) {
        Log::Warning("[UE4Anim] State already registered: %s", stateName.c_str());
        return false;
    }
    
    m_states[stateName] = desc;
    Log::Debug("[UE4Anim] Registered state: %s (type=%d, loop=%d)", 
               stateName.c_str(), static_cast<int>(desc.stateType), desc.looping);
    
    return true;
}

bool UE4Animation::RegisterTransition(const AnimTransitionRule& rule) {
    m_transitions.push_back(rule);
    Log::Debug("[UE4Anim] Registered transition: %s -> %s (blend=%.2fs)", 
               rule.fromState.c_str(), rule.toState.c_str(), rule.blendTime);
    
    return true;
}

uint32_t UE4Animation::CreateInstance(const std::string& animClass) {
    if (!m_initialized) {
        Log::Warning("[UE4Anim] Cannot create instance: not initialized");
        return 0;
    }
    
    auto instance = std::make_unique<AnimationInstance>();
    instance->animClass = animClass;
    instance->currentState = "Idle";
    instance->isPlaying = true;
    
    uint32_t instanceId = m_nextInstanceId++;
    m_instances[std::to_string(instanceId)] = std::move(instance);
    
    Log::Debug("[UE4Anim] Created animation instance %u (class=%s)", instanceId, animClass.c_str());
    return instanceId;
}

void UE4Animation::DestroyInstance(uint32_t instanceId) {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it != m_instances.end()) {
        m_instances.erase(it);
        Log::Debug("[UE4Anim] Destroyed animation instance %u", instanceId);
    }
}

bool UE4Animation::UpdateAnimation(uint32_t instanceId, float deltaTime,
                                    const std::unordered_map<std::string, float>& blendParams) {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return false;
    }
    
    auto& instance = it->second;
    
    // Update blend parameters
    for (const auto& [param, value] : blendParams) {
        instance->blendParams[param] = value;
    }
    
    // Update current animation time
    if (instance->isPlaying && !instance->isMontagePlaying) {
        auto stateIt = m_states.find(instance->currentState);
        if (stateIt != m_states.end()) {
            instance->currentTime += deltaTime * instance->playRate;
            
            // Handle looping
            if (stateIt->second.looping) {
                float duration = 1.0f; // Default duration
                if (instance->currentTime >= duration) {
                    instance->currentTime = std::fmod(instance->currentTime, duration);
                }
            }
            
            // Process notifies
            ProcessNotifies(instanceId, instance->currentTime);
        }
    }
    
    // Update montage time
    if (instance->isMontagePlaying) {
        instance->currentMontageTime += deltaTime * instance->playRate;
        
        // Check if montage is finished
        if (instance->currentMontageTime >= 1.0f) { // Default montage duration
            instance->isMontagePlaying = false;
            instance->currentMontage.clear();
            instance->currentMontageTime = 0.0f;
        }
    }
    
    return true;
}

bool UE4Animation::PlayMontage(uint32_t instanceId, const std::string& montageName, float playRate) {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return false;
    }
    
    auto& instance = it->second;
    instance->currentMontage = montageName;
    instance->currentMontageTime = 0.0f;
    instance->playRate = playRate;
    instance->isMontagePlaying = true;
    
    Log::Debug("[UE4Anim] Playing montage %u: %s (rate=%.2f)", instanceId, montageName.c_str(), playRate);
    
    // Add montage-specific notifies
    AnimNotifyDesc startNotify;
    startNotify.name = "MontageStart";
    startNotify.type = EAnimNotifyType::Custom;
    startNotify.time = 0.0f;
    instance->pendingNotifies.push_back(startNotify);
    
    return true;
}

void UE4Animation::StopMontage(uint32_t instanceId, float blendOutTime) {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return;
    }
    
    auto& instance = it->second;
    
    if (instance->isMontagePlaying) {
        // Add blend out notify
        AnimNotifyDesc endNotify;
        endNotify.name = "MontageEnd";
        endNotify.type = EAnimNotifyType::Custom;
        endNotify.time = blendOutTime;
        instance->pendingNotifies.push_back(endNotify);
        
        instance->isMontagePlaying = false;
        instance->currentMontage.clear();
        instance->currentMontageTime = 0.0f;
        
        Log::Debug("[UE4Anim] Stopped montage %u (blend=%.2fs)", instanceId, blendOutTime);
    }
}

void UE4Animation::SetBlendParameter(uint32_t instanceId, const std::string& paramName, float value) {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return;
    }
    
    it->second->blendParams[paramName] = value;
}

std::string UE4Animation::GetCurrentState(uint32_t instanceId) const {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return "";
    }
    
    return it->second->currentState;
}

bool UE4Animation::IsPlaying(uint32_t instanceId) const {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return false;
    }
    
    return it->second->isPlaying;
}

size_t UE4Animation::GetInstanceCount() const {
    return m_instances.size();
}

void UE4Animation::ProcessNotifies(uint32_t instanceId, float currentTime) {
    std::string idStr = std::to_string(instanceId);
    auto it = m_instances.find(idStr);
    if (it == m_instances.end()) {
        return;
    }
    
    auto& instance = it->second;
    
    // Process pending notifies
    auto notifyIt = instance->pendingNotifies.begin();
    while (notifyIt != instance->pendingNotifies.end()) {
        if (currentTime >= notifyIt->time) {
            Log::Debug("[UE4Anim] Triggering notify: %s (type=%d)", 
                      notifyIt->name.c_str(), static_cast<int>(notifyIt->type));
            
            // Notify processing would go here (sound, particles, etc.)
            
            notifyIt = instance->pendingNotifies.erase(notifyIt);
        } else {
            ++notifyIt;
        }
    }
    
    // Check state notifies
    auto stateIt = m_states.find(instance->currentState);
    if (stateIt != m_states.end()) {
        for (const auto& notify : stateIt->second.notifies) {
            float notifyTime = std::fmod(currentTime, 1.0f);
            if (std::abs(notifyTime - notify.time) < 0.016f) { // Within one frame
                Log::Debug("[UE4Anim] State notify: %s", notify.name.c_str());
            }
        }
    }
}

//=============================================================================
// GTA 3 DE Animation Registration
//=============================================================================

void RegisterGTA3Animations() {
    auto& anim = UE4Animation::Instance();
    
    // Core character animations
    anim.RegisterAnimation("Idle", 2.0f, 65);
    anim.RegisterAnimation("Walk_Fwd", 1.2f, 65);
    anim.RegisterAnimation("Run_Fwd", 0.8f, 65);
    anim.RegisterAnimation("Sprint", 0.6f, 65);
    anim.RegisterAnimation("Jump_Start", 0.5f, 65);
    anim.RegisterAnimation("Jump_Loop", 1.0f, 65);
    anim.RegisterAnimation("Jump_Land", 0.4f, 65);
    anim.RegisterAnimation("Crouch_Walk", 1.5f, 65);
    anim.RegisterAnimation("Crouch_Run", 1.0f, 65);
    
    // Combat animations
    anim.RegisterAnimation("Punch_Left", 0.6f, 65);
    anim.RegisterAnimation("Punch_Right", 0.6f, 65);
    anim.RegisterAnimation("Kick", 0.8f, 65);
    anim.RegisterAnimation("Block", 0.3f, 65);
    anim.RegisterAnimation("Hit_Front", 0.4f, 65);
    anim.RegisterAnimation("Hit_Back", 0.4f, 65);
    anim.RegisterAnimation("Death_Front", 1.2f, 65);
    anim.RegisterAnimation("Death_Back", 1.2f, 65);
    
    // Vehicle animations
    anim.RegisterAnimation("Vehicle_Enter_Driver", 1.5f, 65);
    anim.RegisterAnimation("Vehicle_Enter_Passenger", 1.5f, 65);
    anim.RegisterAnimation("Vehicle_Exit_Driver", 1.2f, 65);
    anim.RegisterAnimation("Vehicle_Exit_Passenger", 1.2f, 65);
    anim.RegisterAnimation("Vehicle_Drive_Idle", 2.0f, 65);
    anim.RegisterAnimation("Vehicle_Drive_Forward", 1.0f, 65);
    anim.RegisterAnimation("Vehicle_Drive_Backward", 1.0f, 65);
    anim.RegisterAnimation("Vehicle_Drive_Left", 0.8f, 65);
    anim.RegisterAnimation("Vehicle_Drive_Right", 0.8f, 65);
    
    Log::Info("[UE4Anim] Registered GTA 3 animations");
}

void RegisterCharacterAnimations() {
    auto& anim = UE4Animation::Instance();
    
    // Movement states
    AnimStateDesc idle;
    idle.name = "Idle";
    idle.stateType = EAnimStateType::Idle;
    idle.animationSequence = "Idle";
    idle.looping = true;
    anim.RegisterState("Idle", idle);
    
    AnimStateDesc walk;
    walk.name = "Walk";
    walk.stateType = EAnimStateType::Walk;
    walk.animationSequence = "Walk_Fwd";
    walk.looping = true;
    walk.blendInTime = 0.2f;
    anim.RegisterState("Walk", walk);
    
    AnimStateDesc run;
    run.name = "Run";
    run.stateType = EAnimStateType::Run;
    run.animationSequence = "Run_Fwd";
    run.looping = true;
    run.blendInTime = 0.15f;
    anim.RegisterState("Run", run);
    
    AnimStateDesc sprint;
    sprint.name = "Sprint";
    sprint.stateType = EAnimStateType::Sprint;
    sprint.animationSequence = "Sprint";
    sprint.looping = true;
    sprint.blendInTime = 0.1f;
    anim.RegisterState("Sprint", sprint);
    
    // Jump states
    AnimStateDesc jump;
    jump.name = "Jump";
    jump.stateType = EAnimStateType::Jump;
    jump.animationSequence = "Jump_Start";
    jump.looping = false;
    jump.blendInTime = 0.1f;
    jump.blendOutTime = 0.1f;
    anim.RegisterState("Jump", jump);
    
    AnimStateDesc fall;
    fall.name = "Fall";
    fall.stateType = EAnimStateType::Fall;
    fall.animationSequence = "Jump_Loop";
    fall.looping = true;
    anim.RegisterState("Fall", fall);
    
    AnimStateDesc land;
    land.name = "Land";
    land.stateType = EAnimStateType::Land;
    land.animationSequence = "Jump_Land";
    land.looping = false;
    land.blendInTime = 0.05f;
    land.blendOutTime = 0.2f;
    anim.RegisterState("Land", land);
    
    // Crouch states
    AnimStateDesc crouch;
    crouch.name = "Crouch";
    crouch.stateType = EAnimStateType::Crouch;
    crouch.animationSequence = "Crouch_Walk";
    crouch.looping = true;
    anim.RegisterState("Crouch", crouch);
    
    // Death states
    AnimStateDesc death;
    death.name = "Death";
    death.stateType = EAnimStateType::Death;
    death.animationSequence = "Death_Front";
    death.looping = false;
    death.blendInTime = 0.1f;
    anim.RegisterState("Death", death);
    
    // Register transitions
    AnimTransitionRule idleToWalk;
    idleToWalk.fromState = "Idle";
    idleToWalk.toState = "Walk";
    idleToWalk.conditionExpression = "Speed > 0.1";
    idleToWalk.blendTime = 0.2f;
    anim.RegisterTransition(idleToWalk);
    
    AnimTransitionRule walkToRun;
    walkToRun.fromState = "Walk";
    walkToRun.toState = "Run";
    walkToRun.conditionExpression = "Speed > 0.5";
    walkToRun.blendTime = 0.15f;
    anim.RegisterTransition(walkToRun);
    
    AnimTransitionRule runToSprint;
    runToSprint.fromState = "Run";
    runToSprint.toState = "Sprint";
    runToSprint.conditionExpression = "Speed > 0.8 && bSprintPressed";
    runToSprint.blendTime = 0.1f;
    anim.RegisterTransition(runToSprint);
    
    Log::Info("[UE4Anim] Registered character animation states");
}

void RegisterVehicleAnimations() {
    auto& anim = UE4Animation::Instance();
    
    // Vehicle entry/exit
    AnimStateDesc vehicleEnter;
    vehicleEnter.name = "VehicleEnter";
    vehicleEnter.stateType = EAnimStateType::VehicleEnter;
    vehicleEnter.animationSequence = "Vehicle_Enter_Driver";
    vehicleEnter.looping = false;
    vehicleEnter.blendInTime = 0.1f;
    vehicleEnter.blendOutTime = 0.2f;
    anim.RegisterState("VehicleEnter", vehicleEnter);
    
    AnimStateDesc vehicleExit;
    vehicleExit.name = "VehicleExit";
    vehicleExit.stateType = EAnimStateType::VehicleExit;
    vehicleExit.animationSequence = "Vehicle_Exit_Driver";
    vehicleExit.looping = false;
    vehicleExit.blendInTime = 0.1f;
    vehicleExit.blendOutTime = 0.2f;
    anim.RegisterState("VehicleExit", vehicleExit);
    
    // Vehicle driving
    AnimStateDesc vehicleDrive;
    vehicleDrive.name = "VehicleDrive";
    vehicleDrive.stateType = EAnimStateType::VehicleDrive;
    vehicleDrive.animationSequence = "Vehicle_Drive_Idle";
    vehicleDrive.looping = true;
    anim.RegisterState("VehicleDrive", vehicleDrive);
    
    // Vehicle passenger
    AnimStateDesc vehiclePassenger;
    vehiclePassenger.name = "VehiclePassenger";
    vehiclePassenger.stateType = EAnimStateType::VehiclePassenger;
    vehiclePassenger.animationSequence = "Vehicle_Drive_Idle";
    vehiclePassenger.looping = true;
    anim.RegisterState("VehiclePassenger", vehiclePassenger);
    
    Log::Info("[UE4Anim] Registered vehicle animation states");
}

void RegisterWeaponAnimations() {
    auto& anim = UE4Animation::Instance();
    
    // Weapon firing
    AnimStateDesc weaponFire;
    weaponFire.name = "WeaponFire";
    weaponFire.stateType = EAnimStateType::WeaponFire;
    weaponFire.animationSequence = "Punch_Right"; // Placeholder
    weaponFire.looping = false;
    weaponFire.blendInTime = 0.05f;
    weaponFire.blendOutTime = 0.1f;
    anim.RegisterState("WeaponFire", weaponFire);
    
    // Reload
    AnimStateDesc reload;
    reload.name = "Reload";
    reload.stateType = EAnimStateType::Reload;
    reload.animationSequence = "Punch_Left"; // Placeholder
    reload.looping = false;
    reload.blendInTime = 0.1f;
    reload.blendOutTime = 0.2f;
    anim.RegisterState("Reload", reload);
    
    // Melee
    AnimStateDesc melee;
    melee.name = "Melee";
    melee.stateType = EAnimStateType::Melee;
    melee.animationSequence = "Punch_Right";
    melee.looping = false;
    melee.blendInTime = 0.05f;
    melee.blendOutTime = 0.1f;
    anim.RegisterState("Melee", melee);
    
    Log::Info("[UE4Anim] Registered weapon animation states");
}

} // namespace Kyty::Libs
