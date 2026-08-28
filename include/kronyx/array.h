#ifndef KRONYX_ARRAY_H
#define KRONYX_ARRAY_H

#include "defines.h"
#include "memory.h"

typedef struct kyArray {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
    kyAllocator *alloc;
} kyArray;

KY_API void ky_array_init(kyArray *a, kyAllocator *alloc, size_t elem_size, size_t initial_cap);
KY_API void ky_array_deinit(kyArray *a);
KY_API void ky_array_reserve(kyArray *a, size_t cap);
KY_API void *ky_array_emplace(kyArray *a);
KY_API void *ky_array_push(kyArray *a, const void *elem);
KY_API void *ky_array_get(const kyArray *a, size_t index);
KY_API void ky_array_remove_swap(kyArray *a, size_t index);
KY_API void ky_array_clear(kyArray *a);

#endif
