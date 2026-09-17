#include "kronyx/engine.h"
#include "kronyx/log.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

#define KY_KEY_MAP_SIZE 512
static int g_key_state[KY_KEY_MAP_SIZE];

typedef struct kyEngineWindow {
    GLFWwindow *w;
    float w_px;
    float h_px;
} kyEngineWindow;

static kyEngineWindow *g_window = NULL;

static void key_callback(GLFWwindow *win, int key, int scancode, int action, int mods) {
    KY_UNUSED(win);
    KY_UNUSED(scancode);
    KY_UNUSED(mods);
    if (key >= 0 && key < KY_KEY_MAP_SIZE) {
        g_key_state[key] = (action == GLFW_PRESS || action == GLFW_REPEAT) ? 1 : 0;
    }
}

int ky_engine_init(int width, int height, const char *title) {
    memset(g_key_state, 0, sizeof(g_key_state));
    if (!glfwInit()) {
        KY_LOG_ERROR("GLFW init failed");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *win = glfwCreateWindow(width, height, title ? title : "Kronyx", NULL, NULL);
    if (!win) {
        KY_LOG_ERROR("glfwCreateWindow failed");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(win);
    glfwSetKeyCallback(win, key_callback);
    glfwSwapInterval(1); /* vsync */

    kyEngineWindow *gw = (kyEngineWindow *)malloc(sizeof(kyEngineWindow));
    if (!gw) { glfwDestroyWindow(win); glfwTerminate(); return -1; }
    gw->w = win;
    int wx, wy;
    glfwGetWindowSize(win, &wx, &wy);
    gw->w_px = (float)wx;
    gw->h_px = (float)wy;
    g_window = gw;

    KY_LOG_INFO("Engine started %dx%d", wx, wy);
    return 0;
}

int ky_engine_run(float (*update)(float dt), void (*render)(void)) {
    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(g_window->w)) {
        double now = glfwGetTime();
        float dt = (float)(now - last_time);
        if (dt > 0.1f) dt = 0.1f;
        last_time = now;

        glfwPollEvents();
        if (update) update(dt);
        if (render) render();
        glfwSwapBuffers(g_window->w);
    }
    return 0;
}

void ky_engine_shutdown(void) {
    if (g_window) {
        glfwDestroyWindow(g_window->w);
        free(g_window);
        g_window = NULL;
    }
    glfwTerminate();
}

float ky_engine_width(void)  { return g_window ? g_window->w_px  : 0.0f; }
float ky_engine_height(void) { return g_window ? g_window->h_px : 0.0f; }

int ky_engine_key_pressed(int key) {
    if (key < 0 || key >= KY_KEY_MAP_SIZE) return 0;
    return g_key_state[key];
}
