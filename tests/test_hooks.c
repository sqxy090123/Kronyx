#include "kronyx/event.h"
#include "kronyx/input.h"
#include "kronyx/physics.h"
#include "kronyx/render.h"
#include "kronyx/2d.h"
#include <stdio.h>
#include <string.h>

static int g_event_hit_count = 0;
static const char *g_last_event_name = NULL;

static void event_listener(const char *name, const void *data, void *user) {
    g_event_hit_count++;
    g_last_event_name = name;
    (void)data; (void)user;
}

static void test_event_basic(void) {
    printf("--- event basic ---\n");
    int assertions = 0, failures = 0;
    #define A(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while(0)

    g_event_hit_count = 0;
    g_last_event_name = NULL;
    ky_event_register("jump", event_listener, NULL);
    ky_event_trigger("jump", NULL);
    A(g_event_hit_count == 1, "single listener fires once");
    A(strcmp(g_last_event_name, "jump") == 0, "event name preserved");
    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
}

static int g_queue_received = 0;

static void queue_sink(void *user, const kyInputEvent *ev, int count) {
    (void)user;
    g_queue_received += count;
}

static void test_input_queue(void) {
    printf("--- input queue ---\n");
    int assertions = 0, failures = 0;
    #define A(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while(0)

    ky_input_reset();
    g_queue_received = 0;

    ky_input_simulate_key(KY_KEY_A, 1);
    kyInputEvent ev;
    A(ky_input_poll(&ev) == 1, "poll returns event in default mode");
    A(ev.key == KY_KEY_A && ev.pressed == 1, "event content correct");

    ky_input_set_queue_handler(queue_sink, NULL);
    ky_input_simulate_key(KY_KEY_B, 1);
    A(g_queue_received == 1, "handler receives event");

    ky_input_set_queue_handler(NULL, NULL);
    ky_input_reset();
    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
}

static int g_broad_called = 0;
static int g_narrow_called = 0;

static void mock_broad(const kyExtents *extents, const uint32_t *ids, int count,
                       uint32_t *out_a, uint32_t *out_b, int *out_count, int max_pairs) {
    g_broad_called = 1;
    *out_count = 0;
    (void)extents; (void)ids; (void)out_a; (void)out_b; (void)max_pairs;
}

static void mock_narrow(uint32_t a, uint32_t b, int *alive) {
    g_narrow_called = 1;
    *alive = 0;
    (void)b;
}

static void test_physics_hooks(void) {
    printf("--- physics hooks ---\n");
    int assertions = 0, failures = 0;
    #define A(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while(0)

    kyAllocator al = ky_default_allocator();
    kyPhysicsWorld *pw = ky_physics_create(ky_vec3(0, -9.81f, 0));
    A(pw != NULL, "physics world created");

    g_broad_called = 0;
    g_narrow_called = 0;
    ky_physics_set_broadphase(pw, mock_broad);
    ky_physics_set_narrowphase(pw, mock_narrow);
    ky_physics_step(pw, 0.016f);
    A(g_broad_called, "broadphase hook called");
    /* narrowphase may not fire if there are no collision pairs */
    (void)g_narrow_called;

    ky_physics_set_broadphase(pw, NULL);
    ky_physics_set_narrowphase(pw, NULL);
    ky_physics_destroy(pw);
    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
}

static int g_draw_pass_called = 0;

static void mock_draw_pass(kyRenderDevice *rd, void *cmdlist, void *user) {
    g_draw_pass_called = 1;
    (void)rd; (void)cmdlist; (void)user;
}

static void test_render_hook(void) {
    printf("--- render draw-pass hook ---\n");
    int assertions = 0, failures = 0;
    #define A(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while(0);

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    A(rd != NULL, "console renderer created");

    g_draw_pass_called = 0;
    ky_rd_set_draw_pass(rd, mock_draw_pass, NULL);

    void *cl = ky_rd_begin(rd);
    A(cl != NULL, "command list begun");
    ky_rd_submit(rd, cl);
    A(g_draw_pass_called, "draw-pass hook called on submit");

    ky_rd_set_draw_pass(rd, NULL, NULL);
    ky_rd_destroy(rd);
    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
}

static int g_sprite_gen_called = 0;
static int g_camera_matrix_called = 0;

static int custom_sprite_gen(const kyTransform *tr, const kySprite *sp,
                              void *out_verts, void *out_idx,
                              size_t *out_vert_count, size_t *out_idx_count,
                              void *user) {
    (void)tr; (void)sp; (void)out_verts; (void)out_idx;
    (void)user;
    g_sprite_gen_called = 1;
    *out_vert_count = 0;
    *out_idx_count = 0;
    return 0;
}

static kyMat4 custom_camera_matrix(const kyCamera2D *cam, const kyTransform *tr, void *user) {
    (void)cam; (void)tr; (void)user;
    g_camera_matrix_called = 1;
    return ky_mat4_identity();
}

static void test_2d_hooks(void) {
    printf("--- 2d hooks ---\n");
    int assertions = 0, failures = 0;
    #define A(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while(0);

    ky2dContext ctx = ky2d_context_default();
    ctx.sprite_gen = custom_sprite_gen;
    ctx.camera_matrix = custom_camera_matrix;

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    ky2d_register_components(w);

    uint32_t tid_cam = 0;
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *ct =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (ct && ct->name && strcmp(ct->name, "camera2d") == 0)
            tid_cam = ct->type_id;
    }

    kyEntity cam = ky_world_spawn(w);
    ky_world_add_component(w, cam, tid_cam);
    kyCamera2D *c = (kyCamera2D *)ky_world_get_component(w, cam, tid_cam);
    if (c) { c->active = 1; c->viewport = ky_vec2(8.0f, 8.0f); }

    g_sprite_gen_called = 0;
    g_camera_matrix_called = 0;
    ky2d_render_with(rd, w, cam, &ctx);
    /* sprite_gen only fires when there are sprites; camera_matrix always fires */
    A(g_camera_matrix_called, "camera_matrix hook was called");

    ky_world_destroy(w);
    ky_rd_destroy(rd);
    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
}

int main(void) {
    printf("=== Extension Hook Tests ===\n\n");
    test_event_basic();
    test_input_queue();
    test_physics_hooks();
    test_render_hook();
    test_2d_hooks();
    return 0;
}
