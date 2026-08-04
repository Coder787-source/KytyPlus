#include "libs/ue4HLE.h"
#include "common/logging/log.h"
#include "common/dateTime.h"
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Kyty::Libs {

// Forward declaration
void RegisterClassFunction(const char* className, const char* functionName);

// Global UE4 HLE instance
static UE4HLE g_ue4HLE;

UE4HLE& GetUE4HLE() {
    return g_ue4HLE;
}

// Native function registry
static std::unordered_map<std::string, UE4NativeFunc> g_nativeFunctions;

void RegisterUENative(const char* name, UE4NativeFunc func) {
    if (!name || !func) return;
    g_nativeFunctions[name] = func;
    LOGF("[UE4] DEBUG: " "Registered native: %s", name);
}

UE4HLE::UE4HLE() {
    m_objects.reserve(UE4_MAX_OBJECTS);
    m_classes.reserve(UE4_MAX_CLASSES);
}

UE4HLE::~UE4HLE() {
    Shutdown();
}

bool UE4HLE::Initialize() {
    if (m_initialized) {
        LOGF("[UE4] WARNING: " "Already initialized");
        return true;
    }

    LOGF("[UE4] INFO: " "Initializing UE4 HLE...");

    // Initialize built-in classes
    InitializeBuiltInClasses();
    
    // Initialize built-in functions
    InitializeBuiltInFunctions();

    // Set start time
    auto now = std::chrono::steady_clock::now();
    m_startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    m_engineState.isInitialized = true;
    m_engineState.isGameStarted = false;
    m_engineState.gameTime = 0.0f;
    m_engineState.frameTime = 1.0f / 60.0f;
    m_engineState.frameCount = 0;
    m_engineState.isPaused = false;
    m_engineState.isRendering = true;

    m_initialized = true;

    LOGF("[UE4] INFO: " "UE4 HLE initialized successfully");
    LOGF("[UE4] INFO: " "  Classes: %zu", m_classes.size());
    LOGF("[UE4] INFO: " "  Objects: %zu", m_objects.size());

    return true;
}

void UE4HLE::Shutdown() {
    if (!m_initialized) {
        return;
    }

    LOGF("[UE4] INFO: " "Shutting down UE4 HLE...");

    // Destroy all objects
    for (auto& obj : m_objects) {
        if (obj && obj->userData) {
            // Clean up user data if needed
        }
    }

    m_objects.clear();
    m_classes.clear();
    m_classNameToId.clear();
    m_objectNameToId.clear();
    m_consoleCommands.clear();

    m_engineState.isInitialized = false;
    m_initialized = false;

    LOGF("[UE4] INFO: " "UE4 HLE shutdown complete");
}

void UE4HLE::InitializeBuiltInClasses() {
    LOGF("[UE4] DEBUG: " "Registering built-in classes...");

    // Core UE4 classes
    RegisterClass("UObject", nullptr);
    RegisterClass("UClass", "UObject");
    RegisterClass("UFunction", "UObject");
    RegisterClass("UProperty", "UObject");
    RegisterClass("UStruct", "UObject");
    RegisterClass("UEnum", "UObject");
    RegisterClass("UPackage", "UObject");
    
    // Engine classes
    RegisterClass("UEngine", "UObject");
    RegisterClass("UGameEngine", "UEngine");
    RegisterClass("UWorld", "UObject");
    RegisterClass("ULevel", "UObject");
    RegisterClass("AActor", "UObject");
    RegisterClass("APawn", "AActor");
    RegisterClass("ACharacter", "APawn");
    RegisterClass("APlayerController", "APawn");
    RegisterClass("AGameMode", "AActor");
    RegisterClass("AGameState", "AActor");
    RegisterClass("AHUD", "AActor");
    
    // GTA 3 DE specific classes (common UE4 classes used)
    RegisterClass("UTexture2D", "UObject");
    RegisterClass("UMaterial", "UObject");
    RegisterClass("UMaterialInstance", "UMaterial");
    RegisterClass("UStaticMesh", "UObject");
    RegisterClass("USkeletalMesh", "UObject");
    RegisterClass("UAnimInstance", "UObject");
    RegisterClass("UParticleSystem", "UObject");
    RegisterClass("USoundBase", "UObject");
    RegisterClass("USoundWave", "USoundBase");
    RegisterClass("UAudioComponent", "UActorComponent");
    RegisterClass("UActorComponent", "UObject");
    RegisterClass("USceneComponent", "UActorComponent");
    RegisterClass("UPrimitiveComponent", "USceneComponent");
    RegisterClass("UStaticMeshComponent", "UPrimitiveComponent");
    RegisterClass("USkeletalMeshComponent", "UPrimitiveComponent");
    RegisterClass("UCameraComponent", "USceneComponent");
    RegisterClass("UInputComponent", "UActorComponent");
    
    // UI classes
    RegisterClass("UUserWidget", "UObject");
    RegisterClass("UWidget", "UObject");
    RegisterClass("UButton", "UWidget");
    RegisterClass("UTextBlock", "UWidget");
    RegisterClass("UImage", "UWidget");
    
    LOGF("[UE4] INFO: " "Registered %zu built-in classes", m_classes.size());
}

void UE4HLE::InitializeBuiltInFunctions() {
    LOGF("[UE4] DEBUG: " "Registering built-in functions...");

    // Core object functions
    RegisterClassFunction("UObject", "IsValid");
    RegisterClassFunction("UObject", "GetClass");
    RegisterClassFunction("UObject", "GetName");
    RegisterClassFunction("UObject", "GetFullName");
    RegisterClassFunction("UObject", "MarkPendingKill");
    RegisterClassFunction("UObject", "BeginDestroy");
    
    // Engine functions
    RegisterClassFunction("UEngine", "GetWorld");
    RegisterClassFunction("UEngine", "GetGameViewport");
    RegisterClassFunction("UEngine", "AddOnScreenDebugMessage");
    
    // World functions
    RegisterClassFunction("UWorld", "GetFirstPlayerController");
    RegisterClassFunction("UWorld", "SpawnActor");
    RegisterClassFunction("UWorld", "DestroyActor");
    RegisterClassFunction("UWorld", "GetTimeSeconds");
    RegisterClassFunction("UWorld", "GetRealTimeSeconds");
    
    // Actor functions
    RegisterClassFunction("AActor", "GetActorLocation");
    RegisterClassFunction("AActor", "GetActorRotation");
    RegisterClassFunction("AActor", "SetActorLocation");
    RegisterClassFunction("AActor", "SetActorRotation");
    RegisterClassFunction("AActor", "GetWorld");
    RegisterClassFunction("AActor", "Destroy");
    
    // Component functions
    RegisterClassFunction("UActorComponent", "GetOwner");
    RegisterClassFunction("UActorComponent", "GetWorld");
    RegisterClassFunction("USceneComponent", "GetRelativeLocation");
    RegisterClassFunction("USceneComponent", "SetRelativeLocation");
    
    // Input functions
    RegisterClassFunction("APlayerController", "GetHud");
    RegisterClassFunction("APlayerController", "SetInputMode");
    RegisterClassFunction("APlayerController", "GetPawn");
    RegisterClassFunction("APlayerController", "Possess");
    
    // Game mode functions
    RegisterClassFunction("AGameMode", "GetWorldSettings");
    RegisterClassFunction("AGameMode", "GetGameState");
    
    // String/table functions
    RegisterClassFunction("UKismetStringLibrary", "Conv_StringToText");
    RegisterClassFunction("UKismetTextLibrary", "Conv_TextToString");
    RegisterClassFunction("UKismetMathLibrary", "MakeVector");
    RegisterClassFunction("UKismetMathLibrary", "MakeRotator");
    
    LOGF("[UE4] INFO: " "Registered built-in functions");
}

int32_t UE4HLE::Init() {
    LOGF("[UE4] INFO: " "UE4::Init() called");
    return m_initialized ? 0 : -1;
}

int32_t UE4HLE::InitCommandLine(const char* commandLine) {
    LOGF("[UE4] INFO: " "UE4::InitCommandLine(): %s", commandLine ? commandLine : "null");
    
    // Parse common command line args
    if (commandLine) {
        std::string cmd(commandLine);
        
        if (cmd.find("-windowed") != std::string::npos) {
            LOGF("[UE4] INFO: " "Windowed mode requested");
        }
        if (cmd.find("-res=") != std::string::npos) {
            LOGF("[UE4] INFO: " "Resolution override requested");
        }
        if (cmd.find("-log") != std::string::npos) {
            LOGF("[UE4] INFO: " "Logging enabled");
        }
    }
    
    return 0;
}

void UE4HLE::Exit() {
    LOGF("[UE4] INFO: " "UE4::Exit() called");
    RequestExit(0);
}

void UE4HLE::RequestExit(int32_t exitCode) {
    LOGF("[UE4] INFO: " "UE4::RequestExit(%d)", exitCode);
    m_engineState.isGameStarted = false;
}

bool UE4HLE::IsGame() {
    return !IsEditor();
}

bool UE4HLE::IsEditor() {
    return false; // We're emulating a game, not editor
}

bool UE4HLE::IsUnattended() {
    return false;
}

uint32_t UE4HLE::FindObject(const char* className, const char* objectName) {
    if (!objectName) return 0;
    
    std::string fullName = objectName;
    if (className) {
        fullName = std::string(className) + "." + objectName;
    }
    
    auto it = m_objectNameToId.find(fullName);
    if (it != m_objectNameToId.end()) {
        return it->second;
    }
    
    // Try just the object name
    it = m_objectNameToId.find(objectName);
    if (it != m_objectNameToId.end()) {
        return it->second;
    }
    
    return 0;
}

uint32_t UE4HLE::FindClass(const char* className) {
    if (!className) return 0;
    
    auto it = m_classNameToId.find(className);
    if (it != m_classNameToId.end()) {
        return it->second;
    }
    
    LOGF("[UE4] DEBUG: " "Class not found: %s", className);
    return 0;
}

uint32_t UE4HLE::SpawnObject(uint32_t classId, const char* name) {
    UE4Class* cls = GetClass(classId);
    if (!cls) {
        LOGF("[UE4] ERROR: " "SpawnObject: Invalid class ID %u", classId);
        return 0;
    }
    
    UE4ObjectType objType = UE4ObjectType::UObject;
    if (cls->name == "UClass") objType = UE4ObjectType::UClass;
    else if (cls->name == "UFunction") objType = UE4ObjectType::UFunction;
    else if (cls->name == "UProperty") objType = UE4ObjectType::UProperty;
    
    uint32_t objId = CreateObject(objType, name, cls->name.c_str());
    
    LOGF("[UE4] DEBUG: " "Spawned object: %s (class=%s, id=%u)", 
              name ? name : "unnamed", cls->name.c_str(), objId);
    
    return objId;
}

void UE4HLE::DestroyObject(uint32_t objectId) {
    if (objectId == 0) return;
    
    UE4Object* obj = GetObject(objectId);
    if (!obj) return;
    
    LOGF("[UE4] DEBUG: " "Destroying object: %s (id=%u)", obj->name.c_str(), objectId);
    
    // Remove from name map
    if (!obj->name.empty()) {
        m_objectNameToId.erase(obj->name);
    }
    
    // Mark for destruction (don't actually free, just invalidate)
    obj->isInitialized = false;
    obj->userData = nullptr;
}

bool UE4HLE::IsValidObject(uint32_t objectId) {
    const UE4Object* obj = GetObject(objectId);
    return obj && obj->isInitialized;
}

uint32_t UE4HLE::RegisterClass(const char* className, const char* parentClass) {
    if (!className) return 0;
    
    // Check if already registered
    auto it = m_classNameToId.find(className);
    if (it != m_classNameToId.end()) {
        return it->second;
    }
    
    if (m_classes.size() >= UE4_MAX_CLASSES) {
        LOGF("[UE4] ERROR: " "Maximum class count reached");
        return 0;
    }
    
    auto cls = std::make_unique<UE4Class>();
    cls->id = m_nextClassId++;
    cls->name = className;
    cls->parentClass = parentClass ? parentClass : "";
    cls->isRegistered = true;
    
    uint32_t classId = cls->id;
    m_classes.push_back(std::move(cls));
    m_classNameToId[className] = classId;
    
    LOGF("[UE4] DEBUG: " "Registered class: %s (parent=%s, id=%u)", 
              className, parentClass ? parentClass : "none", classId);
    
    return classId;
}

bool UE4HLE::RegisterProperty(uint32_t classId, const char* propertyName) {
    UE4Class* cls = GetClass(classId);
    if (!cls || !propertyName) return false;
    
    cls->properties.push_back(propertyName);
    return true;
}

bool UE4HLE::RegisterFunction(uint32_t classId, const char* functionName) {
    UE4Class* cls = GetClass(classId);
    if (!cls || !functionName) return false;
    
    cls->functions.push_back(functionName);
    return true;
}

bool UE4HLE::GetPropertyValue(uint32_t objectId, const char* propertyName, void* outValue) {
    UE4Object* obj = GetObject(objectId);
    if (!obj || !propertyName || !outValue) return false;
    
    // Stub - in production would access actual property data
    LOGF("[UE4] DEBUG: " "GetProperty: obj=%u prop=%s (stub)", objectId, propertyName);
    std::memset(outValue, 0, 64); // Zero out for safety
    return true;
}

bool UE4HLE::SetPropertyValue(uint32_t objectId, const char* propertyName, const void* value) {
    UE4Object* obj = GetObject(objectId);
    if (!obj || !propertyName || !value) return false;
    
    // Stub - in production would set actual property data
    LOGF("[UE4] DEBUG: " "SetProperty: obj=%u prop=%s (stub)", objectId, propertyName);
    return true;
}

uint64_t UE4HLE::CallFunction(uint32_t objectId, const char* functionName, void* params) {
    if (!functionName) return 0;
    
    UE4Object* obj = GetObject(objectId);
    if (!obj) {
        LOGF("[UE4] DEBUG: " "CallFunction: Invalid object %u", objectId);
        return 0;
    }
    
    // Check for native implementation
    auto it = g_nativeFunctions.find(functionName);
    if (it != g_nativeFunctions.end()) {
        return it->second(params);
    }
    
    // Default stub behavior
    LOGF("[UE4] DEBUG: " "CallFunction: %s::%s (stub)", obj->name.c_str(), functionName);
    return 0;
}

uint64_t UE4HLE::ProcessEvent(uint32_t objectId, uint32_t functionId, void* params) {
    // UE4's native event processor
    LOGF("[UE4] DEBUG: " "ProcessEvent: obj=%u func=%u", objectId, functionId);
    return 0;
}

bool UE4HLE::LoadMap(const char* mapName) {
    if (!mapName) return false;
    
    LOGF("[UE4] INFO: " "Loading map: %s", mapName);
    
    m_engineState.currentMap = 1;
    m_engineState.isGameStarted = true;
    m_engineState.gameTime = 0.0f;
    
    return true;
}

bool UE4HLE::UnloadMap() {
    LOGF("[UE4] INFO: " "Unloading map");
    m_engineState.currentMap = 0;
    m_engineState.isGameStarted = false;
    return true;
}

bool UE4HLE::OpenLevel(const char* levelName) {
    if (!levelName) return false;
    
    LOGF("[UE4] INFO: " "Opening level: %s", levelName);
    return LoadMap(levelName);
}

void UE4HLE::ServerTravel(const char* url, bool bAbsolute) {
    LOGF("[UE4] INFO: " "ServerTravel: %s (absolute=%d)", url ? url : "null", bAbsolute ? 1 : 0);
    
    if (url) {
        LoadMap(url);
    }
}

float UE4HLE::GetWorldTimeSeconds() const {
    if (!m_engineState.isInitialized) return 0.0f;
    
    auto now = std::chrono::steady_clock::now();
    int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return static_cast<float>(currentTime - m_startTime) / 1000.0f;
}

float UE4HLE::GetRealTimeSeconds() const {
    return GetWorldTimeSeconds();
}

bool UE4HLE::BindInput(const char* commandName, const char* keyName) {
    LOGF("[UE4] DEBUG: " "BindInput: %s -> %s", commandName ? commandName : "null", 
              keyName ? keyName : "null");
    return true;
}

bool UE4HLE::UnbindInput(const char* commandName) {
    LOGF("[UE4] DEBUG: " "UnbindInput: %s", commandName ? commandName : "null");
    return true;
}

bool UE4HLE::ExecCommand(const char* command) {
    if (!command) return false;
    
    LOGF("[UE4] INFO: " "Exec: %s", command);
    
    // Check for built-in commands
    std::string cmd(command);
    
    if (cmd == "exit" || cmd == "quit") {
        RequestExit(0);
        return true;
    }
    
    if (cmd == "pause") {
        m_engineState.isPaused = !m_engineState.isPaused;
        LOGF("[UE4] INFO: " "Game %s", m_engineState.isPaused ? "paused" : "resumed");
        return true;
    }
    
    if (cmd.find("open ") == 0) {
        std::string levelName = cmd.substr(5);
        return OpenLevel(levelName.c_str());
    }
    
    // Check custom commands
    auto it = m_consoleCommands.find(cmd);
    if (it != m_consoleCommands.end()) {
        it->second();
        return true;
    }
    
    LOGF("[UE4] WARNING: " "Unknown command: %s", command);
    return false;
}

void UE4HLE::AddConsoleCommand(const char* command, std::function<void()> callback) {
    if (!command) return;
    m_consoleCommands[command] = std::move(callback);
    LOGF("[UE4] DEBUG: " "Added console command: %s", command);
}

void UE4HLE::Log(const char* category, const char* message) {
    if (!category || !message) return;
    LOGF("[UE4[%s]] INFO: ", category, "%s", message);
}

void UE4HLE::Warning(const char* category, const char* message) {
    if (!category || !message) return;
    LOGF("[UE4[%s]] WARNING: ", category, "%s", message);
}

void UE4HLE::Error(const char* category, const char* message) {
    if (!category || !message) return;
    LOGF("[UE4[%s]] ERROR: ", category, "%s", message);
}

int32_t UE4HLE::GetObjectCount() const {
    return static_cast<int32_t>(m_objects.size());
}

int32_t UE4HLE::GetClassCount() const {
    return static_cast<int32_t>(m_classes.size());
}

void UE4HLE::DumpObjects() const {
    LOGF("[UE4] INFO: " "=== UE4 Objects ===");
    for (const auto& obj : m_objects) {
        if (obj && obj->isInitialized) {
            LOGF("[UE4] INFO: " "  [%u] %s (%s)", obj->id, obj->name.c_str(), obj->className.c_str());
        }
    }
    LOGF("[UE4] INFO: " "Total: %d objects", GetObjectCount());
}

void UE4HLE::DumpClasses() const {
    LOGF("[UE4] INFO: " "=== UE4 Classes ===");
    for (const auto& cls : m_classes) {
        if (cls && cls->isRegistered) {
            LOGF("[UE4] INFO: " "  [%u] %s (parent: %s)", cls->id, cls->name.c_str(), 
                     cls->parentClass.c_str());
        }
    }
    LOGF("[UE4] INFO: " "Total: %d classes", GetClassCount());
}

UE4Object* UE4HLE::GetObject(uint32_t id) {
    if (id == 0 || id > m_objects.size()) return nullptr;
    
    for (auto& obj : m_objects) {
        if (obj && obj->id == id) {
            return obj.get();
        }
    }
    return nullptr;
}

const UE4Object* UE4HLE::GetObject(uint32_t id) const {
    return const_cast<UE4HLE*>(this)->GetObject(id);
}

UE4Class* UE4HLE::GetClass(uint32_t id) {
    if (id == 0 || id > m_classes.size()) return nullptr;
    
    for (auto& cls : m_classes) {
        if (cls && cls->id == id) {
            return cls.get();
        }
    }
    return nullptr;
}

const UE4Class* UE4HLE::GetClass(uint32_t id) const {
    return const_cast<UE4HLE*>(this)->GetClass(id);
}

uint32_t UE4HLE::CreateObject(UE4ObjectType type, const char* name, const char* className) {
    if (m_objects.size() >= UE4_MAX_OBJECTS) {
        LOGF("[UE4] ERROR: " "Maximum object count reached");
        return 0;
    }
    
    auto obj = std::make_unique<UE4Object>();
    obj->id = m_nextObjectId++;
    obj->type = type;
    obj->name = name ? name : "";
    obj->className = className ? className : "";
    obj->isInitialized = true;
    
    uint32_t objId = obj->id;
    m_objects.push_back(std::move(obj));
    
    if (!obj->name.empty()) {
        m_objectNameToId[obj->name] = objId;
    }
    
    return objId;
}

void RegisterClassFunction(const char* className, const char* functionName) {
    // Helper to register class functions
    UE4HLE& ue4 = GetUE4HLE();
    uint32_t classId = ue4.FindClass(className);
    if (classId != 0) {
        ue4.RegisterFunction(classId, functionName);
    }
}

} // namespace Kyty::Libs
