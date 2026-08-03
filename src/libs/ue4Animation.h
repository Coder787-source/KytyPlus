#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Kyty::Libs {

/**
 * UE4 Animation System HLE for GTA 3 Definitive Edition
 * 
 * This module provides high-level emulation of UE4's animation system,
 * including animation blueprints, montages, blend spaces, and state machines.
 * 
 * Covers: UAnimInstance, UAnimBlueprint, UAnimMontage, UBlendSpace,
 *         UAnimStateMachine, UAnimGraph, and related classes.
 */

// Animation montage notify types
enum class EAnimNotifyType : uint8_t {
    None = 0,
    PlaySound = 1,
    PlayParticleEffect = 2,
    TriggerAnimation = 3,
    EnablePhysics = 4,
    DisablePhysics = 5,
    SpawnActor = 6,
    DamageTarget = 7,
    PlayCameraShake = 8,
    SetMaterialParameter = 9,
    TriggerRumble = 10,
    Custom = 255
};

// Animation blend modes
enum class EAnimBlendMode : uint8_t {
    BlendBySpace = 0,
    BlendByBone = 1,
    BlendByMask = 2,
    BlendByWeight = 3
};

// Animation state types
enum class EAnimStateType : uint8_t {
    Idle = 0,
    Walk = 1,
    Run = 2,
    Sprint = 3,
    Jump = 4,
    Fall = 5,
    Land = 6,
    Crouch = 7,
    Prone = 8,
    Swim = 9,
    Climb = 10,
    Cover = 11,
    Melee = 12,
    Reload = 13,
    WeaponFire = 14,
    VehicleEnter = 15,
    VehicleExit = 16,
    VehicleDrive = 17,
    VehiclePassenger = 18,
    Death = 19,
    Knockout = 20,
    Custom = 255
};

// Forward declarations
class UAnimInstance;
class UAnimSequence;
class UAnimMontage;
class UBlendSpace;
class UAnimStateMachine;

/**
 * Animation Notify Descriptor
 */
struct AnimNotifyDesc {
    std::string name;
    EAnimNotifyType type;
    float time;
    float duration;
    std::unordered_map<std::string, float> parameters;
    std::unordered_map<std::string, std::string> stringParameters;
    
    AnimNotifyDesc() : type(EAnimNotifyType::None), time(0.0f), duration(0.0f) {}
};

/**
 * Animation State Descriptor
 */
struct AnimStateDesc {
    std::string name;
    EAnimStateType stateType;
    std::string animationSequence;
    float blendInTime;
    float blendOutTime;
    bool looping;
    float playRate;
    std::vector<AnimNotifyDesc> notifies;
    
    AnimStateDesc() : stateType(EAnimStateType::Idle), blendInTime(0.1f), 
                      blendOutTime(0.1f), looping(true), playRate(1.0f) {}
};

/**
 * Animation Transition Rule
 */
struct AnimTransitionRule {
    std::string fromState;
    std::string toState;
    std::string conditionExpression;
    float blendTime;
    bool interruptible;
    
    AnimTransitionRule() : blendTime(0.2f), interruptible(true) {}
};

/**
 * Animation Blend Parameter
 */
struct AnimBlendParam {
    std::string name;
    float value;
    float minValue;
    float maxValue;
    bool isDirectional;
    
    AnimBlendParam() : value(0.0f), minValue(0.0f), maxValue(1.0f), isDirectional(false) {}
};

/**
 * UE4 Animation Instance HLE
 */
class UE4Animation {
public:
    static UE4Animation& Instance();
    
    /**
     * Initialize animation system
     * @return true if initialization succeeded
     */
    bool Initialize();
    
    /**
     * Shutdown animation system
     */
    void Shutdown();
    
    /**
     * Register an animation sequence
     * @param name Name of the animation
     * @param duration Duration in seconds
     * @param boneCount Number of bones in the skeleton
     * @return true if registration succeeded
     */
    bool RegisterAnimation(const std::string& name, float duration, size_t boneCount = 0);
    
    /**
     * Register an animation state
     * @param stateName Name of the state
     * @param desc State descriptor
     * @return true if registration succeeded
     */
    bool RegisterState(const std::string& stateName, const AnimStateDesc& desc);
    
    /**
     * Register a transition rule
     * @param rule Transition rule
     * @return true if registration succeeded
     */
    bool RegisterTransition(const AnimTransitionRule& rule);
    
    /**
     * Update animation instance
     * @param instanceId ID of the animation instance
     * @param deltaTime Time since last update
     * @param blendParams Blend parameters
     * @return true if update succeeded
     */
    bool UpdateAnimation(uint32_t instanceId, float deltaTime, 
                         const std::unordered_map<std::string, float>& blendParams);
    
    /**
     * Play an animation montage
     * @param instanceId ID of the animation instance
     * @param montageName Name of the montage
     * @param playRate Playback rate
     * @return true if playback started
     */
    bool PlayMontage(uint32_t instanceId, const std::string& montageName, float playRate = 1.0f);
    
    /**
     * Stop current montage
     * @param instanceId ID of the animation instance
     * @param blendOutTime Blend out time
     */
    void StopMontage(uint32_t instanceId, float blendOutTime = 0.2f);
    
    /**
     * Set animation blend parameter
     * @param instanceId ID of the animation instance
     * @param paramName Name of the parameter
     * @param value Parameter value
     */
    void SetBlendParameter(uint32_t instanceId, const std::string& paramName, float value);
    
    /**
     * Get current animation state
     * @param instanceId ID of the animation instance
     * @return Current state name
     */
    std::string GetCurrentState(uint32_t instanceId) const;
    
    /**
     * Check if animation is playing
     * @param instanceId ID of the animation instance
     * @return true if animation is playing
     */
    bool IsPlaying(uint32_t instanceId) const;
    
    /**
     * Create a new animation instance
     * @param animClass Animation class name
     * @return Instance ID, or 0 if failed
     */
    uint32_t CreateInstance(const std::string& animClass);
    
    /**
     * Destroy an animation instance
     * @param instanceId ID of the instance to destroy
     */
    void DestroyInstance(uint32_t instanceId);
    
    /**
     * Get the number of active instances
     * @return Number of active instances
     */
    size_t GetInstanceCount() const;
    
    /**
     * Process animation notifies
     * @param instanceId ID of the animation instance
     * @param currentTime Current animation time
     */
    void ProcessNotifies(uint32_t instanceId, float currentTime);
    
private:
    UE4Animation() = default;
    ~UE4Animation() = default;
    
    UE4Animation(const UE4Animation&) = delete;
    UE4Animation& operator=(const UE4Animation&) = delete;
    
    struct AnimationInstance;
    
    std::unordered_map<std::string, std::unique_ptr<AnimationInstance>> m_instances;
    std::unordered_map<std::string, AnimStateDesc> m_states;
    std::vector<AnimTransitionRule> m_transitions;
    uint32_t m_nextInstanceId = 1;
    bool m_initialized = false;
};

/**
 * Register all GTA 3 DE specific animations
 */
void RegisterGTA3Animations();

/**
 * Register all character animations
 */
void RegisterCharacterAnimations();

/**
 * Register all vehicle animations
 */
void RegisterVehicleAnimations();

/**
 * Register all weapon animations
 */
void RegisterWeaponAnimations();

} // namespace Kyty::Libs
