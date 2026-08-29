#ifndef KRONYX_SCRIPT_H
#define KRONYX_SCRIPT_H

#include "defines.h"
#include <stddef.h>
#include <stdint.h>

#define KYX_MAX_TOKENS   8192
#define KYX_MAX_LINE_LEN 4096
#define KYX_MAX_NESTING  64
#define KYX_MAX_UPVALS   32
#define KYX_MAX_FIELDS   64
#define KYX_MAX_PARAMS   32
#define KYX_MAX_LOCALS   64
#define KYX_MAX_VARS     256
#define KYX_MAX_STRINGS  4096
#define KYX_MAX_PROTOS   256
#define KYX_MAX_REGISTRY 512
#define KYX_MAX_CALLS    256
#define KYX_REG_COUNT    32
#define KYX_STACK_SIZE   256

typedef struct kyVM kyVM;
typedef struct kyLexer kyLexer;
typedef struct kyParser kyParser;
typedef struct kyAstNode kyAstNode;
typedef struct kyProto kyProto;

typedef enum kyValType {
    KYT_NIL = 0, KYT_BOOL, KYT_INT, KYT_FLOAT, KYT_STRING,
    KYT_FUNCTION, KYT_ARRAY, KYT_NATIVE,
} kyValType;

typedef struct kyValue {
    kyValType type;
    union {
        int64_t     ival;
        double      fval;
        const char *sval;
        kyVM       *closure;
        void       *arr;
        void       *native;
    } as;
} kyValue;

typedef enum kyTokenKind {
    KYX_TK_EOF       = 0,
    KYX_TK_IDENT,
    KYX_TK_INT_LIT,
    KYX_TK_FLOAT_LIT,
    KYX_TK_STRING_LIT,
    KYX_TK_BOOL_LIT,
    KYX_TK_NIL_LIT,
    KYX_TK_PLUS,      KYX_TK_MINUS,
    KYX_TK_STAR,      KYX_TK_SLASH,       KYX_TK_MOD,
    KYX_TK_EQ,        KYX_TK_NEQ,         KYX_TK_LT,
    KYX_TK_LE,        KYX_TK_GT,          KYX_TK_GE,
    KYX_TK_AND,       KYX_TK_OR,          KYX_TK_NOT,
    KYX_TK_BNOT,      KYX_TK_SHL,         KYX_TK_SHR,
    KYX_TK_ASSIGN,    KYX_TK_PLUSEQ,      KYX_TK_MINUSEQ,
    KYX_TK_STAREQ,    KYX_TK_DIVEQ,       KYX_TK_MODEQ,
    KYX_TK_INC,       KYX_TK_DEC,
    KYX_TK_LPAREN,    KYX_TK_RPAREN,
    KYX_TK_LBRACE,    KYX_TK_RBRACE,
    KYX_TK_LBRACK,    KYX_TK_RBRACK,
    KYX_TK_DOT,       KYX_TK_COMMA,       KYX_TK_SEMI,
    KYX_TK_COLON,
    KYX_TK_QUESTION,
    KYX_TK_FUNCTION,  KYX_TK_FUN,
    KYX_TK_CLASS,
    KYX_TK_VAR,       KYX_TK_LET,         KYX_TK_CONST,
    KYX_TK_IF,        KYX_TK_ELSE,
    KYX_TK_WHILE,     KYX_TK_FOR,
    KYX_TK_BREAK,     KYX_TK_CONTINUE,
    KYX_TK_RETURN,
    KYX_TK_NEW,
    KYX_TK_USE,       KYX_TK_NAMESPACE,
    KYX_TK_IMPORT,    KYX_TK_EXPORT,
    KYX_TK_SELF,      KYX_TK_SUPER,
    KYX_TK_TRUE,      KYX_TK_FALSE,
    KYX_TK_ERROR,
} kyTokenKind;

typedef struct kyToken {
    kyTokenKind kind;
    const char *start;
    size_t      len;
    int         line;
    int         col;
    union {
        int64_t  ival;
        double   fval;
        const char *sval;
    } as;
} kyToken;

typedef kyValue (*kyNativeFn)(struct kyVM *vm, kyValue *args, int argc, void *user);

KY_API kyLexer   *kyx_lexer_create(const char *src, const char *name);
KY_API void       kyx_lexer_destroy(kyLexer *lx);
KY_API kyToken    kyx_lexer_next(kyLexer *lx);
KY_API kyToken    kyx_lexer_peek(kyLexer *lx);
KY_API const char *kyx_lexer_error(const kyLexer *lx);
KY_API int         kyx_lexer_has_error(const kyLexer *lx);

KY_API kyParser  *kyx_parser_create(void *stream);
KY_API void       kyx_parser_destroy(kyParser *p);
KY_API kyAstNode *kyx_parser_parse(kyParser *p);
KY_API const char *kyx_parser_error(const kyParser *p);
KY_API int         kyx_parser_error_count(const kyParser *p);

KY_API kyVM   *ky_vm_create(const void *info);
KY_API void    ky_vm_destroy(kyVM *vm);
KY_API int     ky_vm_load_string(kyVM *vm, const char *src, const char *name);
KY_API int     ky_vm_load_file(kyVM *vm, const char *path);
KY_API int     ky_vm_call(kyVM *vm, const char *func_name, kyValue *args, int argc, kyValue *ret);
KY_API void    ky_vm_register_native(kyVM *vm, const char *ns, const char *name, kyNativeFn fn, void *user);
KY_API const char *ky_vm_last_error(kyVM *vm);
KY_API void    ky_vm_set_import_root(kyVM *vm, const char *dir);

/* Compiler: AST → bytecode */
KY_API kyProto *kyx_compile(kyVM *vm, kyAstNode *root, char *err_buf, int err_buf_size);

#endif
