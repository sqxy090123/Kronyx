#ifndef KRONYX_PHYSICS_H
#define KRONYX_PHYSICS_H

#include "defines.h"
#include "math.h"

typedef enum kyColliderShape {
    KY_SHAPE_SPHERE = 0,
    KY_SHAPE_BOX,
    KY_SHAPE_CAPSULE,
    KY_SHAPE_PLANE,
    KY_SHAPE_CONVEX_MESH,
    KY_SHAPE_TRI_MESH
} kyColliderShape;

typedef struct kyCollider {
    kyColliderShape shape;
    union {
        kySphere sphere;
        struct { kyVec3 center; kyVec3 half_extents; } box;
        struct { float radius; float half_height; } capsule;
        struct { kyVec3 normal; float d; } plane;
    } u;
} kyCollider;

typedef struct kyRigidBody {
    kyVec3   position;
    kyQuat   rotation;
    kyVec3   linear_velocity;
    kyVec3   angular_velocity;
    float    inv_mass;
    float    inv_inertia[3];
    float    restitution;
    float    friction;
    uint32_t collider_id;
    uint32_t flags;
    void    *user_data;
} kyRigidBody;

typedef struct kyRayHit {
    int    hit;
    float  t;
    kyVec3 normal;
    uint32_t body_id;
} kyRayHit;

typedef struct kyPhysicsWorld kyPhysicsWorld;

KY_API kyPhysicsWorld *ky_physics_create(kyVec3 gravity);
KY_API void            ky_physics_destroy(kyPhysicsWorld *pw);
KY_API uint32_t        ky_physics_add_collider(kyPhysicsWorld *pw, const kyCollider *c);
KY_API uint32_t        ky_physics_add_body(kyPhysicsWorld *pw, const kyRigidBody *b);
KY_API void            ky_physics_set_gravity(kyPhysicsWorld *pw, kyVec3 g);
KY_API void            ky_physics_step(kyPhysicsWorld *pw, float dt);
KY_API void            ky_physics_apply_impulse(kyPhysicsWorld *pw, uint32_t body, kyVec3 impulse, kyVec3 at);
KY_API void            ky_physics_get_body(const kyPhysicsWorld *pw, uint32_t body, kyRigidBody *out);
KY_API void            ky_physics_cast_ray(const kyPhysicsWorld *pw, kyVec3 origin, kyVec3 dir, float max_t, kyRayHit *out_hit);
KY_API void            ky_physics_get_aabb(const kyPhysicsWorld *pw, uint32_t body, kyVec3 *min_out, kyVec3 *max_out);

#endif

/* Force field support */
#include "kronyx/force_field.h"

KY_API uint32_t      ky_physics_add_force_field(kyPhysicsWorld *pw, const kyForceField *field);
KY_API int           ky_physics_remove_force_field(kyPhysicsWorld *pw, uint32_t id);
KY_API int           ky_physics_get_force_field_count(const kyPhysicsWorld *pw);
KY_API kyForceField  ky_physics_get_force_field(const kyPhysicsWorld *pw, uint32_t id);
KY_API void          ky_physics_set_force_field(kyPhysicsWorld *pw, uint32_t id, kyForceField field);
