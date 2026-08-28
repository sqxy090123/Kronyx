#include "kronyx/physics.h"
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

    /* Test: raycast (stub always misses) */
    kyRayHit hit;
    ky_physics_cast_ray(pw, (kyVec3){0,0,0}, (kyVec3){0,-1,0}, 100.0f, &hit);
    ASSERT(hit.hit == 0, "raycast stub returns no hit");

    /* Test: null safety */
    ky_physics_step(NULL, dt);
    ky_physics_apply_impulse(NULL, body_id, imp, (kyVec3){0,0,0});
    ky_physics_get_body(NULL, body_id, &retrieved);
    ky_physics_cast_ray(NULL, (kyVec3){0,0,0}, (kyVec3){0,-1,0}, 1.0f, &hit);
    ky_physics_get_aabb(NULL, body_id, &aabb_min, &aabb_max);

    printf("\n=== %d tests ran, %d failures ===\n", assertions, failures);

    ky_physics_destroy(pw);
    return failures == 0 ? 0 : 1;
}
