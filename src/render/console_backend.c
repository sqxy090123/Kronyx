#include "render_backend.h"
#include "kronyx/math.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct kyConsoleDevice {
    int width;
    int height;
    int frame_count;
} kyConsoleDevice;

typedef struct kyConsoleBuffer {
    size_t size;
    void *data;
} kyConsoleBuffer;

typedef struct kyConsoleCmdList {
    kyRenderDevice *rd;
    int pipeline_id;
    int vbo_stride;
    int depth_test;
    int depth_write;
    int cull_mode;
    int blend_on;
    int texture_count;
} kyConsoleCmdList;

/* ky_rd_begin aliases any command list through kyCmdHeader, which requires
 * the render device pointer to sit at offset 0. */
KY_STATIC_ASSERT(offsetof(kyConsoleCmdList, rd) == 0,
                 "kyConsoleCmdList.rd must be the first member");

static void *console_create(void *platform_win) {
    KY_UNUSED(platform_win);
    kyConsoleDevice *d = (kyConsoleDevice *)malloc(sizeof(kyConsoleDevice));
    if (!d) return NULL;
    d->width = 800;
    d->height = 600;
    d->frame_count = 0;
    return d;
}

static void console_destroy(void *impl) {
    free(impl);
}

static const char *console_name(void) {
    return "console";
}

static void *console_create_shader(void *impl, const kyShaderSource *src) {
    KY_UNUSED(impl);
    KY_UNUSED(src);
    return malloc(1);
}

static void console_destroy_shader(void *impl, void *shader) {
    KY_UNUSED(impl);
    free(shader);
}

static int console_shader_uniform(void *impl, void *shader, const char *name) {
    KY_UNUSED(impl);
    KY_UNUSED(shader);
    KY_UNUSED(name);
    return -1;
}

static void *console_create_buffer(void *impl, size_t size, const void *data, int dynamic) {
    KY_UNUSED(impl);
    KY_UNUSED(dynamic);
    kyConsoleBuffer *b = (kyConsoleBuffer *)malloc(sizeof(kyConsoleBuffer));
    if (!b) return NULL;
    b->size = size;
    b->data = malloc(size > 0 ? size : 1);
    if (!b->data) {
        free(b);
        return NULL;
    }
    if (data && size > 0) memcpy(b->data, data, size);
    return b;
}

static void console_destroy_buffer(void *impl, void *buf) {
    KY_UNUSED(impl);
    kyConsoleBuffer *b = (kyConsoleBuffer *)buf;
    if (!b) return;
    free(b->data);
    free(b);
}

static int console_update_buffer(void *impl, void *buf, size_t offset, size_t size, const void *data) {
    KY_UNUSED(impl);
    kyConsoleBuffer *b = (kyConsoleBuffer *)buf;
    if (!b || !data || size == 0) return -1;
    if (offset > b->size || size > b->size - offset) return -1;
    memcpy((char *)b->data + offset, data, size);
    return 0;
}

static void *console_create_texture(void *impl, int w, int h, int ch, const void *px) {
    KY_UNUSED(impl);
    KY_UNUSED(w);
    KY_UNUSED(h);
    KY_UNUSED(ch);
    KY_UNUSED(px);
    return malloc(1);
}

static void console_destroy_texture(void *impl, void *tex) {
    KY_UNUSED(impl);
    free(tex);
}

static void *console_create_pipeline(void *impl, const kyPipelineDesc *desc) {
    KY_UNUSED(impl);
    KY_UNUSED(desc);
    int *pipe = (int *)malloc(sizeof(int) * 5);
    if (!pipe) return NULL;
    pipe[0] = desc->shader ? 1 : 0;
    pipe[1] = desc->depth_test;
    pipe[2] = desc->depth_write;
    pipe[3] = desc->cull_mode;
    pipe[4] = desc->blend.on;
    return pipe;
}

static void console_destroy_pipeline(void *impl, void *pipe) {
    KY_UNUSED(impl);
    free(pipe);
}

static void *console_begin(void *impl) {
    KY_UNUSED(impl);
    kyConsoleCmdList *cl = (kyConsoleCmdList *)calloc(1, sizeof(kyConsoleCmdList));
    return cl;
}

static void console_set_pipeline(void *cl, void *pipe) {
    if (!cl || !pipe) return;
    kyConsoleCmdList *cmd = (kyConsoleCmdList *)cl;
    int *p = (int *)pipe;
    cmd->pipeline_id = p[0];
    cmd->depth_test  = p[1];
    cmd->depth_write = p[2];
    cmd->cull_mode   = p[3];
    cmd->blend_on    = p[4];
}

static void console_set_vertex_buffer(void *cl, void *vb, uint32_t stride) {
    KY_UNUSED(vb);
    if (!cl) return;
    kyConsoleCmdList *cmd = (kyConsoleCmdList *)cl;
    cmd->vbo_stride = stride;
}

static void console_set_index_buffer(void *cl, void *ib, uint32_t index_size) {
    KY_UNUSED(cl);
    KY_UNUSED(ib);
    KY_UNUSED(index_size);
}

static void console_set_texture(void *cl, int slot, void *tex) {
    KY_UNUSED(slot);
    KY_UNUSED(tex);
    if (!cl) return;
    kyConsoleCmdList *cmd = (kyConsoleCmdList *)cl;
    cmd->texture_count++;
}

static void console_set_uniform(void *cl, int loc, const void *data, int bytes) {
    KY_UNUSED(cl);
    KY_UNUSED(loc);
    KY_UNUSED(data);
    KY_UNUSED(bytes);
}

static void console_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    KY_UNUSED(instances);
    if (!cl) return;
    kyConsoleCmdList *cmd = (kyConsoleCmdList *)cl;
    printf("DREW %u verts (blend=%s depth=%s cull=%s tex=%d)\n",
           count, cmd->blend_on ? "on" : "off",
           cmd->depth_test ? "on" : "off",
           cmd->cull_mode ? "on" : "off",
           cmd->texture_count);
}

static void console_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    KY_UNUSED(instances);
    if (!cl) return;
    kyConsoleCmdList *cmd = (kyConsoleCmdList *)cl;
    printf("DREW ARRAY %u verts (blend=%s)\n",
           vertex_count, cmd->blend_on ? "on" : "off");
}

static void console_submit(void *impl, void *cl) {
    KY_UNUSED(impl);
    free(cl);
}

static void console_present(void *impl) {
    kyConsoleDevice *d = (kyConsoleDevice *)impl;
    d->frame_count++;
    printf("=== FRAME %d (%dx%d) ===\n", d->frame_count, d->width, d->height);
}

static void console_clear(void *impl, kyVec4 color, float depth) {
    KY_UNUSED(impl);
    KY_UNUSED(depth);
    printf("CLEAR(%.2f,%.2f,%.2f,%.2f)\n", color.x, color.y, color.z, color.w);
}

const kyRenderBackend ky_backend_console = {
    .create           = console_create,
    .destroy          = console_destroy,
    .name             = console_name,
    .create_shader    = console_create_shader,
    .destroy_shader   = console_destroy_shader,
    .shader_uniform   = console_shader_uniform,
    .create_buffer    = console_create_buffer,
    .destroy_buffer   = console_destroy_buffer,
    .update_buffer    = console_update_buffer,
    .create_texture   = console_create_texture,
    .destroy_texture  = console_destroy_texture,
    .create_pipeline  = console_create_pipeline,
    .destroy_pipeline = console_destroy_pipeline,
    .begin            = console_begin,
    .set_pipeline     = console_set_pipeline,
    .set_vertex_buffer = console_set_vertex_buffer,
    .set_index_buffer = console_set_index_buffer,
    .set_texture      = console_set_texture,
    .set_uniform      = console_set_uniform,
    .draw_indexed     = console_draw_indexed,
    .draw_array       = console_draw_array,
    .submit           = console_submit,
    .present          = console_present,
    .clear            = console_clear,
};
