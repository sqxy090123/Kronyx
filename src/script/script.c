#include "kronyx/script.h"
#include "script_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char *const keywords[] = {
    "use", "namespace", "if", "else", "while", "for",
    "break", "continue", "return", "function", "fun",
    "class", "var", "let", "const", "true", "false",
    "nil", "self", "super", "new", "this",
    "import", "export", "&&", "||", "^", NULL
};

static const char *lookup_keyword(const char *start, size_t len) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if ((size_t)strlen(keywords[i]) == len &&
            memcmp(start, keywords[i], len) == 0) {
            return keywords[i];
        }
    }
    return NULL;
}

static kyTokenKind token_kind_from_name(const char *name) {
    if (strcmp(name, "use") == 0) return KYX_TK_USE;
    if (strcmp(name, "namespace") == 0) return KYX_TK_NAMESPACE;
    if (strcmp(name, "if") == 0) return KYX_TK_IF;
    if (strcmp(name, "else") == 0) return KYX_TK_ELSE;
    if (strcmp(name, "while") == 0) return KYX_TK_WHILE;
    if (strcmp(name, "for") == 0) return KYX_TK_FOR;
    if (strcmp(name, "break") == 0) return KYX_TK_BREAK;
    if (strcmp(name, "continue") == 0) return KYX_TK_CONTINUE;
    if (strcmp(name, "return") == 0) return KYX_TK_RETURN;
    if (strcmp(name, "function") == 0 || strcmp(name, "fun") == 0) return KYX_TK_FUNCTION;
    if (strcmp(name, "class") == 0) return KYX_TK_CLASS;
    if (strcmp(name, "var") == 0) return KYX_TK_VAR;
    if (strcmp(name, "let") == 0) return KYX_TK_LET;
    if (strcmp(name, "const") == 0) return KYX_TK_CONST;
    if (strcmp(name, "true") == 0) return KYX_TK_TRUE;
    if (strcmp(name, "false") == 0) return KYX_TK_FALSE;
    if (strcmp(name, "nil") == 0) return KYX_TK_NIL_LIT;
    if (strcmp(name, "self") == 0) return KYX_TK_SELF;
    if (strcmp(name, "super") == 0) return KYX_TK_SUPER;
    if (strcmp(name, "new") == 0) return KYX_TK_NEW;
    if (strcmp(name, "this") == 0) return KYX_TK_SELF;
    if (strcmp(name, "import") == 0) return KYX_TK_IMPORT;
    if (strcmp(name, "export") == 0) return KYX_TK_EXPORT;
    if (strcmp(name, "&&") == 0) return KYX_TK_AND;
    if (strcmp(name, "||") == 0) return KYX_TK_OR;
    if (strcmp(name, "^") == 0) return KYX_TK_BNOT;
    return KYX_TK_IDENT;
}

static void skip_forced_comments(kyLexer *lx) {
    const char *p = lx->src + lx->pos;
    while (1) {
        /* Look for opening marker [ <!-- */
        if (p[0] != '[' || p[1] != '/' || p[2] != '*' || p[3] != '<' ||
            p[4] != '!' || p[5] != '-') break;
        /* Look for closing marker -- > * / ] */
        const char *q = p + 6;
        int found = 0;
        while (q[0] && q[1] && q[2] && q[3] && q[4] && q[5]) {
            if (q[0] == '-' && q[1] == '-' && q[2] == '>' && q[3] == '*' && q[4] == '/' && q[5] == ']') {
                found = 1;
                break;
            }
            q++;
        }
        if (!found) {
            lx->has_error = 1;
            snprintf(lx->error_msg, sizeof(lx->error_msg),
                     "unterminated forced comment at line %d", lx->line);
            return;
        }
        p = q + 6;
    }
    lx->pos = (int)(p - lx->src);
}

static void skip_line_comment(kyLexer *lx) {
    const char *p = lx->src + lx->pos;
    while (*p && *p != '\n') p++;
    lx->pos = (int)(p - lx->src);
    if (*p == '\n') { lx->pos++; lx->line++; lx->col = 0; }
}

static void skip_block_comment(kyLexer *lx) {
    const char *p = lx->src + lx->pos;
    while (*p && !(*p == '*' && *(p+1) == '/')) p++;
    if (*p == '*' && *(p+1) == '/') p += 2;
    lx->pos = (int)(p - lx->src);
}

static void advance(kyLexer *lx) {
    if (lx->src[lx->pos] == '\n') { lx->line++; lx->col = 0; }
    else { lx->col++; }
    lx->pos++;
}

static int peek(kyLexer *lx) {
    return lx->src[lx->pos];
}

static int peek_n(kyLexer *lx, int n) {
    return lx->src[lx->pos + n];
}

static kyToken make_token(kyLexer *lx, kyTokenKind kind, int start_pos, int start_line, int start_col) {
    kyToken t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.start = lx->src + start_pos;
    t.len = (size_t)(lx->pos - start_pos);
    t.line = start_line;
    t.col = start_col;
    return t;
}

kyLexer *kyx_lexer_create(const char *src, const char *name) {
    KY_UNUSED(name);
    kyLexer *lx = (kyLexer *)malloc(sizeof(kyLexer));
    if (!lx) return NULL;
    lx->src = src;
    lx->pos = 0;
    lx->line = 1;
    lx->col = 0;
    lx->has_error = 0;
    lx->error_msg[0] = '\0';
    lx->forced_comments_stripped = 0;
    return lx;
}

void kyx_lexer_destroy(kyLexer *lx) {
    if (lx) free(lx);
}

const char *kyx_lexer_error(const kyLexer *lx) {
    return lx ? lx->error_msg : "no lexer";
}

int kyx_lexer_has_error(const kyLexer *lx) {
    return lx && lx->has_error;
}

kyToken kyx_lexer_next(kyLexer *lx) {
    kyToken t;
    memset(&t, 0, sizeof(t));
    t.kind = KYX_TK_EOF;
    t.line = lx->line;
    t.col = lx->col;

    if (!lx || !lx->src[lx->pos]) return t;

    int start_pos = lx->pos;
    int start_line = lx->line;
    int start_col = lx->col;
    int c;

    while (1) {
        c = peek(lx);
        if (c == '\0') {
            return make_token(lx, KYX_TK_EOF, start_pos, start_line, start_col);
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lx); continue;
        }
        if (c == '\n') {
            advance(lx); continue;
        }
        if (c == '/' && peek_n(lx, 1) == '/') {
            skip_line_comment(lx); continue;
        }
        if (c == '/' && peek_n(lx, 1) == '*') {
            skip_block_comment(lx); continue;
        }
        /* Check for forced comments */
        if (c == '[' && peek_n(lx, 1) == '/' && peek_n(lx, 2) == '*' && peek_n(lx, 3) == '<' && peek_n(lx, 4) == '!' && peek_n(lx, 5) == '-') {
            skip_forced_comments(lx);
            if (lx->has_error) return t;
            continue;
        }
        break;
    }

    start_pos = lx->pos;
    start_line = lx->line;
    start_col = lx->col;
    c = peek(lx);

    if (isdigit(c) || (c == '.' && isdigit(peek_n(lx, 1)))) {
        int got_dot = 0;
        if (c == '0' && peek_n(lx, 1) == 'x') {
            /* hex integer: 0xFF */
            advance(lx); advance(lx);
            while (isxdigit(peek(lx))) advance(lx);
            t = make_token(lx, KYX_TK_INT_LIT, start_pos, start_line, start_col);
            t.as.ival = (int64_t)strtoll(lx->src + start_pos, NULL, 16);
            return t;
        }
        if (c == '.') { got_dot = 1; advance(lx); }
        while (isdigit(peek(lx))) advance(lx);
        if (!got_dot) {
            if (peek(lx) == '.') {
                got_dot = 1; advance(lx);
                while (isdigit(peek(lx))) advance(lx);
            }
        }
        if (got_dot) {
            t = make_token(lx, KYX_TK_FLOAT_LIT, start_pos, start_line, start_col);
            t.as.fval = strtod(lx->src + start_pos, NULL);
        } else {
            t = make_token(lx, KYX_TK_INT_LIT, start_pos, start_line, start_col);
            t.as.ival = (int64_t)strtoll(lx->src + start_pos, NULL, 10);
        }
        return t;
    }

    if (c == '"') {
        advance(lx);
        int str_start = lx->pos;
        while (peek(lx) != '"' && peek(lx) != '\0') {
            if (peek(lx) == '\\') advance(lx);
            advance(lx);
        }
        if (peek(lx) == '"') advance(lx);
        else {
            lx->has_error = 1;
            snprintf(lx->error_msg, sizeof(lx->error_msg),
                     "unterminated string literal at line %d", start_line);
            return make_token(lx, KYX_TK_ERROR, start_pos, start_line, start_col);
        }
        t = make_token(lx, KYX_TK_STRING_LIT, start_pos, start_line, start_col);
        t.as.sval = lx->src + str_start;
        return t;
    }

    if (isalpha(c) || c == '_') {
        while (isalnum(peek(lx)) || peek(lx) == '_') advance(lx);
        const char *kw = lookup_keyword(lx->src + start_pos,
                                        (size_t)(lx->pos - start_pos));
        if (kw) {
            t = make_token(lx, token_kind_from_name(kw), start_pos, start_line, start_col);
        } else {
            t = make_token(lx, KYX_TK_IDENT, start_pos, start_line, start_col);
        }
        return t;
    }

    switch (c) {
        case '+': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_PLUSEQ, start_pos, start_line, start_col); }
            if (peek(lx) == '+') { advance(lx); return make_token(lx, KYX_TK_INC, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_PLUS, start_pos, start_line, start_col);
        case '-': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_MINUSEQ, start_pos, start_line, start_col); }
            if (peek(lx) == '-') { advance(lx); return make_token(lx, KYX_TK_DEC, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_MINUS, start_pos, start_line, start_col);
        case '*': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_STAREQ, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_STAR, start_pos, start_line, start_col);
        case '/': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_DIVEQ, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_SLASH, start_pos, start_line, start_col);
        case '%': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_MODEQ, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_MOD, start_pos, start_line, start_col);
        case '=': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_EQ, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_ASSIGN, start_pos, start_line, start_col);
        case '!': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_NEQ, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_NOT, start_pos, start_line, start_col);
        case '<': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_LE, start_pos, start_line, start_col); }
            if (peek(lx) == '<') { advance(lx); if (peek(lx) == '=') advance(lx); return make_token(lx, KYX_TK_SHL, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_LT, start_pos, start_line, start_col);
        case '>': advance(lx);
            if (peek(lx) == '=') { advance(lx); return make_token(lx, KYX_TK_GE, start_pos, start_line, start_col); }
            if (peek(lx) == '>') { advance(lx); if (peek(lx) == '=') advance(lx); return make_token(lx, KYX_TK_SHR, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_GT, start_pos, start_line, start_col);
        case '&': advance(lx);
            if (peek(lx) == '&') { advance(lx); return make_token(lx, KYX_TK_AND, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_BAND, start_pos, start_line, start_col);
        case '|': advance(lx);
            if (peek(lx) == '|') { advance(lx); return make_token(lx, KYX_TK_OR, start_pos, start_line, start_col); }
            return make_token(lx, KYX_TK_BOR, start_pos, start_line, start_col);
        case '^': advance(lx);
            return make_token(lx, KYX_TK_BXOR, start_pos, start_line, start_col);
        case '~': advance(lx);
            return make_token(lx, KYX_TK_BNOT, start_pos, start_line, start_col);
        case '(': advance(lx); return make_token(lx, KYX_TK_LPAREN, start_pos, start_line, start_col);
        case ')': advance(lx); return make_token(lx, KYX_TK_RPAREN, start_pos, start_line, start_col);
        case '{': advance(lx); return make_token(lx, KYX_TK_LBRACE, start_pos, start_line, start_col);
        case '}': advance(lx); return make_token(lx, KYX_TK_RBRACE, start_pos, start_line, start_col);
        case '[': advance(lx); return make_token(lx, KYX_TK_LBRACK, start_pos, start_line, start_col);
        case ']': advance(lx); return make_token(lx, KYX_TK_RBRACK, start_pos, start_line, start_col);
        case '.': advance(lx); return make_token(lx, KYX_TK_DOT, start_pos, start_line, start_col);
        case ',': advance(lx); return make_token(lx, KYX_TK_COMMA, start_pos, start_line, start_col);
        case ';': advance(lx); return make_token(lx, KYX_TK_SEMI, start_pos, start_line, start_col);
        case ':': advance(lx); return make_token(lx, KYX_TK_COLON, start_pos, start_line, start_col);
        case '?': advance(lx); return make_token(lx, KYX_TK_QUESTION, start_pos, start_line, start_col);
        default:
            lx->has_error = 1;
            snprintf(lx->error_msg, sizeof(lx->error_msg),
                     "unexpected character '%c' at line %d col %d", c, start_line, start_col);
            return make_token(lx, KYX_TK_ERROR, start_pos, start_line, start_col);
    }
}

kyToken kyx_lexer_peek(kyLexer *lx) {
    if (!lx) {
        kyToken t; memset(&t, 0, sizeof(t)); t.kind = KYX_TK_EOF; return t;
    }
    int saved = lx->pos;
    int saved_line = lx->line;
    int saved_col = lx->col;
    kyToken t = kyx_lexer_next(lx);
    lx->pos = saved;
    lx->line = saved_line;
    lx->col = saved_col;
    return t;
}
