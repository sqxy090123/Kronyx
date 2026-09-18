#include "kronyx/gc.h"
#include "kronyx/script.h"
#include "script_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward decl: kyVM layout with gc field added */
/* The real struct is in vm.c; we need to extend it there. */
/* For now we forward-declare and let vm.c define the full struct. */

/* ---------------------------------------------------------------------------
 * Timing helper
 * --------------------------------------------------------------------------- */
static int64_t now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (int64_t)cnt.QuadPart * 1000 / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* ---------------------------------------------------------------------------
 * ky_gc_obj_in_gen
 * --------------------------------------------------------------------------- */
int ky_gc_obj_in_gen(const uint8_t *ptr, const uint8_t *base, uint32_t size) {
    if (!ptr || !base) return 0;
    return (const uint8_t *)ptr >= base &&
           (const uint8_t *)ptr <  base + size;
}

/* ---------------------------------------------------------------------------
 * ky_gc_is_gc_str — is ptr pointing into a GC string object?
 * --------------------------------------------------------------------------- */
int ky_gc_is_gc_str(struct kyVM *vm, const void *ptr) {
    if (!vm || !ptr) return 0;
    KyGcHeap *gc = &vm->gc;
    /* Check nursery */
    if (ky_gc_obj_in_gen((const uint8_t *)ptr, gc->nursery, gc->nursery_size)) {
        return 1;
    }
    /* Check tenured */
    if (ky_gc_obj_in_gen((const uint8_t *)ptr, gc->tenured, gc->tenured_size)) {
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * ky_gc_heap_init
 * --------------------------------------------------------------------------- */
void ky_gc_heap_init(struct kyVM *vm, uint32_t nursery_bytes, uint32_t tenured_bytes) {
    if (nursery_bytes == 0) nursery_bytes = KY_GC_NURSERY_DEF;
    if (tenured_bytes == 0) tenured_bytes = KY_GC_TENURED_DEF;
    /* Align up */
    nursery_bytes = (nursery_bytes + KY_GC_OBJ_ALIGN - 1) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
    tenured_bytes = (tenured_bytes + KY_GC_OBJ_ALIGN - 1) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);

    KyGcHeap *gc = &vm->gc;
    memset(gc, 0, sizeof(KyGcHeap));
    gc->nursery_size  = nursery_bytes;
    gc->tenured_size  = tenured_bytes;
    gc->nursery       = (uint8_t *)calloc(1, gc->nursery_size);
    gc->tenured       = (uint8_t *)calloc(1, gc->tenured_size);
    gc->nursery_trigger = gc->nursery_size; /* trigger when full */
    gc->barrier_cap   = 256;
    gc->barrier_log   = (uint8_t **)calloc(gc->barrier_cap, sizeof(uint8_t *));
    gc->auto_disabled = 0;
    gc->gc_suppressed = 0;
}

/* ---------------------------------------------------------------------------
 * ky_gc_barrier_append — append a pointer to the barrier log, grow if needed
 * --------------------------------------------------------------------------- */
void ky_gc_barrier_append(struct kyVM *vm, uint8_t *ptr) {
    if (!vm || !ptr) return;
    KyGcHeap *gc = &vm->gc;
    if (gc->barrier_count >= gc->barrier_cap) {
        uint32_t new_cap = gc->barrier_cap * 2;
        uint8_t **new_log = (uint8_t **)realloc(gc->barrier_log, new_cap * sizeof(uint8_t *));
        if (!new_log) return; /* full, drop */
        gc->barrier_log = new_log;
        gc->barrier_cap = new_cap;
    }
    gc->barrier_log[gc->barrier_count++] = ptr;
}

/* ---------------------------------------------------------------------------
 * ky_gc_heap_free
 * --------------------------------------------------------------------------- */
void ky_gc_heap_free(struct kyVM *vm) {
    if (!vm) return;
    KyGcHeap *gc = &vm->gc;
    if (gc->nursery)  free(gc->nursery);
    if (gc->tenured)  free(gc->tenured);
    if (gc->barrier_log) free(gc->barrier_log);
    memset(gc, 0, sizeof(KyGcHeap));
}

/* ---------------------------------------------------------------------------
 * ky_vm_set_heap_size — must be called before first script execution
 * --------------------------------------------------------------------------- */
int ky_vm_set_heap_size(struct kyVM *vm, uint32_t nursery_bytes, uint32_t tenured_bytes) {
    if (!vm) return -1;
    KyGcHeap *gc = &vm->gc;
    /* Only allow before VM has run scripts (no heap usage yet) */
    if (gc->nursery_used > 0 || gc->tenured_used > 0) return -1;
    if (gc->nursery)  free(gc->nursery);
    if (gc->tenured)  free(gc->tenured);
    if (gc->barrier_log) { free(gc->barrier_log); gc->barrier_log = NULL; gc->barrier_count = 0; gc->barrier_cap = 0; }
    ky_gc_heap_init(vm, nursery_bytes, tenured_bytes);
    return 0;
}

/* ---------------------------------------------------------------------------
 * ky_gc_heap_alloc
 *
 * Allocates a kyGcObject of the given type and byte_len payload.
 * Returns pointer to data[] (just after the header) or NULL on failure.
 * On failure: runs nursery GC, then full GC, retries once.
 * --------------------------------------------------------------------------- */
void *ky_gc_heap_alloc(struct kyVM *vm, kyGcObjType type, uint32_t byte_len) {
    if (!vm) return NULL;
    KyGcHeap *gc = &vm->gc;

    /* Compute object size: header + aligned payload */
    uint32_t hdr = sizeof(kyGcObject);
    uint32_t total = (uint32_t)(hdr + byte_len + (KY_GC_OBJ_ALIGN - 1)) &
                     ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);

    /* Try nursery first */
    if (gc->nursery_used + total <= gc->nursery_size) {
        uint8_t *ptr = gc->nursery + gc->nursery_used;
        kyGcObject *obj = (kyGcObject *)ptr;
        memset(obj, 0, sizeof(kyGcObject));
        obj->type = (uint32_t)type;
        obj->size = total;
        obj->age  = 0;
        gc->nursery_used += total;
        return obj->data;
    }

    /* Nursery full: try GC */
    if (!gc->auto_disabled && !gc->gc_suppressed &&
        gc->nursery_used >= gc->nursery_trigger) {
        ky_gc_run_nursery(vm);
        if (gc->nursery_used + total <= gc->nursery_size) {
            uint8_t *ptr = gc->nursery + gc->nursery_used;
            kyGcObject *obj = (kyGcObject *)ptr;
            memset(obj, 0, sizeof(kyGcObject));
            obj->type = (uint32_t)type;
            obj->size = total;
            obj->age  = 0;
            gc->nursery_used += total;
            return obj->data;
        }
    }

    /* Fall back to tenured */
    if (gc->tenured_used + total <= gc->tenured_size) {
        uint8_t *ptr = gc->tenured + gc->tenured_used;
        kyGcObject *obj = (kyGcObject *)ptr;
        memset(obj, 0, sizeof(kyGcObject));
        obj->type = (uint32_t)type;
        obj->size = total;
        obj->age  = 1; /* already old */
        gc->tenured_used += total;
        return obj->data;
    }

    /* Tenured full: run full GC */
    ky_gc_run_full(vm);
    if (gc->nursery_used + total <= gc->nursery_size) {
        uint8_t *ptr = gc->nursery + gc->nursery_used;
        kyGcObject *obj = (kyGcObject *)ptr;
        memset(obj, 0, sizeof(kyGcObject));
        obj->type = (uint32_t)type;
        obj->size = total;
        gc->nursery_used += total;
        return obj->data;
    }
    if (gc->tenured_used + total <= gc->tenured_size) {
        uint8_t *ptr = gc->tenured + gc->tenured_used;
        kyGcObject *obj = (kyGcObject *)ptr;
        memset(obj, 0, sizeof(kyGcObject));
        obj->type = (uint32_t)type;
        obj->size = total;
        gc->tenured_used += total;
        return obj->data;
    }

    /* Total failure */
    snprintf(vm->error_msg, sizeof(vm->error_msg), "gc alloc failed");
    return NULL;
}

/* ---------------------------------------------------------------------------
 * GC internals — mark phase
 * --------------------------------------------------------------------------- */
static void mark_object(struct kyVM *vm, kyGcObject *obj) {
    if (!obj) return;
    if (obj->type & KY_GC_OBJ_MARKED) return;
    obj->type |= KY_GC_OBJ_MARKED;

    switch (obj->type & 0xFF) {
        case KY_GC_OBJ_STRING: {
            /* No child pointers in string payload */
            break;
        }
        case KY_GC_OBJ_ARRAY: {
            /* Payload: kyValue * elems; elems is embedded or heap-allocated */
            /* For now we skip scanning array elements (no GC'd sub-objects expected) */
            break;
        }
        case KY_GC_OBJ_CLOSURE: {
            /* Closure payload: kyClosure* (proto pointer) is NOT a GC object */
            /* Upval region: kyValue upvals[0..upval_count-1] */
            /* Scan upvals for GC heap pointers */
            kyValue *upvals = (kyValue *)obj->data;
            /* upval_count is stored in pad field of header */
            uint32_t upval_count = obj->pad;
            for (uint32_t i = 0; i < upval_count && i < 64; i++) {
                kyValue *v = &upvals[i];
                if (v->type == KYT_STRING && ky_gc_is_gc_str(vm, v->as.sval)) {
                    kyGcObject *child = (kyGcObject *)((uint8_t *)v->as.sval - sizeof(kyGcObject));
                    mark_object(vm, child);
                } else if (v->type == KYT_ARRAY && v->as.arr) {
                    kyGcObject *child = (kyGcObject *)((uint8_t *)v->as.arr - sizeof(kyGcObject));
                    mark_object(vm, child);
                }
            }
            break;
        }
        case KY_GC_OBJ_HEADER:
            break;
    }
}

/* Collect roots from VM state */
static void collect_roots(struct kyVM *vm, uint8_t **out, uint32_t *count, uint32_t cap) {
    *count = 0;
    KyGcHeap *gc = &vm->gc;

    /* Register the active stack */
    for (int i = 0; i < vm->stack_top && *count < cap; i++) {
        kyValue *v = &vm->stack[i];
        if (v->type == KYT_STRING) {
            if (ky_gc_is_gc_str(vm, v->as.sval))
                out[(*count)++] = (uint8_t *)v->as.sval;
        } else if (v->type == KYT_ARRAY && v->as.arr) {
            if (ky_gc_is_gc_str(vm, v->as.arr))
                out[(*count)++] = (uint8_t *)v->as.arr;
        }
    }

    /* Global variables */
    for (int i = 0; i < vm->gvar_count && *count < cap; i++) {
        kyValue *v = &vm->gvar_vals[i];
        if (v->type == KYT_STRING) {
            if (ky_gc_is_gc_str(vm, v->as.sval))
                out[(*count)++] = (uint8_t *)v->as.sval;
        } else if (v->type == KYT_ARRAY && v->as.arr) {
            if (ky_gc_is_gc_str(vm, v->as.arr))
                out[(*count)++] = (uint8_t *)v->as.arr;
        }
    }

    /* Host-registered GC roots */
    for (int i = 0; i < vm->gc_root_count && *count < cap; i++) {
        out[(*count)++] = (uint8_t *)vm->gc_roots[i];
    }

    /* Barrier log entries (old->new reverse edges) */
    for (uint32_t i = 0; i < gc->barrier_count && *count < cap; i++) {
        out[(*count)++] = gc->barrier_log[i];
    }
}

/* ---------------------------------------------------------------------------
 * ky_gc_handle_barrier — mark all old-gen objects referenced by barrier log
 * --------------------------------------------------------------------------- */
void ky_gc_handle_barrier(struct kyVM *vm) {
    KyGcHeap *gc = &vm->gc;
    for (uint32_t i = 0; i < gc->barrier_count; i++) {
        uint8_t *ptr = gc->barrier_log[i];
        kyGcObject *obj = (kyGcObject *)(ptr - sizeof(kyGcObject));
        mark_object(vm, obj);
    }
    gc->barrier_count = 0;
}

/* ---------------------------------------------------------------------------
 * ky_gc_run_nursery — mark-and-sweep nursery, promote old objects
 * --------------------------------------------------------------------------- */
size_t ky_gc_run_nursery(struct kyVM *vm) {
    KyGcHeap *gc = &vm->gc;
    if (!gc->nursery) return 0;

    int64_t t0 = now_ms();

    /* 1. Clear all marks */
    /* Nursery */
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= gc->nursery_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->nursery + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }
    /* Tenured */
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= gc->tenured_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->tenured + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }

    /* 2. Handle barrier log (mark old->new reverse edges) */
    ky_gc_handle_barrier(vm);

    /* 3. Mark from roots */
    uint8_t *roots[KY_MAX_GC_ROOTS + 512];
    uint32_t root_count = 0;
    collect_roots(vm, roots, &root_count, KY_MAX_GC_ROOTS + 512);
    for (uint32_t i = 0; i < root_count; i++) {
        kyGcObject *obj = (kyGcObject *)(roots[i] - sizeof(kyGcObject));
        mark_object(vm, obj);
    }

    /* 4. Compact nursery */
    uint32_t write = 0;
    uint32_t old_used = gc->nursery_used;
    int promoted = 0, freed = 0;

    for (uint32_t off = 0; off + sizeof(kyGcObject) <= old_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->nursery + off);
        uint32_t total = obj->size;
        if (total == 0 || off + total > old_used) break;
        off = (off + total + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) off = total;

        if (obj->type & KY_GC_OBJ_MARKED) {
            /* Promote if age threshold reached */
            obj->age++;
            if (obj->age >= KY_GC_PROMOTE_AGE) {
                /* Try tenured */
                if (gc->tenured_used + total <= gc->tenured_size) {
                    kyGcObject *dst = (kyGcObject *)(gc->tenured + gc->tenured_used);
                    memcpy(dst, obj, total);
                    dst->age = 0;
                    dst->type &= ~KY_GC_OBJ_MARKED;
                    gc->tenured_used += total;
                    promoted++;
                    continue; /* skip copying to nursery */
                } else {
                    gc->nursery_degraded++;
                }
            }
            /* Keep in nursery */
            if (write != off) {
                kyGcObject *dst = (kyGcObject *)(gc->nursery + write);
                memcpy(dst, obj, total);
            }
            write += total;
            /* keep mark for further use in this cycle */
        } else {
            freed++;
        }
    }

    /* Clear all remaining marks */
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= write; ) {
        kyGcObject *obj = (kyGcObject *)(gc->nursery + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }

    gc->nursery_used = write;
    gc->gc_count_n++;
    int64_t dt = now_ms() - t0;
    gc->last_gc_time_ms = (int)dt;

    size_t freed_bytes = (size_t)(old_used - write) + (size_t)promoted * sizeof(kyGcObject);
    gc->freed_bytes += freed_bytes;

    /* Suppression: two consecutive GCs that freed nothing */
    if (freed == 0 && promoted == 0) {
        gc->gc_suppressed = 1;
    } else {
        gc->gc_suppressed = 0;
    }

    return freed_bytes;
}

/* ---------------------------------------------------------------------------
 * ky_gc_run_full — mark-and-sweep entire heap (nursery + tenured)
 * --------------------------------------------------------------------------- */
size_t ky_gc_run_full(struct kyVM *vm) {
    KyGcHeap *gc = &vm->gc;
    if (!gc->nursery || !gc->tenured) return 0;

    int64_t t0 = now_ms();

    /* Clear all marks in both generations */
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= gc->nursery_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->nursery + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= gc->tenured_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->tenured + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }

    /* Barrier log */
    ky_gc_handle_barrier(vm);

    /* Mark from roots */
    uint8_t *roots[KY_MAX_GC_ROOTS + 512];
    uint32_t root_count = 0;
    collect_roots(vm, roots, &root_count, KY_MAX_GC_ROOTS + 512);
    for (uint32_t i = 0; i < root_count; i++) {
        kyGcObject *obj = (kyGcObject *)(roots[i] - sizeof(kyGcObject));
        mark_object(vm, obj);
    }

    /* Compact nursery */
    uint32_t nw = 0;
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= gc->nursery_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->nursery + off);
        uint32_t total = obj->size;
        if (total == 0 || off + total > gc->nursery_used) break;
        off = (off + total + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) off = total;
        if (obj->type & KY_GC_OBJ_MARKED) {
            if (nw != off) {
                kyGcObject *dst = (kyGcObject *)(gc->nursery + nw);
                memcpy(dst, obj, total);
            }
            nw += total;
        }
    }
    gc->nursery_used = nw;

    /* Compact tenured */
    uint32_t tw = 0;
    size_t freed_bytes = 0;
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= gc->tenured_used; ) {
        kyGcObject *obj = (kyGcObject *)(gc->tenured + off);
        uint32_t total = obj->size;
        if (total == 0 || off + total > gc->tenured_used) break;
        off = (off + total + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) off = total;
        if (obj->type & KY_GC_OBJ_MARKED) {
            if (tw != off) {
                kyGcObject *dst = (kyGcObject *)(gc->tenured + tw);
                memcpy(dst, obj, total);
            }
            tw += total;
        } else {
            freed_bytes += total;
        }
    }
    gc->tenured_used = tw;

    /* Clear remaining marks */
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= nw; ) {
        kyGcObject *obj = (kyGcObject *)(gc->nursery + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }
    for (uint32_t off = 0; off + sizeof(kyGcObject) <= tw; ) {
        kyGcObject *obj = (kyGcObject *)(gc->tenured + off);
        off = (off + obj->size + (KY_GC_OBJ_ALIGN - 1)) & ~(uint32_t)(KY_GC_OBJ_ALIGN - 1);
        if (off == 0) break;
        obj->type &= ~KY_GC_OBJ_MARKED;
    }

    gc->gc_count_t++;
    int64_t dt = now_ms() - t0;
    gc->last_gc_time_ms = (int)dt;

    gc->freed_bytes += freed_bytes;
    return freed_bytes;
}

/* ---------------------------------------------------------------------------
 * ky_vm_gc_collect — request GC, returns 0 if deferred to next safe point
 * --------------------------------------------------------------------------- */
size_t ky_vm_gc_collect(struct kyVM *vm) {
    if (!vm) return 0;
    vm->gc_collect_requested = 1;
    /* If VM is not currently executing (stack_top == 0 or not in a call),
       we can run it immediately. */
    if (vm->call_depth == 0) {
        size_t freed = ky_gc_run_full(vm);
        vm->gc_collect_requested = 0;
        return freed;
    }
    return 0; /* will be run at next safe point */
}

/* ---------------------------------------------------------------------------
 * ky_vm_gc_stats
 * --------------------------------------------------------------------------- */
int ky_vm_gc_stats(struct kyVM *vm, KyGcStats *out) {
    if (!vm || !out) return -1;
    KyGcHeap *gc = &vm->gc;
    out->nursery_used_bytes       = gc->nursery_used;
    out->nursery_capacity_bytes   = gc->nursery_size;
    out->tenured_used_bytes       = gc->tenured_used;
    out->tenured_capacity_bytes   = gc->tenured_size;
    out->total_gc_count           = gc->gc_count_n;
    out->total_t_gen_gc_count     = gc->gc_count_t;
    out->total_freed_bytes        = gc->freed_bytes;
    out->nursery_degraded_count   = gc->nursery_degraded;
    out->last_gc_time_ms          = gc->last_gc_time_ms;
    out->auto_disabled            = gc->auto_disabled;
    return 0;
}

/* ---------------------------------------------------------------------------
 * ky_vm_gc_mark / unmark — host-registered GC roots
 * --------------------------------------------------------------------------- */
int ky_vm_gc_mark(struct kyVM *vm, void *ptr) {
    if (!vm || !ptr) return -1;
    if (vm->gc_root_count >= KY_MAX_GC_ROOTS) return -1;
    vm->gc_roots[vm->gc_root_count++] = ptr;
    return 0;
}

int ky_vm_gc_unmark(struct kyVM *vm, void *ptr) {
    if (!vm || !ptr) return -1;
    for (int i = 0; i < vm->gc_root_count; i++) {
        if (vm->gc_roots[i] == ptr) {
            vm->gc_roots[i] = vm->gc_roots[vm->gc_root_count - 1];
            vm->gc_roots[vm->gc_root_count - 1] = 0;
            vm->gc_root_count--;
            return 0;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * ky_vm_gc_set_auto_threshold
 * --------------------------------------------------------------------------- */
int ky_vm_gc_set_auto_threshold(struct kyVM *vm, uint32_t trigger_bytes) {
    if (!vm) return -1;
    vm->gc.nursery_trigger = trigger_bytes;
    vm->gc.auto_disabled   = (trigger_bytes == 0);
    return 0;
}
