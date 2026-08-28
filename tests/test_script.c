#include "kronyx/script.h"
#include <stdio.h>
#include <string.h>

static int assertions = 0;
static int failures = 0;

#define ASSERT(cond, msg) do { \
    assertions++; \
    if (!(cond)) { \
        failures++; \
        printf("FAIL: %s at line %d\n", msg, __LINE__); \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while(0)

static void test_lexer_basic(void) {
    const char *src = "var x = 42;";
    kyLexer *lx = kyx_lexer_create(src, "test");
    ASSERT(lx != NULL, "create lexer");

    kyToken t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_VAR, "token: var");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_IDENT, "token: ident 'x'");
    ASSERT(t.len == 1, "ident length 1");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_ASSIGN, "token: =");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_INT_LIT, "token: int 42");
    ASSERT(t.as.ival == 42, "int value 42");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_SEMI, "token: ;");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_EOF, "token: EOF");

    kyx_lexer_destroy(lx);
}

static void test_lexer_keywords(void) {
    const char *src = "if true false nil function class return break continue";
    kyLexer *lx = kyx_lexer_create(src, "test");
    ASSERT(lx != NULL, "create lexer for keywords");

    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_IF, "keyword: if");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_TRUE, "keyword: true");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_FALSE, "keyword: false");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_NIL_LIT, "keyword: nil");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_FUNCTION, "keyword: function");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_CLASS, "keyword: class");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_RETURN, "keyword: return");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_BREAK, "keyword: break");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_CONTINUE, "keyword: continue");
    ASSERT(kyx_lexer_next(lx).kind == KYX_TK_EOF, "EOF after keywords");

    kyx_lexer_destroy(lx);
}

static void test_lexer_operators(void) {
    const char *src = "+ - * / % = == != < <= > >= && || ! & | ^ << >>";
    kyLexer *lx = kyx_lexer_create(src, "test");
    ASSERT(lx != NULL, "create lexer for operators");

    /* Store tokens in locals to avoid expression-evaluation order issues */
    kyToken _t0 = kyx_lexer_next(lx);
    kyToken _t1 = kyx_lexer_next(lx);
    kyToken _t2 = kyx_lexer_next(lx);
    kyToken _t3 = kyx_lexer_next(lx);
    kyToken _t4 = kyx_lexer_next(lx);
    kyToken _t5 = kyx_lexer_next(lx);
    kyToken _t6 = kyx_lexer_next(lx);
    kyToken _t7 = kyx_lexer_next(lx);
    kyToken _t8 = kyx_lexer_next(lx);
    kyToken _t9 = kyx_lexer_next(lx);
    kyToken _t10 = kyx_lexer_next(lx);
    kyToken _t11 = kyx_lexer_next(lx);
    kyToken _t12 = kyx_lexer_next(lx);
    kyToken _t13 = kyx_lexer_next(lx);
    kyToken _t14 = kyx_lexer_next(lx);
    kyToken _t15 = kyx_lexer_next(lx);
    kyToken _t16 = kyx_lexer_next(lx);
    kyToken _t17 = kyx_lexer_next(lx);
    kyToken _t18 = kyx_lexer_next(lx);
    kyToken _t19 = kyx_lexer_next(lx);
    kyToken _t20 = kyx_lexer_next(lx);
    ASSERT(_t0.kind == KYX_TK_PLUS,   "token: +");
    ASSERT(_t1.kind == KYX_TK_MINUS,  "token: -");
    ASSERT(_t2.kind == KYX_TK_STAR,   "token: *");
    ASSERT(_t3.kind == KYX_TK_SLASH,  "token: /");
    ASSERT(_t4.kind == KYX_TK_MOD,    "token: %");
    ASSERT(_t5.kind == KYX_TK_ASSIGN, "token: =");
    ASSERT(_t6.kind == KYX_TK_EQ,     "token: ==");
    ASSERT(_t7.kind == KYX_TK_NEQ,    "token: !=");
    ASSERT(_t8.kind == KYX_TK_LT,     "token: <");
    ASSERT(_t9.kind == KYX_TK_LE,     "token: <=");
    ASSERT(_t10.kind == KYX_TK_GT,    "token: >");
    ASSERT(_t11.kind == KYX_TK_GE,    "token: >=");
    ASSERT(_t12.kind == KYX_TK_AND,   "token: &&");
    ASSERT(_t13.kind == KYX_TK_OR,    "token: ||");
    ASSERT(_t14.kind == KYX_TK_NOT,   "token: !");
    ASSERT(_t15.kind == KYX_TK_BNOT,  "token: &");
    ASSERT(_t16.kind == KYX_TK_BNOT,  "token: |");
    ASSERT(_t17.kind == KYX_TK_BNOT,  "token: ^");
    ASSERT(_t18.kind == KYX_TK_SHL,   "token: <<");
    ASSERT(_t19.kind == KYX_TK_SHR,   "token: >>");
    ASSERT(_t20.kind == KYX_TK_EOF,   "EOF");

    kyx_lexer_destroy(lx);
}

static void test_lexer_strings_numbers(void) {
    const char *src = "\"hello world\" 3.14 0xFF -42";
    kyLexer *lx = kyx_lexer_create(src, "test");
    ASSERT(lx != NULL, "create lexer for literals");

    kyToken t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_STRING_LIT, "token: string");
    ASSERT(t.len == 13, "string len 13");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_FLOAT_LIT, "token: float");
    ASSERT(fabs(t.as.fval - 3.14) < 0.001, "float value 3.14");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_INT_LIT, "token: hex int");
    ASSERT(t.as.ival == 255, "hex int 0xFF = 255");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_MINUS, "token: minus operator");
    /* negative number: minus then positive number */
    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_INT_LIT, "token: int 42");
    ASSERT(t.as.ival == 42, "int value 42");

    kyx_lexer_destroy(lx);
}

static void test_lexer_forced_comments(void) {
    const char *src = "x[/*<!--{comment}-->*/] = 1;";
    kyLexer *lx = kyx_lexer_create(src, "test");
    ASSERT(lx != NULL, "create lexer with forced comments");

    kyToken t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_IDENT, "token: ident after comment");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_ASSIGN, "token: = after comment");

    t = kyx_lexer_next(lx);
    ASSERT(t.kind == KYX_TK_INT_LIT, "token: int after comment");
    ASSERT(t.as.ival == 1, "int value 1");

    kyx_lexer_destroy(lx);
}

static void test_lexer_line_numbers(void) {
    const char *src = "a\nb\nc";
    kyLexer *lx = kyx_lexer_create(src, "test");
    ASSERT(lx != NULL, "create lexer for line numbers");

    kyToken t = kyx_lexer_next(lx);
    ASSERT(t.line == 1, "first token on line 1");
    t = kyx_lexer_next(lx);
    ASSERT(t.line == 2, "second token on line 2");
    t = kyx_lexer_next(lx);
    ASSERT(t.line == 3, "third token on line 3");

    kyx_lexer_destroy(lx);
}

static void test_vm_create_destroy(void) {
    kyVM *vm = ky_vm_create(NULL);
    ASSERT(vm != NULL, "create VM");
    ASSERT(strcmp(ky_vm_last_error(vm), "no vm") != 0, "last error empty initially");
    ky_vm_destroy(vm);
    ASSERT(ky_vm_last_error(NULL) != NULL, "last error null-safe");
}

static void test_vm_load_string(void) {
    kyVM *vm = ky_vm_create(NULL);
    ASSERT(vm != NULL, "create VM for load test");
    int r = ky_vm_load_string(vm, "", "empty");
    ASSERT(r == 0, "load empty string succeeds");
    ASSERT(!ky_vm_last_error(vm)[0] || ky_vm_last_error(vm)[0] == '\0' ||
           strcmp(ky_vm_last_error(vm), "no vm") != 0, "no error after empty load");
    ky_vm_destroy(vm);
}

static void test_vm_register_native(void) {
    kyVM *vm = ky_vm_create(NULL);
    ASSERT(vm != NULL, "create VM for native test");
    ky_vm_register_native(vm, "test_ns", "my_func", NULL, NULL);
    ky_vm_destroy(vm);
}

static void test_null_safety(void) {
    kyToken t;
    memset(&t, 0, sizeof(t));
    kyLexer *lx = kyx_lexer_create(NULL, "test");
    if (lx) kyx_lexer_destroy(lx);
    ASSERT(kyx_lexer_error(NULL) != NULL, "lexer error null-safe");
    ASSERT(kyx_lexer_has_error(NULL) == 0, "lexer has_error null-safe");

    kyVM *vm = ky_vm_create(NULL);
    ASSERT(ky_vm_last_error(vm) != NULL, "vm last_error non-null");
    ASSERT(ky_vm_call(NULL, NULL, NULL, 0, NULL) == -1, "vm call with null vm fails");
    ASSERT(ky_vm_load_file(vm, NULL) == -1, "vm load file with null path fails");
    ky_vm_destroy(vm);
}

int main(void) {
    printf("=== Script (kyx) Test ===\n");

    test_lexer_basic();
    test_lexer_keywords();
    test_lexer_operators();
    test_lexer_strings_numbers();
    test_lexer_forced_comments();
    test_lexer_line_numbers();
    test_vm_create_destroy();
    test_vm_load_string();
    test_vm_register_native();
    test_null_safety();

    printf("\n=== %d tests ran, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
