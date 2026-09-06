#ifndef KRONYX_ARENA_H
#define KRONYX_ARENA_H

#include "defines.h"
#include "memory.h"
#include <stddef.h>

/*
 * kyArena — bump-style region allocator.
 *
 * All allocations are within one contiguous backing block obtained from
 * the underlying allocator.  `ky_arena_reset` rewinds the bump pointer in
 * O(1); the backing block is kept.  `ky_arena_destroy` releases it.
 * Growth uses realloc and may relocate the block.
 */

typedef struct kyArena {
    void   *base;          /* pointer to the raw backing block            */
    size_t  capacity;      /* total size of the backing block             */
    size_t  used;          /* current bump pointer offset                 */
    kyAllocator alloc;     /* underlying allocator used to grab the block */
} kyArena;

/* Create an arena that requests at least `initial_capacity` bytes. */
KY_API kyArena ky_arena_create(size_t initial_capacity, kyAllocator *alloc);

/* Free the backing block.  No individual pointers returned by ky_arena_alloc
 * may be used after this call. */
KY_API void ky_arena_destroy(kyArena *a);

/* Bump-allocate `size` bytes, aligned to `align` (power of 2, else 16).
 * Grows the backing block if needed.  Returns NULL if growth fails. */
KY_API void *ky_arena_alloc(kyArena *a, size_t size, size_t align);

/* Allocate `size` bytes with the default 16-byte alignment. */
KY_API void *ky_arena_alloc_aligned(kyArena *a, size_t size);

/* Rewind the bump pointer.  Pointers stay valid until the next alloc or grow. */
KY_API void ky_arena_reset(kyArena *a);

/* Number of bytes currently consumed. */
KY_API size_t ky_arena_used_bytes(kyArena *a);

/* Total bytes requested from the underlying allocator (may be larger than
 * used_bytes due to growth steps). */
KY_API size_t ky_arena_capacity(kyArena *a);

#endif /* KRONYX_ARENA_H */
