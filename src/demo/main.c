/* Demo: 2D platformer vertical slice.
 *
 * Replaces the old raw-GL rotating square. Uses the ECS world +
 * Transform/Sprite/Camera2D + sprite-batch pipeline + AABB physics.
 *
 * Two entry paths:
 *   - headed : GLFW available -> ky_engine_run(update, render) game loop
 *   - headless: no display    -> N-frame smoke through the same platformer_tick
 *
 * Anti-tamper verification runs first and is the gate to both paths.
 */

#include "kronyx/engine.h"
#include "kronyx/input.h"
#include "kronyx/render.h"
#include "platformer_world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GLFW key codes (subset) — values from GLFW 3.x */
#define KY_GLFW_KEY_A      65
#define KY_GLFW_KEY_D      68
#define KY_GLFW_KEY_W      87
#define KY_GLFW_KEY_SPACE  32
#define KY_GLFW_KEY_LEFT   263
#define KY_GLFW_KEY_RIGHT  265
#define KY_GLFW_KEY_UP     266
#define KY_GLFW_KEY_ESCAPE 256

static void *g_demo_pw = NULL;

/* ------------------------------------------------------------------ */
/* Headed path                                                         */
/* ------------------------------------------------------------------ */

static float demo_update(float dt) {
    PlatformerWorld *pw = (PlatformerWorld *)g_demo_pw;
    if (!pw) return dt;

    float vx = 0.0f;
    int jump = 0;

    if (ky_engine_key_pressed(KY_GLFW_KEY_A) || ky_engine_key_pressed(KY_GLFW_KEY_LEFT))
        vx -= PLATFORMER_MAX_SPEED;
    if (ky_engine_key_pressed(KY_GLFW_KEY_D) || ky_engine_key_pressed(KY_GLFW_KEY_RIGHT))
        vx += PLATFORMER_MAX_SPEED;
    if (ky_engine_key_pressed(KY_GLFW_KEY_SPACE) ||
        ky_engine_key_pressed(KY_GLFW_KEY_W) ||
        ky_engine_key_pressed(KY_GLFW_KEY_UP))
        jump = 1;

    (void)platformer_tick(pw, dt, vx, jump);
    return dt;
}

static void demo_render(void) {
    /* Rendering is already done inside platformer_tick; nothing to add. */
}

/* ------------------------------------------------------------------ */
/* Headless path                                                       */
/* ------------------------------------------------------------------ */

static int run_headless_smoke(PlatformerWorld *pw) {
    const float dt = 1.0f / 60.0f;
    float rest = PLATFORMER_GROUND_TOP + PLATFORMER_ROLE_RADIUS;

    /* Fall to ground (no input). */
    for (int i = 0; i < 60; i++) platformer_tick(pw, dt, 0.0f, 0);
    kyVec2 p0 = platformer_role_pos(pw);
    if (fabsf(p0.y - rest) > 0.2f) {
        fprintf(stderr, "headless: role not at rest (y=%.3f, rest=%.3f)\n", p0.y, rest);
        return 1;
    }

    /* Move left, then right. */
    float start_x = p0.x;
    for (int i = 0; i < 30; i++) platformer_tick(pw, dt, -PLATFORMER_MAX_SPEED, 0);
    float left_x = platformer_role_pos(pw).x;
    for (int i = 0; i < 30; i++) platformer_tick(pw, dt,  PLATFORMER_MAX_SPEED, 0);
    float right_x = platformer_role_pos(pw).x;
    if (!(left_x < start_x && right_x > left_x)) {
        fprintf(stderr, "headless: horizontal movement wrong (start=%.2f left=%.2f right=%.2f)\n",
                start_x, left_x, right_x);
        return 1;
    }

    /* Jump: role must rise above rest. */
    int rose = 0;
    for (int i = 0; i < 60; i++) {
        platformer_tick(pw, dt, 0.0f, (i == 0) ? 1 : 0);
        if (platformer_role_pos(pw).y > rest + 0.25f) { rose = 1; break; }
    }
    if (!rose) {
        fprintf(stderr, "headless: jump did not lift role\n");
        return 1;
    }

    kyVec2 p_end = platformer_role_pos(pw);
    printf("Headless smoke OK (role.x=%.2f, role.y=%.2f)\n", p_end.x, p_end.y);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    KyTamperResult res = ky_tamper_init(KY_TAMPER_MODE_ERROR, 64u * 1024u * 1024u);
    if (res != KY_TAMPER_OK) {
        fprintf(stderr, "Tamper verification failed: %d\n", (int)res);
        return 1;
    }
    printf("Anti-tamper verification passed.\n");

    /* Prefer the GPU (pbuffer) backend; fall back to the console backend when
     * EGL is unavailable. Render and tick logic are identical either way. */
    PlatformerWorld pw;
    memset(&pw, 0, sizeof(pw));
    int setup_ok = (platformer_setup(&pw, KY_RENDERER_GL) == 0);
    if (!setup_ok)
        setup_ok = (platformer_setup(&pw, KY_RENDERER_CONSOLE) == 0);
    if (!setup_ok) {
        fprintf(stderr, "platformer_setup failed (gl and console both failed)\n");
        ky_tamper_shutdown();
        return 1;
    }
    printf("Render backend: %s\n", ky_rd_backend_name(pw.rd));
    g_demo_pw = &pw;

    int headed_ok = (ky_engine_init(800, 600, "Kronyx — 2D platformer") == 0);
    int rc = 0;
    if (headed_ok) {
        /* Headed loop uses the same platformer_tick; demo_update reads GLFW keys. */
        printf("Running. A/D or arrows to move, SPACE/W to jump.\n");
        ky_engine_run(demo_update, demo_render);
        ky_engine_shutdown();
    } else {
        printf("No display; running headless smoke.\n");
        rc = run_headless_smoke(&pw);
    }

    platformer_teardown(&pw);
    g_demo_pw = NULL;
    ky_tamper_shutdown();
    printf("Done.\n");
    return rc;
}
