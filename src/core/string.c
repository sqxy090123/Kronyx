#include "kronyx/string.h"
#include <stdarg.h>

void ky_string_init(kyString *s, kyAllocator *alloc) {
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
    s->alloc = alloc;
}

void ky_string_deinit(kyString *s) {
    ky_mem_free(s->alloc, s->data);
    s->data = NULL;
    s->len = s->cap = 0;
}

void ky_string_reserve(kyString *s, size_t cap) {
    if (cap <= s->cap) return;
    size_t nc = s->cap ? s->cap : 16;
    while (nc < cap) nc *= 2;
    s->data = (char *)ky_mem_realloc(s->alloc, s->data, nc);
    s->cap = nc;
}

void ky_string_append_n(kyString *s, const char *text, size_t n) {
    if (n == 0) return;
    ky_string_reserve(s, s->len + n + 1);
    memcpy(s->data + s->len, text, n);
    s->len += n;
    s->data[s->len] = '\0';
}

void ky_string_append(kyString *s, const char *text) {
    ky_string_append_n(s, text, strlen(text));
}

void ky_string_appendf(kyString *s, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return;
    }
    ky_string_reserve(s, s->len + (size_t)n + 1);
    vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    s->len += (size_t)n;
}

void ky_string_clear(kyString *s) {
    s->len = 0;
    if (s->data) s->data[0] = '\0';
}

const char *ky_string_cstr(const kyString *s) {
    if (!s->data) {
        return "";
    }
    return s->data;
}
