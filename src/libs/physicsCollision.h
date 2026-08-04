#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Kyty::Libs {

/**
 * Physics Collision Fix Module for GTA 3 Definitive Edition
 * 
 * This module provides collision detection and response fixes to prevent
 * cars clipping through objects, characters falling through floors, and
 * other physics-related glitches in GTA 3 DE.
 * 
 * Covers: Collision detection, raycasting, overlap tests, collision responses,
 *         and physics simulation fixes.
 */

// Collision channel types
enum class ECollisionChannel : uint8_t {
    ECC_WorldStatic = 0,
    ECC_WorldDynamic = 1,
    ECC_Pawn = 2,
    ECC_Visibility = 3,
    ECC_Camera = 4,
    ECC_PhysicsBody = 5,
    ECC_Vehicle = 6,
    ECC_Destructible = 7,
    ECC_GameTraceChannel1 = 8,
    ECC_GameTraceChannel2 = 9,
    ECC_GameTraceChannel3 = 10,
    ECC_GameTraceChannel4 = 11,
    ECC_GameTraceChannel5 = 12,
    ECC_GameTraceChannel6 = 13,
    ECC_Max = 14
};

// Collision response types
enum class ECollisionResponse : uint8_t {
    ECR_Ignore = 0,
    ECR_Overlap = 1,
    ECR_Block = 2
};

// Collision shape types
enum class ECollisionShapeType : uint8_t {
    Sphere = 0,
    Box = 1,
    Capsule = 2,
    Plane = 3,
    Convex = 4,
    Mesh = 5
};

// Hit result flags
enum class EHitResultFlags : uint32_t {
    None = 0x00000000,
    bBlockingHit = 0x00000001,
    bStartInside = 0x00000002,
    bInitialOverlap = 0x00000004,
    bTimeOfImpact = 0x00000008,
    bNormalImpacted = 0x00000010,
    bLocationImpacted = 0x00000020,
    bItemHit = 0x00000040,
    bFaceIndexHit = 0x00000080
};

/**
 * 3D Vector structure
 */
struct FVector3 {
    float X, Y, Z;
    
    FVector3() : X(0), Y(0), Z(0) {}
    FVector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    
    FVector3 operator+(const FVector3& other) const {
        return FVector3(X + other.X, Y + other.Y, Z + other.Z);
    }
    
    FVector3 operator-(const FVector3& other) const {
        return FVector3(X - other.X, Y - other.Y, Z - other.Z);
    }
    
    FVector3 operator*(float scale) const {
        return FVector3(X * scale, Y * scale, Z * scale);
    }
    
    float Dot(const FVector3& other) const {
        return X * other.X + Y * other.Y + Z * other.Z;
    }
    
    float Length() const {
        return std::sqrt(X * X + Y * Y + Z * Z);
    }
    
    FVector3 Normalized() const {
        float len = Length();
        if (len > 0.0f) {
            return FVector3(X / len, Y / len, Z / len);
        }
        return *this;
    }
};

/**
 * Collision shape descriptor
 */
struct CollisionShape {
    ECollisionShapeType type;
    FVector3 position;
    FVector3 rotation;
    FVector3 scale;
    float radius;      // For sphere/capsule
    float halfHeight;  // For capsule
    FVector3 extent;   // For box
    
    CollisionShape() : type(ECollisionShapeType::Sphere), radius(1.0f), halfHeight(1.0f) {}
};

/**
 * Hit result structure
 */
struct FHitResult {
    EHitResultFlags flags;
    float time;
    FVector3 location;
    FVector3 normal;
    FVector3 impactPoint;
    FVector3 impactNormal;
    uint32_t itemHit;
    uint32_t faceIndex;
    uint32_t hitActorId;
    ECollisionChannel hitChannel;
    
    FHitResult() : flags(EHitResultFlags::None), time(1.0f), itemHit(0), 
                   faceIndex(0), hitActorId(0), hitChannel(ECollisionChannel::ECC_WorldStatic) {}
    
    bool IsBlockingHit() const {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(EHitResultFlags::bBlockingHit)) != 0;
    }
    
    bool HasTimeOfImpact() const {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(EHitResultFlags::bTimeOfImpact)) != 0;
    }
};

/**
 * Collision response container
 */
struct CollisionResponseContainer {
    ECollisionResponse responseWorldStatic;
    ECollisionResponse responseWorldDynamic;
    ECollisionResponse responsePawn;
    ECollisionResponse responseVehicle;
    ECollisionResponse responsePhysicsBody;
    
    CollisionResponseContainer() : responseWorldStatic(ECollisionResponse::ECR_Block),
                                    responseWorldDynamic(ECollisionResponse::ECR_Block),
                                    responsePawn(ECollisionResponse::ECR_Overlap),
                                    responseVehicle(ECollisionResponse::ECR_Block),
                                    responsePhysicsBody(ECollisionResponse::ECR_Block) {}
};

/**
 * Physics Collision Fix System
 */
class PhysicsCollision {
public:
    static PhysicsCollision& Instance();
    
    /**
     * Initialize collision system
     * @return true if initialization succeeded
     */
    bool Initialize();
    
    /**
     * Shutdown collision system
     */
    void Shutdown();
    
    /**
     * Register a collision object
     * @param objectId Unique ID of the object
     * @param shape Collision shape
     * @param channel Collision channel
     * @param response Collision response
     * @return true if registration succeeded
     */
    bool RegisterObject(uint32_t objectId, const CollisionShape& shape,
                        ECollisionChannel channel,
                        const CollisionResponseContainer& response);
    
    /**
     * Update object transform
     * @param objectId ID of the object
     * @param position New position
     * @param rotation New rotation
     */
    void UpdateObjectTransform(uint32_t objectId, const FVector3& position, const FVector3& rotation);
    
    /**
     * Remove a collision object
     * @param objectId ID of the object to remove
     */
    void RemoveObject(uint32_t objectId);
    
    /**
     * Line trace (raycast)
     * @param start Start position
     * @param end End position
     * @param channels Channels to trace against
     * @param outHit Hit result
     * @return true if hit something
     */
    bool LineTraceSingle(const FVector3& start, const FVector3& end,
                         uint32_t channels, FHitResult& outHit);
    
    /**
     * Sphere trace
     * @param start Start position
     * @param end End position
     * @param radius Sphere radius
     * @param channels Channels to trace against
     * @param outHit Hit result
     * @return true if hit something
     */
    bool SphereTraceSingle(const FVector3& start, const FVector3& end,
                           float radius, uint32_t channels, FHitResult& outHit);
    
    /**
     * Box trace
     * @param start Start position
     * @param end End position
     * @param extent Box extent
     * @param rotation Box rotation
     * @param channels Channels to trace against
     * @param outHit Hit result
     * @return true if hit something
     */
    bool BoxTraceSingle(const FVector3& start, const FVector3& end,
                        const FVector3& extent, const FVector3& rotation,
                        uint32_t channels, FHitResult& outHit);
    
    /**
     * Overlap test sphere
     * @param position Sphere center
     * @param radius Sphere radius
     * @param channels Channels to test against
     * @param outHits Array of hit actor IDs
     * @return Number of overlaps
     */
    size_t OverlapTestSphere(const FVector3& position, float radius,
                             uint32_t channels, std::vector<uint32_t>& outHits);
    
    /**
     * Overlap test box
     * @param position Box center
     * @param extent Box extent
     * @param rotation Box rotation
     * @param channels Channels to test against
     * @param outHits Array of hit actor IDs
     * @return Number of overlaps
     */
    size_t OverlapTestBox(const FVector3& position, const FVector3& extent,
                          const FVector3& rotation, uint32_t channels,
                          std::vector<uint32_t>& outHits);
    
    /**
     * Sweep collision test
     * @param objectId ID of the object to sweep
     * @param start Start position
     * @param end End position
     * @param outHit Hit result
     * @return true if collision detected during sweep
     */
    bool SweepCollision(uint32_t objectId, const FVector3& start, const FVector3& end,
                        FHitResult& outHit);
    
    /**
     * Resolve collision penetration
     * @param objectId ID of the object
     * @param penetrationDepth Depth of penetration
     * @param penetrationNormal Normal of penetration
     * @return Resolved position
     */
    FVector3 ResolvePenetration(uint32_t objectId, float penetrationDepth,
                                const FVector3& penetrationNormal);
    
    /**
     * Enable collision for an object
     * @param objectId ID of the object
     * @param enabled Enable/disable
     */
    void SetCollisionEnabled(uint32_t objectId, bool enabled);
    
    /**
     * Check if collision is enabled for an object
     * @param objectId ID of the object
     * @return true if collision is enabled
     */
    bool IsCollisionEnabled(uint32_t objectId) const;
    
    /**
     * Get the number of registered objects
     * @return Number of objects
     */
    size_t GetObjectCount() const;
    
    /**
     * Set collision fix enabled
     * @param enabled Enable/disable fixes
     */
    void SetFixEnabled(bool enabled);
    
    /**
     * Check if fixes are enabled
     * @return true if fixes are enabled
     */
    bool IsFixEnabled() const;
    
private:
    PhysicsCollision() = default;
    ~PhysicsCollision() = default;
    
    PhysicsCollision(const PhysicsCollision&) = delete;
    PhysicsCollision& operator=(const PhysicsCollision&) = delete;
    
    struct CollisionObject;
    
    std::unordered_map<uint32_t, std::unique_ptr<CollisionObject>> m_objects;
    bool m_fixEnabled = true;
    bool m_initialized = false;
};

/**
 * Initialize GTA 3 DE specific collision fixes
 */
void InitializeGTA3CollisionFixes();

/**
 * Apply vehicle collision fixes
 */
void ApplyVehicleCollisionFixes();

/**
 * Apply character collision fixes
 */
void ApplyCharacterCollisionFixes();

/**
 * Apply world collision fixes
 */
void ApplyWorldCollisionFixes();

} // namespace Kyty::Libs
