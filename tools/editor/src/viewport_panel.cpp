#include <imgui.h>
#include "kronyx/editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * 视口面板实现（C++）
 * ============================================ */
class ViewportPanel {
public:
    ViewportPanel() 
        : width(800), height(600), texture_id(0), 
          show_grid(true), show_axes(true), show_gizmo(false),
          zoom(1.0f), pan_x(0), pan_y(0),
          selected_entity(0xFFFFFFFF) {
        /* 初始化相机参数 */
        camera.pos[0] = 0.0f;
        camera.pos[1] = 0.0f;
        camera.pos[2] = 10.0f;
        camera.target[0] = 0.0f;
        camera.target[1] = 0.0f;
        camera.target[2] = 0.0f;
        camera.up[0] = 0.0f;
        camera.up[1] = 1.0f;
        camera.up[2] = 0.0f;
        camera.fov = 60.0f;
        camera.near_plane = 0.1f;
        camera.far_plane = 1000.0f;
    }

    ~ViewportPanel() {}

    void init() {
        /* 初始化逻辑 */
    }

    void destroy() {
        /* 清理逻辑 */
    }

    void update(float dt) {
        KY_UNUSED(dt);
        /* 更新逻辑 */
    }

    void draw() {
        if (ImGui::BeginChild("Viewport", ImVec2(0, 0), true)) {
            /* 顶部工具栏 */
            if (ImGui::BeginMenuBar()) {
                if (ImGui::MenuItem("Grid")) {
                    show_grid = !show_grid;
                }
                ImGui::SameLine();
                if (ImGui::MenuItem("Axes")) {
                    show_axes = !show_axes;
                }
                ImGui::SameLine();
                if (ImGui::MenuItem("Gizmo")) {
                    show_gizmo = !show_gizmo;
                }
                ImGui::EndMenuBar();
            }

            /* 视口内容区域 */
            ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
            ImVec2 viewport_size = ImGui::GetContentRegionAvail();
            
            if (viewport_size.x > 0 && viewport_size.y > 0) {
                /* 更新视口尺寸 */
                if ((int)viewport_size.x != width || (int)viewport_size.y != height) {
                    width = (int)viewport_size.x;
                    height = (int)viewport_size.y;
                    /* 预留：通知RHI重新调整FBO */
                }

                /* 绘制背景 */
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(viewport_pos, viewport_pos + viewport_size, 
                    IM_COL32(32, 32, 32, 255));

                /* 绘制网格 */
                if (show_grid) {
                    draw_grid(viewport_pos, viewport_size);
                }

                /* 绘制坐标轴 */
                if (show_axes) {
                    draw_axes(viewport_pos, viewport_size);
                }

                /* 绘制Gizmo（预留） */
                if (show_gizmo && selected_entity != 0xFFFFFFFF) {
                    /* 预留：绘制变换Gizmo */
                }

                /* 绘制十字准星 */
                ImVec2 center = viewport_pos + viewport_size * 0.5f;
                float cross_size = 10.0f;
                draw_list->AddLine(ImVec2(center.x - cross_size, center.y), 
                    ImVec2(center.x + cross_size, center.y), 
                    IM_COL32(255, 255, 255, 128), 1.0f);
                draw_list->AddLine(ImVec2(center.x, center.y - cross_size), 
                    ImVec2(center.x, center.y + cross_size), 
                    IM_COL32(255, 255, 255, 128), 1.0f);

                /* 统计信息 */
                char stats[128];
                snprintf(stats, sizeof(stats), "W: %d H: %d | Zoom: %.1fx | Entity: 0x%08X",
                    width, height, zoom, selected_entity);
                draw_list->AddText(viewport_pos + ImVec2(4, 4), 
                    IM_COL32(200, 200, 200, 255), stats);
            }
        }
        ImGui::EndChild();
    }

    void toggle(bool *minimized) {
        KY_UNUSED(minimized);
        /* 预留：最小化切换 */
    }

private:
    void draw_grid(ImVec2 pos, ImVec2 size) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float grid_size = 50.0f;
        int lines = 20;
        
        ImVec2 start = pos;
        ImVec2 end = pos + size;
        
        /* 垂直线 */
        for (int i = -lines; i <= lines; i++) {
            float x = start.x + (i * grid_size / lines) * (size.x / 2.0f) + size.x / 2.0f;
            float alpha = (i == 0) ? 200 : 80;
            draw_list->AddLine(ImVec2(x, start.y), ImVec2(x, end.y),
                IM_COL32(255, 255, 255, alpha), 1.0f);
        }
        
        /* 水平线 */
        for (int i = -lines; i <= lines; i++) {
            float y = start.y + (i * grid_size / lines) * (size.y / 2.0f) + size.y / 2.0f;
            float alpha = (i == 0) ? 200 : 80;
            draw_list->AddLine(ImVec2(start.x, y), ImVec2(end.x, y),
                IM_COL32(255, 255, 255, alpha), 1.0f);
        }
    }

    void draw_axes(ImVec2 pos, ImVec2 size) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 center = pos + size * 0.5f;
        float axis_len = 30.0f;
        
        /* X轴 - 红色 */
        draw_list->AddLine(center, center + ImVec2(axis_len, 0), 
            IM_COL32(255, 80, 80, 255), 2.0f);
        draw_list->AddText(center + ImVec2(axis_len + 4, -8), 
            IM_COL32(255, 80, 80, 255), "X");
        
        /* Y轴 - 绿色 */
        draw_list->AddLine(center, center + ImVec2(0, -axis_len), 
            IM_COL32(80, 255, 80, 255), 2.0f);
        draw_list->AddText(center + ImVec2(4, -axis_len - 4), 
            IM_COL32(80, 255, 80, 255), "Y");
        
        /* Z轴 - 蓝色 */
        draw_list->AddLine(center, center + ImVec2(-axis_len * 0.5f, axis_len * 0.5f), 
            IM_COL32(80, 80, 255, 255), 2.0f);
        draw_list->AddText(center + ImVec2(-axis_len * 0.5f - 12, axis_len * 0.5f + 4), 
            IM_COL32(80, 80, 255, 255), "Z");
    }

    int width, height;
    int texture_id;
    bool show_grid, show_axes, show_gizmo;
    float zoom, pan_x, pan_y;
    uint32_t selected_entity;
    
    struct {
        float pos[3];
        float target[3];
        float up[3];
        float fov, near_plane, far_plane;
    } camera;
};

/* 视口面板虚表实现 */
static const kyEditorPanelVtbl viewport_panel_vtbl = {
    .name = "Viewport",
    .icon = "\xef\x83\xa1",  /* Unicode box icon */
    .init = [](kyEditorPanel *panel, void *user) -> void {
        KY_UNUSED(user);
        ViewportPanel *impl = new ViewportPanel();
        impl->init();
        panel->impl = impl;
    },
    .destroy = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            ViewportPanel *vp = static_cast<ViewportPanel*>(panel->impl);
            vp->destroy();
            delete vp;
            panel->impl = nullptr;
        }
    },
    .update = [](kyEditorPanel *panel, float dt) -> void {
        if (panel && panel->impl) {
            ViewportPanel *vp = static_cast<ViewportPanel*>(panel->impl);
            vp->update(dt);
        }
    },
    .draw = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            ViewportPanel *vp = static_cast<ViewportPanel*>(panel->impl);
            vp->draw();
        }
    },
    .toggle = [](kyEditorPanel *panel, bool *minimized) -> void {
        if (panel && panel->impl) {
            ViewportPanel *vp = static_cast<ViewportPanel*>(panel->impl);
            vp->toggle(minimized);
        }
    },
    .config = nullptr,
    .reorder = nullptr,
    .reserved = {0}
};

KY_API kyEditorPanel *ky_editor_panel_create_viewport(void) {
    return ky_editor_panel_create(&viewport_panel_vtbl, nullptr);
}
