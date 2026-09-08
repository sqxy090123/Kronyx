/* Headless tests for kyScene save/load round-trip.
 *
 * Tests:
 *   - empty scene save + load
 *   - default scene (camera + player + ground) round-trip
 *   - property values preserved after load
 *   - missing file returns error
 *   - malformed file returns error
 */

#include "kronyx/scene.h"
#include "kronyx/2d.h"
#include "kronyx/memory.h"
#include "kronyx/file.h"
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

static kyAllocator g_alloc;
static char g_buf[65536];  /* temp buffer for writing test .ksn files */

static void write_test_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    CHECK(f != NULL, "test file created");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

/* ── Test: empty scene ─────────────────────────────────────────────── */

static void test_empty(void) {
    g_alloc = ky_default_allocator();
    kyScene *s = ky_scene_create(&g_alloc, "empty");
    CHECK(s != NULL, "scene create");

    int r = ky_scene_save(s, "/tmp/ky_scene_empty.ksn");
    CHECK(r == 0, "save empty scene");

    kyScene *s2 = ky_scene_create(&g_alloc, "loaded");
    r = ky_scene_load(s2, "/tmp/ky_scene_empty.ksn");
    CHECK(r == 0, "load empty scene");
    CHECK(ky_world_alive_count(s2->world) == 0, "empty scene has 0 entities");

    ky_scene_destroy(s);
    ky_scene_destroy(s2);
}

/* ── Test: default scene round-trip ────────────────────────────────── */

static void test_default_scene(void) {
    g_alloc = ky_default_allocator();
    kyScene *s = ky_scene_create(&g_alloc, "default");
    CHECK(s != NULL, "scene create");

    /* Build a default 2D scene */
    int reg = ky2d_register_components(s->world);
    CHECK(reg == 0, "2d components registered");

    /* Camera entity */
    kyEntity cam = ky_world_spawn(s->world);
    ky_world_add_component(s->world, cam,
        ky_world_component_type_by_name(s->world, "transform"));
    ky_world_add_component(s->world, cam,
        ky_world_component_type_by_name(s->world, "camera2d"));
    kyTransform *ctr = (kyTransform *)ky_world_get_component(s->world, cam,
        ky_world_component_type_by_name(s->world, "transform"));
    kyCamera2D *ccam = (kyCamera2D *)ky_world_get_component(s->world, cam,
        ky_world_component_type_by_name(s->world, "camera2d"));
    ctr->pos.x = 0.0f;  ctr->pos.y = 0.0f;
    ctr->scale.x = 1.0f; ctr->scale.y = 1.0f;
    ccam->zoom = 1.5f;
    ccam->viewport.x = 800.0f;
    ccam->viewport.y = 600.0f;
    ccam->clear_color.x = 0.1f; ccam->clear_color.y = 0.2f;
    ccam->clear_color.z = 0.3f; ccam->clear_color.w = 1.0f;
    ccam->active = 1;

    /* Player entity */
    kyEntity pl = ky_world_spawn(s->world);
    ky_world_add_component(s->world, pl,
        ky_world_component_type_by_name(s->world, "transform"));
    ky_world_add_component(s->world, pl,
        ky_world_component_type_by_name(s->world, "sprite"));
    kyTransform *ptr = (kyTransform *)ky_world_get_component(s->world, pl,
        ky_world_component_type_by_name(s->world, "transform"));
    kySprite *psp = (kySprite *)ky_world_get_component(s->world, pl,
        ky_world_component_type_by_name(s->world, "sprite"));
    ptr->pos.x = 1.0f;  ptr->pos.y = 2.0f;
    ptr->scale.x = 2.0f; ptr->scale.y = 3.0f;
    ptr->rotation_z = 0.5f;
    psp->layer = 5;
    psp->flip = 1;

    /* Ground entity */
    kyEntity gd = ky_world_spawn(s->world);
    ky_world_add_component(s->world, gd,
        ky_world_component_type_by_name(s->world, "transform"));
    kyTransform *gtr = (kyTransform *)ky_world_get_component(s->world, gd,
        ky_world_component_type_by_name(s->world, "transform"));
    gtr->pos.x = 0.0f;  gtr->pos.y = -5.0f;
    gtr->scale.x = 10.0f; gtr->scale.y = 1.0f;

    CHECK(ky_world_alive_count(s->world) == 3, "world has 3 entities");

    /* Save */
    int r = ky_scene_save(s, "/tmp/ky_scene_default.ksn");
    CHECK(r == 0, "save default scene");

    /* Load into fresh scene */
    kyScene *s2 = ky_scene_create(&g_alloc, "loaded");
    CHECK(s2 != NULL, "loaded scene create");
    r = ky_scene_load(s2, "/tmp/ky_scene_default.ksn");
    CHECK(r == 0, "load default scene");
    CHECK(ky_world_alive_count(s2->world) == 3, "loaded scene has 3 entities");

    /* Verify camera properties */
    /* Find camera entity (has both transform and camera2d) */
    int cnt = ky_world_alive_count(s2->world);
    kyEntity found_cam = {0, 0};
    uint32_t ttid = ky_world_component_type_by_name(s2->world, "transform");
    uint32_t ctid = ky_world_component_type_by_name(s2->world, "camera2d");
    for (int i = 0; i < cnt; i++) {
        kyEntity e = ky_world_get_alive_entity(s2->world, i);
        if (ky_world_has_component(s2->world, e, ttid) &&
            ky_world_has_component(s2->world, e, ctid)) {
            found_cam = e;
            break;
        }
    }
    CHECK(ky_entity_valid(s2->world, found_cam), "camera entity found after load");

    kyTransform *ltr = (kyTransform *)ky_world_get_component(s2->world, found_cam, ttid);
    kyCamera2D *lcam = (kyCamera2D *)ky_world_get_component(s2->world, found_cam, ctid);
    CHECK(ltr != NULL, "camera transform exists");
    CHECK(lcam != NULL, "camera camera2d exists");
    if (ltr) {
        CHECK_F(ltr->pos.x, 0.0f, 1e-5, "cam pos.x == 0");
        CHECK_F(ltr->pos.y, 0.0f, 1e-5, "cam pos.y == 0");
        CHECK_F(ltr->scale.x, 1.0f, 1e-5, "cam scale.x == 1");
        CHECK_F(ltr->scale.y, 1.0f, 1e-5, "cam scale.y == 1");
    }
    if (lcam) {
        CHECK_F(lcam->zoom, 1.5f, 1e-5, "cam zoom == 1.5");
        CHECK_F(lcam->viewport.x, 800.0f, 1e-5, "cam viewport_w == 800");
        CHECK_F(lcam->viewport.y, 600.0f, 1e-5, "cam viewport_h == 600");
        CHECK_F(lcam->clear_color.x, 0.1f, 1e-5, "cam clear_r == 0.1");
        CHECK_F(lcam->clear_color.y, 0.2f, 1e-5, "cam clear_g == 0.2");
        CHECK_F(lcam->clear_color.z, 0.3f, 1e-5, "cam clear_b == 0.3");
        CHECK_F(lcam->clear_color.w, 1.0f, 1e-5, "cam clear_a == 1");
        CHECK(lcam->active == 1, "cam active == 1");
    }

    /* Verify player properties */
    kyEntity found_pl = {0, 0};
    for (int i = 0; i < cnt; i++) {
        kyEntity e = ky_world_get_alive_entity(s2->world, i);
        if (ky_world_has_component(s2->world, e, ttid) &&
            ky_world_has_component(s2->world, e,
                ky_world_component_type_by_name(s2->world, "sprite"))) {
            found_pl = e;
            break;
        }
    }
    CHECK(ky_entity_valid(s2->world, found_pl), "player entity found after load");

    kyTransform *ptr2 = (kyTransform *)ky_world_get_component(s2->world, found_pl, ttid);
    kySprite *psp2 = (kySprite *)ky_world_get_component(s2->world, found_pl,
        ky_world_component_type_by_name(s2->world, "sprite"));
    if (ptr2) {
        CHECK_F(ptr2->pos.x, 1.0f, 1e-5, "player pos.x == 1");
        CHECK_F(ptr2->pos.y, 2.0f, 1e-5, "player pos.y == 2");
        CHECK_F(ptr2->scale.x, 2.0f, 1e-5, "player scale.x == 2");
        CHECK_F(ptr2->scale.y, 3.0f, 1e-5, "player scale.y == 3");
        CHECK_F(ptr2->rotation_z, 0.5f, 1e-4, "player rotation_z == 0.5");
    }
    if (psp2) {
        CHECK(psp2->layer == 5, "player sprite layer == 5");
        CHECK(psp2->flip == 1, "player sprite flip == 1");
    }

    /* Verify ground properties */
    kyEntity found_gd = {0, 0};
    for (int i = 0; i < cnt; i++) {
        kyEntity e = ky_world_get_alive_entity(s2->world, i);
        if (ky_entity_valid(s2->world, e) &&
            ky_world_has_component(s2->world, e, ttid) &&
            !ky_world_has_component(s2->world, e,
                ky_world_component_type_by_name(s2->world, "sprite")) &&
            !ky_world_has_component(s2->world, e,
                ky_world_component_type_by_name(s2->world, "camera2d"))) {
            found_gd = e;
            break;
        }
    }
    /* Alternative: find entity with only transform */
    if (!ky_entity_valid(s2->world, found_gd)) {
        for (int i = 0; i < cnt; i++) {
            kyEntity e = ky_world_get_alive_entity(s2->world, i);
            if (!ky_entity_valid(s2->world, e)) continue;
            int has_tr = ky_world_has_component(s2->world, e, ttid);
            int has_sp = ky_world_has_component(s2->world, e,
                ky_world_component_type_by_name(s2->world, "sprite"));
            int has_cam = ky_world_has_component(s2->world, e,
                ky_world_component_type_by_name(s2->world, "camera2d"));
            if (has_tr && !has_sp && !has_cam) {
                found_gd = e;
                break;
            }
        }
    }
    CHECK(ky_entity_valid(s2->world, found_gd), "ground entity found after load");
    if (ky_entity_valid(s2->world, found_gd)) {
        kyTransform *gtr2 = (kyTransform *)ky_world_get_component(s2->world, found_gd, ttid);
        if (gtr2) {
            CHECK_F(gtr2->pos.x, 0.0f, 1e-5, "ground pos.x == 0");
            CHECK_F(gtr2->pos.y, -5.0f, 1e-5, "ground pos.y == -5");
            CHECK_F(gtr2->scale.x, 10.0f, 1e-5, "ground scale.x == 10");
        }
    }

    ky_scene_destroy(s);
    ky_scene_destroy(s2);
}

/* ── Test: missing file ────────────────────────────────────────────── */

static void test_missing_file(void) {
    g_alloc = ky_default_allocator();
    kyScene *s = ky_scene_create(&g_alloc, "x");
    int r = ky_scene_load(s, "/tmp/nonexistent_ky_scene_xyz.ksn");
    CHECK(r < 0, "load missing file returns error");
    ky_scene_destroy(s);
}

/* ── Test: malformed file ──────────────────────────────────────────── */

static void test_malformed(void) {
    g_alloc = ky_default_allocator();
    /* Entity without transform is an error */
    write_test_file("/tmp/ky_scene_bad.ksn",
        "scene \"bad\" v1\n"
        "\n"
        "entity\n"
        "  sprite layer=\"5\"\n"
        "end\n");

    kyScene *s = ky_scene_create(&g_alloc, "bad");
    int r = ky_scene_load(s, "/tmp/ky_scene_bad.ksn");
    CHECK(r < 0, "load malformed scene (no transform) returns error");
    ky_scene_destroy(s);
}

/* ── Test: save then cat file ──────────────────────────────────────── */

static void test_save_readable(void) {
    g_alloc = ky_default_allocator();
    kyScene *s = ky_scene_create(&g_alloc, "readable");
    CHECK(s != NULL, "scene create");

    /* Create a minimal scene */
    ky2d_register_components(s->world);
    kyEntity e = ky_world_spawn(s->world);
    uint32_t ttid = ky_world_component_type_by_name(s->world, "transform");
    uint32_t ctid = ky_world_component_type_by_name(s->world, "camera2d");
    ky_world_add_component(s->world, e, ttid);
    ky_world_add_component(s->world, e, ctid);
    kyTransform *tr = (kyTransform *)ky_world_get_component(s->world, e, ttid);
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(s->world, e, ctid);
    tr->pos.x = 10.0f; tr->pos.y = 20.0f;
    cam->zoom = 2.0f; cam->viewport.x = 640.0f; cam->viewport.y = 480.0f;

    int r = ky_scene_save(s, "/tmp/ky_scene_readable.ksn");
    CHECK(r == 0, "save readable scene");

    /* Read back the raw file content */
    void *raw = NULL;
    size_t raw_len = 0;
    r = ky_file_read("/tmp/ky_scene_readable.ksn", &raw, &raw_len);
    CHECK(r == KY_FILE_OK, "read saved file");
    CHECK(raw != NULL, "file content non-null");
    if (raw && raw_len > 0) {
        /* Must contain expected tokens */
        char *text = (char *)raw;
        CHECK(strstr(text, "scene") != NULL, "file contains 'scene'");
        CHECK(strstr(text, "transform") != NULL, "file contains 'transform'");
        CHECK(strstr(text, "camera2d") != NULL, "file contains 'camera2d'");
        CHECK(strstr(text, "zoom") != NULL, "file contains 'zoom'");
        CHECK(strstr(text, "end") != NULL, "file contains 'end'");
    }
    free(raw);

    ky_scene_destroy(s);
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void) {
    test_empty();
    test_default_scene();
    test_missing_file();
    test_malformed();
    test_save_readable();

    printf("%d assertions, %d failures\n", assertions, failures);
    return failures > 0 ? 1 : 0;
}
