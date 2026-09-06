#include "kronyx/pool.h"
#include <string.h>
#include <stdint.h>

#define KY_POOL_MIN_CAP 16u
#define KY_POOL_GROW_FACTOR 2u

static size_t pool_slot_size_raw(size_t requested) {
    size_t s = requested;
    if (s < sizeof(kyPoolSlot)) s = sizeof(kyPoolSlot);
    /* Keep slots 8-byte aligned for natural alignment on most platforms */
    if (s & 7u) s = (s + 7u) & ~7u;
    return s;
}

kyPool ky_pool_create(size_t slot_size, size_t initial_cap, kyAllocator *alloc) {
    if (initial_cap < KY_POOL_MIN_CAP) initial_cap = KY_POOL_MIN_CAP;
    kyPool p;
    p.alloc = *alloc;
    p.slot_size = pool_slot_size_raw(slot_size);
    p.count = 0;
    p.cap = initial_cap;
    size_t meta_size = p.cap * sizeof(kyPoolSlot);
    size_t data_size = p.cap * p.slot_size;
    /* Allocate one contiguous block: metadata first, then data. */
    size_t total = meta_size + data_size;
    uint8_t *raw = (uint8_t *)ky_mem_alloc(alloc, total);
    if (!raw) {
        p.slots = NULL;
        p.free_list = NULL;
        p.head = 0xFFFFFFFFu;
        return p;
    }
    p.slots = (void *)(raw + meta_size);
    p.free_list = (uint32_t *)raw;
    p.head = 0;
    for (size_t i = 0; i < p.cap; i++) {
        p.free_list[i] = (uint32_t)(i + 1);
    }
    p.free_list[p.cap - 1] = 0xFFFFFFFFu;
    return p;
}

void ky_pool_destroy(kyPool *p) {
    if (!p || !p->free_list) return;
    ky_mem_free(&p->alloc, p->free_list);
    p->slots = NULL;
    p->free_list = NULL;
    p->head = 0xFFFFFFFFu;
    p->cap = 0;
    p->count = 0;
}

void ky_pool_reset(kyPool *p) {
    if (!p) return;
    p->count = 0;
    p->head = 0;
    for (size_t i = 0; i < p->cap; i++) {
        p->free_list[i] = (uint32_t)(i + 1);
    }
    p->free_list[p->cap - 1] = 0xFFFFFFFFu;
}

static int pool_grow(kyPool *p) {
    size_t old_cap = p->cap;
    size_t new_cap = old_cap * KY_POOL_GROW_FACTOR;
    size_t old_meta = old_cap * sizeof(uint32_t);
    size_t new_meta = new_cap * sizeof(uint32_t);
    size_t total = new_meta + new_cap * p->slot_size;
    uint8_t *old_raw = (uint8_t *)p->free_list;
    uint8_t *raw = (uint8_t *)ky_mem_alloc(&p->alloc, total);
    if (!raw) return 0;
    memcpy(raw, old_raw, old_meta);
    memcpy(raw + new_meta, old_raw + old_meta, old_cap * p->slot_size);
    ky_mem_free(&p->alloc, old_raw);
    p->free_list = (uint32_t *)raw;
    p->slots = (void *)(raw + new_meta);
    uint32_t old_head = p->head;
    for (size_t i = old_cap; i < new_cap; i++)
        p->free_list[i] = (uint32_t)(i + 1);
    p->free_list[new_cap - 1] = old_head;
    p->head = (uint32_t)old_cap;
    p->cap = new_cap;
    return 1;
}

void *ky_pool_alloc(kyPool *p) {
    if (!p) return NULL;
    if (p->head == 0xFFFFFFFFu) {
        if (!pool_grow(p)) return NULL;
    }
    uint32_t idx = p->head;
    p->head = p->free_list[idx];
    p->count++;
    return (uint8_t *)p->slots + (size_t)idx * p->slot_size;
}

void ky_pool_free(kyPool *p, void *ptr) {
    if (!p || !ptr) return;
    uint8_t *base = (uint8_t *)p->slots;
    if ((uint8_t *)ptr < base || (uint8_t *)ptr >= base + p->cap * p->slot_size) return;
    size_t idx = ((uint8_t *)ptr - base) / p->slot_size;
    if (idx >= p->cap) return;
    p->free_list[idx] = p->head;
    p->head = (uint32_t)idx;
    if (p->count > 0) p->count--;
}

size_t ky_pool_count(const kyPool *p) {
    return p ? p->count : 0;
}

size_t ky_pool_capacity(const kyPool *p) {
    return p ? p->cap : 0;
}
