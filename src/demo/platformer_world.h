#ifndef KRONYX_DEMO_PLATFORMER_H
#define KRONYX_DEMO_PLATFORMER_H

#include "kronyx/ecs.h"
#include "kronyx/physics.h"
#include "kronyx/render.h"
#include "kronyx/2d.h"
#include "kronyx/memory.h"
#include "kronyx/file.h"

/* Tunable constants for the platformer vertical slice (engine units, EU). */
#define PLATFORMER_ROLE_RADIUS   0.50f
#define PLATFORMER_GROUND_TOP    0.00f
#define PLATFORMER_MAX_SPEED     4.00f
#define PLATFORMER_JUMP_VY       6.00f
#define PLATFORMER_GRAVITY_Y    -10.00f
#define PLATFORMER_CAM_OFFSET_Y  2.00f
#define PLATFORMER_START_Y       3.00f

typedef struct PlatformerWorld {
    kyAllocator    alloc;
    kyWorld        *world;
    kyPhysicsWorld *phys;
    kyRenderDevice *rd;

    kyEntity role;
    kyEntity camera;

    uint32_t role_body;    /* reserved (unused by the manual integrator) */
    uint32_t ground_body;  /* static ground body id */

    uint32_t tid_transform;
    uint32_t tid_sprite;
    uint32_t tid_camera;

    kyTexture *role_tex;   /* owned; NULL = white sprite (asset missing / build fail) */

    int  grounded;         /* role resting on the ground */
    int  jump_consumed;    /* jump request already applied this frame */
    float vx, vy;          /* current role velocity (EU/s) for readback */
} PlatformerWorld;

/* Build the world + physics + render device + role/ground/camera.
 * Returns 0 on success, negative on failure. Safe to call on a fresh struct. */
int platformer_setup(PlatformerWorld *out, kyRendererBackend rd_backend);

/* Read back the role's current world-space position (EU, x/y). */
kyVec2 platformer_role_pos(const PlatformerWorld *pw);

/* Read back the role's current velocity (EU/s). */
void platformer_role_velocity(const PlatformerWorld *pw, float *vx, float *vy);

/* Advance one frame. `vx_target` is desired horizontal velocity (EU/s);
 * `jump` is a single-shot request (edge-triggered by the caller).
 * Runs input-apply -> integrate -> ground contact -> sync -> camera -> render.
 * Returns 0 on success, negative on failure. */
int platformer_tick(PlatformerWorld *pw, float dt, float vx_target, int jump);

/* Release everything. Safe to call twice. */
void platformer_teardown(PlatformerWorld *pw);

#endif
