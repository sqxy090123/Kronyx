#include "kronyx/audio.h"
#include "kronyx/ecs.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* LCG (xorshift32, same family as particle2d.c)                       */
/* ------------------------------------------------------------------ */

static uint32_t lcg_next(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0u) x = 0x12345678u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float lcg_unit(uint32_t *state) {
    return (float)(lcg_next(state) & 0xFFFFFF) / (float)0x1000000u;
}

/* ------------------------------------------------------------------ */
/* Voice table (process-global static, same pattern as particle2d)     */
/* ------------------------------------------------------------------ */

typedef struct AudioVoice {
    kyAudioClip *clip;
    float        vol;
    size_t       idx;
    int          used;
} AudioVoice;

static AudioVoice g_voices[KY_AUDIO_MAX_VOICES];
static int        g_voice_count = 0;

/* Mix buffer (null sink). */
static float  g_mix[KY_AUDIO_MIX_MAX];
static size_t g_mix_samples = 0;

/*
 * Freed-clip tracker.
 *
 * A voice may outlive its clip (the host calls ky_audio_clip_free while a
 * voice still references it).  We must not dereference the freed kyAudioClip,
 * so liveness is tracked in module-owned memory here rather than in
 * clip->alive.  Only clips that are BOTH freed AND still held by at least
 * one live voice are tracked, so the set stays bounded by the voice count
 * (<= KY_AUDIO_MAX_VOICES).  Entries are dropped when the last voice that
 * referenced the clip retires.
 */
static kyAudioClip *g_dead_clips[KY_AUDIO_MAX_VOICES];
static int          g_dead_count = 0;

static int clip_is_dead(kyAudioClip *c) {
    for (int i = 0; i < g_dead_count; i++) {
        if (g_dead_clips[i] == c) return 1;
    }
    return 0;
}

static void track_dead(kyAudioClip *c) {
    if (!c || clip_is_dead(c)) return;
    if (g_dead_count < KY_AUDIO_MAX_VOICES) g_dead_clips[g_dead_count++] = c;
}

/* ------------------------------------------------------------------ */
/* kySfxParams default                                                  */
/* ------------------------------------------------------------------ */

kySfxParams ky_sfx_params_new(kySfxKind kind, float duration,
                               float freq0, float freq1, float amplitude) {
    kySfxParams p;
    memset(&p, 0, sizeof(p));
    p.kind      = kind;
    p.duration  = duration;
    p.freq0     = freq0;
    p.freq1     = freq1;
    p.amplitude = amplitude;
    p.seed      = 0u;
    return p;
}

/* ------------------------------------------------------------------ */
/* Synthesis                                                            */
/* ------------------------------------------------------------------ */

static float clamp_f(float v) {
    if (v < -1.0f) return -1.0f;
    if (v >  1.0f) return  1.0f;
    return v;
}

kyAudioClip *ky_audio_synthesize(const kySfxParams *p) {
    if (!p) return NULL;
    if (p->duration <= 0.0f) return NULL;
    if (p->kind < KY_SFX_SINE_SWEEP || p->kind > KY_SFX_CLICK) return NULL;

    size_t n = (size_t)ceilf(p->duration * (float)KY_AUDIO_SAMPLE_RATE);
    if (n == 0) n = 1;

    kyAudioClip *clip = (kyAudioClip *)malloc(sizeof(kyAudioClip));
    if (!clip) return NULL;
    clip->samples = (float *)malloc(n * sizeof(float));
    if (!clip->samples) {
        free(clip);
        return NULL;
    }
    clip->sample_count = n;
    clip->alive        = 1;

    float amp = p->amplitude;
    if (amp < 0.0f) amp = 0.0f;
    if (amp > 1.0f) amp = 1.0f;

    float f0 = p->freq0;
    float f1 = (p->kind == KY_SFX_SINE_SWEEP) ? p->freq1 : p->freq0;
    if (f0 < 0.0f) f0 = 0.0f;
    if (f1 < 0.0f) f1 = 0.0f;
    float sweep = (f1 - f0) / (float)n;

    switch (p->kind) {
    case KY_SFX_SINE_SWEEP: {
        double phase = 0.0;
        for (size_t i = 0; i < n; i++) {
            float freq = f0 + sweep * (float)i;
            phase += 2.0 * M_PI * (double)freq / (double)KY_AUDIO_SAMPLE_RATE;
            float fade = 1.0f;
            /* 5% fade-in / fade-out to avoid clicks */
            float fi = 0.05f * (float)n;
            float fo = 0.05f * (float)n;
            if ((float)i < fi) fade *= (float)i / fi;
            if ((float)(n - 1 - i) < fo) fade *= (float)(n - 1 - i) / fo;
            clip->samples[i] = clamp_f((float)sin(phase) * amp * fade);
        }
        break;
    }
    case KY_SFX_SQUARE: {
        double phase = 0.0;
        for (size_t i = 0; i < n; i++) {
            phase += 2.0 * M_PI * (double)f0 / (double)KY_AUDIO_SAMPLE_RATE;
            float v = (sin(phase) >= 0.0) ? 1.0f : -1.0f;
            clip->samples[i] = clamp_f(v * amp * 0.5f);
        }
        break;
    }
    case KY_SFX_NOISE: {
        uint32_t st = p->seed ? p->seed : 0x12345678u;
        for (size_t i = 0; i < n; i++) {
            float v = lcg_unit(&st) * 2.0f - 1.0f;  /* [-1, 1) */
            /* linear decay envelope */
            float env = 1.0f - (float)i / (float)n;
            clip->samples[i] = clamp_f(v * amp * env);
        }
        break;
    }
    case KY_SFX_CLICK: {
        for (size_t i = 0; i < n; i++) {
            float t = (float)i / (float)n;
            float env = (1.0f - t) * (1.0f - t);  /* quadratic decay */
            clip->samples[i] = clamp_f(amp * env * (i == 0 ? 1.0f : -1.0f));
        }
        break;
    }
    }

    return clip;
}

void ky_audio_clip_free(kyAudioClip *clip) {
    if (!clip) return;
    clip->alive = 0;
    /* Mark in the module-owned tracker so in-flight voices can detect the
     * freed clip without dereferencing it.  The entry is dropped later when
     * the last referencing voice retires (see ky_audio_mix / shutdown). */
    track_dead(clip);
    free(clip->samples);
    free(clip);
}

/* ------------------------------------------------------------------ */
/* Voice / play / mix                                                   */
/* ------------------------------------------------------------------ */

int ky_audio_active_count(void) { return g_voice_count; }

int ky_audio_play(kyAudioClip *clip, float volume) {
    if (!clip) return -1;
    if (volume < 0.0f || volume > 1.0f) return -1;

    for (int i = 0; i < KY_AUDIO_MAX_VOICES; i++) {
        if (!g_voices[i].used) {
            g_voices[i].clip = clip;
            g_voices[i].vol  = volume;
            g_voices[i].idx  = 0;
            g_voices[i].used = 1;
            g_voice_count++;
            return g_voice_count;
        }
    }
    return -1; /* full: silent drop */
}

int ky_audio_mix(float dt) {
    if (dt <= 0.0f) {
        g_mix_samples = 0;
        return 0;
    }

    size_t frames = (size_t)ceilf(dt * (float)KY_AUDIO_SAMPLE_RATE);
    if (frames > KY_AUDIO_MIX_MAX) frames = KY_AUDIO_MIX_MAX;

    memset(g_mix, 0, frames * sizeof(float));

    for (int i = 0; i < KY_AUDIO_MAX_VOICES; i++) {
        AudioVoice *v = &g_voices[i];
        if (!v->used) continue;

        /* clip may have been freed by the host; the module-owned dead set
         * tells us without dereferencing the freed pointer */
        if (clip_is_dead(v->clip)) {
            v->used = 0;
            g_voice_count--;
            continue; /* entry freed by the final retiree below */
        }

        size_t clip_end = v->clip->sample_count;
        for (size_t f = 0; f < frames; f++) {
            size_t idx = v->idx + f;
            if (idx >= clip_end) break;
            g_mix[f] += v->clip->samples[idx] * v->vol;
        }
        v->idx += frames;

        /* Retire voice if it has advanced past the end of its clip
         * (covers both "finished this frame" and "was already exhausted
         *  from a prior frame"). */
        if (v->idx >= clip_end) {
            v->used = 0;
            g_voice_count--;
        }
    }

    /* clamp mix output to [-1, 1] */
    for (size_t i = 0; i < frames; i++) {
        g_mix[i] = clamp_f(g_mix[i]);
    }

    g_mix_samples = frames;
    return (int)frames;
}

float *ky_audio_mix_buffer(void) { return g_mix; }
size_t ky_audio_mix_buffer_samples(void) { return g_mix_samples; }

void ky_audio_shutdown(void) {
    for (int i = 0; i < KY_AUDIO_MAX_VOICES; i++) {
        g_voices[i].used = 0;
    }
    g_voice_count = 0;
    g_mix_samples = 0;
    memset(g_mix, 0, sizeof(g_mix));
    g_dead_count = 0;
}

/* ------------------------------------------------------------------ */
/* ECS integration: "sound" component + "audio-update" system           */
/* ------------------------------------------------------------------ */

static int component_registered(const kyWorld *w, const char *name) {
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *t =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (t && t->name && strcmp(t->name, name) == 0) return 1;
    }
    return 0;
}

static int system_exists(const kyWorld *w, const char *name) {
    for (size_t i = 0; i < w->systems.len; i++) {
        const kySystem *s = (const kySystem *)ky_array_get(&w->systems, i);
        if (s && s->name && strcmp(s->name, name) == 0) return 1;
    }
    return 0;
}

static void sound_ctor(void *c) {
    *(kySoundEmitter *)c = (kySoundEmitter){NULL, 1.0f, 0};
}

static void sound_dtor(void *c) {
    KY_UNUSED(c);
}

static void audio_update(kyWorld *w, float dt, void *user) {
    KY_UNUSED(user);
    if (dt <= 0.0f) return;

    uint32_t tid_sound = ky_world_component_type_by_name(w, "sound");
    if (tid_sound == UINT32_MAX) return;

    const uint32_t types[1] = { tid_sound };
    kyViewIter it;
    int more = ky_view_begin(w, types, 1, &it);
    while (more) {
        kyEntity e = it.current;
        kySoundEmitter *se =
            (kySoundEmitter *)ky_world_get_component(w, e, tid_sound);
        if (se && se->trigger) {
            ky_audio_play(se->clip, se->volume);
            se->trigger = 0;
        }
        more = ky_view_next(&it);
    }
}

int ky_audio_register_components(kyWorld *w) {
    if (!w) return -1;

    if (!component_registered(w, "sound")) {
        kyComponentType ct;
        memset(&ct, 0, sizeof(ct));
        ct.name = "sound";
        ct.size = sizeof(kySoundEmitter);
        ct.ctor = sound_ctor;
        ct.dtor = sound_dtor;
        uint32_t id = ky_world_register_component(w, &ct);
        if (id == UINT32_MAX) return -2;
    }

    if (!system_exists(w, "audio-update")) {
        kySystem sys;
        memset(&sys, 0, sizeof(sys));
        sys.name   = "audio-update";
        sys.order  = 0;
        sys.update = audio_update;
        sys.user   = NULL;
        ky_world_register_system(w, &sys);
    }

    return 0;
}
