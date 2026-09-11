#include "kronyx/resource.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* helpers                                                               */
/* ------------------------------------------------------------------ */

static void default_pixelbuffer_destroy(kyAllocator *a, void *payload) {
    if (!payload) return;
    kyPixelBuffer *pb = (kyPixelBuffer *)payload;
    ky_mem_free(a, pb->pixels);
    ky_mem_free(a, pb->name);
    ky_mem_free(a, pb);
}

static void default_raw_bytes_destroy(kyAllocator *a, void *payload) {
    ky_mem_free(a, payload);
}

static char *dup_cstr(kyAllocator *a, const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = (char *)ky_mem_alloc(a, n);
    if (d) memcpy(d, s, n);
    return d;
}

static char *copy_pixels(kyAllocator *a, const uint8_t *p, size_t n) {
    if (!p || n == 0) return NULL;
    uint8_t *d = (uint8_t *)ky_mem_alloc(a, n);
    if (d) memcpy(d, p, n);
    return d;
}

kyResourceManager *ky_resmgr_create(kyAllocator *alloc) {
    kyResourceManager *m = (kyResourceManager *)ky_mem_alloc(alloc, sizeof(kyResourceManager));
    m->alloc = *alloc;
    ky_hashmap_init(&m->resources, alloc, 32);
    return m;
}

void ky_resmgr_destroy(kyResourceManager *m) {
    if (!m) return;
    for (size_t i = 0; i < m->resources.cap; ++i) {
        kyHashEntry *e = &m->resources.entries[i];
        if (e->state != KY_HASHMAP_STATE_USED) continue;
        kyResource *r = (kyResource *)e->value;
        if (r && r->on_destroy && r->payload) r->on_destroy(&m->alloc, r->payload);
        ky_mem_free(&m->alloc, r);
        e->state = KY_HASHMAP_STATE_EMPTY;
        e->value = NULL;
        if (e->owned) {
            ky_mem_free(&m->alloc, (void *)e->key);
            e->key = NULL;
        }
    }
    ky_hashmap_deinit(&m->resources);
    ky_mem_free(&m->alloc, m);
}

int ky_resmgr_register(kyResourceManager *m, kyResource *r) {
    if (ky_resmgr_find(m, r->path)) return 0;
    size_t n = strlen(r->path) + 1;
    char *key = (char *)ky_mem_alloc(&m->alloc, n);
    memcpy(key, r->path, n);
    r->path = key;
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
    if (!m || !r) return;
    r->ref_count--;
    if (r->ref_count <= 0) {
        const char *key = r->path;
        ky_hashmap_remove(&m->resources, key);
        if (r->on_destroy && r->payload) r->on_destroy(&m->alloc, r->payload);
        ky_mem_free(&m->alloc, r);
    }
}

size_t ky_resmgr_count(const kyResourceManager *m) {
    if (!m) return 0;
    return ky_hashmap_count(&m->resources);
}

/* ------------------------------------------------------------------ */
/* convenience constructors                                              */
/* ------------------------------------------------------------------ */

kyResource *ky_resmgr_make_pixelbuffer(kyResourceManager *m, const char *path,
                                       const uint8_t *pixels, size_t byte_count,
                                       int width, int height, int channels) {
    if (!m || !path || !path[0] || !pixels) return NULL;
    if (width <= 0 || height <= 0) return NULL;
    if (channels < 1 || channels > 4) return NULL;
    if (byte_count < (size_t)(width * height * channels)) return NULL;
    if (ky_resmgr_find(m, path)) return NULL; /* idempotent */

    kyAllocator *a = &m->alloc;
    kyResource *r = (kyResource *)ky_mem_alloc(a, sizeof(kyResource));
    if (!r) return NULL;
    memset(r, 0, sizeof(*r));
    r->kind = KY_RES_PIXELBUFFER;
    r->id = (uint64_t)(uintptr_t)r;

    uint8_t *copy = copy_pixels(a, pixels, byte_count);
    if (!copy) { ky_mem_free(a, r); return NULL; }

    kyPixelBuffer *pb = (kyPixelBuffer *)ky_mem_alloc(a, sizeof(kyPixelBuffer));
    if (!pb) { ky_mem_free(a, copy); ky_mem_free(a, r); return NULL; }
    memset(pb, 0, sizeof(*pb));
    pb->pixels = copy;
    pb->byte_count = byte_count;
    pb->width = width;
    pb->height = height;
    pb->channels = channels;
    pb->name = dup_cstr(a, path);

    r->payload = pb;
    r->on_destroy = default_pixelbuffer_destroy;

    /* Pre-point r->path at the caller string so register can copy it;
     * register takes ownership of the copy in r->path on success.
     * Callers usually pass an owned/literal string we don't otherwise touch. */
    r->path = (char *)path;

    if (ky_resmgr_register(m, r) == 0) {
        /* Duplicate path in map: roll back all of it. */
        if (r->on_destroy && r->payload) r->on_destroy(a, r->payload);
        ky_mem_free(a, r);
        return NULL;
    }
    r->ref_count = 1; /* initial reference; caller must ky_resmgr_release to drop */
    return r;
}

kyResource *ky_resmgr_make_raw_bytes(kyResourceManager *m, const char *path,
                                     const void *data, size_t byte_count) {
    if (!m || !path || !path[0] || !data || byte_count == 0) return NULL;
    if (ky_resmgr_find(m, path)) return NULL;

    kyAllocator *a = &m->alloc;
    uint8_t *copy = copy_pixels(a, (const uint8_t *)data, byte_count);
    if (!copy) return NULL;

    kyResource *r = (kyResource *)ky_mem_alloc(a, sizeof(kyResource));
    if (!r) { ky_mem_free(a, copy); return NULL; }
    memset(r, 0, sizeof(*r));
    r->kind = KY_RES_TEXTURE; /* raw bytes; caller interprets */
    r->id = (uint64_t)(uintptr_t)r;
    r->payload = copy;
    r->on_destroy = default_raw_bytes_destroy;
    r->path = (char *)path; /* register copies on success */

    if (ky_resmgr_register(m, r) == 0) {
        ky_mem_free(a, copy);
        ky_mem_free(a, r);
        return NULL;
    }
    r->ref_count = 1;
    return r;
}
