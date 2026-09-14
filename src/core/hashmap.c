#include "kronyx/hashmap.h"

uint64_t ky_hash_bytes(const void *data, size_t len, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ^ (len * 0x9E3779B97F4A7C15ull);
    while (len >= 8) {
        uint64_t k;
        memcpy(&k, p, 8);
        k *= 0xFF51AFD7ED558CCDull;
        k ^= k >> 33;
        k *= 0xC4CEB9FE1A85EC53ull;
        h ^= k;
        h *= 0xFF51AFD7ED558CCDull;
        h ^= h >> 33;
        p += 8;
        len -= 8;
    }
    while (len--) {
        h ^= *p++;
        h *= 0x100000001B3ull;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    return h;
}

uint64_t ky_hash_str(const char *key) {
    return ky_hash_bytes(key, strlen(key), 0x1234ABCDull);
}

static int hashmap_resize(kyHashMap *m, size_t new_cap) {
    kyHashEntry *new_entries = (kyHashEntry *)ky_mem_alloc(m->alloc, new_cap * sizeof(kyHashEntry));
    if (!new_entries) return 0;
    memset(new_entries, 0, new_cap * sizeof(kyHashEntry));
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < m->cap; ++i) {
        const kyHashEntry *e = &m->entries[i];
        if (e->state != KY_HASHMAP_STATE_USED) continue;
        size_t slot = (size_t)ky_hash_str(e->key) & mask;
        while (new_entries[slot].state == KY_HASHMAP_STATE_USED) {
            slot = (slot + 1) & mask;
        }
        new_entries[slot] = *e;
    }
    ky_mem_free(m->alloc, m->entries);
    m->entries = new_entries;
    m->cap = new_cap;
    m->tomb_count = 0;
    return 1;
}

void ky_hashmap_init(kyHashMap *m, kyAllocator *alloc, size_t initial_cap) {
    m->entries = NULL;
    m->count = 0;
    m->cap = 0;
    m->tomb_count = 0;
    m->alloc = alloc;
    if (initial_cap == 0) initial_cap = 16;
    hashmap_resize(m, initial_cap);
}

void ky_hashmap_deinit(kyHashMap *m) {
    for (size_t i = 0; i < m->cap; ++i) {
        if (m->entries[i].state == KY_HASHMAP_STATE_USED) {
            if (m->entries[i].owned) {
                ky_mem_free(m->alloc, (void *)m->entries[i].key);
            }
            m->entries[i].state = KY_HASHMAP_STATE_EMPTY;
            m->entries[i].key = NULL;
            m->entries[i].value = NULL;
        }
    }
    ky_mem_free(m->alloc, m->entries);
    m->entries = NULL;
    m->count = m->cap = m->tomb_count = 0;
}

static kyHashEntry *find_entry(kyHashMap *m, const char *key, size_t *out_dist) {
    if (m->cap == 0) return NULL;
    size_t mask = m->cap - 1;
    size_t slot = (size_t)ky_hash_str(key) & mask;
    size_t dist = 0;
    /* TOMB entries keep probe chains alive, so only EMPTY terminates a probe. */
    while (m->entries[slot].state != KY_HASHMAP_STATE_EMPTY) {
        if (m->entries[slot].state == KY_HASHMAP_STATE_USED &&
            strcmp(m->entries[slot].key, key) == 0) {
            if (out_dist) *out_dist = dist;
            return &m->entries[slot];
        }
        slot = (slot + 1) & mask;
        dist++;
        if (dist >= m->cap) break;
    }
    return NULL;
}

void *ky_hashmap_get(const kyHashMap *m, const char *key) {
    kyHashEntry *e = find_entry((kyHashMap *)m, key, NULL);
    return e ? e->value : NULL;
}

int ky_hashmap_has(const kyHashMap *m, const char *key) {
    return find_entry((kyHashMap *)m, key, NULL) != NULL;
}

static void hashmap_set_impl(kyHashMap *m, const char *key, void *value, int owns_key) {
    if (m->cap == 0) {
        hashmap_resize(m, 16);
    }
    size_t used = m->count + m->tomb_count;
    if (used * 4 >= m->cap * 3) {
        hashmap_resize(m, m->cap * 2);
    }
    size_t mask = m->cap - 1;
    size_t slot = (size_t)ky_hash_str(key) & mask;
    size_t insert_slot = (size_t)-1;
    /* Walk the full probe chain, remembering the first reusable TOMB. */
    while (m->entries[slot].state != KY_HASHMAP_STATE_EMPTY) {
        if (m->entries[slot].state == KY_HASHMAP_STATE_USED &&
            strcmp(m->entries[slot].key, key) == 0) {
            /* Ownership of the incoming key transfers only on real insert. */
            if (owns_key) ky_mem_free(m->alloc, (void *)(uintptr_t)key);
            m->entries[slot].value = value;
            return;
        }
        if (m->entries[slot].state == KY_HASHMAP_STATE_TOMB &&
            insert_slot == (size_t)-1) {
            insert_slot = slot;
        }
        slot = (slot + 1) & mask;
    }
    if (insert_slot == (size_t)-1) {
        insert_slot = slot; /* slot is EMPTY */
    } else {
        m->tomb_count--; /* reusing a tombstone */
    }
    KY_ASSERT(insert_slot < m->cap);
    m->entries[insert_slot].key = key;
    m->entries[insert_slot].value = value;
    m->entries[insert_slot].state = KY_HASHMAP_STATE_USED;
    m->entries[insert_slot].owned = owns_key;
    m->count++;
    KY_ASSERT(m->count + m->tomb_count <= m->cap);
}

void ky_hashmap_set(kyHashMap *m, const char *key, void *value) {
    if (value == NULL) {
        ky_hashmap_remove(m, key);
        return;
    }
    hashmap_set_impl(m, key, value, 0);
}

void ky_hashmap_set_key(kyHashMap *m, char *owned_key, void *value) {
    hashmap_set_impl(m, owned_key, value, 1);
}

int ky_hashmap_remove(kyHashMap *m, const char *key) {
    kyHashEntry *e = find_entry(m, key, NULL);
    if (!e) return 0;
    /* Release owned key, mark TOMB so later probes still walk past this slot. */
    if (e->owned) {
        ky_mem_free(m->alloc, (void *)(uintptr_t)e->key);
    }
    e->key = NULL;
    e->value = NULL;
    e->owned = 0;
    e->state = KY_HASHMAP_STATE_TOMB;
    m->count--;
    m->tomb_count++;
    /* Reclamation happens in place on the next insert or resize. */
    return 1;
}

size_t ky_hashmap_count(const kyHashMap *m) {
    return m->count;
}
