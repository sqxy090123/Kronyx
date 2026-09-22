#include "kronyx/event.h"
#include <string.h>

#define KY_EVENT_MAX_LISTENERS 32

typedef struct kyEventListener {
    const char *name;
    kyEventFn fn;
    void *user;
    int name_count; /* how many registered slots have the same name */
} kyEventListener;

static kyEventListener g_registry[KY_EVENT_MAX_LISTENERS];

static int find_name_count(const char *name) {
    int count = 0;
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++)
        if (g_registry[i].fn && strcmp(g_registry[i].name, name) == 0)
            count++;
    return count;
}

int ky_event_register(const char *name, kyEventFn fn, void *user) {
    if (!name || !fn) return -1;
    /* Reject duplicate registration of the same (name, fn, user) triple */
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        if (g_registry[i].fn && g_registry[i].name &&
            strcmp(g_registry[i].name, name) == 0 &&
            g_registry[i].fn == fn && g_registry[i].user == user)
            return -3; /* already registered */
    }
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        if (!g_registry[i].fn) {
            int prior = find_name_count(name);
            g_registry[i].name = name;
            g_registry[i].fn = fn;
            g_registry[i].user = user;
            g_registry[i].name_count = prior + 1;
            return prior + 1;
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
