#ifndef KRONYX_POOL_H
#define KRONYX_POOL_H

#include "defines.h"
#include "memory.h"
#include <stdint.h>

/*
 * kyPool — fixed-size slab pool allocator.
 *
 * Each slot is `slot_size` bytes (rounded up).  Allocations come from a
 * free list.  ky_pool_reset rebuilds the free list in O(N) and keeps the
 * backing block.  Growth copies into a larger block and invalidates
 * previously returned slot pointers.
 */

typedef struct kyPoolSlot {
    uint32_t next_free;
} kyPoolSlot;

typedef struct kyPool {
    kyAllocator alloc;
    size_t       slot_size;   /* rounded up to at least sizeof(kyPoolSlot) */
    size_t       count;
    size_t       cap;
    uint32_t    *free_list;   /* freelist head indices (uint32_t[] or kyPoolSlot[]) */
    void        *slots;       /* raw storage for all slots                    */
    uint32_t     head;        /* index of first free slot, 0xFFFFFFFF = full  */
} kyPool;

/* Create a pool with `initial_cap` slots of `slot_size` bytes. */
KY_API kyPool ky_pool_create(size_t slot_size, size_t initial_cap, kyAllocator *alloc);

/* Destroy the pool and release its backing memory. */
KY_API void ky_pool_destroy(kyPool *p);

/* Reset: invalidate all slots and refill the free list. */
KY_API void ky_pool_reset(kyPool *p);

/* Grab one slot.  Grows the pool when exhausted; returns NULL on OOM. */
KY_API void *ky_pool_alloc(kyPool *p);

/* Return one slot to the pool.  The slot must have been previously returned
 * by ky_pool_alloc and must not have been reused in the meantime. */
KY_API void ky_pool_free(kyPool *p, void *ptr);

/* Number of allocated (non-free) slots. */
KY_API size_t ky_pool_count(const kyPool *p);

/* Total capacity in slots. */
KY_API size_t ky_pool_capacity(const kyPool *p);

#endif /* KRONYX_POOL_H */
