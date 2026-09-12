#ifndef KRONYX_HASHMAP_H
#define KRONYX_HASHMAP_H

/**
 * @file kronyx/hashmap.h
 * @brief Open-addressing hash map with linear probing.
 *
 * Entries use three states: EMPTY (0), USED (1), TOMB (2).
 * Tomb entries preserve probe chains after removal.
 * Auto-resizes when (count + tomb_count) * 4 >= cap * 3.
 * ky_hashmap_remove resets entries to EMPTY and frees owned keys immediately,
 * triggering compaction when tomb accumulation exceeds the threshold.
 */

#include "defines.h"
#include "memory.h"

typedef struct kyHashEntry {
    const char *key;
    void *value;
    uint32_t state;
    uint8_t owned;
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
/** Set value; if value is NULL calls remove. Does not own the key. */
KY_API void ky_hashmap_set(kyHashMap *m, const char *key, void *value);
/** Set value; caller owns key (hashmap will free it on remove/deinit). */
KY_API void ky_hashmap_set_key(kyHashMap *m, char *owned_key, void *value);
KY_API void *ky_hashmap_get(const kyHashMap *m, const char *key);
KY_API int ky_hashmap_has(const kyHashMap *m, const char *key);
/** Remove entry; resets slot to EMPTY and frees owned key immediately. */
KY_API int ky_hashmap_remove(kyHashMap *m, const char *key);
KY_API size_t ky_hashmap_count(const kyHashMap *m);
/** Compute hash for a null-terminated string key. */
KY_API uint64_t ky_hash_str(const char *key);
/** Compute hash for arbitrary byte data. */
KY_API uint64_t ky_hash_bytes(const void *data, size_t len, uint64_t seed);

#endif
