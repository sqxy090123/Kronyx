#include "kronyx/2d.h"
#include "kronyx/math.h"
#include "kronyx/log.h"
#include <stdio.h>
#include <string.h>

#ifdef KY_HAS_EGL
#include <GLES3/gl3.h>
#endif

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
} while (0)

#define NEAR(a, b) KY_CHECK_NEAR_IMPL(a, b, 0.001f)
static int near_f(float a, float b, float eps) {
    return (a > b - eps) && (a < b + eps);
}
#define KY_CHECK_NEAR_IMPL(a, b, eps) near_f((a), (b), (eps))

static void test_component_defaults(void) {
    printf("--- component defaults ---\n");

    kyTransform t = ky_transform_new();
    ASSERT(t.pos.x == 0 && t.pos.y == 0, "transform default pos (0,0)");
    ASSERT(t.rotation_z == 0.0f, "transform default rotation 0");
    ASSERT(t.scale.x == 1.0f && t.scale.y == 1.0f, "transform default scale (1,1)");
    ASSERT(t.z == 0.0f, "transform default z 0");

    kySprite s = ky_sprite_new();
    ASSERT(s.texture == NULL, "sprite default texture NULL");
    ASSERT(s.color.x == 1.0f && s.color.w == 1.0f, "sprite default color white");
    ASSERT(s.size.x == 1.0f && s.size.y == 1.0f, "sprite default size (1,1)");
    ASSERT(s.uv0.x == 0.0f && s.uv1.x == 1.0f, "sprite default uv full");
    ASSERT(s.layer == 0 && s.flip == 0, "sprite default layer/flip");

    kyCamera2D c = ky_camera2d_new();
    ASSERT(c.zoom == 1.0f, "camera default zoom 1");
    ASSERT(c.active == 0, "camera default inactive");
    ASSERT(c.clear_color.x == 0.1f, "camera default clear color");

    /* registration: success + idempotent */
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ASSERT(w != NULL, "world create");
    ASSERT(ky2d_register_components(w) == 0, "register components");
    ASSERT(ky2d_register_components(w) == 0, "re-register components");

    size_t count = 0;
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *ct =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (ct->name && strcmp(ct->name, "transform") == 0) count++;
    }
    ASSERT(count == 1, "duplicate registration reused type_id");

    /* ctor defaults applied on add */
    uint32_t tid_sprite = 0;
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *ct =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (ct->name && strcmp(ct->name, "sprite") == 0) tid_sprite = ct->type_id;
    }
    kyEntity e = ky_world_spawn(w);
    kySprite *s2 = (kySprite *)ky_world_add_component(w, e, tid_sprite);
    ASSERT(s2 && s2->color.w == 1.0f && s2->size.x == 1.0f, "ctor defaults applied");
    ky_world_destroy(w);
}

static void test_camera_math(void) {
    printf("--- camera math ---\n");

    /* camera at (2,3), viewport 10x10: world point under camera maps to clip origin */
    kyCamera2D cam = ky_camera2d_new();
    cam.pos.x = 2.0f;
    cam.pos.y = 3.0f;
    cam.viewport.x = 10.0f;
    cam.viewport.y = 10.0f;
    kyMat4 vp = ky2d_camera_view_proj(&cam, NULL);
    kyVec4 p = ky_mat4_mul_vec4(&vp, ky_vec4(2.0f, 3.0f, 0.0f, 1.0f));
    ASSERT(near_f(p.x, 0.0f, 0.001f) && near_f(p.y, 0.0f, 0.001f),
           "camera center maps to clip origin");

    /* zoom=2 magnifies: world offset (2,0) -> clip x = 2*2/(10/2) = 0.8 */
    cam.zoom = 2.0f;
    cam.pos.x = 0.0f;
    cam.pos.y = 0.0f;
    vp = ky2d_camera_view_proj(&cam, NULL);
    p = ky_mat4_mul_vec4(&vp, ky_vec4(2.0f, 0.0f, 0.0f, 1.0f));
    ASSERT(near_f(p.x, 0.8f, 0.001f), "zoom=2 magnifies world into clip");

    /* rotation 90 deg CCW: world (1,0) -> view (0,-1) -> clip y=-0.2 */
    cam.zoom = 1.0f;
    cam.rotation_z = KY_PI * 0.5f;
    vp = ky2d_camera_view_proj(&cam, NULL);
    p = ky_mat4_mul_vec4(&vp, ky_vec4(1.0f, 0.0f, 0.0f, 1.0f));
    ASSERT(near_f(p.x, 0.0f, 0.001f) && near_f(p.y, -0.2f, 0.001f),
           "camera rotation rotates world inversely");

    /* camera transform offset: cam_tr adds pos */
    kyCamera2D cam2 = ky_camera2d_new();
    cam2.viewport.x = 10.0f;
    cam2.viewport.y = 10.0f;
    kyTransform ctr = ky_transform_new();
    ctr.pos.x = 5.0f;
    vp = ky2d_camera_view_proj(&cam2, &ctr);
    p = ky_mat4_mul_vec4(&vp, ky_vec4(5.0f, 0.0f, 0.0f, 1.0f));
    ASSERT(near_f(p.x, 0.0f, 0.001f) && near_f(p.y, 0.0f, 0.001f),
           "camera transform contributes to view");
}

static void test_render_logic(void) {
    printf("--- render logic (console) ---\n");

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    ASSERT(rd != NULL, "console device");
    ASSERT(ky2d_render_world(NULL, NULL, (kyEntity){0u, 999u}) < 0, "NULL args rejected");

    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);

    uint32_t tid_sprite = 0, tid_transform = 0, tid_camera = 0;
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *ct =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (ct->name && strcmp(ct->name, "sprite") == 0) tid_sprite = ct->type_id;
        if (ct->name && strcmp(ct->name, "transform") == 0) tid_transform = ct->type_id;
        if (ct->name && strcmp(ct->name, "camera2d") == 0) tid_camera = ct->type_id;
    }

    /* no camera yet */
    ASSERT(ky2d_render_world_auto(rd, w) == 0, "no active camera returns 0");

    kyEntity cam = ky_world_spawn(w);
    kyCamera2D *c = (kyCamera2D *)ky_world_add_component(w, cam, tid_camera);
    ASSERT(c != NULL, "camera added with defaults");

    /* camera inactive by default: auto skips */
    ASSERT(ky2d_render_world_auto(rd, w) == 0, "inactive camera skipped by auto");
    /* explicit render with zero viewport returns 0 */
    c->viewport.x = 0.0f;
    c->viewport.y = 0.0f;
    ASSERT(ky2d_render_world(rd, w, cam) == 0, "zero viewport returns 0");
    c->viewport.x = 8.0f;
    c->viewport.y = 8.0f;

    c->active = 1;
    c->viewport.x = 8.0f;
    c->viewport.y = 8.0f;

    /* sprites without transform: still drawn with identity */
    kyEntity s1 = ky_world_spawn(w);
    kySprite *sp1 = (kySprite *)ky_world_add_component(w, s1, tid_sprite);
    sp1->size.x = 2.0f;
    sp1->size.y = 2.0f;
    ASSERT(ky2d_render_world_auto(rd, w) == 1, "sprite without transform drawn");

    /* zero-scale transform skipped */
    kyEntity s2 = ky_world_spawn(w);
    kySprite *sp2 = (kySprite *)ky_world_add_component(w, s2, tid_sprite);
    sp2->size.x = 2.0f;
    sp2->size.y = 2.0f;
    kyTransform *tr2 = (kyTransform *)ky_world_add_component(w, s2, tid_transform);
    tr2->scale.x = 0.0f;
    ASSERT(ky2d_render_world_auto(rd, w) == 1, "zero-scale sprite skipped");

    /* invalid entity rejected */
    kyEntity bad = cam;
    bad.version += 100;
    ASSERT(ky2d_render_world(rd, w, bad) < 0, "stale entity rejected");

    /* debug listing smoke */
    ky2d_render_debug(1);
    int r = ky2d_render_world_auto(rd, w);
    ky2d_render_debug(0);
    ASSERT(r == 1, "debug listing smoke run");

    /* three sprites: mixed layers/z ordering still all drawn */
    kyEntity s3 = ky_world_spawn(w);
    kySprite *sp3 = (kySprite *)ky_world_add_component(w, s3, tid_sprite);
    sp3->size.x = 1.0f;
    sp3->size.y = 1.0f;
    sp3->layer = 5;
    kyTransform *tr3 = (kyTransform *)ky_world_add_component(w, s3, tid_transform);
    tr3->z = -1.0f;
    ASSERT(ky2d_render_world_auto(rd, w) == 2, "ordering scene draw count");

    ky_world_destroy(w);
    ky_rd_destroy(rd);
}

#ifdef KY_HAS_EGL
/* GL pixel assertions on the fixed 128x128 surface. viewport 8 EU -> 16 px/EU. */
static void test_gl_pixels(void) {
    printf("--- GL pixel assertions ---\n");

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_GL, NULL);
    ASSERT(rd != NULL, "GL device");

    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);

    uint32_t tid_sprite = 0, tid_camera = 0;
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *ct =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (ct->name && strcmp(ct->name, "sprite") == 0) tid_sprite = ct->type_id;
        if (ct->name && strcmp(ct->name, "camera2d") == 0) tid_camera = ct->type_id;
    }

    kyEntity cam = ky_world_spawn(w);
    kyCamera2D *c = (kyCamera2D *)ky_world_add_component(w, cam, tid_camera);
    c->active = 1;
    c->viewport.x = 8.0f;
    c->viewport.y = 8.0f;

    /* scene A: red 2x2 EU sprite at origin -> center pixel red */
    kyEntity a = ky_world_spawn(w);
    kySprite *sa = (kySprite *)ky_world_add_component(w, a, tid_sprite);
    sa->size.x = 2.0f;
    sa->size.y = 2.0f;
    sa->color.x = 1.0f;
    sa->color.y = 0.0f;
    sa->color.z = 0.0f;
    ky2d_render_debug(1);
    int drawn = ky2d_render_world_auto(rd, w);
    ASSERT(drawn == 1, "GL scene A draws 1 sprite");
    ky2d_render_debug(0);
    unsigned char px[4];
    glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);

    /* scene B: blue sprite with higher layer on top */
    kyEntity b = ky_world_spawn(w);
    kySprite *sb = (kySprite *)ky_world_add_component(w, b, tid_sprite);
    sb->size.x = 2.0f;
    sb->size.y = 2.0f;
    sb->color.x = 0.0f;
    sb->color.y = 0.0f;
    sb->color.z = 1.0f;
    sb->layer = 10;
    drawn = ky2d_render_world_auto(rd, w);
    ASSERT(drawn == 2, "GL scene B draws 2 sprites");
    glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    ASSERT(px[2] > 200 && px[0] < 50, "higher layer covers lower (blue on top)");

    /* scene C: camera shifts +1 EU -> sprite moves 16 px left */
    ((kyCamera2D *)ky_world_get_component(w, cam, tid_camera))->pos.x = 1.0f;
    ky_world_despawn(w, b);
    drawn = ky2d_render_world_auto(rd, w);
    ASSERT(drawn == 1, "GL scene C draws 1 sprite");
    glReadPixels(48, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    ASSERT(px[0] > 200 && px[1] < 50, "shifted camera: pixel at 48 is red");
    glReadPixels(80, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    ASSERT(px[0] < 50, "shifted camera: pixel at 80 is clear color");

    /* scene D: restore camera, 2x1 red/blue texture, linear-filtered edges */
    ((kyCamera2D *)ky_world_get_component(w, cam, tid_camera))->pos.x = 0.0f;
    kyEntity d = ky_world_spawn(w);
    kySprite *sd = (kySprite *)ky_world_add_component(w, d, tid_sprite);
    sd->size.x = 2.0f;
    sd->size.y = 2.0f;
    sd->color.x = 1.0f;
    sd->color.y = 1.0f;
    sd->color.z = 1.0f;
    const unsigned char rb[8] = {255, 0, 0, 255, 0, 0, 255, 255};
    sd->texture = ky_rd_create_texture_2d(rd, 2, 1, 4, rb);
    ASSERT(sd->texture != NULL, "scene D texture created");
    drawn = ky2d_render_world_auto(rd, w);
    ASSERT(drawn == 2, "GL scene D draws 2 sprites (red + textured)");
    glReadPixels(52, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    ASSERT(px[0] > 150 && px[2] < 100 && px[1] < 60, "texture left half reddish");
    glReadPixels(76, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    ASSERT(px[2] > 150 && px[0] < 100 && px[1] < 60, "texture right half blueish");

    ky_world_destroy(w);
    ky_rd_destroy(rd);
}
#endif

int main(void) {
    printf("=== 2D Render Pipeline Test ===\n");
    test_component_defaults();
    test_camera_math();
    test_render_logic();
#ifdef KY_HAS_EGL
    test_gl_pixels();
#else
    printf("SKIP: GL pixel assertions (built without EGL)\n");
#endif
    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
