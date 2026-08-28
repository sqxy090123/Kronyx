#pragma once
#include "kronyx/script.h"
#include <stdint.h>

typedef struct kyLexer {
    const char    *src;
    int            pos;
    int            line;
    int            col;
    int            has_error;
    char           error_msg[256];
    int            forced_comments_stripped;
} kyLexer;
