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

        /* 预留：工具栏 */
