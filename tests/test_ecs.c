#include "kronyx/kronyx.h"
#include "kronyx/ecs.h"
#include "kronyx/scene.h"
#include "kronyx/resource.h"
#include "kytest.h"

int ky_test_failures = 0;
int ky_test_assertions = 0;

typedef struct Transform {
    kyVec3 pos;
    kyVec3 scale;
} Transform;

typedef struct Velocity {
    kyVec3 linear;
} Velocity;

typedef struct Tag {
    int value;
} Tag;

typedef struct Counter {
    int ctor_count;
    int dtor_count;
} Counter;

static uint32_t tid_transform;
static uint32_t tid_velocity;
static uint32_t tid_tag;
static uint32_t tid_counter;

static void counter_ctor(void *comp) {
    Counter *c = (Counter *)comp;
    c->ctor_count++;
}

static void counter_dtor(void *comp) {
    Counter *c = (Counter *)comp;
    c->dtor_count++;
}

static void register_types(kyWorld *w) {
    kyComponentType ct;
    ct.ctor = NULL;
    ct.dtor = NULL;

    ct.name = "transform";
    ct.size = sizeof(Transform);
    tid_transform = ky_world_register_component(w, &ct);

    ct.name = "velocity";
    ct.size = sizeof(Velocity);
    tid_velocity = ky_world_register_component(w, &ct);

    ct.name = "tag";
    ct.size = sizeof(Tag);
    tid_tag = ky_world_register_component(w, &ct);

    ct.name = "counter";
    ct.size = sizeof(Counter);
    ct.ctor = counter_ctor;
    ct.dtor = counter_dtor;
    tid_counter = ky_world_register_component(w, &ct);
}

static void test_spawn_despawn(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    kyEntity a = ky_world_spawn(w);
    kyEntity b = ky_world_spawn(w);
    KY_CHECK(ky_entity_valid(w, a));
    KY_CHECK(ky_entity_valid(w, b));
    KY_CHECK(a.id != b.id);

    ky_world_despawn(w, a);
    KY_CHECK(!ky_entity_valid(w, a));

    kyEntity c = ky_world_spawn(w);
    KY_CHECK(c.id == a.id);
    KY_CHECK(c.version != a.version);
    KY_CHECK(ky_entity_valid(w, c));

    ky_world_destroy(w);
}

static void test_add_get_remove(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    kyEntity e = ky_world_spawn(w);
    Transform *t = (Transform *)ky_world_add_component(w, e, tid_transform);
    KY_CHECK(t != NULL);
    KY_CHECK(ky_world_has_component(w, e, tid_transform));
    KY_CHECK(!ky_world_has_component(w, e, tid_velocity));
    KY_CHECK(ky_world_get_component(w, e, tid_transform) == t);

    t->pos = ky_vec3(1, 2, 3);
    Velocity *v = (Velocity *)ky_world_add_component(w, e, tid_velocity);
    v->linear = ky_vec3(4, 5, 6);

    Transform *t2 = (Transform *)ky_world_get_component(w, e, tid_transform);
    KY_CHECK(t2->pos.x == 1.0f && t2->pos.y == 2.0f && t2->pos.z == 3.0f);

    ky_world_remove_component(w, e, tid_velocity);
    KY_CHECK(!ky_world_has_component(w, e, tid_velocity));
    KY_CHECK(ky_world_has_component(w, e, tid_transform));

    ky_world_remove_component(w, e, tid_transform);
    KY_CHECK(!ky_world_has_component(w, e, tid_transform));

    ky_world_destroy(w);
}

static void test_component_persistence(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    kyEntity e = ky_world_spawn(w);
    Tag *tag = (Tag *)ky_world_add_component(w, e, tid_tag);
    tag->value = 42;
    Velocity *v = (Velocity *)ky_world_add_component(w, e, tid_velocity);
    v->linear = ky_vec3(1, 1, 1);

    Tag *tag2 = (Tag *)ky_world_get_component(w, e, tid_tag);
    KY_CHECK(tag2->value == 42);

    ky_world_remove_component(w, e, tid_tag);
    KY_CHECK(!ky_world_has_component(w, e, tid_tag));
    KY_CHECK(ky_world_has_component(w, e, tid_velocity));

    ky_world_destroy(w);
}

static void test_counter_ctor_dtor(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    kyEntity e = ky_world_spawn(w);
    Counter *c = (Counter *)ky_world_add_component(w, e, tid_counter);
    KY_CHECK(c->ctor_count == 1);

    ky_world_remove_component(w, e, tid_counter);
    KY_CHECK(c->dtor_count == 1);

    ky_world_destroy(w);
}

static void test_many_entities(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    enum { N = 10000 };
    kyEntity ents[N];
    for (int i = 0; i < N; i++) {
        ents[i] = ky_world_spawn(w);
        Transform *t = (Transform *)ky_world_add_component(w, ents[i], tid_transform);
        t->pos = ky_vec3((float)i, 0, 0);
        if (i % 2 == 0) {
            Velocity *v = (Velocity *)ky_world_add_component(w, ents[i], tid_velocity);
            v->linear = ky_vec3(1, 0, 0);
        }
    }
    KY_CHECK(ky_world_has_component(w, ents[0], tid_transform));
    KY_CHECK(ky_world_has_component(w, ents[0], tid_velocity));
    KY_CHECK(!ky_world_has_component(w, ents[1], tid_velocity));

    Transform *t = (Transform *)ky_world_get_component(w, ents[9999], tid_transform);
    KY_CHECK(t->pos.x == 9999.0f);

    ky_world_destroy(w);
}

static int order_log[8];
static int log_index = 0;

static void sys_a(kyWorld *w, float dt, void *user) {
    KY_UNUSED(w);
    KY_UNUSED(dt);
    KY_UNUSED(user);
    order_log[log_index++] = 0;
}
static void sys_b(kyWorld *w, float dt, void *user) {
    KY_UNUSED(w);
    KY_UNUSED(dt);
    KY_UNUSED(user);
    order_log[log_index++] = 1;
}
static void sys_c(kyWorld *w, float dt, void *user) {
    KY_UNUSED(w);
    KY_UNUSED(dt);
    KY_UNUSED(user);
    order_log[log_index++] = 2;
}

static void test_system_scheduling(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    log_index = 0;

    kySystem s;
    s.user = NULL;
    s.order = 30;
    s.update = sys_c;
    ky_world_register_system(w, &s);
    s.order = 10;
    s.update = sys_a;
    ky_world_register_system(w, &s);
    s.order = 20;
    s.update = sys_b;
    ky_world_register_system(w, &s);
    ky_world_sort_systems(w);

    ky_world_step(w, 1.0f / 60.0f);
    KY_CHECK(log_index == 3);
    KY_CHECK(order_log[0] == 0 && order_log[1] == 1 && order_log[2] == 2);

    ky_world_destroy(w);
}

static void test_view_iteration(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    register_types(w);

    enum { N = 100 };
    int count_transform = 0;
    int count_both = 0;
    for (int i = 0; i < N; i++) {
        kyEntity e = ky_world_spawn(w);
        ky_world_add_component(w, e, tid_transform);
        count_transform++;
        if (i % 3 == 0) {
            ky_world_add_component(w, e, tid_velocity);
            count_both++;
        }
    }

    uint32_t only_t[] = {tid_transform};
    kyViewIter it;
    int seen = 0;
    if (ky_view_begin(w, only_t, 1, &it)) {
        seen = 1;
        while (ky_view_next(&it)) {
            seen++;
        }
    }
    KY_CHECK(seen == count_transform);

    uint32_t both[] = {tid_transform, tid_velocity};
    seen = 0;
    if (ky_view_begin(w, both, 2, &it)) {
        seen = 1;
        while (ky_view_next(&it)) {
            seen++;
        }
    }
    KY_CHECK(seen == count_both);

    ky_world_destroy(w);
}

static void test_scene_meta(void) {
    kyAllocator al = ky_default_allocator();
    kyScene *sc = ky_scene_create(&al, "test_scene");
    KY_CHECK(strcmp(sc->name, "test_scene") == 0);
    ky_scene_set_meta(sc, "author", "kronyx");
    KY_CHECK(strcmp(ky_scene_get_meta(sc, "author"), "kronyx") == 0);
    KY_CHECK(ky_scene_get_meta(sc, "missing") == NULL);
    ky_scene_destroy(sc);
}

static void test_resource_manager(void) {
    kyAllocator al = ky_default_allocator();
    kyResourceManager *m = ky_resmgr_create(&al);

    kyResource *r1 = (kyResource *)ky_mem_alloc(&al, sizeof(kyResource));
    memset(r1, 0, sizeof(*r1));
    r1->kind = KY_RES_SHADER;
    r1->path = "shaders/default.glsl";
    r1->reload = NULL;
    KY_CHECK(ky_resmgr_register(m, r1) == 1);
    KY_CHECK(ky_resmgr_register(m, r1) == 0);

    kyResource *r2 = (kyResource *)ky_mem_alloc(&al, sizeof(kyResource));
    memset(r2, 0, sizeof(*r2));
    r2->kind = KY_RES_TEXTURE;
    r2->path = "textures/player.png";
    KY_CHECK(ky_resmgr_register(m, r2) == 1);

    KY_CHECK(ky_resmgr_count(m) == 2);
    kyResource *f = ky_resmgr_find(m, "shaders/default.glsl");
    KY_CHECK(f != NULL && f->kind == KY_RES_SHADER);
    KY_CHECK(ky_resmgr_find(m, "nope.xyz") == NULL);

    kyResource *a = ky_resmgr_acquire(m, "textures/player.png");
    KY_CHECK(a != NULL && a->ref_count == 1);
    ky_resmgr_release(m, a);
    KY_CHECK(ky_resmgr_count(m) == 1);
    KY_CHECK(ky_resmgr_find(m, "textures/player.png") == NULL);

    ky_resmgr_release(m, r1);
    KY_CHECK(ky_resmgr_count(m) == 0);

    ky_resmgr_destroy(m);
}

void ky_test_run_all(void) {
    test_spawn_despawn();
    test_add_get_remove();
    test_component_persistence();
    test_counter_ctor_dtor();
    test_many_entities();
    test_system_scheduling();
    test_view_iteration();
    test_scene_meta();
    test_resource_manager();
}

KY_TEST_MAIN()
