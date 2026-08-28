#include "kronyx/resource.h"

kyResourceManager *ky_resmgr_create(kyAllocator *alloc) {
    kyResourceManager *m = (kyResourceManager *)ky_mem_alloc(alloc, sizeof(kyResourceManager));
    m->alloc = *alloc;
    ky_hashmap_init(&m->resources, alloc, 32);
    ky_array_init(&m->owned_keys, alloc, sizeof(char *), 32);
    return m;
}

static void resmgr_release_key(kyResourceManager *m, const char *key) {
    for (size_t i = 0; i < m->owned_keys.len; ++i) {
        char *k = *(char **)ky_array_get(&m->owned_keys, i);
        if (strcmp(k, key) == 0) {
            ky_mem_free(&m->alloc, k);
            ky_array_remove_swap(&m->owned_keys, i);
            return;
        }
    }
}

void ky_resmgr_destroy(kyResourceManager *m) {
    for (size_t i = 0; i < m->owned_keys.len; ++i) {
        char *k = *(char **)ky_array_get(&m->owned_keys, i);
        if (!k) continue;
        kyResource *r = (kyResource *)ky_hashmap_get(&m->resources, k);
        if (r) {
            ky_hashmap_remove(&m->resources, k);
            ky_mem_free(&m->alloc, r);
        }
        ky_mem_free(&m->alloc, k);
        *(char **)ky_array_get(&m->owned_keys, i) = NULL;
    }
    ky_array_deinit(&m->owned_keys);
    ky_hashmap_deinit(&m->resources);
    ky_mem_free(&m->alloc, m);
}

int ky_resmgr_register(kyResourceManager *m, kyResource *r) {
    if (ky_resmgr_find(m, r->path)) return 0;
    size_t n = strlen(r->path) + 1;
    char *key = (char *)ky_mem_alloc(&m->alloc, n);
    memcpy(key, r->path, n);
    r->path = key;
    ky_array_push(&m->owned_keys, &key);
    ky_hashmap_set_key(&m->resources, key, r);
    return 1;
}

kyResource *ky_resmgr_find(const kyResourceManager *m, const char *path) {
    return (kyResource *)ky_hashmap_get(&m->resources, path);
}

kyResource *ky_resmgr_acquire(kyResourceManager *m, const char *path) {
    kyResource *r = ky_resmgr_find(m, path);
    if (!r) return NULL;
    r->ref_count++;
    return r;
}

void ky_resmgr_release(kyResourceManager *m, kyResource *r) {
    if (!r) return;
    r->ref_count--;
    if (r->ref_count <= 0) {
        const char *key = r->path;
        ky_hashmap_remove(&m->resources, key);
        resmgr_release_key(m, key);
        ky_mem_free(&m->alloc, r);
    }
}

size_t ky_resmgr_count(const kyResourceManager *m) {
    return ky_hashmap_count(&m->resources);
}
