#include "kronyx/physics.h"
#include "physics_internal.h"
#include <string.h>
#include <stdlib.h>

kyPhysicsWorld *ky_physics_create(kyVec3 gravity) {
    kyAllocator alloc = ky_default_allocator();
    kyPhysicsWorld *pw = (kyPhysicsWorld *)ky_mem_alloc(&alloc, sizeof(kyPhysicsWorld));
    if (!pw) return NULL;
    pw->alloc = alloc;
    pw->gravity = gravity;
    pw->body_count = 0;
    pw->collider_count = 0;
    pw->sap_event_count = 0;
    pw->pair_count = 0;
    pw->next_body_id = 1;
    pw->next_collider_id = 1;
    pw->force_field_count = 0;
    pw->next_force_field_id = 1;
    memset(pw->bodies, 0, sizeof(pw->bodies));
    memset(pw->colliders, 0, sizeof(pw->colliders));
    memset(pw->pairs, 0, sizeof(pw->pairs));
    memset(pw->force_fields, 0, sizeof(pw->force_fields));
    return pw;
}

void ky_physics_destroy(kyPhysicsWorld *pw) {
    if (pw) ky_mem_free(&pw->alloc, pw);
}

uint32_t ky_physics_add_collider(kyPhysicsWorld *pw, const kyCollider *c) {
    if (!pw || !c || pw->collider_count >= KY_PHYSICS_MAX_COLLIDERS) return 0;
    uint32_t id = pw->next_collider_id++;
    kyPhysCollider *col = &pw->colliders[pw->collider_count++];
    col->alive = 1;
    col->collider = *c;
    return id;
}

uint32_t ky_physics_add_body(kyPhysicsWorld *pw, const kyRigidBody *b) {
    if (!pw || !b || pw->body_count >= KY_PHYSICS_MAX_BODIES) return 0;
    uint32_t id = pw->next_body_id++;
    kyPhysBody *pb = &pw->bodies[pw->body_count++];
    pb->alive = 1;
    pb->body = *b;
    pb->body.flags = 0;
    phys_body_update_aabb(pb, pw);
    return id;
}

void ky_physics_set_gravity(kyPhysicsWorld *pw, kyVec3 g) {
    if (pw) pw->gravity = g;
}

/* ===== Force Field Implementation ===== */

uint32_t ky_physics_add_force_field(kyPhysicsWorld *pw, const kyForceField *field) {
    if (!pw || !field || pw->force_field_count >= KY_PHYSICS_MAX_FORCE_FIELDS) return 0;
    uint32_t id = pw->next_force_field_id++;
    kyPhysForceField *ff = &pw->force_fields[pw->force_field_count++];
    ff->alive = 1;
    ff->id = id;
    ff->field = *field;
    return id;
}

int ky_physics_remove_force_field(kyPhysicsWorld *pw, uint32_t id) {
    if (!pw) return 0;
    for (int i = 0; i < pw->force_field_count; i++) {
        if (pw->force_fields[i].alive && pw->force_fields[i].id == id) {
            pw->force_fields[i].alive = 0;
            for (int j = i; j < pw->force_field_count - 1; j++) {
                pw->force_fields[j] = pw->force_fields[j + 1];
            }
            pw->force_field_count--;
            return 1;
        }
    }
    return 0;
}

int ky_physics_get_force_field_count(const kyPhysicsWorld *pw) {
    if (!pw) return 0;
    return pw->force_field_count;
}

kyForceField ky_physics_get_force_field(const kyPhysicsWorld *pw, uint32_t id) {
    kyForceField empty = {{0,0,0}, 0, 0, KY_FORCE_FIELD_GRAVITY, NULL};
    if (!pw) return empty;
    for (int i = 0; i < pw->force_field_count; i++) {
        if (pw->force_fields[i].alive && pw->force_fields[i].id == id) {
            return pw->force_fields[i].field;
        }
    }
    return empty;
}

void ky_physics_set_force_field(kyPhysicsWorld *pw, uint32_t id, kyForceField field) {
    if (!pw) return;
    for (int i = 0; i < pw->force_field_count; i++) {
        if (pw->force_fields[i].alive && pw->force_fields[i].id == id) {
            pw->force_fields[i].field = field;
            return;
        }
    }
}

static void phys_apply_force_fields(kyPhysicsWorld *pw, float dt) {
    if (!pw || pw->force_field_count == 0) return;
    
    for (int i = 0; i < pw->body_count; i++) {
        kyPhysBody *b = &pw->bodies[i];
        if (!b->alive) continue;
        kyRigidBody *r = &b->body;
        if (r->inv_mass <= 0.0f) continue;
        
        float mass = 1.0f / r->inv_mass;
        kyVec3 total_force = ky_vec3_zero();
        
        for (int j = 0; j < pw->force_field_count; j++) {
            if (!pw->force_fields[j].alive) continue;
            kyVec3 force = ky_vec3_zero();
            ky_force_field_apply(&pw->force_fields[j].field, r->position, mass, &force);
            total_force = ky_vec3_add(total_force, force);
        }
        
        kyVec3 acceleration = ky_vec3_scale(total_force, r->inv_mass);
        r->linear_velocity.x += acceleration.x * dt;
        r->linear_velocity.y += acceleration.y * dt;
        r->linear_velocity.z += acceleration.z * dt;
    }
}

static void sap_build_events(kyPhysicsWorld *pw) {
    pw->sap_event_count = 0;
    for (int i = 0; i < pw->body_count; i++) {
        kyPhysBody *b = &pw->bodies[i];
        if (!b->alive) continue;
        if (pw->sap_event_count + 2 >= KY_PHYSICS_MAX_SAP_EVENTS) break;
        int base = pw->sap_event_count;
        pw->sap_events[base].coord = b->aabb_min.x;
        pw->sap_events[base].body_idx = i;
        pw->sap_events[base].axis = 0;
        pw->sap_events[base].min_event = 1;
        pw->sap_events[base + 1].coord = b->aabb_max.x;
        pw->sap_events[base + 1].body_idx = i;
        pw->sap_events[base + 1].axis = 0;
        pw->sap_events[base + 1].min_event = 0;
        pw->sap_event_count += 2;
    }
    qsort((void *)pw->sap_events, (size_t)pw->sap_event_count, sizeof(kySAPEvent),
          (int (*)(const void *, const void *))sap_event_cmp);
}

static void sap_find_pairs(kyPhysicsWorld *pw) {
    pw->pair_count = 0;
    int active[KY_PHYSICS_MAX_BODIES] = {0};
    for (int i = 0; i < pw->sap_event_count; i++) {
        kySAPEvent *e = &pw->sap_events[i];
        int idx = e->body_idx;
        if (e->min_event) {
            for (int j = 0; j < idx; j++) {
                if (active[j] && pw->pair_count < KY_PHYSICS_MAX_PAIRS) {
                    kyPhysBody *a = &pw->bodies[j];
                    kyPhysBody *bb = &pw->bodies[idx];
                    if (a->alive && bb->alive) {
                        kyContactPair *p = &pw->pairs[pw->pair_count++];
                        p->body_a = (uint32_t)(j + 1);
                        p->body_b = (uint32_t)(idx + 1);
                        p->alive = 1;
                    }
                }
            }
            active[idx] = 1;
        } else {
            active[idx] = 0;
        }
    }
}

void ky_physics_step(kyPhysicsWorld *pw, float dt) {
    if (!pw || dt <= 0.0f) return;

    for (int i = 0; i < pw->body_count; i++) {
        kyPhysBody *b = &pw->bodies[i];
        if (!b->alive) continue;
        kyRigidBody *r = &b->body;
        if (r->inv_mass <= 0.0f) continue;

        r->linear_velocity.x += pw->gravity.x * dt;
        r->linear_velocity.y += pw->gravity.y * dt;
        r->linear_velocity.z += pw->gravity.z * dt;

        r->position.x += r->linear_velocity.x * dt;
        r->position.y += r->linear_velocity.y * dt;
        r->position.z += r->linear_velocity.z * dt;

        phys_body_update_aabb(b, pw);
    }

    /* Apply force fields */
    phys_apply_force_fields(pw, dt);

    sap_build_events(pw);
    sap_find_pairs(pw);
}

void ky_physics_apply_impulse(kyPhysicsWorld *pw, uint32_t body_id, kyVec3 impulse, kyVec3 at) {
    KY_UNUSED(at);
    if (!pw) return;
    int idx = (int)body_id - 1;
    if (idx < 0 || idx >= pw->body_count) return;
    kyPhysBody *b = &pw->bodies[idx];
    if (!b->alive) return;
    kyRigidBody *r = &b->body;
    if (r->inv_mass <= 0.0f) return;
    r->linear_velocity.x += impulse.x * r->inv_mass;
    r->linear_velocity.y += impulse.y * r->inv_mass;
    r->linear_velocity.z += impulse.z * r->inv_mass;
}

void ky_physics_get_body(const kyPhysicsWorld *pw, uint32_t body_id, kyRigidBody *out) {
    if (!pw || !out) return;
    int idx = (int)body_id - 1;
    if (idx < 0 || idx >= pw->body_count) return;
    const kyPhysBody *b = &pw->bodies[idx];
    if (!b->alive) return;
    *out = b->body;
}

void ky_physics_cast_ray(const kyPhysicsWorld *pw, kyVec3 origin, kyVec3 dir,
                         float max_t, kyRayHit *out_hit) {
    KY_UNUSED(pw); KY_UNUSED(origin); KY_UNUSED(dir);
    if (!out_hit) return;
    out_hit->hit = 0;
    out_hit->t = max_t;
    out_hit->body_id = 0;
}

void ky_physics_get_aabb(const kyPhysicsWorld *pw, uint32_t body_id,
                         kyVec3 *min_out, kyVec3 *max_out) {
    if (!pw || !min_out || !max_out) return;
    int idx = (int)body_id - 1;
    if (idx < 0 || idx >= pw->body_count) return;
    const kyPhysBody *b = &pw->bodies[idx];
    if (!b->alive) return;
    *min_out = b->aabb_min;
    *max_out = b->aabb_max;
}
