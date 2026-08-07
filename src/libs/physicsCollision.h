// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KYTY_LIBS_PHYSICS_COLLISION_H_
#define KYTY_LIBS_PHYSICS_COLLISION_H_

#include "common/common.h"

#include <cstdint>
#include <vector>

namespace Libs::PhysicsCollision {

// ─── RAGE / Bullet-style collision primitives ────────────────────────────────
//
// GTA V uses a modified Bullet Physics library through RAGE. The guest allocates
// collision worlds, adds rigid bodies, and performs ray/sweep tests each frame.
// This scaffolding provides the data structures and stub returns so the game's
// initialisation code paths complete without hard-exiting.

enum class CollisionShapeType : uint32_t {
	Box       = 0,
	Sphere    = 1,
	Capsule   = 2,
	Cylinder  = 3,
	Convex    = 4,
	TriangleMesh = 5,
	HeightField  = 6,
};

struct Vec3 {
	float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Quat {
	float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

struct Transform {
	Vec3 position;
	Quat rotation;
	Vec3 scale {1.0f, 1.0f, 1.0f};
};

struct CollisionShape {
	uint32_t          shape_id = 0;
	CollisionShapeType type    = CollisionShapeType::Box;
	Vec3              half_extents {0.5f, 0.5f, 0.5f};
	float             radius     = 0.5f;
	float             height     = 1.0f;
};

struct RigidBody {
	uint32_t    body_id     = 0;
	uint32_t    shape_id    = 0;
	Transform   transform;
	float       mass        = 1.0f;
	float       friction    = 0.5f;
	float       restitution = 0.0f;
	bool        is_static   = false;
	bool        is_kinematic = false;
	uint32_t    collision_group = 0;
	uint32_t    collision_mask  = 0xFFFFFFFF;
};

struct RayHit {
	bool     hit        = false;
	Vec3     hit_point;
	Vec3     hit_normal;
	float    distance   = 0.0f;
	uint32_t body_id    = 0;
};

struct CollisionWorld {
	uint32_t             world_id = 0;
	Vec3                 gravity {0.0f, 0.0f, -9.81f};
	std::vector<RigidBody> bodies;
};

// ─── Public API ──────────────────────────────────────────────────────────────

void Initialize();
void Shutdown();

uint32_t CreateWorld(const Vec3& gravity = {0.0f, 0.0f, -9.81f});
void     DestroyWorld(uint32_t world_id);

uint32_t CreateShape(CollisionShapeType type, const Vec3& half_extents = {},
                     float radius = 0.5f, float height = 1.0f);

uint32_t AddRigidBody(uint32_t world_id, uint32_t shape_id,
                      const Transform& transform, float mass = 1.0f);
void     RemoveRigidBody(uint32_t world_id, uint32_t body_id);

void StepSimulation(uint32_t world_id, float dt);

RayHit Raycast(uint32_t world_id, const Vec3& from, const Vec3& to);

void SetGravity(uint32_t world_id, const Vec3& gravity);

} // namespace Libs::PhysicsCollision

#endif // KYTY_LIBS_PHYSICS_COLLISION_H_
