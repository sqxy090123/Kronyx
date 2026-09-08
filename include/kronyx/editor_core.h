#ifndef KRONYX_EDITOR_CORE_H
#define KRONYX_EDITOR_CORE_H

#include "kronyx/ecs.h"
#include "kronyx/2d.h"
#include "kronyx/render.h"
#include "kronyx/input.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal headless editor core.  No ImGui, no GLFW required.
 *
 * Provides:
 *   - Hierarchy listing of all alive entities in the world
 *   - Property read/write for Transform, Sprite, Camera2D on the selected entity
 *   - Input polling (key events become property edits)
 *   - Viewport rendering via ky2d_render_world
 */

typedef struct kyEditorApp kyEditorApp;

/* Create an editor app wrapping the given world.
 * backend should be KY_RENDERER_CONSOLE for headless use,
 * or any other backend when a real window is available.
 * platform_win is ignored for KY_RENDERER_CONSOLE.
 * Returns NULL on allocation failure. */
KY_API kyEditorApp *ky_editor_app_create(kyWorld *world,
                                         kyRendererBackend backend,
                                         void *platform_win);

KY_API void ky_editor_app_destroy(kyEditorApp *app);

/* ── Hierarchy ───────────────────────────────────────────────────────── */

/* Number of alive entities visible in the hierarchy. */
KY_API int ky_editor_app_count_entities(const kyEditorApp *app);

/* Return the entity at hierarchy index `idx` (0-based).
 * Invalid index returns {0,0} with no crash. */
KY_API kyEntity ky_editor_app_get_entity(const kyEditorApp *app, int idx);

/* ── Selection ───────────────────────────────────────────────────────── */

KY_API void ky_editor_app_select(kyEditorApp *app, kyEntity e);
KY_API kyEntity ky_editor_app_get_selected(const kyEditorApp *app);
KY_API int  ky_editor_app_selected_is_valid(const kyEditorApp *app);

/* Spawn a new entity and auto-select it.  Returns the new entity. */
KY_API kyEntity ky_editor_app_spawn_and_select(kyEditorApp *app);

/* Despawn the currently selected entity (no-op if none selected or invalid). */
KY_API void ky_editor_app_despawn_selected(kyEditorApp *app);

/* ── Transform properties ─────────────────────────────────────────────── */

KY_API void ky_editor_app_set_pos(kyEditorApp *app, float x, float y);
KY_API float ky_editor_app_get_pos_x(const kyEditorApp *app);
KY_API float ky_editor_app_get_pos_y(const kyEditorApp *app);

KY_API void ky_editor_app_set_scale(kyEditorApp *app, float sx, float sy);
KY_API float ky_editor_app_get_scale_x(const kyEditorApp *app);
KY_API float ky_editor_app_get_scale_y(const kyEditorApp *app);

/* ── Sprite properties ───────────────────────────────────────────────── */

KY_API void ky_editor_app_set_sprite_layer(kyEditorApp *app, int layer);
KY_API int  ky_editor_app_get_sprite_layer(const kyEditorApp *app);

/* ── Camera2D properties ─────────────────────────────────────────────── */

KY_API void ky_editor_app_set_camera_zoom(kyEditorApp *app, float zoom);
KY_API float ky_editor_app_get_camera_zoom(const kyEditorApp *app);

KY_API void ky_editor_app_set_camera_viewport_size(kyEditorApp *app,
                                                   float w, float h);
KY_API float ky_editor_app_get_camera_viewport_w(const kyEditorApp *app);
KY_API float ky_editor_app_get_camera_viewport_h(const kyEditorApp *app);

/* ── Input ───────────────────────────────────────────────────────────── */

/* Poll one input event.  Returns 1 if an event was consumed, 0 if buffer empty.
 * `out_key` receives the key code (KY_KEY_*), `out_pressed` is 1/0. */
KY_API int ky_editor_app_poll_input(const kyEditorApp *app,
                                    kyInputKey *out_key, int *out_pressed);

/* ── Component type lookups (for tests) ─────────────────────────────── */

KY_API uint32_t ky_editor_transform_type_id(const kyWorld *w);
KY_API uint32_t ky_editor_sprite_type_id(const kyWorld *w);
KY_API uint32_t ky_editor_camera_type_id(const kyWorld *w);

/* ── Rendering ───────────────────────────────────────────────────────── */

/* Render one frame to `rd`.  Uses the selected entity as camera if it has
 * a Camera2D component; falls back to entity 0 if not found.
 * Returns number of sprites drawn on success, negative on error. */
KY_API int ky_editor_app_render(const kyEditorApp *app,
                                kyRenderDevice *rd);

#ifdef __cplusplus
}
#endif

#endif /* KRONYX_EDITOR_CORE_H */
