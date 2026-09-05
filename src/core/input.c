#include "kronyx/input.h"
#include <string.h>

#define KY_INPUT_RING 256

typedef struct kyInputRing {
    kyInputEvent data[KY_INPUT_RING];
    int head;
    int tail;
    int count;
} kyInputRing;

static kyInputRing g_ring = {{{0}}, 0, 0, 0};
static kyInputQueueFn g_queue_fn = NULL;
static void *g_queue_user = NULL;

static int ring_push(const kyInputEvent *e) {
    if (g_ring.count >= KY_INPUT_RING) return 0;
    g_ring.data[g_ring.tail] = *e;
    g_ring.tail = (g_ring.tail + 1) % KY_INPUT_RING;
    g_ring.count++;
    return 1;
}

void ky_input_set_queue_handler(kyInputQueueFn fn, void *user) {
    g_queue_fn = fn;
    g_queue_user = user;
}

void ky_input_simulate_key(kyInputKey key, int pressed) {
    kyInputEvent ev;
    ev.key = key;
    ev.pressed = pressed ? 1 : 0;
    ev.x = 0.0f;
    ev.y = 0.0f;
    if (g_queue_fn) {
        g_queue_fn(g_queue_user, &ev, 1);
    } else {
        ring_push(&ev);
    }
}

int ky_input_poll(kyInputEvent *out) {
    if (!out || g_ring.count <= 0) return 0;
    *out = g_ring.data[g_ring.head];
    g_ring.head = (g_ring.head + 1) % KY_INPUT_RING;
    g_ring.count--;
    return 1;
}

void ky_input_reset(void) {
    g_ring.head = 0;
    g_ring.tail = 0;
    g_ring.count = 0;
}
