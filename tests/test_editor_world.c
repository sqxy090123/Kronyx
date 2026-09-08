/* Headless editor-world integration test.
 *
 * Validates that the editor core connects to a kyWorld:
 *   - Hierarchy lists alive entities
 *   - Properties read/write Transform / Sprite / Camera2D
 *   - Spawn / despawn changes hierarchy count
 *   - Null-safety (no crash on NULL world / invalid entity)
 */

#include "kronyx/editor_core.h"
#include "kronyx/2d.h"
#include "kronyx/render.h"
#include "kronyx/input.h"
#include "kronyx/memory.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int assertions = 0;
static int failures = 0;

#define CHECK(cond, msg) do { \
    assertions++; \
    if (!(cond)) { failures++; printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

#define CHECK_F(a, b, eps, msg) do { \
    assertions++; \
    double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (eps)) { \
        failures++; \
        printf("FAIL: %s (got %f, want %f) line %d\n", msg, _a, _b, __LINE__); \
    } \
} while (0)

/* Allocator that outlives all worlds created in this test. */
static kyAllocator g_alloc;

static kyWorld *make_world(void) {
    kyWorld *w = ky_world_create(&g_alloc);
    CHECK(w != NULL, "world creation");
    int r = ky2d_register_components(w);
    CHECK(r == 0, "2d component registration");
    return w;
}

static void make_default_scene(kyWorld *w,
                               kyEntity *out_player, kyEntity *out_cam,
                               kyEntity *out_ground) {
    /* Player: Transform + Sprite */
    kyEntity p = ky_world_spawn(w);
    ky_world_add_component(w, p,
        ky_world_component_type_by_name(w, "transform"));
    ky_world_add_component(w, p,
        ky_world_component_type_by_name(w, "sprite"));
    kyTransform *ptr = (kyTransform *)ky_world_get_component(w, p,
        ky_world_component_type_by_name(w, "transform"));
    ptr->pos.x = 1.0f;
    ptr->pos.y = 2.0f;
    ptr->scale.x = 2.0f;
    ptr->scale.y = 3.0f;
    kySprite *psp = (kySprite *)ky_world_get_component(w, p,
        ky_world_component_type_by_name(w, "sprite"));
    psp->layer = 5;
    if (out_player) *out_player = p;

    /* Camera: Transform + Camera2D */
    kyEntity c = ky_world_spawn(w);
    ky_world_add_component(w, c,
        ky_world_component_type_by_name(w, "transform"));
    ky_world_add_component(w, c,
        ky_world_component_type_by_name(w, "camera2d"));
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(w, c,
        ky_world_component_type_by_name(w, "camera2d"));
    cam->zoom = 2.0f;
    cam->viewport.x = 800.0f;
    cam->viewport.y = 600.0f;
    if (out_cam) *out_cam = c;

    /* Ground: Transform + Sprite */
    kyEntity g = ky_world_spawn(w);
    ky_world_add_component(w, g,
        ky_world_component_type_by_name(w, "transform"));
    ky_world_add_component(w, g,
        ky_world_component_type_by_name(w, "sprite"));
    if (out_ground) *out_ground = g;
}

/* ── Test: create / destroy ─────────────────────────────────────────── */

static void test_create_destroy(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    CHECK(app != NULL, "editor create with world");
    CHECK(ky_editor_app_count_entities(app) == 0,
          "empty hierarchy after create");
    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: null safety ──────────────────────────────────────────────── */

static void test_null_safety(void) {
    CHECK(ky_editor_app_count_entities(NULL) == 0, "null app count");
    CHECK(ky_editor_app_get_pos_x(NULL) == 0.0f, "null app get pos x");
    CHECK_F(ky_editor_app_get_pos_y(NULL), 0.0, 1e-6,
            "null app get pos y");
}

/* ── Test: hierarchy ───────────────────────────────────────────────── */

static void test_hierarchy(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    CHECK(ky_editor_app_count_entities(app) == 0,
          "hierarchy empty before spawn");

    /* Spawn and register via editor so they appear in hierarchy. */
    kyEntity e1 = ky_editor_app_spawn_and_select(app);
    /* Give it a component so it is "alive" in ECS terms. */
    ky_world_add_component(w, e1, ky_editor_transform_type_id(w));
    CHECK(ky_editor_app_count_entities(app) == 1,
          "hierarchy 1 after first spawn");
    kyEntity got1 = ky_editor_app_get_entity(app, 0);
    CHECK(ky_entity_valid(w, got1), "entity at index 0 is valid");

    kyEntity e2 = ky_editor_app_spawn_and_select(app);
    ky_world_add_component(w, e2, ky_editor_transform_type_id(w));
    CHECK(ky_editor_app_count_entities(app) == 2,
          "hierarchy 2 after second spawn");
    kyEntity got0 = ky_editor_app_get_entity(app, 0);
    kyEntity got1b = ky_editor_app_get_entity(app, 1);
    CHECK(ky_entity_valid(w, got0), "entity[0] valid");
    CHECK(ky_entity_valid(w, got1b), "entity[1] valid");

    /* Out-of-range returns zero entity */
    kyEntity bad = ky_editor_app_get_entity(app, 999);
    CHECK(!ky_entity_valid(w, bad), "out-of-range entity invalid");

    /* Despawning one still counts correctly */
    ky_editor_app_select(app, e1);
    ky_editor_app_despawn_selected(app);
    CHECK(ky_editor_app_count_entities(app) == 1,
          "hierarchy 1 after despawn one");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: transform read/write ─────────────────────────────────────── */

static void test_transform_props(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    make_default_scene(w, NULL, NULL, NULL);

    /* Select first entity (player from default scene) */
    kyEntity e = ky_editor_app_get_entity(app, 0);
    ky_editor_app_select(app, e);
    CHECK(ky_editor_app_selected_is_valid(app),
          "selected entity is valid");

    /* Read back default position */
    CHECK_F(ky_editor_app_get_pos_x(app), 1.0f, 1e-5,
            "pos_x == 1.0");
    CHECK_F(ky_editor_app_get_pos_y(app), 2.0f, 1e-5,
            "pos_y == 2.0");
    CHECK_F(ky_editor_app_get_scale_x(app), 2.0f, 1e-5,
            "scale_x == 2.0");
    CHECK_F(ky_editor_app_get_scale_y(app), 3.0f, 1e-5,
            "scale_y == 3.0");

    /* Write new values */
    ky_editor_app_set_pos(app, 5.5f, -3.2f);
    CHECK_F(ky_editor_app_get_pos_x(app), 5.5f, 1e-5,
            "pos_x after set");
    CHECK_F(ky_editor_app_get_pos_y(app), -3.2f, 1e-5,
            "pos_y after set");

    ky_editor_app_set_scale(app, 0.5f, 4.0f);
    CHECK_F(ky_editor_app_get_scale_x(app), 0.5f, 1e-5,
            "scale_x after set");
    CHECK_F(ky_editor_app_get_scale_y(app), 4.0f, 1e-5,
            "scale_y after set");

    /* Writing to invalid selection is a no-op */
    kyEntity bad = {999, 999};
    ky_editor_app_select(app, bad);
    float before_x = ky_editor_app_get_pos_x(app);
    ky_editor_app_set_pos(app, 999.0f, 999.0f);
    CHECK_F(ky_editor_app_get_pos_x(app), before_x, 1e-5,
            "set on invalid selected is no-op");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: sprite layer ─────────────────────────────────────────────── */

static void test_sprite_layer(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    make_default_scene(w, NULL, NULL, NULL);

    kyEntity e = ky_editor_app_get_entity(app, 0);
    ky_editor_app_select(app, e);
    CHECK(ky_editor_app_get_sprite_layer(app) == 5,
          "default sprite layer == 5");

    ky_editor_app_set_sprite_layer(app, 10);
    CHECK(ky_editor_app_get_sprite_layer(app) == 10,
          "sprite layer updated");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: camera zoom ──────────────────────────────────────────────── */

static void test_camera_zoom(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    make_default_scene(w, NULL, NULL, NULL);

    /* Find camera entity */
    int cnt = ky_editor_app_count_entities(app);
    kyEntity cam_e = {0, 0};
    uint32_t ctid = ky_editor_camera_type_id(w);
    for (int i = 0; i < cnt; i++) {
        kyEntity e = ky_editor_app_get_entity(app, i);
        if (ky_world_has_component(w, e, ctid)) {
            cam_e = e;
            break;
        }
    }
    CHECK(ky_entity_valid(w, cam_e), "camera entity found");

    ky_editor_app_select(app, cam_e);
    CHECK_F(ky_editor_app_get_camera_zoom(app), 2.0f, 1e-5,
            "default zoom == 2.0");

    ky_editor_app_set_camera_zoom(app, 4.5f);
    CHECK_F(ky_editor_app_get_camera_zoom(app), 4.5f, 1e-5,
            "zoom updated");

    /* Viewport size */
    CHECK_F(ky_editor_app_get_camera_viewport_w(app), 800.0f, 1e-5,
            "viewport w == 800");
    CHECK_F(ky_editor_app_get_camera_viewport_h(app), 600.0f, 1e-5,
            "viewport h == 600");

    ky_editor_app_set_camera_viewport_size(app, 1024.0f, 768.0f);
    CHECK_F(ky_editor_app_get_camera_viewport_w(app), 1024.0f, 1e-5,
            "viewport w after set");
    CHECK_F(ky_editor_app_get_camera_viewport_h(app), 768.0f, 1e-5,
            "viewport h after set");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: spawn_and_select / despawn_selected ──────────────────────── */

static void test_spawn_despawn(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);

    CHECK(ky_editor_app_count_entities(app) == 0,
          "hierarchy empty before spawn");

    kyEntity e = ky_editor_app_spawn_and_select(app);
    CHECK(ky_entity_valid(w, e), "spawned entity valid");
    CHECK(ky_editor_app_count_entities(app) == 1,
          "hierarchy has 1 after spawn");
    kyEntity sel = ky_editor_app_get_selected(app);
    CHECK(sel.id == e.id && sel.version == e.version,
          "selected == newly spawned");

    ky_editor_app_despawn_selected(app);
    CHECK(!ky_entity_valid(w, e), "despawned entity invalid");
    CHECK(ky_editor_app_count_entities(app) == 0,
          "hierarchy empty after despawn");
    CHECK(!ky_editor_app_selected_is_valid(app),
          "no selection after despawn");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: render with console backend ──────────────────────────────── */

static void test_render(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    make_default_scene(w, NULL, NULL, NULL);

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    CHECK(rd != NULL, "console renderer created");

    int n = ky_editor_app_render(app, rd);
    CHECK(n >= 0, "render returned non-negative (sprites drawn)");

    ky_rd_destroy(rd);
    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: input poll ───────────────────────────────────────────────── */

static void test_input_poll(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);

    /* No events buffered */
    kyInputKey key;
    int pressed;
    CHECK(ky_editor_app_poll_input(app, &key, &pressed) == 0,
          "no input when buffer empty");

    /* Simulate a key press */
    ky_input_simulate_key(KY_KEY_A, 1);
    CHECK(ky_editor_app_poll_input(app, &key, &pressed) == 1,
          "poll returns event after simulate");
    CHECK(key == KY_KEY_A, "key is A");
    CHECK(pressed == 1, "pressed == 1");

    /* Release event */
    ky_input_simulate_key(KY_KEY_A, 0);
    CHECK(ky_editor_app_poll_input(app, &key, &pressed) == 1,
          "release event available");
    CHECK(key == KY_KEY_A, "key is A on release");
    CHECK(pressed == 0, "pressed == 0 on release");

    /* Buffer drained */
    CHECK(ky_editor_app_poll_input(app, &key, &pressed) == 0,
          "buffer empty after draining");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Test: properties write reaches world ───────────────────────────── */

static void test_properties_write_reaches_world(void) {
    kyWorld *w = make_world();
    kyEditorApp *app = ky_editor_app_create(w, KY_RENDERER_CONSOLE, NULL);
    make_default_scene(w, NULL, NULL, NULL);

    /* Pick the player entity (first one) and modify via editor */
    kyEntity player = ky_editor_app_get_entity(app, 0);
    CHECK(ky_entity_valid(w, player), "player entity valid");

    ky_editor_app_select(app, player);
    ky_editor_app_set_pos(app, 10.0f, 20.0f);
    ky_editor_app_set_scale(app, 0.5f, 0.75f);
    ky_editor_app_set_sprite_layer(app, 7);

    /* Verify by reading directly from the world */
    uint32_t ttid = ky_editor_transform_type_id(w);
    CHECK(ttid != (uint32_t)-1, "transform type found");
    kyTransform *tr = (kyTransform *)ky_world_get_component(w, player, ttid);
    CHECK(tr != NULL, "transform component exists");
    CHECK_F(tr->pos.x, 10.0f, 1e-5, "world pos_x after editor set");
    CHECK_F(tr->pos.y, 20.0f, 1e-5, "world pos_y after editor set");
    CHECK_F(tr->scale.x, 0.5f, 1e-5, "world scale_x after editor set");
    CHECK_F(tr->scale.y, 0.75f, 1e-5, "world scale_y after editor set");

    uint32_t stid = ky_editor_sprite_type_id(w);
    kySprite *sp = (kySprite *)ky_world_get_component(w, player, stid);
    CHECK(sp != NULL, "sprite component exists");
    CHECK(sp->layer == 7, "world sprite layer after editor set");

    ky_editor_app_destroy(app);
    ky_world_destroy(w);
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void) {
    /* One long-lived allocator for all worlds in this test */
    g_alloc = ky_default_allocator();

    test_create_destroy();
    test_null_safety();
    test_hierarchy();
    test_transform_props();
    test_sprite_layer();
    test_camera_zoom();
    test_spawn_despawn();
    test_render();
    test_input_poll();
    test_properties_write_reaches_world();

    printf("%d assertions, %d failures\n", assertions, failures);
    return failures > 0 ? 1 : 0;
}
