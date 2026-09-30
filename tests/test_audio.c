#include "kronyx/kronyx.h"
#include "kronyx/audio.h"
#include "kronyx/ecs.h"
#include "kytest.h"
#include <stdint.h>
#include <math.h>

int ky_test_failures = 0;
int ky_test_assertions = 0;

/*
 * Test strategy for a process-global voice table + mix buffer:
 *
 * The voice table and mix buffer are process-wide statics (same pattern as
 * particle2d.c's g_pool / 2d.c's g_items).  Each simulation test calls
 * ky_audio_shutdown() at the start so it begins from a clean slate, and
 * assertions use relative counts (delta vs. a captured baseline) so any
 * residual state cannot leak into the next test.
 */

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static kyAudioClip *make_click(float duration, float amp, uint32_t seed) {
    kySfxParams p = ky_sfx_params_new(KY_SFX_CLICK, duration, 0.0f, 0.0f, amp);
    p.seed = seed;
    return ky_audio_synthesize(&p);
}

static kyAudioClip *make_sine(float dur, float f0, float f1, float amp) {
    kySfxParams p = ky_sfx_params_new(KY_SFX_SINE_SWEEP, dur, f0, f1, amp);
    return ky_audio_synthesize(&p);
}

/* ------------------------------------------------------------------ */
/* R1: synthesis                                                       */
/* ------------------------------------------------------------------ */

static void test_synth_valid(void) {
    ky_audio_shutdown();

    kyAudioClip *c1 = make_click(0.05f, 1.0f, 42u);
    KY_CHECK(c1 != NULL);
    KY_CHECK(c1->sample_count == (size_t)ceilf(0.05f * 44100.0f));

    kyAudioClip *c2 = make_sine(0.1f, 200.0f, 800.0f, 0.8f);
    KY_CHECK(c2 != NULL);
    KY_CHECK(c2->sample_count == (size_t)ceilf(0.1f * 44100.0f));

    kySfxParams sp = ky_sfx_params_new(KY_SFX_SQUARE, 0.03f, 440.0f, 0.0f, 1.0f);
    kyAudioClip *c3 = ky_audio_synthesize(&sp);
    KY_CHECK(c3 != NULL);
    KY_CHECK(c3->sample_count == (size_t)ceilf(0.03f * 44100.0f));

    kySfxParams np = ky_sfx_params_new(KY_SFX_NOISE, 0.06f, 0.0f, 0.0f, 1.0f);
    kyAudioClip *c4 = ky_audio_synthesize(&np);
    KY_CHECK(c4 != NULL);
    KY_CHECK(c4->sample_count == (size_t)ceilf(0.06f * 44100.0f));

    ky_audio_clip_free(c1);
    ky_audio_clip_free(c2);
    ky_audio_clip_free(c3);
    ky_audio_clip_free(c4);
    KY_CHECK(ky_audio_active_count() == 0);
}

static void test_synth_invalid(void) {
    ky_audio_shutdown();

    KY_CHECK(ky_audio_synthesize(NULL) == NULL);

    kySfxParams p0 = ky_sfx_params_new(KY_SFX_CLICK, 0.0f, 0, 0, 1.0f);
    KY_CHECK(ky_audio_synthesize(&p0) == NULL);
    p0.duration = -0.1f;
    KY_CHECK(ky_audio_synthesize(&p0) == NULL);
    p0.duration = 0.1f;
    p0.kind = (kySfxKind)99;
    KY_CHECK(ky_audio_synthesize(&p0) == NULL);

    KY_CHECK(ky_audio_active_count() == 0);
}

static void test_synth_deterministic(void) {
    ky_audio_shutdown();

    kySfxParams a = ky_sfx_params_new(KY_SFX_NOISE, 0.05f, 0.0f, 0.0f, 1.0f);
    a.seed = 777u;
    kyAudioClip *c1 = ky_audio_synthesize(&a);
    kyAudioClip *c2 = ky_audio_synthesize(&a);
    KY_CHECK(c1 != NULL);
    KY_CHECK(c2 != NULL);
    if (c1 && c2) {
        int same = 1;
        for (size_t i = 0; i < c1->sample_count; i++) {
            if (c1->samples[i] != c2->samples[i]) { same = 0; break; }
        }
        KY_CHECK(same);
    }
    ky_audio_clip_free(c1);
    ky_audio_clip_free(c2);
}

static void test_synth_clamped(void) {
    ky_audio_shutdown();

    kyAudioClip *c = make_click(0.1f, 1.0f, 42u);
    KY_CHECK(c != NULL);
    if (c) {
        int in_range = 1;
        for (size_t i = 0; i < c->sample_count; i++) {
            if (c->samples[i] < -1.0f || c->samples[i] > 1.0f) {
                in_range = 0; break;
            }
        }
        KY_CHECK(in_range);
    }

    kyAudioClip *s = make_sine(0.1f, 200.0f, 800.0f, 1.0f);
    KY_CHECK(s != NULL);
    if (s) {
        int in_range = 1;
        for (size_t i = 0; i < s->sample_count; i++) {
            if (s->samples[i] < -1.0f || s->samples[i] > 1.0f) {
                in_range = 0; break;
            }
        }
        KY_CHECK(in_range);
    }
    ky_audio_clip_free(c);
    ky_audio_clip_free(s);
}

/* ------------------------------------------------------------------ */
/* R2: play + mix                                                      */
/* ------------------------------------------------------------------ */

static void test_play_basic(void) {
    ky_audio_shutdown();
    kyAudioClip *c = make_click(0.05f, 1.0f, 42u);
    KY_CHECK(c != NULL);

    int n = ky_audio_play(c, 0.8f);
    KY_CHECK(n >= 1);
    KY_CHECK(ky_audio_active_count() == 1);

    int r1 = ky_audio_play(c, 1.5f);
    KY_CHECK(r1 < 0);
    int r2 = ky_audio_play(c, -0.1f);
    KY_CHECK(r2 < 0);
    int r3 = ky_audio_play(NULL, 0.5f);
    KY_CHECK(r3 < 0);
    KY_CHECK(ky_audio_active_count() == 1);

    ky_audio_clip_free(c);
    ky_audio_shutdown();
}

static void test_mix_zero(void) {
    ky_audio_shutdown();
    kyAudioClip *c = make_click(0.05f, 1.0f, 42u);
    KY_CHECK(c != NULL);
    ky_audio_play(c, 1.0f);
    KY_CHECK(ky_audio_active_count() == 1);

    int r = ky_audio_mix(0.0f);
    KY_CHECK(r == 0);
    KY_CHECK(ky_audio_active_count() == 1);

    r = ky_audio_mix(-0.001f);
    KY_CHECK(r == 0);

    ky_audio_clip_free(c);
    ky_audio_shutdown();
}

static void test_mix_finite(void) {
    ky_audio_shutdown();
    kyAudioClip *c = make_click(0.05f, 1.0f, 42u);
    KY_CHECK(c != NULL);
    ky_audio_play(c, 1.0f);

    int r = ky_audio_mix(0.05f);
    size_t expected = (size_t)ceilf(0.05f * 44100.0f);
    KY_CHECK((size_t)r == expected);
    KY_CHECK(ky_audio_mix_buffer_samples() == expected);
    KY_CHECK(ky_audio_active_count() == 0);

    float *buf = ky_audio_mix_buffer();
    KY_CHECK(buf != NULL);
    int any_nonzero = 0;
    for (size_t i = 0; i < expected; i++) {
        if (buf[i] != 0.0f) { any_nonzero = 1; break; }
    }
    KY_CHECK(any_nonzero);

    ky_audio_clip_free(c);
    ky_audio_shutdown();
}

static void test_voice_full(void) {
    ky_audio_shutdown();
    kyAudioClip *c = make_click(1.0f, 1.0f, 42u);
    KY_CHECK(c != NULL);

    int last = 0;
    for (int i = 0; i < KY_AUDIO_MAX_VOICES; i++) {
        int r = ky_audio_play(c, 0.5f);
        if (r >= 0) last = r;
    }
    KY_CHECK(last == KY_AUDIO_MAX_VOICES);
    KY_CHECK(ky_audio_active_count() == KY_AUDIO_MAX_VOICES);

    int r = ky_audio_play(c, 0.5f);
    KY_CHECK(r < 0);
    KY_CHECK(ky_audio_active_count() == KY_AUDIO_MAX_VOICES);

    ky_audio_clip_free(c);
    ky_audio_shutdown();
}

/* ------------------------------------------------------------------ */
/* R3: ECS integration                                                 */
/* ------------------------------------------------------------------ */

static kyWorld *g_world = NULL;
static uint32_t g_tid_sound = 0;
static kyAllocator g_alloc;

static int setup_audio_world(void) {
    g_alloc = ky_default_allocator();
    g_world = ky_world_create(&g_alloc);
    if (!g_world) return -1;
    if (ky_audio_register_components(g_world) != 0) return -2;
    g_tid_sound = ky_world_component_type_by_name(g_world, "sound");
    if (g_tid_sound == UINT32_MAX) return -3;
    return 0;
}

static void teardown_audio_world(void) {
    if (!g_world) return;
    int n = ky_world_alive_count(g_world);
    for (int i = n - 1; i >= 0; i--) {
        kyEntity e = ky_world_get_alive_entity(g_world, i);
        ky_world_despawn(g_world, e);
    }
    ky_world_destroy(g_world);
    g_world = NULL;
    ky_audio_shutdown();
}

static void test_ecs_register(void) {
    KY_CHECK(ky_audio_register_components(NULL) == -1);

    if (setup_audio_world() != 0) {
        KY_CHECK(0);
        return;
    }
    KY_CHECK(g_tid_sound != UINT32_MAX);
    KY_CHECK(ky_audio_register_components(g_world) == 0);
    teardown_audio_world();
}

static void test_ecs_trigger(void) {
    if (setup_audio_world() != 0) {
        KY_CHECK(0);
        return;
    }
    ky_audio_shutdown();

    kyAudioClip *c = make_click(0.05f, 1.0f, 42u);
    KY_CHECK(c != NULL);

    kyEntity e = ky_world_spawn(g_world);
    kySoundEmitter *se =
        (kySoundEmitter *)ky_world_add_component(g_world, e, g_tid_sound);
    KY_CHECK(se != NULL);
    if (se) {
        se->clip    = c;
        se->volume  = 0.7f;
        se->trigger = 1;
    }

    int before = ky_audio_active_count();
    ky_world_step(g_world, 1.0f / 60.0f);
    int after = ky_audio_active_count();
    KY_CHECK(after - before == 1);
    KY_CHECK(se && se->trigger == 0);

    ky_audio_clip_free(c);
    teardown_audio_world();
}

/* ------------------------------------------------------------------ */
/* R5: lifecycle                                                       */
/* ------------------------------------------------------------------ */

static void test_clip_free_null(void) {
    ky_audio_shutdown();
    ky_audio_clip_free(NULL);
    KY_CHECK(ky_audio_active_count() == 0);
}

static void test_early_free_voice_safe(void) {
    ky_audio_shutdown();
    kyAudioClip *c = make_click(1.0f, 1.0f, 42u);
    KY_CHECK(c != NULL);
    KY_CHECK(ky_audio_play(c, 0.5f) == 1);

    ky_audio_clip_free(c);

    int r = ky_audio_mix(0.01f);
    KY_CHECK(r >= 0);
    KY_CHECK(ky_audio_active_count() == 0);
    ky_audio_shutdown();
}

static void test_shutdown_resets(void) {
    ky_audio_shutdown();
    kyAudioClip *c = make_click(1.0f, 1.0f, 42u);
    KY_CHECK(c != NULL);
    for (int i = 0; i < 5; i++) ky_audio_play(c, 0.5f);
    KY_CHECK(ky_audio_active_count() == 5);

    ky_audio_clip_free(c);
    ky_audio_shutdown();
    KY_CHECK(ky_audio_active_count() == 0);
    KY_CHECK(ky_audio_mix_buffer_samples() == 0);

    ky_audio_shutdown();
    KY_CHECK(ky_audio_active_count() == 0);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

void ky_test_run_all(void) {
    test_synth_valid();
    test_synth_invalid();
    test_synth_deterministic();
    test_synth_clamped();
    test_play_basic();
    test_mix_zero();
    test_mix_finite();
    test_voice_full();
    test_ecs_register();
    test_ecs_trigger();
    test_clip_free_null();
    test_early_free_voice_safe();
    test_shutdown_resets();
}

KY_TEST_MAIN()
