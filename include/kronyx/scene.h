#ifndef KRONYX_SCENE_H
#define KRONYX_SCENE_H

#include "defines.h"
#include "memory.h"
#include "ecs.h"
#include "hashmap.h"

typedef struct kyScene {
    char *name;
    kyWorld *world;
    kyHashMap meta;
    kyAllocator alloc;
} kyScene;

KY_API kyScene *ky_scene_create(kyAllocator *alloc, const char *name);
KY_API void ky_scene_destroy(kyScene *s);
KY_API void ky_scene_set_meta(kyScene *s, const char *key, const char *value);
KY_API const char *ky_scene_get_meta(const kyScene *s, const char *key);

#endif
