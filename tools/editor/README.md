# Kronyx Editor

## 概述

Kronyx Editor 是 Kronyx 游戏引擎的图形化编辑器，用于场景编辑、实体管理、属性检查和调试。

## 设计理念

### 模块化与可扩展性

编辑器采用模块化设计，每个面板都是独立的模块：

- **Viewport Panel**: 场景视口渲染
- **Hierarchy Panel**: 实体层级树
- **Properties Panel**: 组件属性检查器
- **Console Panel**: 日志控制台

### 虚表模式

使用虚表（Virtual Table）模式实现面板的扩展：

```c
typedef struct kyEditorPanelVtbl {
    const char *name;                    /* 面板名称 */
    const char *icon;                    /* 图标（预留） */
    kyEditorPanelInit init;              /* 初始化回调 */
    kyEditorPanelDestroy destroy;        /* 销毁回调 */
    kyEditorPanelUpdate update;          /* 更新回调（每帧） */
    kyEditorPanelDraw draw;              /* 绘制回调 */
    kyEditorPanelToggle toggle;          /* 最小化切换回调 */
    kyEditorPanelConfig config;          /* 可选：配置回调 */
    kyEditorPanelReorder reorder;        /* 可选：拖拽排序回调 */
    void *reserved[16];                  /* 预留扩展字段 */
} kyEditorPanelVtbl;
```

### 事件系统

使用回调函数处理编辑器事件：

```c
typedef enum kyEditorCallbackType {
    KY_EDITOR_CB_NONE = 0,
    KY_EDITOR_CB_VIEWPORT_DRAW = 1,
    KY_EDITOR_CB_ENTITY_SELECTED = 4,
    KY_EDITOR_CB_PROPERTY_CHANGED = 7,
    KY_EDITOR_CB_SCRIPT_ERROR = 11,
    KY_EDITOR_CB_LOG_MESSAGE = 13,
    KY_EDITOR_CB_PERF_FRAME = 16,
    KY_EDITOR_CB_PLUGIN_LOADED = 18,
    KY_EDITOR_CB_MAX
} kyEditorCallbackType;

typedef void (*kyEditorCallback)(kyEditorHooks *hooks, const void *event_data, void *user);

KY_API void ky_editor_hooks_register(kyEditorHooks *hooks, kyEditorCallbackType type, kyEditorCallback cb, void *user);
```

### 插件系统

预留插件接口，支持动态加载扩展：

```c
typedef struct kyEditorPluginVtbl {
    const char *name;                    /* 插件名称 */
    const char *version;                 /* 版本号 */
    kyEditorPluginInit init;             /* 初始化回调 */
    void (*destroy)(kyEditorPlugin *plugin);  /* 销毁回调 */
    void *reserved[16];                  /* 预留扩展字段 */
} kyEditorPluginVtbl;
```

## 使用方法

### 基本创建

```c
#include "kronyx/editor.h"

int main(void) {
    /* 创建编辑器配置 */
    kyEditorConfig *cfg = ky_editor_config_create();

    /* 创建编辑器 */
    kyEditor *editor = ky_editor_create(cfg, KY_RENDERER_GL, window);

    /* 创建面板 */
    kyEditorPanel *viewport = ky_editor_panel_create_viewport();
    kyEditorPanel *hierarchy = ky_editor_panel_create_hierarchy();
    kyEditorPanel *properties = ky_editor_panel_create_properties();
    kyEditorPanel *console = ky_editor_panel_create_console();

    /* 注册面板到编辑器 */
    ky_editor_panel_register(editor, viewport);
    ky_editor_panel_register(editor, hierarchy);
    ky_editor_panel_register(editor, properties);
    ky_editor_panel_register(editor, console);

    /* 运行编辑器主循环 */
    ky_editor_run(editor);

    /* 清理 */
    ky_editor_panel_unregister(editor, viewport);
    ky_editor_panel_unregister(editor, hierarchy);
    ky_editor_panel_unregister(editor, properties);
    ky_editor_panel_unregister(editor, console);
    ky_editor_destroy(editor);
    ky_editor_config_destroy(cfg);

    return 0;
}
```

### 注册回调

```c
/* 创建钩子 */
kyEditorHooks *hooks = ky_editor_hooks_create();

/* 注册实体选择回调 */
ky_editor_hooks_register(hooks, KY_EDITOR_CB_ENTITY_SELECTED,
    [](kyEditorHooks *h, const void *data, void *user) {
        uint32_t entity_id = *(const uint32_t *)data;
        printf("Entity selected: 0x%08X\n", entity_id);
    }, NULL);

/* 运行编辑器 */
ky_editor_run(editor);

/* 清理钩子 */
ky_editor_hooks_destroy(hooks);
```

### 添加日志

```c
/* 获取控制台面板 */
kyEditorPanel *console = ky_editor_panel_create_console();

/* 添加日志条目 */
ky_editor_console_error(console, "Something went wrong", __FILE__, __LINE__);
ky_editor_console_warning(console, "Performance warning", __FILE__, __LINE__);
ky_editor_console_info(console, "Game started", __FILE__, __LINE__);
ky_editor_console_debug(console, "Debug info", __FILE__, __LINE__);
```

## 扩展编辑器

### 创建自定义面板

1. 定义虚表结构：

```c
static const kyEditorPanelVtbl my_panel_vtbl = {
    .name = "My Panel",
    .icon = "M",  /* Unicode图标 */
    .init = my_panel_init,
    .destroy = my_panel_destroy,
    .update = my_panel_update,
    .draw = my_panel_draw,
    .toggle = my_panel_toggle,
    .config = NULL,  /* 可选 */
    .reorder = NULL,  /* 可选 */
    .reserved = {0}
};
```

2. 实现回调函数：

```c
static void my_panel_init(kyEditorPanel *panel, void *user) {
    my_panel_impl *impl = malloc(sizeof(my_panel_impl));
    memset(impl, 0, sizeof(my_panel_impl));

    /* 初始化你的状态 */
    panel->impl = impl;
}

static void my_panel_destroy(kyEditorPanel *panel) {
    if (panel && panel->impl) {
        free(panel->impl);
        panel->impl = NULL;
    }
}

static void my_panel_draw(kyEditorPanel *panel) {
    if (!panel || !panel->impl) return;

    my_panel_impl *impl = (my_panel_impl *)panel->impl;

    /* 使用ImGui绘制你的面板 */
    if (ImGui::BeginTabBar("MyTabBar")) {
        if (ImGui::BeginTabItem("Tab1")) {
            /* 绘制Tab1内容 */
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tab2")) {
            /* 绘制Tab2内容 */
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
```

3. 创建面板实例：

```c
kyEditorPanel *my_panel = ky_editor_panel_create(&my_panel_vtbl, user_data);
ky_editor_panel_register(editor, my_panel);
```

### 创建自定义插件

```c
static const kyEditorPluginVtbl my_plugin_vtbl = {
    .name = "My Plugin",
    .version = "1.0.0",
    .init = my_plugin_init,
    .destroy = my_plugin_destroy,
    .reserved = {0}
};

static bool my_plugin_init(kyEditorConfig *cfg, void *user) {
    printf("Plugin initialized\n");
    return true;
}

static void my_plugin_destroy(kyEditorPlugin *plugin) {
    printf("Plugin destroyed\n");
}

/* 加载插件 */
kyEditorPlugin *my_plugin = ky_editor_plugin_create(&my_plugin_vtbl, user_data);
ky_editor_plugin_load(editor, my_plugin);
```

## 文件结构

```
tools/editor/
├── include/
│   └── kronyx/
│       └── editor.h          # 编辑器公共接口
├── src/
│   ├── editor.c              # 编辑器核心实现
│   ├── viewport_panel.c      # 视口面板
│   ├── hierarchy_panel.c     # 层级面板
│   ├── properties_panel.c    # 属性检查器面板
│   └── console_panel.c       # 控制台面板
└── CMakeLists.txt            # 构建配置
```

## 未来扩展

### 预留扩展点

1. **ImGui后端适配器**：连接ImGui与Kronyx RHI
2. **资源浏览器**：纹理、网格、场景预览
3. **性能监视器**：FPS、DrawCall、内存监控
4. **脚本调试器**：kyx脚本断点、单步、变量监视
5. **热重载系统**：脚本、着色器、纹理热重载
6. **ImGuizmo集成**：3D变换Gizmo
7. **拖拽排序**：面板拖拽排序
8. **快捷键系统**：自定义快捷键
9. **主题系统**：编辑器外观主题
10. **宏录制**：宏录制与回放

### 版本兼容性

使用兼容性标记管理未来版本迁移：

```c
typedef enum kyEditorCompatibility {
    KY_EDITOR_COMPAT_NONE = 0,
    KY_EDITOR_COMPAT_V1 = 1,
    KY_EDITOR_COMPAT_CURRENT = KY_EDITOR_COMPAT_V1
} kyEditorCompatibility;
```

## 测试

运行编辑器API测试：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DKYR_BUILD_EDITOR=ON
cmake --build build -j2
./build/ky_test_editor
```

## 注意事项

1. **模块依赖**：编辑器模块依赖 `ky_core`、`ky_script`、`ky_render` 和 `imgui`
2. **线程安全**：编辑器内部状态不是线程安全的，主循环需在单线程运行
3. **内存管理**：面板和插件使用虚表模式，需正确管理生命周期
4. **扩展预留**：使用 `.reserved` 字段扩展，避免破坏现有接口

## 相关文档

- [架构设计白皮书](../docs/Kronyx_架构设计白皮书.md) - 第10节：GUI编辑器
- [Dear ImGui文档](https://github.com/ocornut/imgui)
