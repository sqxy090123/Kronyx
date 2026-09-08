# G5 · 编辑器接 ECS 世界 · 设计

## 概览

编辑器模块在 `tools/editor/` 下，编译为独立可执行文件 `ky_editor`（需 `KYR_BUILD_EDITOR=ON`）。EditorApp 持有 `kyWorld`，每帧刷新 Hierarchy/Properties/Viewport 三面板。

```
ky_editor main ──▶ ky_world_create ──▶ EditorApp{world, rd, selected_entity}
                                        ├── Hierarchy panel  ← ky_world_count_entities()
                                        ├── Properties panel ← ky_world_get_component()
                                        └── Viewport         ← ky2d_render_world(rd, world, cam)
                                              │
                                          ky_input_* / ky_event_*
                                              │
                                        UI 回调 → 写入 Transform/Sprite/Camera2D
```

## 文件结构

```
tools/editor/
  editor_app.h       — EditorApp 结构体声明
  editor_app.c       — 主循环、panel 刷新、输入分发
  editor_main.c      — main()，初始化 renderer + world + editor
  CMakeLists.txt     — ky_editor 目标（条件编译）
include/kronyx/editor.h  — 公共 API 头（KyEditorHandle 等）
tests/test_editor_world.c  — 逻辑测试
```

## 关键设计决策

### Entity 表示
- `kyEntity` 直接用于选中状态（`EditorApp.selected_entity`）。
- Hierarchy 条目：`(id << 16) | version` 编码为整数供 UI 显示；选回时反编码。

### 组件访问路径
- `ky_world_get_component(world, entity, type_id)` 取组件指针。
- Transform：`comp->pos.x / pos.y`，`comp->scale.x / scale.y`
- Sprite：`comp->atlas.x / y / w / h`
- Camera2D：`comp->target_rect.x / y / w / h`

### 面板刷新策略
- **Hierarchy**：`ky_world_count_entities()` 遍历所有实体槽位，收集活的实体 ID+version。
- **Properties**：选中实体变化时重新查询组件；字段绑定到组件内存（指针引用，非拷贝）。
- **Viewport**：每帧 `ky2d_render_world(rd, world, cam_entity)`。

### 输入分发
- `ky_input_poll()` 驱动事件循环。
- UI 控件响应 `KY_INPUT_KEY_PRESSED` / `KY_INPUT_POINTER_*`。
- 属性修改通过虚拟键盘（字符输入事件）或方向键增量修改。

## 测试策略

`tests/test_editor_world.c`（无头，无窗口）：
1. 创建 EditorApp → 验证默认三个实体在 Hierarchy 中。
2. 选中 player 实体 → 修改 Position X/Y → 验证 Transform 同步。
3. 选中 camera 实体 → 修改 target_rect → 验证 Camera2D 同步。
4. despawn 某实体 → Hierarchy 刷新后条目减少。

## 依赖

- `kronyx` core（world、input、event）
- `ky_2d`（ky2d_render_world）
- `ky_render`（renderer backend）
- `KYR_BUILD_EDITOR=ON`（CMake 开关）

## 退出标准

- `ky_test_editor_world` 绿（10+ 断言）。
- `ctest` 全量绿。
- `ky_editor` 无头冒烟 exit=0（EGL 环境）。
