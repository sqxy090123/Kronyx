/* Demo: rotating, WASD-controlled square rendered with raw OpenGL 3.3. */

#include <GL/glew.h>
#include "kronyx/engine.h"
#include "kronyx/math.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Shader source                                                       */
/* ------------------------------------------------------------------ */

static const char *vert_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "uniform mat4 u_mvp;\n"
    "void main(){ gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0); }\n";

static const char *frag_src =
    "#version 330 core\n"
    "out vec4 out_color;\n"
    "void main(){ out_color = vec4(1.0); }\n";

/* ------------------------------------------------------------------ */
/* Simple matrix helpers (col-major 4x4)                               */
/* ------------------------------------------------------------------ */

static void mat4_identity(float m[16]) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_mul(const float a[16], const float b[16], float out[16]) {
    float t[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            t[j*4+i] = 0.0f;
            for (int k = 0; k < 4; k++)
                t[j*4+i] += a[k*4+i] * b[j*4+k];
        }
    memcpy(out, t, 64);
}

static void mat4_ortho(float left, float right, float bottom, float top,
                       float zn, float zf, float m[16]) {
    mat4_identity(m);
    m[0]  = 2.0f / (right - left);
    m[5]  = 2.0f / (top - bottom);
    m[10] = -2.0f / (zf - zn);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(zf + zn) / (zf - zn);
}

static void mat4_rotateZ(float angle, float m[16]) {
    mat4_identity(m);
    float c = cosf(angle), s = sinf(angle);
    m[0] = c;  m[1] = s;
    m[4] = -s; m[5] = c;
}

static void mat4_translate(float x, float y, float m[16]) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
}

/* ------------------------------------------------------------------ */
/* Demo state                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    float x, y;
    float angle;
} State;

static State  g_state = {0};
static GLuint g_prog = 0, g_vao = 0, g_vbo = 0, g_ibo = 0;
static GLint  g_loc_mvp = -1;

/* Square vertices (centered at origin, size 1.0) + indices */
static const float g_verts[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f,
};
static const unsigned char g_indices[] = { 0,1,2, 0,2,3 };

static int compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static int setup_gl(void) {
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW init failed: %s\n", glewGetErrorString(err));
        return 0;
    }

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0; }

    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    int ok;
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(g_prog, 512, NULL, log);
        fprintf(stderr, "Link error:\n%s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    g_loc_mvp = glGetUniformLocation(g_prog, "u_mvp");

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_verts), g_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glGenBuffers(1, &g_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_indices), g_indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glUseProgram(0);
    return 1;
}

static void cleanup_gl(void) {
    if (g_ibo) glDeleteBuffers(1, &g_ibo);
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    if (g_vao) glDeleteVertexArrays(1, &g_vao);
    if (g_prog) glDeleteProgram(g_prog);
    g_prog = 0; g_vao = 0; g_vbo = 0; g_ibo = 0;
}

static float update(float dt) {
    float speed = 3.0f;
    if (ky_engine_key_pressed(GLFW_KEY_W) || ky_engine_key_pressed(GLFW_KEY_UP))
        g_state.y += speed * dt;
    if (ky_engine_key_pressed(GLFW_KEY_S) || ky_engine_key_pressed(GLFW_KEY_DOWN))
        g_state.y -= speed * dt;
    if (ky_engine_key_pressed(GLFW_KEY_A) || ky_engine_key_pressed(GLFW_KEY_LEFT))
        g_state.x -= speed * dt;
    if (ky_engine_key_pressed(GLFW_KEY_D) || ky_engine_key_pressed(GLFW_KEY_RIGHT))
        g_state.x += speed * dt;
    g_state.angle += dt * 1.5f;
    return dt;
}

static void render(void) {
    float w = ky_engine_width(), h = ky_engine_height();
    float aspect = w / h;
    float half = 4.0f;
    float lr = half * aspect, bt = half;

    float proj[16], model[16], view[16], mvp[16];
    mat4_ortho(-lr, lr, -bt, bt, -1.0f, 1.0f, proj);
    mat4_rotateZ(g_state.angle, model);
    mat4_translate(g_state.x, g_state.y, view);
    mat4_mul(proj, view, mvp);
    mat4_mul(mvp, model, mvp);

    glViewport(0, 0, (GLuint)w, (GLuint)h);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_prog);
    glUniformMatrix4fv(g_loc_mvp, 1, GL_FALSE, mvp);
    glBindVertexArray(g_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, NULL);
    glBindVertexArray(0);
    glUseProgram(0);
}

int main(void) {
    if (ky_engine_init(800, 600, "Kronyx — rotating square demo") != 0)
        return 1;
    if (!setup_gl()) {
        fprintf(stderr, "Failed to initialise OpenGL\n");
        ky_engine_shutdown();
        return 1;
    }
    printf("Running. WASD to move, ESC to quit.\n");
    ky_engine_run(update, render);
    cleanup_gl();
    ky_engine_shutdown();
    printf("Demo exited cleanly.\n");
    return 0;
}
