#include "kronyx/scene.h"
#include "kronyx/string.h"

kyScene *ky_scene_create(kyAllocator *alloc, const char *name) {
    kyScene *s = (kyScene *)ky_mem_alloc(alloc, sizeof(kyScene));
    s->alloc = *alloc;
    s->world = ky_world_create(alloc);
    s->name = NULL;
    if (name) {
        size_t n = strlen(name) + 1;
        s->name = (char *)ky_mem_alloc(alloc, n);
        memcpy(s->name, name, n);
    }
    ky_hashmap_init(&s->meta, alloc, 8);
    return s;
}

void ky_scene_destroy(kyScene *s) {
    kyAllocator a = s->alloc;
    ky_world_destroy(s->world);
    ky_mem_free(&a, s->name);
    for (size_t i = 0; i < s->meta.cap; ++i) {
        kyHashEntry *e = &s->meta.entries[i];
        if (e->state == KY_HASHMAP_STATE_USED) {
            ky_mem_free(&a, (void *)e->key);
            ky_mem_free(&a, e->value);
        }
    }
    ky_hashmap_deinit(&s->meta);
    ky_mem_free(&a, s);
}

void ky_scene_set_meta(kyScene *s, const char *key, const char *value) {
    kyAllocator *al = &s->world->alloc;
    char *k = (char *)ky_mem_dup(al, key, strlen(key) + 1);
    char *v = (char *)ky_mem_dup(al, value, strlen(value) + 1);
    ky_hashmap_set_key(&s->meta, k, v);
}

const char *ky_scene_get_meta(const kyScene *s, const char *key) {
    return (const char *)ky_hashmap_get(&s->meta, key);
}
