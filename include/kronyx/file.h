#ifndef KRONYX_FILE_H
#define KRONYX_FILE_H

#include "defines.h"
#include <stddef.h>

/* File read API. Uses C fopen/fread internally so it stays portable.
 * The buffer is allocated with malloc; the matching release call is
 * ky_free_read. The two are intentionally NOT routed through kyAllocator
 * to keep this module usable from tests, tools and third-party code. */

typedef enum {
    KY_FILE_OK          = 0,
    KY_FILE_ERR_NOT_FOUND = -1, /* path missing, not a regular file, or fopen failed */
    KY_FILE_ERR_BAD_SIZE  = -2, /* fstat size > KY_FILE_MAX_BYTES, or no size available */
    KY_FILE_ERR_IO        = -3, /* short/failed read */
    KY_FILE_ERR_NOMEM     = -4, /* buffer alloc failed */
} kyFileCode;

KY_API kyFileCode ky_file_read(const char *path, void **buf_out, size_t *len_out);
KY_API void       ky_free_read(void *buf);

#endif
