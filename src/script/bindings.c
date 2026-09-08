/*
 * Copyright (C) 2026. All rights reserved.
 * kronyx
 *
 *  G3 kyx game bindings.
 *
 *  Usage (C side):
 *      KyxBinds *b = ky_bind_create(vm, world);   // registers std.* natives
 *      ... scripts can call std.spawn / std.setpos / std.poll_input etc.
 *      ky_bind_destroy(b);                        // frees the context
 *
 *  Script side:
 *      function update(dt) {
 *          std.poll_input()
 *          if std.axis_x() > 0 { std.setpos(hero, 1.0, 0.78) }
 *      }
 */

#include "kronyx/script.h"
#include "kronyx/ecs.h"
#include "kronyx/2d.h"
#include "kronyx/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct KyxBinds {
    kyWorld *world;
    float    axis_x;
    int      last_key;
} KyxBinds;

/* helpers */
static kyValue kv_nil(void)         { kyValue v; v.type=KYT_NIL;    return v; }
static kyValue kv_int(int64_t x)    { kyValue v; v.type=KYT_INT;   v.as.ival=x;       return v; }
static kyValue kv_float(float x)    { kyValue v; v.type=KYT_FLOAT; v.as.fval=x;       return v; }
static kyValue kv_err(const char *m){ kyValue v; v.type=KYT_STRING;v.as.sval=m;       return v; }

static float argf(const kyValue *a) {
    if (!a) return 0.0f;
    if (a->type == KYT_FLOAT) return (float)a->as.fval;
    if (a->type == KYT_INT)   return (float)a->as.ival;
    if (a->type == KYT_BOOL)  return a->as.ival ? 1.0f : 0.0f;
    return 0.0f;
}

/* Entity roundtrip: low 32 = id, high 32 = version (ky_entity_valid checks
 * version, so we must preserve it across the script boundary). */
static int64_t ent_pack(kyEntity e) {
    return ((int64_t)e.version << 32) | (int64_t)(uint32_t)e.id;
}
static kyEntity ent_unpack(int64_t p) {
    kyEntity e;
    e.id      = (uint32_t)(p & 0xFFFFFFFFu);
    e.version = (uint32_t)((uint64_t)p >> 32);
    return e;
}
static int arg_ent(const kyValue *a, kyEntity *out) {
    if (!a || (a->type != KYT_INT && a->type != KYT_FLOAT)) return -1;
    int64_t p = (a->type == KYT_INT) ? a->as.ival : (int64_t)a->as.fval;
    *out = ent_unpack(p);
    return 0;
}

/* ---------- natives: entity ---------- */

static kyValue n_spawn(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)args;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    kyEntity e = ky_world_spawn(b->world);
    return kv_int(ent_pack(e));
}

static kyValue n_alive(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    if (argc < 1)        return kv_err("need entity");
    kyEntity e;
    if (arg_ent(&args[0], &e) != 0) return kv_err("bad entity");
    return kv_int(ky_entity_valid(b->world, e) ? 1LL : 0LL);
}

static kyValue n_despawn(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    if (argc < 1)        return kv_err("need entity");
    kyEntity e;
    if (arg_ent(&args[0], &e) != 0) return kv_err("bad entity");
    ky_world_despawn(b->world, e);
    return kv_nil();
}

/* ---------- natives: transform (entity, [axis]) ---------- */

#define KB_NO_TID ((uint32_t)0xFFFFFFFFu)
static uint32_t tid_transform(kyWorld *w) {
    for (uint32_t i = 0; ; i++) {
        const kyComponentType *ct = ky_world_component_type(w, i);
        if (!ct) return KB_NO_TID;
        if (ct->name && (strcmp(ct->name, "transform") == 0 ||
                         strcmp(ct->name, "Transform") == 0))
            return i;
    }
}

static kyValue n_setpos(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    if (argc < 3)        return kv_err("need entity,x,y");
    uint32_t tid = tid_transform(b->world);
    if (tid == KB_NO_TID)        return kv_err("Transform not registered");
    kyEntity e;
    if (arg_ent(&args[0], &e) != 0) return kv_err("bad entity");
    void *tr = ky_world_get_component(b->world, e, tid);
    if (!tr) return kv_err("entity without Transform");
    kyTransform *t = (kyTransform *)tr;
    t->pos.x = argf(&args[1]);
    t->pos.y = argf(&args[2]);
    return kv_nil();
}

static kyValue n_getpos(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    if (argc < 2)        return kv_err("need entity,axis");
    uint32_t tid = tid_transform(b->world);
    if (tid == KB_NO_TID)        return kv_err("Transform not registered");
    kyEntity e;
    if (arg_ent(&args[0], &e) != 0) return kv_err("bad entity");
    void *tr = ky_world_get_component(b->world, e, tid);
    if (!tr) return kv_err("entity without Transform");
    const kyTransform *t = (const kyTransform *)tr;
    int axis = (int)argf(&args[1]);
    return kv_float(axis == 1 ? t->pos.y : t->pos.x);
}

static kyValue n_setscale(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    if (argc < 3)        return kv_err("need entity,sx,sy");
    uint32_t tid = tid_transform(b->world);
    if (tid == KB_NO_TID)        return kv_err("Transform not registered");
    kyEntity e;
    if (arg_ent(&args[0], &e) != 0) return kv_err("bad entity");
    void *tr = ky_world_get_component(b->world, e, tid);
    if (!tr) return kv_err("entity without Transform");
    kyTransform *t = (kyTransform *)tr;
    t->scale.x = argf(&args[1]);
    t->scale.y = argf(&args[2]);
    return kv_nil();
}

static kyValue n_getscale(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)argc;
    KyxBinds *b = user;
    if (!b || !b->world) return kv_err("no world");
    if (argc < 2)        return kv_err("need entity,axis");
    uint32_t tid = tid_transform(b->world);
    if (tid == KB_NO_TID)        return kv_err("Transform not registered");
    kyEntity e;
    if (arg_ent(&args[0], &e) != 0) return kv_err("bad entity");
    void *tr = ky_world_get_component(b->world, e, tid);
    if (!tr) return kv_err("entity without Transform");
    const kyTransform *t = (const kyTransform *)tr;
    int axis = (int)argf(&args[1]);
    return kv_float(axis == 1 ? t->scale.y : t->scale.x);
}

/* ---------- natives: input ---------- */

static kyValue n_poll_input(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)args;(void)argc;
    KyxBinds *b = user;
    if (!b) return kv_err("no ctx");
    b->axis_x   = 0.0f;
    b->last_key = 0;
    kyInputEvent ev;
    int left = 0, right = 0;
    while (ky_input_poll(&ev) > 0) {
        if (ev.pressed) {
            if (ev.key == KY_KEY_A || ev.key == KY_KEY_ARROW_LEFT)  left  = 1;
            if (ev.key == KY_KEY_D || ev.key == KY_KEY_ARROW_RIGHT) right = 1;
            b->last_key = ev.key;
        }
    }
    if (right) b->axis_x =  1.0f;
    else if (left) b->axis_x = -1.0f;
    return kv_nil();
}

static kyValue n_axis_x(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)args;(void)argc;
    KyxBinds *b = user;
    if (!b) return kv_err("no ctx");
    return kv_float(b->axis_x);
}

static kyValue n_last_key(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)args;(void)argc;
    KyxBinds *b = user;
    if (!b) return kv_err("no ctx");
    return kv_int(b->last_key);
}

/* ---------- natives: log ---------- */

static const char *val_str(const kyValue *v, char *buf, size_t cap) {
    switch (v->type) {
        case KYT_STRING: return v->as.sval;
        case KYT_INT:    snprintf(buf, cap, "%lld", (long long)v->as.ival); return buf;
        case KYT_FLOAT:  snprintf(buf, cap, "%g", v->as.fval);               return buf;
        case KYT_BOOL:   return v->as.ival ? "true" : "false";
        default:         return "nil";
    }
}

static kyValue n_log(kyVM *vm, kyValue *args, int argc, void *user) {
    (void)vm;(void)user;
    char buf[64];
    for (int i = 0; i < argc; i++) {
        const char *s = val_str(&args[i], buf, sizeof(buf));
        if (i) fputs(" ", stderr);
        fputs(s, stderr);
    }
    fputs("\n", stderr);
    return kv_nil();
}

/* ---------- public ---------- */

KyxBinds *ky_bind_create(kyVM *vm, kyWorld *world)
{
    if (!vm) return NULL;
    KyxBinds *b = (KyxBinds *)calloc(1, sizeof(*b));
    if (!b)   return NULL;
    b->world = world;
    b->axis_x = 0.0f;
    b->last_key = 0;

    ky_vm_register_native(vm, "std", "spawn",      n_spawn,     b);
    ky_vm_register_native(vm, "std", "alive",      n_alive,     b);
    ky_vm_register_native(vm, "std", "despawn",    n_despawn,   b);
    ky_vm_register_native(vm, "std", "setpos",     n_setpos,    b);
    ky_vm_register_native(vm, "std", "getpos",     n_getpos,    b);
    ky_vm_register_native(vm, "std", "setscale",   n_setscale,  b);
    ky_vm_register_native(vm, "std", "getscale",   n_getscale,  b);
    ky_vm_register_native(vm, "std", "poll_input", n_poll_input,b);
    ky_vm_register_native(vm, "std", "axis_x",     n_axis_x,    b);
    ky_vm_register_native(vm, "std", "last_key",   n_last_key,  b);
    ky_vm_register_native(vm, "std", "log",        n_log,       b);
    return b;
}

void ky_bind_destroy(KyxBinds *b)
{
    free(b);
}
