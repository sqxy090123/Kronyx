#include "kronyx/render.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

kyRenderDevice *ky_rd_create(kyRendererBackend backend, void *platform_win) {
    KY_UNUSED(platform_win);
    if (backend != KY_RENDERER_GL) return NULL;
    kyRenderDevice *rd = (kyRenderDevice *)malloc(sizeof(kyRenderDevice));
    if (!rd) return NULL;
    rd->backend = KY_RENDERER_GL;
    rd->impl = NULL;
    rd->alloc = ky_default_allocator();
    return rd;
}

void ky_rd_destroy(kyRenderDevice *rd) {
    if (rd) {
        free(rd->impl);
        free(rd);
    }
}

const char *ky_rd_backend_name(const kyRenderDevice *rd) {
    if (!rd) return "unknown";
    switch (rd->backend) {
        case KY_RENDERER_GL: return "opengl3.3";
        case KY_RENDERER_VULKAN: return "vulkan1.2";
        default: return "none";
    }
}

kyShader *ky_rd_create_shader(kyRenderDevice *rd, const kyShaderSource *src) {
    KY_UNUSED(rd);
    KY_UNUSED(src);
    return (kyShader *)malloc(1);
}

void ky_rd_destroy_shader(kyRenderDevice *rd, kyShader *s) {
    KY_UNUSED(rd);
    if (s) free(s);
}

kyBuffer *ky_rd_create_buffer(kyRenderDevice *rd, size_t size, const void *data, int dynamic) {
    KY_UNUSED(rd);
    KY_UNUSED(size);
    KY_UNUSED(data);
    KY_UNUSED(dynamic);
    return (kyBuffer *)malloc(1);
}

void ky_rd_destroy_buffer(kyRenderDevice *rd, kyBuffer *b) {
    KY_UNUSED(rd);
    if (b) free(b);
}

kyTexture *ky_rd_create_texture_2d(kyRenderDevice *rd, int w, int h, int channels, const void *pixels) {
    KY_UNUSED(rd);
    KY_UNUSED(w);
    KY_UNUSED(h);
    KY_UNUSED(channels);
    KY_UNUSED(pixels);
    return (kyTexture *)malloc(1);
}

void ky_rd_destroy_texture(kyRenderDevice *rd, kyTexture *t) {
    KY_UNUSED(rd);
    if (t) free(t);
}

kyPipeline *ky_rd_create_pipeline(kyRenderDevice *rd, const kyPipelineDesc *desc) {
    if (!rd || !desc) return NULL;
    kyPipelineImpl *p = (kyPipelineImpl *)malloc(sizeof(kyPipelineImpl));
    if (!p) return NULL;
    p->rd = rd;
    p->shader = (kyShader *)desc->shader;
    p->layout = desc->layout;
    p->topology = desc->topology;
    p->depth_test = desc->depth_test;
    p->depth_write = desc->depth_write;
    p->cull_mode = desc->cull_mode;
    p->blend_on = desc->blend.on;
    p->blend_color = desc->blend.color;
    p->impl = NULL;
    return (kyPipeline *)p;
}

void ky_rd_destroy_pipeline(kyRenderDevice *rd, kyPipeline *p) {
    KY_UNUSED(rd);
    if (p) free(p);
}

void *ky_rd_begin(kyRenderDevice *rd) {
    KY_UNUSED(rd);
    return malloc(1);
}

void ky_cmd_set_pipeline(void *cl, kyPipeline *p) {
    KY_UNUSED(cl);
    KY_UNUSED(p);
}

void ky_cmd_set_vertex_buffer(void *cl, kyBuffer *vb, uint32_t stride) {
    KY_UNUSED(cl);
    KY_UNUSED(vb);
    KY_UNUSED(stride);
}

void ky_cmd_set_index_buffer(void *cl, kyBuffer *ib, uint32_t index_size) {
    KY_UNUSED(cl);
    KY_UNUSED(ib);
    KY_UNUSED(index_size);
}

void ky_cmd_set_texture(void *cl, int slot, kyTexture *tex) {
    KY_UNUSED(cl);
    KY_UNUSED(slot);
    KY_UNUSED(tex);
}

void ky_cmd_set_uniform(void *cl, int location, const void *data, int bytes) {
    KY_UNUSED(cl);
    KY_UNUSED(location);
    KY_UNUSED(data);
    KY_UNUSED(bytes);
}

void ky_cmd_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    KY_UNUSED(cl);
    KY_UNUSED(count);
    KY_UNUSED(instances);
}

void ky_cmd_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    KY_UNUSED(cl);
    KY_UNUSED(vertex_count);
    KY_UNUSED(instances);
}

void ky_rd_submit(kyRenderDevice *rd, void *cl) {
    KY_UNUSED(rd);
    free(cl);
}

void ky_rd_present(kyRenderDevice *rd) {
    KY_UNUSED(rd);
}

void ky_rd_clear(kyRenderDevice *rd, kyVec4 clear_color, float clear_depth) {
    KY_UNUSED(rd);
    KY_UNUSED(clear_color);
    KY_UNUSED(clear_depth);
}
