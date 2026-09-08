#ifndef KRONYX_ANIM2D_H
#define KRONYX_ANIM2D_H

#include "2d.h"
#include "ecs.h"

/*
 * Sprite frame animation component and helper.
 *
 * A kyAnimator describes how a single sprite sheet (col × row grid) is
 * subdivided into `frames` equally-sized cells. The renderer reads the
 * current cell as uv0/uv1 on the attached kySprite each frame.
 *
 * Coordinate conventions:
 *   - Atlas is an upright grid of cols columns and ceil(frames/cols) rows.
 *   - Frame 0 is top-left; row-major order (0..cols-1, cols..2*cols-1, …).
 *   - UV coordinates are normalized to [0,1] across the sprite's texture.
 *   - fps > 0 advances one cell per 1/fps seconds.
 *
 * Usage pattern:
 *   ky2d_register_components(w);              // registers sprite, transform, camera2d
 *   ky_anim2d_register(w);                    // registers "animator" + built-in system
 *   kyEntity e = ky_world_spawn(w);
 *   kySprite *sp = (kySprite *)ky_world_add_component(w, e, tid_sprite);
 *   kyAnimator *a = (kyAnimator *)ky_world_add_component(w, e, tid_animator);
 *   a->cols = 4; a->frames = 8; a->fps = 8; a->looping = 1;
 *   ky_world_step(w, dt);                     // system advances a->current_frame, writes sp->uv
 */

typedef struct kyAnimator {
    int      cols;       /* atlas columns >= 1 */
    int      frames;     /* total frames in the strip, 1..cols*rows */
    float    fps;        /* target tick rate, > 0 */
    int      looping;    /* 1 = wrap at last frame; 0 = clamp and stop */
    float    time;       /* seconds accumulated (monotonic) */
    int      current_frame; /* last applied frame index, 0-based */
} kyAnimator;

KY_API kyAnimator ky_animator_new(void);

/* Return 0 on success. Sets out_uv0/out_uv1 to the normalized cell
 * boundaries for the animator's current_frame. The caller is expected
 * to write those into kySprite.uv0/uv1 each frame (or call
 * ky_animator_apply_to_sprite directly). */
KY_API int ky_animator_frame_uv(const kyAnimator *a,
                                kyVec2 *out_uv0, kyVec2 *out_uv1);

/* Convenience: apply the current cell's UV to `sp`. Returns 0 on success,
 * negative if `a` or `sp` is NULL, frames <= 0, cols <= 0, fps <= 0. */
KY_API int ky_animator_apply_to_sprite(const kyAnimator *a, kySprite *sp);

/* Register the "animator" component type plus the built-in update
 * system (order 0). Must be called once per world, after
 * ky2d_register_components has registered "sprite". Returns 0 on
 * success, negative on error. */
KY_API int ky_anim2d_register(kyWorld *w);

#endif
