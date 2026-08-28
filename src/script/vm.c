#include "kronyx/script.h"
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

static kyValue str_val(const char *s) {
    kyValue v; v.type = KYT_STRING; v.as.sval = s; return v;
}

static kyValue fn_val(kyClosure *c) {
    kyValue v; v.type = KYT_FUNCTION; v.as.closure = c; return v;
}

static kyValue native_val(void *p) {
    kyValue v; v.type = KYT_NATIVE; v.as.native = p; return v;
}

static int to_int(kyValue v) {
    if (v.type == KYT_INT) return (int)v.as.ival;
    if (v.type == KYT_FLOAT) return (int)v.as.fval;
    return 0;
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

static kyValue vm_get_var(kyVM *vm, int idx) {
    if (idx < 0 || idx >= KY_MAX_VARS) return nil_val();
    return vm->globals[idx];
}

static void vm_set_var(kyVM *vm, int idx, kyValue v) {
    if (idx >= 0 && idx < KY_MAX_VARS) vm->globals[idx] = v;
}

static kyValue call_proto(kyVM *vm, kyProto *proto, kyValue *args, int argc) {
    if (vm->stack_top + 10 > KY_MAX_STACK) {
        strncpy(vm->error_msg, "stack overflow", sizeof(vm->error_msg));
        vm->error_flag = 1;
        return nil_val();
    }
    int base = vm->stack_top;
    int pc = 0;
    int locals_count = proto->param_count;
    for (int i = 0; i < locals_count && i < argc; i++) {
        vm->stack[base + i] = args[i];
    }
    while (pc < proto->code_count) {
        kyInstr *instr = &proto->code[pc++];
        int A = instr->A, B = instr->B, C = instr->C;
        switch (instr->op) {
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
                vm->stack[base + A] = float_val(to_float(a) + to_float(b));
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
                vm->stack[base + A] = truthy ? a : a;
                break;
            }
            case OP_OR: {
                kyValue a = vm->stack[base + B];
                int truthy = (a.type != KYT_NIL && !(a.type == KYT_BOOL && !a.as.ival));
                vm->stack[base + A] = truthy ? a : vm->stack[base + C];
                break;
            }
            case OP_JMPIF:
                if (!vm->stack[base + A].as.ival) pc++;
                break;
            case OP_JMPIFNOT:
                if (vm->stack[base + A].as.ival) pc++;
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
                    kyClosure *cl = fn.as.closure;
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
                return ret;
            }
            case OP_EXIT:
                return nil_val();
            default:
                break;
        }
    }
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
    /* compile + execute stub: load global function and run */
    /* For now, simply mark success with empty program */
    vm->error_flag = 0;
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
    KY_UNUSED(func_name); KY_UNUSED(args); KY_UNUSED(argc); KY_UNUSED(ret);
    if (!vm) return -1;
    return 0;
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
