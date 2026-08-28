#include "kronyx/array.h"

void ky_array_init(kyArray *a, kyAllocator *alloc, size_t elem_size, size_t initial_cap) {
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
    a->elem_size = elem_size;
    a->alloc = alloc;
    if (initial_cap > 0) {
        ky_array_reserve(a, initial_cap);
    }
}

void ky_array_deinit(kyArray *a) {
    ky_mem_free(a->alloc, a->data);
    a->data = NULL;
    a->len = a->cap = 0;
}

void ky_array_reserve(kyArray *a, size_t cap) {
    if (cap <= a->cap) return;
    size_t nc = a->cap ? a->cap : 8;
    while (nc < cap) nc *= 2;
    a->data = ky_mem_realloc(a->alloc, a->data, nc * a->elem_size);
    a->cap = nc;
}

void *ky_array_emplace(kyArray *a) {
    if (a->len >= a->cap) {
        ky_array_reserve(a, a->cap ? a->cap * 2 : 8);
    }
    return (char *)a->data + a->len++ * a->elem_size;
}

void *ky_array_push(kyArray *a, const void *elem) {
    void *slot = ky_array_emplace(a);
    if (elem) memcpy(slot, elem, a->elem_size);
    return slot;
}

void *ky_array_get(const kyArray *a, size_t index) {
    if (index >= a->len) return NULL;
    return (char *)a->data + index * a->elem_size;
}

void ky_array_remove_swap(kyArray *a, size_t index) {
    if (index >= a->len) return;
    char *base = (char *)a->data;
    if (index != a->len - 1) {
        memcpy(base + index * a->elem_size, base + (a->len - 1) * a->elem_size, a->elem_size);
    }
    a->len--;
}

void ky_array_clear(kyArray *a) {
    a->len = 0;
}
