#include "kronyx/editor_core.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations — find_*_tid defined later in this file */
static uint32_t find_transform_tid(kyWorld *w);
static uint32_t find_sprite_tid(kyWorld *w);
static uint32_t find_camera_tid(kyWorld *w);

struct kyEditorApp {
    kyWorld          *world;
    kyRendererBackend backend;
    kyEntity         selected;
    int              entity_cache_len;
    kyEntity         *entity_cache;
    size_t           entity_cache_cap;
    /* Directories of entities created via this editor (for spawn/despawn tracking). */
    kyEntity         *spawned;
    size_t           spawned_len;
    size_t           spawned_cap;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void editor_refresh_cache(kyEditorApp *app) {
    const kyWorld *w = app->world;
    if (!w) { app->entity_cache_len = 0; return; }

    /* Count total unique entities: alive ECS entities + spawned-but-not-alive. */
    int alive_count = ky_world_alive_count(w);
    int count = alive_count;
    for (size_t si = 0; si < app->spawned_len; ++si) {
        int is_alive = 0;
        for (int ai = 0; ai < alive_count; ++ai) {
            kyEntity ae = ky_world_get_alive_entity(w, ai);
            if (ae.id == app->spawned[si].id && ae.version == app->spawned[si].version) {
                is_alive = 1;
                break;
            }
        }
        if (!is_alive) count++;
    }

    if ((size_t)count > app->entity_cache_cap) {
        size_t new_cap = app->entity_cache_cap == 0 ? 16 : app->entity_cache_cap * 2;
        while ((size_t)count > new_cap) new_cap *= 2;
        kyEntity *new_buf = (kyEntity *)realloc(app->entity_cache,
                                                new_cap * sizeof(kyEntity));
        if (!new_buf) return;
        app->entity_cache = new_buf;
        app->entity_cache_cap = new_cap;
    }

    /* Fill cache: alive entities first, then spawned-but-not-alive. */
    int i = 0;
    for (int ai = 0; ai < alive_count && i < (int)app->entity_cache_cap; ++ai) {
        app->entity_cache[i++] = ky_world_get_alive_entity(w, ai);
    }
    for (size_t si = 0; si < app->spawned_len && i < (int)app->entity_cache_cap; ++si) {
        int is_alive = 0;
        for (int ai = 0; ai < alive_count; ++ai) {
            kyEntity ae = ky_world_get_alive_entity(w, ai);
            if (ae.id == app->spawned[si].id && ae.version == app->spawned[si].version) {
                is_alive = 1;
                break;
            }
        }
        if (!is_alive) {
            app->entity_cache[i++] = app->spawned[si];
        }
    }
    app->entity_cache_len = i;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

kyEditorApp *ky_editor_app_create(kyWorld *world,
                                  kyRendererBackend backend,
                                  void *platform_win) {
    KY_UNUSED(platform_win);
    if (!world) return NULL;

    kyEditorApp *app = (kyEditorApp *)calloc(1, sizeof(kyEditorApp));
    if (!app) return NULL;

    app->world = world;
    app->backend = backend;
    app->selected.id = 0;
    app->selected.version = 0;
    app->entity_cache_len = 0;
    app->entity_cache_cap = 0;
    app->entity_cache = NULL;
    app->spawned_len = 0;
    app->spawned_cap = 0;
    app->spawned = NULL;

    editor_refresh_cache(app);
    return app;
}

void ky_editor_app_destroy(kyEditorApp *app) {
    if (!app) return;
    free(app->entity_cache);
    free(app->spawned);
    free(app);
}

int ky_editor_app_count_entities(const kyEditorApp *app) {
    if (!app) return 0;
    editor_refresh_cache((kyEditorApp *)app);
    return app->entity_cache_len;
}

kyEntity ky_editor_app_get_entity(const kyEditorApp *app, int idx) {
    kyEntity zero = {0, 0};
    if (!app || idx < 0) return zero;
    editor_refresh_cache((kyEditorApp *)app);
    if ((size_t)idx >= (size_t)app->entity_cache_len) return zero;
    return app->entity_cache[idx];
}

void ky_editor_app_select(kyEditorApp *app, kyEntity e) {
    if (!app) return;
    app->selected = e;
}

kyEntity ky_editor_app_get_selected(const kyEditorApp *app) {
    if (!app) { kyEntity z = {0,0}; return z; }
    return app->selected;
}

int ky_editor_app_selected_is_valid(const kyEditorApp *app) {
    if (!app || !app->world) return 0;
    return ky_entity_valid(app->world, app->selected);
}

kyEntity ky_editor_app_spawn_and_select(kyEditorApp *app) {
    kyEntity z = {0, 0};
    if (!app || !app->world) return z;
    kyEntity e = ky_world_spawn(app->world);
    app->selected = e;
    /* Track this spawn so it appears in hierarchy even before components added. */
    if (app->spawned_len >= app->spawned_cap) {
        size_t new_cap = app->spawned_cap == 0 ? 8 : app->spawned_cap * 2;
        kyEntity *nb = (kyEntity *)realloc(app->spawned, new_cap * sizeof(kyEntity));
        if (!nb) return e;
        app->spawned = nb;
        app->spawned_cap = new_cap;
    }
    app->spawned[app->spawned_len++] = e;
    editor_refresh_cache(app);
    return e;
}

void ky_editor_app_despawn_selected(kyEditorApp *app) {
    if (!app || !app->world) return;
    if (!ky_entity_valid(app->world, app->selected)) return;
    ky_world_despawn(app->world, app->selected);
    /* Remove from spawned tracking list */
    for (size_t i = 0; i < app->spawned_len; ++i) {
        if (app->spawned[i].id == app->selected.id &&
            app->spawned[i].version == app->selected.version) {
            app->spawned[i] = app->spawned[app->spawned_len - 1];
            app->spawned_len--;
            break;
        }
    }
    app->selected.id = 0;
    app->selected.version = 0;
    editor_refresh_cache(app);
}

/* ── Transform ── */

uint32_t ky_editor_transform_type_id(const kyWorld *w) {
    return find_transform_tid((kyWorld *)w);
}

uint32_t ky_editor_sprite_type_id(const kyWorld *w) {
    return find_sprite_tid((kyWorld *)w);
}

uint32_t ky_editor_camera_type_id(const kyWorld *w) {
    return find_camera_tid((kyWorld *)w);
}

static uint32_t find_transform_tid(kyWorld *w) {
    if (!w) return (uint32_t)-1;
    for (uint32_t i = 0; ; ++i) {
        const kyComponentType *ct = ky_world_component_type(w, i);
        if (!ct) return (uint32_t)-1;
        if (ct->name && (strcmp(ct->name, "transform") == 0 ||
                         strcmp(ct->name, "Transform") == 0))
            return i;
    }
}

static uint32_t find_sprite_tid(kyWorld *w) {
    if (!w) return (uint32_t)-1;
    for (uint32_t i = 0; ; ++i) {
        const kyComponentType *ct = ky_world_component_type(w, i);
        if (!ct) return (uint32_t)-1;
        if (ct->name && (strcmp(ct->name, "sprite") == 0 ||
                         strcmp(ct->name, "Sprite") == 0))
            return i;
    }
}

static uint32_t find_camera_tid(kyWorld *w) {
    if (!w) return (uint32_t)-1;
    for (uint32_t i = 0; ; ++i) {
        const kyComponentType *ct = ky_world_component_type(w, i);
        if (!ct) return (uint32_t)-1;
        if (ct->name && (strcmp(ct->name, "camera2d") == 0 ||
                         strcmp(ct->name, "Camera2D") == 0))
            return i;
    }
}

void ky_editor_app_set_pos(kyEditorApp *app, float x, float y) {
    if (!app || !app->world) return;
    if (!ky_entity_valid(app->world, app->selected)) return;
    uint32_t tid = find_transform_tid(app->world);
    if (tid == (uint32_t)-1) return;
    kyTransform *tr = (kyTransform *)ky_world_get_component(app->world,
                                                            app->selected, tid);
    if (!tr) return;
    tr->pos.x = x;
    tr->pos.y = y;
}

float ky_editor_app_get_pos_x(const kyEditorApp *app) {
    if (!app || !app->world) return 0.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 0.0f;
    uint32_t tid = find_transform_tid(app->world);
    if (tid == (uint32_t)-1) return 0.0f;
    kyTransform *tr = (kyTransform *)ky_world_get_component(app->world,
                                                            app->selected, tid);
    if (!tr) return 0.0f;
    return tr->pos.x;
}

float ky_editor_app_get_pos_y(const kyEditorApp *app) {
    if (!app || !app->world) return 0.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 0.0f;
    uint32_t tid = find_transform_tid(app->world);
    if (tid == (uint32_t)-1) return 0.0f;
    kyTransform *tr = (kyTransform *)ky_world_get_component(app->world,
                                                            app->selected, tid);
    if (!tr) return 0.0f;
    return tr->pos.y;
}

void ky_editor_app_set_scale(kyEditorApp *app, float sx, float sy) {
    if (!app || !app->world) return;
    if (!ky_entity_valid(app->world, app->selected)) return;
    uint32_t tid = find_transform_tid(app->world);
    if (tid == (uint32_t)-1) return;
    kyTransform *tr = (kyTransform *)ky_world_get_component(app->world,
                                                            app->selected, tid);
    if (!tr) return;
    tr->scale.x = sx;
    tr->scale.y = sy;
}

float ky_editor_app_get_scale_x(const kyEditorApp *app) {
    if (!app || !app->world) return 1.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 1.0f;
    uint32_t tid = find_transform_tid(app->world);
    if (tid == (uint32_t)-1) return 1.0f;
    kyTransform *tr = (kyTransform *)ky_world_get_component(app->world,
                                                            app->selected, tid);
    if (!tr) return 1.0f;
    return tr->scale.x;
}

float ky_editor_app_get_scale_y(const kyEditorApp *app) {
    if (!app || !app->world) return 1.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 1.0f;
    uint32_t tid = find_transform_tid(app->world);
    if (tid == (uint32_t)-1) return 1.0f;
    kyTransform *tr = (kyTransform *)ky_world_get_component(app->world,
                                                            app->selected, tid);
    if (!tr) return 1.0f;
    return tr->scale.y;
}

/* ── Sprite ── */

void ky_editor_app_set_sprite_layer(kyEditorApp *app, int layer) {
    if (!app || !app->world) return;
    if (!ky_entity_valid(app->world, app->selected)) return;
    uint32_t tid = find_sprite_tid(app->world);
    if (tid == (uint32_t)-1) return;
    kySprite *sp = (kySprite *)ky_world_get_component(app->world,
                                                      app->selected, tid);
    if (!sp) return;
    sp->layer = layer;
}

int ky_editor_app_get_sprite_layer(const kyEditorApp *app) {
    if (!app || !app->world) return 0;
    if (!ky_entity_valid(app->world, app->selected)) return 0;
    uint32_t tid = find_sprite_tid(app->world);
    if (tid == (uint32_t)-1) return 0;
    kySprite *sp = (kySprite *)ky_world_get_component(app->world,
                                                      app->selected, tid);
    if (!sp) return 0;
    return sp->layer;
}

/* ── Camera2D ── */

void ky_editor_app_set_camera_zoom(kyEditorApp *app, float zoom) {
    if (!app || !app->world) return;
    if (!ky_entity_valid(app->world, app->selected)) return;
    uint32_t tid = find_camera_tid(app->world);
    if (tid == (uint32_t)-1) return;
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(app->world,
                                                           app->selected, tid);
    if (!cam) return;
    cam->zoom = zoom;
}

float ky_editor_app_get_camera_zoom(const kyEditorApp *app) {
    if (!app || !app->world) return 1.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 1.0f;
    uint32_t tid = find_camera_tid(app->world);
    if (tid == (uint32_t)-1) return 1.0f;
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(app->world,
                                                           app->selected, tid);
    if (!cam) return 1.0f;
    return cam->zoom;
}

void ky_editor_app_set_camera_viewport_size(kyEditorApp *app, float w, float h) {
    if (!app || !app->world) return;
    if (!ky_entity_valid(app->world, app->selected)) return;
    uint32_t tid = find_camera_tid(app->world);
    if (tid == (uint32_t)-1) return;
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(app->world,
                                                           app->selected, tid);
    if (!cam) return;
    cam->viewport.x = w;
    cam->viewport.y = h;
}

float ky_editor_app_get_camera_viewport_w(const kyEditorApp *app) {
    if (!app || !app->world) return 0.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 0.0f;
    uint32_t tid = find_camera_tid(app->world);
    if (tid == (uint32_t)-1) return 0.0f;
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(app->world,
                                                           app->selected, tid);
    if (!cam) return 0.0f;
    return cam->viewport.x;
}

float ky_editor_app_get_camera_viewport_h(const kyEditorApp *app) {
    if (!app || !app->world) return 0.0f;
    if (!ky_entity_valid(app->world, app->selected)) return 0.0f;
    uint32_t tid = find_camera_tid(app->world);
    if (tid == (uint32_t)-1) return 0.0f;
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(app->world,
                                                           app->selected, tid);
    if (!cam) return 0.0f;
    return cam->viewport.y;
}

/* ── Input ── */

int ky_editor_app_poll_input(const kyEditorApp *app,
                             kyInputKey *out_key, int *out_pressed) {
    if (!app || !out_key || !out_pressed) return 0;
    kyInputEvent ev;
    int got = ky_input_poll(&ev);
    if (got) {
        *out_key = ev.key;
        *out_pressed = ev.pressed;
    }
    return got;
}

/* ── Rendering ── */

int ky_editor_app_render(const kyEditorApp *app, kyRenderDevice *rd) {
    if (!app || !rd) return -1;
    kyWorld *w = app->world;
    if (!w) return -2;

    /* Use selected entity as camera if it has Camera2D */
    kyEntity cam = app->selected;
    uint32_t ctid = find_camera_tid(w);
    int has_cam = (ctid != (uint32_t)-1 &&
                   ky_world_has_component(w, cam, ctid));
    if (!ky_entity_valid(w, cam) || !has_cam) {
        /* Find first entity with both Transform and Camera2D */
        const uint32_t cam_types[] = {
            find_transform_tid(w),
            find_camera_tid(w)
        };
        kyViewIter it;
        int found = 0;
        if (cam_types[0] != (uint32_t)-1 && cam_types[1] != (uint32_t)-1) {
            if (ky_view_begin(w, cam_types, 2, &it)) {
                cam = it.current;
                found = 1;
            }
        }
        if (!found) {
            cam.id = 0;
            cam.version = 0;
        }
    }

    return ky2d_render_world(rd, w, cam);
}
