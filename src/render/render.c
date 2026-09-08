#include "render_backend.h"
#include "kronyx/math.h"
#include "kronyx/memory.h"
#include <stdlib.h>
#include <string.h>

typedef struct kyCmdHeader {
    kyRenderDevice *rd;
} kyCmdHeader;

kyRenderDevice *ky_rd_create(kyRendererBackend backend, void *platform_win) {
    const kyRenderBackend *vt = NULL;
    switch (backend) {
        case KY_RENDERER_CONSOLE: vt = &ky_backend_console; break;
        case KY_RENDERER_GL:      vt = &ky_backend_gl;      break;
        default: return NULL;
    }

    void *impl = vt->create(platform_win);
    if (!impl) return NULL;

    kyRenderDevice *rd = (kyRenderDevice *)malloc(sizeof(kyRenderDevice));
    if (!rd) { vt->destroy(impl); return NULL; }
    memset(rd, 0, sizeof(*rd));

    rd->backend = backend;
    rd->impl = impl;
    rd->vt = vt;
    rd->alloc = ky_default_allocator();
    return rd;
}

void ky_rd_destroy(kyRenderDevice *rd) {
    if (!rd) return;
    rd->vt->destroy(rd->impl);
    free(rd);
}

const char *ky_rd_backend_name(const kyRenderDevice *rd) {
    if (!rd) return "unknown";
    return rd->vt->name();
}

kyRendererBackend ky_rd_backend(const kyRenderDevice *rd) {
    return rd ? rd->backend : KY_RENDERER_NONE;
}

kyShader *ky_rd_create_shader(kyRenderDevice *rd, const kyShaderSource *src) {
    if (!rd) return NULL;
    return (kyShader *)rd->vt->create_shader(rd->impl, src);
}

int ky_rd_shader_uniform(const kyRenderDevice *rd, const kyShader *s, const char *name) {
    if (!rd || !s || !name) return -1;
    return rd->vt->shader_uniform(rd->impl, (void *)s, name);
}

void ky_rd_destroy_shader(kyRenderDevice *rd, kyShader *s) {
    if (!rd || !s) return;
    rd->vt->destroy_shader(rd->impl, s);
}

kyBuffer *ky_rd_create_buffer(kyRenderDevice *rd, size_t size, const void *data, int dynamic) {
    if (!rd) return NULL;
    return (kyBuffer *)rd->vt->create_buffer(rd->impl, size, data, dynamic);
}

void ky_rd_destroy_buffer(kyRenderDevice *rd, kyBuffer *b) {
    if (!rd || !b) return;
    rd->vt->destroy_buffer(rd->impl, b);
}

int ky_rd_update_buffer(kyRenderDevice *rd, kyBuffer *b, size_t offset, size_t size, const void *data) {
    if (!rd || !b || !data || size == 0) return -1;
    return rd->vt->update_buffer(rd->impl, b, offset, size, data);
}

kyTexture *ky_rd_create_texture_2d(kyRenderDevice *rd, int w, int h, int channels, const void *pixels) {
    if (!rd) return NULL;
    return (kyTexture *)rd->vt->create_texture(rd->impl, w, h, channels, pixels);
}

void ky_rd_destroy_texture(kyRenderDevice *rd, kyTexture *t) {
    if (!rd || !t) return;
    rd->vt->destroy_texture(rd->impl, t);
}

kyPipeline *ky_rd_create_pipeline(kyRenderDevice *rd, const kyPipelineDesc *desc) {
    if (!rd) return NULL;
    return (kyPipeline *)rd->vt->create_pipeline(rd->impl, desc);
}

void ky_rd_destroy_pipeline(kyRenderDevice *rd, kyPipeline *p) {
    if (!rd || !p) return;
    rd->vt->destroy_pipeline(rd->impl, p);
}

void *ky_rd_begin(kyRenderDevice *rd) {
    if (!rd) return NULL;
    void *cl = rd->vt->begin(rd->impl);
    if (cl) {
        kyCmdHeader *hdr = (kyCmdHeader *)cl;
        hdr->rd = rd;
    }
    return cl;
}

static kyRenderDevice *cl_rd(void *cl) {
    return cl ? ((kyCmdHeader *)cl)->rd : NULL;
}

void ky_cmd_set_pipeline(void *cl, kyPipeline *p) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->set_pipeline(cl, p);
}

void ky_cmd_set_vertex_buffer(void *cl, kyBuffer *vb, uint32_t stride) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->set_vertex_buffer(cl, vb, stride);
}

void ky_cmd_set_index_buffer(void *cl, kyBuffer *ib, uint32_t index_size) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->set_index_buffer(cl, ib, index_size);
}

void ky_cmd_set_texture(void *cl, int slot, kyTexture *tex) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->set_texture(cl, slot, tex);
}

void ky_cmd_set_uniform(void *cl, int location, const void *data, int bytes) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->set_uniform(cl, location, data, bytes);
}

void ky_cmd_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->draw_indexed(cl, count, instances);
}

void ky_cmd_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    kyRenderDevice *rd = cl_rd(cl);
    if (rd) rd->vt->draw_array(cl, vertex_count, instances);
}

void ky_rd_submit(kyRenderDevice *rd, void *cl) {
    if (!rd || !cl) return;
    if (rd->draw_pass_hook) {
        rd->draw_pass_hook(rd, cl, rd->draw_pass_user);
        return;
    }
    rd->vt->submit(rd->impl, cl);
}

void ky_rd_present(kyRenderDevice *rd) {
    if (!rd) return;
    rd->vt->present(rd->impl);
}

void ky_rd_clear(kyRenderDevice *rd, kyVec4 clear_color, float clear_depth) {
    if (!rd) return;
    rd->vt->clear(rd->impl, clear_color, clear_depth);
}

void ky_rd_set_draw_pass(kyRenderDevice *rd, kyDrawPassFn fn, void *user) {
    if (!rd) return;
    rd->draw_pass_hook = fn;
    rd->draw_pass_user = user;
}
