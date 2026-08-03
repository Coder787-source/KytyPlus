#pragma once

#include "libs.h"
#include "common/types.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace Kyty::Libs {

// Unreal Engine 4 HLE for GTA 3 Definitive Edition
// Implements core UE4 functions needed for boot and gameplay

constexpr int32_t UE4_MAX_CLASSES = 4096;
constexpr int32_t UE4_MAX_OBJECTS = 65536;
constexpr int32_t UE4_MAX_PROPERTIES = 256;

// UE4 Core Types
enum class UE4ObjectType {
    UObject,
    UClass,
    UFunction,
    UProperty,
    UStruct,
    UEnum,
    UPackage,
    Unknown
};

struct UE4Object {
    uint32_t id = 0;
    UE4ObjectType type = UE4ObjectType::Unknown;
    std::string name;
    std::string className;
    uint64_t vtable = 0;
    bool isInitialized = false;
    void* userData = nullptr;
};

struct UE4Class {
    uint32_t id = 0;
    std::string name;
    std::string parentClass;
    std::vector<std::string> properties;
    std::vector<std::string> functions;
    uint64_t classFlags = 0;
    bool isRegistered = false;
};

struct UE4Function {
    std::string name;
    std::string returnType;
    std::vector<std::string> parameters;
    uint64_t funcAddress = 0;
    uint32_t functionFlags = 0;
};

// UE4 Engine State
struct UE4EngineState {
    bool isInitialized = false;
    bool isGameStarted = false;
    int32_t currentMap = 0;
    float gameTime = 0.0f;
    float frameTime = 0.0f;
    int32_t frameCount = 0;
    bool isPaused = false;
    bool isRendering = false;
};

// UE4 HLE Engine
class UE4HLE {
public:
    UE4HLE();
    ~UE4HLE();

    // Initialization
    bool Initialize();
    void Shutdown();

    // Core engine functions
    int32_t Init();
    int32_t InitCommandLine(const char* commandLine);
    void Exit();
    void RequestExit(int32_t exitCode);
    bool IsGame();
    bool IsEditor();
    bool IsUnattended();

    // Object system
    uint32_t FindObject(const char* className, const char* objectName);
    uint32_t FindClass(const char* className);
    uint32_t SpawnObject(uint32_t classId, const char* name);
    void DestroyObject(uint32_t objectId);
    bool IsValidObject(uint32_t objectId);
    
    // Class registration
    uint32_t RegisterClass(const char* className, const char* parentClass = nullptr);
    bool RegisterProperty(uint32_t classId, const char* propertyName);
    bool RegisterFunction(uint32_t classId, const char* functionName);

    // Property access
    bool GetPropertyValue(uint32_t objectId, const char* propertyName, void* outValue);
    bool SetPropertyValue(uint32_t objectId, const char* propertyName, const void* value);
    
    // Function calling
    uint64_t CallFunction(uint32_t objectId, const char* functionName, void* params = nullptr);
    uint64_t ProcessEvent(uint32_t objectId, uint32_t functionId, void* params);

    // World/Level management
    bool LoadMap(const char* mapName);
    bool UnloadMap();
    bool OpenLevel(const char* levelName);
    void ServerTravel(const char* url, bool bAbsolute);
    
    // Game state
    UE4EngineState& GetEngineState() { return m_engineState; }
    float GetWorldTimeSeconds() const;
    float GetRealTimeSeconds() const;
    int32_t GetFrameNumber() const { return m_engineState.frameCount; }
    float GetFrameTime() const { return m_engineState.frameTime; }
    
    // Input
    bool BindInput(const char* commandName, const char* keyName);
    bool UnbindInput(const char* commandName);
    
    // Console
    bool ExecCommand(const char* command);
    void AddConsoleCommand(const char* command, std::function<void()> callback);
    
    // Logging
    void Log(const char* category, const char* message);
    void Warning(const char* category, const char* message);
    void Error(const char* category, const char* message);

    // Statistics
    int32_t GetObjectCount() const;
    int32_t GetClassCount() const;
    void DumpObjects() const;
    void DumpClasses() const;

private:
    UE4Object* GetObject(uint32_t id);
    const UE4Object* GetObject(uint32_t id) const;
    UE4Class* GetClass(uint32_t id);
    const UE4Class* GetClass(uint32_t id) const;
    
    uint32_t CreateObject(UE4ObjectType type, const char* name, const char* className);
    void InitializeBuiltInClasses();
    void InitializeBuiltInFunctions();

    bool m_initialized = false;
    UE4EngineState m_engineState;
    
    std::vector<std::unique_ptr<UE4Object>> m_objects;
    std::vector<std::unique_ptr<UE4Class>> m_classes;
    std::unordered_map<std::string, uint32_t> m_classNameToId;
    std::unordered_map<std::string, uint32_t> m_objectNameToId;
    
    uint32_t m_nextObjectId = 1;
    uint32_t m_nextClassId = 1;
    
    std::unordered_map<std::string, std::function<void()>> m_consoleCommands;
    
    int64_t m_startTime = 0;
};

// Global UE4 HLE instance
UE4HLE& GetUE4HLE();

// Native function signatures
using UE4NativeFunc = int32_t(*)(void* params);

// Register native implementations
void RegisterUENative(const char* name, UE4NativeFunc func);

} // namespace Kyty::Libs
