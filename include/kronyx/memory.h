#ifndef KRONYX_MEMORY_H
#define KRONYX_MEMORY_H

#include "defines.h"

typedef void *(*kyAllocFn)(void *ud, size_t size);
typedef void *(*kyReallocFn)(void *ud, void *ptr, size_t size);
typedef void (*kyFreeFn)(void *ud, void *ptr);

typedef struct kyAllocator {
    void *user_data;
    kyAllocFn alloc;
    kyReallocFn realloc_fn;
    kyFreeFn free_fn;
} kyAllocator;

typedef struct kyMemStats {
    size_t alloc_count;
    size_t free_count;
    size_t live_bytes;
    size_t peak_bytes;
} kyMemStats;

KY_API kyAllocator ky_default_allocator(void);
KY_API void *ky_mem_alloc(kyAllocator *a, size_t size);
KY_API void *ky_mem_realloc(kyAllocator *a, void *ptr, size_t size);
KY_API void ky_mem_free(kyAllocator *a, void *ptr);
KY_API void *ky_mem_dup(kyAllocator *a, const void *src, size_t size);

KY_API kyAllocator ky_tracking_allocator(kyMemStats *stats_out);
KY_API void ky_mem_stats_snapshot(kyMemStats *out);

#endif
