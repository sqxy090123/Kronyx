#include "render_backend.h"
#include "kronyx/math.h"
#include "kronyx/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef KY_HAS_EGL

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#define KY_GL_MAX_ATTRIBS 8

typedef struct kyGLDevice {
    EGLDisplay dpy;
    EGLContext ctx;
    EGLSurface surf;
    GLuint vao;
    int width;
    int height;
    int frame_count;
} kyGLDevice;

typedef struct kyGLShader {
    GLuint program;
} kyGLShader;

typedef struct kyGLBuffer {
    GLuint id;
    size_t size;
    int dynamic;
} kyGLBuffer;

typedef struct kyGLTexture {
    GLuint id;
    int w;
    int h;
} kyGLTexture;

typedef struct kyGLPipeline {
    kyGLShader *shader;
    kyVertexLayout layout;
    kyVertexAttrib attribs[KY_GL_MAX_ATTRIBS];
    kyPrimitiveTopology topology;
    int depth_test;
    int depth_write;
    int cull_mode;
    int blend_on;
} kyGLPipeline;

typedef struct kyGLCmdList {
    kyRenderDevice *rd;
    kyGLPipeline *pipeline;
    uint32_t index_size;
} kyGLCmdList;

/* ky_rd_begin aliases command lists via kyCmdHeader; rd must be first. */
KY_STATIC_ASSERT(offsetof(kyGLCmdList, rd) == 0,
                 "kyGLCmdList.rd must be the first member");

static GLenum gl_topology(kyPrimitiveTopology t) {
    switch (t) {
        case KY_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
        case KY_LINES:          return GL_LINES;
        case KY_POINTS:         return GL_POINTS;
        default:                return GL_TRIANGLES;
    }
}

static void gl_apply_state(kyGLPipeline *p) {
    if (p->depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(p->depth_write ? GL_TRUE : GL_FALSE);
    if (p->cull_mode) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }
    if (p->blend_on) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

static void *gl_create(void *platform_win) {
    kyGLDevice *d = (kyGLDevice *)calloc(1, sizeof(kyGLDevice));
    if (!d) return NULL;

    if (platform_win) {
        d->dpy = eglGetDisplay((EGLNativeDisplayType)platform_win);
    } else {
        d->dpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
        if (d->dpy == EGL_NO_DISPLAY)
            d->dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    if (d->dpy == EGL_NO_DISPLAY || !eglInitialize(d->dpy, NULL, NULL)) {
        ky_log_write(KY_LOG_ERROR, "gl_backend: eglInitialize failed (0x%x)", eglGetError());
        free(d);
        return NULL;
    }

    EGLConfig cfg = NULL;
    EGLint n = 0;
    EGLint cattr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    if (!eglChooseConfig(d->dpy, cattr, &cfg, 1, &n) || n < 1) {
        ky_log_write(KY_LOG_ERROR, "gl_backend: no GLES3 config (0x%x)", eglGetError());
        eglTerminate(d->dpy);
        free(d);
        return NULL;
    }

    EGLint pbuf[] = { EGL_WIDTH, 128, EGL_HEIGHT, 128, EGL_NONE };
    d->surf = eglCreatePbufferSurface(d->dpy, cfg, pbuf);
    if (d->surf == EGL_NO_SURFACE) {
        ky_log_write(KY_LOG_ERROR, "gl_backend: pbuffer surface failed (0x%x)", eglGetError());
        eglTerminate(d->dpy);
        free(d);
        return NULL;
    }

    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    d->ctx = eglCreateContext(d->dpy, cfg, EGL_NO_CONTEXT, ctxattr);
    if (d->ctx == EGL_NO_CONTEXT || !eglMakeCurrent(d->dpy, d->surf, d->surf, d->ctx)) {
        ky_log_write(KY_LOG_ERROR, "gl_backend: GLES3 context failed (0x%x)", eglGetError());
        eglDestroySurface(d->dpy, d->surf);
        eglTerminate(d->dpy);
        free(d);
        return NULL;
    }

    d->width = 128;
    d->height = 128;
    glGenVertexArrays(1, &d->vao);
    glBindVertexArray(d->vao);
    return d;
}

static void gl_destroy(void *impl) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d) return;
    glDeleteVertexArrays(1, &d->vao);
    eglMakeCurrent(d->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(d->dpy, d->ctx);
    eglDestroySurface(d->dpy, d->surf);
    eglTerminate(d->dpy);
    free(d);
}

static const char *gl_name(void) {
    return "opengl3.3";
}

static void *gl_create_shader(void *impl, const kyShaderSource *src) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d || !src || !src->vs || !src->fs) return NULL;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &src->vs, NULL);
    glCompileShader(vs);
    GLint ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(vs, sizeof(log), NULL, log);
        ky_log_write(KY_LOG_ERROR, "gl_backend: vertex shader: %s", log);
        glDeleteShader(vs);
        return NULL;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &src->fs, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(fs, sizeof(log), NULL, log);
        ky_log_write(KY_LOG_ERROR, "gl_backend: fragment shader: %s", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return NULL;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        ky_log_write(KY_LOG_ERROR, "gl_backend: program link: %s", log);
        glDeleteProgram(prog);
        return NULL;
    }

    kyGLShader *s = (kyGLShader *)malloc(sizeof(kyGLShader));
    if (!s) { glDeleteProgram(prog); return NULL; }
    s->program = prog;
    return s;
}

static void gl_destroy_shader(void *impl, void *shader) {
    kyGLDevice *d = (kyGLDevice *)impl;
    kyGLShader *s = (kyGLShader *)shader;
    if (!d || !s) return;
    glDeleteProgram(s->program);
    free(s);
}

static int gl_shader_uniform(void *impl, void *shader, const char *name) {
    kyGLDevice *d = (kyGLDevice *)impl;
    kyGLShader *s = (kyGLShader *)shader;
    if (!d || !s || !name) return -1;
    return glGetUniformLocation(s->program, name);
}

static void *gl_create_buffer(void *impl, size_t size, const void *data, int dynamic) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d || size == 0) return NULL;

    kyGLBuffer *b = (kyGLBuffer *)malloc(sizeof(kyGLBuffer));
    if (!b) return NULL;
    glGenBuffers(1, &b->id);
    glBindBuffer(GL_ARRAY_BUFFER, b->id);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    b->size = size;
    b->dynamic = dynamic;
    return b;
}

static void gl_destroy_buffer(void *impl, void *buf) {
    kyGLDevice *d = (kyGLDevice *)impl;
    kyGLBuffer *b = (kyGLBuffer *)buf;
    if (!d || !b) return;
    glDeleteBuffers(1, &b->id);
    free(b);
}

static int gl_update_buffer(void *impl, void *buf, size_t offset, size_t size, const void *data) {
    kyGLDevice *d = (kyGLDevice *)impl;
    kyGLBuffer *b = (kyGLBuffer *)buf;
    if (!d || !b || !data || size == 0) return -1;
    if (offset > b->size || size > b->size - offset) return -1;
    glBindBuffer(GL_ARRAY_BUFFER, b->id);
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
    return 0;
}

static void *gl_create_texture(void *impl, int w, int h, int ch, const void *px) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d || w <= 0 || h <= 0) return NULL;

    kyGLTexture *t = (kyGLTexture *)malloc(sizeof(kyGLTexture));
    if (!t) return NULL;
    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    GLenum fmt = (ch >= 3) ? GL_RGBA : GL_LUMINANCE;
    glTexImage2D(GL_TEXTURE_2D, 0, (ch >= 3) ? GL_RGBA : GL_LUMINANCE, w, h, 0,
                 fmt, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    t->w = w;
    t->h = h;
    return t;
}

static void gl_destroy_texture(void *impl, void *tex) {
    kyGLDevice *d = (kyGLDevice *)impl;
    kyGLTexture *t = (kyGLTexture *)tex;
    if (!d || !t) return;
    glDeleteTextures(1, &t->id);
    free(t);
}

static void *gl_create_pipeline(void *impl, const kyPipelineDesc *desc) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d || !desc) return NULL;

    kyGLPipeline *p = (kyGLPipeline *)calloc(1, sizeof(kyGLPipeline));
    if (!p) return NULL;
    p->shader = (kyGLShader *)desc->shader;
    p->topology = desc->topology;
    p->depth_test = desc->depth_test;
    p->depth_write = desc->depth_write;
    p->cull_mode = desc->cull_mode;
    p->blend_on = desc->blend.on;
    if (desc->layout.attribs && desc->layout.count > 0) {
        uint32_t n = desc->layout.count;
        if (n > KY_GL_MAX_ATTRIBS) n = KY_GL_MAX_ATTRIBS;
        memcpy(p->attribs, desc->layout.attribs, sizeof(kyVertexAttrib) * n);
        p->layout.attribs = p->attribs;
        p->layout.count = n;
        p->layout.stride = desc->layout.stride;
    }
    return p;
}

static void gl_destroy_pipeline(void *impl, void *pipe) {
    kyGLDevice *d = (kyGLDevice *)impl;
    kyGLPipeline *p = (kyGLPipeline *)pipe;
    if (!d || !p) return;
    free(p);
}

static void *gl_begin(void *impl) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d) return NULL;
    glViewport(0, 0, d->width, d->height);
    kyGLCmdList *cl = (kyGLCmdList *)calloc(1, sizeof(kyGLCmdList));
    return cl;
}

static void gl_set_pipeline(void *cl, void *pipe) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    kyGLPipeline *p = (kyGLPipeline *)pipe;
    if (!c || !p) return;
    c->pipeline = p;
    if (p->shader) glUseProgram(p->shader->program);
    gl_apply_state(p);
}

static void gl_set_vertex_buffer(void *cl, void *vb, uint32_t stride) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    kyGLBuffer *b = (kyGLBuffer *)vb;
    if (!c || !b || !c->pipeline) return;
    glBindBuffer(GL_ARRAY_BUFFER, b->id);
    const kyVertexLayout *lay = &c->pipeline->layout;
    for (uint32_t i = 0; i < lay->count; i++) {
        const kyVertexAttrib *a = &lay->attribs[i];
        GLenum gltype = a->type == KY_ATTRIB_UBYTE ? GL_UNSIGNED_BYTE : GL_FLOAT;
        glEnableVertexAttribArray(a->location);
        glVertexAttribPointer(a->location, a->size, gltype,
                              a->normalized ? GL_TRUE : GL_FALSE,
                              stride, (const void *)(uintptr_t)a->offset);
    }
}

static void gl_set_index_buffer(void *cl, void *ib, uint32_t index_size) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    kyGLBuffer *b = (kyGLBuffer *)ib;
    if (!c || !b) return;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b->id);
    c->index_size = index_size;
}

static void gl_set_texture(void *cl, int slot, void *tex) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    kyGLTexture *t = (kyGLTexture *)tex;
    if (!c || !t) return;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, t->id);
}

static void gl_set_uniform(void *cl, int loc, const void *data, int bytes) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    if (!c || !data || bytes <= 0) return;
    if (bytes == 64) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, (const GLfloat *)data);
    } else {
        glUniform4fv(loc, bytes / 16, (const GLfloat *)data);
    }
}

static void gl_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    if (!c || !c->pipeline) return;
    GLenum type = (c->index_size == 4) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    glDrawElementsInstanced(gl_topology(c->pipeline->topology),
                            count, type, 0, instances > 0 ? instances : 1);
}

static void gl_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    kyGLCmdList *c = (kyGLCmdList *)cl;
    if (!c || !c->pipeline) return;
    glDrawArraysInstanced(gl_topology(c->pipeline->topology), 0,
                          vertex_count, instances > 0 ? instances : 1);
}

static void gl_submit(void *impl, void *cl) {
    if (!impl || !cl) return;
    free(cl);
}

static void gl_present(void *impl) {
    kyGLDevice *d = (kyGLDevice *)impl;
    if (!d) return;
    glFinish();
    d->frame_count++;
}

static void gl_clear(void *impl, kyVec4 color, float depth) {
    if (!impl) return;
    glClearColor(color.x, color.y, color.z, color.w);
    glClearDepthf(depth);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

#else /* !KY_HAS_EGL: compile-time stub */

typedef struct kyGLDevice {
    int width;
    int height;
    int frame_count;
} kyGLDevice;

typedef struct kyGLCmdList {
    kyRenderDevice *rd;
} kyGLCmdList;

/* Stub variant used when EGL is unavailable; same offset-0 contract. */
KY_STATIC_ASSERT(offsetof(kyGLCmdList, rd) == 0,
                 "kyGLCmdList.rd must be the first member");

static void *gl_create(void *platform_win) {
    KY_UNUSED(platform_win);
    kyGLDevice *d = (kyGLDevice *)malloc(sizeof(kyGLDevice));
    if (!d) return NULL;
    d->width = 800;
    d->height = 600;
    d->frame_count = 0;
    return d;
}

static void gl_destroy(void *impl) {
    free(impl);
}

static const char *gl_name(void) {
    return "opengl3.3";
}

static void *gl_create_shader(void *impl, const kyShaderSource *src) {
    KY_UNUSED(impl);
    KY_UNUSED(src);
    return malloc(1);
}

static void gl_destroy_shader(void *impl, void *shader) {
    KY_UNUSED(impl);
    free(shader);
}

static int gl_shader_uniform(void *impl, void *shader, const char *name) {
    KY_UNUSED(impl);
    KY_UNUSED(shader);
    KY_UNUSED(name);
    return -1;
}

static void *gl_create_buffer(void *impl, size_t size, const void *data, int dynamic) {
    KY_UNUSED(impl);
    KY_UNUSED(size);
    KY_UNUSED(data);
    KY_UNUSED(dynamic);
    return malloc(1);
}

static void gl_destroy_buffer(void *impl, void *buf) {
    KY_UNUSED(impl);
    free(buf);
}

static int gl_update_buffer(void *impl, void *buf, size_t offset, size_t size, const void *data) {
    KY_UNUSED(impl);
    KY_UNUSED(buf);
    KY_UNUSED(offset);
    KY_UNUSED(size);
    KY_UNUSED(data);
    return 0;
}

static void *gl_create_texture(void *impl, int w, int h, int ch, const void *px) {
    KY_UNUSED(impl);
    KY_UNUSED(w);
    KY_UNUSED(h);
    KY_UNUSED(ch);
    KY_UNUSED(px);
    return malloc(1);
}

static void gl_destroy_texture(void *impl, void *tex) {
    KY_UNUSED(impl);
    free(tex);
}

static void *gl_create_pipeline(void *impl, const kyPipelineDesc *desc) {
    KY_UNUSED(impl);
    KY_UNUSED(desc);
    return malloc(1);
}

static void gl_destroy_pipeline(void *impl, void *pipe) {
    KY_UNUSED(impl);
    free(pipe);
}

static void *gl_begin(void *impl) {
    KY_UNUSED(impl);
    kyGLCmdList *cl = (kyGLCmdList *)calloc(1, sizeof(kyGLCmdList));
    return cl;
}

static void gl_set_pipeline(void *cl, void *pipe) {
    KY_UNUSED(cl);
    KY_UNUSED(pipe);
}

static void gl_set_vertex_buffer(void *cl, void *vb, uint32_t stride) {
    KY_UNUSED(cl);
    KY_UNUSED(vb);
    KY_UNUSED(stride);
}

static void gl_set_index_buffer(void *cl, void *ib, uint32_t index_size) {
    KY_UNUSED(cl);
    KY_UNUSED(ib);
    KY_UNUSED(index_size);
}

static void gl_set_texture(void *cl, int slot, void *tex) {
    KY_UNUSED(cl);
    KY_UNUSED(slot);
    KY_UNUSED(tex);
}

static void gl_set_uniform(void *cl, int loc, const void *data, int bytes) {
    KY_UNUSED(cl);
    KY_UNUSED(loc);
    KY_UNUSED(data);
    KY_UNUSED(bytes);
}

static void gl_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    KY_UNUSED(cl);
    KY_UNUSED(count);
    KY_UNUSED(instances);
}

static void gl_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    KY_UNUSED(cl);
    KY_UNUSED(vertex_count);
    KY_UNUSED(instances);
}

static void gl_submit(void *impl, void *cl) {
    KY_UNUSED(impl);
    free(cl);
}

static void gl_present(void *impl) {
    KY_UNUSED(impl);
}

static void gl_clear(void *impl, kyVec4 color, float depth) {
    KY_UNUSED(impl);
    KY_UNUSED(color);
    KY_UNUSED(depth);
}

#endif /* KY_HAS_EGL */

const kyRenderBackend ky_backend_gl = {
    .create           = gl_create,
    .destroy          = gl_destroy,
    .name             = gl_name,
    .create_shader    = gl_create_shader,
    .destroy_shader   = gl_destroy_shader,
    .shader_uniform   = gl_shader_uniform,
    .create_buffer    = gl_create_buffer,
    .destroy_buffer   = gl_destroy_buffer,
    .update_buffer    = gl_update_buffer,
    .create_texture   = gl_create_texture,
    .destroy_texture  = gl_destroy_texture,
    .create_pipeline  = gl_create_pipeline,
    .destroy_pipeline = gl_destroy_pipeline,
    .begin            = gl_begin,
    .set_pipeline     = gl_set_pipeline,
    .set_vertex_buffer = gl_set_vertex_buffer,
    .set_index_buffer = gl_set_index_buffer,
    .set_texture      = gl_set_texture,
    .set_uniform      = gl_set_uniform,
    .draw_indexed     = gl_draw_indexed,
    .draw_array       = gl_draw_array,
    .submit           = gl_submit,
    .present          = gl_present,
    .clear            = gl_clear,
};
