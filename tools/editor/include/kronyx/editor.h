#ifndef KRONYX_EDITOR_H
#define KRONYX_EDITOR_H

#include "kronyx/script.h"
#include "kronyx/render.h"
#include "kronyx/ecs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 编辑器版本与兼容性
 * ============================================ */
#define KY_EDITOR_VERSION_MAJOR 1
#define KY_EDITOR_VERSION_MINOR 0
#define KY_EDITOR_VERSION_PATCH 0

/* 兼容性标记 - 用于未来版本迁移 */
typedef enum kyEditorCompatibility {
    KY_EDITOR_COMPAT_NONE = 0,
    KY_EDITOR_COMPAT_V1 = 1,
    KY_EDITOR_COMPAT_CURRENT = KY_EDITOR_COMPAT_V1
} kyEditorCompatibility;

/* ============================================
 * 编辑器配置结构（可扩展）
 * ============================================ */
typedef struct kyEditorConfig {
    int vsync;               /* 垂直同步: 0=off, 1=on */
    int log_level;           /* 日志级别: 0=ERROR, 1=WARNING, 2=INFO, 3=DEBUG */
    void *reserved[64];      /* 预留扩展字段 */
} kyEditorConfig;

KY_API kyEditorConfig *ky_editor_config_create(void);
KY_API void ky_editor_config_destroy(kyEditorConfig *cfg);

/* 可配置项 - 使用函数指针实现灵活扩展 */
/* typedef void (*kyEditorConfigCallback)(kyEditorConfig *cfg, void *user); */

/* ============================================
 * 编辑器钩子接口（事件回调）
 * ============================================ */
typedef struct kyEditorHooks kyEditorHooks;

KY_API kyEditorHooks *ky_editor_hooks_create(void);
KY_API void ky_editor_hooks_destroy(kyEditorHooks *hooks);

/* 预留扩展：回调类型枚举 */
typedef enum kyEditorCallbackType {
    KY_EDITOR_CB_NONE = 0,

    /* 视口回调 */
    KY_EDITOR_CB_VIEWPORT_DRAW = 1,
    KY_EDITOR_CB_VIEWPORT_SIZE_CHANGED = 2,
    KY_EDITOR_CB_VIEWPORT_GIZMO_CHANGED = 3,

    /* 实体选择回调 */
    KY_EDITOR_CB_ENTITY_SELECTED = 4,
    KY_EDITOR_CB_ENTITY_DESELECTED = 5,
    KY_EDITOR_CB_ENTITY_TREE_CHANGED = 6,

    /* 属性变更回调 */
    KY_EDITOR_CB_PROPERTY_CHANGED = 7,
    KY_EDITOR_CB_COMPONENT_ADDED = 8,
    KY_EDITOR_CB_COMPONENT_REMOVED = 9,

    /* 脚本回调 */
    KY_EDITOR_CB_SCRIPT_LOADED = 10,
    KY_EDITOR_CB_SCRIPT_ERROR = 11,
    KY_EDITOR_CB_SCRIPT_HOT_RELOAD = 12,

    /* 控制台回调 */
    KY_EDITOR_CB_LOG_MESSAGE = 13,
    KY_EDITOR_CB_LOG_ERROR = 14,
    KY_EDITOR_CB_LOG_WARNING = 15,

    /* 性能监控回调 */
    KY_EDITOR_CB_PERF_FRAME = 16,
    KY_EDITOR_CB_PERF_MEMORY = 17,

    /* 插件回调 */
    KY_EDITOR_CB_PLUGIN_LOADED = 18,
    KY_EDITOR_CB_PLUGIN_UNLOADED = 19,

    KY_EDITOR_CB_MAX
} kyEditorCallbackType;

/* 通用回调函数类型 */
typedef void (*kyEditorCallback)(kyEditorHooks *hooks, const void *event_data, void *user);

/* 注册回调 - 使用类型+函数指针实现扩展 */
KY_API void ky_editor_hooks_register(kyEditorHooks *hooks, kyEditorCallbackType type, kyEditorCallback cb, void *user);
KY_API void ky_editor_hooks_unregister(kyEditorHooks *hooks, kyEditorCallbackType type, kyEditorCallback cb);

/* ============================================
 * 编辑器主接口
 * ============================================ */
typedef struct kyEditor kyEditor;

KY_API kyEditor *ky_editor_create(kyEditorConfig *cfg, kyRendererBackend backend, void *platform_win);
KY_API void ky_editor_destroy(kyEditor *editor);

/* 编辑器主循环 */
KY_API void ky_editor_run(kyEditor *editor);

/* 编辑器控制 */
KY_API void ky_editor_stop(kyEditor *editor);
KY_API bool ky_editor_is_running(const kyEditor *editor);

/* 资源访问接口（预留扩展） */
KY_API kyRendererBackend ky_editor_get_backend(const kyEditor *editor);
KY_API const char *ky_editor_get_backend_name(const kyEditor *editor);

/* ============================================
 * 面板系统接口（模块化设计）
 * ============================================ */
typedef struct kyEditorPanel kyEditorPanel;

/* 面板初始化/销毁回调 */
typedef void (*kyEditorPanelInit)(kyEditorPanel *panel, void *user);
typedef void (*kyEditorPanelDestroy)(kyEditorPanel *panel);

/* 面板更新回调（每帧调用） */
typedef void (*kyEditorPanelUpdate)(kyEditorPanel *panel, float dt);

/* 面板绘制回调（ImGui调用） */
typedef void (*kyEditorPanelDraw)(kyEditorPanel *panel);

/* 面板最小化/最大化回调 */
typedef void (*kyEditorPanelToggle)(kyEditorPanel *panel, bool *minimized);

/* 预留：面板拖拽排序回调 */
typedef void (*kyEditorPanelReorder)(kyEditorPanel *panel, int new_index, void *user);

/* 面板接口结构（类似虚表） */
typedef struct kyEditorPanelVtbl {
    /* 必需接口 */
    const char *name;
    const char *icon;
    kyEditorPanelInit init;
    kyEditorPanelDestroy destroy;
    kyEditorPanelUpdate update;
    kyEditorPanelDraw draw;
    kyEditorPanelToggle toggle;

    /* 可选接口 - 未来扩展 */
    void *config;     /* C++: kyEditorPanelConfig */
    void *reorder;    /* C++: kyEditorPanelReorder */
    void *reserved[14];  /* 预留扩展空间 */
} kyEditorPanelVtbl;

/* 面板基类结构 */
struct kyEditorPanel {
    const kyEditorPanelVtbl *vtbl;      /* 虚表 */
    void *impl;                         /* 实现数据 */
    bool is_active;                     /* 是否激活 */
    bool is_minimized;                  /* 是否最小化 */
    void *user_data;                    /* 用户自定义数据 */
};

/* 创建面板实例 */
KY_API kyEditorPanel *ky_editor_panel_create(const kyEditorPanelVtbl *vtbl, void *user);
KY_API void ky_editor_panel_destroy(kyEditorPanel *panel);

/* 创建内置面板 */
KY_API kyEditorPanel *ky_editor_panel_create_viewport(void);
KY_API kyEditorPanel *ky_editor_panel_create_hierarchy(void);
KY_API kyEditorPanel *ky_editor_panel_create_properties(void);
KY_API kyEditorPanel *ky_editor_panel_create_console(void);

/* 更新面板 */
KY_API void ky_editor_panel_update(kyEditorPanel *panel, float dt);

/* 绘制面板 */
KY_API void ky_editor_panel_draw(kyEditorPanel *panel);

/* 面板控制 */
KY_API void ky_editor_panel_toggle_minimize(kyEditorPanel *panel);
KY_API bool ky_editor_panel_is_minimized(const kyEditorPanel *panel);

/* ============================================
 * 面板注册系统
 * ============================================ */
KY_API void ky_editor_panel_register(kyEditor *editor, kyEditorPanel *panel);
KY_API void ky_editor_panel_unregister(kyEditor *editor, kyEditorPanel *panel);

/* ============================================
 * 插件系统接口（预留扩展）
 * ============================================ */
typedef struct kyEditorPlugin kyEditorPlugin;

/* 插件接口 */
typedef struct kyEditorPluginVtbl {
    const char *name;
    const char *version;
    void *init;       /* C++: kyEditorPluginInit */
    void (*destroy)(kyEditorPlugin *plugin);
    void *reserved[15];  /* 预留扩展空间 */
} kyEditorPluginVtbl;

struct kyEditorPlugin {
    const kyEditorPluginVtbl *vtbl;
    void *impl;
    bool is_loaded;
    void *user_data;
};

KY_API kyEditorPlugin *ky_editor_plugin_create(const kyEditorPluginVtbl *vtbl, void *user);
KY_API void ky_editor_plugin_destroy(kyEditorPlugin *plugin);

KY_API bool ky_editor_plugin_load(kyEditor *editor, kyEditorPlugin *plugin);
KY_API void ky_editor_plugin_unload(kyEditor *editor, kyEditorPlugin *plugin);

KY_API void ky_editor_plugin_register_all(kyEditor *editor);
KY_API void ky_editor_plugin_unregister_all(kyEditor *editor);

/* ============================================
 * 工具函数（预留扩展）
 * ============================================ */
KY_API bool ky_editor_is_valid(const kyEditor *editor);

/* ============================================
 * 控制台面板日志接口
 * ============================================ */
KY_API void ky_editor_console_info(kyEditorPanel *console, const char *msg, const char *file, int line);
KY_API void ky_editor_console_warning(kyEditorPanel *console, const char *msg, const char *file, int line);
KY_API void ky_editor_console_error(kyEditorPanel *console, const char *msg, const char *file, int line);

/* ============================================
 * 回调类型定义（C++兼容）
 * ============================================ */
#ifdef __cplusplus
/* 预留扩展：面板配置回调类型 */
typedef void (*kyEditorPanelConfig)(kyEditorPanel *panel, void *user);

/* 预留扩展：插件初始化回调类型 */
typedef bool (*kyEditorPluginInit)(kyEditorConfig *cfg, void *user);
#endif

#ifdef __cplusplus
}
#endif

#endif /* KRONYX_EDITOR_H */
