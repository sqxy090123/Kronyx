#include "kronyx/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#else
#include <sys/stat.h>
#endif

static int is_regular_file(const char *path, long *size_out) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    if (size_out) *size_out = (long)st.st_size;
    return 1;
}

kyFileCode ky_file_read(const char *path, void **buf_out, size_t *len_out) {
    if (!path || !*path || !buf_out) return KY_FILE_ERR_BAD_SIZE;
    *buf_out = NULL;
    if (len_out) *len_out = 0;

    long size = 0;
    if (!is_regular_file(path, &size)) return KY_FILE_ERR_NOT_FOUND;
    if (size <= 0) {
        /* Zero-length is allowed: caller gets an empty buffer. */
        if (size < 0) return KY_FILE_ERR_BAD_SIZE;
    }
    if (size > (1LL << 28)) { /* 256 MiB */
        return KY_FILE_ERR_BAD_SIZE;
    }

    FILE *f = fopen(path, "rb");
    if (!f) return KY_FILE_ERR_NOT_FOUND;

    size_t cap = (size_t)size;
    char *buf = (char *)malloc(cap ? cap : 1);
    if (!buf) {
        fclose(f);
        return KY_FILE_ERR_NOMEM;
    }

    size_t got = 0;
    while (got < cap) {
        size_t r = fread(buf + got, 1, cap - got, f);
        if (r == 0) {
            int e = ferror(f);
            feof(f);
            if (e) {
                fclose(f);
                free(buf);
                return KY_FILE_ERR_IO;
            }
            /* EOF early: pad nothing, cap to what we have. */
            break;
        }
        got += r;
    }
    fclose(f);

    if (got < cap) {
        char *sh = (char *)realloc(buf, got ? got : 1);
        if (!sh) {
            free(buf);
            return KY_FILE_ERR_NOMEM;
        }
        buf = sh;
        cap = got;
    }

    *buf_out = buf;
    if (len_out) *len_out = cap;
    return KY_FILE_OK;
}

void ky_free_read(void *buf) {
    free(buf);
}
