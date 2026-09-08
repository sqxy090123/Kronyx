#ifndef KRONYX_RESOURCE_H
#define KRONYX_RESOURCE_H

#include "defines.h"
#include "memory.h"
#include "hashmap.h"
#include "array.h"

typedef enum kyResourceKind {
    KY_RES_UNKNOWN = 0,
    KY_RES_MESH,
    KY_RES_TEXTURE,
    KY_RES_SHADER,
    KY_RES_MATERIAL,
    KY_RES_SCENE,
    KY_RES_AUDIO,
    KY_RES_PIXELBUFFER,
} kyResourceKind;

/* Payload for KY_RES_PIXELBUFFER: raw pixel bytes + descriptor.
 * The kyResource owns this block; it must be set via ky_resmgr_make_pixelbuffer
 * (which also installs a matching on_destroy) or by a caller that also sets
 * r->on_destroy. The resource manager calls on_destroy(payload) when the
 * resource's refcount hits zero or on resmgr_destroy. */
typedef struct kyPixelBuffer {
    uint8_t *pixels;
    size_t   byte_count;
    int      width;
    int      height;
    int      channels; /* 1, 2, 3 or 4 */
    char    *name;     /* optional; owned, freed with pixels */
} kyPixelBuffer;

typedef struct kyResource {
    kyResourceKind kind;
    uint64_t id;
    char *path;
    int32_t ref_count;
    void  *payload;                       /* opaque; set per-kind */
    void (*on_destroy)(kyAllocator *a, void *payload); /* called on refcount 0 / destroy */
    void (*reload)(struct kyResource *r);
} kyResource;

typedef struct kyResourceManager {
    kyAllocator alloc;
    kyHashMap resources;
    kyArray owned_keys;
} kyResourceManager;

KY_API kyResourceManager *ky_resmgr_create(kyAllocator *alloc);
KY_API void ky_resmgr_destroy(kyResourceManager *m);

KY_API int ky_resmgr_register(kyResourceManager *m, kyResource *r);
KY_API kyResource *ky_resmgr_find(const kyResourceManager *m, const char *path);
KY_API kyResource *ky_resmgr_acquire(kyResourceManager *m, const char *path);
KY_API void ky_resmgr_release(kyResourceManager *m, kyResource *r);
KY_API size_t ky_resmgr_count(const kyResourceManager *m);

/* Convenience constructors: allocate a kyResource, wire payload + on_destroy
 * to match the kind, and register with m. On success returns the registered
 * resource (call ky_resmgr_release to drop the initial reference).
 * Returns NULL on allocation failure / duplicate path / invalid params.
 *
 * Note: the caller still owns the input bytes (pixels / data) - the
 * convenience ctor copies them into the resource-managed payload. */
KY_API kyResource *ky_resmgr_make_pixelbuffer(kyResourceManager *m, const char *path,
                                              const uint8_t *pixels, size_t byte_count,
                                              int width, int height, int channels);
KY_API kyResource *ky_resmgr_make_raw_bytes(kyResourceManager *m, const char *path,
                                            const void *data, size_t byte_count);

#endif
