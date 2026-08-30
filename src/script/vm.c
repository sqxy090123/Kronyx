#include "kronyx/script.h"
#include "script_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef enum kyOpCode {
    OP_LOADNIL  = 0, OP_LOADBOOL, OP_LOADINT, OP_LOADFLOAT, OP_LOADCONST,
    OP_MOVE     = 5, OP_ADD,      OP_SUB,      OP_MUL,      OP_DIV,
    OP_MOD,      OP_NEG,          OP_NOT,      OP_BNOT,     OP_EQ,
    OP_NEQ,      OP_LT,           OP_LE,       OP_GT,       OP_GE,
    OP_AND,      OP_OR,           OP_CONCAT,   OP_NEWARRAY = 25,
    OP_GETFIELD = 30,              OP_SETFIELD, OP_GETINDEX, OP_SETINDEX,
    OP_GETGLOBAL,                      OP_SETGLOBAL,
    OP_CLOSURE  = 40,              OP_CALL,    OP_TAILCALL, OP_RETURN,
    OP_JUMP     = 50,              OP_JMPIF,   OP_JMPIFNOT,
    OP_INVOKE   = 60,              OP_NATIVECALL,
    OP_EXIT,
} kyOpCode;

typedef struct kyInstr {
    kyOpCode op;
    int A, B, C;
} kyInstr;

typedef struct kyProto {
    kyInstr   *code;
    int        code_count;
    int        code_cap;
    double    *constants;
    int        const_count;
    int        const_cap;
    char     **strings;
    int        str_count;
    int        str_cap;
    int        max_stack;
    int        param_count;
    char     *name;
} kyProto;

typedef struct kyClosure {
    kyProto   *proto;
    kyValue   *upvals;
    int        n_upvals;
} kyClosure;

typedef struct kyArray {
    kyValue *data;
    int      len;
    int      cap;
} kyArray;

typedef struct kyNativeEntry {
    kyValue (*fn)(struct kyVM *, kyValue *args, int argc, void *user);
    void    *user;
} kyNativeEntry;

#define KY_MAX_STACK 512
#define KY_MAX_VARS  1024
#define KY_MAX_UPVAL 64
#define KY_MAX_CALLS 256
#define KY_MAX_PROTOS KYX_MAX_PROTOS
#define KY_MAX_REGISTRY KYX_MAX_REGISTRY
#define KY_MAX_STRINGS KYX_MAX_STRINGS

typedef struct kyFrame {
    kyClosure  *closure;
    int         base;
    int         pc;
    int         top;
} kyFrame;

typedef struct kyVM kyVM;
struct kyVM {
    kyValue   stack[KY_MAX_STACK];
    kyValue   globals[KY_MAX_VARS];
    kyValue   locals[KY_MAX_VARS];
    kyProto   protos[KYX_MAX_PROTOS];
    kyClosure *closures[KYX_MAX_PROTOS];
    kyNativeEntry natives[KYX_MAX_REGISTRY];
    char     *strings[KYX_MAX_STRINGS];
    int       string_count;
    int       stack_top;
    int       frame_count;
    kyFrame   frames[KYX_MAX_CALLS];
    int       proto_count;
    int       error_flag;
    char      error_msg[256];
    int       running;
};

static kyValue nil_val(void) {
    kyValue v; memset(&v, 0, sizeof(v)); v.type = KYT_NIL; return v;
}

static kyValue bool_val(int b) {
    kyValue v; v.type = KYT_BOOL; v.as.ival = b ? 1 : 0; return v;
}

static kyValue int_val(int64_t i) {
    kyValue v; v.type = KYT_INT; v.as.ival = i; return v;
}

static kyValue float_val(double f) {
    kyValue v; v.type = KYT_FLOAT; v.as.fval = f; return v;
}

static double to_float(kyValue v) {
    if (v.type == KYT_FLOAT) return v.as.fval;
    if (v.type == KYT_INT) return (double)v.as.ival;
    return 0.0;
}

static kyValue load_const(kyVM *vm, kyProto *proto, int idx) {
    if (idx < 0 || idx >= proto->const_count) return nil_val();
    double val = proto->constants[idx];
    if (val == (double)(int64_t)val && fabs(val) < 1e15) {
        return int_val((int64_t)val);
    }
    return float_val(val);
}

static kyValue call_proto(kyVM *vm, kyProto *proto, kyValue *args, int argc) {
    if (vm->stack_top + 10 > KY_MAX_STACK) {
        strncpy(vm->error_msg, "stack overflow", sizeof(vm->error_msg));
        vm->error_flag = 1;
        return nil_val();
    }
    int saved_top = vm->stack_top;
    int base = vm->stack_top;
    int pc = 0;
    int locals_count = proto->param_count;
    for (int i = 0; i < locals_count && i < argc; i++) {
        vm->stack[base + i] = args[i];
    }
    while (pc + 3 < proto->code_count) {
        int opcode = proto->code[pc];
        int A = proto->code[pc + 1];
        int B = proto->code[pc + 2];
        int C = proto->code[pc + 3];
        pc += 4;
        switch (instr.op) {
            case OP_LOADNIL:
                vm->stack[base + A] = nil_val();
                break;
            case OP_LOADBOOL:
                vm->stack[base + A] = bool_val(B);
                break;
            case OP_LOADINT:
                vm->stack[base + A] = int_val((int64_t)B);
                break;
            case OP_LOADFLOAT:
                vm->stack[base + A] = float_val((double)B);
                break;
            case OP_LOADCONST:
                vm->stack[base + A] = load_const(vm, proto, B);
                break;
            case OP_MOVE:
                vm->stack[base + A] = vm->stack[base + B];
                break;
            case OP_ADD: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                double result = to_float(a) + to_float(b);
                vm->stack[base + A] = float_val(result);
                break;
            }
            case OP_SUB: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                vm->stack[base + A] = float_val(to_float(a) - to_float(b));
                break;
            }
            case OP_MUL: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                vm->stack[base + A] = float_val(to_float(a) * to_float(b));
                break;
            }
            case OP_DIV: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                double bd = to_float(b);
                vm->stack[base + A] = float_val(bd != 0.0 ? to_float(a) / bd : 0.0);
                break;
            }
            case OP_NEG:
                vm->stack[base + A] = float_val(-to_float(vm->stack[base + B]));
                break;
            case OP_NOT:
                vm->stack[base + A] = bool_val(vm->stack[base + B].type == KYT_NIL ||
                                               (vm->stack[base + B].type == KYT_BOOL && !vm->stack[base + B].as.ival));
                break;
            case OP_EQ: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                int eq = (a.type == b.type);
                if (eq) {
                    if (a.type == KYT_INT) eq = (a.as.ival == b.as.ival);
                    else if (a.type == KYT_FLOAT) eq = (a.as.fval == b.as.fval);
                    else if (a.type == KYT_STRING) eq = (strcmp(a.as.sval, b.as.sval) == 0);
                    else eq = (a.as.native == b.as.native);
                }
                vm->stack[base + A] = bool_val(eq);
                break;
            }
            case OP_NEQ: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                int eq = (a.type == b.type);
                if (eq) {
                    if (a.type == KYT_INT) eq = (a.as.ival == b.as.ival);
                    else if (a.type == KYT_FLOAT) eq = (a.as.fval == b.as.fval);
                    else if (a.type == KYT_STRING) eq = (strcmp(a.as.sval, b.as.sval) == 0);
                    else eq = (a.as.native == b.as.native);
                }
                vm->stack[base + A] = bool_val(!eq);
                break;
            }
            case OP_LT:
                vm->stack[base + A] = bool_val(to_float(vm->stack[base + B]) < to_float(vm->stack[base + C]));
                break;
            case OP_LE:
                vm->stack[base + A] = bool_val(to_float(vm->stack[base + B]) <= to_float(vm->stack[base + C]));
                break;
            case OP_GT:
                vm->stack[base + A] = bool_val(to_float(vm->stack[base + B]) > to_float(vm->stack[base + C]));
                break;
            case OP_GE:
                vm->stack[base + A] = bool_val(to_float(vm->stack[base + B]) >= to_float(vm->stack[base + C]));
                break;
            case OP_AND: {
                kyValue a = vm->stack[base + B];
                int truthy = (a.type != KYT_NIL && !(a.type == KYT_BOOL && !a.as.ival));
                vm->stack[base + A] = truthy ? a : vm->stack[base + C];
                break;
            }
            case OP_OR: {
                kyValue a = vm->stack[base + B];
                int truthy = (a.type != KYT_NIL && !(a.type == KYT_BOOL && !a.as.ival));
                vm->stack[base + A] = truthy ? a : vm->stack[base + C];
                break;
            }
            case OP_JMPIF:
                if (!vm->stack[base + A].as.ival) pc += 4;
                break;
            case OP_JMPIFNOT:
                if (vm->stack[base + A].as.ival) pc += 4;
                break;
            case OP_JUMP:
                pc += B;
                break;
            case OP_CALL: {
                int fn_reg = A;
                int nargs = B - 1;
                if (nargs < 0) nargs = 0;
                kyValue fn = vm->stack[base + fn_reg];
                if (fn.type == KYT_FUNCTION) {
                    kyClosure *cl = (kyClosure *)(void *)fn.as.closure;
                    if (cl && cl->proto) {
                        kyValue *call_args = &vm->stack[base + fn_reg + 1];
                        kyValue ret = call_proto(vm, cl->proto, call_args, nargs);
                        vm->stack[base + fn_reg] = ret;
                    }
                }
                break;
            }
            case OP_RETURN: {
                kyValue ret = vm->stack[base + A];
                vm->stack_top = saved_top;
                return ret;
            }
            case OP_EXIT:
                vm->stack_top = saved_top;
                return nil_val();
            default:
                break;
        }
    }
    vm->stack_top = saved_top;
    return nil_val();
}

kyVM *ky_vm_create(const void *info) {
    KY_UNUSED(info);
    kyVM *vm = (kyVM *)calloc(1, sizeof(kyVM));
    if (!vm) return NULL;
    for (int i = 0; i < KY_MAX_STACK; i++) vm->stack[i] = nil_val();
    for (int i = 0; i < KY_MAX_VARS; i++) vm->globals[i] = nil_val();
    for (int i = 0; i < KYX_MAX_REGISTRY; i++) vm->natives[i].fn = NULL;
    vm->stack_top = 0;
    vm->frame_count = 0;
    vm->proto_count = 0;
    vm->error_flag = 0;
    vm->running = 0;
    return vm;
}

void ky_vm_destroy(kyVM *vm) {
    if (vm) {
        for (int i = 0; i < vm->string_count; i++) free((void *)vm->strings[i]);
        for (int i = 0; i < vm->proto_count; i++) {
            free(vm->protos[i].code);
            free(vm->protos[i].constants);
            free(vm->protos[i].name);
            for (int j = 0; j < vm->protos[i].str_count; j++)
                free(vm->protos[i].strings[j]);
            free(vm->protos[i].strings);
        }
        free(vm);
    }
}

int ky_vm_load_string(kyVM *vm, const char *src, const char *name) {
    KY_UNUSED(name);
    if (!vm || !src) return -1;
    if (src[0] == '\0') { vm->error_flag = 0; return 0; }

    kyLexer *lx = kyx_lexer_create(src, "script");
    if (!lx) { snprintf(vm->error_msg, sizeof(vm->error_msg), "lexer alloc failed"); return -1; }

    kyTokenStream ts;
    memset(&ts, 0, sizeof(ts));
    for (int i = 0; i < KYX_MAX_TOKENS && !kyx_lexer_has_error(lx); i++) {
        ts.tokens[i] = kyx_lexer_next(lx);
        ts.count++;
        if (ts.tokens[i].kind == KYX_TK_EOF) break;
    }
    kyx_lexer_destroy(lx);
    if (kyx_lexer_has_error(lx)) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "lex error");
        return -1;
    }

    kyParser *p = kyx_parser_create(&ts);
    if (!p) { snprintf(vm->error_msg, sizeof(vm->error_msg), "parser alloc failed"); return -1; }
    kyAstNode *root = kyx_parser_parse(p);
    if (!root || kyx_parser_error_count(p) > 0) {
        kyx_parser_destroy(p);
        snprintf(vm->error_msg, sizeof(vm->error_msg), "parse error");
        return -1;
    }

    char err_buf[256];
    kyProto *proto = kyx_compile(vm, root, err_buf, sizeof(err_buf));
    kyx_parser_destroy(p);

    if (!proto) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "compile error: %s", err_buf);
        return -1;
    }

    return 0;
}

int ky_vm_load_file(kyVM *vm, const char *path) {
    if (!vm || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(vm->error_msg, sizeof(vm->error_msg), "cannot open %s", path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    int r = ky_vm_load_string(vm, buf, path);
    free(buf);
    return r;
}

int ky_vm_call(kyVM *vm, const char *func_name, kyValue *args, int argc, kyValue *ret) {
    if (!vm) return -1;
    for (int i = 0; i < vm->proto_count; i++) {
        kyProto *p = &vm->protos[i];
        if (p->name && strcmp(p->name, func_name) == 0) {
            kyClosure *cl = vm->closures[i];
            if (!cl) return -1;
            kyValue r = call_proto(vm, p, args, argc);
            if (ret) *ret = r;
            return 0;
        }
    }
    snprintf(vm->error_msg, sizeof(vm->error_msg), "function '%s' not found", func_name);
    return -1;
}

void ky_vm_register_native(kyVM *vm, const char *ns, const char *name, kyNativeFn fn, void *user) {
    KY_UNUSED(ns); KY_UNUSED(name);
    if (!vm || !fn) return;
    int id = vm->proto_count++;
    if (id < KYX_MAX_REGISTRY) {
        vm->natives[id].fn = fn;
        vm->natives[id].user = user;
    }
}

const char *ky_vm_last_error(kyVM *vm) {
    return vm ? vm->error_msg : "no vm";
}

void ky_vm_set_import_root(kyVM *vm, const char *dir) {
    KY_UNUSED(vm); KY_UNUSED(dir);
}

/* ===== Bytecode Compiler ===== */

typedef struct kyCompileState {
    int *code;
    int code_count;
    int code_cap;
    double *constants;
    int const_count;
    int const_cap;
    char **strings;
    int str_count;
    int str_cap;
    int param_count;
    int local_count;
} kyCompileState;

static void compile_emit(kyCompileState *cs, int opcode, int A, int B, int C) {
    if (cs->code_count >= cs->code_cap) {
        cs->code_cap = cs->code_cap ? cs->code_cap * 2 : 64;
        cs->code = (int *)realloc(cs->code, (size_t)cs->code_cap * sizeof(int));
    }
    int idx = cs->code_count;
    cs->code[idx++] = opcode;
    cs->code[idx++] = A;
    cs->code[idx++] = B;
    cs->code[idx++] = C;
    cs->code_count = idx;
}

static int compile_add_const(kyCompileState *cs, double val) {
    if (cs->const_count >= cs->const_cap) {
        cs->const_cap = cs->const_cap ? cs->const_cap * 2 : 16;
        cs->constants = (double *)realloc(cs->constants, (size_t)cs->const_cap * sizeof(double));
    }
    cs->constants[cs->const_count] = val;
    return cs->const_count++;
}

static int compile_add_string(kyCompileState *cs, const char *s) {
    for (int i = 0; i < cs->str_count; i++) {
        if (strcmp(cs->strings[i], s) == 0) return i;
    }
    if (cs->str_count >= cs->str_cap) {
        cs->str_cap = cs->str_cap ? cs->str_cap * 2 : 16;
        cs->strings = (char **)realloc(cs->strings, (size_t)cs->str_cap * sizeof(char *));
    }
    cs->strings[cs->str_count] = strdup(s);
    return cs->str_count++;
}

static int compile_alloc_local(kyCompileState *cs, const char *name) {
    for (int i = 0; i < cs->local_count; i++) {
        if (strcmp(cs->strings[i], name) == 0) return i;
    }
    if (cs->local_count >= KYX_MAX_LOCALS) return -1;
    compile_add_string(cs, name);
    cs->local_count++;
    return cs->local_count - 1;
}

static int compile_find_local(kyCompileState *cs, const char *name) {
    for (int i = 0; i < cs->local_count; i++) {
        if (strcmp(cs->strings[i], name) == 0) return i;
    }
    return -1;
}

static void compile_expression(kyCompileState *cs, kyAstNode *node, int dest);
static void compile_statement(kyCompileState *cs, kyAstNode *stmt);

static void compile_block(kyCompileState *cs, kyAstNode *block) {
    if (!block || block->kind != KY_AST_BLOCK) return;
    for (int i = 0; i < block->as.block.count; i++) {
        compile_statement(cs, block->as.block.stmts[i]);
    }
}

static void compile_statement(kyCompileState *cs, kyAstNode *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case KY_AST_VAR_DECL: {
            int local = compile_alloc_local(cs, stmt->as.var_decl.name);
            if (local < 0) return;
            if (stmt->as.var_decl.init) {
                compile_expression(cs, stmt->as.var_decl.init, local);
            } else {
                compile_emit(cs, 0, local, 0, 0);
            }
            break;
        }
        case KY_AST_RETURN_STMT: {
            if (stmt->as.return_stmt.expr) {
                int reg = 0;
                compile_expression(cs, stmt->as.return_stmt.expr, reg);
                compile_emit(cs, 43, reg, 0, 0);
            } else {
                compile_emit(cs, 43, 0, 0, 0);
            }
            break;
        }
        case KY_AST_IF_STMT: {
            int cond_reg = 0;
            compile_expression(cs, stmt->as.if_stmt.cond, cond_reg);
            int jmp_idx = cs->code_count + 1;
            compile_emit(cs, 52, cond_reg, 0, 0);
            compile_block(cs, stmt->as.if_stmt.then_b);
            if (stmt->as.if_stmt.else_b) {
                int jump_idx = cs->code_count;
                compile_emit(cs, 50, 0, 0, 0);
                int else_start = cs->code_count;
                cs->code[jmp_idx] = else_start - jmp_idx;
                compile_block(cs, stmt->as.if_stmt.else_b);
                int end_of_else = cs->code_count;
                compile_emit(cs, 50, 0, 0, 0);
                int jump_end = cs->code_count;
                cs->code[jump_idx] = jump_end - jump_idx;
                cs->code[end_of_else - 1] = end_of_else - jump_end;
            } else {
                cs->code[jmp_idx] = cs->code_count - jmp_idx;
            }
            break;
        }
        case KY_AST_WHILE_STMT: {
            int loop_start = cs->code_count;
            int cond_reg = 0;
            compile_expression(cs, stmt->as.while_stmt.cond, cond_reg);
            int jmp_idx = cs->code_count + 1;
            compile_emit(cs, 52, cond_reg, 0, 0);
            compile_block(cs, stmt->as.while_stmt.body);
            compile_emit(cs, 50, 0, loop_start - cs->code_count, 0);
            cs->code[jmp_idx] = cs->code_count - jmp_idx;
            break;
        }
        case KY_AST_FOR_STMT: {
            if (stmt->as.for_stmt.init) compile_statement(cs, stmt->as.for_stmt.init);
            int loop_start = cs->code_count;
            if (stmt->as.for_stmt.cond) {
                int cond_reg = 0;
                compile_expression(cs, stmt->as.for_stmt.cond, cond_reg);
                int jmp_idx = cs->code_count + 1;
                compile_emit(cs, 52, cond_reg, 0, 0);
                compile_block(cs, stmt->as.for_stmt.body);
                if (stmt->as.for_stmt.inc) {
                    compile_expression(cs, stmt->as.for_stmt.inc, 0);
                }
                compile_emit(cs, 50, 0, loop_start - cs->code_count, 0);
                cs->code[jmp_idx] = cs->code_count - jmp_idx;
            }
            break;
        }
        case KY_AST_EXPR_STMT: {
            if (stmt->as.expr_stmt.expr) {
                int reg = 0;
                compile_expression(cs, stmt->as.expr_stmt.expr, reg);
            }
            break;
        }
        default:
            break;
    }
}

static void compile_expression(kyCompileState *cs, kyAstNode *node, int dest) {
    if (!node) return;
    switch (node->kind) {
        case KY_AST_EXPR_LITERAL: {
            kyToken *t = &node->as.literal.tok;
            if (t->kind == KYX_TK_INT_LIT) {
                compile_emit(cs, 6, dest, (int)t->as.ival, 0);
            } else if (t->kind == KYX_TK_FLOAT_LIT) {
                int c = compile_add_const(cs, t->as.fval);
                compile_emit(cs, 9, dest, c, 0);
            } else if (t->kind == KYX_TK_STRING_LIT) {
                int c = compile_add_string(cs, t->as.sval);
                compile_emit(cs, 8, dest, c, 0);
            } else if (t->kind == KYX_TK_TRUE) {
                compile_emit(cs, 1, dest, 1, 0);
            } else if (t->kind == KYX_TK_FALSE) {
                compile_emit(cs, 1, dest, 0, 0);
            } else if (t->kind == KYX_TK_NIL_LIT) {
                compile_emit(cs, 0, dest, 0, 0);
            }
            break;
        }
        case KY_AST_EXPR_IDENT: {
            int local = compile_find_local(cs, node->as.ident.name);
            if (local >= 0) {
                compile_emit(cs, 5, dest, local, 0);
            } else {
                int idx = compile_add_string(cs, node->as.ident.name);
                compile_emit(cs, 32, dest, idx, 0);
            }
            break;
        }
        case KY_AST_EXPR_BINOP: {
            int lhs = dest;
            compile_expression(cs, node->as.binop.left, lhs);
            int rhs = (lhs == dest) ? (dest + 1) : dest;
            compile_expression(cs, node->as.binop.right, rhs);
            if (strcmp(node->as.binop.op, "+") == 0) {
                compile_emit(cs, 6, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "-") == 0) {
                compile_emit(cs, 7, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "*") == 0) {
                compile_emit(cs, 8, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "/") == 0) {
                compile_emit(cs, 9, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "%") == 0) {
                compile_emit(cs, 10, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "==") == 0) {
                compile_emit(cs, 14, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "!=") == 0) {
                compile_emit(cs, 15, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "<") == 0) {
                compile_emit(cs, 16, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "<=") == 0) {
                compile_emit(cs, 17, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, ">") == 0) {
                compile_emit(cs, 18, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, ">=") == 0) {
                compile_emit(cs, 19, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "&&") == 0) {
                compile_emit(cs, 20, dest, lhs, rhs);
            } else if (strcmp(node->as.binop.op, "||") == 0) {
                compile_emit(cs, 21, dest, lhs, rhs);
            }
            break;
        }
        case KY_AST_EXPR_UNOP: {
            if (strcmp(node->as.unop.op, "-") == 0) {
                int val = dest;
                compile_expression(cs, node->as.unop.operand, val);
                compile_emit(cs, 12, dest, val, 0);
            } else if (strcmp(node->as.unop.op, "!") == 0) {
                int val = dest;
                compile_expression(cs, node->as.unop.operand, val);
                compile_emit(cs, 13, dest, val, 0);
            }
            break;
        }
        case KY_AST_EXPR_CALL: {
            int fn_reg = dest;
            compile_expression(cs, node->as.call.callee, fn_reg);
            int nargs = node->as.call.arg_count;
            for (int i = 0; i < nargs; i++) {
                compile_expression(cs, node->as.call.args[i], fn_reg + 1 + i);
            }
            compile_emit(cs, 41, fn_reg, nargs + 1, 0);
            break;
        }
        default:
            break;
    }
}

kyProto *kyx_compile(kyVM *vm, kyAstNode *root, char *err_buf, int err_buf_size) {
    KY_UNUSED(err_buf); KY_UNUSED(err_buf_size);
    if (!root || root->kind != KY_AST_PROGRAM) return NULL;

    for (int i = 0; i < root->as.program.count; i++) {
        kyAstNode *stmt = root->as.program.children[i];
        if (stmt->kind != KY_AST_FUNC_DECL) continue;

        kyCompileState cs;
        memset(&cs, 0, sizeof(cs));
        cs.code_cap = 64;
        cs.const_cap = 16;
        cs.str_cap = 16;
        cs.code = (int *)malloc((size_t)cs.code_cap * sizeof(int));
        cs.constants = (double *)calloc((size_t)cs.const_cap, sizeof(double));
        cs.strings = (char **)calloc((size_t)cs.str_cap, sizeof(char *));

        cs.param_count = stmt->as.func_decl.param_count;

        for (int j = 0; j < stmt->as.func_decl.param_count; j++) {
            compile_alloc_local(&cs, stmt->as.func_decl.params[j]);
        }

        compile_block(&cs, stmt->as.func_decl.body);

        int id = vm->proto_count++;
        if (id < KYX_MAX_PROTOS && cs.code) {
            vm->protos[id].code = (kyInstr *)cs.code;
            vm->protos[id].code_count = cs.code_count;
            vm->protos[id].constants = cs.constants;
            vm->protos[id].const_count = cs.const_count;
            vm->protos[id].strings = cs.strings;
            vm->protos[id].str_count = cs.str_count;
            vm->protos[id].param_count = cs.param_count;
            vm->protos[id].max_stack = 16;
            vm->protos[id].name = strdup(stmt->as.func_decl.name);
            vm->closures[id] = (kyClosure *)calloc(1, sizeof(kyClosure));
            if (vm->closures[id]) {
                vm->closures[id]->proto = &vm->protos[id];
            }
        } else {
            free(cs.code);
            free(cs.constants);
            free(cs.strings);
        }
    }

    return &vm->protos[0];
}
