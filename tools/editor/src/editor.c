#include "kronyx/editor.h"

/* 完整定义kyEditorHooks结构体（避免不完整typedef错误） */
struct kyEditorHooks {
    struct {
        kyEditorCallbackType type;
        kyEditorCallback cb;
        void *user;
    } callbacks[KY_EDITOR_CB_MAX];
};

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * 内部状态管理
 * ============================================ */
typedef struct kyEditorImpl kyEditorImpl;

struct kyEditorImpl {
    /* 配置 */
    kyEditorConfig *config;

    /* 渲染后端 */
    kyRendererBackend backend;
    void *platform_win;

    /* 运行状态 */
    bool is_running;
    bool is_paused;

    /* 面板列表 */
    kyEditorPanel **panels;
    size_t panel_count;
    size_t panel_capacity;

    /* 插件列表 */
    kyEditorPlugin **plugins;
    size_t plugin_count;
    size_t plugin_capacity;

    /* 事件系统 */
    struct {
        kyEditorCallbackType type;
        kyEditorCallback cb;
        void *user;
    } callbacks[KY_EDITOR_CB_MAX];

    /* 预留扩展字段 */
    void *reserved[64];
};

/* ============================================
 * 编辑器配置实现
 * ============================================ */
kyEditorConfig *ky_editor_config_create(void) {
    kyEditorConfig *cfg = (kyEditorConfig *)malloc(sizeof(kyEditorConfig));
    if (!cfg) return NULL;

    memset(cfg, 0, sizeof(kyEditorConfig));

    /* 默认配置值 */
    cfg->vsync = 1;
    cfg->log_level = 2;  /* 0=ERROR, 1=WARNING, 2=INFO */

    return cfg;
}

void ky_editor_config_destroy(kyEditorConfig *cfg) {
    if (cfg) {
        /* 预留：释放配置相关的资源 */
        free(cfg);
    }
}

/* ============================================
 * 编辑器钩子实现
 * ============================================ */
kyEditorHooks *ky_editor_hooks_create(void) {
    kyEditorHooks *hooks = (kyEditorHooks *)malloc(sizeof(kyEditorHooks));
    if (!hooks) return NULL;

    memset(hooks, 0, sizeof(kyEditorHooks));
    return hooks;
}

void ky_editor_hooks_destroy(kyEditorHooks *hooks) {
    if (hooks) {
        free(hooks);
    }
}

void ky_editor_hooks_register(kyEditorHooks *hooks, kyEditorCallbackType type, kyEditorCallback cb, void *user) {
    if (!hooks || type >= KY_EDITOR_CB_MAX) return;
    hooks->callbacks[type].type = type;
    hooks->callbacks[type].cb = cb;
    hooks->callbacks[type].user = user;
}

void ky_editor_hooks_unregister(kyEditorHooks *hooks, kyEditorCallbackType type, kyEditorCallback cb) {
    if (!hooks || type >= KY_EDITOR_CB_MAX) return;
    if (hooks->callbacks[type].cb == cb) {
        hooks->callbacks[type].cb = NULL;
        hooks->callbacks[type].user = NULL;
    }
}

/* ============================================
 * 编辑器实现
 * ============================================ */
kyEditor *ky_editor_create(kyEditorConfig *cfg, kyRendererBackend backend, void *platform_win) {
    kyEditorImpl *impl = (kyEditorImpl *)malloc(sizeof(kyEditorImpl));
    if (!impl) return NULL;

    memset(impl, 0, sizeof(kyEditorImpl));

    /* 保存配置 */
    impl->config = cfg;
    impl->backend = backend;
    impl->platform_win = platform_win;

    /* 初始化运行状态 */
    impl->is_running = false;
    impl->is_paused = false;

    /* 初始化面板数组（预留容量） */
    impl->panel_capacity = 16;
    impl->panels = (kyEditorPanel **)calloc(impl->panel_capacity, sizeof(kyEditorPanel *));
    if (!impl->panels) {
        free(impl);
        return NULL;
    }

    /* 初始化插件数组（预留容量） */
    impl->plugin_capacity = 8;
    impl->plugins = (kyEditorPlugin **)calloc(impl->plugin_capacity, sizeof(kyEditorPlugin *));
    if (!impl->plugins) {
        free(impl->panels);
        free(impl);
        return NULL;
    }

    /* 预留扩展字段初始化 */
    memset(impl->reserved, 0, sizeof(impl->reserved));

    return (kyEditor *)impl;
}

void ky_editor_destroy(kyEditor *editor) {
    if (!editor) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    /* 停止编辑器 */
    if (impl->is_running) {
        ky_editor_stop(editor);
    }

    /* 销毁面板 */
    for (size_t i = 0; i < impl->panel_count; i++) {
        if (impl->panels[i]) {
            if (impl->panels[i]->vtbl && impl->panels[i]->vtbl->destroy) {
                impl->panels[i]->vtbl->destroy(impl->panels[i]);
            }
            free(impl->panels[i]);
        }
    }
    free(impl->panels);

    /* 销毁插件 */
    for (size_t i = 0; i < impl->plugin_count; i++) {
        if (impl->plugins[i]) {
            if (impl->plugins[i]->vtbl && impl->plugins[i]->vtbl->destroy) {
                impl->plugins[i]->vtbl->destroy(impl->plugins[i]);
            }
            free(impl->plugins[i]);
        }
    }
    free(impl->plugins);

    /* 销毁配置 */
    if (impl->config) {
        ky_editor_config_destroy(impl->config);
    }

    /* 销毁钩子（如果有） */
    /* 预留：编辑器内部不需要管理hooks的生命周期 */

    free(impl);
}

void ky_editor_run(kyEditor *editor) {
    if (!editor) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;
    impl->is_running = true;
    impl->is_paused = false;

    /* 预留：主循环逻辑将在后续实现 */
    /* 目前仅标记为运行状态 */
}

void ky_editor_stop(kyEditor *editor) {
    if (!editor) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;
    impl->is_running = false;
    impl->is_paused = false;
}

bool ky_editor_is_running(const kyEditor *editor) {
    if (!editor) return false;
    const kyEditorImpl *impl = (const kyEditorImpl *)editor;
    return impl->is_running;
}

/* ============================================
 * 面板系统实现
 * ============================================ */
kyEditorPanel *ky_editor_panel_create(const kyEditorPanelVtbl *vtbl, void *user) {
    if (!vtbl || !vtbl->name) return NULL;

    kyEditorPanel *panel = (kyEditorPanel *)malloc(sizeof(kyEditorPanel));
    if (!panel) return NULL;

    memset(panel, 0, sizeof(kyEditorPanel));

    /* 设置虚表 */
    panel->vtbl = vtbl;

    /* 调用初始化回调 */
    if (vtbl->init) {
        vtbl->init(panel, user);
    }

    panel->is_active = true;
    panel->is_minimized = false;
    panel->user_data = user;

    return panel;
}

void ky_editor_panel_destroy(kyEditorPanel *panel) {
    if (!panel) return;

    /* 调用销毁回调 */
    if (panel->vtbl && panel->vtbl->destroy) {
        panel->vtbl->destroy(panel);
    }

    free(panel);
}

void ky_editor_panel_update(kyEditorPanel *panel, float dt) {
    if (!panel || !panel->is_active) return;

    /* 调用更新回调 */
    if (panel->vtbl && panel->vtbl->update) {
        panel->vtbl->update(panel, dt);
    }
}

void ky_editor_panel_draw(kyEditorPanel *panel) {
    if (!panel || !panel->is_active) return;

    /* 调用绘制回调 */
    if (panel->vtbl && panel->vtbl->draw) {
        panel->vtbl->draw(panel);
    }
}

void ky_editor_panel_toggle_minimize(kyEditorPanel *panel) {
    if (!panel) return;

    if (panel->vtbl && panel->vtbl->toggle) {
        panel->vtbl->toggle(panel, &panel->is_minimized);
    } else {
        panel->is_minimized = !panel->is_minimized;
    }
}

bool ky_editor_panel_is_minimized(const kyEditorPanel *panel) {
    if (!panel) return false;
    return panel->is_minimized;
}

void ky_editor_panel_register(kyEditor *editor, kyEditorPanel *panel) {
    if (!editor || !panel) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    /* 扩展数组容量 */
    if (impl->panel_count >= impl->panel_capacity) {
        size_t new_capacity = impl->panel_capacity * 2;
        kyEditorPanel **new_panels = (kyEditorPanel **)realloc(impl->panels, new_capacity * sizeof(kyEditorPanel *));
        if (!new_panels) return;
        impl->panels = new_panels;
        impl->panel_capacity = new_capacity;
    }

    impl->panels[impl->panel_count++] = panel;
}

void ky_editor_panel_unregister(kyEditor *editor, kyEditorPanel *panel) {
    if (!editor || !panel) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    for (size_t i = 0; i < impl->panel_count; i++) {
        if (impl->panels[i] == panel) {
            /* 移动最后一个元素填补空缺 */
            impl->panels[i] = impl->panels[impl->panel_count - 1];
            impl->panels[impl->panel_count - 1] = NULL;
            impl->panel_count--;
            break;
        }
    }
}

/* ============================================
 * 插件系统实现
 * ============================================ */
kyEditorPlugin *ky_editor_plugin_create(const kyEditorPluginVtbl *vtbl, void *user) {
    if (!vtbl || !vtbl->name) return NULL;

    kyEditorPlugin *plugin = (kyEditorPlugin *)malloc(sizeof(kyEditorPlugin));
    if (!plugin) return NULL;

    memset(plugin, 0, sizeof(kyEditorPlugin));

    plugin->vtbl = vtbl;
    plugin->is_loaded = false;
    plugin->user_data = user;

    return plugin;
}

void ky_editor_plugin_destroy(kyEditorPlugin *plugin) {
    if (!plugin) return;

    if (plugin->vtbl && plugin->vtbl->destroy) {
        plugin->vtbl->destroy(plugin);
    }

    free(plugin);
}

bool ky_editor_plugin_load(kyEditor *editor, kyEditorPlugin *plugin) {
    if (!editor || !plugin) return false;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    /* 调用初始化回调 */
    if (plugin->vtbl && plugin->vtbl->init) {
        /* C++: 转换函数指针 */
#ifdef __cplusplus
        typedef bool (*kyEditorPluginInitFn)(kyEditorConfig *cfg, void *user);
        kyEditorPluginInitFn init_fn = (kyEditorPluginInitFn)plugin->vtbl->init;
        if (!init_fn(impl->config, plugin->user_data)) {
            return false;
        }
#else
        /* C: 直接调用 */
        typedef bool (*kyEditorPluginInitFn)(kyEditorConfig *cfg, void *user);
        kyEditorPluginInitFn init_fn = (kyEditorPluginInitFn)plugin->vtbl->init;
        if (!init_fn(impl->config, plugin->user_data)) {
            return false;
        }
#endif
    }

    plugin->is_loaded = true;

    return true;
}

void ky_editor_plugin_unload(kyEditor *editor, kyEditorPlugin *plugin) {
    if (!editor || !plugin || !plugin->is_loaded) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    plugin->is_loaded = false;

    /* 预留：可以调用插件特定的清理逻辑 */
}

void ky_editor_plugin_register_all(kyEditor *editor) {
    if (!editor) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    /* 预留：从插件目录加载插件 */
    /* 当前实现为空，等待后续扩展 */
}

void ky_editor_plugin_unregister_all(kyEditor *editor) {
    if (!editor) return;

    kyEditorImpl *impl = (kyEditorImpl *)editor;

    /* 卸载所有插件 */
    for (size_t i = 0; i < impl->plugin_count; i++) {
        if (impl->plugins[i] && impl->plugins[i]->is_loaded) {
            ky_editor_plugin_unload(editor, impl->plugins[i]);
        }
    }
}

/* ============================================
 * 工具函数实现
 * ============================================ */
bool ky_editor_is_valid(const kyEditor *editor) {
    if (!editor) return false;
    return ((const kyEditorImpl *)editor)->config != NULL;
}

/* ============================================
 * 后端访问接口
 * ============================================ */
kyRendererBackend ky_editor_get_backend(const kyEditor *editor) {
    if (!editor) return KY_RENDERER_NONE;
    return ((const kyEditorImpl *)editor)->backend;
}

const char *ky_editor_get_backend_name(const kyEditor *editor) {
    if (!editor) return "unknown";
    kyRendererBackend backend = ky_editor_get_backend(editor);
    switch (backend) {
        case KY_RENDERER_GL: return "opengl3.3";
        case KY_RENDERER_VULKAN: return "vulkan1.2";
        default: return "none";
    }
}
