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
    int         has_aabb; /* 1 when a valid collider-derived AABB is set */
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

/* Joint / constraint entry (G9). Solved in phys_apply_constraints after
 * collision resolution. body_id 0 on either end = static world. */
typedef struct kyPhysConstraint {
    kyConstraintDesc desc;
    int       alive;
    uint32_t  id;      /* 1-based public id */
} kyPhysConstraint;

struct kyPhysicsWorld {
    kyAllocator     alloc;
    kyVec3          gravity;
    kyPhysBody     bodies[KY_PHYSICS_MAX_BODIES];
    int             body_count;
    kyPhysCollider colliders[KY_PHYSICS_MAX_COLLIDERS];
    int             collider_count;
    kySAPEvent     sap_events[KY_PHYSICS_MAX_SAP_EVENTS * 2];
    int             sap_event_count;
    int             sap_active[KY_PHYSICS_MAX_BODIES];
    kyContactPair  pairs[KY_PHYSICS_MAX_PAIRS];
    int             pair_count;
    uint32_t        next_body_id;
    uint32_t        next_collider_id;

    /* Reverse index: collider_id (1-based) -> storage index+1 in
     * colliders[], or 0 when no live collider carries that id. Rebuilt
     * by add_collider. O(1) lookup replaces the per-call O(n) scan in
     * phys_body_update_aabb / cast_ray. */
    int             collider_index[KY_PHYSICS_MAX_COLLIDERS + 1];
    
    /* Force fields */
    kyPhysForceField force_fields[KY_PHYSICS_MAX_FORCE_FIELDS];
    int               force_field_count;
    uint32_t          next_force_field_id;

    /* Custom collision hooks — when non-NULL, these override the built-in
     * SAP broadphase (set_broadphase) and AABB narrowphase (set_narrowphase).
     * When NULL the built-in implementations are used. */
    kyPhysicsBroadFn  broad_fn;
    kyPhysicsNarrowFn narrow_fn;

    /* Pre-allocated scratch buffers for custom broadphase (avoid per-frame
     * calloc/free when broad_fn is set). */
    kyExtents *broad_exts;
    uint32_t  *broad_ids;

    /* Joint / constraint system (G9): static storage, no heap allocation. */
    kyPhysConstraint constraints[KY_PHYSICS_MAX_CONSTRAINTS];
    int               constraint_count;
    uint32_t          next_constraint_id;
};

static inline int sap_event_cmp(const void *a, const void *b) {
    const kySAPEvent *ea = (const kySAPEvent *)a;
    const kySAPEvent *eb = (const kySAPEvent *)b;
    if (ea->coord < eb->coord) return -1;
    if (ea->coord > eb->coord) return 1;
    /* Tie-break: min events before max events at same coord, then body idx */
    if (ea->min_event != eb->min_event) return ea->min_event ? -1 : 1;
    if (ea->body_idx != eb->body_idx) return ea->body_idx < eb->body_idx ? -1 : 1;
    return 0;
}

static inline void phys_body_update_aabb(kyPhysBody *b, const kyPhysicsWorld *pw) {
    const kyRigidBody *r = &b->body;
    const kyCollider *c = NULL;
    uint32_t cid = r->collider_id;
    int entry = (cid <= (uint32_t)KY_PHYSICS_MAX_COLLIDERS) ? pw->collider_index[cid] : 0;
    if (entry && pw->colliders[entry - 1].alive) c = &pw->colliders[entry - 1].collider;
    if (!c) {
        b->has_aabb = 0;
        b->aabb_min = (kyVec3){0, 0, 0};
        b->aabb_max = (kyVec3){0, 0, 0};
        return;
    }
    b->has_aabb = 1;
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
            b->has_aabb = 0;
            b->aabb_min = (kyVec3){0, 0, 0};
            b->aabb_max = (kyVec3){0, 0, 0};
            break;
    }
}
