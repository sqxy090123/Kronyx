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
    HierarchyPanel() 
        : selected_entity(0xFFFFFFFF), 
          node_capacity(16), node_count(0) {
        entity_nodes = (void **)calloc(node_capacity, sizeof(void *));
        entity_names = (const char **)calloc(node_capacity, sizeof(const char *));
    }

    ~HierarchyPanel() {
        free(entity_nodes);
        free(entity_names);
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

    void add_entity(uint32_t entity, const char *name) {
        if (node_count >= node_capacity) {
            node_capacity *= 2;
            entity_nodes = (void **)realloc(entity_nodes, node_capacity * sizeof(void *));
            entity_names = (const char **)realloc(entity_names, node_capacity * sizeof(const char *));
        }
        entity_nodes[node_count] = (void *)(uintptr_t)entity;
        entity_names[node_count] = name ? strdup(name) : strdup("Untitled");
        node_count++;
    }

    void remove_entity(uint32_t entity) {
        for (size_t i = 0; i < node_count; i++) {
            if ((uint32_t)(uintptr_t)entity_nodes[i] == entity) {
                free((void *)entity_names[i]);
                entity_nodes[i] = entity_nodes[node_count - 1];
                entity_names[i] = entity_names[node_count - 1];
                node_count--;
                break;
            }
        }
    }

    void draw() {
        if (ImGui::BeginChild("Hierarchy", ImVec2(0, 0), true)) {
            /* 工具栏 */
            if (ImGui::Button("Refresh")) {
                /* 预留：刷新实体列表 */
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Entity")) {
                /* 预留：创建新实体 */
            }
            ImGui::Separator();

            /* 实体树 */
            if (node_count == 0) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entities");
                ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Use Hierarchy > Add Entity");
            } else {
                for (size_t i = 0; i < node_count; i++) {
                    uint32_t entity = (uint32_t)(uintptr_t)entity_nodes[i];
                    const char *name = entity_names[i];
                    
                    bool is_selected = (entity == selected_entity);
                    ImVec4 text_color = is_selected ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
                    
                    if (ImGui::Selectable(name, is_selected)) {
                        selected_entity = entity;
                        /* 触发选择事件 */
                    }
                    
                    /* 右键菜单 */
                    if (ImGui::BeginPopupContextItem("EntityCtx")) {
                        if (ImGui::MenuItem("Delete")) {
                            remove_entity(entity);
                            if (selected_entity == entity) {
                                selected_entity = 0xFFFFFFFF;
                            }
                        }
                        if (ImGui::MenuItem("Duplicate")) {
                            /* 预留：复制实体 */
                        }
                        ImGui::EndPopup();
                    }
                }
            }
            
            ImGui::Separator();
            
            /* 选中实体信息 */
            if (selected_entity != 0xFFFFFFFF) {
                char info[64];
                snprintf(info, sizeof(info), "Selected: 0x%08X", selected_entity);
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), info);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entity selected");
            }
        }
        ImGui::EndChild();
    }

    void toggle(bool *minimized) {
        KY_UNUSED(minimized);
        /* 预留：最小化切换 */
    }

    uint32_t get_selected() const { return selected_entity; }

private:
    uint32_t selected_entity;
    void **entity_nodes;
    const char **entity_names;
    size_t node_capacity;
    size_t node_count;
};

/* 层级面板虚表实现 */
static const kyEditorPanelVtbl hierarchy_panel_vtbl = {
    .name = "Hierarchy",
    .icon = "\xef\x83\x84",  /* Unicode list icon */
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
