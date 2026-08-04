#include "ue4ClassesExtended.h"
#include "ue4HLE.h"
#include "common/logging/log.h"
#include <algorithm>

namespace Kyty::Libs {

using namespace Common;

//=============================================================================
// UE4ClassesExtended Implementation
//=============================================================================

UE4ClassesExtended& UE4ClassesExtended::Instance() {
    static UE4ClassesExtended instance;
    return instance;
}

bool UE4ClassesExtended::RegisterClass(const std::string& className,
                                        const std::string& superClassName,
                                        uint32_t classSize,
                                        uint32_t flags) {
    if (m_classes.find(className) != m_classes.end()) {
        LOGF("[UE4] Class already registered: %s", className.c_str());
        return false;
    }
    
    auto entry = std::make_unique<UE4ClassEntry>();
    entry->className = className;
    entry->superClassName = superClassName;
    entry->classSize = classSize;
    entry->classFlags = flags;
    
    m_classes[className] = std::move(entry);
    LOGF("[UE4] Registered class: %s (size=%u, parent=%s)", 
               className.c_str(), classSize, superClassName.c_str());
    
    return true;
}

bool UE4ClassesExtended::RegisterProperty(const std::string& className,
                                           const std::string& propertyName,
                                           const std::string& propertyType,
                                           size_t offset,
                                           size_t size,
                                           uint64_t flags) {
    auto it = m_classes.find(className);
    if (it == m_classes.end()) {
        LOGF("[UE4] Cannot register property on unregistered class: %s", className.c_str());
        return false;
    }
    
    auto prop = std::make_unique<UProperty>();
    // Property registration stub - actual implementation would set up property metadata
    
    it->second->properties[propertyName] = prop.release();
    LOGF("[UE4] Registered property: %s.%s (type=%s, offset=%zu)", 
               className.c_str(), propertyName.c_str(), propertyType.c_str(), offset);
    
    return true;
}

bool UE4ClassesExtended::RegisterFunction(const std::string& className,
                                           const std::string& functionName,
                                           std::function<void*()> stub) {
    auto it = m_classes.find(className);
    if (it == m_classes.end()) {
        LOGF("[UE4] Cannot register function on unregistered class: %s", className.c_str());
        return false;
    }
    
    it->second->functionStubs[functionName] = std::move(stub);
    LOGF("[UE4] Registered function: %s::%s", className.c_str(), functionName.c_str());
    
    return true;
}

UE4ClassEntry* UE4ClassesExtended::FindClass(const std::string& className) {
    auto it = m_classes.find(className);
    if (it != m_classes.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool UE4ClassesExtended::IsClassRegistered(const std::string& className) const {
    return m_classes.find(className) != m_classes.end();
}

size_t UE4ClassesExtended::GetClassCount() const {
    return m_classes.size();
}

bool UE4ClassesExtended::Initialize() {
    if (m_initialized) {
        LOGF("[UE4] Classes already initialized");
        return true;
    }
    
    LOGF("[UE4] Initializing extended class registry...");
    
    // Register all class categories
    RegisterActorClasses();
    RegisterComponentClasses();
    RegisterGameplayClasses();
    RegisterMissionClasses();
    
    m_initialized = true;
    
    LOGF("[UE4] Extended class registry initialized (%zu classes)", m_classes.size());
    return true;
}

void UE4ClassesExtended::Shutdown() {
    LOGF("[UE4] Shutting down extended class registry...");
    
    // Clean up properties
    for (auto& [className, entry] : m_classes) {
        for (auto& [propName, prop] : entry->properties) {
            delete prop;
        }
        entry->properties.clear();
        entry->functionStubs.clear();
    }
    
    m_classes.clear();
    m_initialized = false;
    
    LOGF("[UE4] Extended class registry shut down");
}

//=============================================================================
// Actor Classes Registration
//=============================================================================

void RegisterActorClasses() {
    auto& registry = UE4ClassesExtended::Instance();
    
    // AActor - Base actor class
    registry.RegisterClass("AActor", "UObject", 0x02C0, 
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AActor", "RootComponent", "USceneComponent*", 0x0030, 8);
    registry.RegisterProperty("AActor", "bActorInitialized", "bool", 0x0038, 1);
    registry.RegisterProperty("AActor", "bActorIsBeingDestroyed", "bool", 0x0039, 1);
    registry.RegisterProperty("AActor", "AttachmentRootComponent", "USceneComponent*", 0x0040, 8);
    registry.RegisterProperty("AActor", "Tags", "TArray<FName>", 0x0060, 16);
    registry.RegisterProperty("AActor", "AttachedActors", "TArray<AActor*>", 0x0070, 16);
    registry.RegisterProperty("AActor", "Instigator", "APawn*", 0x0180, 8);
    registry.RegisterProperty("AActor", "bNetLoadOnClient", "bool", 0x01B8, 1);
    registry.RegisterProperty("AActor", "bNetUseOwnerRelevancy", "bool", 0x01B9, 1);
    registry.RegisterProperty("AActor", "bReplicateMovement", "bool", 0x01BA, 1);
    registry.RegisterProperty("AActor", "bHidden", "bool", 0x01BB, 1);
    registry.RegisterProperty("AActor", "bTearOff", "bool", 0x01BC, 1);
    registry.RegisterProperty("AActor", "bExchangedRoles", "bool", 0x01BD, 1);
    registry.RegisterProperty("AActor", "bNetTemporary", "bool", 0x01BE, 1);
    registry.RegisterProperty("AActor", "bNetStartup", "bool", 0x01BF, 1);
    
    // APawn - Controllable actor
    registry.RegisterClass("APawn", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("APawn", "Controller", "AController*", 0x02C0, 8);
    registry.RegisterProperty("APawn", "OriginalController", "AController*", 0x02C8, 8);
    registry.RegisterProperty("APawn", "bUseControllerRotationPitch", "bool", 0x02D0, 1);
    registry.RegisterProperty("APawn", "bUseControllerRotationYaw", "bool", 0x02D1, 1);
    registry.RegisterProperty("APawn", "bUseControllerRotationRoll", "bool", 0x02D2, 1);
    registry.RegisterProperty("APawn", "bOrientRotationToMovement", "bool", 0x02D3, 1);
    registry.RegisterProperty("APawn", "bCollideWhenPlacing", "bool", 0x02D4, 1);
    registry.RegisterProperty("APawn", "bSpawnDistanceCulled", "bool", 0x02D5, 1);
    registry.RegisterProperty("APawn", "bEnableGravity", "bool", 0x02D6, 1);
    registry.RegisterProperty("APawn", "bAutoCrouch", "bool", 0x02D7, 1);
    registry.RegisterProperty("APawn", "CrouchedHalfHeight", "float", 0x02D8, 4);
    registry.RegisterProperty("APawn", "BaseEyeHeight", "float", 0x02DC, 4);
    registry.RegisterProperty("APawn", "DefaultMeshOffset", "FVector", 0x02E0, 12);
    
    // ACharacter - Humanoid pawn
    registry.RegisterClass("ACharacter", "APawn", 0x0480,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("ACharacter", "Mesh", "USkeletalMeshComponent*", 0x0380, 8);
    registry.RegisterProperty("ACharacter", "CapsuleComponent", "UCapsuleComponent*", 0x0388, 8);
    registry.RegisterProperty("ACharacter", "CharacterMovement", "UCharacterMovementComponent*", 0x0390, 8);
    registry.RegisterProperty("ACharacter", "bIsCrouched", "bool", 0x0398, 1);
    registry.RegisterProperty("ACharacter", "bWantsToCrouch", "bool", 0x0399, 1);
    registry.RegisterProperty("ACharacter", "bCanCrouchInProne", "bool", 0x039A, 1);
    registry.RegisterProperty("ACharacter", "bIsInProneCrouch", "bool", 0x039B, 1);
    registry.RegisterProperty("ACharacter", "bIsMovingOnWater", "bool", 0x039C, 1);
    registry.RegisterProperty("ACharacter", "bCanJumpInWater", "bool", 0x039D, 1);
    registry.RegisterProperty("ACharacter", "bWasJumping", "bool", 0x039E, 1);
    registry.RegisterProperty("ACharacter", "JumpForce", "float", 0x03A0, 4);
    registry.RegisterProperty("ACharacter", "JumpMaxHoldTime", "float", 0x03A4, 4);
    registry.RegisterProperty("ACharacter", "JumpMaxCount", "int32", 0x03A8, 4);
    
    // AController - Player/AI controller
    registry.RegisterClass("AController", "AActor", 0x0400,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AController", "Pawn", "APawn*", 0x02C0, 8);
    registry.RegisterProperty("AController", "Character", "ACharacter*", 0x02C8, 8);
    registry.RegisterProperty("AController", "SpectatorPawn", "ASpectatorPawn*", 0x02D0, 8);
    registry.RegisterProperty("AController", "bIsPlayerController", "bool", 0x02D8, 1);
    registry.RegisterProperty("AController", "bAutoManageActiveCameraTarget", "bool", 0x02D9, 1);
    registry.RegisterProperty("AController", "AutoManageActiveCameraInterval", "float", 0x02DC, 4);
    registry.RegisterProperty("AController", "DesiredInputYaw", "float", 0x02E0, 4);
    registry.RegisterProperty("AController", "DesiredInputPitch", "float", 0x02E4, 4);
    
    LOGF("[UE4] Registered %zu actor classes", registry.GetClassCount());
}

//=============================================================================
// Component Classes Registration
//=============================================================================

void RegisterComponentClasses() {
    auto& registry = UE4ClassesExtended::Instance();
    
    // UActorComponent - Base component
    registry.RegisterClass("UActorComponent", "UObject", 0x0180,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("UActorComponent", "bWantsInitializeComponent", "bool", 0x0030, 1);
    registry.RegisterProperty("UActorComponent", "bAutoActivate", "bool", 0x0031, 1);
    registry.RegisterProperty("UActorComponent", "bIsActive", "bool", 0x0032, 1);
    registry.RegisterProperty("UActorComponent", "bComponentUsesTickFunctions", "bool", 0x0033, 1);
    registry.RegisterProperty("UActorComponent", "bAutoRegisterWithTaskGraph", "bool", 0x0034, 1);
    registry.RegisterProperty("UActorComponent", "bHasRegisteredComponentTick", "bool", 0x0035, 1);
    registry.RegisterProperty("UActorComponent", "bIsEditorOnly", "bool", 0x0036, 1);
    registry.RegisterProperty("UActorComponent", "bAllowTickBeforeBeginPlay", "bool", 0x0037, 1);
    registry.RegisterProperty("UActorComponent", "ComponentTags", "TArray<FName>", 0x0040, 16);
    registry.RegisterProperty("UActorComponent", "ComponentTagsCon", "TArray<FName>", 0x0050, 16);
    
    // USceneComponent - Transform component
    registry.RegisterClass("USceneComponent", "UActorComponent", 0x0280,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("USceneComponent", "AttachParent", "USceneComponent*", 0x0180, 8);
    registry.RegisterProperty("USceneComponent", "AttachChildren", "TArray<USceneComponent*>", 0x0188, 16);
    registry.RegisterProperty("USceneComponent", "RelativeLocation", "FVector", 0x01A0, 12);
    registry.RegisterProperty("USceneComponent", "RelativeRotation", "FRotator", 0x01AC, 12);
    registry.RegisterProperty("USceneComponent", "RelativeScale3D", "FVector", 0x01B8, 12);
    registry.RegisterProperty("USceneComponent", "ComponentToWorld", "FTransform", 0x01D0, 48);
    registry.RegisterProperty("USceneComponent", "bAbsoluteLocation", "bool", 0x0200, 1);
    registry.RegisterProperty("USceneComponent", "bAbsoluteRotation", "bool", 0x0201, 1);
    registry.RegisterProperty("USceneComponent", "bAbsoluteScale", "bool", 0x0202, 1);
    registry.RegisterProperty("USceneComponent", "bVisible", "bool", 0x0203, 1);
    
    // UPrimitiveComponent - Base for mesh/collision
    registry.RegisterClass("UPrimitiveComponent", "USceneComponent", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("UPrimitiveComponent", "bCastDynamicShadow", "bool", 0x0280, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bCastStaticShadow", "bool", 0x0281, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bCastHiddenShadow", "bool", 0x0282, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bCastShadowAsTwoSided", "bool", 0x0283, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bAffectDynamicIndirectLighting", "bool", 0x0284, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bAffectDistanceFieldLighting", "bool", 0x0285, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bReceiveCombinedCSMAndStaticShadowsFromStationaryLights", "bool", 0x0286, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bSelfShadowOnly", "bool", 0x0287, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bUseAsOccluder", "bool", 0x0288, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bOwnerNoSee", "bool", 0x0289, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bOnlyOwnerSee", "bool", 0x028A, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bIgnoreOwnerHidden", "bool", 0x028B, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bUseOnePassLightingOnTranslucency", "bool", 0x028C, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bMarkedRenderStateDirty", "bool", 0x028D, 1);
    registry.RegisterProperty("UPrimitiveComponent", "bMarkedRenderStateForResolutionChangeDirty", "bool", 0x028E, 1);
    
    // UStaticMeshComponent - Static mesh
    registry.RegisterClass("UStaticMeshComponent", "UPrimitiveComponent", 0x0480,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("UStaticMeshComponent", "StaticMesh", "UStaticMesh*", 0x0380, 8);
    registry.RegisterProperty("UStaticMeshComponent", "ForcedLodModel", "int32", 0x0388, 4);
    registry.RegisterProperty("UStaticMeshComponent", "bOverrideMinLOD", "bool", 0x038C, 1);
    registry.RegisterProperty("UStaticMeshComponent", "OverrideMinLOD", "float", 0x0390, 4);
    registry.RegisterProperty("UStaticMeshComponent", "bOverrideMaxLOD", "bool", 0x0394, 1);
    registry.RegisterProperty("UStaticMeshComponent", "OverrideMaxLOD", "float", 0x0398, 4);
    registry.RegisterProperty("UStaticMeshComponent", "bCastShadowFromTwoSidedGeometry", "bool", 0x039C, 1);
    registry.RegisterProperty("UStaticMeshComponent", "bCastVolumetricTranslucentShadow", "bool", 0x039D, 1);
    registry.RegisterProperty("UStaticMeshComponent", "bCastVolumetricShadow", "bool", 0x039E, 1);
    registry.RegisterProperty("UStaticMeshComponent", "bVisibleInRayTracing", "bool", 0x039F, 1);
    
    // USkeletalMeshComponent - Skeletal mesh
    registry.RegisterClass("USkeletalMeshComponent", "UPrimitiveComponent", 0x0680,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("USkeletalMeshComponent", "SkeletalMesh", "USkeletalMesh*", 0x0380, 8);
    registry.RegisterProperty("USkeletalMeshComponent", "AnimBlueprintGeneratedClass", "UClass*", 0x0388, 8);
    registry.RegisterProperty("USkeletalMeshComponent", "AnimScriptInstance", "UAnimInstance*", 0x0390, 8);
    registry.RegisterProperty("USkeletalMeshComponent", "AnimInstance", "UAnimInstance*", 0x0398, 8);
    registry.RegisterProperty("USkeletalMeshComponent", "bEnableUpdateRateOptimization", "bool", 0x03A0, 1);
    registry.RegisterProperty("USkeletalMeshComponent", "bAllowUpdateRateOptimizationOverride", "bool", 0x03A1, 1);
    registry.RegisterProperty("USkeletalMeshComponent", "bEnableUpdateRateOptimizationOverride", "bool", 0x03A2, 1);
    registry.RegisterProperty("USkeletalMeshComponent", "bAllowAnimGraphOptimization", "bool", 0x03A3, 1);
    registry.RegisterProperty("USkeletalMeshComponent", "bEnableUpdateRateOptimizationForLOD", "bool", 0x03A4, 1);
    registry.RegisterProperty("USkeletalMeshComponent", "bEnableUpdateRateOptimizationForLODOverride", "bool", 0x03A5, 1);
    
    // UBoxComponent - Box collision
    registry.RegisterClass("UBoxComponent", "UPrimitiveComponent", 0x0400,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("UBoxComponent", "BoxExtent", "FVector", 0x0380, 12);
    registry.RegisterProperty("UBoxComponent", "ShapeScale", "FVector", 0x038C, 12);
    
    // USphereComponent - Sphere collision
    registry.RegisterClass("USphereComponent", "UPrimitiveComponent", 0x0400,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("USphereComponent", "SphereRadius", "float", 0x0380, 4);
    
    // UCapsuleComponent - Capsule collision
    registry.RegisterClass("UCapsuleComponent", "UPrimitiveComponent", 0x0400,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("UCapsuleComponent", "CapsuleRadius", "float", 0x0380, 4);
    registry.RegisterProperty("UCapsuleComponent", "CapsuleHalfHeight", "float", 0x0384, 4);
    
    LOGF("[UE4] Registered %zu component classes", registry.GetClassCount());
}

//=============================================================================
// Gameplay Classes Registration
//=============================================================================

void RegisterGameplayClasses() {
    auto& registry = UE4ClassesExtended::Instance();
    
    // AGameModeBase - Game mode base
    registry.RegisterClass("AGameModeBase", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGameModeBase", "GameSession", "AGameSession*", 0x02C0, 8);
    registry.RegisterProperty("AGameModeBase", "GameStateClass", "TSubclassOf<AGameStateBase>", 0x02C8, 8);
    registry.RegisterProperty("AGameModeBase", "GameModeClass", "TSubclassOf<AGameModeBase>", 0x02D0, 8);
    registry.RegisterProperty("AGameModeBase", "DefaultPawnClass", "TSubclassOf<APawn>", 0x02D8, 8);
    registry.RegisterProperty("AGameModeBase", "HUDClass", "TSubclassOf<AHUD>", 0x02E0, 8);
    registry.RegisterProperty("AGameModeBase", "PlayerControllerClass", "TSubclassOf<APlayerController>", 0x02E8, 8);
    registry.RegisterProperty("AGameModeBase", "SpectatorClass", "TSubclassOf<ASpectatorPawn>", 0x02F0, 8);
    registry.RegisterProperty("AGameModeBase", "ReplaySpectatorPlayerControllerClass", "TSubclassOf<APlayerController>", 0x02F8, 8);
    registry.RegisterProperty("AGameModeBase", "PlayerStateClass", "TSubclassOf<APlayerState>", 0x0300, 8);
    
    // AGameStateBase - Game state base
    registry.RegisterClass("AGameStateBase", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGameStateBase", "GameModeClass", "TSubclassOf<AGameModeBase>", 0x02C0, 8);
    registry.RegisterProperty("AGameStateBase", "SpectatorClass", "TSubclassOf<ASpectatorPawn>", 0x02C8, 8);
    registry.RegisterProperty("AGameStateBase", "PlayerStateClass", "TSubclassOf<APlayerState>", 0x02D0, 8);
    registry.RegisterProperty("AGameStateBase", "bReplicatedHasBegunPlay", "bool", 0x02D8, 1);
    registry.RegisterProperty("AGameStateBase", "ReplicatedHasBegunPlay", "bool", 0x02D9, 1);
    registry.RegisterProperty("AGameStateBase", "ElapsedTime", "float", 0x02DC, 4);
    
    // APlayerController - Player controller
    registry.RegisterClass("APlayerController", "AController", 0x0680,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("APlayerController", "Player", "ULocalPlayer*", 0x0400, 8);
    registry.RegisterProperty("APlayerController", "PlayerCameraManager", "ACameraManager*", 0x0408, 8);
    registry.RegisterProperty("APlayerController", "bAutoManageActiveCameraTarget", "bool", 0x0410, 1);
    registry.RegisterProperty("APlayerController", "SpectatorPawn", "ASpectatorPawn*", 0x0418, 8);
    registry.RegisterProperty("APlayerController", "bShowMouseCursor", "bool", 0x0420, 1);
    registry.RegisterProperty("APlayerController", "bEnableClickEvents", "bool", 0x0421, 1);
    registry.RegisterProperty("APlayerController", "bEnableTouchEvents", "bool", 0x0422, 1);
    registry.RegisterProperty("APlayerController", "bEnableMouseOverEvents", "bool", 0x0423, 1);
    registry.RegisterProperty("APlayerController", "bEnableTouchOverEvents", "bool", 0x0424, 1);
    registry.RegisterProperty("APlayerController", "bEnableGamepadEvents", "bool", 0x0425, 1);
    
    // AHUD - HUD
    registry.RegisterClass("AHUD", "AActor", 0x0480,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AHUD", "PlayerOwner", "APlayerController*", 0x02C0, 8);
    registry.RegisterProperty("AHUD", "bShowHUD", "bool", 0x02C8, 1);
    registry.RegisterProperty("AHUD", "bShowHitBoxDebugInfo", "bool", 0x02C9, 1);
    registry.RegisterProperty("AHUD", "bShowDebugInfo", "bool", 0x02CA, 1);
    registry.RegisterProperty("AHUD", "bEnableHUDTransitions", "bool", 0x02CB, 1);
    
    // APlayerState - Player state
    registry.RegisterClass("APlayerState", "AActor", 0x0480,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("APlayerState", "PlayerName", "FString", 0x02C0, 16);
    registry.RegisterProperty("APlayerState", "Score", "float", 0x02D0, 4);
    registry.RegisterProperty("APlayerState", "bIsABot", "bool", 0x02D4, 1);
    registry.RegisterProperty("APlayerState", "bIsSpectator", "bool", 0x02D5, 1);
    registry.RegisterProperty("APlayerState", "bOnlyControllerConnection", "bool", 0x02D6, 1);
    registry.RegisterProperty("APlayerState", "bUsingDefaultTexture", "bool", 0x02D7, 1);
    registry.RegisterProperty("APlayerState", "bIsInactive", "bool", 0x02D8, 1);
    registry.RegisterProperty("APlayerState", "bFromPreviousLevel", "bool", 0x02D9, 1);
    registry.RegisterProperty("APlayerState", "Ping", "int32", 0x02DC, 4);
    registry.RegisterProperty("APlayerState", "Deaths", "int32", 0x02E0, 4);
    
    LOGF("[UE4] Registered %zu gameplay classes", registry.GetClassCount());
}

//=============================================================================
// Mission Classes Registration (GTA 3 DE Specific)
//=============================================================================

void RegisterMissionClasses() {
    auto& registry = UE4ClassesExtended::Instance();
    
    // AGTA3_MissionTrigger - Mission trigger volume
    registry.RegisterClass("AGTA3_MissionTrigger", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_MissionTrigger", "MissionID", "int32", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_MissionTrigger", "MissionName", "FString", 0x02C8, 16);
    registry.RegisterProperty("AGTA3_MissionTrigger", "bTriggered", "bool", 0x02D8, 1);
    registry.RegisterProperty("AGTA3_MissionTrigger", "bOneTimeOnly", "bool", 0x02D9, 1);
    registry.RegisterProperty("AGTA3_MissionTrigger", "RequiredMissionID", "int32", 0x02DC, 4);
    registry.RegisterProperty("AGTA3_MissionTrigger", "TriggerVolume", "UBoxComponent*", 0x02E0, 8);
    
    // AGTA3_MissionObjective - Mission objective
    registry.RegisterClass("AGTA3_MissionObjective", "AActor", 0x0400,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_MissionObjective", "ObjectiveID", "int32", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_MissionObjective", "ObjectiveType", "EObjectiveType", 0x02C4, 4);
    registry.RegisterProperty("AGTA3_MissionObjective", "bCompleted", "bool", 0x02C8, 1);
    registry.RegisterProperty("AGTA3_MissionObjective", "bFailed", "bool", 0x02C9, 1);
    registry.RegisterProperty("AGTA3_MissionObjective", "TargetActor", "AActor*", 0x02D0, 8);
    registry.RegisterProperty("AGTA3_MissionObjective", "TargetLocation", "FVector", 0x02D8, 12);
    registry.RegisterProperty("AGTA3_MissionObjective", "TargetRadius", "float", 0x02E4, 4);
    registry.RegisterProperty("AGTA3_MissionObjective", "Description", "FString", 0x02E8, 16);
    
    // AGTA3_CutsceneActor - Cutscene actor
    registry.RegisterClass("AGTA3_CutsceneActor", "AActor", 0x0400,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_CutsceneActor", "CharacterID", "int32", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_CutsceneActor", "bIsPlayer", "bool", 0x02C4, 1);
    registry.RegisterProperty("AGTA3_CutsceneActor", "CutsceneMesh", "USkeletalMeshComponent*", 0x02D0, 8);
    registry.RegisterProperty("AGTA3_CutsceneActor", "CurrentAnimation", "UAnimSequence*", 0x02D8, 8);
    registry.RegisterProperty("AGTA3_CutsceneActor", "bLoopingAnimation", "bool", 0x02E0, 1);
    
    // AGTA3_VehicleSpawner - Vehicle spawner
    registry.RegisterClass("AGTA3_VehicleSpawner", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_VehicleSpawner", "VehicleClass", "TSubclassOf<AVehicle>", 0x02C0, 8);
    registry.RegisterProperty("AGTA3_VehicleSpawner", "SpawnLocation", "FVector", 0x02C8, 12);
    registry.RegisterProperty("AGTA3_VehicleSpawner", "SpawnRotation", "FRotator", 0x02D4, 12);
    registry.RegisterProperty("AGTA3_VehicleSpawner", "bAutoSpawn", "bool", 0x02E0, 1);
    registry.RegisterProperty("AGTA3_VehicleSpawner", "RespawnTime", "float", 0x02E4, 4);
    
    // AGTA3_PedSpawner - Pedestrian spawner
    registry.RegisterClass("AGTA3_PedSpawner", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_PedSpawner", "PedClass", "TSubclassOf<APed>", 0x02C0, 8);
    registry.RegisterProperty("AGTA3_PedSpawner", "SpawnLocation", "FVector", 0x02C8, 12);
    registry.RegisterProperty("AGTA3_PedSpawner", "bAutoSpawn", "bool", 0x02D4, 1);
    registry.RegisterProperty("AGTA3_PedSpawner", "BehaviorType", "EBehaviorType", 0x02D8, 4);
    
    // AGTA3_WeaponPickup - Weapon pickup
    registry.RegisterClass("AGTA3_WeaponPickup", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_WeaponPickup", "WeaponType", "EWeaponType", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_WeaponPickup", "AmmoCount", "int32", 0x02C4, 4);
    registry.RegisterProperty("AGTA3_WeaponPickup", "bPickedUp", "bool", 0x02C8, 1);
    registry.RegisterProperty("AGTA3_WeaponPickup", "RespawnTime", "float", 0x02CC, 4);
    registry.RegisterProperty("AGTA3_WeaponPickup", "PickupMesh", "UStaticMeshComponent*", 0x02D0, 8);
    
    // AGTA3_Checkpoint - Race/checkpoint
    registry.RegisterClass("AGTA3_Checkpoint", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_Checkpoint", "CheckpointID", "int32", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_Checkpoint", "bPassed", "bool", 0x02C4, 1);
    registry.RegisterProperty("AGTA3_Checkpoint", "CheckpointType", "ECheckpointType", 0x02C8, 4);
    registry.RegisterProperty("AGTA3_Checkpoint", "NextCheckpointID", "int32", 0x02CC, 4);
    registry.RegisterProperty("AGTA3_Checkpoint", "TriggerVolume", "UBoxComponent*", 0x02D0, 8);
    
    // AGTA3_PhoneBooth - Phone booth for missions
    registry.RegisterClass("AGTA3_PhoneBooth", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_PhoneBooth", "PhoneID", "int32", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_PhoneBooth", "bInUse", "bool", 0x02C4, 1);
    registry.RegisterProperty("AGTA3_PhoneBooth", "AssociatedMissionID", "int32", 0x02C8, 4);
    registry.RegisterProperty("AGTA3_PhoneBooth", "InteractionVolume", "USphereComponent*", 0x02D0, 8);
    
    // AGTA3_SafeHouse - Safe house
    registry.RegisterClass("AGTA3_SafeHouse", "AActor", 0x0380,
                          static_cast<uint32_t>(EClassFlags::CLASS_Native));
    registry.RegisterProperty("AGTA3_SafeHouse", "SafeHouseID", "int32", 0x02C0, 4);
    registry.RegisterProperty("AGTA3_SafeHouse", "bUnlocked", "bool", 0x02C4, 1);
    registry.RegisterProperty("AGTA3_SafeHouse", "HealthPickupLocation", "FVector", 0x02C8, 12);
    registry.RegisterProperty("AGTA3_SafeHouse", "ArmorPickupLocation", "FVector", 0x02D4, 12);
    registry.RegisterProperty("AGTA3_SafeHouse", "WeaponStorageLocation", "FVector", 0x02E0, 12);
    
    LOGF("[UE4] Registered %zu mission classes", registry.GetClassCount());
}

} // namespace Kyty::Libs
