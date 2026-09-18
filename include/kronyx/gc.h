#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/*
 * kyx Generational GC
 *
 * - Two generations: nursery (young) and tenured (old)
 * - Bump-pointer allocator inside each generation, 16-byte aligned
 * - Objects carry a 16-byte header: type, size, age, marked bits
 * - Promote to tenured after age >= KY_GC_PROMOTE_AGE
 * - Write barrier: log old->new edges, replayed during nursery GC
 */

#define KY_GC_OBJ_ALIGN    16
#define KY_GC_PROMOTE_AGE  3

/* Default generation sizes */
#define KY_GC_NURSERY_DEF  (256 * 1024)
#define KY_GC_TENURED_DEF  (4 * 1024)

typedef enum KyGcObjType {
    KY_GC_OBJ_STRING  = 0,
    KY_GC_OBJ_ARRAY   = 1,
    KY_GC_OBJ_CLOSURE = 2,
    KY_GC_OBJ_HEADER  = 3,   /* opaque user block */
} kyGcObjType;

typedef struct KyGcObject {
    uint32_t type;      /* kyGcObjType + marked bit */
    uint32_t size;      /* total object size in bytes (incl. header) */
    uint32_t age;
    uint32_t pad;
    uint8_t  data[0];   /* payload, aligned after this struct */
} kyGcObject;

/* MARKED flag stored in type field, bit 31 */
#define KY_GC_OBJ_MARKED  0x80000000u

typedef struct KyGcStats {
    uint32_t nursery_used_bytes;
    uint32_t nursery_capacity_bytes;
    uint32_t tenured_used_bytes;
    uint32_t tenured_capacity_bytes;
    uint64_t total_gc_count;         /* nursery GC runs */
    uint64_t total_t_gen_gc_count;   /* tenured GC runs */
    uint64_t total_freed_bytes;
    uint64_t nursery_degraded_count;
    int      last_gc_time_ms;
    int      auto_disabled;
} KyGcStats;

typedef struct KyGcHeap {
    /* nursery */
    uint8_t  *nursery;
    uint32_t  nursery_size;
    uint32_t  nursery_used;
    /* tenured */
    uint8_t  *tenured;
    uint32_t  tenured_size;
    uint32_t  tenured_used;
    /* write barrier log: pointers to new-gen objects referenced from old-gen */
    uint8_t **barrier_log;
    uint32_t  barrier_count;
    uint32_t  barrier_cap;
    /* stats */
    uint64_t  gc_count_n;
    uint64_t  gc_count_t;
    uint64_t  freed_bytes;
    uint64_t  nursery_degraded;
    int       last_gc_time_ms;
    int       auto_disabled;
    /* config */
    uint32_t  nursery_trigger;   /* 0 = auto disabled */
    /* suppression: skip next nursery GC if two in a row freed nothing */
    int       gc_suppressed;
} KyGcHeap;

/* Forward decls */
struct kyVM;

/* Lifecycle */
void ky_gc_heap_init(struct kyVM *vm, uint32_t nursery_bytes, uint32_t tenured_bytes);
void ky_gc_heap_free(struct kyVM *vm);
int  ky_vm_set_heap_size(struct kyVM *vm, uint32_t nursery_bytes, uint32_t tenured_bytes);

/* Allocation (returns NULL on failure after GC retry) */
void *ky_gc_heap_alloc(struct kyVM *vm, kyGcObjType type, uint32_t byte_len);

/* GC runs */
size_t ky_gc_run_nursery(struct kyVM *vm);
size_t ky_gc_run_full(struct kyVM *vm);
void   ky_gc_handle_barrier(struct kyVM *vm);
void   ky_gc_barrier_append(struct kyVM *vm, uint8_t *ptr);

/* GC API */
size_t ky_vm_gc_collect(struct kyVM *vm);
int    ky_vm_gc_stats(struct kyVM *vm, KyGcStats *out);
int    ky_vm_gc_mark(struct kyVM *vm, void *ptr);
int    ky_vm_gc_unmark(struct kyVM *vm, void *ptr);
int    ky_vm_gc_set_auto_threshold(struct kyVM *vm, uint32_t trigger_bytes);

/* Pointer helpers */
int  ky_gc_is_gc_str(struct kyVM *vm, const void *ptr);
int  ky_gc_obj_in_gen(const uint8_t *ptr, const uint8_t *base, uint32_t size);

