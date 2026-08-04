#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Kyty::Libs {

/**
 * Extended UE4 Class Registry for GTA 3 Definitive Edition
 * 
 * This module provides additional UE4 class registrations and property
 * accessors needed for late-game missions and complex scripting scenarios.
 * 
 * Covers: AActor, APawn, ACharacter, AController, UActorComponent,
 *         UPrimitiveComponent, UStaticMeshComponent, USkeletalMeshComponent,
 *         UBoxComponent, USphereComponent, UCapsuleComponent, etc.
 */

// UE4 Class Flags
enum class EClassFlags : uint32_t {
    CLASS_None = 0x00000000,
    CLASS_Abstract = 0x00000001,
    CLASS_DefaultConfig = 0x00000002,
    CLASS_Config = 0x00000004,
    CLASS_Transient = 0x00000008,
    CLASS_MatchedSerializers = 0x00000010,
    CLASS_Native = 0x00000020,
    CLASS_NoExport = 0x00000040,
    CLASS_NotPlaceable = 0x00000080,
    CLASS_PerObjectConfig = 0x00000100,
    CLASS_ReplicationDataIsSetUp = 0x00000200,
    CLASS_EditInlineNew = 0x00000400,
    CLASS_CollapseCategories = 0x00000800,
    CLASS_Interface = 0x00001000,
    CLASS_CustomConstructor = 0x00002000,
    CLASS_Const = 0x00004000,
    CLASS_NeedsDeferredDependencyLoading = 0x00008000,
    CLASS_Deprecated = 0x00010000,
    CLASS_HideDropDown = 0x00020000,
    CLASS_HideFunctions = 0x00040000,
    CLASS_AllowAbstract = 0x00080000,
    CLASS_DefaultToInstanced = 0x00100000,
    CLASS_TokenStreamAssembled = 0x00200000,
    CLASS_HasInstancedReference = 0x00400000,
    CLASS_Hidden = 0x00800000,
    CLASS_DeprecatedForceNone = 0x01000000,
    CLASS_Optional = 0x02000000,
    CLASS_Within = 0x04000000,
    CLASS_WithinActorComponent = 0x08000000,
    CLASS_NeverLoadOnClient = 0x10000000,
    CLASS_SafeReplace = 0x20000000,
    CLASS_UnsafeReplace = 0x40000000,
    CLASS_KeepDebugInfoOnCooked = 0x80000000,
};

// UE4 Property Flags
enum class EPropertyFlags : uint64_t {
    CPF_None = 0x0000000000000000,
    CPF_Edit = 0x0000000000000001,
    CPF_ConstParm = 0x0000000000000002,
    CPF_BlueprintVisible = 0x0000000000000004,
    CPF_ExportObject = 0x0000000000000008,
    CPF_BlueprintReadOnly = 0x0000000000000010,
    CPF_Net = 0x0000000000000020,
    CPF_EditFixedSize = 0x0000000000000040,
    CPF_Parm = 0x0000000000000080,
    CPF_OutParm = 0x0000000000000100,
    CPF_ZeroConstructor = 0x0000000000000200,
    CPF_ReturnParm = 0x0000000000000400,
    CPF_DisableEditOnTemplate = 0x0000000000000800,
    CPF_Transient = 0x0000000000001000,
    CPF_Config = 0x0000000000002000,
    CPF_DisableEditOnInstance = 0x0000000000004000,
    CPF_EditConst = 0x0000000000008000,
    CPF_GlobalConfig = 0x0000000000010000,
    CPF_InstancedReference = 0x0000000000020000,
    CPF_DuplicateTransient = 0x0000000000040000,
    CPF_SaveGame = 0x0000000000080000,
    CPF_NoClear = 0x0000000000100000,
    CPF_ReferenceParm = 0x0000000000200000,
    CPF_BlueprintAssignable = 0x0000000000400000,
    CPF_Deprecated = 0x0000000000800000,
    CPF_IsPlainOldData = 0x0000000001000000,
    CPF_RepSkip = 0x0000000002000000,
    CPF_RepNotify = 0x0000000004000000,
    CPF_Interp = 0x0000000008000000,
    CPF_NonTransactional = 0x0000000010000000,
    CPF_EditorOnly = 0x0000000020000000,
    CPF_NoDestructor = 0x0000000040000000,
    CPF_AutoWeak = 0x0000000080000000,
    CPF_ContainsInstancedReference = 0x0000000100000000,
    CPF_AssetRegistrySearchable = 0x0000000200000000,
    CPF_SimpleDisplay = 0x0000000400000000,
    CPF_AdvancedDisplay = 0x0000000800000000,
    CPF_Protected = 0x0000001000000000,
    CPF_BlueprintCallable = 0x0000002000000000,
    CPF_BlueprintAuthorityOnly = 0x0000004000000000,
    CPF_TextExportTransient = 0x0000008000000000,
    CPF_NonPIEDuplicateTransient = 0x0000010000000000,
    CPF_ExposeOnSpawn = 0x0000020000000000,
    CPF_PersistentInstance = 0x0000040000000000,
    CPF_UObjectWrapper = 0x0000080000000000,
    CPF_NativeAccessSpecifierPublic = 0x0000100000000000,
    CPF_NativeAccessSpecifierProtected = 0x0000200000000000,
    CPF_NativeAccessSpecifierPrivate = 0x0000400000000000,
    CPF_SkipSerialization = 0x0000800000000000,
};

// Forward declarations
class UObject;
class UClass;
class UField;
class UStruct;

// UProperty definition (needed for make_unique)
struct UProperty {
    std::string name;
    std::string type;
    uint64_t flags = 0;
    size_t offset = 0;
    size_t size = 0;
};

/**
 * UE4 Class Registration Entry
 */
struct UE4ClassEntry {
    std::string className;
    std::string superClassName;
    uint32_t classFlags;
    uint32_t classSize;
    std::unordered_map<std::string, UProperty*> properties;
    std::unordered_map<std::string, std::function<void*()>> functionStubs;
    
    UE4ClassEntry() : classFlags(0), classSize(0) {}
};

/**
 * UE4 Property Descriptor
 */
struct UE4PropertyDesc {
    std::string name;
    std::string type;
    uint64_t flags;
    size_t offset;
    size_t size;
    
    UE4PropertyDesc() : flags(0), offset(0), size(0) {}
};

/**
 * Extended UE4 Class Registry
 * 
 * Manages registration and lookup of UE4 classes required for
 * GTA 3 DE late-game missions and complex scripting.
 */
class UE4ClassesExtended {
public:
    static UE4ClassesExtended& Instance();
    
    /**
     * Register a new UE4 class
     * @param className Name of the class
     * @param superClassName Name of the parent class
     * @param classSize Size of the class in bytes
     * @param flags Class flags
     * @return true if registration succeeded
     */
    bool RegisterClass(const std::string& className, 
                       const std::string& superClassName,
                       uint32_t classSize,
                       uint32_t flags = 0);
    
    /**
     * Register a property on a class
     * @param className Name of the class
     * @param propertyName Name of the property
     * @param propertyType Type of the property
     * @param offset Offset within the class
     * @param size Size of the property
     * @param flags Property flags
     * @return true if registration succeeded
     */
    bool RegisterProperty(const std::string& className,
                          const std::string& propertyName,
                          const std::string& propertyType,
                          size_t offset,
                          size_t size,
                          uint64_t flags = 0);
    
    /**
     * Register a function stub for a class
     * @param className Name of the class
     * @param functionName Name of the function
     * @param stub Function stub to call
     * @return true if registration succeeded
     */
    bool RegisterFunction(const std::string& className,
                          const std::string& functionName,
                          std::function<void*()> stub);
    
    /**
     * Find a class by name
     * @param className Name of the class to find
     * @return Pointer to class entry, nullptr if not found
     */
    UE4ClassEntry* FindClass(const std::string& className);
    
    /**
     * Check if a class is registered
     * @param className Name of the class
     * @return true if class is registered
     */
    bool IsClassRegistered(const std::string& className) const;
    
    /**
     * Get the number of registered classes
     * @return Number of registered classes
     */
    size_t GetClassCount() const;
    
    /**
     * Initialize all extended classes for GTA 3 DE
     * @return true if initialization succeeded
     */
    bool Initialize();
    
    /**
     * Shutdown and cleanup
     */
    void Shutdown();
    
private:
    UE4ClassesExtended() = default;
    ~UE4ClassesExtended() = default;
    
    UE4ClassesExtended(const UE4ClassesExtended&) = delete;
    UE4ClassesExtended& operator=(const UE4ClassesExtended&) = delete;
    
    std::unordered_map<std::string, std::unique_ptr<UE4ClassEntry>> m_classes;
    bool m_initialized = false;
};

/**
 * Register all actor-related classes
 */
void RegisterActorClasses();

/**
 * Register all component-related classes
 */
void RegisterComponentClasses();

/**
 * Register all gameplay-related classes
 */
void RegisterGameplayClasses();

/**
 * Register all mission-specific classes
 */
void RegisterMissionClasses();

} // namespace Kyty::Libs
