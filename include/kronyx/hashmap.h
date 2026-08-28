#ifndef KRONYX_HASHMAP_H
#define KRONYX_HASHMAP_H

#include "defines.h"
#include "memory.h"

typedef struct kyHashEntry {
    const char *key;
    void *value;
    uint32_t state;
} kyHashEntry;

typedef struct kyHashMap {
    kyHashEntry *entries;
    size_t count;
    size_t cap;
    size_t tomb_count;
    kyAllocator *alloc;
} kyHashMap;

#define KY_HASHMAP_STATE_EMPTY 0u
#define KY_HASHMAP_STATE_USED 1u
#define KY_HASHMAP_STATE_TOMB 2u

KY_API void ky_hashmap_init(kyHashMap *m, kyAllocator *alloc, size_t initial_cap);
KY_API void ky_hashmap_deinit(kyHashMap *m);
KY_API void ky_hashmap_set(kyHashMap *m, const char *key, void *value);
KY_API void ky_hashmap_set_key(kyHashMap *m, char *owned_key, void *value);
KY_API void *ky_hashmap_get(const kyHashMap *m, const char *key);
KY_API int ky_hashmap_has(const kyHashMap *m, const char *key);
KY_API int ky_hashmap_remove(kyHashMap *m, const char *key);
KY_API size_t ky_hashmap_count(const kyHashMap *m);
KY_API uint64_t ky_hash_str(const char *key);
KY_API uint64_t ky_hash_bytes(const void *data, size_t len, uint64_t seed);

#endif
