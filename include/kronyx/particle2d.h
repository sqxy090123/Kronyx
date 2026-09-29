#ifndef KRONYX_PARTICLE2D_H
#define KRONYX_PARTICLE2D_H

#include "2d.h"

/*
 * 2D particle system: ECS "emitter" component + "particle-update" system +
 * a render pass invoked by ky2d_render_frame after the sprite batches.
 *
 * Conventions:
 *   - Particle pool is a process-global static of KY_PARTICLE_MAX entries
 *     (same pattern as 2d.c's g_items/g_vbo staging).
 *   - Per-emitter deterministic LCG random source; same seed + same step
 *     sequence reproduces identical particle state (see test_particle.c).
 *   - Simulation is pure C, runs headless; rendering reuses the RHI vtable
 *     so both console and GL backends need no changes.
 *
 * Usage pattern:
 *   ky_particle2d_register(w);              // registers "emitter" + system
 *   kyEntity e = ky_world_spawn(w);
 *   kyEmitter *em = (kyEmitter *)ky_world_add_component(w, e, tid_emitter);
 *   *em = ky_emitter_new();
 *   em->enabled = 1; em->emit_rate = 100; em->life0 = em->life1 = 1.0f;
 *   em->color_from = (kyVec4){1,1,0,1}; em->color_to = (kyVec4){1,0,0,0};
 *   ky_world_step(w, dt);                  // system advances simulation
 *   // rendering: particles are drawn automatically by ky2d_render_world
 */

#define KY_PARTICLE_MAX 16384

typedef enum kyEmitShape {
    KY_EMIT_POINT = 0,  /* all particles spawn at the emitter origin */
    KY_EMIT_CIRCLE,     /* spawn on a ring of radius shape_scale */
    KY_EMIT_LINE        /* spawn along an axis-aligned line of half-width shape_scale */
} kyEmitShape;

typedef struct kyEmitter {
    int      enabled;
    float    emit_rate;     /* particles/second, > 0 */
    kyEmitShape shape;
    float    shape_scale;   /* CIRCLE: radius; LINE: half-width; POINT: ignored */
    float    direction;     /* radians, 0 = +X */
    float    speed0, speed1; /* initial speed range, EU/s */
    float    life0, life1;   /* lifetime range, seconds */
    float    size0, size1;   /* particle size range, EU (full extent) */
    kyVec4   color_from, color_to; /* RGBA, 0..1 each channel, linear lerp */
    kyVec2   gravity;        /* EU/s^2 applied to velocity */
    float    drag;           /* 1/s, exponential-ish linear damping factor */
    kyTexture *texture;      /* NULL -> engine built-in white texture */
    uint32_t seed;          /* LCG seed; 0 -> module picks 0x12345678 */
    /* runtime state, owned by the module */
    float    emit_acc;      /* fractional emission accumulator */
    uint32_t rng_state;     /* LCG state */
} kyEmitter;

/* Component ctor defaults: all-zero with sensible fallbacks. */
KY_API kyEmitter ky_emitter_new(void);

/* Register "emitter" component + "particle-update" system into world.
 * Idempotent. Returns 0 on success, -1 if world is NULL. */
KY_API int ky_particle2d_register(kyWorld *w);

/* Number of currently-alive particles in the global pool. */
KY_API int ky_particle2d_alive_count(void);

/* Non-zero once ky_particle2d_register has been called on any world.
 * Used by 2d.c's render_frame to decide whether to run the particle pass. */
KY_API int ky_particle2d_registered(void);

/* Draw all alive particles as quads into the given command list.
 * Invoked internally by ky2d_render_frame; public for host code that
 * wants to drive a particle pass without a full 2D world render.
 * Returns 0 on success, -1 if rd or cl is NULL. No-op (returns 0) when
 * no particles are alive.
 *
 * Texture: V1 renders every particle with a single shared texture,
 * settable via ky_particle2d_set_texture() (default: built-in white). */
KY_API int ky_particle2d_render_pass(kyRenderDevice *rd, void *cl,
                                     const kyCamera2D *cam, const kyTransform *cam_tr);

/* Set the shared texture for the next particle pass. NULL restores the
 * built-in white texture. Take no ref; the texture must outlive the pass. */
KY_API void ky_particle2d_set_texture(kyTexture *tex);

#endif
