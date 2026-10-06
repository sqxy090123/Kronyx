#include "kronyx/physics.h"
#include "physics_internal.h"
#include "kronyx/event.h"
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
    memset(pw->sap_active, 0, sizeof(pw->sap_active));
    memset(pw->collider_index, 0, sizeof(pw->collider_index));
    pw->broad_fn = NULL;
    pw->narrow_fn = NULL;
    pw->constraint_count = 0;
    pw->next_constraint_id = 1;
    memset(pw->constraints, 0, sizeof(pw->constraints));
    pw->broad_exts = (kyExtents *)calloc(KY_PHYSICS_MAX_BODIES, sizeof(kyExtents));
    pw->broad_ids  = (uint32_t  *)calloc(KY_PHYSICS_MAX_BODIES, sizeof(uint32_t));
    return pw;
}

void ky_physics_destroy(kyPhysicsWorld *pw) {
    if (!pw) return;
    free(pw->broad_exts);
    free(pw->broad_ids);
    ky_mem_free(&pw->alloc, pw);
}

uint32_t ky_physics_add_collider(kyPhysicsWorld *pw, const kyCollider *c) {
    if (!pw || !c || pw->collider_count >= KY_PHYSICS_MAX_COLLIDERS) return 0;
    uint32_t id = pw->next_collider_id++;
    int storage = pw->collider_count++;
    kyPhysCollider *col = &pw->colliders[storage];
    col->alive = 1;
    col->collider = *c;
    /* Store 1-based storage index so that 0 means "no collider". */
    pw->collider_index[id] = storage + 1;
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

static kyPhysForceField *find_force_field(const kyPhysicsWorld *pw, uint32_t id) {
    if (!pw) return NULL;
    for (int i = 0; i < pw->force_field_count; i++) {
        if (pw->force_fields[i].alive && pw->force_fields[i].id == id)
            return (kyPhysForceField *)&pw->force_fields[i];
    }
    return NULL;
}

int ky_physics_remove_force_field(kyPhysicsWorld *pw, uint32_t id) {
    kyPhysForceField *ff = find_force_field(pw, id);
    if (!ff) return 0;
    int i = (int)(ff - pw->force_fields);
    memmove(&pw->force_fields[i], &pw->force_fields[i + 1],
            (size_t)(pw->force_field_count - i - 1) * sizeof(kyPhysForceField));
    pw->force_field_count--;
    return 1;
}

int ky_physics_get_force_field_count(const kyPhysicsWorld *pw) {
    return pw ? pw->force_field_count : 0;
}

kyForceField ky_physics_get_force_field(const kyPhysicsWorld *pw, uint32_t id) {
    kyPhysForceField *ff = find_force_field(pw, id);
    if (ff) return ff->field;
    kyForceField empty = {{0, 0, 0}, 0, 0, KY_FORCE_FIELD_GRAVITY, NULL};
    return empty;
}

void ky_physics_set_force_field(kyPhysicsWorld *pw, uint32_t id, kyForceField field) {
    kyPhysForceField *ff = find_force_field(pw, id);
    if (ff) ff->field = field;
}

/* ===== Constraint / joint system (G9) ===== */

static int body_index_valid(const kyPhysicsWorld *pw, uint32_t body_id) {
    /* 0 = static world; otherwise must be a registered body. */
    if (body_id == 0) return 1;
    return body_id >= 1 && body_id <= (uint32_t)pw->body_count &&
           pw->bodies[body_id - 1].alive;
}

uint32_t ky_physics_add_constraint(kyPhysicsWorld *pw, const kyConstraintDesc *desc) {
    if (!pw || !desc) return 0;
    if (desc->type != KY_CONSTRAINT_DISTANCE &&
        desc->type != KY_CONSTRAINT_HINGE) return 0;
    if (!body_index_valid(pw, desc->body_a)) return 0;
    if (!body_index_valid(pw, desc->body_b)) return 0;
    if (pw->constraint_count >= KY_PHYSICS_MAX_CONSTRAINTS) return 0;

    uint32_t id = pw->next_constraint_id++;
    kyPhysConstraint *c = &pw->constraints[pw->constraint_count++];
    c->alive = 1;
    c->id = id;
    c->desc = *desc;
    return id;
}

int ky_physics_remove_constraint(kyPhysicsWorld *pw, uint32_t id) {
    if (!pw || id == 0) return 0;
    for (int i = 0; i < pw->constraint_count; i++) {
        if (pw->constraints[i].alive && pw->constraints[i].id == id) {
            pw->constraints[i].alive = 0;
            return 1;
        }
    }
    return 0;
}

int ky_physics_get_constraint_count(const kyPhysicsWorld *pw) {
    return pw ? pw->constraint_count : 0;
}

/* Anchor world position: static-world end uses the given coordinate
 * directly; the dynamic end rotates its local anchor by the body quat
 * and translates to the body origin. */
static kyVec3 constraint_anchor_world(const kyRigidBody *b, const kyVec3 *local) {
    if (!b) return *local;
    kyVec3 rotated = ky_quat_rotate(b->rotation, *local);
    return ky_vec3_add(b->position, rotated);
}

/* World-space velocity of an anchor point on a body: linear velocity
 * plus the tangential contribution from angular velocity. */
static kyVec3 constraint_anchor_velocity(const kyRigidBody *b, kyVec3 anchor_world) {
    if (!b) return ky_vec3_zero();
    kyVec3 r = ky_vec3_sub(anchor_world, b->position);
    return ky_vec3_add(b->linear_velocity, ky_vec3_cross(b->angular_velocity, r));
}

/* 1 scalar rotation (radians) encoded in a quat's angle around +Z,
 * for 2D hinge angle bookkeeping. */
static float quat_z_angle(const kyQuat *q) {
    /* q = (0, 0, sin(a/2), cos(a/2)) for a pure Z rotation; extract a. */
    float s = 2.0f * q->z * q->w;
    float c = 1.0f - 2.0f * q->z * q->z;
    return atan2f(s, c);
}

static void quat_z_rotate(kyQuat *q, float delta) {
    kyQuat d = ky_quat_axis_angle(ky_vec3(0.0f, 0.0f, 1.0f), delta);
    *q = ky_quat_normalize(ky_quat_mul(d, *q));
}

static void solve_distance(kyPhysicsWorld *pw, kyPhysConstraint *c) {
    kyPhysBody *ba = c->desc.body_a ? &pw->bodies[c->desc.body_a - 1] : NULL;
    kyPhysBody *bb = c->desc.body_b ? &pw->bodies[c->desc.body_b - 1] : NULL;
    kyRigidBody *ra = ba ? &ba->body : NULL;
    kyRigidBody *rb = bb ? &bb->body : NULL;
    float inv_a = ra ? ra->inv_mass : 0.0f;
    float inv_b = rb ? rb->inv_mass : 0.0f;
    float inv_sum = inv_a + inv_b;
    if (inv_sum <= 0.0f) return; /* both static: nothing to do */

    kyVec3 wa = constraint_anchor_world(ra, &c->desc.local_a);
    kyVec3 wb = constraint_anchor_world(rb, &c->desc.local_b);
    kyVec3 d = ky_vec3_sub(wb, wa);
    float len = ky_vec3_len(d);
    if (len < 1e-6f) return; /* zero-length: skip to avoid direction divide */

    kyVec3 n = ky_vec3_scale(d, 1.0f / len);
    float err = len - c->desc.distance;

    /* Position correction: project both anchors onto target distance. */
    if (ra) ra->position = ky_vec3_add(ra->position,
        ky_vec3_scale(n, err * (inv_a / inv_sum)));
    if (rb) rb->position = ky_vec3_sub(rb->position,
        ky_vec3_scale(n, err * (inv_b / inv_sum)));

    /* Velocity correction: kill relative velocity along the constraint axis. */
    wa = constraint_anchor_world(ra, &c->desc.local_a);
    wb = constraint_anchor_world(rb, &c->desc.local_b);
    kyVec3 va = constraint_anchor_velocity(ra, wa);
    kyVec3 vb = constraint_anchor_velocity(rb, wb);
    float rel = ky_vec3_dot(ky_vec3_sub(vb, va), n);
    if (ra) ra->linear_velocity = ky_vec3_add(ra->linear_velocity,
        ky_vec3_scale(n, rel * inv_a));
    if (rb) rb->linear_velocity = ky_vec3_sub(rb->linear_velocity,
        ky_vec3_scale(n, rel * inv_b));
}

static void solve_hinge(kyPhysicsWorld *pw, kyPhysConstraint *c) {
    kyPhysBody *ba = c->desc.body_a ? &pw->bodies[c->desc.body_a - 1] : NULL;
    kyPhysBody *bb = c->desc.body_b ? &pw->bodies[c->desc.body_b - 1] : NULL;
    kyRigidBody *ra = ba ? &ba->body : NULL;
    kyRigidBody *rb = bb ? &bb->body : NULL;
    float inv_a = ra ? ra->inv_mass : 0.0f;
    float inv_b = rb ? rb->inv_mass : 0.0f;
    float inv_sum = inv_a + inv_b;
    if (inv_sum <= 0.0f) return;

    kyVec3 wa = constraint_anchor_world(ra, &c->desc.local_a);
    kyVec3 wb = constraint_anchor_world(rb, &c->desc.local_b);
    /* Position: project the two anchors to coincide. */
    kyVec3 gap = ky_vec3_sub(wa, wb);
    if (ra) ra->position = ky_vec3_sub(ra->position,
        ky_vec3_scale(gap, inv_a / inv_sum));
    if (rb) rb->position = ky_vec3_add(rb->position,
        ky_vec3_scale(gap, inv_b / inv_sum));

    /* Angle: keep (angle_a - angle_b) at desc->angle_offset. */
    float diff = quat_z_angle(&ra->rotation) - quat_z_angle(&rb->rotation) - c->desc.angle_offset;
    /* Wrap into [-pi, pi] so the correction takes the short path. */
    if (diff > (float)KY_PI)   diff -= 2.0f * (float)KY_PI;
    if (diff < -(float)KY_PI)  diff += 2.0f * (float)KY_PI;
    if (ra) quat_z_rotate(&ra->rotation, -diff * (inv_a / inv_sum));
    if (rb) quat_z_rotate(&rb->rotation,  diff * (inv_b / inv_sum));

    /* Velocity: kill relative velocity at the coincident anchor. */
    wa = constraint_anchor_world(ra, &c->desc.local_a);
    kyVec3 va = constraint_anchor_velocity(ra, wa);
    kyVec3 vb = constraint_anchor_velocity(rb, wa);
    kyVec3 rel_v = ky_vec3_sub(vb, va);
    if (ra) ra->linear_velocity = ky_vec3_add(ra->linear_velocity,
        ky_vec3_scale(rel_v, inv_a / inv_sum));
    if (rb) rb->linear_velocity = ky_vec3_sub(rb->linear_velocity,
        ky_vec3_scale(rel_v, inv_b / inv_sum));
}

static void phys_apply_constraints(kyPhysicsWorld *pw, float dt) {
    KY_UNUSED(dt);
    if (!pw || pw->constraint_count == 0) return;
    for (int i = 0; i < pw->constraint_count; i++) {
        kyPhysConstraint *c = &pw->constraints[i];
        if (!c->alive || !c->desc.enabled) continue;
        if (c->desc.type == KY_CONSTRAINT_DISTANCE) solve_distance(pw, c);
        else if (c->desc.type == KY_CONSTRAINT_HINGE) solve_hinge(pw, c);
    }
    /* Re-derive AABBs for any body touched by an active constraint. */
    for (int i = 0; i < pw->body_count; i++) {
        kyPhysBody *b = &pw->bodies[i];
        if (!b->alive) continue;
        phys_body_update_aabb(b, pw);
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
        
        r->linear_velocity = ky_vec3_add(r->linear_velocity,
            ky_vec3_scale(total_force, r->inv_mass * dt));
    }
}

static void sap_build_events(kyPhysicsWorld *pw) {
    pw->sap_event_count = 0;
    for (int i = 0; i < pw->body_count; i++) {
        kyPhysBody *b = &pw->bodies[i];
        if (!b->alive) continue;
        if (!b->has_aabb) continue;
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
    memset(pw->sap_active, 0, sizeof(pw->sap_active));
    for (int i = 0; i < pw->sap_event_count; i++) {
        kySAPEvent *e = &pw->sap_events[i];
        int idx = e->body_idx;
        if (e->min_event) {
            for (int j = 0; j < idx; j++) {
                if (pw->sap_active[j] && pw->pair_count < KY_PHYSICS_MAX_PAIRS) {
                    kyPhysBody *a = &pw->bodies[j];
                    kyPhysBody *bb = &pw->bodies[idx];
                    if (a->alive && bb->alive && a->has_aabb && bb->has_aabb) {
                        kyContactPair *p = &pw->pairs[pw->pair_count++];
                        p->body_a = (uint32_t)(j + 1);
                        p->body_b = (uint32_t)(idx + 1);
                        p->alive = 1;
                    }
                }
            }
            pw->sap_active[idx] = 1;
        } else {
            pw->sap_active[idx] = 0;
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

        r->linear_velocity = ky_vec3_add(r->linear_velocity, ky_vec3_scale(pw->gravity, dt));
        r->position = ky_vec3_add(r->position, ky_vec3_scale(r->linear_velocity, dt));

        phys_body_update_aabb(b, pw);
    }

    /* SAP broadphase to find potential collision pairs (or use custom hook) */
    if (pw->broad_fn) {
        /* Pre-allocated at ky_physics_create; no per-frame calloc/free. */
        int n = 0;
        for (int i = 0; i < pw->body_count; i++) {
            if (!pw->bodies[i].alive || !pw->bodies[i].has_aabb) continue;
            pw->broad_exts[n].min = pw->bodies[i].aabb_min;
            pw->broad_exts[n].max = pw->bodies[i].aabb_max;
            pw->broad_ids[n] = (uint32_t)(i + 1);
            n++;
        }
        pw->pair_count = 0;
        pw->broad_fn(pw->broad_exts, pw->broad_ids, n,
                     &pw->pairs[0].body_a, &pw->pairs[0].body_b,
                     &pw->pair_count, KY_PHYSICS_MAX_PAIRS);
        /* Custom broadphase only fills body_a/body_b; set alive so the
         * built-in narrowphase (if no narrow_fn) will process the pairs. */
        for (int i = 0; i < pw->pair_count; i++)
            pw->pairs[i].alive = 1;
    } else {
        sap_build_events(pw);
        sap_find_pairs(pw);
    }

    /* Narrow-phase: detect and resolve overlaps between collided bodies */
    if (pw->narrow_fn) {
        for (int i = 0; i < pw->pair_count; i++) {
            int alive = 0;
            pw->narrow_fn(pw->pairs[i].body_a, pw->pairs[i].body_b, &alive);
            pw->pairs[i].alive = alive;
        }
    } else {
    for (int i = 0; i < pw->pair_count; i++) {
        kyContactPair *pair = &pw->pairs[i];
        if (!pair->alive) continue;
        int ia = (int)pair->body_a - 1;
        int ib = (int)pair->body_b - 1;
        if (ia < 0 || ia >= pw->body_count || ib < 0 || ib >= pw->body_count) continue;
        kyPhysBody *ba = &pw->bodies[ia];
        kyPhysBody *bb = &pw->bodies[ib];
        if (!ba->alive || !bb->alive) continue;
        if (!ba->has_aabb || !bb->has_aabb) continue;

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

    /* Emit collision events for each active contact pair.
     * The event payload is a pointer to the internal contact; callers must
     * copy any needed data immediately because pairs are invalidated on the
     * next physics_step call. */
    for (int i = 0; i < pw->pair_count; i++) {
        const kyContactPair *p = &pw->pairs[i];
        if (!p->alive) continue;
        kyCollision ev = { p->body_a, p->body_b };
        ky_event_trigger(KY_EVENT_COLLIDE, &ev);
    }

    /* Constraint solving runs after collision resolution so that joints
     * hold even when bodies are in contact. Does not emit collision events. */
    phys_apply_constraints(pw, dt);
}

static kyPhysBody *body_by_id(const kyPhysicsWorld *pw, uint32_t body_id) {
    int idx = (int)body_id - 1;
    if (!pw || idx < 0 || idx >= pw->body_count) return NULL;
    kyPhysBody *b = (kyPhysBody *)&pw->bodies[idx];
    return b->alive ? b : NULL;
}

void ky_physics_apply_impulse(kyPhysicsWorld *pw, uint32_t body_id, kyVec3 impulse, kyVec3 at) {
    kyPhysBody *b = body_by_id(pw, body_id);
    if (!b || b->body.inv_mass <= 0.0f) return;
    b->body.linear_velocity = ky_vec3_add(b->body.linear_velocity,
        ky_vec3_scale(impulse, b->body.inv_mass));
    /* Angular impulse: torque = cross(at - position, impulse) * inv_inertia */
    if (b->body.inv_inertia[0] > 0.0f || b->body.inv_inertia[1] > 0.0f ||
        b->body.inv_inertia[2] > 0.0f) {
        kyVec3 lever = ky_vec3_sub(at, b->body.position);
        kyVec3 torque = {
            lever.y * impulse.z - lever.z * impulse.y,
            lever.z * impulse.x - lever.x * impulse.z,
            lever.x * impulse.y - lever.y * impulse.x,
        };
        b->body.angular_velocity.x += torque.x * b->body.inv_inertia[0];
        b->body.angular_velocity.y += torque.y * b->body.inv_inertia[1];
        b->body.angular_velocity.z += torque.z * b->body.inv_inertia[2];
    }
}

void ky_physics_get_body(const kyPhysicsWorld *pw, uint32_t body_id, kyRigidBody *out) {
    kyPhysBody *b = body_by_id(pw, body_id);
    if (b && out) *out = b->body;
}

void ky_physics_cast_ray(const kyPhysicsWorld *pw, kyVec3 origin, kyVec3 dir,
                          float max_t, kyRayHit *out_hit) {
    if (!out_hit) return;
    out_hit->hit = 0;
    out_hit->t = max_t;
    out_hit->body_id = 0;
    if (!pw) return;

    /* Guard against zero-direction ray: inv_dir would be infinite */
    if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) return;

    float best_t = max_t;
    uint32_t best_id = 0;
    kyVec3 best_normal = ky_vec3_zero();

    for (int i = 0; i < pw->body_count; i++) {
        const kyPhysBody *b = &pw->bodies[i];
        if (!b->alive || !b->has_aabb) continue;

        uint32_t cid = b->body.collider_id;
        const kyCollider *col = NULL;
        int centry = (cid <= (uint32_t)KY_PHYSICS_MAX_COLLIDERS) ? pw->collider_index[cid] : 0;
        if (centry && pw->colliders[centry - 1].alive) col = &pw->colliders[centry - 1].collider;
        const kySphere *sph = (col && col->shape == KY_SHAPE_SPHERE) ? &col->u.sphere : NULL;
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
        if (col && col->shape == KY_SHAPE_BOX && b->aabb_min.x < b->aabb_max.x) {
            kyAABB aabb = { b->aabb_min, b->aabb_max };
            float t_aabb;
            if (ky_ray_aabb(origin, dir, best_t, &aabb, &t_aabb) && t_aabb < best_t) {
                best_t = t_aabb;
                best_id = (uint32_t)(i + 1);
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
    kyPhysBody *b = body_by_id(pw, body_id);
    if (!b || !min_out || !max_out) return;
    *min_out = b->aabb_min;
    *max_out = b->aabb_max;
}

int ky_physics_get_contact_count(const kyPhysicsWorld *pw) {
    return pw ? pw->pair_count : 0;
}

int ky_physics_get_contact(const kyPhysicsWorld *pw, uint32_t idx,
                           uint32_t *out_a, uint32_t *out_b) {
    if (!pw || idx >= (uint32_t)pw->pair_count) return 0;
    const kyContactPair *p = &pw->pairs[idx];
    if (out_a) *out_a = p->body_a;
    if (out_b) *out_b = p->body_b;
    return 1;
}

void ky_physics_set_broadphase(kyPhysicsWorld *pw, kyPhysicsBroadFn fn) {
    if (!pw) return;
    pw->broad_fn = fn;
}

void ky_physics_set_narrowphase(kyPhysicsWorld *pw, kyPhysicsNarrowFn fn) {
    if (!pw) return;
    pw->narrow_fn = fn;
}
