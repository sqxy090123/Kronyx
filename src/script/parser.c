#include "kronyx/script.h"
#include "script_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#define KYX_MAX_TOKENS 8192
#define KYX_ERR_LEN    256



static const char *const keywords[] = {
    "use","namespace","if","else","while","for","break","continue",
    "return","function","fun","class","var","let","const",
    "true","false","nil","self","super","new","this",
    "import","export", NULL
};

static kyAstNode *ast_new(kyAstKind kind, int line);
static void ast_free(kyAstNode *n);
static kyAstNode *parse_expression(kyParser *p, int prec);
static kyAstNode *parse_statement(kyParser *p);
static kyAstNode *parse_block_content(kyParser *p);
static kyAstNode *parse_block(kyParser *p);

static void parser_error(kyParser *p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->error_msg, sizeof(p->error_msg), fmt, ap);
    va_end(ap);
    p->error_count++;
}

static kyToken *tok_current(kyParser *p) {
    if (p->stream->pos >= p->stream->count)
        return &p->stream->tokens[p->stream->count];
    return &p->stream->tokens[p->stream->pos];
}

static kyToken *tok_advance(kyParser *p) {
    if (p->stream->pos < p->stream->count) p->stream->pos++;
    kyToken *t = tok_current(p);
    return t;
}

static int tok_at_eof(kyParser *p) {
    kyToken *t = tok_current(p);
    return t->kind == KYX_TK_EOF;
}

static kyToken *tok_consume(kyParser *p, kyTokenKind kind, const char *msg) {
    kyToken *t = tok_current(p);
    if (t->kind != kind) {
        parser_error(p, "%s (expected %s, got %s) at line %d",
                     msg, (kind==KYX_TK_IDENT?"identifier":"token"),
                     (t->kind==KYX_TK_IDENT?"identifier":"token"), t->line);
        return NULL;
    }
    tok_advance(p);
    return t;
}

static kyAstNode *ast_new(kyAstKind kind, int line) {
    kyAstNode *n = (kyAstNode *)calloc(1, sizeof(kyAstNode));
    if (!n) return NULL;
    n->kind = kind; n->line = line; n->next = NULL;
    return n;
}

static void ast_free_node(kyAstNode *n) {
    if (!n) return;
    switch (n->kind) {
        case KY_AST_USE_DECL:
            free(n->as.use_decl.scope); free(n->as.use_decl.path); free(n->as.use_decl.ns); break;
        case KY_AST_VAR_DECL:
            free(n->as.var_decl.name); ast_free(n->as.var_decl.init); break;
        case KY_AST_FUNC_DECL:
            free(n->as.func_decl.name);
            for (int i = 0; i < n->as.func_decl.param_count; i++) free(n->as.func_decl.params[i]);
            free(n->as.func_decl.params); ast_free(n->as.func_decl.body); free(n->as.func_decl.proto); break;
        case KY_AST_CLASS_DECL:
            free(n->as.class_decl.name); free(n->as.class_decl.parent); free(n->as.class_decl.klass); break;
        case KY_AST_IF_STMT: ast_free(n->as.if_stmt.cond); ast_free(n->as.if_stmt.then_b); ast_free(n->as.if_stmt.else_b); break;
        case KY_AST_WHILE_STMT: ast_free(n->as.while_stmt.cond); ast_free(n->as.while_stmt.body); break;
        case KY_AST_FOR_STMT: ast_free(n->as.for_stmt.init); ast_free(n->as.for_stmt.cond); ast_free(n->as.for_stmt.inc); ast_free(n->as.for_stmt.body); break;
        case KY_AST_RETURN_STMT: ast_free(n->as.return_stmt.expr); break;
        case KY_AST_EXPR_STMT: ast_free(n->as.expr_stmt.expr); break;
        case KY_AST_BLOCK:
            for (int i = 0; i < n->as.block.count; i++) ast_free(n->as.block.stmts[i]);
            free(n->as.block.stmts); break;
        case KY_AST_EXPR_FIELD: ast_free(n->as.field.obj); free(n->as.field.field); break;
        case KY_AST_EXPR_NEW:
            for (int i = 0; i < n->as.new_expr.arg_count; i++) ast_free(n->as.new_expr.args[i]);
            free(n->as.new_expr.args); break;
        case KY_AST_EXPR_ANON_FUNC:
            for (int i = 0; i < n->as.anon_func.param_count; i++) free(n->as.anon_func.params[i]);
            free(n->as.anon_func.params); ast_free(n->as.anon_func.body); free(n->as.anon_func.proto); break;
        case KY_AST_EXPR_TERNARY: ast_free(n->as.ternary.cond); ast_free(n->as.ternary.true_b); ast_free(n->as.ternary.false_b); break;
        default: break;
    }
    free(n);
}

static void ast_free(kyAstNode *n) {
    if (!n) return;
    if (n->next) ast_free(n->next);
    ast_free_node(n);
}

static int tok_is_ident(kyToken *t) { return t->kind == KYX_TK_IDENT; }
static int tok_is_keyword(kyToken *t, const char *kw) {
    return t->kind == KYX_TK_IDENT && t->len == strlen(kw) && memcmp(t->start, kw, t->len) == 0;
}
static int tok_is_numlit(kyToken *t) {
    return t->kind == KYX_TK_INT_LIT || t->kind == KYX_TK_FLOAT_LIT;
}

static kyAstNode *parse_primary(kyParser *p);

static kyAstNode *parse_call(kyParser *p, kyAstNode *callee) {
    kyAstNode *n = ast_new(KY_AST_EXPR_CALL, callee->line);
    n->as.call.callee = callee;
    n->as.call.arg_count = 0; n->as.call.args = NULL;
    while (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_RPAREN) {
        kyAstNode *arg = parse_expression(p, 1);
        if (arg) {
            int ac = n->as.call.arg_count++;
            n->as.call.args = (kyAstNode **)realloc(n->as.call.args, (size_t)n->as.call.arg_count * sizeof(kyAstNode *));
            n->as.call.args[ac] = arg;
        }
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_COMMA) tok_advance(p);
    }
    tok_consume(p, KYX_TK_RPAREN, "expected ')'");
    return n;
}

static kyAstNode *parse_postfix(kyParser *p, kyAstNode *base) {
    while (!tok_at_eof(p)) {
        kyToken *t = tok_current(p);
        if (t->kind == KYX_TK_DOT) {
            tok_advance(p);
            kyToken *fname = tok_consume(p, KYX_TK_IDENT, "expected field name");
            if (!fname) { ast_free(base); return NULL; }
            kyAstNode *n = ast_new(KY_AST_EXPR_FIELD, t->line);
            n->as.field.obj = base;
            n->as.field.field = (char *)malloc(fname->len + 1);
            if (n->as.field.field) { memcpy(n->as.field.field, fname->start, fname->len); n->as.field.field[fname->len] = '\0'; }
            base = n;
        } else if (t->kind == KYX_TK_LBRACK) {
            tok_advance(p);
            kyAstNode *idx = parse_expression(p, 0);
            tok_consume(p, KYX_TK_RBRACK, "expected ']'");
            kyAstNode *n = ast_new(KY_AST_EXPR_INDEX, t->line);
            n->as.index.obj = base; n->as.index.idx = idx;
            base = n;
        } else if (t->kind == KYX_TK_LPAREN) {
            tok_advance(p);
            base = parse_call(p, base);
        } else if (t->kind == KYX_TK_INC || t->kind == KYX_TK_DEC) {
            tok_advance(p);
            kyAstNode *n = ast_new(KY_AST_EXPR_UNOP, t->line);
            n->as.unop.op[0] = t->start[0]; n->as.unop.op[1] = '\0';
            n->as.unop.operand = base;
            base = n;
        } else break;
    }
    return base;
}

static kyAstNode *parse_prefix(kyParser *p) {
    kyToken *t = tok_current(p);
    if (t->kind == KYX_TK_MINUS || t->kind == KYX_TK_NOT) {
        tok_advance(p);
        kyAstNode *n = ast_new(KY_AST_EXPR_UNOP, t->line);
        n->as.unop.op[0] = t->start[0];
        n->as.unop.op[1] = '\0';
        n->as.unop.operand = parse_prefix(p);
        return n;
    }
    if (t->kind == KYX_TK_NEW) {
        tok_advance(p);
        kyToken *cname = tok_consume(p, KYX_TK_IDENT, "expected class name after 'new'");
        if (!cname) return NULL;
        kyAstNode *n = ast_new(KY_AST_EXPR_NEW, t->line);
        n->as.new_expr.class_name = (char *)malloc(cname->len + 1);
        if (n->as.new_expr.class_name) { memcpy(n->as.new_expr.class_name, cname->start, cname->len); n->as.new_expr.class_name[cname->len] = '\0'; }
        n->as.new_expr.arg_count = 0; n->as.new_expr.args = NULL;
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_LPAREN) {
            tok_advance(p);
            while (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_RPAREN) {
                kyAstNode *arg = parse_expression(p, 1);
                if (arg) {
                    int ac = n->as.new_expr.arg_count++;
                    n->as.new_expr.args = (kyAstNode **)realloc(n->as.new_expr.args, (size_t)n->as.new_expr.arg_count * sizeof(kyAstNode *));
                    n->as.new_expr.args[ac] = arg;
                }
                if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_COMMA) tok_advance(p);
            }
            tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        }
        return n;
    }
    if (t->kind == KYX_TK_FUNCTION || t->kind == KYX_TK_FUN) {
        tok_advance(p);
        kyAstNode *n = ast_new(KY_AST_EXPR_ANON_FUNC, t->line);
        n->as.anon_func.param_count = 0; n->as.anon_func.params = NULL; n->as.anon_func.proto = NULL;
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_LPAREN) {
            tok_advance(p);
            while (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_RPAREN) {
                kyToken *pt = tok_current(p);
                if (pt->kind == KYX_TK_IDENT) {
                    char *name = (char *)malloc(pt->len + 1);
                    if (name) { memcpy(name, pt->start, pt->len); name[pt->len] = '\0'; }
                    int pc = n->as.anon_func.param_count++;
                    n->as.anon_func.params = (char **)realloc(n->as.anon_func.params, (size_t)n->as.anon_func.param_count * sizeof(char *));
                    n->as.anon_func.params[pc] = name;
                    tok_advance(p);
                    if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_COMMA) tok_advance(p);
                } else { tok_advance(p); }
            }
            tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        }
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_LBRACE)
            n->as.anon_func.body = parse_block(p);
        return n;
    }
    return parse_postfix(p, parse_primary(p));
}

static kyAstNode *parse_primary(kyParser *p) {
    kyToken *t = tok_current(p);
    if (t->kind == KYX_TK_INT_LIT || t->kind == KYX_TK_FLOAT_LIT ||
        t->kind == KYX_TK_STRING_LIT || t->kind == KYX_TK_TRUE ||
        t->kind == KYX_TK_FALSE || t->kind == KYX_TK_NIL_LIT) {
        tok_advance(p);
        kyAstNode *n = ast_new(KY_AST_EXPR_LITERAL, t->line);
        n->as.literal.tok = *t;
        return n;
    }
    if (t->kind == KYX_TK_IDENT) {
        tok_advance(p);
        kyAstNode *n = ast_new(KY_AST_EXPR_IDENT, t->line);
        n->as.ident.name = (char *)malloc(t->len + 1);
        if (n->as.ident.name) { memcpy(n->as.ident.name, t->start, t->len); n->as.ident.name[t->len] = '\0'; }
        return n;
    }
    if (t->kind == KYX_TK_LPAREN) {
        tok_advance(p);
        kyAstNode *expr = parse_expression(p, 1);
        tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        return expr;
    }
    parser_error(p, "expected expression, got %s", (t->kind == KYX_TK_EOF ? "EOF" : "token"));
    tok_advance(p);
    return NULL;
}

static struct { const char *op; int prec; int right_assoc; } binops[] = {
    { "==", 1, 0 }, { "!=", 1, 0 },
    { "<",  2, 0 }, { "<=", 2, 0 }, { ">",  2, 0 }, { ">=", 2, 0 },
    { "<<", 3, 0 }, { ">>", 3, 0 },
    { "+",  4, 0 }, { "-",  4, 0 },
    { "*",  5, 0 }, { "/",  5, 0 }, { "%",  5, 0 },
    { "&",  6, 0 }, { "^",  7, 0 }, { "|",  8, 0 },
    { "&&", 9, 0 }, { "||", 10, 0 },
    { "?",  11, 0 },
    { "=",  12, 1 }, { "+=", 12, 1 }, { "-=", 12, 1 }, { "*=", 12, 1 }, { "/=", 12, 1 },
    { NULL, 0, 0 }
};

static int find_binop(kyToken *t) {
    /* Single-char operators identified by token kind */
    switch (t->kind) {
        case KYX_TK_EQ:       return 0;  /* == */
        case KYX_TK_NEQ:      return 1;  /* != */
        case KYX_TK_LT:       return 2;  /* < */
        case KYX_TK_LE:       return 3;  /* <= */
        case KYX_TK_GT:       return 4;  /* > */
        case KYX_TK_GE:       return 5;  /* >= */
        case KYX_TK_SHL:      return 6;  /* << */
        case KYX_TK_SHR:      return 7;  /* >> */
        case KYX_TK_PLUS:     return 8;  /* + */
        case KYX_TK_MINUS:    return 9;  /* - */
        case KYX_TK_STAR:     return 10; /* * */
        case KYX_TK_SLASH:    return 11; /* / */
        case KYX_TK_MOD:      return 12; /* % */
        case KYX_TK_AND:      return 13; /* && */
        case KYX_TK_OR:       return 14; /* || */
        case KYX_TK_BNOT:     return 15; /* & */
        case KYX_TK_ASSIGN:   return 16; /* = */
        case KYX_TK_PLUSEQ:   return 17; /* += */
        case KYX_TK_MINUSEQ:  return 18; /* -= */
        case KYX_TK_STAREQ:   return 19; /* *= */
        case KYX_TK_DIVEQ:    return 20; /* /= */
    }
    /* Multi-char operators from lexer (as IDENT tokens) */
    for (int i = 0; binops[i].op; i++) {
        if (t->kind == KYX_TK_IDENT && t->len == strlen(binops[i].op) && memcmp(t->start, binops[i].op, t->len) == 0)
            return i;
    }
    return -1;
}

static kyAstNode *parse_expression(kyParser *p, int min_prec) {
    kyAstNode *left = parse_prefix(p);
    if (!left) return NULL;
    while (!tok_at_eof(p)) {
        kyToken *t = tok_current(p);
        int op_idx = find_binop(t);
        if (op_idx < 0 || binops[op_idx].prec < min_prec) break;
        int prec = binops[op_idx].prec;
        int right_assoc = binops[op_idx].right_assoc;
        tok_advance(p);
        kyAstNode *n = ast_new(KY_AST_EXPR_BINOP, t->line);
        strncpy(n->as.binop.op, t->start, t->len < sizeof(n->as.binop.op) - 1 ? t->len : sizeof(n->as.binop.op) - 1);
        n->as.binop.left = left;
        int next_min_prec = right_assoc ? prec : prec + 1;
        if (op_idx >= 16) {
            next_min_prec = 0;
        }
        n->as.binop.right = parse_expression(p, next_min_prec);
        if (!n->as.binop.right) { ast_free(n); return left; }
        left = n;
    }
    return left;
}

static kyAstNode *parse_block_content(kyParser *p) {
    kyAstNode *n = ast_new(KY_AST_BLOCK, 1);
    n->as.block.count = 0; n->as.block.cap = 16;
    n->as.block.stmts = (kyAstNode **)calloc((size_t)n->as.block.cap, sizeof(kyAstNode *));
    while (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_RBRACE) {
        kyAstNode *s = parse_statement(p);
        if (s) {
            if (n->as.block.count >= n->as.block.cap) {
                n->as.block.cap *= 2;
                n->as.block.stmts = (kyAstNode **)realloc(n->as.block.stmts, (size_t)n->as.block.cap * sizeof(kyAstNode *));
            }
            n->as.block.stmts[n->as.block.count++] = s;
        }
    }
    return n;
}

static kyAstNode *parse_block(kyParser *p) {
    kyToken *open = tok_consume(p, KYX_TK_LBRACE, "expected '{'");
    if (!open) return NULL;
    kyAstNode *n = parse_block_content(p);
    tok_consume(p, KYX_TK_RBRACE, "expected '}'");
    return n;
}

static kyAstNode *parse_statement(kyParser *p) {
    kyToken *t = tok_current(p);
    if (t->kind == KYX_TK_VAR || t->kind == KYX_TK_LET || t->kind == KYX_TK_CONST) {
        int is_const = (t->kind == KYX_TK_CONST);
        tok_advance(p);
        kyToken *name = tok_consume(p, KYX_TK_IDENT, "expected variable name");
        if (!name) return NULL;
        kyAstNode *n = ast_new(KY_AST_VAR_DECL, t->line);
        n->as.var_decl.name = (char *)malloc(name->len + 1);
        if (n->as.var_decl.name) { memcpy(n->as.var_decl.name, name->start, name->len); n->as.var_decl.name[name->len] = '\0'; }
        n->as.var_decl.is_const = is_const;
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_ASSIGN) {
            tok_advance(p);
            n->as.var_decl.init = parse_expression(p, 1);
        }
        tok_consume(p, KYX_TK_SEMI, "expected ';'");
        return n;
    }
    if (t->kind == KYX_TK_FUNCTION || t->kind == KYX_TK_FUN) {
        tok_advance(p);
        kyToken *fname = tok_consume(p, KYX_TK_IDENT, "expected function name");
        if (!fname) return NULL;
        kyAstNode *n = ast_new(KY_AST_FUNC_DECL, t->line);
        n->as.func_decl.name = (char *)malloc(fname->len + 1);
        if (n->as.func_decl.name) { memcpy(n->as.func_decl.name, fname->start, fname->len); n->as.func_decl.name[fname->len] = '\0'; }

        n->as.func_decl.param_count = 0; n->as.func_decl.params = NULL; n->as.func_decl.proto = NULL;
        tok_consume(p, KYX_TK_LPAREN, "expected '('");
        while (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_RPAREN) {
            kyToken *pt = tok_current(p);
            if (pt->kind == KYX_TK_IDENT) {
                char *pname = (char *)malloc(pt->len + 1);
                if (pname) { memcpy(pname, pt->start, pt->len); pname[pt->len] = '\0'; }
                int pc = n->as.func_decl.param_count++;
                n->as.func_decl.params = (char **)realloc(n->as.func_decl.params, (size_t)n->as.func_decl.param_count * sizeof(char *));
                n->as.func_decl.params[pc] = pname;
                tok_advance(p);
                if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_COMMA) tok_advance(p);
            } else { tok_advance(p); }
        }
        tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        n->as.func_decl.body = parse_block(p);
        return n;
    }
    if (t->kind == KYX_TK_CLASS) {
        tok_advance(p);
        kyToken *cname = tok_consume(p, KYX_TK_IDENT, "expected class name");
        if (!cname) return NULL;
        kyAstNode *n = ast_new(KY_AST_CLASS_DECL, t->line);
        n->as.class_decl.name = (char *)malloc(cname->len + 1);
        if (n->as.class_decl.name) { memcpy(n->as.class_decl.name, cname->start, cname->len); n->as.class_decl.name[cname->len] = '\0'; }
        n->as.class_decl.parent[0] = '\0';
        n->as.class_decl.klass = (kyAstClass *)calloc(1, sizeof(kyAstClass));
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_IDENT) {
            kyToken *pt = tok_current(p);
            size_t pl = pt->len;
            strncpy(n->as.class_decl.parent, pt->start, pl);
            n->as.class_decl.parent[pl] = '\0';
            tok_advance(p);
        }
        n->as.class_decl.body = parse_block(p);
        return n;
    }
    if (t->kind == KYX_TK_IF) {
        tok_advance(p);
        tok_consume(p, KYX_TK_LPAREN, "expected '('");
        kyAstNode *cond = parse_expression(p, 1);
        tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        kyAstNode *then_b = parse_block(p);
        kyAstNode *else_b = NULL;
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_ELSE) {
            tok_advance(p);
            else_b = parse_block(p);
        }
        kyAstNode *n = ast_new(KY_AST_IF_STMT, t->line);
        n->as.if_stmt.cond = cond; n->as.if_stmt.then_b = then_b; n->as.if_stmt.else_b = else_b;
        return n;
    }
    if (t->kind == KYX_TK_WHILE) {
        tok_advance(p);
        tok_consume(p, KYX_TK_LPAREN, "expected '('");
        kyAstNode *cond = parse_expression(p, 1);
        tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        kyAstNode *body = parse_block(p);
        kyAstNode *n = ast_new(KY_AST_WHILE_STMT, t->line);
        n->as.while_stmt.cond = cond; n->as.while_stmt.body = body;
        return n;
    }
    if (t->kind == KYX_TK_FOR) {
        tok_advance(p);
        tok_consume(p, KYX_TK_LPAREN, "expected '('");
        kyAstNode *init = NULL, *cond = NULL, *inc = NULL;
        if (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_SEMI) {
            /* Check if it's a variable declaration */
            kyToken *tt = tok_current(p);
            if (tt->kind == KYX_TK_VAR || tt->kind == KYX_TK_LET || tt->kind == KYX_TK_CONST) {
                int is_const = (tt->kind == KYX_TK_CONST);
                tok_advance(p);
                kyToken *name = tok_consume(p, KYX_TK_IDENT, "expected variable name");
                if (name) {
                    init = ast_new(KY_AST_VAR_DECL, tt->line);
                    init->as.var_decl.name = (char *)malloc(name->len + 1);
                    if (init->as.var_decl.name) { memcpy(init->as.var_decl.name, name->start, name->len); init->as.var_decl.name[name->len] = '\0'; }
                    init->as.var_decl.is_const = is_const;
                    if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_ASSIGN) {
                        tok_advance(p);
                        init->as.var_decl.init = parse_expression(p, 1);
                    }
                    tok_consume(p, KYX_TK_SEMI, "expected ';' in for");
                }
            } else {
                init = parse_expression(p, 1);
            }
        }
        if (!init) tok_consume(p, KYX_TK_SEMI, "expected ';' in for");
        if (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_SEMI) {
            cond = parse_expression(p, 1);
        }
        tok_consume(p, KYX_TK_SEMI, "expected ';' in for");
        if (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_RPAREN) {
            inc = parse_expression(p, 1);
        }
        tok_consume(p, KYX_TK_RPAREN, "expected ')'");
        kyAstNode *body = parse_block(p);
        kyAstNode *n = ast_new(KY_AST_FOR_STMT, t->line);
        n->as.for_stmt.init = init; n->as.for_stmt.cond = cond;
        n->as.for_stmt.inc = inc; n->as.for_stmt.body = body;
        return n;
    }
    if (t->kind == KYX_TK_RETURN) {
        tok_advance(p);
        kyAstNode *expr = NULL;
        if (!tok_at_eof(p) && tok_current(p)->kind != KYX_TK_SEMI && tok_current(p)->kind != KYX_TK_RBRACE)
            expr = parse_expression(p, 1);
        tok_consume(p, KYX_TK_SEMI, "expected ';'");
        kyAstNode *n = ast_new(KY_AST_RETURN_STMT, t->line);
        n->as.return_stmt.expr = expr;
        return n;
    }
    if (t->kind == KYX_TK_BREAK || t->kind == KYX_TK_CONTINUE) {
        tok_advance(p);
        tok_consume(p, KYX_TK_SEMI, "expected ';'");
        return ast_new(KY_AST_EXPR_STMT, t->line);
    }
    if (t->kind == KYX_TK_USE) {
        tok_advance(p);
        kyToken *scope_tok = tok_consume(p, KYX_TK_IDENT, "expected scope");
        if (!scope_tok) return NULL;
        kyToken *path_tok = tok_consume(p, KYX_TK_STRING_LIT, "expected path string");
        if (!path_tok) return NULL;
        kyAstNode *n = ast_new(KY_AST_USE_DECL, t->line);
        n->as.use_decl.scope = (char *)malloc(scope_tok->len + 1);
        n->as.use_decl.path = (char *)malloc(path_tok->len + 1);
        if (n->as.use_decl.scope) { memcpy(n->as.use_decl.scope, scope_tok->start, scope_tok->len); n->as.use_decl.scope[scope_tok->len] = '\0'; }
        if (n->as.use_decl.path) { memcpy(n->as.use_decl.path, path_tok->start, path_tok->len); n->as.use_decl.path[path_tok->len] = '\0'; }
        n->as.use_decl.ns = NULL;
        if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_NAMESPACE) {
            tok_advance(p);
            kyToken *nstok = tok_consume(p, KYX_TK_IDENT, "expected namespace name");
            if (nstok) {
                n->as.use_decl.ns = (char *)malloc(nstok->len + 1);
                if (n->as.use_decl.ns) { memcpy(n->as.use_decl.ns, nstok->start, nstok->len); n->as.use_decl.ns[nstok->len] = '\0'; }
            }
        }
        tok_consume(p, KYX_TK_SEMI, "expected ';'");
        return n;
    }
    kyAstNode *expr = parse_expression(p, 0);
    if (!expr) return NULL;
    if (!tok_at_eof(p) && tok_current(p)->kind == KYX_TK_SEMI) tok_advance(p);
    kyAstNode *n = ast_new(KY_AST_EXPR_STMT, t->line);
    n->as.expr_stmt.expr = expr;
    return n;
}

kyParser *kyx_parser_create(void *stream) {
    kyTokenStream *ts = (kyTokenStream *)stream;
    kyParser *p = (kyParser *)calloc(1, sizeof(kyParser));
    if (!p || !ts) { free(p); return NULL; }
    p->stream = ts; p->error_count = 0;
    return p;
}

void kyx_parser_destroy(kyParser *p) {
    if (p) { ast_free(p->root); free(p); }
}

kyAstNode *kyx_parser_parse(kyParser *p) {
    if (!p || !p->stream) return NULL;
    p->root = ast_new(KY_AST_PROGRAM, 1);
    p->root->as.program.count = 0; p->root->as.program.cap = 32;
    p->root->as.program.children = (kyAstNode **)calloc((size_t)p->root->as.program.cap, sizeof(kyAstNode *));
    while (!tok_at_eof(p) && p->error_count == 0) {
        kyAstNode *stmt = parse_statement(p);
        if (stmt) {
            if (p->root->as.program.count >= p->root->as.program.cap) {
                p->root->as.program.cap *= 2;
                p->root->as.program.children = (kyAstNode **)realloc(p->root->as.program.children, (size_t)p->root->as.program.cap * sizeof(kyAstNode *));
            }
            p->root->as.program.children[p->root->as.program.count++] = stmt;
        }
    }
    return p->root;
}

const char *kyx_parser_error(const kyParser *p) { return p ? p->error_msg : "no parser"; }
int kyx_parser_error_count(const kyParser *p) { return p ? p->error_count : 0; }
