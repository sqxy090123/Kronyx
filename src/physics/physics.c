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

    /* Apply force fields BEFORE gravity integration so they affect velocity this frame */
    phys_apply_force_fields(pw, dt);

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

    /* SAP broadphase to find potential collision pairs */
    sap_build_events(pw);
    sap_find_pairs(pw);

    /* Narrow-phase: detect and resolve overlaps between collided bodies */
    for (int i = 0; i < pw->pair_count; i++) {
        kyContactPair *pair = &pw->pairs[i];
        if (!pair->alive) continue;
        int ia = (int)pair->body_a - 1;
        int ib = (int)pair->body_b - 1;
        if (ia < 0 || ia >= pw->body_count || ib < 0 || ib >= pw->body_count) continue;
        kyPhysBody *ba = &pw->bodies[ia];
        kyPhysBody *bb = &pw->bodies[ib];
        if (!ba->alive || !bb->alive) continue;

        /* Simple AABB overlap resolution (push apart) */
        kyVec3 overlap = ky_vec3_sub(
            ky_vec3_add(ba->aabb_min, ky_vec3_scale(ky_vec3_sub(ba->aabb_max, ba->aabb_min), 0.5f)),
            ky_vec3_add(bb->aabb_min, ky_vec3_scale(ky_vec3_sub(bb->aabb_max, bb->aabb_min), 0.5f))
        );
        kyVec3 abs_overlap;
        abs_overlap.x = overlap.x < 0 ? -overlap.x : overlap.x;
        abs_overlap.y = overlap.y < 0 ? -overlap.y : overlap.y;
        abs_overlap.z = overlap.z < 0 ? -overlap.z : overlap.z;

        float axis = abs_overlap.x;
        int ax = 0;
        if (abs_overlap.y < axis) { axis = abs_overlap.y; ax = 1; }
        if (abs_overlap.z < axis) { axis = abs_overlap.z; ax = 2; }

        if (axis > 0.0f) {
            float push = axis + 0.001f;
            kyVec3 push_dir = ky_vec3_zero();
            if (ax == 0) push_dir.x = overlap.x > 0 ? 1.0f : -1.0f;
            else if (ax == 1) push_dir.y = overlap.y > 0 ? 1.0f : -1.0f;
            else push_dir.z = overlap.z > 0 ? 1.0f : -1.0f;
            push_dir = ky_vec3_normalize(push_dir);

            if (ba->body.inv_mass > 0.0f) {
                ba->body.position = ky_vec3_add(ba->body.position,
                    ky_vec3_scale(push_dir, push * ba->body.inv_mass / (ba->body.inv_mass + bb->body.inv_mass)));
            }
            if (bb->body.inv_mass > 0.0f) {
                bb->body.position = ky_vec3_sub(bb->body.position,
                    ky_vec3_scale(push_dir, push * bb->body.inv_mass / (ba->body.inv_mass + bb->body.inv_mass)));
                phys_body_update_aabb(bb, pw);
            }
            phys_body_update_aabb(ba, pw);
        }
    }
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
    if (!out_hit) return;
    out_hit->hit = 0;
    out_hit->t = max_t;
    out_hit->body_id = 0;
    if (!pw) return;

    kyVec3 inv_dir;
    inv_dir.x = dir.x != 0.0f ? 1.0f / dir.x : (dir.x > 0 ? 1e30f : -1e30f);
    inv_dir.y = dir.y != 0.0f ? 1.0f / dir.y : (dir.y > 0 ? 1e30f : -1e30f);
    inv_dir.z = dir.z != 0.0f ? 1.0f / dir.z : (dir.z > 0 ? 1e30f : -1e30f);

    float best_t = max_t;
    uint32_t best_id = 0;
    kyVec3 best_normal = ky_vec3_zero();

    for (int i = 0; i < pw->body_count; i++) {
        kyPhysBody *b = &pw->bodies[i];
        if (!b->alive) continue;

        /* Test vs sphere collider */
        uint32_t cid = b->body.collider_id;
        kySphere *sph = NULL;
        for (int j = 0; j < pw->collider_count; j++) {
            if (pw->colliders[j].alive && (uint32_t)j + 1 == cid) {
                if (pw->colliders[j].collider.shape == KY_SHAPE_SPHERE) {
                    sph = &pw->colliders[j].collider.u.sphere;
                }
                break;
            }
        }
        if (sph) {
            kyVec3 oc = ky_vec3_sub(origin, sph->center);
            float a = ky_vec3_dot(dir, dir);
            float b2 = 2.0f * ky_vec3_dot(oc, dir);
            float c = ky_vec3_dot(oc, oc) - sph->radius * sph->radius;
            float disc = b2 * b2 - 4.0f * a * c;
            if (disc >= 0.0f) {
                float t = (-b2 - sqrtf(disc)) / (2.0f * a);
                if (t > 0.0f && t < best_t) {
                    best_t = t;
                    best_id = (uint32_t)(i + 1);
                    best_normal = ky_vec3_normalize(ky_vec3_sub(
                        ky_vec3_scale(dir, t), ky_vec3_sub(origin, sph->center)));
                }
            }
        }

        /* Test vs box collider using existing ray-aabb */
        if (b->aabb_min.x < b->aabb_max.x) {
            kyAABB aabb = { b->aabb_min, b->aabb_max };
            float t_aabb;
            if (ky_ray_aabb(origin, inv_dir, best_t, &aabb, &t_aabb)) {
                /* More precise sphere test inside AABB for boxes */
                uint32_t bcid = b->body.collider_id;
                for (int j = 0; j < pw->collider_count; j++) {
                    if (pw->colliders[j].alive && (uint32_t)j + 1 == bcid &&
                        pw->colliders[j].collider.shape == KY_SHAPE_BOX) {
                        /* Box hit: use AABB hit point as approximation */
                        if (t_aabb < best_t) {
                            best_t = t_aabb;
                            best_id = (uint32_t)(i + 1);
                        }
                        break;
                    }
                }
            }
        }
    }

    if (best_id != 0) {
        out_hit->hit = 1;
        out_hit->t = best_t;
        out_hit->normal = best_normal;
        out_hit->body_id = best_id;
    }
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
