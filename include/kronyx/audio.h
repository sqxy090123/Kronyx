#ifndef KRONYX_AUDIO_H
#define KRONYX_AUDIO_H

#include "ecs.h"
#include "memory.h"

/*
 * Audio module (G11): a pure-C PCM SFX synthesizer + mixer with a null
 * output sink.  No system audio library is required, matching the
 * "zero-dependency console-backend" strategy used for rendering.
 *
 * Conventions:
 *   - All waveforms are mono float32, sample rate KY_AUDIO_SAMPLE_RATE.
 *   - Clips are allocated by ky_audio_synthesize() and released by
 *     ky_audio_clip_free(); a clip in use by a voice is safe to free early
 *     because the voice checks clip->alive before touching samples.
 *   - The voice table and mix buffer are process-global statics (same
 *     pattern as particle2d.c / 2d.c's g_items).
 *   - The ECS "sound" component + "audio-update" system auto-play
 *     kySoundEmitter.trigger on world step.
 *
 * Usage pattern:
 *   kyAudioClip *c = ky_audio_synthesize(&p);
 *   ky_audio_play(c, 0.6f);
 *   for (...) ky_audio_mix(dt);   // or auto via world step
 *   ky_audio_clip_free(c);
 */

#define KY_AUDIO_SAMPLE_RATE 44100
#define KY_AUDIO_MAX_VOICES  32
#define KY_AUDIO_MIX_MAX     (44100 * 2)

typedef enum kySfxKind {
    KY_SFX_SINE_SWEEP = 0, /* sine sweep freq0 -> freq1 over duration */
    KY_SFX_SQUARE,         /* square wave, tone at freq0 */
    KY_SFX_NOISE,          /* LCG white-noise burst, seedable */
    KY_SFX_CLICK          /* short decaying impulse */
} kySfxKind;

typedef struct kySfxParams {
    kySfxKind kind;
    float    duration;   /* seconds, > 0 */
    float    freq0;      /* Hz */
    float    freq1;      /* Hz, sweep end (ignored by SQUARE/NOISE/CLICK) */
    float    amplitude;  /* 0..1 */
    uint32_t seed;       /* noise reproducibility; 0 -> 0x12345678 */
} kySfxParams;

typedef struct kyAudioClip {
    float  *samples;      /* length == sample_count */
    size_t  sample_count;
    uint32_t alive;       /* cleared by ky_audio_clip_free; voices skip when 0 */
} kyAudioClip;

/* ECS component: when trigger is set to 1 in a frame, the audio-update
 * system plays clip once (or the built-in click if clip == NULL) and
 * clears trigger.  volume is 0..1. */
typedef struct kySoundEmitter {
    kyAudioClip *clip;   /* NULL -> built-in click */
    float        volume;
    int          trigger;
} kySoundEmitter;

/* Default SFX parameters (valid, zero-amplitude-safe). */
KY_API kySfxParams ky_sfx_params_new(kySfxKind kind, float duration,
                                      float freq0, float freq1, float amplitude);

/* Synthesize a clip from params.  Returns NULL if p is NULL, duration <= 0,
 * or kind is out of range.  All samples are clamped to [-1.0, 1.0]. */
KY_API kyAudioClip *ky_audio_synthesize(const kySfxParams *p);

/* Free a clip.  Safe to call with NULL.  In-flight voices will safely
 * skip the clip on the next mix (alive flag). */
KY_API void ky_audio_clip_free(kyAudioClip *clip);

/* Queue a voice to play clip at volume (0..1).  Returns the new active
 * voice count on success, -1 if clip is NULL, volume out of range, or
 * the voice table is full (silent drop, no error side-effect). */
KY_API int ky_audio_play(kyAudioClip *clip, float volume);

/* Advance all voices by dt seconds, mixing into the process mix buffer.
 * Returns the number of samples written.  dt <= 0 returns 0 and does
 * nothing. */
KY_API int ky_audio_mix(float dt);

/* Pointer to the process mix buffer (float32 mono). */
KY_API float *ky_audio_mix_buffer(void);

/* Number of samples written by the most recent ky_audio_mix call. */
KY_API size_t ky_audio_mix_buffer_samples(void);

/* Current number of active voices. */
KY_API int ky_audio_active_count(void);

/* Reclaim all voices and reset the mix buffer.  Idempotent.
 * After this call ky_audio_active_count() == 0. */
KY_API void ky_audio_shutdown(void);

/* Register the "sound" component and "audio-update" system into world.
 * Idempotent.  Returns 0 on success, -1 if world is NULL. */
KY_API int ky_audio_register_components(kyWorld *w);

#endif
