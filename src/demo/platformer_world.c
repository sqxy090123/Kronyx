#include "platformer_world.h"
#include "kronyx/log.h"
#include <string.h>
#include <math.h>

/* Role centre while resting: ground top + role radius. */
static float rest_height(void) {
    return PLATFORMER_GROUND_TOP + PLATFORMER_ROLE_RADIUS;
}

/* Load the demo hero texture (8×8 RGBA8) from assets/hero_8x8.bin and attach
 * to the role sprite. On any failure the sprite stays white (sp->texture ==
 * NULL) so the demo still runs without the asset on disk. */
static void demo_load_texture(PlatformerWorld *out) {
    void *buf = NULL;
    size_t len = 0;
    kyFileCode rc = ky_file_read("assets/hero_8x8.bin", &buf, &len);
    if (rc != KY_FILE_OK) {
        if (buf) ky_free_read(buf);
        if (len) out->role_tex = NULL;
        return; /* keep sp->texture == NULL (white) */
    }

    /* 8*8*4 bytes expected; accept any size that's a valid w*h*4 with w==h. */
    int w = 8, h = 8, ch = 4;
    if (len != (size_t)(w * h * ch)) {
        ky_free_read(buf);
        out->role_tex = NULL;
        return;
    }

    kyTexture *t = ky2d_make_texture(out->rd, w, h, ch, buf);
    ky_free_read(buf);
    if (!t) { out->role_tex = NULL; return; }

    out->role_tex = t;
    kySprite *sp = (kySprite *)ky_world_get_component(out->world, out->role, out->tid_sprite);
    if (sp) sp->texture = t;
}

/* ------------------------------------------------------------------ */
/* setup                                                               */
/* ------------------------------------------------------------------ */

int platformer_setup(PlatformerWorld *out, kyRendererBackend rd_backend) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    out->grounded = 1;

    /* Allocator must outlive the world's component arrays: ky_array stores a
     * pointer to it, and ky_world_destroy -> ky_array_deinit -> ky_mem_free
     * dereferences it. A stack-local allocator here would dangle after return. */
    out->alloc = ky_default_allocator();

    out->world = ky_world_create(&out->alloc);
    if (!out->world) return -2;

    if (ky2d_register_components(out->world) != 0) {
        ky_world_destroy(out->world);
        return -3;
    }

    out->rd = ky_rd_create(rd_backend, NULL);
    if (!out->rd) {
        ky_world_destroy(out->world);
        return -4;
    }

    int seen_transform = 0, seen_sprite = 0, seen_camera = 0;
    for (size_t i = 0; i < out->world->component_types.len; i++) {
        const kyComponentType *ct =
            (const kyComponentType *)ky_array_get(&out->world->component_types, i);
        if (!ct || !ct->name) continue;
        if (strcmp(ct->name, "transform") == 0) { out->tid_transform = ct->type_id; seen_transform = 1; }
        if (strcmp(ct->name, "sprite")    == 0) { out->tid_sprite    = ct->type_id; seen_sprite    = 1; }
        if (strcmp(ct->name, "camera2d")  == 0) { out->tid_camera    = ct->type_id; seen_camera    = 1; }
    }
    if (!seen_transform || !seen_sprite || !seen_camera) {
        platformer_teardown(out);
        return -5;
    }

    /* Static ground collider lives in the physics world (available for future
     * raycasts). The role is driven by the platformer integrator in tick():
     * the built-in narrow phase resolves along the minimum-penetration axis,
     * so it cannot stop a body falling straight onto a flat top whose x/z
     * penetration is ~zero. */
    out->phys = ky_physics_create((kyVec3){0.0f, PLATFORMER_GRAVITY_Y, 0.0f});
    if (!out->phys) { platformer_teardown(out); return -6; }

    kyCollider ground_col;
    memset(&ground_col, 0, sizeof(ground_col));
    ground_col.shape = KY_SHAPE_BOX;
    ground_col.u.box.center = (kyVec3){0, -1.0f, 0};
    ground_col.u.box.half_extents = (kyVec3){20.0f, 1.0f, 1.0f};
    uint32_t ground_cid = ky_physics_add_collider(out->phys, &ground_col);
    if (ground_cid) {
        kyRigidBody gbody;
        memset(&gbody, 0, sizeof(gbody));
        gbody.position = (kyVec3){0, -1.0f, 0};
        gbody.inv_mass = 0.0f;
        gbody.collider_id = ground_cid;
        out->ground_body = ky_physics_add_body(out->phys, &gbody);
    }

    /* Role ECS entity: Transform (for the 2D renderer) + Sprite. */
    out->role = ky_world_spawn(out->world);
    kyTransform *tr = (kyTransform *)ky_world_add_component(out->world, out->role, out->tid_transform);
    kySprite    *sp = (kySprite    *)ky_world_add_component(out->world, out->role, out->tid_sprite);
    if (!tr || !sp) { platformer_teardown(out); return -9; }
    tr->pos = (kyVec2){0, PLATFORMER_START_Y};
    sp->color = (kyVec4){1.0f, 0.35f, 0.35f, 1.0f};
    sp->size  = (kyVec2){1.0f, 1.0f};

    /* Attach the demo hero texture if the asset is on disk; otherwise the
     * sprite stays its solid red colour (white-texture fallback path). */
    demo_load_texture(out);

    /* Camera entity. */
    out->camera = ky_world_spawn(out->world);
    kyCamera2D *cam = (kyCamera2D *)ky_world_add_component(out->world, out->camera, out->tid_camera);
    if (!cam) { platformer_teardown(out); return -12; }
    cam->active      = 1;
    cam->viewport    = (kyVec2){10.0f, 7.5f};
    cam->zoom        = 1.0f;
    cam->pos         = (kyVec2){0, (float)(PLATFORMER_START_Y + PLATFORMER_CAM_OFFSET_Y)};
    cam->clear_color = (kyVec4){0.08f, 0.08f, 0.12f, 1.0f};

    return 0;
}

/* ------------------------------------------------------------------ */
/* readback                                                            */
/* ------------------------------------------------------------------ */

kyVec2 platformer_role_pos(const PlatformerWorld *pw) {
    if (!pw || !pw->world) return (kyVec2){0, 0};
    const kyTransform *tr =
        (const kyTransform *)ky_world_get_component(pw->world, pw->role, pw->tid_transform);
    return tr ? tr->pos : (kyVec2){0, 0};
}

void platformer_role_velocity(const PlatformerWorld *pw, float *vx, float *vy) {
    if (!pw) return;
    if (vx) *vx = pw->vx;
    if (vy) *vy = pw->vy;
}

/* ------------------------------------------------------------------ */
/* tick                                                                */
/* ------------------------------------------------------------------ */

int platformer_tick(PlatformerWorld *pw, float dt, float vx_target, int jump) {
    if (!pw || !pw->world || !pw->rd) return -1;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;

    float px = platformer_role_pos(pw).x;
    float py = platformer_role_pos(pw).y;
    float vx = ky_math_clampf(vx_target, -PLATFORMER_MAX_SPEED, PLATFORMER_MAX_SPEED);
    float vy = pw->vy;

    /* Jump: single-shot, only valid while grounded. */
    if (jump && pw->grounded) {
        vy = PLATFORMER_JUMP_VY;
        pw->grounded = 0;
    }

    /* Integrate: horizontal constant velocity, vertical under gravity. */
    px += vx * dt;
    vy += PLATFORMER_GRAVITY_Y * dt;
    py += vy * dt;

    /* Ground contact: the role's bottom (centre - radius) rests on the top. */
    float rest = rest_height();
    if (py <= rest && vy < 0.0f) {
        py = rest;
        vy = 0.0f;
        pw->grounded = 1;
    }

    pw->vx = vx;
    pw->vy = vy;

    /* Sync the role's Transform to the integrator state. */
    kyTransform *tr = (kyTransform *)ky_world_get_component(pw->world, pw->role, pw->tid_transform);
    if (tr) tr->pos = (kyVec2){px, py};

    /* Camera follows the role. */
    kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(pw->world, pw->camera, pw->tid_camera);
    if (cam) cam->pos = (kyVec2){px, py + PLATFORMER_CAM_OFFSET_Y};

    /* Render one frame. ky2d_render_world_auto submits the whole frame; an
     * extra ky_rd_present from here would double-issue on backends that do
     * not expect it. */
    (void)ky2d_render_world_auto(pw->rd, pw->world);

    return 0;
}

/* ------------------------------------------------------------------ */
/* teardown                                                            */
/* ------------------------------------------------------------------ */

void platformer_teardown(PlatformerWorld *pw) {
    if (!pw) return;
    if (pw->world) { ky_world_destroy(pw->world); pw->world = NULL; }
    if (pw->role_tex) { ky_rd_destroy_texture(pw->rd, pw->role_tex); pw->role_tex = NULL; }
    if (pw->phys)  { ky_physics_destroy(pw->phys); pw->phys = NULL; }
    if (pw->rd)    { ky_rd_destroy(pw->rd); pw->rd = NULL; }
}
