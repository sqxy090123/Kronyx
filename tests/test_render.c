#include "kronyx/render.h"
#include "kronyx/math.h"
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

    ASSERT(strcmp(ky_rd_backend_name(NULL), "unknown") == 0,
           "null device returns 'unknown' backend");
    ASSERT(ky_rd_backend(NULL) == KY_RENDERER_NONE,
           "null device backend enum is NONE");

    /* Console backend */
    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    ASSERT(rd != NULL, "create console renderer succeeds");
    ASSERT(strcmp(ky_rd_backend_name(rd), "console") == 0,
           "backend name is 'console'");
    ASSERT(ky_rd_backend(rd) == KY_RENDERER_CONSOLE,
           "backend enum is CONSOLE");

    /* GL backend (stub) */
    kyRenderDevice *rd_gl = ky_rd_create(KY_RENDERER_GL, NULL);
    ASSERT(rd_gl != NULL, "create GL renderer succeeds");
    ASSERT(strcmp(ky_rd_backend_name(rd_gl), "opengl-es3") == 0,
           "GL backend name is 'opengl-es3'");
    ASSERT(ky_rd_backend(rd_gl) == KY_RENDERER_GL,
           "GL backend enum is GL");

    /* Shader / buffer / texture / pipeline on console */
    kyShaderSource src = { .vs = "v", .fs = "f", .cs = NULL, .entry = "main" };
    kyShader *s = ky_rd_create_shader(rd, &src);
    ASSERT(s != NULL, "create shader succeeds");

    kyBuffer *vb = ky_rd_create_buffer(rd, 256, NULL, 0);
    ASSERT(vb != NULL, "create buffer succeeds");

    kyTexture *tex = ky_rd_create_texture_2d(rd, 64, 64, 4, NULL);
    ASSERT(tex != NULL, "create texture succeeds");

    kyVec4 col = {0.2f, 0.4f, 0.6f, 1.0f};
    kyPipelineDesc desc = {0};
    desc.shader = s;
    desc.topology = KY_TRIANGLES;
    desc.blend.on = 1;
    desc.blend.color = col;

    kyPipeline *p = ky_rd_create_pipeline(rd, &desc);
    ASSERT(p != NULL, "create pipeline succeeds");

    /* Command list */
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

    /* Cancel path: a begun command list that is aborted before submit must
     * release its backend allocation (the known leak this closes).  Run it
     * before destroying the pipeline so cl2's referenced resources are still
     * valid. */
    void *cl2 = ky_rd_begin(rd);
    ASSERT(cl2 != NULL, "cancel: begin command list for abort");
    ky_cmd_set_pipeline(cl2, p);
    ky_rd_cancel(rd, cl2); /* cl2 now owned/freed by the backend */

    /* Cancel with NULL is a safe no-op. */
    ky_rd_cancel(rd, NULL);
    ky_rd_cancel(NULL, NULL);

    /* Destroy */
    ky_rd_destroy_pipeline(rd, p);
    ky_rd_destroy_texture(rd, tex);
    ky_rd_destroy_buffer(rd, vb);
    ky_rd_destroy_shader(rd, s);

    ky_rd_destroy(rd);
    ky_rd_destroy(rd_gl);

    /* Vulkan backend (stub or real lavapipe) — resource lifecycle must not
     * crash and the backend name/enum must round-trip. */
    kyRenderDevice *rd_vk = ky_rd_create(KY_RENDERER_VULKAN, NULL);
    ASSERT(rd_vk != NULL, "create Vulkan renderer succeeds");
    if (rd_vk) {
        ASSERT(strcmp(ky_rd_backend_name(rd_vk), "vulkan") == 0,
               "Vulkan backend name is 'vulkan'");
        ASSERT(ky_rd_backend(rd_vk) == KY_RENDERER_VULKAN,
               "Vulkan backend enum is VULKAN");
        kyShaderSource vsrc = { .vs = "v", .fs = "f", .cs = NULL, .entry = "main" };
        kyShader *sh = ky_rd_create_shader(rd_vk, &vsrc);
        ASSERT(sh != NULL, "Vulkan shader create");
        kyBuffer *vbuf = ky_rd_create_buffer(rd_vk, 64, NULL, 0);
        ASSERT(vbuf != NULL, "Vulkan buffer create");
        kyTexture *vtex = ky_rd_create_texture_2d(rd_vk, 16, 16, 4, NULL);
        ASSERT(vtex != NULL, "Vulkan texture create");
        kyVec4 vcol = {0.1f, 0.5f, 0.9f, 1.0f};
        kyPipelineDesc vdesc = {0};
        vdesc.shader = sh;
        vdesc.topology = KY_TRIANGLES;
        vdesc.blend.on = 1;
        vdesc.blend.color = vcol;
        kyPipeline *vpipe = ky_rd_create_pipeline(rd_vk, &vdesc);
        ASSERT(vpipe != NULL, "Vulkan pipeline create");
        void *vcl = ky_rd_begin(rd_vk);
        ASSERT(vcl != NULL, "Vulkan command list begin");
        ky_cmd_set_pipeline(vcl, vpipe);
        ky_cmd_set_vertex_buffer(vcl, vbuf, 8);
        ky_cmd_draw_array(vcl, 3, 1);
        ky_rd_clear(rd_vk, vcol, 1.0f);
        ky_rd_submit(rd_vk, vcl);
        ky_rd_present(rd_vk);
        void *vcl2 = ky_rd_begin(rd_vk);
        ky_cmd_set_pipeline(vcl2, vpipe);
        ky_rd_cancel(rd_vk, vcl2);
        ky_rd_destroy_pipeline(rd_vk, vpipe);
        ky_rd_destroy_texture(rd_vk, vtex);
        ky_rd_destroy_buffer(rd_vk, vbuf);
        ky_rd_destroy_shader(rd_vk, sh);
        ky_rd_destroy(rd_vk);
    }

#ifdef KY_HAS_EGL
#include <GLES3/gl3.h>

    /* GL backend real render: fullscreen triangle + pixel readback */
    {
        kyRenderDevice *rd = ky_rd_create(KY_RENDERER_GL, NULL);
        ASSERT(rd != NULL, "GL device with real EGL context");
        if (rd) {
            const char *vs =
                "#version 300 es\n"
                "layout(location=0) in vec2 aPos;\n"
                "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }";
            const char *fs =
                "#version 300 es\n"
                "precision mediump float;\n"
                "out vec4 fragColor;\n"
                "void main() { fragColor = vec4(0.0, 1.0, 0.0, 1.0); }";
            kyShaderSource src = { .vs = vs, .fs = fs, .cs = NULL, .entry = "main" };
            kyShader *sh = ky_rd_create_shader(rd, &src);
            ASSERT(sh != NULL, "GL shader compile+link");

            kyVertexAttrib attrib = { .location = 0, .offset = 0, .size = 2, .normalized = 0 };
            kyPipelineDesc desc = {0};
            desc.shader = sh;
            desc.topology = KY_TRIANGLES;
            desc.layout.attribs = &attrib;
            desc.layout.count = 1;
            desc.layout.stride = 8;
            kyPipeline *pipe = ky_rd_create_pipeline(rd, &desc);
            ASSERT(pipe != NULL, "GL pipeline create");

            float tri[6] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
            kyBuffer *vb = ky_rd_create_buffer(rd, sizeof(tri), tri, 0);
            ASSERT(vb != NULL, "GL vertex buffer");

            kyVec4 clear = {0.0f, 0.0f, 0.0f, 1.0f};
            ky_rd_clear(rd, clear, 1.0f);
            void *cl = ky_rd_begin(rd);
            ASSERT(cl != NULL, "GL command list");
            ky_cmd_set_pipeline(cl, pipe);
            ky_cmd_set_vertex_buffer(cl, vb, 8);
            ky_cmd_draw_array(cl, 3, 1);
            ky_rd_submit(rd, cl);
            ky_rd_present(rd);

            unsigned char px[4];
            glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            ASSERT(px[0] == 0 && px[1] == 255 && px[2] == 0,
                   "GL pixel readback: center is green");

            ky_rd_destroy_buffer(rd, vb);
            ky_rd_destroy_pipeline(rd, pipe);
            ky_rd_destroy_shader(rd, sh);
            ky_rd_destroy(rd);
        }
    }
#else
    printf("SKIP: GL real render (built without EGL)\n");
#endif


    printf("\n=== %d tests ran, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
