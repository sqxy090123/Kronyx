#ifndef KRONYX_INPUT_H
#define KRONYX_INPUT_H

#include "defines.h"

/* Input key codes (subset sufficient for a 2D platformer). */
typedef enum kyInputKey {
    KY_KEY_NONE   = 0,
    KY_KEY_SPACE  = 32,
    KY_KEY_APOSTROPHE,
    KY_KEY_COMMA,
    KY_KEY_MINUS,
    KY_KEY_PERIOD,
    KY_KEY_SLASH,
    KY_KEY_0, KY_KEY_1, KY_KEY_2, KY_KEY_3, KY_KEY_4,
    KY_KEY_5, KY_KEY_6, KY_KEY_7, KY_KEY_8, KY_KEY_9,
    KY_KEY_SEMICOLON,
    KY_KEY_EQUAL,
    KY_KEY_A, KY_KEY_B, KY_KEY_C, KY_KEY_D, KY_KEY_E,
    KY_KEY_F, KY_KEY_G, KY_KEY_H, KY_KEY_I, KY_KEY_J,
    KY_KEY_K, KY_KEY_L, KY_KEY_M, KY_KEY_N, KY_KEY_O,
    KY_KEY_P, KY_KEY_Q, KY_KEY_R, KY_KEY_S, KY_KEY_T,
    KY_KEY_U, KY_KEY_V, KY_KEY_W, KY_KEY_X, KY_KEY_Y,
    KY_KEY_Z,
    KY_KEY_LEFT_BRACKET,
    KY_KEY_BACKSLASH,
    KY_KEY_RIGHT_BRACKET,
    KY_KEY_GRAVE_ACCENT,
    KY_KEY_WORLD_1,
    KY_KEY_WORLD_2,
    KY_KEY_ESCAPE     = 256,
    KY_KEY_ENTER,
    KY_KEY_TAB,
    KY_KEY_BACKSPACE,
    KY_KEY_DELETE,
    KY_KEY_ARROW_LEFT,
    KY_KEY_ARROW_RIGHT,
    KY_KEY_ARROW_UP,
    KY_KEY_ARROW_DOWN,
    KY_KEY_LAST
} kyInputKey;

/* A single input event, produced by poll. */
typedef struct kyInputEvent {
    kyInputKey key;
    int pressed;  /* 1 = press, 0 = release */
    float x, y;   /* cursor position in screen space (placeholder) */
} kyInputEvent;

/* Simulate a key event from an external source (e.g. stdin for headless mode). */
KY_API void ky_input_simulate_key(kyInputKey key, int pressed);

/* Drain the oldest event from the internal ring buffer. Returns 1 on success,
 * 0 when the buffer is empty. */
KY_API int  ky_input_poll(kyInputEvent *out);

/* Discard all buffered events. Useful between game loop iterations. */
KY_API void ky_input_reset(void);

/* Replace the default ring-buffer event queue with a custom sink.
 * When set, simulate_key calls the handler instead of pushing to the ring;
 * poll() then draws from the handler-provided buffer (or returns 0 if the
 * handler does not populate one).  Pass NULL to restore the default ring
 * buffer.  The handler receives an array of events and their count. */
typedef void (*kyInputQueueFn)(void *user, const kyInputEvent *ev, int count);
KY_API void ky_input_set_queue_handler(kyInputQueueFn fn, void *user);

#endif
