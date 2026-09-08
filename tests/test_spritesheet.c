#include "kronyx/anim2d.h"
#include "kronyx/2d.h"
#include "kronyx/ecs.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define KY_CHECK(expr) do { \
    if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; } \
} while (0)

static int failures = 0;
static float uv_eq(float a, float b) {
    float d = a - b;
    return d >= -1e-5f && d <= 1e-5f;
}

static void test_defaults(void) {
    kyAnimator a = ky_animator_new();
    KY_CHECK(a.cols == 1);
    KY_CHECK(a.frames == 1);
    KY_CHECK(a.fps > 0);
    KY_CHECK(a.looping == 1);
    KY_CHECK(a.time == 0.0f);
    KY_CHECK(a.current_frame == 0);
}

static void test_single_frame_uv(void) {
    kyAnimator a = ky_animator_new();
    a.cols = 1; a.frames = 1;
    kyVec2 u0, u1;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.x, 0.0f) && uv_eq(u0.y, 0.0f));
    KY_CHECK(uv_eq(u1.x, 1.0f) && uv_eq(u1.y, 1.0f));
}

static void test_1x4_vertical_strip(void) {
    kyAnimator a;
    memset(&a, 0, sizeof(a));
    a.cols = 1; a.frames = 4; a.fps = 4; a.looping = 0; a.current_frame = 0;
    kyVec2 u0, u1;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.x, 0.0f) && uv_eq(u0.y, 0.0f));
    KY_CHECK(uv_eq(u1.x, 1.0f) && uv_eq(u1.y, 0.25f));

    a.current_frame = 1;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.y, 0.25f) && uv_eq(u1.y, 0.5f));

    a.current_frame = 3;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.y, 0.75f) && uv_eq(u1.y, 1.0f));
}

static void test_2x3_grid(void) {
    kyAnimator a;
    memset(&a, 0, sizeof(a));
    a.cols = 2; a.frames = 6; a.fps = 6; a.looping = 1;
    kyVec2 u0, u1;

    a.current_frame = 0;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.x, 0.0f) && uv_eq(u0.y, 0.0f));
    KY_CHECK(uv_eq(u1.x, 0.5f) && uv_eq(u1.y, 0.333333f));

    a.current_frame = 1;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.x, 0.5f) && uv_eq(u0.y, 0.0f));
    KY_CHECK(uv_eq(u1.x, 1.0f) && uv_eq(u1.y, 0.333333f));

    a.current_frame = 2;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.x, 0.0f) && uv_eq(u0.y, 0.333333f));
    KY_CHECK(uv_eq(u1.x, 0.5f) && uv_eq(u1.y, 0.666666f));

    a.current_frame = 5;
    KY_CHECK(ky_animator_frame_uv(&a, &u0, &u1) == 0);
    KY_CHECK(uv_eq(u0.x, 0.5f) && uv_eq(u0.y, 0.666666f));
    KY_CHECK(uv_eq(u1.x, 1.0f) && uv_eq(u1.y, 1.0f));
}

static void test_apply_to_sprite(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);
    KY_CHECK(ky_anim2d_register(w) == 0);

    uint32_t tid_sp = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_anim = ky_world_component_type_by_name(w, "animator");
    KY_CHECK(tid_sp != UINT32_MAX);
    KY_CHECK(tid_anim != UINT32_MAX);

    kyEntity e = ky_world_spawn(w);
    kySprite *sp = (kySprite *)ky_world_add_component(w, e, tid_sp);
    sp->texture = NULL;
    sp->size = ky_vec2(1, 1);

    kyAnimator a = ky_animator_new();
    a.cols = 2; a.frames = 4; a.fps = 4; a.looping = 1;
    a.current_frame = 1;
    void *aptr = ky_world_add_component(w, e, tid_anim);
    KY_CHECK(aptr != NULL);
    memcpy(aptr, &a, sizeof(a));

    KY_CHECK(ky_animator_apply_to_sprite(&a, sp) == 0);
    KY_CHECK(uv_eq(sp->uv0.x, 0.5f) && uv_eq(sp->uv0.y, 0.0f));
    KY_CHECK(uv_eq(sp->uv1.x, 1.0f) && uv_eq(sp->uv1.y, 0.5f));

    ky_world_destroy(w);
}

static void test_system_advances_frame(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);
    KY_CHECK(ky_anim2d_register(w) == 0);

    uint32_t tid_sp   = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_anim = ky_world_component_type_by_name(w, "animator");
    KY_CHECK(tid_anim != UINT32_MAX);

    kyEntity e = ky_world_spawn(w);
    ky_world_add_component(w, e, tid_sp);

    kyAnimator a = ky_animator_new();
    a.cols = 2; a.frames = 4; a.fps = 4; a.looping = 0;
    void *aptr = ky_world_add_component(w, e, tid_anim);
    KY_CHECK(aptr != NULL);
    memcpy(aptr, &a, sizeof(a));

    ky_world_step(w, 0.25f);

    a      = *(kyAnimator *)ky_world_get_component(w, e, tid_anim);
    kySprite *sp = (kySprite *)ky_world_get_component(w, e, tid_sp);
    KY_CHECK(a.current_frame == 1);
    KY_CHECK(uv_eq(sp->uv0.x, 0.5f) && uv_eq(sp->uv0.y, 0.0f));
    KY_CHECK(uv_eq(sp->uv1.x, 1.0f) && uv_eq(sp->uv1.y, 0.5f));

    ky_world_destroy(w);
}

static void test_looping_wrap(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);
    KY_CHECK(ky_anim2d_register(w) == 0);

    uint32_t tid_sp   = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_anim = ky_world_component_type_by_name(w, "animator");

    kyEntity e = ky_world_spawn(w);
    ky_world_add_component(w, e, tid_sp);
    kyAnimator a = ky_animator_new();
    a.cols = 2; a.frames = 4; a.fps = 4; a.looping = 1;
    void *aptr = ky_world_add_component(w, e, tid_anim);
    memcpy(aptr, &a, sizeof(a));

    ky_world_step(w, 1.0f);
    a = *(kyAnimator *)ky_world_get_component(w, e, tid_anim);
    KY_CHECK(a.current_frame == 0);

    ky_world_step(w, 1.25f);
    a = *(kyAnimator *)ky_world_get_component(w, e, tid_anim);
    KY_CHECK(a.current_frame == 1);

    ky_world_destroy(w);
}

static void test_non_looping_clamp(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);
    KY_CHECK(ky_anim2d_register(w) == 0);

    uint32_t tid_sp   = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_anim = ky_world_component_type_by_name(w, "animator");

    kyEntity e = ky_world_spawn(w);
    ky_world_add_component(w, e, tid_sp);
    kyAnimator a = ky_animator_new();
    a.cols = 2; a.frames = 4; a.fps = 4; a.looping = 0;
    void *aptr = ky_world_add_component(w, e, tid_anim);
    memcpy(aptr, &a, sizeof(a));

    ky_world_step(w, 10.0f);
    a = *(kyAnimator *)ky_world_get_component(w, e, tid_anim);
    KY_CHECK(a.current_frame == 3);
    KY_CHECK(a.time <= 1.0f + 1e-6f);

    ky_world_destroy(w);
}

static void test_invalid_params(void) {
    kyAnimator bad;
    memset(&bad, 0, sizeof(bad));
    kyVec2 u0, u1;
    KY_CHECK(ky_animator_frame_uv(NULL, &u0, &u1) < 0);
    KY_CHECK(ky_animator_frame_uv(&bad, NULL, &u1) < 0);
    KY_CHECK(ky_animator_frame_uv(&bad, &u0, NULL) < 0);

    bad.cols = 0;
    KY_CHECK(ky_animator_frame_uv(&bad, &u0, &u1) < 0);
    bad.cols = 2; bad.frames = 0;
    KY_CHECK(ky_animator_frame_uv(&bad, &u0, &u1) < 0);
    bad.frames = 4; bad.fps = 0;
    KY_CHECK(ky_animator_frame_uv(&bad, &u0, &u1) < 0);

    KY_CHECK(ky_animator_apply_to_sprite(&bad, NULL) < 0);
}

static void test_no_system_no_advance(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);

    uint32_t tid_sp = ky_world_component_type_by_name(w, "sprite");
    KY_CHECK(ky_world_component_type_by_name(w, "animator") == UINT32_MAX);

    kyEntity e = ky_world_spawn(w);
    kySprite *sp = (kySprite *)ky_world_add_component(w, e, tid_sp);
    sp->texture = NULL;
    sp->size = ky_vec2(1, 1);
    sp->uv0 = ky_vec2(0.0f, 0.0f);
    sp->uv1 = ky_vec2(0.5f, 0.5f);

    ky_world_step(w, 0.25f);
    sp = (kySprite *)ky_world_get_component(w, e, tid_sp);
    KY_CHECK(uv_eq(sp->uv0.x, 0.0f) && uv_eq(sp->uv1.x, 0.5f));

    ky_world_destroy(w);
}

int main(void) {
    printf("=== Sprite Animation Tests ===\n");
    test_defaults();
    test_single_frame_uv();
    test_1x4_vertical_strip();
    test_2x3_grid();
    test_apply_to_sprite();
    test_system_advances_frame();
    test_looping_wrap();
    test_non_looping_clamp();
    test_invalid_params();
    test_no_system_no_advance();
    printf("\n%s: passed=%d, failures=%d\n", __FILE__, 30 - failures, failures);
    return failures ? 1 : 0;
}
