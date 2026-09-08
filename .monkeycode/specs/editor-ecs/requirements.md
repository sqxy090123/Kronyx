# G5 · 编辑器接 ECS 世界 · 需求

## 目标

编辑器骨架（`tools/editor`）接到真实 ECS 世界：Hierarchy 列出实体，Properties 面板编辑 Transform/Sprite/Camera2D，Viewport 调 `ky2d_render_world` 渲染。

## 角色

- **EditorApp**：主循环，持有 `kyWorld` 与 `KyRenderer *rd`
- **Hierarchy Panel**：只读列表，点击选中实体
- **Properties Panel**：双向绑定 — 选中的 `kyEntity` 的 `Transform`/`Sprite`/`Camera2D` 可编辑
- **Viewport**：全屏 `ky2d_render_world(rd, world, cam)`

## 需求

### R1 世界初始化

- R1.1 当编辑器启动，必须创建 `kyWorld` 并注册 `ty_transform`、`ty_sprite`、`ty_camera2d` 组件类型。
- R1.2 默认创建三个实体：player（带 Transform + Sprite）、camera（带 Camera2D）、ground（带 Transform + Sprite）。
- R1.3 `kyRBuildEditor` 为 ON 时才链接编辑器代码。

### R2 Hierarchy

- R2.1 当世界变化（spawn/despawn），Hierarchy 必须立即刷新实体列表。
- R2.2 每个条目显示 `entity_id version` 字符串（如 `0(1)`）。
- R2.3 点击条目选中该实体（editor_state->selected_entity）。
- R2.4 未选中时 Properties 面板显示 "No selection"。

### R3 Properties

- R3.1 当实体有 `Transform` 组件，Properties 面板显示 Position X/Y、Scale X/Y 字段。
- R3.2 修改 Position X/Y 后，世界中的 Transform 必须同步写入。
- R3.3 修改 Scale X/Y 后，世界中的 Transform 必须同步写入。
- R3.4 当实体有 `Sprite` 组件，Properties 面板显示 atlas rect（x/y/w/h）。
- R3.5 修改 atlas rect 后，Sprite 组件必须同步写入。
- R3.6 当实体有 `Camera2D` 组件，Properties 面板显示 target_rect（x/y/w/h）。
- R3.7 修改 target_rect 后，Camera2D 组件必须同步写入。

### R4 Viewport

- R4.1 Viewport 每帧调用 `ky2d_render_world(rd, world, camera_entity)`。
- R4.2 切换 renderer 后端（Console/GLES2/GLES3/OpenGL）不影响世界状态。
- R4.3 无窗口环境（EGL 软渲染）必须可正常跑冒烟测试。

### R5 测试

- R5.1 有逻辑测试 `tests/test_editor_world.c`：验证 spawn 后 Hierarchy 有条目、选中后 Properties 字段写入世界。
- R5.2 测试在无窗口条件下运行（不依赖 GLFW 窗口）。

## 验收

- `ky_test_editor_world` 绿。
- `ctest` 全量绿（含 `editor_world`）。
- `KYR_BUILD_EDITOR=ON cmake ...` 可构建 `ky_editor`。

## 非目标

- 可视化脚本、插件市场、独立渲染器
- 多窗口布局、拖放调整面板
-  Undo/Redo 历史
