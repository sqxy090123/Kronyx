#ifndef KRONYX_STRING_H
#define KRONYX_STRING_H

#include "defines.h"
#include "memory.h"

typedef struct kyString {
    char *data;
    size_t len;
    size_t cap;
    kyAllocator *alloc;
} kyString;

KY_API void ky_string_init(kyString *s, kyAllocator *alloc);
KY_API void ky_string_deinit(kyString *s);
KY_API void ky_string_reserve(kyString *s, size_t cap);
KY_API void ky_string_append(kyString *s, const char *text);
KY_API void ky_string_append_n(kyString *s, const char *text, size_t n);
KY_API void ky_string_appendf(kyString *s, const char *fmt, ...);
KY_API void ky_string_clear(kyString *s);
KY_API const char *ky_string_cstr(const kyString *s);

#endif
