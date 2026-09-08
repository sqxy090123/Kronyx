/* G2 end-to-end resource flow:
 *   file read -> pixelbuffer resource -> texture -> sprite -> one frame -> release
 * Covers the ky_file_* layer (happy + not-found + bad size), resmgr idempotent
 * registration, reference counting, and the 2D texture pipeline integration.
 */

#include "kronyx/file.h"
#include "kronyx/resource.h"
#include "kronyx/2d.h"
#include "kronyx/render.h"
#include "kronyx/math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int assertions = 0;
static int failures = 0;
#define CHECK(cond, msg) do { \
    assertions++; \
    if (!(cond)) { failures++; printf("FAIL %s  (line %d)\n", msg, __LINE__); } \
} while (0)

/* ------------------------------------------------------------------ */
/* file layer                                                          */
/* ------------------------------------------------------------------ */

static void test_file_read_happy(void) {
    const char *tmp = "test_file_flow.bin";
    uint8_t bytes[] = {1, 2, 3, 4, 5};
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    fwrite(bytes, 1, sizeof(bytes), f);
    fclose(f);

    void *buf = NULL; size_t len = 0;
    kyFileCode rc = ky_file_read(tmp, &buf, &len);
    CHECK(rc == KY_FILE_OK, "file_read happy: OK");
    CHECK(len == sizeof(bytes), "file_read happy: len matches");
    CHECK(buf != NULL && memcmp(buf, bytes, sizeof(bytes)) == 0, "file_read happy: bytes match");
    if (buf) ky_free_read(buf);

    FILE *del = fopen(tmp, "wb"); if (del) fclose(del); /* unlink via stdio-free rename */
    /* remove best-effort; ignore if absent */
    remove(tmp);
}

static void test_file_read_not_found(void) {
    void *buf = NULL; size_t len = 99;
    kyFileCode rc = ky_file_read("/does/not/exist_kronyx.bin", &buf, &len);
    CHECK(rc == KY_FILE_ERR_NOT_FOUND, "file_read: missing path -> NOT_FOUND");
    CHECK(buf == NULL, "file_read: missing path leaves buf NULL");
}

static void test_file_read_empty_path(void) {
    void *buf = NULL; size_t len = 0;
    kyFileCode rc = ky_file_read("", &buf, &len);
    CHECK(rc < 0, "file_read: empty path -> error");
}

/* ------------------------------------------------------------------ */
/* resource manager: idempotent add, refcount, release                 */
/* ------------------------------------------------------------------ */

static uint8_t *px4( uint8_t fill) {
    uint8_t *p = malloc(4 * 4 * 4);
    for (size_t i = 0; i < 4 * 4 * 4; i++) p[i] = fill;
    return p;
}

static void test_resmgr_idempotent_and_release(void) {
    kyAllocator al = ky_default_allocator();
    kyResourceManager *m = ky_resmgr_create(&al);
    CHECK(m != NULL, "resmgr create");

    uint8_t *px = px4(244);
    kyResource *r1 = ky_resmgr_make_pixelbuffer(m, "hero.bin", px, 4*4*4, 4, 4, 4);
    CHECK(r1 != NULL, "make_pixelbuffer #1");

    /* duplicate path must return NULL (idempotent, do not double-own) */
    uint8_t *px2 = px4(1);
    kyResource *dup = ky_resmgr_make_pixelbuffer(m, "hero.bin", px2, 4*4*4, 4, 4, 4);
    CHECK(dup == NULL, "make_pixelbuffer dup -> NULL");
    CHECK(ky_resmgr_count(m) == 1, "count stays 1 after dup");
    free(px2);

    /* refcount: acquire -> 2, release -> 1, release -> 0 -> destroyed */
    kyResource *a2 = ky_resmgr_acquire(m, "hero.bin");
    CHECK(a2 == r1, "acquire returns registered resource");
    CHECK(a2->ref_count == 2, "refcount == 2 after acquire");

    ky_resmgr_release(m, a2);
    CHECK(r1->ref_count == 1, "refcount == 1 after one release");

    /* payload should still be reachable before dropping last ref */
    kyResource *peek = ky_resmgr_find(m, "hero.bin");
    CHECK(peek == r1, "find before final release still returns resource");

    ky_resmgr_release(m, r1);
    CHECK(ky_resmgr_find(m, "hero.bin") == NULL, "after last release resource gone");
    CHECK(ky_resmgr_count(m) == 0, "count == 0 after final release");

    free(px);
    ky_resmgr_destroy(m);
}

/* ------------------------------------------------------------------ */
/* 2D integration: pixelbuffer -> texture -> sprite -> render frame    */
/* ------------------------------------------------------------------ */

static void test_2d_texture_integration(void) {
    kyAllocator al = ky_default_allocator();
    kyWorld *w = ky_world_create(&al);
    CHECK(w != NULL, "world create");
    CHECK(ky2d_register_components(w) == 0, "register 2d components");

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    CHECK(rd != NULL, "console rd create");

    /* Locate component types. */
    uint32_t tid_tr = 0, tid_sp = 0, tid_cam = 0;
    int s_tr = 0, s_sp = 0, s_cam = 0;
    for (size_t i = 0; i < w->component_types.len; i++) {
        const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, i);
        if (!ct || !ct->name) continue;
        if (strcmp(ct->name, "transform") == 0) { tid_tr = ct->type_id;  s_tr = 1; }
        if (strcmp(ct->name, "sprite")    == 0) { tid_sp = ct->type_id;  s_sp = 1; }
        if (strcmp(ct->name, "camera2d")  == 0) { tid_cam = ct->type_id; s_cam = 1; }
    }
    CHECK(s_tr && s_sp && s_cam, "all 3 component types found");

    /* Role sprite with a real texture. */
    uint8_t *px = px4(210);
    kyTexture *tex = ky2d_make_texture(rd, 4, 4, 4, px);
    CHECK(tex != NULL, "make_texture from 4x4 RGBA");
    free(px);

    kyEntity role = ky_world_spawn(w);
    kyTransform *tr = (kyTransform *)ky_world_add_component(w, role, tid_tr);
    kySprite    *sp = (kySprite    *)ky_world_add_component(w, role, tid_sp);
    CHECK(tr && sp, "role has transform + sprite");
    tr->pos = (kyVec2){0, 0.5f};
    sp->size = (kyVec2){1, 1};
    sp->texture = tex;

    /* Camera. */
    kyEntity cam = ky_world_spawn(w);
    kyCamera2D *c = (kyCamera2D *)ky_world_add_component(w, cam, tid_cam);
    CHECK(c != NULL, "camera entity");
    c->active = 1; c->viewport = (kyVec2){10, 8}; c->zoom = 1.0f;
    c->pos = (kyVec2){0, 1}; c->clear_color = (kyVec4){0.1f, 0.1f, 0.2f, 1};

    /* Render one frame through the auto-picking path; expect at least the role. */
    int drawn = ky2d_render_world_auto(rd, w);
    CHECK(drawn >= 1, "render frame draws >=1 sprite");

    /* Invalid params must fail cleanly (uses a fresh valid buffer). */
    uint8_t *px2 = px4(9);
    CHECK(ky2d_make_texture(rd, 0, 4, 4, px2) == NULL, "w<=0 -> NULL");
    CHECK(ky2d_make_texture(rd, 4, 0, 4, px2) == NULL, "h<=0 -> NULL");
    CHECK(ky2d_make_texture(rd, 4, 4, 5, px2) == NULL, "channels=5 -> NULL");
    CHECK(ky2d_make_texture(rd, 4, 4, 4, NULL) == NULL, "NULL pixels -> NULL");
    free(px2);

    ky_rd_destroy_texture(rd, tex);
    ky_rd_destroy(rd);
    ky_world_destroy(w);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== G2 Resource Flow Test ===\n");
    test_file_read_happy();
    test_file_read_not_found();
    test_file_read_empty_path();
    test_resmgr_idempotent_and_release();
    test_2d_texture_integration();
    printf("%d assertions, %d failures\n", assertions, failures);
    return failures ? 1 : 0;
}
