#ifndef KRONYX_GLFW_H
#define KRONYX_GLFW_H

#include "defines.h"
#include "math.h"

typedef struct kyGlfwWindow kyGlfwWindow;

KY_API int      ky_glfw_init(void);
KY_API void     ky_glfw_term(void);
KY_API kyGlfwWindow *ky_glfw_create_window(const char *title, int w, int h);
KY_API void     ky_glfw_destroy_window(kyGlfwWindow *win);
KY_API void     ky_glfw_poll_events(void);
KY_API int      ky_glfw_window_should_close(const kyGlfwWindow *win);
KY_API void     ky_glfw_swap_buffers(kyGlfwWindow *win);
KY_API void     ky_glfw_set_window_pos(kyGlfwWindow *win, int x, int y);
KY_API void     ky_glfw_get_cursor_pos(const kyGlfwWindow *win, float *xout, float *yout);
KY_API void     ky_glfw_set_cursor_pos(kyGlfwWindow *win, float x, float y);
KY_API void     ky_glfw_hide_cursor(kyGlfwWindow *win, int hide);
KY_API int      ky_glfw_key_pressed(kyGlfwWindow *win, int glfw_key);

#endif
