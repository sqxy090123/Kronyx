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
} kyResourceKind;

typedef struct kyResource {
    kyResourceKind kind;
    uint64_t id;
    char *path;
    int32_t ref_count;
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

#endif
