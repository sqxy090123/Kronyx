#ifndef KRONYX_ENGINE_H
#define KRONYX_ENGINE_H

#include "defines.h"
#include "math.h"

/* Window handle (opaque). */
typedef struct kyEngineWindow kyEngineWindow;

/* Engine initialisation. title may be NULL. Returns 0 on success. */
KY_API int ky_engine_init(int width, int height, const char *title);

/* Main loop: calls update(dt) then render() each frame until the window is
 * closed.  Both function pointers may be NULL (treated as no-ops). */
KY_API int ky_engine_run(float (*update)(float dt), void (*render)(void));

/* Shut down and destroy the window. */
KY_API void ky_engine_shutdown(void);

/* Query window dimensions (may change on resize). */
KY_API float ky_engine_width(void);
KY_API float ky_engine_height(void);

/* Poll whether a GLFW key is currently held down. */
KY_API int  ky_engine_key_pressed(int key);

#endif
