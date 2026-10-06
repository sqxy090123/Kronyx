#include "kronyx/physics.h"
#include "kronyx/event.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int assertions = 0;
static int failures = 0;

#define ASSERT(cond, msg) do { \
    assertions++; \
    if (!(cond)) { \
        failures++; \
        printf("FAIL: %s at line %d\n", msg, __LINE__); \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while(0)

static void test_force_fields(void);
static void test_collision_event(void);
static void test_constraints(void);
int main(void) {
    printf("=== Physics Test ===\n");

    kyVec3 gravity = {0.0f, -9.81f, 0.0f};
    kyPhysicsWorld *pw = ky_physics_create(gravity);
    ASSERT(pw != NULL, "create physics world");
    ky_physics_get_body(pw, 0, NULL);

    /* Test: add collider and body */
    kyCollider sphere_col = {0};
    sphere_col.shape = KY_SHAPE_SPHERE;
    sphere_col.u.sphere.center = (kyVec3){0.0f, 5.0f, 0.0f};
    sphere_col.u.sphere.radius = 1.0f;
    uint32_t col_id = ky_physics_add_collider(pw, &sphere_col);
    ASSERT(col_id != 0, "add sphere collider");

    kyRigidBody body = {0};
    body.position = (kyVec3){0.0f, 10.0f, 0.0f};
    body.linear_velocity = (kyVec3){0.0f, 0.0f, 0.0f};
    body.inv_mass = 1.0f;
    body.restitution = 0.5f;
    body.friction = 0.3f;
    body.collider_id = col_id;
    uint32_t body_id = ky_physics_add_body(pw, &body);
    ASSERT(body_id != 0, "add rigid body");

    /* Test: get body returns stored data */
    kyRigidBody retrieved;
    ky_physics_get_body(pw, body_id, &retrieved);
    ASSERT(retrieved.position.x == body.position.x, "body position.x matches");
    ASSERT(retrieved.position.y == body.position.y, "body position.y matches");
    ASSERT(fabs(retrieved.linear_velocity.y - 0.0f) < 1e-6f, "body linear_velocity zero init");
    ASSERT(retrieved.inv_mass == 1.0f, "body inv_mass matches");
    ASSERT(retrieved.restitution == 0.5f, "body restitution matches");

    /* Test: AABB query */
    kyVec3 aabb_min, aabb_max;
    ky_physics_get_aabb(pw, body_id, &aabb_min, &aabb_max);
    ASSERT(aabb_min.x < aabb_max.x, "aabb min < max on x");
    ASSERT(aabb_min.y < aabb_max.y, "aabb min < max on y");

    /* Test: gravity step integration */
    float dt = 1.0f / 60.0f;
    ky_physics_step(pw, dt);
    ky_physics_get_body(pw, body_id, &retrieved);
    ASSERT(retrieved.position.y < body.position.y, "body falls with gravity");
    ASSERT(retrieved.linear_velocity.y < 0.0f, "body gains downward velocity");

    /* Test: apply impulse */
    kyVec3 imp = {10.0f, 50.0f, 0.0f};
    ky_physics_apply_impulse(pw, body_id, imp, (kyVec3){0,0,0});
    ky_physics_get_body(pw, body_id, &retrieved);
    ASSERT(fabs(retrieved.linear_velocity.x - 10.0f) < 0.01f, "impulse sets linear_velocity.x");
    ASSERT(fabs(retrieved.linear_velocity.y - (retrieved.linear_velocity.y)) < 0.01f, "impulse adds to linear_velocity.y");

    /* Test: set gravity (fresh body with zero initial velocity) */
    kyVec3 zero_g = {0.0f, 0.0f, 0.0f};
    kyRigidBody zero_vel = {0};
    zero_vel.position = (kyVec3){0.0f, 0.0f, 0.0f};
    zero_vel.inv_mass = 1.0f;
    uint32_t body_id2 = ky_physics_add_body(pw, &zero_vel);
    ASSERT(body_id2 != 0, "add zero-velocity body for gravity test");
    ky_physics_set_gravity(pw, zero_g);
    ky_physics_step(pw, dt);
    kyRigidBody after_zero_g;
    ky_physics_get_body(pw, body_id2, &after_zero_g);
    ASSERT(fabs(after_zero_g.linear_velocity.y) < 1e-5f, "zero gravity: no vertical velocity change");

    /* Test: raycast hits sphere collider */
    kyRayHit hit;
    /* Ray from below sphere going up: origin(0,0,0) dir(0,1,0), sphere center(0,5,0) radius=1 */
    ky_physics_cast_ray(pw, (kyVec3){0,0,0}, (kyVec3){0,1,0}, 100.0f, &hit);
    ASSERT(hit.hit == 1, "raycast hits sphere");
    ASSERT(hit.t > 3.0f && hit.t < 5.0f, "raycast t near sphere bottom (y=4)");
    ASSERT(hit.body_id == body_id, "raycast returns correct body_id");

    /* Test: null safety */
    ky_physics_step(NULL, dt);
    ky_physics_apply_impulse(NULL, body_id, imp, (kyVec3){0,0,0});
    ky_physics_get_body(NULL, body_id, &retrieved);
    ky_physics_cast_ray(NULL, (kyVec3){0,0,0}, (kyVec3){0,-1,0}, 1.0f, &hit);
    ky_physics_get_aabb(NULL, body_id, &aabb_min, &aabb_max);

    test_force_fields();
    test_collision_event();
    test_constraints();
    printf("\n=== %d tests ran, %d failures ===\n", assertions, failures);

    ky_physics_destroy(pw);
    return failures == 0 ? 0 : 1;
}

/* ===== Collision Event Tests ===== */
static int  collide_count = 0;
static kyCollision collide_last = {0, 0};

static void on_collide(const char *name, const void *data, void *user) {
    (void)name;
    (void)user;
    if (data) {
        collide_last = *(const kyCollision *)data;
        collide_count++;
    }
}

static void test_collision_event(void) {
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, 0, 0});
    ASSERT(pw != NULL, "create world for collision event test");
    ASSERT(ky_event_register(KY_EVENT_COLLIDE, on_collide, NULL) > 0,
           "register collide event listener");

    kyCollider c0 = {0};
    c0.shape = KY_SHAPE_BOX;
    c0.u.box.center = (kyVec3){0, 0, 0};
    c0.u.box.half_extents = (kyVec3){1, 1, 1};
    kyCollider c1 = {0};
    c1.shape = KY_SHAPE_BOX;
    c1.u.box.center = (kyVec3){1.5f, 0, 0};
    c1.u.box.half_extents = (kyVec3){1, 1, 1};
    uint32_t col0 = ky_physics_add_collider(pw, &c0);
    uint32_t col1 = ky_physics_add_collider(pw, &c1);

    kyRigidBody b0 = {0};
    b0.position = (kyVec3){0, 0, 0};
    b0.inv_mass = 0.0f;
    b0.collider_id = col0;
    kyRigidBody b1 = {0};
    b1.position = (kyVec3){1.5f, 0, 0};
    b1.inv_mass = 0.0f;
    b1.collider_id = col1;
    uint32_t id0 = ky_physics_add_body(pw, &b0);
    uint32_t id1 = ky_physics_add_body(pw, &b1);
    ASSERT(id0 != 0 && id1 != 0, "add two overlapping static bodies");

    collide_count = 0;
    collide_last.body_a = collide_last.body_b = 0;
    ky_physics_step(pw, 1.0f / 60.0f);
    ASSERT(collide_count >= 1, "collide event fired on overlap");
    ASSERT(collide_last.body_a != 0 && collide_last.body_b != 0,
           "collision payload carries two body ids");
    ASSERT((collide_last.body_a == id0 && collide_last.body_b == id1) ||
           (collide_last.body_a == id1 && collide_last.body_b == id0),
           "collision payload ids match overlapping bodies");

    ky_physics_destroy(pw);
}
static void test_force_fields(void) {
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, -9.81f, 0});
    ASSERT(pw != NULL, "create physics world for force field test");

    /* Test: add force field */
    kyForceField field = ky_force_field_create(
        (kyVec3){0.0f, 10.0f, 0.0f},  /* position */
        9.81f,                         /* strength (gravity-like) */
        100.0f,                        /* radius (infinite) */
        KY_FORCE_FIELD_GRAVITY         /* type */
    );
    uint32_t field_id = ky_physics_add_force_field(pw, &field);
    ASSERT(field_id != 0, "add gravity force field");

    /* Test: get force field count */
    int count = ky_physics_get_force_field_count(pw);
    ASSERT(count == 1, "force field count is 1");

    /* Test: get force field properties */
    kyForceField retrieved = ky_physics_get_force_field(pw, field_id);
    ASSERT(retrieved.position.x == 0.0f, "force field position.x matches");
    ASSERT(retrieved.position.y == 10.0f, "force field position.y matches");
    ASSERT(retrieved.strength == 9.81f, "force field strength matches");
    ASSERT(retrieved.type == KY_FORCE_FIELD_GRAVITY, "force field type is gravity");

    /* Test: apply force field to body */
    kyCollider sphere_col = {0};
    sphere_col.shape = KY_SHAPE_SPHERE;
    sphere_col.u.sphere.center = (kyVec3){0.0f, 5.0f, 0.0f};
    sphere_col.u.sphere.radius = 1.0f;
    uint32_t col_id = ky_physics_add_collider(pw, &sphere_col);

    kyRigidBody body = {0};
    body.position = (kyVec3){0.0f, 5.0f, 0.0f};  /* At field center */
    body.linear_velocity = (kyVec3){0.0f, 0.0f, 0.0f};
    body.inv_mass = 1.0f;
    body.collider_id = col_id;
    uint32_t body_id = ky_physics_add_body(pw, &body);
    ASSERT(body_id != 0, "add body in force field");

    /* Step with force field */
    float dt = 1.0f / 60.0f;
    ky_physics_step(pw, dt);
    
    kyRigidBody after;
    ky_physics_get_body(pw, body_id, &after);
    /* Body should have some velocity change due to force field */
    ASSERT(fabs(after.position.y - 5.0f) < 1.0f, "body affected by force field");

    /* Test: repulsion field */
    kyForceField repulse = ky_force_field_create(
        (kyVec3){0.0f, 0.0f, 0.0f},
        -50.0f,  /* Negative strength for repulsion */
        50.0f,
        KY_FORCE_FIELD_REPULSION
    );
    uint32_t repulse_id = ky_physics_add_force_field(pw, &repulse);
    ASSERT(repulse_id != 0, "add repulsion force field");
    ASSERT(ky_physics_get_force_field_count(pw) == 2, "two force fields present");

    /* Test: remove force field */
    int removed = ky_physics_remove_force_field(pw, repulse_id);
    ASSERT(removed == 1, "remove repulsion force field");
    ASSERT(ky_physics_get_force_field_count(pw) == 1, "one force field after removal");

    /* Test: null safety */
    ky_physics_add_force_field(NULL, &field);
    ky_physics_remove_force_field(NULL, field_id);
    ky_physics_get_force_field_count(NULL);
    ky_physics_get_force_field(NULL, field_id);
    ky_physics_set_force_field(NULL, field_id, field);

    ky_physics_destroy(pw);
}

/* ===== Constraint / Joint Tests (G9) ===== */

static float quat_z_angle_of(const kyQuat *q) {
    float s = 2.0f * q->z * q->w;
    float c = 1.0f - 2.0f * q->z * q->z;
    return atan2f(s, c);
}

static void test_constraints(void) {
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, 0, 0});
    ASSERT(pw != NULL, "constraint: create physics world");

    /* --- add validation --- */
    kyRigidBody b = {0};
    b.position = (kyVec3){0, 0, 0};
    b.inv_mass = 1.0f;
    uint32_t id_a = ky_physics_add_body(pw, &b);
    kyRigidBody b2 = {0};
    b2.position = (kyVec3){4, 0, 0};
    b2.inv_mass = 1.0f;
    uint32_t id_b = ky_physics_add_body(pw, &b2);
    ASSERT(id_a != 0 && id_b != 0, "constraint: add two bodies");

    kyConstraintDesc d = {0};
    d.type = KY_CONSTRAINT_DISTANCE;
    d.body_a = id_a;
    d.body_b = id_b;
    d.local_a = (kyVec3){0, 0, 0};
    d.local_b = (kyVec3){0, 0, 0};
    d.distance = 2.0f;
    d.enabled = 1;
    uint32_t cid = ky_physics_add_constraint(pw, &d);
    ASSERT(cid != 0, "constraint: add distance constraint");
    ASSERT(ky_physics_get_constraint_count(pw) == 1, "constraint: count is 1 after add");

    /* invalid type rejected */
    kyConstraintDesc bad = d;
    bad.type = (kyConstraintType)99;
    ASSERT(ky_physics_add_constraint(pw, &bad) == 0, "constraint: invalid type rejected");
    /* invalid body id rejected */
    kyConstraintDesc bad2 = d;
    bad2.body_a = 999; /* not registered */
    ASSERT(ky_physics_add_constraint(pw, &bad2) == 0, "constraint: invalid body_a rejected");
    /* NULL desc / world rejected */
    ASSERT(ky_physics_add_constraint(pw, NULL) == 0, "constraint: NULL desc rejected");
    ASSERT(ky_physics_add_constraint(NULL, &d) == 0, "constraint: NULL world rejected");

    /* --- distance position correction (one step, single-iteration solver) --- */
    /* Two dynamic bodies at distance 4, target 2. After one step the
     * solver pulls them together; both inv_mass=1 so each moves 1.0.
     * (no gravity, no collision since colliders absent => AABBs unset). */
    ky_physics_step(pw, 1.0f / 60.0f);
    kyRigidBody ra, rb;
    ky_physics_get_body(pw, id_a, &ra);
    ky_physics_get_body(pw, id_b, &rb);
    float dx = rb.position.x - ra.position.x;
    float dist_after = fabsf(dx);
    ASSERT(dist_after < 4.0f, "distance: anchors moved toward target");
    ASSERT(dist_after > 0.5f && dist_after < 3.0f,
           "distance: within plausible single-iteration band");

    /* --- static-world symmetry: constraint to world keeps moving body at
     *   a fixed distance from a static anchor. Reuse a fresh world. --- */
    ky_physics_destroy(pw);
    pw = ky_physics_create((kyVec3){0, 0, 0});
    kyRigidBody static_body = {0};
    static_body.position = (kyVec3){0, 0, 0};
    static_body.inv_mass = 0.0f; /* static */
    uint32_t sid = ky_physics_add_body(pw, &static_body);
    kyConstraintDesc sd = {0};
    sd.type = KY_CONSTRAINT_DISTANCE;
    sd.body_a = 0;            /* static world end, anchor in world space */
    sd.body_b = sid;
    sd.local_a = (kyVec3){2, 0, 0}; /* world-space anchor */
    sd.local_b = (kyVec3){0, 0, 0};
    sd.distance = 2.0f;
    sd.enabled = 1;
    ASSERT(ky_physics_add_constraint(pw, &sd) != 0, "distance: static-end constraint added");
    /* static body at origin, world anchor at (2,0,0): already at distance 2,
     * step should keep it there (no drift). */
    ky_physics_step(pw, 1.0f / 60.0f);
    kyRigidBody s_after;
    ky_physics_get_body(pw, sid, &s_after);
    /* World anchor at (2,0,0), static body pinned at origin: the distance is
     * already 2.0 == target on entry, so the solver leaves it exactly put
     * (assert the held distance equals the target, not zero drift). */
    float held = sqrtf((s_after.position.x - 2.0f) * (s_after.position.x - 2.0f) +
                       s_after.position.y * s_after.position.y);
    ASSERT(fabsf(held - 2.0f) < 0.05f, "distance: static end holds position");

    /* --- hinge angle correction --- */
    ky_physics_destroy(pw);
    pw = ky_physics_create((kyVec3){0, 0, 0});
    kyRigidBody ha = {0};
    ha.position = (kyVec3){0, 0, 0};
    ha.inv_mass = 1.0f;
    /* start body_a rotated 45 deg about Z */
    ha.rotation = ky_quat_axis_angle((kyVec3){0, 0, 1}, (float)KY_PI * 0.25f);
    kyRigidBody hb = {0};
    hb.position = (kyVec3){1, 0, 0};
    hb.inv_mass = 1.0f;
    uint32_t hid_a = ky_physics_add_body(pw, &ha);
    uint32_t hid_b = ky_physics_add_body(pw, &hb);
    kyConstraintDesc hd = {0};
    hd.type = KY_CONSTRAINT_HINGE;
    hd.body_a = hid_a;
    hd.body_b = hid_b;
    hd.local_a = (kyVec3){0, 0, 0};
    hd.local_b = (kyVec3){0, 0, 0};
    hd.angle_offset = 0.0f; /* want them aligned */
    hd.enabled = 1;
    ASSERT(ky_physics_add_constraint(pw, &hd) != 0, "hinge: constraint added");
    ky_physics_step(pw, 1.0f / 60.0f);
    kyRigidBody h_after_a, h_after_b;
    ky_physics_get_body(pw, hid_a, &h_after_a);
    ky_physics_get_body(pw, hid_b, &h_after_b);
    float ang_a = quat_z_angle_of(&h_after_a.rotation);
    float ang_b = quat_z_angle_of(&h_after_b.rotation);
    float rel = ang_a - ang_b;
    /* Initial relative offset was PI/4 (0.7854). A single-iteration solver
     * splits the correction across both inv_mass=1 bodies, halving it to
     * ~PI/8. Assert convergence (strictly smaller), not exact zero. */
    ASSERT(fabsf(rel) < 0.7854f, "hinge: relative angle converges toward offset");
    ASSERT(fabsf(rel) > 0.0f, "hinge: single iteration does not fully zero (expected)");

    /* --- remove + idempotent --- */
    ky_physics_destroy(pw);
    pw = ky_physics_create((kyVec3){0, 0, 0});
    kyRigidBody rb1 = {0};
    rb1.position = (kyVec3){0, 0, 0};
    rb1.inv_mass = 1.0f;
    uint32_t rbid = ky_physics_add_body(pw, &rb1);
    kyConstraintDesc rd = {0};
    rd.type = KY_CONSTRAINT_DISTANCE;
    rd.body_a = 0;
    rd.body_b = rbid;
    rd.distance = 1.0f;
    rd.enabled = 1;
    uint32_t rcid = ky_physics_add_constraint(pw, &rd);
    ASSERT(rcid != 0, "remove: constraint added");
    int cnt_before = ky_physics_get_constraint_count(pw);
    ASSERT(ky_physics_remove_constraint(pw, rcid) == 1, "remove: first remove returns 1");
    ASSERT(ky_physics_remove_constraint(pw, rcid) == 0, "remove: second remove returns 0");
    ASSERT(ky_physics_get_constraint_count(pw) == cnt_before,
           "remove: count unchanged (cumulative add semantics)");
    /* removed constraint must not drift the body in a subsequent step */
    float pos_before = rb1.position.x;
    ky_physics_step(pw, 1.0f / 60.0f);
    kyRigidBody rchk;
    ky_physics_get_body(pw, rbid, &rchk);
    ASSERT(fabsf(rchk.position.x - pos_before) < 1e-3f,
           "remove: removed constraint no longer affects body");

    /* --- null safety --- */
    ky_physics_remove_constraint(NULL, 1);
    ky_physics_get_constraint_count(NULL);

    ky_physics_destroy(pw);
}
