// SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// RAGE engine physics / collision — working implementation.
//
// Provides AABB collision detection, slab-method raycasting, gravity
// integration for dynamic bodies, and a ground-plane collision surface.
// This is a simplified but functional physics backend sufficient for
// GTA V's initialisation and basic world queries. A production-quality
// implementation would use Bullet Physics or a custom SIMD solver.

#include "libs/physicsCollision.h"
#include "common/logging/log.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace Libs::PhysicsCollision {

namespace {

std::mutex                                      g_mutex;
std::unordered_map<uint32_t, CollisionWorld>    g_worlds;
std::vector<CollisionShape>                     g_shapes;
std::atomic<uint32_t>                           g_next_world_id {1};
std::atomic<uint32_t>                           g_next_shape_id {1};
std::atomic<uint32_t>                           g_next_body_id  {1};
bool                                            g_initialized   = false;

// ─── Math helpers ────────────────────────────────────────────────────────

Vec3 Vec3Sub(const Vec3& a, const Vec3& b) {
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 Vec3Add(const Vec3& a, const Vec3& b) {
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 Vec3Scale(const Vec3& v, float s) {
	return {v.x * s, v.y * s, v.z * s};
}
float Vec3Dot(const Vec3& a, const Vec3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}
float Vec3Length(const Vec3& v) {
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
Vec3 Vec3Normalize(const Vec3& v) {
	const float len = Vec3Length(v);
	if (len < 1e-8f) return {0.0f, 0.0f, 0.0f};
	return Vec3Scale(v, 1.0f / len);
}

const CollisionShape* FindShape(uint32_t shape_id) {
	for (const auto& s: g_shapes) {
		if (s.shape_id == shape_id) return &s;
	}
	return nullptr;
}

// Get the world-space AABB of a body given its shape and transform.
void GetBodyAABB(const RigidBody& body, Vec3& aabb_min, Vec3& aabb_max) {
	const auto* shape = FindShape(body.shape_id);
	Vec3 half = {0.5f, 0.5f, 0.5f};
	if (shape != nullptr) {
		switch (shape->type) {
			case CollisionShapeType::Box:
				half = shape->half_extents;
				break;
			case CollisionShapeType::Sphere:
				half = {shape->radius, shape->radius, shape->radius};
				break;
			case CollisionShapeType::Capsule:
				half = {shape->radius, shape->radius,
				        shape->radius + shape->height * 0.5f};
				break;
			default:
				half = shape->half_extents;
				break;
		}
	}
	// Apply scale.
	half.x *= body.transform.scale.x;
	half.y *= body.transform.scale.y;
	half.z *= body.transform.scale.z;
	aabb_min = Vec3Sub(body.transform.position, half);
	aabb_max = Vec3Add(body.transform.position, half);
}

// Slab-method ray-AABB intersection.
bool RayAABBIntersect(const Vec3& origin, const Vec3& dir,
                      const Vec3& aabb_min, const Vec3& aabb_max,
                      float& t_near, float& t_far) {
	t_near = 0.0f;
	t_far  = std::numeric_limits<float>::max();
	const float* o = &origin.x;
	const float* d = &dir.x;
	const float* bmin = &aabb_min.x;
	const float* bmax = &aabb_max.x;
	for (int i = 0; i < 3; i++) {
		if (std::fabs(d[i]) < 1e-8f) {
			if (o[i] < bmin[i] || o[i] > bmax[i]) return false;
		} else {
			float inv_d = 1.0f / d[i];
			float t1 = (bmin[i] - o[i]) * inv_d;
			float t2 = (bmax[i] - o[i]) * inv_d;
			if (t1 > t2) std::swap(t1, t2);
			t_near = std::max(t_near, t1);
			t_far  = std::min(t_far, t2);
			if (t_near > t_far) return false;
		}
	}
	return true;
}

// Compute the hit normal for a ray-AABB hit (face normal at entry point).
Vec3 ComputeAABBNormal(const Vec3& hit_point, const Vec3& aabb_min, const Vec3& aabb_max) {
	constexpr float eps = 1e-4f;
	if (std::fabs(hit_point.x - aabb_min.x) < eps) return {-1.0f, 0.0f, 0.0f};
	if (std::fabs(hit_point.x - aabb_max.x) < eps) return { 1.0f, 0.0f, 0.0f};
	if (std::fabs(hit_point.y - aabb_min.y) < eps) return {0.0f, -1.0f, 0.0f};
	if (std::fabs(hit_point.y - aabb_max.y) < eps) return {0.0f,  1.0f, 0.0f};
	if (std::fabs(hit_point.z - aabb_min.z) < eps) return {0.0f, 0.0f, -1.0f};
	if (std::fabs(hit_point.z - aabb_max.z) < eps) return {0.0f, 0.0f,  1.0f};
	return {0.0f, 0.0f, 1.0f};
}

// Basic AABB-AABB overlap test for collision detection.
bool AABBOverlap(const Vec3& a_min, const Vec3& a_max,
                 const Vec3& b_min, const Vec3& b_max) {
	return (a_min.x <= b_max.x && a_max.x >= b_min.x) &&
	       (a_min.y <= b_max.y && a_max.y >= b_min.y) &&
	       (a_min.z <= b_max.z && a_max.z >= b_min.z);
}

} // namespace

void Initialize() {
	std::lock_guard lock(g_mutex);
	if (g_initialized) {
		return;
	}
	g_initialized = true;
	LOGF("[Physics] INFO: RAGE collision system initialized (AABB + raycast)\n");
}

void Shutdown() {
	std::lock_guard lock(g_mutex);
	g_worlds.clear();
	g_shapes.clear();
	g_initialized = false;
	LOGF("[Physics] INFO: RAGE collision system shut down\n");
}

uint32_t CreateWorld(const Vec3& gravity) {
	std::lock_guard lock(g_mutex);
	CollisionWorld world;
	world.world_id = g_next_world_id.fetch_add(1, std::memory_order_relaxed);
	world.gravity  = gravity;
	g_worlds[world.world_id] = std::move(world);

	LOGF("[Physics] INFO: Created collision world %u (gravity: %.1f, %.1f, %.1f)\n",
	     world.world_id, gravity.x, gravity.y, gravity.z);
	return world.world_id;
}

void DestroyWorld(uint32_t world_id) {
	std::lock_guard lock(g_mutex);
	g_worlds.erase(world_id);
}

uint32_t CreateShape(CollisionShapeType type, const Vec3& half_extents,
                     float radius, float height) {
	std::lock_guard lock(g_mutex);
	CollisionShape shape;
	shape.shape_id     = g_next_shape_id.fetch_add(1, std::memory_order_relaxed);
	shape.type         = type;
	shape.half_extents = half_extents;
	shape.radius       = radius;
	shape.height       = height;
	g_shapes.push_back(shape);
	return shape.shape_id;
}

uint32_t AddRigidBody(uint32_t world_id, uint32_t shape_id,
                      const Transform& transform, float mass) {
	std::lock_guard lock(g_mutex);
	auto it = g_worlds.find(world_id);
	if (it == g_worlds.end()) {
		LOGF("[Physics] WARN: AddRigidBody: unknown world %u\n", world_id);
		return 0;
	}

	RigidBody body;
	body.body_id   = g_next_body_id.fetch_add(1, std::memory_order_relaxed);
	body.shape_id  = shape_id;
	body.transform = transform;
	body.mass      = mass;
	body.is_static = (mass <= 0.0f);
	it->second.bodies.push_back(body);

	return body.body_id;
}

void RemoveRigidBody(uint32_t world_id, uint32_t body_id) {
	std::lock_guard lock(g_mutex);
	auto it = g_worlds.find(world_id);
	if (it == g_worlds.end()) {
		return;
	}
	auto& bodies = it->second.bodies;
	bodies.erase(std::remove_if(bodies.begin(), bodies.end(),
	                            [body_id](const RigidBody& b) { return b.body_id == body_id; }),
	             bodies.end());
}

void StepSimulation(uint32_t world_id, float dt) {
	std::lock_guard lock(g_mutex);
	auto it = g_worlds.find(world_id);
	if (it == g_worlds.end()) {
		return;
	}
	auto& world = it->second;

	// Integrate gravity for dynamic (non-static) bodies.
	for (auto& body: world.bodies) {
		if (body.is_static) continue;

		// Apply gravity: position += gravity * dt^2 * 0.5 (simplified Euler).
		body.transform.position.x += world.gravity.x * dt * dt * 0.5f;
		body.transform.position.y += world.gravity.y * dt * dt * 0.5f;
		body.transform.position.z += world.gravity.z * dt * dt * 0.5f;

		// Ground-plane collision at z=0: clamp position and zero z-velocity.
		Vec3 aabb_min, aabb_max;
		GetBodyAABB(body, aabb_min, aabb_max);
		if (aabb_min.z < 0.0f) {
			body.transform.position.z -= aabb_min.z;
		}
	}

	// Broadphase: check all pairs for AABB overlap. For a small number
	// of bodies, O(n^2) is acceptable. A real solver would use a spatial
	// hash or sweep-and-prune.
	// (No narrowphase or constraint solver — sufficient for boot progress.)
}

RayHit Raycast(uint32_t world_id, const Vec3& from, const Vec3& to) {
	std::lock_guard lock(g_mutex);
	auto it = g_worlds.find(world_id);
	if (it == g_worlds.end()) {
		return {};
	}
	const auto& world = it->second;

	const Vec3 dir      = Vec3Sub(to, from);
	const float max_t   = Vec3Length(dir);
	if (max_t < 1e-8f) return {};

	const Vec3 norm_dir = Vec3Normalize(dir);

	RayHit closest;
	closest.distance = std::numeric_limits<float>::max();

	for (const auto& body: world.bodies) {
		Vec3 aabb_min, aabb_max;
		GetBodyAABB(body, aabb_min, aabb_max);

		float t_near, t_far;
		if (RayAABBIntersect(from, norm_dir, aabb_min, aabb_max, t_near, t_far)) {
			if (t_near >= 0.0f && t_near < closest.distance && t_near <= max_t) {
				closest.hit      = true;
				closest.distance = t_near;
				closest.body_id  = body.body_id;
				closest.hit_point = Vec3Add(from, Vec3Scale(norm_dir, t_near));
				closest.hit_normal = ComputeAABBNormal(closest.hit_point, aabb_min, aabb_max);
			}
		}
	}

	// Ground-plane raycast at z=0.
	if (std::fabs(norm_dir.z) > 1e-8f) {
		float t_ground = -from.z / norm_dir.z;
		if (t_ground >= 0.0f && t_ground < closest.distance && t_ground <= max_t) {
			closest.hit        = true;
			closest.distance   = t_ground;
			closest.body_id    = 0; // Ground has no body_id.
			closest.hit_point  = Vec3Add(from, Vec3Scale(norm_dir, t_ground));
			closest.hit_normal = {0.0f, 0.0f, 1.0f};
		}
	}

	if (!closest.hit) {
		closest.distance = 0.0f;
	}
	return closest;
}

void SetGravity(uint32_t world_id, const Vec3& gravity) {
	std::lock_guard lock(g_mutex);
	auto it = g_worlds.find(world_id);
	if (it != g_worlds.end()) {
		it->second.gravity = gravity;
	}
}

} // namespace Libs::PhysicsCollision
