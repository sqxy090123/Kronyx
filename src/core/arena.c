#include "kronyx/arena.h"
#include <string.h>
#include <stdint.h>

#define KY_ARENA_ALIGN 16u
#define KY_ARENA_GROW_MIN (1u << 14)  /* 16 KiB minimum grow step */
#define KY_ARENA_GROW_FACTOR 2u

static size_t align_up(size_t n, size_t align) {
    return (n + align - 1u) & ~(align - 1u);
}

kyArena ky_arena_create(size_t initial_capacity, kyAllocator *alloc) {
    if (initial_capacity == 0) initial_capacity = KY_ARENA_GROW_MIN;
    initial_capacity = align_up(initial_capacity, KY_ARENA_ALIGN);
    kyArena a;
    a.alloc = *alloc;
    a.base = ky_mem_alloc(alloc, initial_capacity);
    a.capacity = initial_capacity;
    a.used = 0;
    return a;
}

void ky_arena_destroy(kyArena *a) {
    if (!a || !a->base) return;
    ky_mem_free(&a->alloc, a->base);
    a->base = NULL;
    a->capacity = 0;
    a->used = 0;
}

static void *grow_arena(kyArena *a, size_t needed) {
    size_t new_cap = a->capacity;
    if (new_cap == 0) new_cap = KY_ARENA_GROW_MIN;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / KY_ARENA_GROW_FACTOR) {
            new_cap = needed; /* overflow guard: stop doubling, jump to needed */
            break;
        }
        new_cap *= KY_ARENA_GROW_FACTOR;
    }
    if (new_cap < needed) new_cap = needed;
    void *new_base = ky_mem_realloc(&a->alloc, a->base, new_cap);
    if (!new_base) return NULL;
    a->base = new_base;
    a->capacity = new_cap;
    return new_base;
}

void *ky_arena_alloc(kyArena *a, size_t size, size_t align) {
    if (!a || size == 0) return NULL;
    if (align == 0) align = KY_ARENA_ALIGN;
    else if ((align & (align - 1)) != 0) align = KY_ARENA_ALIGN;
    uintptr_t base = (uintptr_t)a->base;
    uintptr_t current = base + a->used;
    uintptr_t aligned = (current + (uintptr_t)align - 1u) & ~((uintptr_t)align - 1u);
    size_t aligned_used = (size_t)(aligned - base);
    size_t total = aligned_used + size;
    if (total > a->capacity) {
        if (!grow_arena(a, total)) return NULL;
        base = (uintptr_t)a->base;
        current = base + a->used;
        aligned = (current + (uintptr_t)align - 1u) & ~((uintptr_t)align - 1u);
        aligned_used = (size_t)(aligned - base);
        total = aligned_used + size;
        if (total > a->capacity) return NULL;
    }
    void *ptr = (char *)a->base + aligned_used;
    a->used = total;
    return ptr;
}

void *ky_arena_alloc_aligned(kyArena *a, size_t size) {
    return ky_arena_alloc(a, size, KY_ARENA_ALIGN);
}

void ky_arena_reset(kyArena *a) {
    if (a) a->used = 0;
}

size_t ky_arena_used_bytes(kyArena *a) {
    return a ? a->used : 0;
}

size_t ky_arena_capacity(kyArena *a) {
    return a ? a->capacity : 0;
}
