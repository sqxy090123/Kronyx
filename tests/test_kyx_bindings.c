/*
 * kyx game bindings (G3) — end-to-end:
 *   spawn (script) -> C-side rebuild from returned value -> valid
 *   spawn_with_transform -> setpos/getpos roundtrip (entity passed via arg)
 *   setscale/getscale roundtrip
 *   input: simulate -> poll_input -> axis_x / last_key
 *   log: std.log(...) does not crash
 *   despawn -> alive 0
 *   null-world path returns error strings
 *
 * Design notes (kronyx VM semantics):
 *   - script function return values are coerced INT->FLOAT by OP_RETURN, so
 *     all cross-boundary numeric results are asserted as KYT_FLOAT
 *   - 64-bit entity handles must NOT be embedded as integer literals in the
 *     script source (OP_LOADINT truncates to 32 bits); pass them as
 *     ky_vm_call arguments instead, which copy the kyValue verbatim
 */
#include "kronyx/script.h"
#include "kronyx/bindings.h"
#include "kronyx/ecs.h"
#include "kronyx/2d.h"
#include "kronyx/input.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Allocator must outlive the world for the program's lifetime, matching the
   one-scope convention used by test_ecs.c / test_core.c: every kyArray in the
   world stores this allocator BY POINTER, so it must be alive whenever
   ky_world_destroy walks the arrays. A stack-local in a helper would dangle.  */
static kyAllocator g_alloc;
static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL: %s\n", msg); } \
} while (0)

/* ---- world + transform helpers (C side) ---- */
static kyWorld *make_world(void) {
    g_alloc = ky_default_allocator();
    kyWorld *w = ky_world_create(&g_alloc);
    assert(w);
    int rc = ky2d_register_components(w);
    (void)rc;
    return w;
}
static uint32_t t_tid(kyWorld *w) {
    for (uint32_t i = 0; ; i++) {
        const kyComponentType *ct = ky_world_component_type(w, i);
        if (!ct) return 0u;
        if (ct->name && strcmp(ct->name, "transform") == 0) return i;
    }
}
static kyEntity spawn_with_transform(kyWorld *w) {
    kyEntity e = ky_world_spawn(w);
    void *tr = ky_world_add_component(w, e, t_tid(w));
    (void)tr;
    return e;
}
static int64_t ent_pack(kyEntity e) {
    return ((int64_t)e.version << 32) | (int64_t)(uint32_t)e.id;
}
static kyEntity ent_unpack(int64_t p) {
    kyEntity e;
    e.id      = (uint32_t)(p & 0xFFFFFFFFu);
    e.version = (uint32_t)((uint64_t)p >> 32);
    return e;
}
static kyValue kv_i(int64_t x) {
    kyValue v; v.type = KYT_INT; v.as.ival = x; return v;
}

int main(void) {
    kyWorld *w = make_world();

    /* 1) script spawn -> returned value (as FLOAT, per VM coercion)
       rebuilds a valid entity on the C side */
    {
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        const char *src = "function s(){ return std.spawn(); }";
        CHECK(ky_vm_load_string(vm, src, "m") == 0, "load spawn fn");
        kyValue ret;
        CHECK(ky_vm_call(vm, "s", NULL, 0, &ret) == 0, "call std.spawn");
        CHECK(ret.type == KYT_FLOAT, "spawn yields a numeric (FLOAT) return");
        if (ret.type == KYT_FLOAT) {
            kyEntity e = ent_unpack((int64_t)ret.as.fval);
            CHECK(ky_entity_valid(w, e), "spawned entity is valid");
        }
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    /* 2) setpos/getpos roundtrip (entity handle passed as call arg) */
    {
        kyEntity e = spawn_with_transform(w);
        int64_t packed = ent_pack(e);
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        /* write y via setpos */
        const char *src = "function w(a){ std.setpos(a, 1.5, -2.5); return 0; }";
        /* setpos expects (entity, x, y); read back axis 1 == y */
        const char *srcy = "function ry(a){ return std.getpos(a, 1); }";
        const char *srcx = "function rx(a){ return std.getpos(a, 0); }";
        CHECK(ky_vm_load_string(vm, src, "w") == 0, "load setpos");
        CHECK(ky_vm_load_string(vm, srcy, "ry") == 0, "load getpos-y");
        CHECK(ky_vm_load_string(vm, srcx, "rx") == 0, "load getpos-x");

        kyValue arg = kv_i(packed);
        kyValue ret;
        CHECK(ky_vm_call(vm, "w", &arg, 1, &ret) == 0, "call setpos");
        CHECK(ky_vm_call(vm, "ry", &arg, 1, &ret) == 0, "call getpos-y");
        CHECK(ret.type == KYT_FLOAT && ret.as.fval == -2.5, "getpos y == -2.5");
        CHECK(ky_vm_call(vm, "rx", &arg, 1, &ret) == 0, "call getpos-x");
        CHECK(ret.type == KYT_FLOAT && ret.as.fval == 1.5, "getpos x == 1.5");
        /* verify the C side actually has the updated position */
        kyTransform *tr = (kyTransform*)ky_world_get_component(w, e, t_tid(w));
        CHECK(tr && tr->pos.x == 1.5f && tr->pos.y == -2.5f, "C sees updated pos");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    /* 3) setscale/getscale roundtrip */
    {
        kyEntity e = spawn_with_transform(w);
        int64_t packed = ent_pack(e);
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        const char *srcs = "function w(a){ std.setscale(a, 3.0, 4.0); return 0; }";
        const char *srcy = "function ry(a){ return std.getscale(a, 1); }";
        CHECK(ky_vm_load_string(vm, srcs, "w") == 0, "load setscale");
        CHECK(ky_vm_load_string(vm, srcy, "ry") == 0, "load getscale-y");
        kyValue arg = kv_i(packed);
        kyValue ret;
        CHECK(ky_vm_call(vm, "w", &arg, 1, &ret) == 0, "call setscale");
        CHECK(ky_vm_call(vm, "ry", &arg, 1, &ret) == 0, "call getscale-y");
        CHECK(ret.type == KYT_FLOAT && ret.as.fval == 4.0, "getscale y == 4.0");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    /* 4) input: LEFT (A) => axis_x -1 */
    {
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        ky_input_reset();
        ky_input_simulate_key(KY_KEY_A, 1);
        const char *src = "function a(){ std.poll_input(); return std.axis_x(); }";
        CHECK(ky_vm_load_string(vm, src, "m") == 0, "load axis");
        kyValue ret;
        CHECK(ky_vm_call(vm, "a", NULL, 0, &ret) == 0, "call axis");
        CHECK(ret.type == KYT_FLOAT && ret.as.fval == -1.0f, "axis_x == -1 after A");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }
    /* RIGHT (D) => axis_x +1 */
    {
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        ky_input_reset();
        ky_input_simulate_key(KY_KEY_D, 1);
        const char *src = "function a(){ std.poll_input(); return std.axis_x(); }";
        ky_vm_load_string(vm, src, "m");
        kyValue ret;
        CHECK(ky_vm_call(vm, "a", NULL, 0, &ret) == 0, "call axis (D)");
        CHECK(ret.type == KYT_FLOAT && ret.as.fval == 1.0f, "axis_x == +1 after D");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    /* 5) log does not crash */
    {
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        const char *src = "function l(){ std.log(\"kronyx\", 42, 3.14); return 1; }";
        CHECK(ky_vm_load_string(vm, src, "m") == 0, "load log");
        kyValue ret;
        CHECK(ky_vm_call(vm, "l", NULL, 0, &ret) == 0, "call log ok");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    /* 6) despawn -> alive 0 */
    {
        kyEntity e = spawn_with_transform(w);
        int64_t packed = ent_pack(e);
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, w);
        const char *srcd = "function d(a){ std.despawn(a); return 0; }";
        const char *srck = "function k(a){ return std.alive(a); }";
        ky_vm_load_string(vm, srcd, "d");
        ky_vm_load_string(vm, srck, "k");
        kyValue arg = kv_i(packed);
        kyValue ret;
        CHECK(ky_vm_call(vm, "d", &arg, 1, &ret) == 0, "call despawn");
        CHECK(ky_vm_call(vm, "k", &arg, 1, &ret) == 0, "call alive");
        CHECK(ret.type == KYT_FLOAT && ret.as.fval == 0.0, "alive == 0 after despawn");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    /* 7) null-world path returns an error string from spawn */
    {
        kyVM *vm = ky_vm_create(NULL);
        KyxBinds *b = ky_bind_create(vm, NULL);
        const char *src = "function s(){ return std.spawn(); }";
        ky_vm_load_string(vm, src, "m");
        kyValue ret;
        CHECK(ky_vm_call(vm, "s", NULL, 0, &ret) == 0, "call spawn (null world)");
        CHECK(ret.type == KYT_STRING, "spawn with null world yields error string");
        ky_bind_destroy(b);
        ky_vm_destroy(vm);
    }

    ky_world_destroy(w);

    printf("%d assertions, %d failures\n", g_pass + g_fail, g_fail);
    return g_fail == 0 ? 0 : 1;
}
