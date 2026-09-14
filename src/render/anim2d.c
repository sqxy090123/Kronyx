#include "kronyx/anim2d.h"
#include "kronyx/ecs.h"
#include <string.h>

/* ── Public API ──────────────────────────────────────────────────── */

kyAnimator ky_animator_new(void) {
    kyAnimator a;
    memset(&a, 0, sizeof(a));
    a.cols              = 1;
    a.frames            = 1;
    a.fps               = 10.0f;
    a.looping           = 1;
    a.time              = 0.0f;
    a.current_frame     = 0;
    return a;
}

int ky_animator_frame_uv(const kyAnimator *a, kyVec2 *out_uv0, kyVec2 *out_uv1) {
    if (!a || !out_uv0 || !out_uv1) return -1;
    if (a->cols <= 0 || a->frames <= 0 || a->fps <= 0) return -2;
    int f = a->current_frame % a->frames;
    if (f < 0) f += a->frames;

    int col      = f % a->cols;
    int row      = f / a->cols;
    int nrows    = (a->frames + a->cols - 1) / a->cols;

    float cell_w = 1.0f / a->cols;
    float cell_h = 1.0f / nrows;

    out_uv0->x = (float)col * cell_w;
    out_uv0->y = (float)row * cell_h;
    out_uv1->x = out_uv0->x + cell_w;
    out_uv1->y = out_uv0->y + cell_h;
    return 0;
}

int ky_animator_apply_to_sprite(const kyAnimator *a, kySprite *sp) {
    kyVec2 u0, u1;
    int r = ky_animator_frame_uv(a, &u0, &u1);
    if (r != 0) return r;
    sp->uv0 = u0;
    sp->uv1 = u1;
    return 0;
}

/* ── System ──────────────────────────────────────────────────────── */

static void anim_update(kyWorld *w, float dt, void *user) {
    KY_UNUSED(user);
    uint32_t tid_sprite = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_anim   = ky_world_component_type_by_name(w, "animator");
    if (tid_sprite == UINT32_MAX || tid_anim == UINT32_MAX) return;

    const uint32_t types[2] = { tid_sprite, tid_anim };
    kyViewIter it;
    int more = ky_view_begin(w, types, 2, &it);
    while (more) {
        kyEntity e     = it.current;
        kySprite   *sp = (kySprite *)ky_world_get_component(w, e, tid_sprite);
        kyAnimator *a  = (kyAnimator *)ky_world_get_component(w, e, tid_anim);
        if (sp && a && a->cols > 0 && a->frames > 0 && a->fps > 0) {
            a->time += dt;
            float step = 1.0f / a->fps;
            int   f    = (int)(a->time / step);
            if (f < 0) f = 0;
            if (a->looping) {
                a->current_frame = f % a->frames;
            } else {
                if ((unsigned)f >= (unsigned)a->frames) {
                    f = a->frames - 1;
                    a->time = step * f;
                }
                a->current_frame = f;
            }
            ky_animator_apply_to_sprite(a, sp);
        }
        more = ky_view_next(&it);
    }
}

int ky_anim2d_register(kyWorld *w) {
    kyComponentType ct;
    memset(&ct, 0, sizeof(ct));
    ct.name = "animator";
    ct.size = sizeof(kyAnimator);
    ct.ctor = NULL;
    ct.dtor = NULL;
    ky_world_register_component(w, &ct);

    kySystem sys;
    memset(&sys, 0, sizeof(sys));
    sys.name   = "sprite-animator";
    sys.order  = 0;
    sys.update = anim_update;
    ky_world_register_system(w, &sys);
    return 0;
}
