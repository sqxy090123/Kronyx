#ifndef KRONYX_SCENE_H
#define KRONYX_SCENE_H

#include "defines.h"
#include "memory.h"
#include "ecs.h"
#include "hashmap.h"

/* ── Public type ───────────────────────────────────────────────────── */

typedef struct kyScene {
    char  *name;          /* scene name (owned) */
    kyWorld *world;       /* ECS world backing this scene */
    kyHashMap meta;       /* key/value metadata */
    kyAllocator alloc;    /* allocator used for owned strings */
} kyScene;

/* ── Lifecycle ─────────────────────────────────────────────────────── */

KY_API kyScene *ky_scene_create(kyAllocator *alloc, const char *name);
KY_API void     ky_scene_destroy(kyScene *s);

/* ── Metadata ──────────────────────────────────────────────────────── */

KY_API void ky_scene_set_meta(kyScene *s, const char *key, const char *value);
KY_API const char *ky_scene_get_meta(const kyScene *s, const char *key);

/* ── Serialization ─────────────────────────────────────────────────── */

/* Save all alive entities with a Transform component to `path`.
 * Returns 0 on success, negative on error:
 *   -1  file I/O error (not found, permission, short read/write)
 *   -2  out of memory
 *   -3  format error during write (should not happen)
 */
KY_API int ky_scene_save(const kyScene *s, const char *path);

/* Load entities from `path` into `s->world`.
 * Clears existing world state first, re-registers transform/sprite/camera2d.
 * Returns 0 on success, negative on error:
 *   -1  file I/O error
 *   -2  out of memory
 *   -3  parse error (malformed .ksn)
 *   -4  missing required component (transform)
 */
KY_API int ky_scene_load(kyScene *s, const char *path);

#endif /* KRONYX_SCENE_H */
