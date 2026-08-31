#include <kronyx/editor.h>
#include <stdio.h>

int main(void) {
    printf("Testing Kronyx Editor API...\n\n");

    /* 测试编辑器配置 */
    printf("1. Testing editor config creation...\n");
    kyEditorConfig *cfg = ky_editor_config_create();
    if (cfg) {
        printf("   ✓ Editor config created successfully\n");
        printf("   - VSync: %d\n", cfg->vsync);
        printf("   - Log level: %d\n", cfg->log_level);
        ky_editor_config_destroy(cfg);
        printf("   ✓ Editor config destroyed\n\n");
    } else {
        printf("   ✗ Failed to create editor config\n\n");
        return 1;
    }

    /* 测试编辑器钩子 */
    printf("2. Testing editor hooks...\n");
    kyEditorHooks *hooks = ky_editor_hooks_create();
    if (hooks) {
        printf("   ✓ Editor hooks created successfully\n");
        ky_editor_hooks_destroy(hooks);
        printf("   ✓ Editor hooks destroyed\n\n");
    } else {
        printf("   ✗ Failed to create editor hooks\n\n");
        return 1;
    }

    /* 测试面板创建 */
    printf("3. Testing panel creation...\n");
    kyEditorPanel *viewport = ky_editor_panel_create_viewport();
    kyEditorPanel *hierarchy = ky_editor_panel_create_hierarchy();
    kyEditorPanel *properties = ky_editor_panel_create_properties();
    kyEditorPanel *console = ky_editor_panel_create_console();

    if (viewport && hierarchy && properties && console) {
        printf("   ✓ All panels created successfully\n");
        printf("   - Viewport: %s\n", viewport->vtbl->name);
        printf("   - Hierarchy: %s\n", hierarchy->vtbl->name);
        printf("   - Properties: %s\n", properties->vtbl->name);
        printf("   - Console: %s\n", console->vtbl->name);
    } else {
        printf("   ✗ Failed to create panels\n");
        if (viewport) ky_editor_panel_destroy(viewport);
        if (hierarchy) ky_editor_panel_destroy(hierarchy);
        if (properties) ky_editor_panel_destroy(properties);
        if (console) ky_editor_panel_destroy(console);
        return 1;
    }

    /* 测试面板销毁 */
    printf("   Destroying panels...\n");
    ky_editor_panel_destroy(viewport);
    ky_editor_panel_destroy(hierarchy);
    ky_editor_panel_destroy(properties);
    ky_editor_panel_destroy(console);
    printf("   ✓ All panels destroyed\n\n");

    /* 测试控制台日志 */
    printf("4. Testing console logging...\n");
    kyEditorPanel *console_test = ky_editor_panel_create_console();
    if (console_test) {
        ky_editor_console_info(console_test, "Test info message", __FILE__, __LINE__);
        ky_editor_console_warning(console_test, "Test warning message", __FILE__, __LINE__);
        ky_editor_console_error(console_test, "Test error message", __FILE__, __LINE__);
        printf("   ✓ Console log functions work\n");
        ky_editor_panel_destroy(console_test);
    } else {
        printf("   ✗ Failed to create console panel for testing\n\n");
        return 1;
    }

    /* 测试编辑器创建（不运行） */
    printf("5. Testing editor creation...\n");
    kyEditorConfig *editor_cfg = ky_editor_config_create();
    if (editor_cfg) {
        kyEditor *editor = ky_editor_create(editor_cfg, KY_RENDERER_GL, NULL);
        if (editor) {
            printf("   ✓ Editor created successfully\n");
            printf("   - Backend: %s\n", ky_editor_get_backend_name(editor));
            printf("   - Valid: %s\n", ky_editor_is_valid(editor) ? "yes" : "no");
            ky_editor_destroy(editor);
            printf("   ✓ Editor destroyed\n\n");
        } else {
            ky_editor_config_destroy(editor_cfg);
            printf("   ✗ Failed to create editor\n\n");
            return 1;
        }
    } else {
        printf("   ✗ Failed to create editor config\n\n");
        return 1;
    }

    printf("=== All editor tests passed! ===\n");
    return 0;
}
