#ifndef KRONYX_RENDER_H
#define KRONYX_RENDER_H

#include "defines.h"
#include "math.h"
#include "memory.h"

typedef enum kyRendererBackend {
    KY_RENDERER_NONE = 0,
    KY_RENDERER_GL,
    KY_RENDERER_VULKAN
} kyRendererBackend;

typedef enum kyShaderStage {
    KY_STAGE_VERTEX = 0,
    KY_STAGE_FRAGMENT,
    KY_STAGE_COMPUTE
} kyShaderStage;

typedef enum kyPrimitiveTopology {
    KY_TRIANGLES = 0,
    KY_TRIANGLE_STRIP,
    KY_LINES,
    KY_POINTS
} kyPrimitiveTopology;

typedef struct kyShaderSource {
    const char *vs;
    const char *fs;
    const char *cs;
    const char *entry;
} kyShaderSource;

typedef struct kyVertexAttrib {
    uint32_t location;
    uint32_t offset;
    uint32_t size;
    uint8_t normalized;
} kyVertexAttrib;

typedef struct kyVertexLayout {
    const kyVertexAttrib *attribs;
    uint32_t count;
    uint32_t stride;
} kyVertexLayout;

typedef struct kyPipelineDesc {
    void *shader;
    kyVertexLayout layout;
    kyPrimitiveTopology topology;
    int depth_test;
    int depth_write;
    int cull_mode;
    struct { int on; kyVec4 color; } blend;
} kyPipelineDesc;

typedef struct kyRenderDevice kyRenderDevice;
typedef struct kyShader kyShader;
typedef struct kyBuffer kyBuffer;
typedef struct kyTexture kyTexture;
typedef struct kyPipeline kyPipeline;
typedef struct kyCommandList kyCommandList;

KY_API kyRenderDevice *ky_rd_create(kyRendererBackend backend, void *platform_win);
KY_API void            ky_rd_destroy(kyRenderDevice *rd);
KY_API const char     *ky_rd_backend_name(const kyRenderDevice *rd);

KY_API kyShader   *ky_rd_create_shader(kyRenderDevice *rd, const kyShaderSource *src);
KY_API void        ky_rd_destroy_shader(kyRenderDevice *rd, kyShader *s);
KY_API kyBuffer   *ky_rd_create_buffer(kyRenderDevice *rd, size_t size, const void *data, int dynamic);
KY_API void        ky_rd_destroy_buffer(kyRenderDevice *rd, kyBuffer *b);
KY_API kyTexture  *ky_rd_create_texture_2d(kyRenderDevice *rd, int w, int h, int channels, const void *pixels);
KY_API void        ky_rd_destroy_texture(kyRenderDevice *rd, kyTexture *t);
KY_API kyPipeline *ky_rd_create_pipeline(kyRenderDevice *rd, const kyPipelineDesc *desc);
KY_API void        ky_rd_destroy_pipeline(kyRenderDevice *rd, kyPipeline *p);

KY_API void    *ky_rd_begin(kyRenderDevice *rd);
KY_API void     ky_cmd_set_pipeline(void *cl, kyPipeline *p);
KY_API void     ky_cmd_set_vertex_buffer(void *cl, kyBuffer *vb, uint32_t stride);
KY_API void     ky_cmd_set_index_buffer(void *cl, kyBuffer *ib, uint32_t index_size);
KY_API void     ky_cmd_set_texture(void *cl, int slot, kyTexture *tex);
KY_API void     ky_cmd_set_uniform(void *cl, int location, const void *data, int bytes);
KY_API void     ky_cmd_draw_indexed(void *cl, uint32_t count, uint32_t instances);
KY_API void     ky_cmd_draw_array(void *cl, uint32_t vertex_count, uint32_t instances);
KY_API void     ky_rd_submit(kyRenderDevice *rd, void *cl);
KY_API void     ky_rd_present(kyRenderDevice *rd);
KY_API void     ky_rd_clear(kyRenderDevice *rd, kyVec4 clear_color, float clear_depth);

#endif
