#include "kronyx/memory.h"

typedef struct kyTrackingState {
    size_t alloc_count;
    size_t free_count;
    size_t live_bytes;
    size_t peak_bytes;
} kyTrackingState;

static void *def_alloc(void *ud, size_t size) {
    KY_UNUSED(ud);
    return malloc(size ? size : 1);
}

static void *def_realloc(void *ud, void *ptr, size_t size) {
    KY_UNUSED(ud);
    return realloc(ptr, size ? size : 1);
}

static void def_free(void *ud, void *ptr) {
    KY_UNUSED(ud);
    free(ptr);
}

kyAllocator ky_default_allocator(void) {
    kyAllocator a;
    a.user_data = NULL;
    a.alloc = def_alloc;
    a.realloc_fn = def_realloc;
    a.free_fn = def_free;
    return a;
}

#define KY_TRACK_HDR 16u

static void *track_alloc(void *ud, size_t size) {
    kyTrackingState *st = (kyTrackingState *)ud;
    size_t payload = size ? size : 1;
    uint8_t *raw = (uint8_t *)malloc(KY_TRACK_HDR + payload);
    if (!raw) return NULL;
    *(size_t *)raw = payload;
    st->alloc_count++;
    st->live_bytes += payload;
    if (st->live_bytes > st->peak_bytes) st->peak_bytes = st->live_bytes;
    return raw + KY_TRACK_HDR;
}

static void track_free(void *ud, void *ptr) {
    kyTrackingState *st = (kyTrackingState *)ud;
    if (!ptr) return;
    uint8_t *raw = (uint8_t *)ptr - KY_TRACK_HDR;
    size_t old = *(size_t *)raw;
    if (st->live_bytes >= old) st->live_bytes -= old;
    else st->live_bytes = 0;
    st->free_count++;
    free(raw);
}

static void *track_realloc(void *ud, void *ptr, size_t size) {
    if (!ptr) return track_alloc(ud, size);
    if (size == 0) {
        track_free(ud, ptr);
        return track_alloc(ud, 1);
    }
    kyTrackingState *st = (kyTrackingState *)ud;
    uint8_t *raw = (uint8_t *)ptr - KY_TRACK_HDR;
    size_t old = *(size_t *)raw;
    uint8_t *nr = (uint8_t *)realloc(raw, KY_TRACK_HDR + size);
    if (!nr) return NULL;
    *(size_t *)nr = size;
    if (st->live_bytes >= old) st->live_bytes -= old;
    else st->live_bytes = 0;
    st->live_bytes += size;
    if (st->live_bytes > st->peak_bytes) st->peak_bytes = st->live_bytes;
    return nr + KY_TRACK_HDR;
}

kyAllocator ky_tracking_allocator(kyMemStats *stats_out) {
    kyTrackingState *st = (kyTrackingState *)stats_out;
    memset(st, 0, sizeof(*st));
    kyAllocator a;
    a.user_data = st;
    a.alloc = track_alloc;
    a.realloc_fn = track_realloc;
    a.free_fn = track_free;
    return a;
}

void ky_mem_stats_snapshot(kyMemStats *out) {
    KY_UNUSED(out);
}

void *ky_mem_alloc(kyAllocator *a, size_t size) {
    return a->alloc(a->user_data, size);
}

void *ky_mem_realloc(kyAllocator *a, void *ptr, size_t size) {
    return a->realloc_fn(a->user_data, ptr, size);
}

void ky_mem_free(kyAllocator *a, void *ptr) {
    if (ptr) a->free_fn(a->user_data, ptr);
}

void *ky_mem_dup(kyAllocator *a, const void *src, size_t size) {
    void *p = ky_mem_alloc(a, size);
    if (p) memcpy(p, src, size);
    return p;
}
