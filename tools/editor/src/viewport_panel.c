#include "kronyx/editor.h"
#include "imgui.h"
#include <string.h>
#include <stdio.h>

/* ============================================
 * 视口面板实现
 * ============================================ */
typedef struct kyViewportPanelImpl {
    int width;
    int height;
    int texture_id;  /* 预留：绑定到FBO纹理 */
    bool show_grid;
    bool show_axes;
    kyEditorCallback draw_callback;
    void *draw_callback_user;
} kyViewportPanelImpl;

static void viewport_panel_init(kyEditorPanel *panel, void *user) {
    kyViewportPanelImpl *impl = (kyViewportPanelImpl *)malloc(sizeof(kyViewportPanelImpl));
    if (!impl) return;

    memset(impl, 0, sizeof(kyViewportPanelImpl));

    impl->width = 800;
    impl->height = 600;
    impl->texture_id = 0;
    impl->show_grid = true;
    impl->show_axes = true;

    panel->impl = impl;
}

static void viewport_panel_destroy(kyEditorPanel *panel) {
    if (!panel || !panel->impl) return;

    kyViewportPanelImpl *impl = (kyViewportPanelImpl *)panel->impl;
    free(impl);
    panel->impl = NULL;
}

static void viewport_panel_update(kyEditorPanel *panel, float dt) {
    KY_UNUSED(panel);
    KY_UNUSED(dt);
    /* 预留：更新逻辑 */
}

static void viewport_panel_draw(kyEditorPanel *panel) {
    if (!panel || !panel->impl) return;

    kyViewportPanelImpl *impl = (kyViewportPanelImpl *)panel->impl;

    /* 预留：从RHI获取FBO纹理并绑定到ImGui */
    /* ImGui::Image((void*)(intptr_t)impl->texture_id, ImVec2(impl->width, impl->height)); */

    /* 绘制网格和坐标轴（预留） */
    if (impl->show_grid) {
        /* 预留：绘制网格 */
    }

    if (impl->show_axes) {
        /* 预留：绘制X/Y/Z坐标轴 */
    }

    /* 预留：ImGuizmo变换Gizmo */
    /* ImGuizmo::SetDrawlist(); */
    /* ImGuizmo::SetOrthographic(false); */
    /* ImGuizmo::SetManipulationAxis(ImGuizmo::TRANSLATE_Y | ImGuizmo::ROTATE_X | ImGuizmo::SCALE_Z); */
}

static void viewport_panel_toggle(kyEditorPanel *panel, bool *minimized) {
    KY_UNUSED(panel);
    KY_UNUSED(minimized);
    /* 预留：最小化切换逻辑 */
}

/* 预留扩展：可配置回调 */
static void viewport_panel_config(kyEditorPanel *panel, void *user) {
    KY_UNUSED(panel);
    KY_UNUSED(user);
    /* 预留：配置面板 */
}

/* 预留扩展：拖拽排序回调 */
static void viewport_panel_reorder(kyEditorPanel *panel, int new_index, void *user) {
    KY_UNUSED(panel);
    KY_UNUSED(new_index);
    KY_UNUSED(user);
    /* 预留：拖拽排序逻辑 */
}

/* 视口面板虚表 */
static const kyEditorPanelVtbl viewport_panel_vtbl = {
    /* 必需接口 */
    .name = "Viewport",
    .icon = "▣",  /* 预留：使用Unicode图标 */
    .init = viewport_panel_init,
    .destroy = viewport_panel_destroy,
    .update = viewport_panel_update,
    .draw = viewport_panel_draw,
    .toggle = viewport_panel_toggle,

    /* 可选接口 */
        .config = (void*)viewport_panel_config,
        .reorder = (void*)viewport_panel_reorder,

    /* 预留扩展字段 */
    .reserved = {0}
};

/* 创建视口面板 */
KY_API kyEditorPanel *ky_editor_panel_create_viewport(void) {
    return ky_editor_panel_create(&viewport_panel_vtbl, NULL);
}
