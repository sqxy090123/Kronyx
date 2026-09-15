#include "kronyx/kronyx.h"
#include "kronyx/script.h"
#include "kronyx/render.h"
#include "kytest.h"
#include <stdint.h>

int ky_test_failures = 0;
int ky_test_assertions = 0;

static void run_vm(const char *src, int *ok) {
    kyVM *vm = ky_vm_create(NULL);
    KY_CHECK(vm != NULL);
    if (!vm) return;
    int rc = ky_vm_load_string(vm, src, "<test>");
    if (ok) *ok = (rc == 0);
    if (rc == 0) {
        kyValue ret;
        ky_vm_call(vm, "main", NULL, 0, &ret);
    }
    ky_vm_destroy(vm);
}

static void test_vm_malformed_src(void) {
    const char *bad[] = {
        "(((",
        "x = +; y = -;",
        "for(;;){",
        "function main() { return 1; }",
        "",
        "if () {}",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        int ok = 0;
        run_vm(bad[i], &ok);
    }
}

static void test_vm_valid_src(void) {
    int ok = 0;
    run_vm("function main() { var x = 1; return x + 2; }", &ok);
    KY_CHECK(ok);
}

static void test_console_buffer_oob(void) {
    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    KY_CHECK(rd != NULL);
    uint8_t data[4] = {1, 2, 3, 4};
    kyBuffer *b = ky_rd_create_buffer(rd, 4, data, 1);
    KY_CHECK(b != NULL);
    uint8_t nd[4] = {9, 9, 9, 9};
    KY_CHECK(ky_rd_update_buffer(rd, b, 2, 2, nd) == 0);
    KY_CHECK(ky_rd_update_buffer(rd, b, 2, 3, nd) == -1);
    KY_CHECK(ky_rd_update_buffer(rd, b, 4, 2, nd) == -1);
    KY_CHECK(ky_rd_update_buffer(rd, b, 3, 2, nd) == -1);
    uint8_t zero[1];
    KY_CHECK(ky_rd_update_buffer(rd, b, 0, 0, zero) == -1);
    kyBuffer *b0 = ky_rd_create_buffer(rd, 0, NULL, 0);
    KY_CHECK(b0 != NULL);
    KY_CHECK(ky_rd_update_buffer(rd, b0, 0, 1, nd) == -1);
    ky_rd_destroy_buffer(rd, b);
    ky_rd_destroy_buffer(rd, b0);
    ky_rd_destroy(rd);
}

static void test_gl_stub_buffer_oob(void) {
    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_GL, NULL);
    KY_CHECK(rd != NULL);
    uint8_t data[4] = {1, 2, 3, 4};
    kyBuffer *b = ky_rd_create_buffer(rd, 4, data, 1);
    uint8_t nd[4] = {5, 6, 7, 8};
    if (b) {
        KY_CHECK(ky_rd_update_buffer(rd, b, 2, 2, nd) == 0);
        KY_CHECK(ky_rd_update_buffer(rd, b, 2, 3, nd) == -1);
    }
    if (b) ky_rd_destroy_buffer(rd, b);
    ky_rd_destroy(rd);
}

void ky_test_run_all(void) {
    test_vm_valid_src();
    test_vm_malformed_src();
    test_console_buffer_oob();
    test_gl_stub_buffer_oob();
}

KY_TEST_MAIN()
