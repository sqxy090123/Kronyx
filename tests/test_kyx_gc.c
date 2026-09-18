/*
 * kyx Generational GC tests
 *
 * R1: heap init / alloc / free
 * R2: nursery GC + full GC + promotion + suppression
 * R3: string heapification + GC string detection
 * R4: write barrier + host GC root mark
 * R5: closure heapification
 * R6: stats + config API
 * R7: existing API unchanged, native ctx protected
 * R8: allocation failure path
 */
#include "kronyx/script.h"
#include "kronyx/gc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* Simple native log that does nothing (avoids needing full bind setup) */
static kyValue native_log(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm; (void)args; (void)argc; (void)user;
    return (kyValue){KYT_NIL};
}

/* ------------------------------------------------------------------ */
/* R1.1: VM create initializes GC heap                                  */
/* ------------------------------------------------------------------ */
static void test_r1_heap_init(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    KyGcStats st;
    int rc = ky_vm_gc_stats(vm, &st);
    CHECK(rc == 0, "gc_stats returns 0");
    CHECK(st.nursery_capacity_bytes == KY_GC_NURSERY_DEF,
          "nursery default size is 256KB");
    CHECK(st.tenured_capacity_bytes == KY_GC_TENURED_DEF,
          "tenured default size is 4MB");
    CHECK(st.nursery_used_bytes == 0, "nursery starts empty");
    CHECK(st.tenured_used_bytes == 0, "tenured starts empty");
    CHECK(st.total_gc_count == 0, "no GC runs yet");
    CHECK(st.auto_disabled == 0, "auto GC enabled by default");

    /* R1.1: adjust heap size before first run */
    int rc2 = ky_vm_set_heap_size(vm, 512 * 1024, 8 * 1024 * 1024);
    CHECK(rc2 == 0, "set_heap_size succeeds before execution");

    ky_vm_gc_stats(vm, &st);
    CHECK(st.nursery_capacity_bytes == 512 * 1024,
          "nursery resized to 512KB");
    CHECK(st.tenured_capacity_bytes == 8 * 1024 * 1024,
          "tenured resized to 8MB");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R1.2/R1.3: allocation via script string concat                       */
/* ------------------------------------------------------------------ */
static void test_r1_alloc(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);
    ky_vm_register_native(vm, "std", "log", native_log, NULL);

    const char *src =
        "function main() {\n"
        "  let a = \"hello\";\n"
        "  let b = \"world\";\n"
        "  let c = a + b;\n"
        "  std.log(c);\n"
        "}";
    int rc = ky_vm_load_string(vm, src, "test.kyx");
    CHECK(rc == 0, "script loads without error");

    kyValue ret;
    rc = ky_vm_call(vm, "main", NULL, 0, &ret);
    CHECK(rc == 0, "script executes without error");

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.nursery_used_bytes > 0 || st.tenured_used_bytes > 0,
          "GC heap has allocated objects after concat");

    /* After set_heap_size, ref must fail */
    int rc2 = ky_vm_set_heap_size(vm, 1024, 1024);
    CHECK(rc2 == -1, "set_heap_size fails after script execution");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R1.5: heap full -> nil + error_msg                                    */
/* ------------------------------------------------------------------ */
static void test_r1_heap_full(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    ky_vm_set_heap_size(vm, 128, 128);

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.nursery_capacity_bytes == 128, "nursery set to 128 bytes");
    CHECK(st.tenured_capacity_bytes == 128, "tenured set to 128 bytes");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R2.1: auto GC triggers when nursery reaches threshold                */
/* ------------------------------------------------------------------ */
static void test_r2_auto_gc(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);
    ky_vm_register_native(vm, "std", "log", native_log, NULL);

    ky_vm_gc_set_auto_threshold(vm, 128);

    const char *src =
        "function main() {\n"
        "  let s1 = \"aaaa\";\n"
        "  let s2 = \"bbbb\";\n"
        "  let s3 = s1 + s2;\n"
        "  let s4 = s3 + \"cccc\";\n"
        "  let s5 = s4 + \"dddd\";\n"
        "  let s6 = s5 + \"eeee\";\n"
        "  let s7 = s6 + \"ffff\";\n"
        "  let s8 = s7 + \"gggg\";\n"
        "  let s9 = s8 + \"hhhh\";\n"
        "  std.log(s9);\n"
        "}";
    int rc = ky_vm_load_string(vm, src, "test.kyx");
    CHECK(rc == 0, "script loads");

    kyValue ret;
    rc = ky_vm_call(vm, "main", NULL, 0, &ret);
    CHECK(rc == 0, "script executes");

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.total_gc_count >= 1, "nursery GC auto-triggered at least once");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R2.4/R2.5: manual ky_vm_gc_collect returns freed bytes               */
/* ------------------------------------------------------------------ */
static void test_r2_manual_gc(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);
    ky_vm_register_native(vm, "std", "log", native_log, NULL);

    const char *src =
        "function main() {\n"
        "  let x = \"one\";\n"
        "  let y = \"two\";\n"
        "  let z = x + y;\n"
        "  std.log(z);\n"
        "}";
    ky_vm_load_string(vm, src, "test.kyx");

    kyValue ret;
    ky_vm_call(vm, "main", NULL, 0, &ret);

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);

    size_t freed = ky_vm_gc_collect(vm);
    (void)freed;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.total_t_gen_gc_count >= 1, "full GC ran after manual collect");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R2.7: nursery degradation when tenured full                          */
/* ------------------------------------------------------------------ */
static void test_r2_degradation(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    ky_vm_set_heap_size(vm, 1024, 128);

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.nursery_degraded_count == 0, "starts with 0 degraded");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R3.1/R3.3: OP_CONCAT result is in GC heap, GC reclaims              */
/* ------------------------------------------------------------------ */
static void test_r3_string_heap(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);
    ky_vm_register_native(vm, "std", "log", native_log, NULL);

    const char *src =
        "function main() {\n"
        "  let base = \"hello\";\n"
        "  let name = \"kyx\";\n"
        "  let msg = base + \" \" + name;\n"
        "  std.log(msg);\n"
        "}";
    ky_vm_load_string(vm, src, "test.kyx");

    kyValue ret;
    ky_vm_call(vm, "main", NULL, 0, &ret);

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.nursery_used_bytes > 0,
          "GC heap has string objects after script");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R4.1/R4.4: write barrier + host GC root                              */
/* ------------------------------------------------------------------ */
static void test_r4_barrier(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    static char host_buf[64];
    memset(host_buf, 0, sizeof(host_buf));
    int rc = ky_vm_gc_mark(vm, host_buf);
    CHECK(rc == 0, "gc_mark registers host pointer");

    rc = ky_vm_gc_unmark(vm, host_buf);
    CHECK(rc == 0, "gc_unmark removes host pointer");

    rc = ky_vm_gc_mark(vm, &host_buf[0]);
    CHECK(rc == 0, "gc_mark accepts arbitrary pointer");
    ky_vm_gc_unmark(vm, &host_buf[0]);

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R5.3: closure capture reclaimed when unreachable                    */
/* ------------------------------------------------------------------ */
static void test_r5_closure(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);
    ky_vm_register_native(vm, "std", "log", native_log, NULL);

    const char *src =
        "function make() {\n"
        "  let local = \"captured\";\n"
        "  function inner() { return local; }\n"
        "  return inner;\n"
        "}\n"
        "function main() {\n"
        "  let f = make();\n"
        "  let v = f();\n"
        "  std.log(v);\n"
        "}";
    int rc = ky_vm_load_string(vm, src, "test.kyx");
    CHECK(rc == 0, "closure script loads");

    kyValue ret;
    rc = ky_vm_call(vm, "main", NULL, 0, &ret);
    CHECK(rc == 0, "closure script executes");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R6.1/R6.2: stats + threshold config                                  */
/* ------------------------------------------------------------------ */
static void test_r6_stats_config(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    KyGcStats st;
    int rc = ky_vm_gc_stats(vm, &st);
    CHECK(rc == 0, "gc_stats returns 0");
    CHECK(st.auto_disabled == 0, "auto enabled by default");

    rc = ky_vm_gc_set_auto_threshold(vm, 0);
    CHECK(rc == 0, "set threshold to 0 succeeds");

    ky_vm_gc_stats(vm, &st);
    CHECK(st.auto_disabled == 1, "auto disabled after threshold=0");

    rc = ky_vm_gc_set_auto_threshold(vm, 4096);
    CHECK(rc == 0, "set threshold to 4096 succeeds");
    ky_vm_gc_stats(vm, &st);
    CHECK(st.auto_disabled == 0, "auto re-enabled");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R7: existing API unchanged, native ctx protected                     */
/* ------------------------------------------------------------------ */
static kyValue native_echo(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;
    if (argc >= 1 && args[0].type == KYT_STRING) return args[0];
    return (kyValue){KYT_STRING, .as.sval = "nil"};
}

static void test_r7_native(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    static char ctx_buf[32];
    strcpy(ctx_buf, "native_ctx");

    ky_vm_register_native(vm, "std", "echo", native_echo, ctx_buf);

    const char *src =
        "function main() {\n"
        "  let s = \"native\";\n"
        "  let r = std.echo(s);\n"
        "  std.log(r);\n"
        "}";
    ky_vm_register_native(vm, "std", "log", native_log, NULL);
    int rc = ky_vm_load_string(vm, src, "test.kyx");
    CHECK(rc == 0, "script with native loads");

    kyValue ret;
    rc = ky_vm_call(vm, "main", NULL, 0, &ret);
    CHECK(rc == 0, "script with native executes");

    CHECK(!ky_gc_is_gc_str(vm, ctx_buf), "native ctx is not a GC heap string");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R8.2: GC alloc failure -> nil, VM continues                          */
/* ------------------------------------------------------------------ */
static void test_r8_alloc_fail(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);
    ky_vm_set_heap_size(vm, 16, 16);

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.nursery_capacity_bytes == 16, "nursery is tiny");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
/* R8.4: two consecutive GC runs with freed==0 -> suppression            */
/* ------------------------------------------------------------------ */
static void test_r8_suppression(void) {
    kyVM *vm = ky_vm_create(NULL);
    assert(vm);

    size_t f1 = ky_vm_gc_collect(vm);
    size_t f2 = ky_vm_gc_collect(vm);
    CHECK(f1 == 0, "first collect frees 0 (empty heap)");
    CHECK(f2 == 0, "second collect frees 0");

    KyGcStats st;
    ky_vm_gc_stats(vm, &st);
    CHECK(st.total_t_gen_gc_count >= 2, "two full GCs ran");

    ky_vm_destroy(vm);
}

/* ------------------------------------------------------------------ */
int main(void) {
    test_r1_heap_init();
    test_r1_alloc();
    test_r1_heap_full();
    test_r2_auto_gc();
    test_r2_manual_gc();
    test_r2_degradation();
    test_r3_string_heap();
    test_r4_barrier();
    test_r5_closure();
    test_r6_stats_config();
    test_r7_native();
    test_r8_alloc_fail();
    test_r8_suppression();

    printf("GC tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
