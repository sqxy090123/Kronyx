#include "kronyx/event.h"
#include <string.h>

#define KY_EVENT_MAX_LISTENERS 32

typedef struct kyEventListener {
    const char *name;
    kyEventFn fn;
    void *user;
} kyEventListener;

static kyEventListener g_registry[KY_EVENT_MAX_LISTENERS];

int ky_event_register(const char *name, kyEventFn fn, void *user) {
    if (!name || !fn) return -1;
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        if (!g_registry[i].fn) {
            g_registry[i].name = name;
            g_registry[i].fn = fn;
            g_registry[i].user = user;
            int count = 0;
            for (int j = 0; j < KY_EVENT_MAX_LISTENERS; j++)
                if (g_registry[j].fn && strcmp(g_registry[j].name, name) == 0)
                    count++;
            return count;
        }
    }
    return -2; /* full */
}

void ky_event_trigger(const char *name, const void *data) {
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        kyEventListener *e = &g_registry[i];
        if (e->fn && e->name && strcmp(e->name, name) == 0)
            e->fn(name, data, e->user);
    }
}
