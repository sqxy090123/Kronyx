#ifndef KRONYX_2D_H
#define KRONYX_2D_H

#include "defines.h"
#include "math.h"
#include "render.h"
#include "ecs.h"

/*
 * 2D rendering layer: Transform / Sprite / Camera2D components plus a
 * batched sprite renderer built on the RHI vtable.
 *
 * Conventions:
 *  - Engine unit (EU): 1 EU maps to (viewport EU / zoom) screen units.
 *  - Sprites sort by (layer asc, z asc, entity id asc).
 *  - A Sprite without a Transform renders at identity (origin) with a
 *    once-per-frame warning.
 *  - Single-batch capacity is KY2D_MAX_SPRITES_BATCH; more sprites or a
 *    texture change starts a new draw batch.
 */

typedef struct kyTransform {
    kyVec2 pos;        /* world position, EU */
    float rotation_z;  /* radians, counter-clockwise */
    kyVec2 scale;      /* default (1,1), multiplies sprite size */
    float z;           /* sort key within a layer */
} kyTransform;

typedef struct kySprite {
    kyTexture *texture; /* NULL -> built-in 1x1 white texture */
    kyVec4 color;       /* default (1,1,1,1) */
    kyVec2 size;        /* EU, half-extent is size*0.5*scale */
    kyVec2 uv0, uv1;    /* default (0,0)-(1,1) */
    int layer;          /* primary sort key, higher draws on top */
    uint8_t flip;       /* bit0 = flip X, bit1 = flip Y */
} kySprite;

typedef struct kyCamera2D {
    kyVec2 pos;
    float rotation_z;   /* radians */
    float zoom;         /* default 1.0 */
    kyVec2 viewport;    /* visible EU extent */
    kyVec4 clear_color; /* default (0.1,0.1,0.1,1) */
    int active;
} kyCamera2D;

/* Register Transform/Sprite/Camera2D component types into a world.
 * Idempotent per world. Returns 0 on success, negative on failure. */
KY_API int ky2d_register_components(kyWorld *w);

KY_API kyTransform ky_transform_new(void);
KY_API kySprite ky_sprite_new(void);
KY_API kyCamera2D ky_camera2d_new(void);

/* Render one frame: clear, collect, sort, batch-draw all sprites visible
 * to the given camera entity (must carry a Camera2D component).
 * Returns the number of sprites drawn, or a negative code on error:
 *   -1 invalid arguments (rd/world/camera)
 * Returns 0 when the camera has a zero viewport or nothing to draw. */
KY_API int ky2d_render_world(kyRenderDevice *rd, kyWorld *w, kyEntity cam);

/* Same as above but picks the active camera with the smallest entity id.
 * Returns 0 when no active camera exists. */
KY_API int ky2d_render_world_auto(kyRenderDevice *rd, kyWorld *w);

/* Toggle console-backend debug listing of sorted sprites (default off). */
KY_API void ky2d_render_debug(int on);

/* View-projection matrix for a camera. `tr` (optional) is the camera
 * entity's Transform; when present, cam pos/rotation offset by it. */
KY_API kyMat4 ky2d_camera_view_proj(const kyCamera2D *cam, const kyTransform *tr);

/* Per-frame sprite generation hook.  The extension writes vertex and index
 * data into pre-allocated buffers (caller-provided).  Returns 0 on success.
 * out_vert_count / out_idx_count receive the number of primitives produced.
 * When NULL the built-in TRS-to-quad generator is used. */
typedef int (*ky2dSpriteGenFn)(const kyTransform *tr, const kySprite *sp,
                                void *out_verts, void *out_idx,
                                size_t *out_vert_count, size_t *out_idx_count,
                                void *user);
/* Per-frame camera view-projection computation hook.  When NULL the
 * built-in ortho projection is used. */
typedef kyMat4 (*ky2dCameraMatrixFn)(const kyCamera2D *cam, const kyTransform *tr, void *user);

/* Extensible render context.  Pass the address of a populated struct to
 * ky2d_render_with(); the default context returned by
 * ky2d_context_default() is used by ky2d_render_world / _auto. */
typedef struct ky2dContext {
    void *user;
    ky2dSpriteGenFn sprite_gen;
    ky2dCameraMatrixFn camera_matrix;
} ky2dContext;

KY_API ky2dContext ky2d_context_default(void);
KY_API int ky2d_render_with(kyRenderDevice *rd, kyWorld *w, kyEntity cam, const ky2dContext *ctx);

#endif
