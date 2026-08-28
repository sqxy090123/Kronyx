#include "kronyx/render.h"
#include "memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int assertions = 0;
static int failures = 0;

#define ASSERT(cond, msg) do { \
    assertions++; \
    if (!(cond)) { \
        failures++; \
        printf("FAIL: %s at line %d\n", msg, __LINE__); \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while(0)

int main(void) {
    printf("=== Render RHI Test ===\n");

    /* Test: null-safe backend query */
    kyRenderDevice *rd_null = NULL;
    ASSERT(strcmp(ky_rd_backend_name(rd_null), "unknown") == 0,
           "null device returns 'unknown' backend");

    /* Test: create and destroy with OpenGL backend */
    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_GL, NULL);
    ASSERT(rd != NULL, "create GL renderer succeeds");
    ASSERT(strcmp(ky_rd_backend_name(rd), "opengl3.3") == 0,
           "backend name is 'opengl3.3'");

    /* Test: create pipeline (no impl, just memory alloc) */
    kyShaderSource src = { .vs = "v", .fs = "f", .cs = NULL, .entry = "main" };
    kyShader *s = ky_rd_create_shader(rd, &src);
    ASSERT(s != NULL, "create shader (stub) succeeds");

    kyBuffer *vb = ky_rd_create_buffer(rd, 256, NULL, 0);
    ASSERT(vb != NULL, "create buffer (stub) succeeds");

    kyTexture *tex = ky_rd_create_texture_2d(rd, 64, 64, 4, NULL);
    ASSERT(tex != NULL, "create texture (stub) succeeds");

    kyVec4 col = {0.2f, 0.4f, 0.6f, 1.0f};
    kyPipelineDesc desc = {0};
    desc.shader = s;
    desc.topology = KY_TRIANGLES;
    desc.blend.on = 1;
    desc.blend.color = col;
    desc.layout = (kyVertexLayout){0};

    kyPipeline *p = ky_rd_create_pipeline(rd, &desc);
    ASSERT(p != NULL, "create pipeline (stub) succeeds");

    /* Test: command list operations are no-ops */
    void *cl = ky_rd_begin(rd);
    ASSERT(cl != NULL, "begin command list succeeds");
    ky_cmd_set_pipeline(cl, p);
    ky_cmd_set_vertex_buffer(cl, vb, sizeof(float) * 2);
    ky_cmd_set_index_buffer(cl, vb, 2);
    ky_cmd_set_texture(cl, 0, tex);
    ky_cmd_set_uniform(cl, 0, &col, (int)sizeof(kyVec4));
    ky_cmd_draw_indexed(cl, 3, 1);
    ky_cmd_draw_array(cl, 3, 1);
    ky_rd_clear(rd, col, 1.0f);
    ky_rd_submit(rd, cl);

    /* Test: destroy order */
    ky_rd_destroy_pipeline(rd, p);
    ky_rd_destroy_texture(rd, tex);
    ky_rd_destroy_buffer(rd, vb);
    ky_rd_destroy_shader(rd, s);
    ky_rd_destroy(rd);
    ASSERT(ky_rd_backend_name(NULL) != NULL, "backend name still valid after destroy");

    printf("\n=== %d tests ran, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
