#pragma once
#include "kronyx/physics.h"
#include "kronyx/memory.h"

#define KY_PHYSICS_MAX_BODIES 1024
#define KY_PHYSICS_MAX_COLLIDERS 512
#define KY_PHYSICS_MAX_PAIRS 4096
#define KY_PHYSICS_MAX_SAP_EVENTS 2048
#define KY_PHYSICS_MAX_FORCE_FIELDS KY_FORCE_FIELD_MAX

typedef struct kyPhysBody {
    kyRigidBody body;
    int         alive;
    kyVec3      aabb_min;
    kyVec3      aabb_max;
} kyPhysBody;

typedef struct kyPhysCollider {
    kyCollider collider;
    int        alive;
} kyPhysCollider;

typedef struct kySAPEvent {
    float   coord;
    int     body_idx;
    int     axis;
    int     min_event;
} kySAPEvent;

typedef struct kyContactPair {
    uint32_t body_a;
    uint32_t body_b;
    int      alive;
} kyContactPair;

typedef struct kyPhysForceField {
    kyForceField  field;
    int           alive;
    uint32_t      id;
} kyPhysForceField;

struct kyPhysicsWorld {
    kyAllocator     alloc;
    kyVec3          gravity;
    kyPhysBody     bodies[KY_PHYSICS_MAX_BODIES];
    int             body_count;
    kyPhysCollider colliders[KY_PHYSICS_MAX_COLLIDERS];
    int             collider_count;
    kySAPEvent     sap_events[KY_PHYSICS_MAX_SAP_EVENTS * 2];
    int             sap_event_count;
    kyContactPair  pairs[KY_PHYSICS_MAX_PAIRS];
    int             pair_count;
    uint32_t        next_body_id;
    uint32_t        next_collider_id;
    
    /* Force fields */
    kyPhysForceField force_fields[KY_PHYSICS_MAX_FORCE_FIELDS];
    int               force_field_count;
    uint32_t          next_force_field_id;
};

static inline int sap_event_cmp(const void *a, const void *b) {
    float va = ((const kySAPEvent *)a)->coord;
    float vb = ((const kySAPEvent *)b)->coord;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static inline void phys_body_update_aabb(kyPhysBody *b, const kyPhysicsWorld *pw) {
    const kyRigidBody *r = &b->body;
    const kyCollider *c = NULL;
    uint32_t cid = r->collider_id;
    for (int i = 0; i < pw->collider_count; i++) {
        if (pw->colliders[i].alive && (uint32_t)i + 1 == cid) {
            c = &pw->colliders[i].collider;
            break;
        }
    }
    if (!c) {
        b->aabb_min = (kyVec3){-1e10f, -1e10f, -1e10f};
        b->aabb_max = (kyVec3){ 1e10f,  1e10f,  1e10f};
        return;
    }
    switch (c->shape) {
        case KY_SHAPE_SPHERE: {
            float rad = c->u.sphere.radius;
            b->aabb_min = ky_vec3_sub(r->position, (kyVec3){rad, rad, rad});
            b->aabb_max = ky_vec3_add(r->position, (kyVec3){rad, rad, rad});
            break;
        }
        case KY_SHAPE_BOX: {
            b->aabb_min = ky_vec3_sub(r->position, c->u.box.half_extents);
            b->aabb_max = ky_vec3_add(r->position, c->u.box.half_extents);
            break;
        }
        default:
            b->aabb_min = (kyVec3){-1e10f, -1e10f, -1e10f};
            b->aabb_max = (kyVec3){ 1e10f,  1e10f,  1e10f};
            break;
    }
}
