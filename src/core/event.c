#include "kronyx/event.h"
#include <string.h>
#include <stdio.h>

#define KY_EVENT_MAX_LISTENERS 32

typedef struct kyEventListener {
    char name[64];
    kyEventFn fn;
    void *user;
    int name_count; /* how many registered slots have the same name */
} kyEventListener;

static kyEventListener g_registry[KY_EVENT_MAX_LISTENERS];

int ky_event_register(const char *name, kyEventFn fn, void *user) {
    if (!name || !fn) return -1;
    if (strlen(name) >= sizeof(((kyEventListener *)0)->name)) return -4; /* name too long */
    /* Single pass: reject the exact (name, fn, user) triple, count the
     * number of prior listeners sharing `name` for the returned index. */
    int prior = 0;
    int dup = 0;
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        if (!g_registry[i].fn || !g_registry[i].name[0]) continue;
        if (strcmp(g_registry[i].name, name) == 0) {
            prior++;
            if (g_registry[i].fn == fn && g_registry[i].user == user) dup = 1;
        }
    }
    if (dup) return -3; /* already registered */
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        if (!g_registry[i].fn) {
            snprintf(g_registry[i].name, sizeof(g_registry[i].name), "%s", name);
            g_registry[i].fn = fn;
            g_registry[i].user = user;
            g_registry[i].name_count = prior + 1;
            return prior + 1;
        }
    }
    return -2; /* full */
}

void ky_event_trigger(const char *name, const void *data) {
    if (!name) return;
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        kyEventListener *e = &g_registry[i];
        if (e->fn && e->name[0] && strcmp(e->name, name) == 0)
            e->fn(name, data, e->user);
    }
}

void ky_event_clear(void) {
    for (int i = 0; i < KY_EVENT_MAX_LISTENERS; i++) {
        g_registry[i].fn = NULL;
        g_registry[i].user = NULL;
        g_registry[i].name[0] = 0;
        g_registry[i].name_count = 0;
    }
}
