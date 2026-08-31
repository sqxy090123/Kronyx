#include <imgui.h>
#include "kronyx/editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * 属性面板实现（C++）
 * ============================================ */
class PropertiesPanel {
public:
    PropertiesPanel() : current_entity(0xFFFFFFFF), property_capacity(16) {
        properties = static_cast<Property*>(calloc(property_capacity, sizeof(Property)));
    }

    ~PropertiesPanel() {
        for (size_t i = 0; i < property_count; i++) {
            free((void *)properties[i].name);
        }
        free(properties);
    }

    void init() {}
    void destroy() {}

    void update(float dt) {
        KY_UNUSED(dt);
    }

    void add_property(const char *name, const char *label, void *value, size_t size, int type) {
        if (property_count >= property_capacity) {
            property_capacity *= 2;
            properties = static_cast<Property*>(realloc(properties, property_capacity * sizeof(Property)));
        }
        properties[property_count].name = strdup(name);
        properties[property_count].label = label;
        properties[property_count].value = value;
        properties[property_count].size = size;
        properties[property_count].type = type;
        property_count++;
    }

    void clear_properties() {
        for (size_t i = 0; i < property_count; i++) {
            free((void *)properties[i].name);
        }
        property_count = 0;
    }

    void draw() {
        if (ImGui::BeginChild("Properties", ImVec2(0, 0), true)) {
            if (current_entity == 0xFFFFFFFF) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entity selected");
                ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Select an entity from Hierarchy");
            } else {
                char title[64];
                snprintf(title, sizeof(title), "Entity: 0x%08X", current_entity);
                
                if (ImGui::CollapsingHeader(title)) {
                    /* Transform 组件 */
                    if (ImGui::TreeNode("Transform")) {
                        /* 位置 */
                        ImGui::Text("Position");
                        static float pos[3] = {0, 0, 0};
                        ImGui::InputFloat3("##pos", pos);
                        
                        /* 旋转 */
                        ImGui::Text("Rotation");
                        static float rot[3] = {0, 0, 0};
                        ImGui::InputFloat3("##rot", rot);
                        
                        /* 缩放 */
                        ImGui::Text("Scale");
                        static float scale[3] = {1, 1, 1};
                        ImGui::InputFloat3("##scale", scale);
                        
                        ImGui::TreePop();
                    }
                    
                    /* RigidBody 组件 */
                    if (ImGui::TreeNode("RigidBody")) {
                        static bool dynamic = true;
                        ImGui::Checkbox("Dynamic", &dynamic);
                        
                        static float mass = 1.0f;
                        ImGui::SliderFloat("Mass", &mass, 0.1f, 100.0f);
                        
                        static float friction = 0.5f;
                        ImGui::SliderFloat("Friction", &friction, 0.0f, 1.0f);
                        
                        static float restitution = 0.3f;
                        ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f);
                        
                        ImGui::TreePop();
                    }
                    
                    /* 预留：其他组件 */
                    if (ImGui::TreeNode("Components")) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Add components via Hierarchy");
                        ImGui::TreePop();
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    void toggle(bool *minimized) {
        KY_UNUSED(minimized);
    }

    void set_entity(uint32_t entity) {
        current_entity = entity;
        clear_properties();
        
        if (entity != 0xFFFFFFFF) {
            /* 预留：从ECS获取组件并添加到属性列表 */
        }
    }

private:
    struct Property {
        char *name;
        const char *label;
        void *value;
        size_t size;
        int type;
    };
    
    uint32_t current_entity;
    Property *properties;
    size_t property_count;
    size_t property_capacity;
};

/* 属性面板虚表实现 */
static const kyEditorPanelVtbl properties_panel_vtbl = {
    .name = "Properties",
    .icon = "\xe2\x99\x99",  /* Gear icon */
    .init = [](kyEditorPanel *panel, void *user) -> void {
        KY_UNUSED(user);
        PropertiesPanel *impl = new PropertiesPanel();
        impl->init();
        panel->impl = impl;
    },
    .destroy = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            PropertiesPanel *pp = static_cast<PropertiesPanel*>(panel->impl);
            pp->destroy();
            delete pp;
            panel->impl = nullptr;
        }
    },
    .update = [](kyEditorPanel *panel, float dt) -> void {
        if (panel && panel->impl) {
            PropertiesPanel *pp = static_cast<PropertiesPanel*>(panel->impl);
            pp->update(dt);
        }
    },
    .draw = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            PropertiesPanel *pp = static_cast<PropertiesPanel*>(panel->impl);
            pp->draw();
        }
    },
    .toggle = [](kyEditorPanel *panel, bool *minimized) -> void {
        if (panel && panel->impl) {
            PropertiesPanel *pp = static_cast<PropertiesPanel*>(panel->impl);
            pp->toggle(minimized);
        }
    },
    .config = nullptr,
    .reorder = nullptr,
    .reserved = {0}
};

KY_API kyEditorPanel *ky_editor_panel_create_properties(void) {
    return ky_editor_panel_create(&properties_panel_vtbl, nullptr);
}
