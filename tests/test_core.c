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
    KY_CHECK(stats.alloc_count == 2);
    KY_CHECK(stats.live_bytes == 300);
    p1 = ky_mem_realloc(&al, p1, 300);
    KY_CHECK(p1 != NULL);
    KY_CHECK(stats.live_bytes == 500);
    ky_mem_free(&al, p1);
    ky_mem_free(&al, p2);
    KY_CHECK(stats.free_count == 2);
    KY_CHECK(stats.live_bytes == 0);
}

static void test_arena(void) {
    kyAllocator al = ky_default_allocator();
    kyArena a = ky_arena_create(256, &al);
    KY_CHECK(a.base != NULL);
    KY_CHECK(ky_arena_used_bytes(&a) == 0);
    KY_CHECK(ky_arena_capacity(&a) >= 256);

    void *p1 = ky_arena_alloc_aligned(&a, 32);
    KY_CHECK(p1 != NULL);
    memcpy(p1, "hello", 6);

    void *p2 = ky_arena_alloc_aligned(&a, 64);
    KY_CHECK(p2 != NULL);

    KY_CHECK(ky_arena_used_bytes(&a) > 0);

    ky_arena_reset(&a);
    KY_CHECK(ky_arena_used_bytes(&a) == 0);
    /* pointers remain valid after reset */
    KY_CHECK(memcmp(p1, "hello", 6) == 0);

    /* reallocate after reset — same backing block */
    void *p3 = ky_arena_alloc_aligned(&a, 128);
    KY_CHECK(p3 != NULL);

    /* grow: arena grows on demand, so this succeeds */
    size_t cap_before = ky_arena_capacity(&a);
    void *big = ky_arena_alloc_aligned(&a, ky_arena_capacity(&a) + 1024);
    KY_CHECK(big != NULL); /* arena auto-grows */
    KY_CHECK(ky_arena_capacity(&a) > cap_before);

    ky_arena_destroy(&a);
    KY_CHECK(a.base == NULL);
}

static void test_arena_alignment(void) {
    kyAllocator al = ky_default_allocator();
    kyArena a = ky_arena_create(128, &al);
    void *p = ky_arena_alloc(&a, 13, 64);
    KY_CHECK(p != NULL);
    KY_CHECK(((uintptr_t)p & 63u) == 0);
    ky_arena_destroy(&a);
}

static void test_pool(void) {
    kyAllocator al = ky_default_allocator();
    kyPool p = ky_pool_create(sizeof(int), 4, &al);
    KY_CHECK(ky_pool_capacity(&p) >= 4);
    KY_CHECK(ky_pool_count(&p) == 0);

    int *x = (int *)ky_pool_alloc(&p);
    KY_CHECK(x != NULL);
    *x = 42;
    KY_CHECK(ky_pool_count(&p) == 1);

    int *y = (int *)ky_pool_alloc(&p);
    KY_CHECK(y != NULL);
    KY_CHECK(x != y);

    ky_pool_free(&p, x);
    KY_CHECK(ky_pool_count(&p) == 1);

    int *z = (int *)ky_pool_alloc(&p);
    KY_CHECK(z != NULL);
    /* may reuse x's slot */
    KY_CHECK(z == x || z == y);

    ky_pool_reset(&p);
    KY_CHECK(ky_pool_count(&p) == 0);
    int *w = (int *)ky_pool_alloc(&p);
    KY_CHECK(w != NULL);

    ky_pool_destroy(&p);
    KY_CHECK(ky_pool_capacity(&p) == 0);
}

static void test_pool_grow(void) {
    kyAllocator al = ky_default_allocator();
    kyPool p = ky_pool_create(sizeof(double), 2, &al);
    size_t cap0 = ky_pool_capacity(&p);
    double *last = NULL;
    for (size_t i = 0; i <= cap0; i++) {
        last = (double *)ky_pool_alloc(&p);
        KY_CHECK(last != NULL);
        *last = (double)i;
    }
    KY_CHECK(ky_pool_capacity(&p) > cap0);
    KY_CHECK(ky_pool_count(&p) == cap0 + 1);
    ky_pool_destroy(&p);
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
    test_arena();
    test_arena_alignment();
    test_pool();
    test_pool_grow();
    test_hash_stability();
}

KY_TEST_MAIN()
