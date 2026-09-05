#ifndef KRONYX_RENDER_BACKEND_H
#define KRONYX_RENDER_BACKEND_H

#include "kronyx/render.h"

typedef void *(*kybe_create_fn)(void *platform_win);
typedef void  (*kybe_destroy_fn)(void *impl);
typedef const char *(*kybe_name_fn)(void);

typedef void *(*kybe_create_shader_fn)(void *impl, const kyShaderSource *src);
typedef void  (*kybe_destroy_shader_fn)(void *impl, void *shader);
typedef int   (*kybe_shader_uniform_fn)(void *impl, void *shader, const char *name);
typedef void *(*kybe_create_buffer_fn)(void *impl, size_t size, const void *data, int dynamic);
typedef void  (*kybe_destroy_buffer_fn)(void *impl, void *buf);
typedef int   (*kybe_update_buffer_fn)(void *impl, void *buf, size_t offset, size_t size, const void *data);
typedef void *(*kybe_create_texture_fn)(void *impl, int w, int h, int ch, const void *px);
typedef void  (*kybe_destroy_texture_fn)(void *impl, void *tex);
typedef void *(*kybe_create_pipeline_fn)(void *impl, const kyPipelineDesc *desc);
typedef void  (*kybe_destroy_pipeline_fn)(void *impl, void *pipe);

typedef void *(*kybe_begin_fn)(void *impl);
typedef void  (*kybe_set_pipeline_fn)(void *cl, void *pipe);
typedef void  (*kybe_set_vertex_buffer_fn)(void *cl, void *vb, uint32_t stride);
typedef void  (*kybe_set_index_buffer_fn)(void *cl, void *ib, uint32_t index_size);
typedef void  (*kybe_set_texture_fn)(void *cl, int slot, void *tex);
typedef void  (*kybe_set_uniform_fn)(void *cl, int loc, const void *data, int bytes);
typedef void  (*kybe_draw_indexed_fn)(void *cl, uint32_t count, uint32_t instances);
typedef void  (*kybe_draw_array_fn)(void *cl, uint32_t vertex_count, uint32_t instances);
typedef void  (*kybe_submit_fn)(void *impl, void *cl);
typedef void  (*kybe_present_fn)(void *impl);
typedef void  (*kybe_clear_fn)(void *impl, kyVec4 color, float depth);

typedef struct kyRenderBackend {
    kybe_create_fn           create;
    kybe_destroy_fn          destroy;
    kybe_name_fn             name;
    kybe_create_shader_fn    create_shader;
    kybe_destroy_shader_fn   destroy_shader;
    kybe_shader_uniform_fn   shader_uniform;
    kybe_create_buffer_fn    create_buffer;
    kybe_destroy_buffer_fn   destroy_buffer;
    kybe_update_buffer_fn    update_buffer;
    kybe_create_texture_fn   create_texture;
    kybe_destroy_texture_fn  destroy_texture;
    kybe_create_pipeline_fn  create_pipeline;
    kybe_destroy_pipeline_fn destroy_pipeline;
    kybe_begin_fn            begin;
    kybe_set_pipeline_fn     set_pipeline;
    kybe_set_vertex_buffer_fn set_vertex_buffer;
    kybe_set_index_buffer_fn set_index_buffer;
    kybe_set_texture_fn      set_texture;
    kybe_set_uniform_fn      set_uniform;
    kybe_draw_indexed_fn     draw_indexed;
    kybe_draw_array_fn       draw_array;
    kybe_submit_fn           submit;
    kybe_present_fn          present;
    kybe_clear_fn            clear;
} kyRenderBackend;

struct kyRenderDevice {
    kyRendererBackend backend;
    void *impl;
    const kyRenderBackend *vt;
    kyAllocator alloc;
};

extern const kyRenderBackend ky_backend_console;
extern const kyRenderBackend ky_backend_gl;

#endif
