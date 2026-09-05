#include "kronyx/render.h"
#include "kronyx/math.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Render Demo (console backend) ===\n");

    kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
    if (!rd) { printf("Failed to create renderer\n"); return 1; }
    printf("Backend: %s\n", ky_rd_backend_name(rd));

    kyShaderSource src = { .vs = "passthrough", .fs = "color", .cs = NULL, .entry = "main" };
    kyShader *shader = ky_rd_create_shader(rd, &src);
    kyBuffer *vb = ky_rd_create_buffer(rd, 72, NULL, 0);
    kyTexture *tex = ky_rd_create_texture_2d(rd, 64, 64, 4, NULL);

    kyPipelineDesc desc = {0};
    desc.shader = shader;
    desc.topology = KY_TRIANGLES;
    desc.blend.on = 1;
    desc.blend.color = ky_vec4(0.5f, 0.8f, 1.0f, 1.0f);

    kyPipeline *pipe = ky_rd_create_pipeline(rd, &desc);

    kyVec4 clear = {0.1f, 0.1f, 0.1f, 1.0f};
    for (int i = 0; i < 3; i++) {
        ky_rd_clear(rd, clear, 1.0f);
        void *cl = ky_rd_begin(rd);
        ky_cmd_set_pipeline(cl, pipe);
        ky_cmd_set_vertex_buffer(cl, vb, 6 * sizeof(float));
        ky_cmd_set_texture(cl, 0, tex);
        ky_cmd_draw_indexed(cl, 3, 1);
        ky_rd_submit(rd, cl);
        ky_rd_present(rd);
    }

    ky_rd_destroy_pipeline(rd, pipe);
    ky_rd_destroy_texture(rd, tex);
    ky_rd_destroy_buffer(rd, vb);
    ky_rd_destroy_shader(rd, shader);
    ky_rd_destroy(rd);
    printf("Demo complete.\n");
    return 0;
}
