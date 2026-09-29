#include "kronyx/particle2d.h"
#include "kronyx/ecs.h"
#include "kronyx/log.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Pool                                                                */
/* ------------------------------------------------------------------ */

typedef struct KyParticle {
    float    px, py;
    float    vx, vy;
    float    age, inv_life;
    float    size0, size1;
    float    gx, gy, drag;          /* captured at emit: gravity + damping */
    uint8_t  fr, fg, fb, fa;        /* color from (0-255) */
    uint8_t  tr, tg, tb, ta;        /* color to   (0-255) */
    uint32_t owner;                 /* emitter entity id */
    int      alive;
} KyParticle;

static KyParticle g_pool[KY_PARTICLE_MAX];
static int g_alive_count = 0;
static int g_registered  = 0;

/* Deterministic xorshift32 PRNG. 0 is a fixed point, so we remap the seed
 * to a non-zero initial state. Two successive calls yield independent
 * unit-range floats in [0, 1). */
static uint32_t lcg_next(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0u) x = 0x12345678u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float lcg_unit(uint32_t *state) {
    return (float)(lcg_next(state) & 0xFFFFFF) / (float)0x1000000u;
}

/* Find a free slot. owner == KY_PARTICLE_OWNER_NONE means "any free slot". */
#define KY_PARTICLE_OWNER_NONE UINT32_MAX

static int pool_alloc(void) {
    for (int i = 0; i < KY_PARTICLE_MAX; i++) {
        if (!g_pool[i].alive) return i;
    }
    return -1;
}

static void pool_kill(int i) {
    g_pool[i].alive = 0;
    g_alive_count--;
}

/* ------------------------------------------------------------------ */
/* Component defaults                                                  */
/* ------------------------------------------------------------------ */

kyEmitter ky_emitter_new(void) {
    kyEmitter e;
    memset(&e, 0, sizeof(e));
    e.shape      = KY_EMIT_POINT;
    e.size0      = 0.1f;
    e.size1      = 0.1f;
    e.life0      = 1.0f;
    e.life1      = 1.0f;
    e.color_from = (kyVec4){1.0f, 1.0f, 1.0f, 1.0f};
    e.color_to   = (kyVec4){1.0f, 1.0f, 1.0f, 0.0f};
    e.seed       = 0x12345678u;
    e.rng_state  = 0u;
    e.emit_acc   = 0.0f;
    return e;
}

/* ------------------------------------------------------------------ */
/* Emit + step one emitter                                             */
/* ------------------------------------------------------------------ */

static uint8_t to_ub(float c) {
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;
    return (uint8_t)(c * 255.0f + 0.5f);
}

static int component_registered(const kyWorld *w, const char *name) {
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *t =
            (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (t && t->name && strcmp(t->name, name) == 0) return 1;
    }
    return 0;
}

static void emit_one(kyWorld *w, kyEntity e, kyEmitter *em, uint32_t owner_id) {
    int slot = pool_alloc();
    if (slot < 0) return; /* pool full: silent truncate */

    if (em->rng_state == 0u) em->rng_state = em->seed ? em->seed : 0x12345678u;
    uint32_t *st = &em->rng_state;

    float r1 = lcg_unit(st);
    float r2 = lcg_unit(st);
    float r3 = lcg_unit(st);
    float r4 = lcg_unit(st);

    KyParticle *p = &g_pool[slot];

    /* initial position offset per emit shape */
    float ox = 0.0f, oy = 0.0f;
    if (em->shape == KY_EMIT_CIRCLE && em->shape_scale > 0.0f) {
        float ang = r1 * 6.2831853071795864769f;
        ox = cosf(ang) * em->shape_scale;
        oy = sinf(ang) * em->shape_scale;
    } else if (em->shape == KY_EMIT_LINE && em->shape_scale > 0.0f) {
        float ca = cosf(em->direction), sa = sinf(em->direction);
        float t  = (r1 * 2.0f - 1.0f) * em->shape_scale;
        ox = ca * t;
        oy = sa * t;
    }

    /* initial velocity: random speed in [speed0, speed1] along direction */
    float speed = em->speed0 + r2 * (em->speed1 - em->speed0);
    p->vx = cosf(em->direction) * speed;
    p->vy = sinf(em->direction) * speed;

    /* emitter world position = entity transform pos (or origin) */
    float bx = 0.0f, by = 0.0f;
    uint32_t tid_tr = ky_world_component_type_by_name(w, "transform");
    if (tid_tr != UINT32_MAX) {
        const kyTransform *tr =
            (const kyTransform *)ky_world_get_component(w, e, tid_tr);
        if (tr) { bx = tr->pos.x; by = tr->pos.y; }
    }
    p->px = bx + ox;
    p->py = by + oy;

    /* lifetime in [life0, life1] */
    float life = em->life0 + r3 * (em->life1 - em->life0);
    if (life < 1e-4f) life = 1e-4f;
    p->inv_life = 1.0f / life;
    p->age      = 0.0f;

    /* size in [size0, size1] (we keep the *emitter's* range so the render
     * pass can interpolate over the particle's remaining life) */
    p->size0 = em->size0;
    p->size1 = em->size0 + r4 * (em->size1 - em->size0);

    /* color endpoints, byte domain 0..255 */
    p->fr = to_ub(em->color_from.x);
    p->fg = to_ub(em->color_from.y);
    p->fb = to_ub(em->color_from.z);
    p->fa = to_ub(em->color_from.w);
    p->tr = to_ub(em->color_to.x);
    p->tg = to_ub(em->color_to.y);
    p->tb = to_ub(em->color_to.z);
    p->ta = to_ub(em->color_to.w);

    /* capture kinematics so despawned emitters' particles still fly */
    p->gx   = em->gravity.x;
    p->gy   = em->gravity.y;
    p->drag = em->drag;

    p->owner = owner_id;
    p->alive = 1;
    g_alive_count++;
}

static void step_particles(kyWorld *w, float dt) {
    if (dt <= 0.0f) return;

    uint32_t tid_em = ky_world_component_type_by_name(w, "emitter");
    if (tid_em == UINT32_MAX) return;

    /* 1) Emission: each enabled emitter adds emit_rate*dt to its
     *    accumulator and spawns floor(acc) particles, keeping the
     *    fractional remainder for the next step (deterministic). */
    const uint32_t types[1] = { tid_em };
    kyViewIter it;
    int more = ky_view_begin(w, types, 1, &it);
    while (more) {
        kyEntity e   = it.current;
        kyEmitter *em = (kyEmitter *)ky_world_get_component(w, e, tid_em);
        if (em && em->enabled && em->emit_rate > 0.0f) {
            em->emit_acc += em->emit_rate * dt;
            int n = (int)em->emit_acc;
            if (n > 0) {
                em->emit_acc -= (float)n;
                for (int i = 0; i < n; i++) emit_one(w, e, em, e.id);
            }
        }
        more = ky_view_next(&it);
    }

    /* 2) Advance every alive particle. Gravity and drag are captured at
     *    emit time, so a particle keeps its trajectory after the owning
     *    entity is despawned (it will still expire via lifetime). */
    for (int i = 0; i < KY_PARTICLE_MAX; i++) {
        KyParticle *p = &g_pool[i];
        if (!p->alive) continue;

        p->age += dt;
        if (p->age * p->inv_life >= 1.0f) {
            pool_kill(i);
            continue;
        }

        p->vx += p->gx * dt;
        p->vy += p->gy * dt;
        if (p->drag != 0.0f) {
            float k = p->drag * dt;
            p->vx *= (1.0f - k);
            p->vy *= (1.0f - k);
        }
        p->px += p->vx * dt;
        p->py += p->vy * dt;
    }
}

/* ------------------------------------------------------------------ */
/* Component ctor/dtor                                                 */
/* ------------------------------------------------------------------ */

/* The component instance does not know its owning entity, so the dtor
 * cannot reclaim that entity's particles directly. Instead we tag each
 * particle with owner == entity id and let the *system* reap dead owners:
 * before stepping, scan the pool for particles whose owner no longer
 * exists in the world and kill them. This keeps the ECS dtor signature
 * intact while honoring R5-AC1. */

static void emitter_ctor(void *c) {
    *(kyEmitter *)c = ky_emitter_new();
}

static void emitter_dtor(void *c) {
    KY_UNUSED(c);
}

/* Run at the start of each step_particles call: reclaim particles whose
 * owner entity has since been despawned. O(pool) worst case, acceptable
 * at 16384. */
static void reap_dead_owners(kyWorld *w) {
    /* An owner entity is despawned when its id is in w->free_ids.
     * O(pool x free_ids); free_ids is small in practice. */
    for (int i = 0; i < KY_PARTICLE_MAX; i++) {
        if (!g_pool[i].alive) continue;
        uint32_t owner = g_pool[i].owner;
        int gone = 0;
        for (size_t f = 0; f < w->free_ids.len; f++) {
            uint32_t fid = *(uint32_t *)ky_array_get(&w->free_ids, f);
            if (fid == owner) { gone = 1; break; }
        }
        if (gone) pool_kill(i);
    }
}

/* ------------------------------------------------------------------ */
/* System + registration                                               */
/* ------------------------------------------------------------------ */

static void particle_update(kyWorld *w, float dt, void *user) {
    KY_UNUSED(user);
    reap_dead_owners(w);
    step_particles(w, dt);
}

static int system_exists(const kyWorld *w, const char *name) {
    for (size_t i = 0; i < w->systems.len; i++) {
        const kySystem *s = (const kySystem *)ky_array_get(&w->systems, i);
        if (s && s->name && strcmp(s->name, name) == 0) return 1;
    }
    return 0;
}

int ky_particle2d_register(kyWorld *w) {
    if (!w) return -1;

    if (!component_registered(w, "emitter")) {
        kyComponentType ct;
        memset(&ct, 0, sizeof(ct));
        ct.name = "emitter";
        ct.size = sizeof(kyEmitter);
        ct.ctor = emitter_ctor;
        ct.dtor = emitter_dtor;
        uint32_t id = ky_world_register_component(w, &ct);
        if (id == UINT32_MAX) return -2;
    }

    if (!system_exists(w, "particle-update")) {
        kySystem sys;
        memset(&sys, 0, sizeof(sys));
        sys.name   = "particle-update";
        sys.order  = 1; /* after physics/animator (order 0) */
        sys.update = particle_update;
        sys.user   = NULL;
        ky_world_register_system(w, &sys);
    }

    g_registered = 1;
    return 0;
}

int ky_particle2d_registered(void) { return g_registered; }
int ky_particle2d_alive_count(void) { return g_alive_count; }

/* ------------------------------------------------------------------ */
/* Render pass                                                         */
/* ------------------------------------------------------------------ */

/* Particle vertex layout matches 2d.c's ky2dVertex:
 *   float x, y, u, v; uint8_t r, g, b, a;   (16 bytes) */
typedef struct KyParticleVertex {
    float x, y, u, v;
    uint8_t r, g, b, a;
} KyParticleVertex;

static KyParticleVertex g_pverts[KY_PARTICLE_MAX * 4];
static uint16_t         g_pidx[KY_PARTICLE_MAX * 6];

/* V1: a single active texture shared by all particles. Default NULL ->
 * the pass falls back to the built-in white texture. The host calls
 * ky_particle2d_set_texture() to override (e.g. the particle atlas). */
static kyTexture *g_p_active_tex = NULL;

void ky_particle2d_set_texture(kyTexture *tex) {
    g_p_active_tex = tex;
}

/* Per-device lazy GPU resources (same pattern as 2d.c's g_cache). */
typedef struct ParticleCache {
    kyRenderDevice *rd;
    kyShader   *shader;
    kyPipeline *pipeline;
    kyBuffer   *vbo;
    kyBuffer   *ibo;
    kyTexture  *white;
    int        uvp_loc;
} ParticleCache;

static ParticleCache g_pc;

static const char *P_VS =
    "#version 300 es\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_color;\n"
    "uniform mat4 u_vp;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main(){ v_uv = a_uv; v_color = a_color;\n"
    "  gl_Position = u_vp * vec4(a_pos, 0.0, 1.0); }\n";

static const char *P_FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main(){ frag = texture(u_tex, clamp(v_uv, 0.0, 1.0)) * v_color; }\n";

static void pc_destroy_with_rd(kyRenderDevice *rd) {
    if (!rd) return;
    if (g_pc.white)    ky_rd_destroy_texture(rd, g_pc.white);
    if (g_pc.vbo)      ky_rd_destroy_buffer(rd, g_pc.vbo);
    if (g_pc.ibo)      ky_rd_destroy_buffer(rd, g_pc.ibo);
    if (g_pc.pipeline) ky_rd_destroy_pipeline(rd, g_pc.pipeline);
    if (g_pc.shader)   ky_rd_destroy_shader(rd, g_pc.shader);
    memset(&g_pc, 0, sizeof(g_pc));
}

static int pc_init(kyRenderDevice *rd) {
    if (g_pc.rd == rd && g_pc.pipeline && g_pc.vbo && g_pc.ibo) return 0;
    if (g_pc.rd != rd && g_pc.rd) pc_destroy_with_rd(g_pc.rd);
    memset(&g_pc, 0, sizeof(g_pc));
    g_pc.rd = rd;

    kyShaderSource src;
    memset(&src, 0, sizeof(src));
    src.vs = P_VS;
    src.fs = P_FS;
    src.entry = "main";
    g_pc.shader = ky_rd_create_shader(rd, &src);
    if (!g_pc.shader) return -1;

    int uvp = ky_rd_shader_uniform(rd, g_pc.shader, "u_vp");
    g_pc.uvp_loc = uvp;

    kyVertexAttrib attribs[3];
    memset(attribs, 0, sizeof(attribs));
    attribs[0].location = 0;
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
    desc.shader = g_pc.shader;
    desc.layout.attribs = attribs;
    desc.layout.count = 3;
    desc.layout.stride = sizeof(KyParticleVertex);
    desc.topology = KY_TRIANGLES;
    desc.depth_test = 0;
    desc.depth_write = 0;
    desc.cull_mode = 0;
    desc.blend.on = 1;
    kyVec4 one = {1, 1, 1, 1};
    desc.blend.color = one;
    g_pc.pipeline = ky_rd_create_pipeline(rd, &desc);
    if (!g_pc.pipeline) {
        ky_rd_destroy_shader(rd, g_pc.shader);
        g_pc.shader = NULL;
        return -1;
    }

    g_pc.vbo = ky_rd_create_buffer(rd, sizeof(g_pverts), NULL, 1);
    g_pc.ibo = ky_rd_create_buffer(rd, sizeof(g_pidx), NULL, 1);
    if (!g_pc.vbo || !g_pc.ibo) {
        if (g_pc.ibo)  ky_rd_destroy_buffer(rd, g_pc.ibo);
        if (g_pc.vbo)  ky_rd_destroy_buffer(rd, g_pc.vbo);
        ky_rd_destroy_pipeline(rd, g_pc.pipeline);
        ky_rd_destroy_shader(rd, g_pc.shader);
        memset(&g_pc, 0, sizeof(g_pc));
        g_pc.rd = rd;
        return -1;
    }

    static const uint8_t white_px[4] = {255, 255, 255, 255};
    g_pc.white = ky_rd_create_texture_2d(rd, 1, 1, 4, white_px);
    if (!g_pc.white) {
        ky_rd_destroy_buffer(rd, g_pc.ibo);
        ky_rd_destroy_buffer(rd, g_pc.vbo);
        ky_rd_destroy_pipeline(rd, g_pc.pipeline);
        ky_rd_destroy_shader(rd, g_pc.shader);
        memset(&g_pc, 0, sizeof(g_pc));
        return -1;
    }
    return 0;
}

static inline void lerp_u8(uint8_t from, uint8_t to, float t, uint8_t *out) {
    float f = (float)from + ((float)to - (float)from) * t;
    if (f < 0.0f) f = 0.0f;
    if (f > 255.0f) f = 255.0f;
    *out = (uint8_t)(f + 0.5f);
}

int ky_particle2d_render_pass(kyRenderDevice *rd, void *cl,
                              const kyCamera2D *cam, const kyTransform *cam_tr) {
    if (!rd || !cl) return -1;
    if (g_alive_count == 0) return 0;
    if (pc_init(rd) != 0) return -2;

    kyMat4 vp = ky2d_camera_view_proj(cam, cam_tr);
    if (g_pc.uvp_loc >= 0)
        ky_cmd_set_uniform(cl, g_pc.uvp_loc, &vp, (int)sizeof(kyMat4));

    size_t n = 0;
    for (int i = 0; i < KY_PARTICLE_MAX && n < KY_PARTICLE_MAX; i++) {
        KyParticle *p = &g_pool[i];
        if (!p->alive) continue;

        float t = p->age * p->inv_life;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        float size = p->size0 + (p->size1 - p->size0) * t;
        float hx = size * 0.5f, hy = size * 0.5f;

        uint8_t cr, cg, cb, ca;
        lerp_u8(p->fr, p->tr, t, &cr);
        lerp_u8(p->fg, p->tg, t, &cg);
        lerp_u8(p->fb, p->tb, t, &cb);
        lerp_u8(p->fa, p->ta, t, &ca);

        KyParticleVertex *v = &g_pverts[n * 4];
        v[0].x = p->px - hx; v[0].y = p->py + hy; v[0].u = 0.0f; v[0].v = 1.0f;
        v[1].x = p->px + hx; v[1].y = p->py + hy; v[1].u = 1.0f; v[1].v = 1.0f;
        v[2].x = p->px + hx; v[2].y = p->py - hy; v[2].u = 1.0f; v[2].v = 0.0f;
        v[3].x = p->px - hx; v[3].y = p->py - hy; v[3].u = 0.0f; v[3].v = 0.0f;
        for (int c = 0; c < 4; c++) {
            v[c].r = cr; v[c].g = cg; v[c].b = cb; v[c].a = ca;
        }

        uint16_t base = (uint16_t)(n * 4);
        uint16_t *ix = &g_pidx[n * 6];
        ix[0] = base; ix[1] = (uint16_t)(base+1); ix[2] = (uint16_t)(base+2);
        ix[3] = base; ix[4] = (uint16_t)(base+2); ix[5] = (uint16_t)(base+3);
        n++;
    }

    if (n == 0) return 0;

    /* V1 texture policy: the pass uses one texture for all particles.
     * The host sets the active texture via ky_particle2d_set_texture()
     * (default: built-in white). Per-emitter texture batching is out of
     * scope for this slice. */
    kyTexture *tex = g_p_active_tex ? g_p_active_tex : g_pc.white;

    int r = ky_rd_update_buffer(rd, g_pc.vbo, 0, n * 4 * sizeof(KyParticleVertex), g_pverts);
    if (r != 0) return -3;
    r = ky_rd_update_buffer(rd, g_pc.ibo, 0, n * 6 * sizeof(uint16_t), g_pidx);
    if (r != 0) return -3;

    ky_cmd_set_pipeline(cl, g_pc.pipeline);
    ky_cmd_set_vertex_buffer(cl, g_pc.vbo, (uint32_t)sizeof(KyParticleVertex));
    ky_cmd_set_index_buffer(cl, g_pc.ibo, 2);
    ky_cmd_set_texture(cl, 0, tex);
    ky_cmd_draw_indexed(cl, (uint32_t)(n * 6), 1);
    return 0;
}
