#ifndef KRONYX_EVENT_H
#define KRONYX_EVENT_H

#include "defines.h"

/*
 * Minimal global event dispatcher. V1 stores registrations in a fixed-size
 * list; names are compared by string match on every trigger.
 */

typedef void (*kyEventFn)(const char *name, const void *data, void *user);

/* Register `fn` to be called whenever an event with `name` is triggered.
 * The same name can be registered multiple times; all callbacks fire in
 * registration order. Returns the number of live listeners for this name
 * after registration (useful for deduplication checks). */
KY_API int ky_event_register(const char *name, kyEventFn fn, void *user);

/* Fire an event named `name`, passing `data` to every listener. Pass NULL
 * for data when the event carries no payload. */
KY_API void ky_event_trigger(const char *name, const void *data);

#endif
