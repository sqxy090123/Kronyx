#pragma once
#include "kronyx/script.h"
#include "kronyx/gc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct kyLexer {
    const char    *src;
    int            pos;
    int            line;
    int            col;
    int            has_error;
    char           error_msg[256];
    int            forced_comments_stripped;
} kyLexer;

typedef enum kyAstKind {
    KY_AST_PROGRAM, KY_AST_USE_DECL, KY_AST_VAR_DECL,
    KY_AST_FUNC_DECL, KY_AST_CLASS_DECL, KY_AST_IF_STMT,
    KY_AST_WHILE_STMT, KY_AST_FOR_STMT, KY_AST_RETURN_STMT,
    KY_AST_EXPR_STMT, KY_AST_BLOCK, KY_AST_EXPR_BINOP,
    KY_AST_EXPR_UNOP, KY_AST_EXPR_LITERAL, KY_AST_EXPR_IDENT,
    KY_AST_EXPR_CALL, KY_AST_EXPR_INDEX, KY_AST_EXPR_FIELD,
    KY_AST_EXPR_NEW, KY_AST_EXPR_ANON_FUNC, KY_AST_EXPR_TERNARY,
} kyAstKind;

typedef struct kyAstNode kyAstNode;
typedef struct kyAstProto kyAstProto;
typedef struct kyAstClass kyAstClass;

struct kyAstNode {
    kyAstKind kind;
    int       line;
    kyAstNode *next;
    union {
        struct { kyAstNode **children; int count; int cap; } program;
        struct { char *scope; char *path; char *ns; } use_decl;
        struct { char *name; kyAstNode *init; int is_const; } var_decl;
        struct { char *name; char **params; int param_count; kyAstNode *body; kyAstProto *proto; } func_decl;
        struct { char *name; char *parent; kyAstClass *klass; kyAstNode *body; } class_decl;
        struct { kyAstNode *cond; kyAstNode *then_b; kyAstNode *else_b; } if_stmt;
        struct { kyAstNode *cond; kyAstNode *body; } while_stmt;
        struct { kyAstNode *init; kyAstNode *cond; kyAstNode *inc; kyAstNode *body; } for_stmt;
        struct { kyAstNode *expr; } return_stmt;
        struct { kyAstNode *expr; } expr_stmt;
        struct { kyAstNode **stmts; int count; int cap; } block;
        struct { kyAstNode **exprs; int count; int cap; } expr_list;
        struct { kyAstNode *left; kyAstNode *right; char op[8]; int op_kind; } binop;
        struct { char op[4]; kyAstNode *operand; } unop;
        struct { kyToken tok; } literal;
        struct { char *name; } ident;
        struct { kyAstNode *callee; kyAstNode **args; int arg_count; } call;
        struct { kyAstNode *obj; kyAstNode *idx; } index;
        struct { kyAstNode *obj; char *field; } field;
        struct { char *class_name; kyAstNode **args; int arg_count; } new_expr;
        struct { char **params; int param_count; kyAstNode *body; kyAstProto *proto; } anon_func;
        struct { kyAstNode *cond; kyAstNode *true_b; kyAstNode *false_b; } ternary;
    } as;
};

struct kyAstProto {
    int *code;
    int  code_count;
    int  code_cap;
    double *constants;
    int   const_count;
    int   const_cap;
    char **strings;
    int   str_count;
    int   max_stack;
    int   param_count;
};

struct kyAstClass {
    char        name[128];
    char        parent[128];
    kyAstNode  **fields;
    int         field_count;
    int         field_cap;
};

/* Token stream for parser */
#define KYX_MAX_TOKENS 8192
typedef struct kyTokenStream {
    kyToken tokens[KYX_MAX_TOKENS];
    int     count;
    int     pos;
} kyTokenStream;

/* Parser structure */
typedef struct kyParser {
    kyTokenStream *stream;
    kyAstNode     *root;
    int            error_count;
    char           error_msg[256];
} kyParser;

/* VM-internal types needed by script_internal.h kyVM definition */
typedef struct kyProto {
    int       *code;
    int        code_count;
    int        code_cap;
    double    *constants;
    int        const_count;
    int        const_cap;
    char     **strings;
    int        str_count;
    int        str_cap;
    int        param_count;
    char      *name;
} kyProto;

typedef struct kyClosure {
    kyProto   *proto;
} kyClosure;

/* VM struct definition — moved here so gc.c can access vm->gc fields */
#define KY_MAX_STACK  512
#define KY_MAX_VARS   1024
#define KY_MAX_CALL_DEPTH 256

typedef struct kyNativeEntry {
    kyValue (*fn)(struct kyVM *, kyValue *args, int argc, void *user);
    void    *user;
    char     ns[64];
    char     name[64];
} kyNativeEntry;

struct kyVM {
    kyValue   stack[KY_MAX_STACK];
    char     *gvar_names[KY_MAX_VARS];
    kyValue   gvar_vals[KY_MAX_VARS];
    int       gvar_count;
    int       top_ran;
    kyProto   protos[KYX_MAX_PROTOS];
    kyClosure *closures[KYX_MAX_PROTOS];
    kyNativeEntry natives[KYX_MAX_REGISTRY];
    int       stack_top;
    int       call_depth;
    int       proto_count;
    int       native_count;
    char      error_msg[256];
    /* GC fields */
    KyGcHeap  gc;
    void     *gc_roots[KY_MAX_GC_ROOTS];
    int       gc_root_count;
    int       gc_collect_requested;
};
