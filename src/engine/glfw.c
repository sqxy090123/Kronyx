#include "kronyx/glfw.h"
#include "kronyx/log.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

#define KY_KEY_MAP_SIZE 512
static int g_key_state[KY_KEY_MAP_SIZE];

typedef struct kyGlfwWindow {
    GLFWwindow *w;
    float cursor_x;
    float cursor_y;
} kyGlfwWindow;

static void key_callback(GLFWwindow *win, int key, int scancode, int action, int mods) {
    KY_UNUSED(scancode);
    KY_UNUSED(mods);
    KY_UNUSED(win);
    if (key >= 0 && key < KY_KEY_MAP_SIZE) {
        g_key_state[key] = (action == GLFW_PRESS || action == GLFW_REPEAT) ? 1 : 0;
    }
}

static void cursor_callback(GLFWwindow *win, double dx, double dy) {
    kyGlfwWindow *gw = (kyGlfwWindow *)glfwGetWindowUserPointer(win);
    if (gw) {
        gw->cursor_x = (float)dx;
        gw->cursor_y = (float)dy;
    }
}

int ky_glfw_init(void) {
    memset(g_key_state, 0, sizeof(g_key_state));
    if (!glfwInit()) {
        KY_LOG_ERROR("GLFW init failed");
        return -1;
    }
    return 0;
}

void ky_glfw_term(void) {
    glfwTerminate();
}

kyGlfwWindow *ky_glfw_create_window(const char *title, int w, int h) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *win = glfwCreateWindow(w, h, title, NULL, NULL);
    if (!win) {
        KY_LOG_ERROR("glfwCreateWindow failed");
        return NULL;
    }
    glfwMakeContextCurrent(win);
    glfwSetKeyCallback(win, key_callback);
    glfwSetCursorPosCallback(win, cursor_callback);

    kyGlfwWindow *gw = (kyGlfwWindow *)malloc(sizeof(kyGlfwWindow));
    if (!gw) { glfwDestroyWindow(win); return NULL; }
    gw->w = win;
    gw->cursor_x = (float)w / 2.0f;
    gw->cursor_y = (float)h / 2.0f;
    glfwSetWindowUserPointer(win, gw);
    return gw;
}

void ky_glfw_destroy_window(kyGlfwWindow *win) {
    if (!win) return;
    glfwDestroyWindow(win->w);
    free(win);
}

void ky_glfw_poll_events(void) {
    glfwPollEvents();
}

int ky_glfw_window_should_close(const kyGlfwWindow *win) {
    return win ? glfwWindowShouldClose(win->w) : 1;
}

void ky_glfw_swap_buffers(kyGlfwWindow *win) {
    if (win) glfwSwapBuffers(win->w);
}

void ky_glfw_set_window_pos(kyGlfwWindow *win, int x, int y) {
    if (win) glfwSetWindowPos(win->w, x, y);
}

void ky_glfw_get_cursor_pos(const kyGlfwWindow *win, float *xout, float *yout) {
    if (!win) return;
    double dx, dy;
    glfwGetCursorPos(win->w, &dx, &dy);
    if (xout) *xout = (float)dx;
    if (yout) *yout = (float)dy;
}

void ky_glfw_set_cursor_pos(kyGlfwWindow *win, float x, float y) {
    if (win) glfwSetCursorPos(win->w, (double)x, (double)y);
}

void ky_glfw_hide_cursor(kyGlfwWindow *win, int hide) {
    if (!win) return;
    glfwSetInputMode(win->w, GLFW_CURSOR,
                     hide ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
}

int ky_glfw_key_pressed(kyGlfwWindow *win, int key) {
    KY_UNUSED(win);
    if (key < 0 || key >= KY_KEY_MAP_SIZE) return 0;
    return g_key_state[key];
}
