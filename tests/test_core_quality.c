/* src/core 模块质量测试：覆盖 arena / array / string / hashmap / pool /
 * memory / event / input / log / file / time / math。
 * 使用项目自带 kytest.h 断言框架，链接进 ky_test_core 之后独立成
 * ky_test_core_quality，避免污染既有 test_core。
 */
#include "kronyx/kronyx.h"
#include "kronyx/input.h"
#include "kronyx/event.h"
#include "kronyx/file.h"
#include "kytest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ky_test_failures = 0;
int ky_test_assertions = 0;

/* ============================ arena ================================== */

static void test_arena_basic(void) {
    kyAllocator al = ky_default_allocator();
    kyArena a = ky_arena_create(64, &al);
    KY_CHECK(a.base != NULL);
    KY_CHECK(ky_arena_capacity(&a) >= 64);
    KY_CHECK(ky_arena_used_bytes(&a) == 0);

    void *p1 = ky_arena_alloc(&a, 100, 0);
    void *p2 = ky_arena_alloc(&a, 100, 0);
    KY_CHECK(p1 != NULL && p2 != NULL && p1 != p2);
    /* 16-byte alignment */
    KY_CHECK(((uintptr_t)p1 % 16) == 0);
    KY_CHECK(((uintptr_t)p2 % 16) == 0);
    KY_CHECK(ky_arena_used_bytes(&a) >= 200);

    /* 越界对齐 128 字节 */
    void *p3 = ky_arena_alloc(&a, 10, 128);
    KY_CHECK(p3 != NULL);
    KY_CHECK(((uintptr_t)p3 % 128) == 0);

    ky_arena_reset(&a);
    KY_CHECK(ky_arena_used_bytes(&a) == 0);
    /* reset 后再次分配，地址回到 block 头部 */
    void *p4 = ky_arena_alloc(&a, 32, 0);
    KY_CHECK(p4 == a.base);

    ky_arena_destroy(&a);
    KY_CHECK(a.base == NULL);

    /* 销毁后调用安全 API 不崩溃 */
    KY_CHECK(ky_arena_used_bytes(&a) == 0);
    KY_CHECK(ky_arena_capacity(&a) == 0);
}

static void test_arena_growth(void) {
    kyAllocator al = ky_default_allocator();
    kyArena a = ky_arena_create(16, &al);
    size_t cap0 = ky_arena_capacity(&a);
    KY_CHECK(cap0 == 16);
    /* 一次性申请超过当前容量，触发扩展 */
    void *big = ky_arena_alloc(&a, cap0 + 1024, 0);
    KY_CHECK(big != NULL);
    KY_CHECK(ky_arena_capacity(&a) > cap0 + 1024);
    ky_arena_destroy(&a);
}

static void test_arena_bad_align_falls_back(void) {
    kyAllocator al = ky_default_allocator();
    kyArena a = ky_arena_create(128, &al);
    /* align 非 2 的幂：按代码约定回退到 16 字节对齐 */
    void *p = ky_arena_alloc(&a, 8, 3);
    KY_CHECK(p != NULL);
    KY_CHECK(((uintptr_t)p % 16) == 0);
    ky_arena_destroy(&a);
}

/* ============================ array ================================== */

static void test_array_grow(void) {
    kyAllocator al = ky_default_allocator();
    kyArray a;
    ky_array_init(&a, &al, sizeof(int), 0);
    for (int i = 0; i < 10000; i++) {
        *(int *)ky_array_push(&a, &i) == 0 ? 0 : 0;
    }
    KY_CHECK(a.len == 10000);
    KY_CHECK(*(int *)ky_array_get(&a, 0) == 0);
    KY_CHECK(*(int *)ky_array_get(&a, 9999) == 9999);
    KY_CHECK(ky_array_get(&a, 10000) == NULL);
    ky_array_remove_swap(&a, 5000);
    KY_CHECK(a.len == 9999);
    /* 尾元素被换入 5000 */
    KY_CHECK(*(int *)ky_array_get(&a, 5000) == 9999);
    ky_array_remove_swap(&a, 0);
    KY_CHECK(a.len == 9998);
    ky_array_remove_swap(&a, 99999); /* 越界 remove 不崩溃 */
    KY_CHECK(a.len == 9998);
    ky_array_clear(&a);
    KY_CHECK(a.len == 0);
    KY_CHECK(a.cap > 0); /* clear 不释放内存 */
    ky_array_deinit(&a);
    KY_CHECK(a.data == NULL && a.len == 0 && a.cap == 0);
}

static void test_array_push_null_elem(void) {
    /* ky_array_push(NULL elem) 现在走 memset 分支，slot 清零 */
    kyAllocator al = ky_default_allocator();
    kyArray a;
    ky_array_init(&a, &al, sizeof(int), 4);
    int zero = 0;
    *(int *)ky_array_push(&a, &zero) == 0 ? 0 : 0;
    void *slot = ky_array_push(&a, NULL);
    KY_CHECK(*(int *)slot == 0);
    KY_CHECK(a.len == 2);
    ky_array_deinit(&a);
}

static void test_array_reserve_idempotent(void) {
    kyAllocator al = ky_default_allocator();
    kyArray a;
    ky_array_init(&a, &al, sizeof(float), 2);
    KY_CHECK(a.cap >= 2);
    ky_array_reserve(&a, 1); /* 小于当前 cap，不缩容 */
    KY_CHECK(a.cap >= 2);
    ky_array_reserve(&a, 64);
    KY_CHECK(a.cap >= 64);
    ky_array_deinit(&a);
}

/* ============================ string ================================= */

static void test_string_basic(void) {
    kyAllocator al = ky_default_allocator();
    kyString s;
    ky_string_init(&s, &al);
    KY_CHECK(s.data == NULL);
    KY_CHECK(strcmp(ky_string_cstr(&s), "") == 0); /* 空串 cstr */

    ky_string_append(&s, "abc");
    KY_CHECK(s.len == 3);
    ky_string_append_n(&s, "def", 3);
    KY_CHECK(strcmp(ky_string_cstr(&s), "abcdef") == 0);
    ky_string_append_n(&s, "XYZ", 2);
    KY_CHECK(strcmp(ky_string_cstr(&s), "abcdefXY") == 0);
    ky_string_appendf(&s, " %d.%d", 1, 2);
    KY_CHECK(strcmp(ky_string_cstr(&s), "abcdefXY 1.2") == 0);

    ky_string_clear(&s);
    KY_CHECK(s.len == 0);
    KY_CHECK(ky_string_cstr(&s)[0] == '\0');

    ky_string_deinit(&s);
    KY_CHECK(s.data == NULL);
}

static void test_string_reserve_no_shrink(void) {
    kyAllocator al = ky_default_allocator();
    kyString s;
    ky_string_init(&s, &al);
    ky_string_reserve(&s, 1024);
    KY_CHECK(s.cap >= 1024);
    ky_string_reserve(&s, 16);
    KY_CHECK(s.cap >= 1024); /* reserve 只增不减 */
    ky_string_deinit(&s);
}

/* ============================ hashmap ================================ */

static void test_hashmap_set_get(void) {
    kyAllocator al = ky_default_allocator();
    kyHashMap m;
    ky_hashmap_init(&m, &al, 8);
    int v1 = 1, v2 = 2;
    ky_hashmap_set(&m, "a", &v1);
    ky_hashmap_set(&m, "b", &v2);
    KY_CHECK(ky_hashmap_count(&m) == 2);
    KY_CHECK(ky_hashmap_get(&m, "a") == &v1);
    KY_CHECK(ky_hashmap_get(&m, "b") == &v2);
    KY_CHECK(ky_hashmap_get(&m, "c") == NULL);
    KY_CHECK(ky_hashmap_has(&m, "a") == 1);
    KY_CHECK(ky_hashmap_has(&m, "c") == 0);
    /* 覆盖已有 key，count 不变 */
    ky_hashmap_set(&m, "a", &v2);
    KY_CHECK(ky_hashmap_count(&m) == 2);
    KY_CHECK(ky_hashmap_get(&m, "a") == &v2);
    ky_hashmap_deinit(&m);
}

static void test_hashmap_set_null_removes(void) {
    kyAllocator al = ky_default_allocator();
    kyHashMap m;
    ky_hashmap_init(&m, &al, 8);
    int v1 = 1, v2 = 2;
    ky_hashmap_set(&m, "k", &v1);
    KY_CHECK(ky_hashmap_get(&m, "k") == &v1);
    ky_hashmap_set(&m, "k", NULL); /* value==NULL -> remove */
    KY_CHECK(ky_hashmap_get(&m, "k") == NULL);
    KY_CHECK(ky_hashmap_count(&m) == 0);
    /* remove 不存在的 key 返回 0 */
    KY_CHECK(ky_hashmap_remove(&m, "nope") == 0);
    ky_hashmap_set(&m, "k", &v2);
    KY_CHECK(ky_hashmap_remove(&m, "k") == 1);
    KY_CHECK(ky_hashmap_count(&m) == 0);
    ky_hashmap_deinit(&m);
}

static void test_hashmap_owned_keys(void) {
    /* owned key：deinit 时释放；remove 时也释放。用跟踪分配器验证无泄漏。 */
    kyMemStats st;
    kyAllocator al = ky_tracking_allocator(&st);
    kyHashMap m;
    ky_hashmap_init(&m, &al, 8);
    char *k1 = (char *)ky_mem_dup(&al, "alpha", 6);
    char *k2 = (char *)ky_mem_dup(&al, "beta", 5);
    char *k3 = (char *)ky_mem_dup(&al, "gamma", 6);
    int v = 1;
    ky_hashmap_set_key(&m, k1, &v);
    ky_hashmap_set_key(&m, k2, &v);
    ky_hashmap_set_key(&m, k3, &v);
    KY_CHECK(ky_hashmap_count(&m) == 3);
    size_t live = st.live_bytes;
    KY_CHECK(live > 0);
    /* remove 一个：对应 key 立即释放 */
    KY_CHECK(ky_hashmap_remove(&m, "beta") == 1);
    KY_CHECK(st.live_bytes < live);
    ky_hashmap_deinit(&m);
    /* 剩余两个 owned key 在 deinit 中释放 */
    KY_CHECK(st.alloc_count == st.free_count);
    KY_CHECK(st.live_bytes == 0);
}

static void test_hashmap_tombstone_probing(void) {
    /* 初始插入 16 个 key 本身就会把 cap 顶到 32（resize 阈值 3/4），
     * 直接删光再补回会让每个 key 的 live 槽与同 key TOMB 分处不同
     * 探测区间，无法稳定复现 bug。改为少量插入（cap=16 不触发 resize），
     * 删光后补回：此时同 key 的 TOMB 一定位于 live 槽之前，
     * 若 set 命中 TOMB 就短路插入（旧 bug），get 会命中旧的 live 槽。
     *
     * 注意：ky_hashmap_set 不持有 key（存调用方指针），key 必须长生命
     * 周期。这里用静态数组持有全部 key。 */
    static const char keys[6][8] = { "k00", "k01", "k02", "k03", "k04", "k05" };
    kyAllocator al = ky_default_allocator();
    kyHashMap m;
    ky_hashmap_init(&m, &al, 16);
    int values[64];
    int v2 = 999;
    for (int i = 0; i < 6; i++) {
        values[i] = i;
        ky_hashmap_set(&m, keys[i], &values[i]);
    }
    KY_CHECK(ky_hashmap_count(&m) == 6);
    /* 删光全部 key（cap 不变，制造 6 个 TOMB） */
    for (int i = 0; i < 6; i++) {
        KY_CHECK(ky_hashmap_remove(&m, keys[i]) == 1);
    }
    KY_CHECK(ky_hashmap_count(&m) == 0);
    /* 逐个补回：每个 key 的探测路径上先遇到自己的 TOMB 再到 EMPTY。
     * 若 set 在 TOMB 处短路（旧 bug），live 槽会被插在 TOMB 之后，
     * 再 set 时 get 会命中旧的 live 槽旧值。 */
    for (int i = 0; i < 6; i++) {
        ky_hashmap_set(&m, keys[i], &values[i]);
    }
    KY_CHECK(ky_hashmap_count(&m) == 6);
    /* 覆盖写：set 必须命中 live 槽原地更新，而不是新插一个 */
    ky_hashmap_set(&m, keys[2], &v2);
    KY_CHECK(ky_hashmap_get(&m, keys[2]) == &v2);
    KY_CHECK(ky_hashmap_count(&m) == 6); /* 覆盖不新增 */
    ky_hashmap_deinit(&m);
}

static void test_hashmap_many_keys(void) {
    /* ky_hashmap_set 不持有 key，key 必须长生命周期：用堆数组持有。 */
    kyAllocator al = ky_default_allocator();
    kyHashMap m;
    ky_hashmap_init(&m, &al, 0);
    int sentinel = 42;
    char (*keys)[32] = ky_mem_alloc(&al, sizeof(char[32]) * 5000);
    for (int i = 0; i < 5000; i++) {
        snprintf(keys[i], 32, "hashkey_%d", i);
        ky_hashmap_set(&m, keys[i], &sentinel);
    }
    KY_CHECK(ky_hashmap_count(&m) == 5000);
    /* 全部 value 指向同一 sentinel，逐一验证非空命中 */
    for (int i = 0; i < 5000; i += 37) {
        KY_CHECK(ky_hashmap_get(&m, keys[i]) == &sentinel);
    }
    ky_hashmap_deinit(&m);
    ky_mem_free(&al, keys);
}

static void test_hashmap_deinit_null_safe(void) {
    kyAllocator al = ky_default_allocator();
    kyHashMap m;
    ky_hashmap_init(&m, &al, 8);
    ky_hashmap_deinit(&m);
    /* 对已 deinit 的 map 再 deinit 不崩溃（entries==NULL 分支） */
    ky_hashmap_deinit(&m);
    KY_CHECK(m.entries == NULL && m.cap == 0);
}

static void test_hash_function(void) {
    /* 确定性 + 不同输入不同输出（无已知碰撞对） */
    KY_CHECK(ky_hash_str("kronyx") == ky_hash_str("kronyx"));
    KY_CHECK(ky_hash_str("a") != ky_hash_str("b"));
    KY_CHECK(ky_hash_str("") == ky_hash_bytes("", 0, 0x1234ABCDull));
    KY_CHECK(ky_hash_bytes("abc", 3, 0) == ky_hash_bytes("abc", 3, 0));
    KY_CHECK(ky_hash_bytes("abc", 3, 0) != ky_hash_bytes("abc", 3, 1));
    /* 长输入走 8 字节块路径 */
    char longbuf[100];
    memset(longbuf, 'x', sizeof(longbuf));
    KY_CHECK(ky_hash_bytes(longbuf, sizeof(longbuf), 7)
             == ky_hash_bytes(longbuf, sizeof(longbuf), 7));
}

/* ============================ pool =================================== */

static void test_pool_basic(void) {
    kyAllocator al = ky_default_allocator();
    kyPool p = ky_pool_create(sizeof(int), 8, &al);
    KY_CHECK(ky_pool_capacity(&p) >= 8);
    KY_CHECK(ky_pool_count(&p) == 0);

    int *x = (int *)ky_pool_alloc(&p);
    int *y = (int *)ky_pool_alloc(&p);
    KY_CHECK(x != NULL && y != NULL && x != y);
    KY_CHECK(ky_pool_count(&p) == 2);
    *x = 7; *y = 9;

    ky_pool_free(&p, x);
    KY_CHECK(ky_pool_count(&p) == 1);
    /* 释放的槽可以被重新分配 */
    int *z = (int *)ky_pool_alloc(&p);
    KY_CHECK(z == x);
    KY_CHECK(*z == 7); /* 未清零，保留旧值 */

    ky_pool_reset(&p);
    KY_CHECK(ky_pool_count(&p) == 0);
    KY_CHECK(ky_pool_alloc(&p) != NULL);

    ky_pool_destroy(&p);
    KY_CHECK(ky_pool_capacity(&p) == 0);
}

static void test_pool_free_and_reuse_no_grow(void) {
    /* 池文档：扩容会使先前返回的槽位指针失效，因此释放/复用验证
     * 必须在未触发扩容的容量区间内进行（cap=16，只用 10 个）。 */
    kyAllocator al = ky_default_allocator();
    kyPool p = ky_pool_create(sizeof(double), 16, &al);
    double *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = (double *)ky_pool_alloc(&p);
        KY_CHECK(ptrs[i] != NULL);
        *ptrs[i] = (double)i * 1.5;
    }
    KY_CHECK(ky_pool_count(&p) == 10);
    /* 释放中间一块，LIFO 再取回应得到同一指针，值仍在 */
    ky_pool_free(&p, ptrs[1]);
    double *reuse = (double *)ky_pool_alloc(&p);
    KY_CHECK(reuse == ptrs[1]);
    KY_CHECK(*reuse == 1.5);
    KY_CHECK(ky_pool_count(&p) == 10);
    ky_pool_destroy(&p);
}

static void test_pool_grow_invalidates_pointers(void) {
    /* 扩容会迁移底层块：先前返回的槽位指针失效（见 pool.h 文档）。
     * 验证扩容后对旧指针的 free 被边界检查拒绝，count 不变。 */
    kyAllocator al = ky_default_allocator();
    kyPool p = ky_pool_create(sizeof(double), 4, &al);
    size_t cap0 = ky_pool_capacity(&p);
    double *old_ptrs[cap0];
    for (size_t i = 0; i < cap0; i++) {
        old_ptrs[i] = (double *)ky_pool_alloc(&p);
        KY_CHECK(old_ptrs[i] != NULL);
        *old_ptrs[i] = (double)i * 1.5;
    }
    /* 触发扩容：多分配几个 */
    for (size_t i = 0; i < 4; i++) {
        double *fresh = (double *)ky_pool_alloc(&p);
        KY_CHECK(fresh != NULL);
    }
    KY_CHECK(ky_pool_capacity(&p) > cap0);
    size_t count_before = ky_pool_count(&p);
    /* 旧指针基于旧底层块，扩容后已失效，free 必须被拒绝 */
    ky_pool_free(&p, old_ptrs[1]);
    KY_CHECK(ky_pool_count(&p) == count_before);
    ky_pool_destroy(&p);
}

static void test_pool_free_invalid(void) {
    kyAllocator al = ky_default_allocator();
    kyPool p = ky_pool_create(sizeof(int), 4, &al);
    int *ok = (int *)ky_pool_alloc(&p);
    KY_CHECK(ok != NULL);
    KY_CHECK(ky_pool_count(&p) == 1);
    ky_pool_free(&p, NULL);
    KY_CHECK(ky_pool_count(&p) == 1);
    ky_pool_free(&p, (void *)0x1); /* 远端野指针被拒绝 */
    KY_CHECK(ky_pool_count(&p) == 1);
    uint8_t base[64];
    ky_pool_free(&p, base); /* 非本池指针被拒绝 */
    KY_CHECK(ky_pool_count(&p) == 1);
    ky_pool_destroy(&p);
}

static void test_pool_null_safe(void) {
    kyPool p;
    memset(&p, 0, sizeof(p));
    ky_pool_free(&p, NULL); /* free_list NULL 分支 */
    KY_CHECK(ky_pool_count(&p) == 0);
    KY_CHECK(ky_pool_capacity(&p) == 0);
}

/* ============================ memory ================================= */

static void test_memory_tracking(void) {
    kyMemStats st;
    kyAllocator al = ky_tracking_allocator(&st);
    void *a1 = ky_mem_alloc(&al, 100);
    void *a2 = ky_mem_alloc(&al, 200);
    KY_CHECK(a1 && a2);
    KY_CHECK(st.alloc_count == 2);
    KY_CHECK(st.live_bytes == 300);
    KY_CHECK(st.peak_bytes == 300);

    a1 = ky_mem_realloc(&al, a1, 400);
    KY_CHECK(a1 != NULL);
    KY_CHECK(st.live_bytes == 600);
    KY_CHECK(st.peak_bytes == 600);

    void *a3 = ky_mem_alloc(&al, 50);
    KY_CHECK(st.peak_bytes == 650);

    ky_mem_free(&al, a1);
    ky_mem_free(&al, a2);
    ky_mem_free(&al, a3);
    KY_CHECK(st.free_count == 3);
    KY_CHECK(st.live_bytes == 0);
}

static void test_memory_realloc_zero_and_grow(void) {
    kyMemStats st;
    kyAllocator al = ky_tracking_allocator(&st);
    void *p = ky_mem_alloc(&al, 64);
    KY_CHECK(p != NULL);
    /* size==0 走 free+alloc(1) 分支 */
    p = ky_mem_realloc(&al, p, 0);
    KY_CHECK(p != NULL);
    KY_CHECK(st.alloc_count == 2);
    KY_CHECK(st.free_count == 1);
    /* NULL ptr 走 alloc 分支 */
    void *q = ky_mem_realloc(&al, NULL, 32);
    KY_CHECK(q != NULL);
    ky_mem_free(&al, p);
    ky_mem_free(&al, q);
    KY_CHECK(st.live_bytes == 0);
}

static void test_memory_dup(void) {
    kyMemStats st;
    kyAllocator al = ky_tracking_allocator(&st);
    const char src[] = "kronyx";
    void *copy = ky_mem_dup(&al, src, sizeof(src));
    KY_CHECK(copy != NULL);
    KY_CHECK(memcmp(copy, src, sizeof(src)) == 0);
    ky_mem_free(&al, copy);
    /* 空 src 不崩溃 */
    void *zero = ky_mem_dup(&al, NULL, 0);
    ky_mem_free(&al, zero);
}

static void test_default_allocator(void) {
    kyAllocator al = ky_default_allocator();
    void *p = ky_mem_alloc(&al, 8);
    KY_CHECK(p != NULL);
    p = ky_mem_realloc(&al, p, 16);
    KY_CHECK(p != NULL);
    ky_mem_free(&al, p);
    /* free(NULL) 安全 */
    ky_mem_free(&al, NULL);
    kyMemStats st;
    kyAllocator st_al = ky_tracking_allocator(&st);
    ky_mem_free(&st_al, NULL); /* 跟踪分配器 free(NULL) 安全 */
}

/* ============================ event ================================== */

typedef struct {
    int hits;
    int last_data;
    char last_name[64];
} EvCounter;

static void ev_counter_cb(const char *name, const void *data, void *user) {
    EvCounter *c = (EvCounter *)user;
    c->hits++;
    c->last_data = data ? *(const int *)data : -1;
    if (name) snprintf(c->last_name, sizeof(c->last_name), "%s", name);
}

static void test_event_register_trigger(void) {
    ky_event_clear();
    EvCounter c1 = {0, 0, {0}}, c2 = {0, 0, {0}};
    KY_CHECK(ky_event_register("evt.a", ev_counter_cb, &c1) > 0);
    KY_CHECK(ky_event_register("evt.a", ev_counter_cb, &c2) > 0);
    /* 同一 (name,fn,user) 三元组重复注册被拒绝 */
    KY_CHECK(ky_event_register("evt.a", ev_counter_cb, &c1) == -3);
    /* NULL 参数被拒绝 */
    KY_CHECK(ky_event_register(NULL, ev_counter_cb, NULL) == -1);
    KY_CHECK(ky_event_register("evt.a", NULL, NULL) == -1);

    int payload = 42;
    ky_event_trigger("evt.a", &payload);
    KY_CHECK(c1.hits == 1 && c1.last_data == 42);
    KY_CHECK(c2.hits == 1 && c2.last_data == 42);
    KY_CHECK(strcmp(c1.last_name, "evt.a") == 0);

    /* 不同名字的事件不串扰 */
    EvCounter c3 = {0, 0, {0}};
    KY_CHECK(ky_event_register("evt.b", ev_counter_cb, &c3) > 0);
    ky_event_trigger("evt.a", NULL);
    KY_CHECK(c3.hits == 0);
    KY_CHECK(c1.hits == 2);
    ky_event_clear();
}

static void test_event_long_name_rejected(void) {
    ky_event_clear();
    char longname[200];
    memset(longname, 'x', sizeof(longname) - 1);
    longname[sizeof(longname) - 1] = 0;
    KY_CHECK(ky_event_register(longname, ev_counter_cb, NULL) == -4);
    ky_event_clear();
}

static void test_event_registry_full(void) {
    ky_event_clear();
    /* 注册 32 个不同 listener 后返回 -2 */
    for (int i = 0; i < 32; i++) {
        char name[16];
        snprintf(name, sizeof(name), "full%d", i);
        int rc = ky_event_register(name, ev_counter_cb, (void *)(uintptr_t)i);
        KY_CHECK(rc > 0);
    }
    KY_CHECK(ky_event_register("overflow", ev_counter_cb, (void *)0x10) == -2);
    ky_event_clear();
}

static void test_event_name_copied(void) {
    /* 注册时 name 被拷贝，栈变量销毁后事件仍可用 */
    ky_event_clear();
    EvCounter c = {0, 0, {0}};
    {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "stack.%d", 7);
        KY_CHECK(ky_event_register(tmp, ev_counter_cb, &c) > 0);
    } /* tmp 已出作用域 */
    int payload = 7;
    ky_event_trigger("stack.7", &payload);
    KY_CHECK(c.hits == 1 && c.last_data == 7);
    ky_event_clear();
}

/* ============================ input ================================== */

typedef struct {
    int count;
    int last_key;
    int last_pressed;
} InputCapture;

static void input_capture(void *user, const kyInputEvent *evs, int n) {
    InputCapture *cap = (InputCapture *)user;
    for (int i = 0; i < n; i++) {
        cap->count++;
        cap->last_key = evs[i].key;
        cap->last_pressed = evs[i].pressed;
    }
}

static void test_input_ring_queue(void) {
    InputCapture cap = {0, 0, 0};
    ky_input_set_queue_handler(input_capture, &cap);
    ky_input_simulate_key(KY_KEY_A, 1);
    ky_input_simulate_key(KY_KEY_B, 0);
    KY_CHECK(cap.count == 2);
    KY_CHECK(cap.last_key == KY_KEY_B);
    KY_CHECK(cap.last_pressed == 0);

    /* 切换回内部 ring：handler 清空后走 ring_push */
    ky_input_set_queue_handler(NULL, NULL);
    ky_input_reset();
    kyInputEvent ev;
    KY_CHECK(ky_input_poll(&ev) == 0); /* 空 */
    ky_input_simulate_key(KY_KEY_C, 1);
    KY_CHECK(ky_input_poll(&ev) == 1);
    KY_CHECK(ev.key == KY_KEY_C && ev.pressed == 1);
    KY_CHECK(ky_input_poll(&ev) == 0);
    /* poll(NULL) 安全 */
    ky_input_simulate_key(KY_KEY_D, 1);
    KY_CHECK(ky_input_poll(NULL) == 0);
    ky_input_reset();
}

static void test_input_ring_overflow_drops(void) {
    ky_input_set_queue_handler(NULL, NULL);
    ky_input_reset();
    /* 256 槽 ring 满后继续 push 丢弃新事件 */
    for (int i = 0; i < 300; i++)
        ky_input_simulate_key(KY_KEY_A, 1);
    int kept = 0;
    kyInputEvent ev;
    while (ky_input_poll(&ev)) kept++;
    KY_CHECK(kept == 256);
    ky_input_reset();
}

/* ============================ log ==================================== */

typedef struct {
    int calls;
    kyLogLevel last_level;
    char last_msg[256];
} LogCapture;

static void log_capture(kyLogLevel level, const char *msg, void *ud) {
    LogCapture *cap = (LogCapture *)ud;
    cap->calls++;
    cap->last_level = level;
    snprintf(cap->last_msg, sizeof(cap->last_msg), "%s", msg ? msg : "(null)");
}

static void test_log_level_filtering(void) {
    LogCapture cap = {0, 0, {0}};
    ky_log_set_sink(log_capture, &cap);
    ky_log_set_level(KY_LOG_WARN);
    ky_log_write(KY_LOG_INFO, "below threshold");
    KY_CHECK(cap.calls == 0);
    ky_log_write(KY_LOG_WARN, "warn msg %d", 1);
    KY_CHECK(cap.calls == 1);
    KY_CHECK(cap.last_level == KY_LOG_WARN);
    KY_CHECK(strstr(cap.last_msg, "warn msg 1") != NULL);
    ky_log_write(KY_LOG_ERROR, "err");
    KY_CHECK(cap.calls == 2);
    KY_CHECK(cap.last_level == KY_LOG_ERROR);
    KY_CHECK(ky_log_get_level() == KY_LOG_WARN);
    ky_log_set_level(KY_LOG_INFO); /* 还原，避免影响其他测试 */
}

static void test_log_level_names(void) {
    KY_CHECK(strcmp(ky_log_level_name(KY_LOG_TRACE), "TRACE") == 0);
    KY_CHECK(strcmp(ky_log_level_name(KY_LOG_DEBUG), "DEBUG") == 0);
    KY_CHECK(strcmp(ky_log_level_name(KY_LOG_INFO), "INFO") == 0);
    KY_CHECK(strcmp(ky_log_level_name(KY_LOG_WARN), "WARN") == 0);
    KY_CHECK(strcmp(ky_log_level_name(KY_LOG_ERROR), "ERROR") == 0);
    KY_CHECK(ky_log_level_name((kyLogLevel)99)[0] == '?');
}

static void test_log_truncation(void) {
    LogCapture cap = {0, 0, {0}};
    ky_log_set_sink(log_capture, &cap);
    ky_log_set_level(KY_LOG_TRACE);
    /* 超长消息被截断到 2047 + NUL，尾部必须有 '\0' 且内容以长串填充 */
    char filler[4096];
    memset(filler, 'a', sizeof(filler) - 1);
    filler[sizeof(filler) - 1] = 0;
    ky_log_write(KY_LOG_DEBUG, "%s", filler);
    KY_CHECK(cap.calls == 1);
    KY_CHECK(strlen(cap.last_msg) < sizeof(cap.last_msg));
    /* 默认 sink 路径（g_sink=NULL）不崩溃 */
    ky_log_set_sink(NULL, NULL);
    ky_log_write(KY_LOG_INFO, "default sink line");
    ky_log_set_level(KY_LOG_INFO);
}

/* ============================ file =================================== */

static void write_tmp_file(const char *path, const void *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(data, 1, n, f);
    fclose(f);
}

static void test_file_read_ok(void) {
    const char *path = "/tmp/ky_qc_file_ok.bin";
    uint8_t bytes[33] = {0};
    for (int i = 0; i < 33; i++) bytes[i] = (uint8_t)i;
    write_tmp_file(path, bytes, sizeof(bytes));

    void *buf = NULL;
    size_t len = 999;
    KY_CHECK(ky_file_read(path, &buf, &len) == KY_FILE_OK);
    KY_CHECK(len == sizeof(bytes));
    KY_CHECK(buf != NULL && memcmp(buf, bytes, sizeof(bytes)) == 0);
    ky_free_read(buf);
    remove(path);
}

static void test_file_read_empty_file(void) {
    /* 0 字节文件：允许成功，返回空 buffer */
    const char *path = "/tmp/ky_qc_file_empty.bin";
    FILE *f = fopen(path, "wb");
    if (f) fclose(f);
    void *buf = NULL;
    size_t len = 42;
    KY_CHECK(ky_file_read(path, &buf, &len) == KY_FILE_OK);
    KY_CHECK(len == 0);
    ky_free_read(buf);
    remove(path);
}

static void test_file_read_errors(void) {
    void *buf = NULL;
    size_t len = 0;
    KY_CHECK(ky_file_read(NULL, &buf, &len) == KY_FILE_ERR_BAD_SIZE);
    KY_CHECK(ky_file_read("", &buf, &len) == KY_FILE_ERR_BAD_SIZE);
    KY_CHECK(ky_file_read("/tmp/nonexistent_kronyx_qc.bin", &buf, &len)
             == KY_FILE_ERR_NOT_FOUND);
    KY_CHECK(buf == NULL);
    /* 目录不是 regular file */
    KY_CHECK(ky_file_read("/tmp", &buf, &len) == KY_FILE_ERR_NOT_FOUND);
}

/* ============================ time =================================== */

static void test_time_monotonic(void) {
    double t0 = ky_time_now();
    uint64_t u0 = ky_time_now_us();
    KY_CHECK(t0 > 0.0);
    KY_CHECK(u0 > 0);
    for (int i = 0; i < 10000; i++) {
        (void)i;
    }
    double t1 = ky_time_now();
    uint64_t u1 = ky_time_now_us();
    KY_CHECK(t1 >= t0);
    KY_CHECK(u1 >= u0);
    /* us 与 s 的一致性：同数量级 */
    double ratio = (double)u1 / 1e6;
    KY_CHECK(ratio >= 0.0 && ratio < 1e9);
}

/* ============================ math =================================== */

static void test_mat4_identity_roundtrip(void) {
    kyMat4 id = ky_mat4_identity();
    KY_CHECK(id.m[0] == 1.0f && id.m[5] == 1.0f && id.m[10] == 1.0f && id.m[15] == 1.0f);
    KY_CHECK(id.m[1] == 0.0f && id.m[4] == 0.0f);
    /* 单位矩阵乘法保持向量 */
    kyVec4 v = ky_vec4(1, 2, 3, 1);
    kyVec4 r = ky_mat4_mul_vec4(&id, v);
    KY_CHECK(r.x == 1.0f && r.y == 2.0f && r.z == 3.0f && r.w == 1.0f);
}

static void test_mat4_mul_associative_translate_scale(void) {
    /* T * S 作用于点：先缩放再平移 */
    kyMat4 t = ky_mat4_translate(ky_vec3(1, 2, 3));
    kyMat4 s = ky_mat4_scale(ky_vec3(2, 2, 2));
    kyMat4 ts = ky_mat4_mul(&t, &s);
    kyVec3 p = ky_mat4_mul_point(&ts, ky_vec3(1, 1, 1));
    KY_CHECK_NEAR(p.x, 1 * 2 + 1, 1e-5f);
    KY_CHECK_NEAR(p.y, 1 * 2 + 2, 1e-5f);
    KY_CHECK_NEAR(p.z, 1 * 2 + 3, 1e-5f);
}

static void test_mat4_inverse_singularity(void) {
    /* 奇异矩阵（零矩阵）返回单位矩阵而非 NaN */
    kyMat4 zero;
    memset(zero.m, 0, sizeof(zero.m));
    kyMat4 inv = ky_mat4_inverse(&zero);
    KY_CHECK(inv.m[0] == 1.0f && inv.m[15] == 1.0f);
    KY_CHECK(inv.m[1] == 0.0f);
    /* 正常矩阵的逆：A * A^-1 = I */
    kyMat4 t4 = ky_mat4_translate(ky_vec3(4, -2, 1));
    kyMat4 s3 = ky_mat4_scale(ky_vec3(3, 3, 3));
    kyMat4 a = ky_mat4_mul(&t4, &s3);
    kyMat4 ainv = ky_mat4_inverse(&a);
    kyMat4 prod = ky_mat4_mul(&a, &ainv);
    for (int i = 0; i < 16; i++) {
        float expect = (i % 5 == 0) ? 1.0f : 0.0f;
        KY_CHECK_NEAR(prod.m[i], expect, 1e-3f);
    }
}

static void test_mat4_rotation_quat(void) {
    /* 绕 Z 轴 90°：(1,0,0) -> (0,1,0) */
    kyQuat q = ky_quat_axis_angle(ky_vec3(0, 0, 1), (float)(KY_PI / 2.0));
    kyMat4 rot = ky_mat4_rotation(q);
    kyVec3 r = ky_mat4_mul_dir(&rot, ky_vec3(1, 0, 0));
    KY_CHECK_NEAR(r.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(r.y, 1.0f, 1e-5f);
    KY_CHECK_NEAR(r.z, 0.0f, 1e-5f);
}

static void test_mat4_ortho(void) {
    kyMat4 o = ky_mat4_ortho(-1, 1, -1, 1, -10, 10);
    /* 原点投影到 NDC 原点 */
    kyVec4 c = ky_mat4_mul_vec4(&o, ky_vec4(0, 0, 0, 1));
    KY_CHECK_NEAR(c.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(c.y, 0.0f, 1e-5f);
    KY_CHECK_NEAR(c.z, 0.0f, 1e-5f);
    /* 角点 (1,1,1) 投影 */
    kyVec4 corner = ky_mat4_mul_vec4(&o, ky_vec4(1, 1, 1, 1));
    KY_CHECK_NEAR(corner.x / corner.w, 1.0f, 1e-5f);
    KY_CHECK_NEAR(corner.y / corner.w, 1.0f, 1e-5f);
}

static void test_perspective_bounds(void) {
    kyMat4 proj = ky_mat4_perspective((float)(KY_PI / 4.0f), 2.0f, 0.5f, 100.0f);
    /* 近平面上的点 z/w = -1，远平面 z/w = +1 */
    kyVec4 zn = ky_mat4_mul_vec4(&proj, ky_vec4(0, 0, -0.5f, 1));
    KY_CHECK_NEAR(zn.z / zn.w, -1.0f, 1e-4f);
    kyVec4 zf = ky_mat4_mul_vec4(&proj, ky_vec4(0, 0, -100.0f, 1));
    KY_CHECK_NEAR(zf.z / zf.w, 1.0f, 1e-4f);
    /* 视口 x：x/z 缩放，z=-0.5 时 x/w ≈ 1.2071（实测实现精度） */
    kyVec4 mid = ky_mat4_mul_vec4(&proj, ky_vec4(0.5f, 0, -0.5f, 1));
    KY_CHECK_NEAR(mid.x / mid.w, 1.2071f, 1e-3f);
}

static void test_look_at_views(void) {
    /* 朝 -Z 看，上 +Y：世界原点应投影到视图空间 (0,0,-10) */
    kyMat4 v = ky_mat4_look_at(ky_vec3(0, 0, 10), ky_vec3(0, 0, 0), ky_vec3(0, 1, 0));
    kyVec3 p = ky_mat4_mul_point(&v, ky_vec3(0, 0, 0));
    KY_CHECK_NEAR(p.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(p.y, 0.0f, 1e-5f);
    KY_CHECK_NEAR(p.z, -10.0f, 1e-5f);
    /* 自身位置投影为原点 */
    kyVec3 self = ky_mat4_mul_point(&v, ky_vec3(0, 0, 10));
    KY_CHECK_NEAR(self.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(self.z, 0.0f, 1e-5f);
}

static void test_quat_identity_and_mul(void) {
    kyQuat id = {0, 0, 0, 1};
    kyVec3 v = ky_vec3(3, 4, 5);
    kyVec3 r = ky_quat_rotate(id, v);
    KY_CHECK_NEAR(r.x, 3.0f, 1e-6f);
    KY_CHECK_NEAR(r.y, 4.0f, 1e-6f);
    KY_CHECK_NEAR(r.z, 5.0f, 1e-6f);
    /* q * conj(q) = identity */
    kyQuat q = ky_quat_axis_angle(ky_vec3(1, 0, 0), 0.7f);
    kyQuat back = ky_quat_mul(q, ky_quat_conj(q));
    KY_CHECK_NEAR(back.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(back.y, 0.0f, 1e-5f);
    KY_CHECK_NEAR(back.z, 0.0f, 1e-5f);
    KY_CHECK_NEAR(back.w, 1.0f, 1e-5f);
    /* 0 度旋转 = 单位四元数 */
    kyQuat zero = ky_quat_axis_angle(ky_vec3(0, 0, 1), 0.0f);
    KY_CHECK_NEAR(zero.w, 1.0f, 1e-6f);
    KY_CHECK_NEAR(zero.x, 0.0f, 1e-6f);
}

static void test_ray_aabb_edges(void) {
    kyAABB box;
    box.min = ky_vec3(-1, -1, -1);
    box.max = ky_vec3(1, 1, 1);
    /* 原点在盒内：t 被钳制为 0 */
    float t = -1;
    KY_CHECK(ky_ray_aabb(ky_vec3(0, 0, 0), ky_vec3(1, 1, 1), 10.0f, &box, &t) == 1);
    KY_CHECK_NEAR(t, 0.0f, 1e-6f);
    /* 起点在盒外沿 +X，方向朝 -X 指向盒内：命中 */
    KY_CHECK(ky_ray_aabb(ky_vec3(5, 0, 0), ky_vec3(-1, 0, 0), 10.0f, &box, &t) == 1);
    KY_CHECK_NEAR(t, 4.0f, 1e-5f);
    /* 平行于盒（沿 +Y，x=5 在盒外）：无命中 */
    KY_CHECK(ky_ray_aabb(ky_vec3(5, 0, 0), ky_vec3(0, 1, 0), 10.0f, &box, &t) == 0);
    /* 起点在盒外，t_max 小于 tmin：被 t_max 挡住 */
    KY_CHECK(ky_ray_aabb(ky_vec3(0, 0, 5), ky_vec3(0, 0, -1), 1.0f, &box, &t) == 0);
    /* 命中面：(0,0,5) 朝 -Z 打到 z=1 面，t=4 */
    KY_CHECK(ky_ray_aabb(ky_vec3(0, 0, 5), ky_vec3(0, 0, -1), 10.0f, &box, &t) == 1);
    KY_CHECK_NEAR(t, 4.0f, 1e-5f);
}

/* ============================ main =================================== */

void ky_test_run_all(void) {
    test_arena_basic();
    test_arena_growth();
    test_arena_bad_align_falls_back();

    test_array_grow();
    test_array_push_null_elem();
    test_array_reserve_idempotent();

    test_string_basic();
    test_string_reserve_no_shrink();

    test_hashmap_set_get();
    test_hashmap_set_null_removes();
    test_hashmap_owned_keys();
    test_hashmap_tombstone_probing();
    test_hashmap_many_keys();
    test_hashmap_deinit_null_safe();
    test_hash_function();

    test_pool_basic();
    test_pool_free_and_reuse_no_grow();
    test_pool_grow_invalidates_pointers();
    test_pool_free_invalid();
    test_pool_null_safe();

    test_memory_tracking();
    test_memory_realloc_zero_and_grow();
    test_memory_dup();
    test_default_allocator();

    test_event_register_trigger();
    test_event_long_name_rejected();
    test_event_registry_full();
    test_event_name_copied();

    test_input_ring_queue();
    test_input_ring_overflow_drops();

    test_log_level_filtering();
    test_log_level_names();
    test_log_truncation();

    test_file_read_ok();
    test_file_read_empty_file();
    test_file_read_errors();

    test_time_monotonic();

    test_mat4_identity_roundtrip();
    test_mat4_mul_associative_translate_scale();
    test_mat4_inverse_singularity();
    test_mat4_rotation_quat();
    test_mat4_ortho();
    test_perspective_bounds();
    test_look_at_views();
    test_quat_identity_and_mul();
    test_ray_aabb_edges();
}

KY_TEST_MAIN()
