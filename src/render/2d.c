#include "kronyx/2d.h"
#include "kronyx/log.h"
#include <string.h>

#define KY2D_MAX_SPRITES_BATCH 4096
#define KY2D_MAX_SPRITES_FRAME 16384

/* ------------------------------------------------------------------ */
/* Component defaults                                                  */
/* ------------------------------------------------------------------ */

kyTransform ky_transform_new(void) {
    kyTransform t;
    t.pos.x = 0.0f;
    t.pos.y = 0.0f;
    t.rotation_z = 0.0f;
    t.scale.x = 1.0f;
    t.scale.y = 1.0f;
    t.z = 0.0f;
    return t;
}

kySprite ky_sprite_new(void) {
    kySprite s;
    memset(&s, 0, sizeof(s));
    s.color.x = 1.0f;
    s.color.y = 1.0f;
    s.color.z = 1.0f;
    s.color.w = 1.0f;
    s.size.x = 1.0f;
    s.size.y = 1.0f;
    s.uv0.x = 0.0f;
    s.uv0.y = 0.0f;
    s.uv1.x = 1.0f;
    s.uv1.y = 1.0f;
    s.layer = 0;
    s.flip = 0;
    return s;
}

kyCamera2D ky_camera2d_new(void) {
    kyCamera2D c;
    memset(&c, 0, sizeof(c));
    c.zoom = 1.0f;
    c.viewport.x = 10.0f;
    c.viewport.y = 10.0f;
    c.clear_color.x = 0.1f;
    c.clear_color.y = 0.1f;
    c.clear_color.z = 0.1f;
    c.clear_color.w = 1.0f;
    return c;
}

/* ------------------------------------------------------------------ */
/* Component registration (idempotent per world, by component name)    */
/* ------------------------------------------------------------------ */

static void transform_ctor(void *c) { *(kyTransform *)c = ky_transform_new(); }
static void sprite_ctor(void *c) { *(kySprite *)c = ky_sprite_new(); }
static void camera_ctor(void *c) { *(kyCamera2D *)c = ky_camera2d_new(); }
static void noop_dtor(void *c) { KY_UNUSED(c); }

static int find_registered(const kyWorld *w, const char *name, uint32_t *out_id) {
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *t = (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (t && t->name && strcmp(t->name, name) == 0) {
            *out_id = t->type_id;
            return 1;
        }
    }
    return 0;
}

static int register_one(kyWorld *w, const char *name, size_t size,
                        void (*ctor)(void *), uint32_t *out_id) {
    if (find_registered(w, name, out_id)) return 0;
    kyComponentType t;
    memset(&t, 0, sizeof(t));
    t.name = name;
    t.size = size;
    t.ctor = ctor;
    t.dtor = noop_dtor;
    *out_id = ky_world_register_component(w, &t);
    return *out_id == UINT32_MAX ? -1 : 0;
}

int ky2d_register_components(kyWorld *w) {
    static uint32_t tid_transform, tid_sprite, tid_camera;
    if (!w) return -1;
    if (register_one(w, "transform", sizeof(kyTransform), transform_ctor, &tid_transform) != 0)
        return -2;
    if (register_one(w, "sprite", sizeof(kySprite), sprite_ctor, &tid_sprite) != 0)
        return -3;
    if (register_one(w, "camera2d", sizeof(kyCamera2D), camera_ctor, &tid_camera) != 0)
        return -4;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Camera math                                                         */
/* ------------------------------------------------------------------ */

kyMat4 ky2d_camera_view_proj(const kyCamera2D *cam, const kyTransform *tr) {
    kyVec2 cpos = cam->pos;
    float crot = cam->rotation_z;
    float zoom = cam->zoom > 0.0f ? cam->zoom : 1.0f;
    if (tr) {
        cpos = ky_vec2_add(cpos, tr->pos);
        crot += tr->rotation_z;
    }

    kyVec3 t = {-cpos.x, -cpos.y, 0.0f};
    kyMat4 view = ky_mat4_translate(t);

    float half = -crot * 0.5f; /* view rotates the world by -camera rotation */
    kyQuat q = {0.0f, 0.0f, sinf(half), cosf(half)};
    kyMat4 rot = ky_mat4_rotation(q);
    view = ky_mat4_mul(&view, &rot);

    /* zoom scales how many EU fit on screen: visible extent = viewport / zoom */
    float hw = cam->viewport.x * 0.5f / zoom;
    float hh = cam->viewport.y * 0.5f / zoom;
    kyMat4 proj = ky_mat4_ortho(-hw, hw, -hh, hh, -1.0f, 1.0f);

    return ky_mat4_mul(&proj, &view);
}

/* ------------------------------------------------------------------ */
/* Batch renderer                                                      */
/* ------------------------------------------------------------------ */

typedef struct ky2dVertex {
    float x, y;
    float u, v;
    uint8_t r, g, b, a;
} ky2dVertex;

typedef struct ky2dItem {
    const kySprite *sp;
    kyTransform tr; /* resolved (identity when component missing) */
    uint32_t id;
    int layer;
    float z;
} ky2dItem;

static ky2dItem g_items[KY2D_MAX_SPRITES_FRAME];
static size_t g_item_count;

static ky2dVertex g_vbo[KY2D_MAX_SPRITES_BATCH * 4];
static uint16_t g_ibo[KY2D_MAX_SPRITES_BATCH * 6];

static int g_debug = 0;

void ky2d_render_debug(int on) { g_debug = on ? 1 : 0; }

/* Lazy per-device GPU resources. V1 assumes a single RenderDevice. */
typedef struct ky2dCache {
    kyRenderDevice *rd;
    kyShader *shader;
    kyPipeline *pipeline;
    kyBuffer *vbo;
    kyBuffer *ibo;
    kyTexture *white;
    int uvp_loc;
} ky2dCache;

static ky2dCache g_cache;
static ky2dContext g_ctx_default = {NULL, NULL, NULL};

static void cache_destroy_with_rd(kyRenderDevice *rd) {
    if (!rd) return;
    if (g_cache.white)    ky_rd_destroy_texture(rd, g_cache.white);
    if (g_cache.vbo)      ky_rd_destroy_buffer(rd, g_cache.vbo);
    if (g_cache.ibo)      ky_rd_destroy_buffer(rd, g_cache.ibo);
    if (g_cache.pipeline) ky_rd_destroy_pipeline(rd, g_cache.pipeline);
    if (g_cache.shader)   ky_rd_destroy_shader(rd, g_cache.shader);
    memset(&g_cache, 0, sizeof(g_cache));
}

static void cache_destroy(void) {
    /* Cached resources are owned by their RenderDevice. The device may
     * already be destroyed when we get here (stale pointer), so we only
     * drop our references; GL objects die with their device context.
     * V1 assumes a single device per process. */
    memset(&g_cache, 0, sizeof(g_cache));
}

static const char *VS_SRC =
    "#version 300 es\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_color;\n"
    "uniform mat4 u_vp;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main(){ v_uv = a_uv; v_color = a_color;\n"
    "  gl_Position = u_vp * vec4(a_pos, 0.0, 1.0); }\n";

static const char *FS_SRC =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "uniform sampler2D u_tex;\n" /* sampler defaults to unit 0 */
    "out vec4 frag;\n"
    "void main(){ frag = texture(u_tex, clamp(v_uv, 0.0, 1.0)) * v_color; }\n";

static int cache_init(kyRenderDevice *rd) {
    if (g_cache.rd == rd && g_cache.pipeline && g_cache.vbo && g_cache.ibo)
        return 0;
    if (g_cache.rd != rd && g_cache.rd)
        cache_destroy_with_rd(g_cache.rd);
    cache_destroy();
    g_cache.rd = rd;

    kyShaderSource src;
    memset(&src, 0, sizeof(src));
    src.vs = VS_SRC;
    src.fs = FS_SRC;
    src.entry = "main";
    g_cache.shader = ky_rd_create_shader(rd, &src);
    if (!g_cache.shader) {
        ky_log_write(KY_LOG_ERROR, "2d: builtin shader creation failed");
        return -1;
    }
    g_cache.uvp_loc = ky_rd_shader_uniform(rd, g_cache.shader, "u_vp");
    if (g_cache.uvp_loc < 0)
        ky_log_write(KY_LOG_WARN, "2d: uniform u_vp not found (backend may ignore uniforms)");

    kyVertexAttrib attribs[3];
    memset(attribs, 0, sizeof(attribs));
    attribs[0].location = 0;
    attribs[0].offset = 0;
    attribs[0].size = 2;
    attribs[0].type = KY_ATTRIB_FLOAT;
    attribs[1].location = 1;
    attribs[1].offset = 8;
    attribs[1].size = 2;
    attribs[1].type = KY_ATTRIB_FLOAT;
    attribs[2].location = 2;
    attribs[2].offset = 16;
    attribs[2].size = 4;
    attribs[2].normalized = 1;
    attribs[2].type = KY_ATTRIB_UBYTE;

    kyPipelineDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.shader = g_cache.shader;
    desc.layout.attribs = attribs;
    desc.layout.count = 3;
    desc.layout.stride = sizeof(ky2dVertex);
    desc.topology = KY_TRIANGLES;
    desc.depth_test = 0;
    desc.depth_write = 0;
    desc.cull_mode = 0;
    desc.blend.on = 1;
    kyVec4 one = {1.0f, 1.0f, 1.0f, 1.0f};
    desc.blend.color = one;
    g_cache.pipeline = ky_rd_create_pipeline(rd, &desc);
    if (!g_cache.pipeline) {
        ky_log_write(KY_LOG_ERROR, "2d: pipeline creation failed");
        return -1;
    }

    g_cache.vbo = ky_rd_create_buffer(rd, sizeof(g_vbo), NULL, 1);
    g_cache.ibo = ky_rd_create_buffer(rd, sizeof(g_ibo), NULL, 1);
    if (!g_cache.vbo || !g_cache.ibo) {
        ky_log_write(KY_LOG_ERROR, "2d: batch buffer creation failed");
        return -1;
    }

    static const uint8_t white_px[4] = {255, 255, 255, 255};
    g_cache.white = ky_rd_create_texture_2d(rd, 1, 1, 4, white_px);
    if (!g_cache.white) {
        ky_log_write(KY_LOG_ERROR, "2d: white texture creation failed");
        return -1;
    }
    return 0;
}

static int item_cmp(const void *pa, const void *pb) {
    const ky2dItem *a = (const ky2dItem *)pa;
    const ky2dItem *b = (const ky2dItem *)pb;
    if (a->layer != b->layer) return a->layer < b->layer ? -1 : 1;
    if (a->z != b->z) return a->z < b->z ? -1 : 1;
    return a->id < b->id ? -1 : (a->id > b->id ? 1 : 0);
}

static uint8_t to_ub(float c) {
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;
    return (uint8_t)(c * 255.0f + 0.5f);
}

static void emit_sprite(const ky2dItem *it, size_t base_vertex) {
    const kySprite *sp = it->sp;
    const kyTransform *tr = &it->tr;

    float hx = sp->size.x * 0.5f * tr->scale.x;
    float hy = sp->size.y * 0.5f * tr->scale.y;
    float cs = cosf(tr->rotation_z);
    float sn = sinf(tr->rotation_z);

    float lx[4] = {-hx, hx, hx, -hx};
    float ly[4] = {hy, hy, -hy, -hy};
    float tx[4] = {0, 1, 1, 0};
    float ty[4] = {1, 1, 0, 0};

    uint8_t r = to_ub(sp->color.x);
    uint8_t g = to_ub(sp->color.y);
    uint8_t b = to_ub(sp->color.z);
    uint8_t a = to_ub(sp->color.w);

    for (int i = 0; i < 4; i++) {
        ky2dVertex *v = &g_vbo[base_vertex + (size_t)i];
        float wx = tr->pos.x + lx[i] * cs - ly[i] * sn;
        float wy = tr->pos.y + lx[i] * sn + ly[i] * cs;
        v->x = wx;
        v->y = wy;
        float uu = sp->uv0.x + (sp->uv1.x - sp->uv0.x) * tx[i];
        float vv = sp->uv0.y + (sp->uv1.y - sp->uv0.y) * ty[i];
        if (sp->flip & 1) uu = sp->uv0.x + (sp->uv1.x - sp->uv0.x) * (1.0f - tx[i]);
        if (sp->flip & 2) vv = sp->uv0.y + (sp->uv1.y - sp->uv0.y) * (1.0f - ty[i]);
        v->u = uu;
        v->v = vv;
        v->r = r;
        v->g = g;
        v->b = b;
        v->a = a;
    }
}

/* Fill one batch of sprites into staging buffers; returns sprite count.
 *
 * If sprite_gen produces more than 4 vertices or 6 indices for any sprite,
 * the counts are clamped so we never over-read the g_vbo/g_ibo staging.
 * Users should implement sprite_gen to emit exactly one quad. */
static size_t fill_batch(const ky2dItem *batch, size_t batch_count,
                          const ky2dContext *ctx) {
    size_t n = 0;
    for (size_t s = 0; s < batch_count; s++) {
        size_t base_vertex = n * 4;
        if (ctx && ctx->sprite_gen) {
            size_t vc = 0, ic = 0;
            ctx->sprite_gen(&batch[s].tr, batch[s].sp,
                            &g_vbo[base_vertex], &g_ibo[n * 6],
                            &vc, &ic, ctx->user);
            if (vc > 4 || ic > 6) {
                ky_log_write(KY_LOG_WARN,
                             "2d: sprite_gen produced %zu verts / %zu idx "
                             "(max 4/6); clamping", vc, ic);
            }
        } else {
            emit_sprite(&batch[s], base_vertex);
            uint16_t base = (uint16_t)base_vertex;
            g_ibo[n * 6 + 0] = base;
            g_ibo[n * 6 + 1] = (uint16_t)(base + 1);
            g_ibo[n * 6 + 2] = (uint16_t)(base + 2);
            g_ibo[n * 6 + 3] = base;
            g_ibo[n * 6 + 4] = (uint16_t)(base + 2);
            g_ibo[n * 6 + 5] = (uint16_t)(base + 3);
        }
        n++;
    }
    return n;
}

static int collect(kyWorld *w, uint32_t tid_transform, uint32_t tid_sprite) {
    g_item_count = 0;
    int warned = 0;
    uint32_t types[1];
    types[0] = tid_sprite;
    kyViewIter it;
    int has = ky_view_begin(w, types, 1, &it);
    while (has) {
        if (g_item_count >= KY2D_MAX_SPRITES_FRAME) {
            ky_log_write(KY_LOG_WARN, "2d: sprite count capped at %d", KY2D_MAX_SPRITES_FRAME);
            break;
        }
        const kySprite *sp =
            (const kySprite *)ky_world_get_component(w, it.current, tid_sprite);
        if (!sp) {
            has = ky_view_next(&it);
            continue;
        }
        ky2dItem *item = &g_items[g_item_count];
        item->sp = sp;
        item->id = it.current.id;
        const kyTransform *tr =
            (const kyTransform *)ky_world_get_component(w, it.current, tid_transform);
        if (tr) {
            item->tr = *tr;
        } else {
            item->tr = ky_transform_new();
            if (!warned) {
                ky_log_write(KY_LOG_WARN, "2d: sprite on entity %u has no transform, using identity", it.current.id);
                warned = 1;
            }
        }
        /* degenerate: zero area after scaling is skipped */
        if (sp->size.x * item->tr.scale.x == 0.0f ||
            sp->size.y * item->tr.scale.y == 0.0f) {
            has = ky_view_next(&it);
            continue;
        }
        item->layer = sp->layer;
        item->z = item->tr.z;
        g_item_count++;
        has = ky_view_next(&it);
    }
    return 0;
}

static int render_frame(kyRenderDevice *rd, kyWorld *w, const kyCamera2D *cam,
                        const kyTransform *cam_tr, uint32_t tid_transform,
                        uint32_t tid_sprite, const ky2dContext *ctx) {
    if (cache_init(rd) != 0) return -2;

    if (collect(w, tid_transform, tid_sprite) != 0) return -3;

    qsort(g_items, g_item_count, sizeof(ky2dItem), item_cmp);

    kyMat4 vp_m;
    if (ctx && ctx->camera_matrix)
        vp_m = ctx->camera_matrix(cam, cam_tr, ctx->user);
    else
        vp_m = ky2d_camera_view_proj(cam, cam_tr);

    if (g_debug) {
        printf("[ky2d] frame: %zu sprites, clear=(%.2f,%.2f,%.2f,%.2f)\n",
               g_item_count, cam->clear_color.x, cam->clear_color.y,
               cam->clear_color.z, cam->clear_color.w);
        for (int r = 0; r < 4; r++)
            printf("[ky2d] vp col%d = %.3f %.3f %.3f %.3f\n", r,
                   vp_m.m[r * 4], vp_m.m[r * 4 + 1], vp_m.m[r * 4 + 2], vp_m.m[r * 4 + 3]);
    }

    ky_rd_clear(rd, cam->clear_color, 1.0f);
    void *cl = ky_rd_begin(rd);
    if (!cl) return -4;
    ky_cmd_set_pipeline(cl, g_cache.pipeline);

    ky_cmd_set_uniform(cl, g_cache.uvp_loc, &vp_m, (int)sizeof(kyMat4));

    size_t drawn = 0;
    size_t i = 0;
    while (i < g_item_count) {
        kyTexture *tex = g_items[i].sp->texture ? g_items[i].sp->texture : g_cache.white;
        size_t start = i;
        while (i < g_item_count && i - start < KY2D_MAX_SPRITES_BATCH) {
            kyTexture *cur = g_items[i].sp->texture ? g_items[i].sp->texture : g_cache.white;
            if (cur != tex) break;
            i++;
        }
        size_t n = fill_batch(&g_items[start], i - start, ctx);
        if (n == 0) continue;

        ky_rd_update_buffer(rd, g_cache.vbo, 0, n * 4 * sizeof(ky2dVertex), g_vbo);
        ky_rd_update_buffer(rd, g_cache.ibo, 0, n * 6 * sizeof(uint16_t), g_ibo);

        if (g_debug) {
            for (size_t s = start; s < i; s++) {
                const ky2dItem *it = &g_items[s];
                printf("[ky2d]   #%u layer=%d z=%.2f pos=(%.2f,%.2f) size=(%.2f,%.2f) tex=%s\n",
                       it->id, it->layer, it->z, it->tr.pos.x, it->tr.pos.y,
                       it->sp->size.x, it->sp->size.y,
                       it->sp->texture ? "user" : "white");
            }
        }

        ky_cmd_set_vertex_buffer(cl, g_cache.vbo, (uint32_t)sizeof(ky2dVertex));
        ky_cmd_set_index_buffer(cl, g_cache.ibo, 2);
        ky_cmd_set_texture(cl, 0, tex);
        ky_cmd_draw_indexed(cl, (uint32_t)(n * 6), 1);
        drawn += n;
    }

    ky_rd_submit(rd, cl);
    ky_rd_present(rd);
    return (int)drawn;
}

static const kyComponentType *world_find_type(const kyWorld *w, const char *name) {
    uint32_t id = ky_world_component_type_by_name(w, name);
    return id != UINT32_MAX ? ky_world_component_type(w, id) : NULL;
}

kyTexture *ky2d_make_texture(kyRenderDevice *rd, int w, int h, int channels, const void *pixels) {
    if (!rd || w <= 0 || h <= 0 || channels < 1 || channels > 4 || !pixels) return NULL;
    return ky_rd_create_texture_2d(rd, w, h, channels, pixels);
}

int ky2d_render_world(kyRenderDevice *rd, kyWorld *w, kyEntity cam) {
    if (!rd || !w) return -1;
    if (!ky_entity_valid(w, cam)) return -1;
    const kyComponentType *ct_sprite = world_find_type(w, "sprite");
    const kyComponentType *ct_transform = world_find_type(w, "transform");
    const kyComponentType *ct_camera = world_find_type(w, "camera2d");
    if (!ct_sprite || !ct_camera) {
        ky_log_write(KY_LOG_ERROR, "2d: world lacks sprite/camera2d registration");
        return -1;
    }
    const kyCamera2D *c =
        (const kyCamera2D *)ky_world_get_component(w, cam, ct_camera->type_id);
    if (!c) {
        ky_log_write(KY_LOG_ERROR, "2d: entity %u has no camera2d", cam.id);
        return -1;
    }
    if (c->viewport.x <= 0.0f || c->viewport.y <= 0.0f) return 0;
    const kyTransform *cam_tr =
        ct_transform ? (const kyTransform *)ky_world_get_component(w, cam, ct_transform->type_id)
                     : NULL;
    return render_frame(rd, w, c, cam_tr,
                        ct_transform ? ct_transform->type_id : 0,
                        ct_sprite->type_id, &g_ctx_default);
}

int ky2d_render_world_auto(kyRenderDevice *rd, kyWorld *w) {
    if (!rd || !w) return -1;
    const kyComponentType *ct_sprite = world_find_type(w, "sprite");
    const kyComponentType *ct_camera = world_find_type(w, "camera2d");
    if (!ct_sprite || !ct_camera) {
        ky_log_write(KY_LOG_ERROR, "2d: world lacks sprite/camera2d registration");
        return -1;
    }

    uint32_t types[1];
    types[0] = ct_camera->type_id;
    kyViewIter it;
    int has = ky_view_begin(w, types, 1, &it);
    kyEntity best;
    int found = 0;
    while (has) {
        const kyCamera2D *c =
            (const kyCamera2D *)ky_world_get_component(w, it.current, ct_camera->type_id);
        if (c && c->active) {
            if (!found || it.current.id < best.id) {
                best = it.current;
                found = 1;
            }
        }
        has = ky_view_next(&it);
    }
    if (!found) return 0;
    return ky2d_render_world(rd, w, best);
}

ky2dContext ky2d_context_default(void) {
    ky2dContext ctx = g_ctx_default;
    return ctx;
}

int ky2d_render_with(kyRenderDevice *rd, kyWorld *w, kyEntity cam,
                     const ky2dContext *ctx) {
    if (!rd || !w || !ctx) return -1;
    if (!ky_entity_valid(w, cam)) return -1;
    const kyComponentType *ct_sprite = world_find_type(w, "sprite");
    const kyComponentType *ct_transform = world_find_type(w, "transform");
    const kyComponentType *ct_camera = world_find_type(w, "camera2d");
    if (!ct_sprite || !ct_camera) {
        ky_log_write(KY_LOG_ERROR, "2d: world lacks sprite/camera2d registration");
        return -1;
    }
    const kyCamera2D *c =
        (const kyCamera2D *)ky_world_get_component(w, cam, ct_camera->type_id);
    if (!c) {
        ky_log_write(KY_LOG_ERROR, "2d: entity %u has no camera2d", cam.id);
        return -1;
    }
    if (c->viewport.x <= 0.0f || c->viewport.y <= 0.0f) return 0;
    const kyTransform *cam_tr =
        ct_transform ? (const kyTransform *)ky_world_get_component(w, cam, ct_transform->type_id)
                     : NULL;
    return render_frame(rd, w, c, cam_tr,
                        ct_transform ? ct_transform->type_id : 0,
                        ct_sprite->type_id, ctx);
}

/* Release GPU resources cached in g_cache for the given device.
 * Must be called before ky_rd_destroy(rd) if the 2D renderer was used. */
void ky2d_shutdown(kyRenderDevice *rd) {
    if (g_cache.rd != rd) return;
    cache_destroy_with_rd(rd);
}
