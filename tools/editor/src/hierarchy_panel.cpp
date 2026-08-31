#include <imgui.h>
#include "kronyx/editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * 层级面板实现（C++）
 * ============================================ */
class HierarchyPanel {
public:
    HierarchyPanel() : selected_entity(0xFFFFFFFF), node_capacity(16), node_count(0) {
        entity_nodes = (void **)calloc(node_capacity, sizeof(void *));
    }

    ~HierarchyPanel() {
        free(entity_nodes);
    }

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
        if (ImGui::BeginChild("Hierarchy", ImVec2(0, 0), true)) {
            /* 预留：绘制实体树 */
            if (node_count == 0) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entities");
            }
        }
        ImGui::EndChild();
    }

    void toggle(bool *minimized) {
        KY_UNUSED(minimized);
        /* 预留：最小化切换 */
    }

    uint32_t selected_entity;
    void **entity_nodes;
    size_t node_capacity;
    size_t node_count;
};

/* 层级面板虚表实现 */
static const kyEditorPanelVtbl hierarchy_panel_vtbl = {
    .name = "Hierarchy",
    .icon = "##",
    .init = [](kyEditorPanel *panel, void *user) -> void {
        KY_UNUSED(user);
        HierarchyPanel *impl = new HierarchyPanel();
        impl->init();
        panel->impl = impl;
    },
    .destroy = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            HierarchyPanel *hp = static_cast<HierarchyPanel*>(panel->impl);
            hp->destroy();
            delete hp;
            panel->impl = nullptr;
        }
    },
    .update = [](kyEditorPanel *panel, float dt) -> void {
        if (panel && panel->impl) {
            HierarchyPanel *hp = static_cast<HierarchyPanel*>(panel->impl);
            hp->update(dt);
        }
    },
    .draw = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            HierarchyPanel *hp = static_cast<HierarchyPanel*>(panel->impl);
            hp->draw();
        }
    },
    .toggle = [](kyEditorPanel *panel, bool *minimized) -> void {
        if (panel && panel->impl) {
            HierarchyPanel *hp = static_cast<HierarchyPanel*>(panel->impl);
            hp->toggle(minimized);
        }
    },
    .config = nullptr,
    .reorder = nullptr,
    .reserved = {0}
};

KY_API kyEditorPanel *ky_editor_panel_create_hierarchy(void) {
    return ky_editor_panel_create(&hierarchy_panel_vtbl, nullptr);
}
