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
    OP_AND,      OP_OR,           OP_CONCAT,   OP_BAND = 22, OP_BOR,  OP_BXOR,
    OP_BSHL,     OP_BSHR,         OP_NEWARRAY = 27, OP_LOADSTRING = 28,
    OP_GETFIELD = 30,              OP_SETFIELD, OP_GETINDEX, OP_SETINDEX,
    OP_GETGLOBAL = 32,              OP_SETGLOBAL,
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
    char     *name;
} kyProto;

typedef struct kyClosure {
    kyProto   *proto;
} kyClosure;


typedef struct kyNativeEntry {
    kyValue (*fn)(struct kyVM *, kyValue *args, int argc, void *user);
    void    *user;
    char     ns[64];
    char     name[64];
} kyNativeEntry;

#define KY_MAX_STACK 512
#define KY_MAX_VARS  1024
#define KY_MAX_CALL_DEPTH 256


#define KY_MAX_PROTOS KYX_MAX_PROTOS
#define KY_MAX_REGISTRY KYX_MAX_REGISTRY



typedef struct kyVM kyVM;
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
    if (v.type == KYT_INT)   return (double)v.as.ival;
    if (v.type == KYT_BOOL)  return v.as.ival ? 1.0 : 0.0;
    return 0.0;
}

static int64_t to_int(kyValue v) {
    double d = to_float(v);
    if (d >= 9.223372036854775807e18)   return INT64_MAX;
    if (d <= -9.223372036854775808e18) return INT64_MIN;
    return (int64_t)d;
}
static int val_truthy(kyValue v) {
    if (v.type == KYT_NIL) return 0;
    if (v.type == KYT_BOOL) return v.as.ival != 0;
    if (v.type == KYT_INT) return v.as.ival != 0;
    if (v.type == KYT_FLOAT) return v.as.fval != 0.0;
    if (v.type == KYT_STRING) return v.as.sval[0] != '\0';
    return 1;
}
static int val_eq(kyValue a, kyValue b) {
    if (a.type != b.type) return 0;
    if (a.type == KYT_INT) return a.as.ival == b.as.ival;
    if (a.type == KYT_FLOAT) return a.as.fval == b.as.fval;
    if (a.type == KYT_STRING) return strcmp(a.as.sval, b.as.sval) == 0;
    return a.as.native == b.as.native;
}

static const char *proto_str(const kyProto *p, int i) {
    return (i >= 0 && i < p->str_count) ? p->strings[i] : NULL;
}

static kyValue load_const(kyVM *vm, kyProto *proto, int idx) {
    if (idx < 0 || idx >= proto->const_count) return nil_val();
    double val = proto->constants[idx];
    if (fabs(val) < 1e15 && val == (double)(int64_t)val) {
        return int_val((int64_t)val);
    }
    return float_val(val);
}

static kyValue call_proto(kyVM *vm, kyProto *proto, kyValue *args, int argc) {
    if (vm->stack_top + 10 > KY_MAX_STACK) {
        strncpy(vm->error_msg, "heap stack overflow", sizeof(vm->error_msg));
        return nil_val();
    }
    if (vm->call_depth >= KY_MAX_CALL_DEPTH) {
        strncpy(vm->error_msg, "call depth exceeded", sizeof(vm->error_msg));
        return nil_val();
    }
    vm->call_depth++;
    int saved_top = vm->stack_top;
    int base = vm->stack_top;
    int pc = 0;
    int locals_count = proto->param_count;
    for (int i = 0; i < locals_count && i < argc; i++) {
        vm->stack[base + i] = args[i];
    }
    while (pc + 4 <= proto->code_count) {
        int opcode = proto->code[pc];
        int A = proto->code[pc + 1];
        int B = proto->code[pc + 2];
        int C = proto->code[pc + 3];
        pc += 4;
        switch (opcode) {
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
            case OP_LOADSTRING: {
                const char *s = proto_str(proto, B);
                vm->stack[base + A] = s ? (kyValue){KYT_STRING, .as.sval = s} : nil_val();
                break;
            }
            case OP_MOVE:
                vm->stack[base + A] = vm->stack[base + B];
                break;
            case OP_ADD: case OP_SUB: case OP_MUL: {
                double x = to_float(vm->stack[base + B]), y = to_float(vm->stack[base + C]);
                vm->stack[base + A] = float_val(opcode == OP_ADD ? x + y :
                                                opcode == OP_SUB ? x - y : x * y);
                break;
            }
            case OP_DIV: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                double bd = to_float(b);
                vm->stack[base + A] = float_val(bd != 0.0 ? to_float(a) / bd : 0.0);
                break;
            }
            case OP_MOD: {
                kyValue a = vm->stack[base + B], b = vm->stack[base + C];
                double bd = to_float(b);
                vm->stack[base + A] = float_val(bd != 0.0 ? fmod(to_float(a), bd) : 0.0);
                break;
            }
            case OP_NEG:
                vm->stack[base + A] = float_val(-to_float(vm->stack[base + B]));
                break;
            case OP_NOT:
                vm->stack[base + A] = bool_val(!val_truthy(vm->stack[base + B]));
                break;
            case OP_BNOT:
                vm->stack[base + A] = int_val(~to_int(vm->stack[base + B]));
                break;
            case OP_EQ:
            case OP_NEQ:
                vm->stack[base + A] = bool_val(val_eq(vm->stack[base + B], vm->stack[base + C]) ==
                                               (opcode == OP_EQ));
                break;
            case OP_LT: case OP_LE: case OP_GT: case OP_GE: {
                double x = to_float(vm->stack[base + B]), y = to_float(vm->stack[base + C]);
                int r = (opcode == OP_LT) ? (x < y) : (opcode == OP_LE) ? (x <= y)
                      : (opcode == OP_GT) ? (x > y) : (x >= y);
                vm->stack[base + A] = bool_val(r);
                break;
            }
            case OP_AND: {
                kyValue a = vm->stack[base + B];
                vm->stack[base + A] = val_truthy(a) ? vm->stack[base + C] : a;
                break;
            }
            case OP_OR: {
                kyValue a = vm->stack[base + B];
                vm->stack[base + A] = val_truthy(a) ? a : vm->stack[base + C];
                break;
            }
            case OP_BAND: case OP_BOR: case OP_BXOR: case OP_BSHL: case OP_BSHR: {
                int64_t x = to_int(vm->stack[base + B]), y = to_int(vm->stack[base + C]);
                int shift = (int)((uint64_t)y & 63u);
                /* Clamp left-shift to [0,62] to avoid signed overflow UB.
                 * Right-shift by 63 is implementation-defined but safe. */
                int64_t r;
                if (opcode == OP_BAND)       r = x & y;
                else if (opcode == OP_BOR)   r = x | y;
                else if (opcode == OP_BXOR)  r = x ^ y;
                else if (opcode == OP_BSHL)  r = shift >= 63 ? 0 : (int64_t)((uint64_t)x << shift);
                else                           r = x >> shift;
                vm->stack[base + A] = int_val(r);
                break;
            }
            case OP_GETFIELD: {
                /* A=dest, B=obj_reg, C=field_string_idx */
                kyValue obj = vm->stack[base + B];
                const char *fname = proto_str(proto, C);
                if (obj.type == KYT_NATIVE && fname) {
                    kyNativeEntry *ne = (kyNativeEntry *)obj.as.native;
                    if (ne && ne->fn && strcmp(ne->name, fname) == 0) {
                        vm->stack[base + A] = obj;
                        break;
                    }
                }
                /* fallback: nil */
                vm->stack[base + A] = nil_val();
                break;
            }
            case OP_JMPIF:
                if (vm->stack[base + A].as.ival) pc += B - 4;
                break;
            case OP_JMPIFNOT:
                if (!vm->stack[base + A].as.ival) pc += B - 4;
                break;
            case OP_JUMP:
                pc += B - 4;
                break;
            case OP_CALL: {
                int fn_reg = A;
                int nargs = B - 1;
                if (nargs < 0) nargs = 0;
                if (fn_reg < 0 || fn_reg >= KY_MAX_STACK || base + fn_reg + nargs >= KY_MAX_STACK) break;
                kyValue fn = vm->stack[base + fn_reg];
                if (fn.type == KYT_FUNCTION) {
                    kyClosure *cl = (kyClosure *)(void *)fn.as.closure;
                    if (cl && cl->proto) {
                        kyValue *call_args = &vm->stack[base + fn_reg + 1];
                        kyValue ret = call_proto(vm, cl->proto, call_args, nargs);
                        vm->stack[base + fn_reg] = ret;
                    }
                } else if (fn.type == KYT_NATIVE) {
                    kyNativeEntry *ne = (kyNativeEntry *)fn.as.native;
                    if (ne && ne->fn) {
                        kyValue *call_args = &vm->stack[base + fn_reg + 1];
                        vm->stack[base + fn_reg] = ne->fn(vm, call_args, nargs, ne->user);
                    }
                }
                break;
            }
            case OP_RETURN: {
                kyValue ret = vm->stack[base + A];
                if (ret.type == KYT_INT || ret.type == KYT_BOOL) {
                    ret.type = KYT_FLOAT;
                    ret.as.fval = (double)ret.as.ival;
                }
                vm->stack_top = saved_top;
                vm->call_depth--;
                return ret;
            }
            case OP_GETGLOBAL: {
                const char *name = proto_str(proto, B);
                if (!name) break;
                int found = 0;
                for (int i = 0; i < vm->gvar_count && !found; i++) {
                    if (strcmp(vm->gvar_names[i], name) == 0) {
                        vm->stack[base + A] = vm->gvar_vals[i];
                        found = 1;
                    }
                }
                for (int i = 0; i < vm->proto_count && !found; i++) {
                    if (vm->protos[i].name && strcmp(vm->protos[i].name, name) == 0) {
                        vm->stack[base + A] = (kyValue){KYT_FUNCTION, .as.closure = vm->closures[i]};
                        found = 1;
                    }
                }
                /* also look up native functions registered under this name */
                for (int i = 0; i < vm->native_count && !found; i++) {
                    if (vm->natives[i].fn &&
                        strcmp(vm->natives[i].name, name) == 0) {
                        vm->stack[base + A] = (kyValue){KYT_NATIVE, .as.native = (void*)&vm->natives[i]};
                        found = 1;
                    }
                }
                if (!found) {
                    vm->stack[base + A] = nil_val();
                }
                break;
            }
            case OP_SETGLOBAL: {
                const char *name = proto_str(proto, B);
                if (!name) break;
                kyValue v = vm->stack[base + A];
                int found = 0;
                for (int i = 0; i < vm->gvar_count && !found; i++) {
                    if (strcmp(vm->gvar_names[i], name) == 0) {
                        vm->gvar_vals[i] = v;
                        found = 1;
                    }
                }
                if (!found && vm->gvar_count < KY_MAX_VARS) {
                    vm->gvar_names[vm->gvar_count] = strdup(name);
                    vm->gvar_vals[vm->gvar_count] = v;
                    vm->gvar_count++;
                }
                break;
            }
            case OP_EXIT:
                vm->stack_top = saved_top;
                vm->call_depth--;
                return nil_val();
            case OP_NATIVECALL: {
                /* A=dest, B=nargs|(name_idx<<8), C=ns_string_idx */
                const char *ns = proto_str(proto, C);
                /* name index is always present for NATIVECALL (emitted from the
                 * field-call path); decode unconditionally.  The old "(B >= 256)"
                 * guard dropped every native whose string index was 0. */
                const char *nm = proto_str(proto, B >> 8);
                int nargs = B & 0xFF;
                kyValue *arg_base = &vm->stack[base + A + 1];
                for (int i = 0; i < vm->native_count && nm && ns; i++) {
                    if (vm->natives[i].fn &&
                        strcmp(vm->natives[i].ns, ns) == 0 &&
                        strcmp(vm->natives[i].name, nm) == 0) {
                        vm->stack[base + A] = vm->natives[i].fn(vm, arg_base, nargs, vm->natives[i].user);
                        break;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    vm->stack_top = saved_top;
    vm->call_depth--;
    return nil_val();
}

kyVM *ky_vm_create(const void *info) {
    KY_UNUSED(info);
    return (kyVM *)calloc(1, sizeof(kyVM));
}

void ky_vm_destroy(kyVM *vm) {
    if (vm) {
        for (int i = 0; i < vm->proto_count; i++) {
            free(vm->protos[i].code);
            free(vm->protos[i].constants);
            free(vm->protos[i].name);
            for (int j = 0; j < vm->protos[i].str_count; j++)
                free(vm->protos[i].strings[j]);
            free(vm->protos[i].strings);
            free(vm->closures[i]);
        }
        for (int i = 0; i < vm->gvar_count; i++)
            free(vm->gvar_names[i]);
        free(vm);
    }
}

int ky_vm_load_string(kyVM *vm, const char *src, const char *name) {
    KY_UNUSED(name);
    if (!vm || !src) return -1;

    kyLexer *lx = kyx_lexer_create(src, "script");
    if (!lx) { snprintf(vm->error_msg, sizeof(vm->error_msg), "lexer alloc failed"); return -1; }

    kyTokenStream ts;
    memset(&ts, 0, sizeof(ts));
    for (int i = 0; i < KYX_MAX_TOKENS && !kyx_lexer_has_error(lx); i++) {
        ts.tokens[i] = kyx_lexer_next(lx);
        ts.count++;
        if (ts.tokens[i].kind == KYX_TK_EOF) break;
    }
    int lex_err = kyx_lexer_has_error(lx);
    kyx_lexer_destroy(lx);
    if (lex_err) {
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
        snprintf(vm->error_msg, sizeof(vm->error_msg), "compile error: %.240s", err_buf);
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
    /* run top-level statements (implicit __top__ proto) once before first call */
    if (!vm->top_ran) {
        vm->top_ran = 1;
        for (int i = 0; i < vm->proto_count; i++) {
            if (vm->protos[i].name && strcmp(vm->protos[i].name, "__top__") == 0 &&
                vm->closures[i]) {
                call_proto(vm, &vm->protos[i], NULL, 0);
                break;
            }
        }
    }
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
    if (!vm || !fn) return;
    if (vm->native_count >= KYX_MAX_REGISTRY) return;
    int id = vm->native_count++;
    vm->natives[id].fn = fn;
    vm->natives[id].user = user;
    strncpy(vm->natives[id].ns, ns ? ns : "", sizeof(vm->natives[id].ns) - 1);
    strncpy(vm->natives[id].name, name ? name : "", sizeof(vm->natives[id].name) - 1);
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
    char *local_names[KYX_MAX_LOCALS];
    int local_count;
    int is_top;
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

static int compile_add_string_n(kyCompileState *cs, const char *s, int len) {
    for (int i = 0; i < cs->str_count; i++) {
        if (strncmp(cs->strings[i], s, (size_t)len) == 0 &&
            cs->strings[i][len] == '\0') return i;
    }
    if (cs->str_count >= cs->str_cap) {
        cs->str_cap = cs->str_cap ? cs->str_cap * 2 : 16;
        cs->strings = (char **)realloc(cs->strings, (size_t)cs->str_cap * sizeof(char *));
    }
    char *copy = (char *)malloc((size_t)len + 1);
    if (copy) {
        memcpy(copy, s, (size_t)len);
        copy[len] = '\0';
    }
    cs->strings[cs->str_count] = copy;
    return cs->str_count++;
}

static int compile_add_string(kyCompileState *cs, const char *s) {
    return compile_add_string_n(cs, s, (int)strlen(s));
}

static int compile_find_local(kyCompileState *cs, const char *name) {
    for (int i = 0; i < cs->local_count; i++) {
        if (cs->local_names[i] && strcmp(cs->local_names[i], name) == 0) return i;
    }
    return -1;
}
static int compile_alloc_local(kyCompileState *cs, const char *name) {
    for (int i = 0; i < cs->local_count; i++) {
        if (cs->local_names[i] && strcmp(cs->local_names[i], name) == 0) return i;
    }
    if (cs->local_count >= KYX_MAX_LOCALS) return -1;
    cs->local_names[cs->local_count] = strdup(name);
    cs->local_count++;
    return cs->local_count - 1;
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
            if (cs->is_top) {
                /* top-level variable: evaluate init into temp, store as global */
                int name_idx = compile_add_string(cs, stmt->as.var_decl.name);
                int tmp = cs->local_count + 1;
                if (stmt->as.var_decl.init) {
                    compile_expression(cs, stmt->as.var_decl.init, tmp);
                } else {
                    compile_emit(cs, 0, tmp, 0, 0);
                }
                compile_emit(cs, 33, tmp, name_idx, 0);
                break;
            }
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
            int temp_reg = cs->local_count + 1;
            compile_expression(cs, stmt->as.if_stmt.cond, temp_reg);
            int jmp_idx = cs->code_count;
            compile_emit(cs, 52, temp_reg, 0, 0);
            compile_block(cs, stmt->as.if_stmt.then_b);
            if (stmt->as.if_stmt.else_b) {
                int jump_idx = cs->code_count;
                compile_emit(cs, 50, 0, 0, 0);
                int else_start = cs->code_count;
                cs->code[jmp_idx + 2] = else_start - jmp_idx;
                compile_block(cs, stmt->as.if_stmt.else_b);
                int end_of_else = cs->code_count;
                compile_emit(cs, 50, 0, 0, 0);
                int jump_end = cs->code_count;
                cs->code[jump_idx + 2] = jump_end - jump_idx;
                cs->code[end_of_else - 1] = end_of_else - jump_end;
            } else {
                cs->code[jmp_idx + 2] = cs->code_count - jmp_idx;
            }
            break;
        }
        case KY_AST_WHILE_STMT: {
            int loop_start = cs->code_count;
            int temp_reg = cs->local_count + 1;
            compile_expression(cs, stmt->as.while_stmt.cond, temp_reg);
            int jmp_idx = cs->code_count;
            compile_emit(cs, 52, temp_reg, 0, 0);
            compile_block(cs, stmt->as.while_stmt.body);
            int jmp_offset = loop_start - cs->code_count;
            compile_emit(cs, 50, 0, jmp_offset, 0);
            cs->code[jmp_idx + 2] = cs->code_count - jmp_idx;
            break;
        }
        case KY_AST_FOR_STMT: {
            if (stmt->as.for_stmt.init) compile_statement(cs, stmt->as.for_stmt.init);
            int loop_start = cs->code_count;
            if (stmt->as.for_stmt.cond) {
                int temp_reg = cs->local_count + 1;
                compile_expression(cs, stmt->as.for_stmt.cond, temp_reg);
                int jmp_idx = cs->code_count;
                compile_emit(cs, 52, temp_reg, 0, 0);
                compile_block(cs, stmt->as.for_stmt.body);
                if (stmt->as.for_stmt.inc) {
                    compile_expression(cs, stmt->as.for_stmt.inc, 0);
                }
                int jmp_offset = loop_start - cs->code_count;
                compile_emit(cs, 50, 0, jmp_offset, 0);
                cs->code[jmp_idx + 2] = cs->code_count - jmp_idx;
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
                int64_t ival = t->as.ival;
                if (ival >= INT32_MIN && ival <= INT32_MAX) {
                    compile_emit(cs, 2, dest, (int)ival, 0);
                } else {
                    int c = compile_add_const(cs, (double)ival);
                    compile_emit(cs, 4, dest, c, 0);
                }
            } else if (t->kind == KYX_TK_FLOAT_LIT) {
                int c = compile_add_const(cs, t->as.fval);
                compile_emit(cs, 4, dest, c, 0);
            } else if (t->kind == KYX_TK_STRING_LIT) {
                int len = t->len >= 2 ? (int)t->len - 2 : 0;
                int c = compile_add_string_n(cs, t->as.sval, len);
                compile_emit(cs, 28, dest, c, 0);
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
            int tok = node->as.binop.op_kind;
            /* Handle assignments (simple and compound) */
            if (tok == KYX_TK_ASSIGN || tok == KYX_TK_PLUSEQ || tok == KYX_TK_MINUSEQ ||
                tok == KYX_TK_STAREQ || tok == KYX_TK_DIVEQ || tok == KYX_TK_MODEQ) {
                if (node->as.binop.left && node->as.binop.left->kind == KY_AST_EXPR_IDENT) {
                    const char *name = node->as.binop.left->as.ident.name;
                    int local = compile_find_local(cs, name);
                    if (local >= 0) {
                        if (tok == KYX_TK_ASSIGN) {
                            compile_expression(cs, node->as.binop.right, local);
                        } else {
                            compile_expression(cs, node->as.binop.right, dest + 1);
                            int oc = tok == KYX_TK_PLUSEQ ? 6 : tok == KYX_TK_MINUSEQ ? 7 :
                                     tok == KYX_TK_STAREQ ? 8 : tok == KYX_TK_DIVEQ ? 9 : 10;
                            compile_emit(cs, oc, local, local, dest + 1);
                        }
                        break;
                    }
                    int name_idx = compile_add_string(cs, name);
                    int tmp = cs->local_count + 1;
                    if (tok == KYX_TK_ASSIGN) {
                        compile_expression(cs, node->as.binop.right, tmp);
                    } else {
                        compile_emit(cs, 32, tmp, name_idx, 0);
                        compile_expression(cs, node->as.binop.right, tmp + 1);
                        int oc = tok == KYX_TK_PLUSEQ ? 6 : tok == KYX_TK_MINUSEQ ? 7 :
                                 tok == KYX_TK_STAREQ ? 8 : tok == KYX_TK_DIVEQ ? 9 : 10;
                        compile_emit(cs, oc, tmp, tmp, tmp + 1);
                    }
                    compile_emit(cs, 33, tmp, name_idx, 0);
                    break;
                }
            }
            int lhs = dest;
            compile_expression(cs, node->as.binop.left, lhs);
            int rhs = (lhs == dest) ? (dest + 1) : dest;
            compile_expression(cs, node->as.binop.right, rhs);
            int opcode = -1;
            switch (tok) {
                case KYX_TK_PLUS:      opcode = 6;  break;
                case KYX_TK_MINUS:     opcode = 7;  break;
                case KYX_TK_STAR:      opcode = 8;  break;
                case KYX_TK_SLASH:     opcode = 9;  break;
                case KYX_TK_MOD:       opcode = 10; break;
                case KYX_TK_EQ:        opcode = 14; break;
                case KYX_TK_NEQ:       opcode = 15; break;
                case KYX_TK_LT:        opcode = 16; break;
                case KYX_TK_LE:        opcode = 17; break;
                case KYX_TK_GT:        opcode = 18; break;
                case KYX_TK_GE:        opcode = 19; break;
                case KYX_TK_AND:       opcode = 20; break;
                case KYX_TK_OR:        opcode = 21; break;
                case KYX_TK_BAND:      opcode = 22; break;
                case KYX_TK_BOR:       opcode = 23; break;
                case KYX_TK_BXOR:      opcode = 24; break;
                case KYX_TK_SHL:       opcode = 25; break;
                case KYX_TK_SHR:       opcode = 26; break;
            }
            if (opcode >= 0) compile_emit(cs, opcode, dest, lhs, rhs);
            break;
        }
        case KY_AST_EXPR_UNOP: {
            if (strcmp(node->as.unop.op, "-") == 0) {
                int val = dest;
                compile_expression(cs, node->as.unop.operand, val);
                compile_emit(cs, 11, dest, val, 0);
            } else if (strcmp(node->as.unop.op, "!") == 0) {
                int val = dest;
                compile_expression(cs, node->as.unop.operand, val);
                compile_emit(cs, 12, dest, val, 0);
            } else if (strcmp(node->as.unop.op, "~") == 0) {
                int val = dest;
                compile_expression(cs, node->as.unop.operand, val);
                compile_emit(cs, 13, dest, val, 0);
            }
            break;
        }
        case KY_AST_EXPR_CALL: {
            kyAstNode *callee = node->as.call.callee;
            int nargs = node->as.call.arg_count;
            int tmp_reg = dest;
            for (int i = 0; i < nargs; i++) {
                compile_expression(cs, node->as.call.args[i], tmp_reg + 1 + i);
            }
            if (callee->kind == KY_AST_EXPR_FIELD &&
                callee->as.field.obj->kind == KY_AST_EXPR_IDENT) {
                int name_idx = compile_add_string(cs, callee->as.field.field);
                int ns_idx = compile_add_string(cs, callee->as.field.obj->as.ident.name);
                compile_emit(cs, 61, tmp_reg, nargs | (name_idx << 8), ns_idx);
            } else {
                compile_expression(cs, callee, tmp_reg);
                compile_emit(cs, 41, tmp_reg, nargs + 1, 0);
            }
            break;
        }
        case KY_AST_EXPR_FIELD: {
            /* compile obj.field: get obj, then getfield by string index */
            int obj_reg = dest;
            compile_expression(cs, node->as.field.obj, obj_reg);
            int field_idx = compile_add_string(cs, node->as.field.field);
            compile_emit(cs, 30, dest, obj_reg, field_idx);
            break;
        }
        default:
            break;
    }
}

kyProto *kyx_compile(kyVM *vm, kyAstNode *root, char *err_buf, int err_buf_size) {
    KY_UNUSED(err_buf); KY_UNUSED(err_buf_size);
    if (!root || root->kind != KY_AST_PROGRAM) return NULL;

    /* compile top-level statements into an implicit __top__ proto */
    int has_top = 0;
    for (int i = 0; i < root->as.program.count; i++) {
        if (root->as.program.children[i]->kind != KY_AST_FUNC_DECL) {
            has_top = 1;
            break;
        }
    }
    if (has_top) {
        kyCompileState cs;
        memset(&cs, 0, sizeof(cs));
        cs.code_cap = 64;
        cs.const_cap = 16;
        cs.str_cap = 16;
        cs.code = (int *)malloc((size_t)cs.code_cap * sizeof(int));
        cs.constants = (double *)calloc((size_t)cs.const_cap, sizeof(double));
        cs.strings = (char **)calloc((size_t)cs.str_cap, sizeof(char *));
        cs.is_top = 1;
        for (int i = 0; i < root->as.program.count; i++) {
            kyAstNode *stmt = root->as.program.children[i];
            if (stmt->kind == KY_AST_FUNC_DECL) continue;
            compile_statement(&cs, stmt);
        }
        compile_emit(&cs, 62, 0, 0, 0);  /* OP_EXIT */
        int id = vm->proto_count++;
        if (id < KYX_MAX_PROTOS && cs.code) {
            size_t code_size = cs.code_count * sizeof(int);
            vm->protos[id].code = malloc(code_size);
            if (vm->protos[id].code) {
                memcpy((void*)vm->protos[id].code, cs.code, code_size);
            }
            vm->protos[id].constants = cs.constants;
            vm->protos[id].strings = cs.strings;
            vm->protos[id].code_count = cs.code_count;
            vm->protos[id].const_count = cs.const_count;
            vm->protos[id].str_count = cs.str_count;
            vm->protos[id].param_count = 0;
            vm->protos[id].name = strdup("__top__");
            vm->closures[id] = (kyClosure *)calloc(1, sizeof(kyClosure));
            if (vm->closures[id]) {
                vm->closures[id]->proto = &vm->protos[id];
            }
            if (!vm->protos[id].code || !vm->protos[id].name || !vm->closures[id]) {
                free(vm->protos[id].code);
                free(vm->protos[id].name);
                free(vm->closures[id]);
                vm->protos[id].code = NULL;
                vm->protos[id].name = NULL;
                vm->closures[id] = NULL;
                cs.constants = NULL;
                cs.strings = NULL;
            } else {
                cs.constants = NULL;
                cs.strings = NULL;
            }
        }
        if (cs.code) free(cs.code);
        if (cs.constants) free(cs.constants);
        if (cs.strings) free(cs.strings);
        for (int j = 0; j < cs.local_count; j++) free(cs.local_names[j]);
    }

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
            size_t code_size = cs.code_count * sizeof(int);
            vm->protos[id].code = malloc(code_size);
            if (vm->protos[id].code) {
                memcpy((void*)vm->protos[id].code, cs.code, code_size);
            }
            vm->protos[id].constants = cs.constants;
            vm->protos[id].strings = cs.strings;
            vm->protos[id].code_count = cs.code_count;
            vm->protos[id].const_count = cs.const_count;
            vm->protos[id].str_count = cs.str_count;
            vm->protos[id].param_count = cs.param_count;
            vm->protos[id].name = strdup(stmt->as.func_decl.name);
            vm->closures[id] = (kyClosure *)calloc(1, sizeof(kyClosure));
            if (vm->closures[id]) {
                vm->closures[id]->proto = &vm->protos[id];
            }
            if (!vm->protos[id].code || !vm->protos[id].name || !vm->closures[id]) {
                free(vm->protos[id].code);
                free(vm->protos[id].name);
                free(vm->closures[id]);
                vm->protos[id].code = NULL;
                vm->protos[id].name = NULL;
                vm->closures[id] = NULL;
                cs.constants = NULL;
                cs.strings = NULL;
            } else {
                cs.constants = NULL;
                cs.strings = NULL;
            }
        }
        if (cs.code) free(cs.code);
        if (cs.constants) free(cs.constants);
        if (cs.strings) free(cs.strings);
        for (int j = 0; j < cs.local_count; j++) free(cs.local_names[j]);
    }

    return &vm->protos[0];
}
