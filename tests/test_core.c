#include "kronyx/kronyx.h"
#include "kytest.h"

int ky_test_failures = 0;
int ky_test_assertions = 0;

static void test_array(void) {
    kyAllocator al = ky_default_allocator();
    kyArray a;
    ky_array_init(&a, &al, sizeof(int), 0);
    KY_CHECK(a.len == 0);
    for (int i = 0; i < 1000; i++) {
        ky_array_push(&a, &i);
    }
    KY_CHECK(a.len == 1000);
    KY_CHECK(*(int *)ky_array_get(&a, 0) == 0);
    KY_CHECK(*(int *)ky_array_get(&a, 999) == 999);
    KY_CHECK(ky_array_get(&a, 1000) == NULL);
    ky_array_remove_swap(&a, 0);
    KY_CHECK(a.len == 999);
    KY_CHECK(*(int *)ky_array_get(&a, 0) == 999);
    ky_array_clear(&a);
    KY_CHECK(a.len == 0);
    ky_array_deinit(&a);
}

static void test_hashmap(void) {
    kyAllocator al = ky_default_allocator();
    kyHashMap m;
    ky_hashmap_init(&m, &al, 0);
    KY_CHECK(ky_hashmap_count(&m) == 0);
    int v1 = 1, v2 = 2, v3 = 3;
    ky_hashmap_set(&m, "alpha", &v1);
    ky_hashmap_set(&m, "beta", &v2);
    ky_hashmap_set(&m, "gamma", &v3);
    KY_CHECK(ky_hashmap_count(&m) == 3);
    KY_CHECK(ky_hashmap_get(&m, "alpha") == &v1);
    KY_CHECK(ky_hashmap_get(&m, "beta") == &v2);
    KY_CHECK(ky_hashmap_get(&m, "gamma") == &v3);
    KY_CHECK(ky_hashmap_get(&m, "missing") == NULL);
    KY_CHECK(ky_hashmap_has(&m, "beta") == 1);
    KY_CHECK(ky_hashmap_remove(&m, "beta") == 1);
    KY_CHECK(ky_hashmap_count(&m) == 2);
    KY_CHECK(ky_hashmap_has(&m, "beta") == 0);
    int v4 = 4;
    ky_hashmap_set(&m, "alpha", &v4);
    KY_CHECK(ky_hashmap_get(&m, "alpha") == &v4);
    for (int i = 0; i < 2000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        char *owned = (char *)ky_mem_dup(&al, key, strlen(key) + 1);
        ky_hashmap_set_key(&m, owned, &v1);
    }
    KY_CHECK(ky_hashmap_count(&m) == 2002);
    KY_CHECK(ky_hashmap_get(&m, "key1999") == &v1);
    KY_CHECK(ky_hashmap_get(&m, "alpha") == &v4);
    ky_hashmap_deinit(&m);
}

static void test_string(void) {
    kyAllocator al = ky_default_allocator();
    kyString s;
    ky_string_init(&s, &al);
    ky_string_append(&s, "hello");
    ky_string_appendf(&s, " %d", 42);
    ky_string_append_n(&s, "!", 1);
    KY_CHECK(strcmp(ky_string_cstr(&s), "hello 42!") == 0);
    KY_CHECK(s.len == strlen("hello 42!"));
    ky_string_clear(&s);
    KY_CHECK(s.len == 0);
    KY_CHECK(strcmp(ky_string_cstr(&s), "") == 0);
    ky_string_append(&s, "again");
    KY_CHECK(strcmp(ky_string_cstr(&s), "again") == 0);
    ky_string_deinit(&s);
}

static void test_memory(void) {
    kyMemStats stats;
    kyAllocator al = ky_tracking_allocator(&stats);
    void *p1 = ky_mem_alloc(&al, 100);
    KY_CHECK(p1 != NULL);
    void *p2 = ky_mem_alloc(&al, 200);
    KY_CHECK(p2 != NULL);
    p1 = ky_mem_realloc(&al, p1, 300);
    KY_CHECK(p1 != NULL);
    ky_mem_free(&al, p1);
    ky_mem_free(&al, p2);
}

static void test_hash_stability(void) {
    KY_CHECK(ky_hash_str("kronyx") == ky_hash_str("kronyx"));
    KY_CHECK(ky_hash_str("a") != ky_hash_str("b"));
    uint64_t h1 = ky_hash_bytes("abc", 3, 7);
    uint64_t h2 = ky_hash_bytes("abc", 3, 7);
    KY_CHECK(h1 == h2);
}

void ky_test_run_all(void) {
    test_array();
    test_hashmap();
    test_string();
    test_memory();
    test_hash_stability();
}

KY_TEST_MAIN()
