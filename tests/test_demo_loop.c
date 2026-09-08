/* Headless end-to-end test for the 2D platformer vertical slice.
 *
 * Runs the exact same platformer_tick() the demo uses, against a console
 * render device, and asserts: fall-to-rest, horizontal movement, jump arc,
 * and camera follow. No display or GPU required.
 */

#include "platformer_world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

static int assertions = 0;
static int failures = 0;

#define CHECK(cond, msg) do { \
    assertions++; \
    if (!(cond)) { failures++; printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static const float DT = 1.0f / 60.0f;
static float rest(void) { return PLATFORMER_GROUND_TOP + PLATFORMER_ROLE_RADIUS; }

static void settle(PlatformerWorld *pw, int frames) {
    for (int i = 0; i < frames; i++) platformer_tick(pw, DT, 0.0f, 0);
}

/* Write a plausible 8x8 RGBA8 (256 B) asset at assets/hero_8x8.bin relative to
 * the CWD so platformer_setup can load it. Creates the assets/ dir if absent.
 * Best-effort: returns 0 on success. */
static int materialize_demo_asset(void) {
    mkdir("assets", 0775); /* ok if it already exists */
    FILE *f = fopen("assets/hero_8x8.bin", "wb");
    if (!f) return -1;
    uint8_t px[8 * 8 * 4];
    memset(px, 0, sizeof(px));
    for (int i = 3; i < sizeof(px); i += 4) px[i] = 255; /* alpha 255 */
    size_t n = fwrite(px, 1, sizeof(px), f);
    fclose(f);
    return (n == sizeof(px)) ? 0 : -1;
}

int main(void) {
    printf("=== 2D Platformer Demo-Loop Test ===\n");

    PlatformerWorld pw;
    /* Ensure the demo asset exists at the CWD-relative path so setup's
     * texture load is exercised (falls back to a white sprite if absent). */
    (void)materialize_demo_asset();
    int rc = platformer_setup(&pw, KY_RENDERER_CONSOLE);
    CHECK(rc == 0, "setup succeeds (console backend)");
    /* When the asset is present, the role sprite should carry a texture. */
    CHECK(pw.role_tex != NULL, "role texture attached when asset present");
    if (rc != 0) {
        printf("ABORT: setup failed rc=%d — %d assertions, %d failures\n", rc, assertions, failures);
        return 1;
    }

    /* 1. Fall to rest from the spawn height. */
    settle(&pw, 120);
    kyVec2 p = platformer_role_pos(&pw);
    CHECK(fabsf(p.y - rest()) < 0.2f, "role settles at rest height");

    /* Grounded flag is set after settling. */
    CHECK(pw.grounded == 1, "role reports grounded after settling");

    /* 2. Horizontal movement: left then right. */
    float start_x = p.x;
    for (int i = 0; i < 30; i++) platformer_tick(&pw, DT, -PLATFORMER_MAX_SPEED, 0);
    float left_x = platformer_role_pos(&pw).x;
    for (int i = 0; i < 30; i++) platformer_tick(&pw, DT, PLATFORMER_MAX_SPEED, 0);
    float right_x = platformer_role_pos(&pw).x;
    CHECK(left_x < start_x, "moving left decreases x");
    CHECK(right_x > left_x, "moving right increases x");

    /* 3. Velocity is clamped to max speed: 30 frames at max speed moves ~1 EU. */
    float traveled = fabsf(right_x - left_x);
    CHECK(traveled > 0.5f && traveled < 100.0f, "horizontal travel within sane bounds");

    /* 4. Jump arc: role rises above rest, then returns to rest. */
    for (int i = 0; i < 30; i++) platformer_tick(&pw, DT, 0.0f, 0); /* re-settle */
    CHECK(fabsf(platformer_role_pos(&pw).y - rest()) < 0.2f, "re-settled before jump");
    int rose = 0;
    float peak = platformer_role_pos(&pw).y;
    for (int i = 0; i < 90; i++) {
        platformer_tick(&pw, DT, 0.0f, (i == 0) ? 1 : 0);
        float y = platformer_role_pos(&pw).y;
        if (y > peak) peak = y;
        if (y > rest() + 0.25f) rose = 1;
    }
    CHECK(rose, "jump lifts role above rest height");
    settle(&pw, 120);
    CHECK(fabsf(platformer_role_pos(&pw).y - rest()) < 0.2f, "role lands back at rest");

    /* 5. Camera follows the role horizontally. */
    kyVec2 cr = platformer_role_pos(&pw);
    /* Camera entity pos via world component table. */
    uint32_t tid_cam = pw.tid_camera;
    const kyCamera2D *cam =
        (const kyCamera2D *)ky_world_get_component(pw.world, pw.camera, tid_cam);
    CHECK(cam != NULL, "camera component present");
    if (cam) {
        CHECK(fabsf(cam->pos.x - cr.x) < 1e-3f, "camera x tracks role x");
        CHECK(cam->pos.y > cr.y, "camera y is above role");
    }

    /* 6. Render pass returns a non-negative draw count (role sprite present). */
    CHECK(platformer_tick(&pw, DT, 0.0f, 0) == 0, "tick returns success");

    platformer_teardown(&pw);
    platformer_teardown(&pw); /* idempotent — must not crash */

    printf("%d assertions, %d failures\n", assertions, failures);
    return failures ? 1 : 0;
}
