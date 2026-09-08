# Requirements: 2D 平台人 Demo

Feature Name: 2d-platformer-demo
Updated: 2026-09-07
Status: Draft v1

## Introduction

替换 `src/demo/main.c` 的裸 GL `rotating square` demo，为 Kronyx 提供第一个"引擎跑成游戏循环"的可运行样本：ECS 世界 + Transform/Sprite/Camera2D + 2D batch 管线 + AABB 物理 + GLFW 输入，用 `ky2d_render_world_auto` 输出画面。该 demo 是 `progress.md` 的 G1，是 G3（脚本绑定）到 G5（编辑器）的主轴。

## Glossary

- Role：带刚体的角色实体
- Ground：隐形刚体（box collider），顶面作为落地平面
- CameraEntity：带 Camera2D 的正交相机实体
- Headless smoke：无 GLFW 环境下跑 N 帧无头逻辑，验证同一套系统协作

## Requirements

### Req 1: 场景搭建

WHEN demo 启动，系统 SHALL 建立下列实体，且顺序固定：

1. Ground：`KY_SHAPE_BOX` collider，顶面 `y=0`，`inv_mass=0`（静态），无 Sprite（不可见）
2. Role：`KY_SHAPE_SPHERE` collider（半径 0.5 EU），初始 `(0, 1, 0)`，带 Transform + Sprite（红色 1.0×1.0，white 纹理）
3. CameraEntity：`Camera2D`，`active=1`，viewport `10×7.5 EU`，zoom `1.0`

WHEN 上述任一实体/组件创建失败，demo SHALL 打印错误并返回非 0。

### Req 2: 输入

WHILE 有 GLFW 窗口，系统 SHALL 通过 `ky_engine_key_pressed(GLFW_KEY_*)` 读取方向：

- `GLFW_KEY_A` 或 `GLFW_KEY_LEFT` → 左移
- `GLFW_KEY_D` 或 `GLFW_KEY_RIGHT` → 右移
- `GLFW_KEY_SPACE` / `GLFW_KEY_W` / `GLFW_KEY_UP` → 跳跃

WHILE 无 GLFW 环境（headless smoke），系统 SHALL 通过 `ky_input_simulate_key` 灌入等价事件并走 `ky_input_poll` 读取。跳跃按上升沿语义，只有 `vy==0`（或 `vy<0` 且落地）才接受。

### Req 3: 每帧 tick 顺序

WHILE 引擎在跑，每一帧 SHALL 严格按以下顺序执行：

1. input poll → 得到 `vx_target`、`jump_requested`
2. 应用输入到 Role：水平速度线性插值到 `vx_target`；`jump_requested && |vy| < 0.01` 时置 `vy=10`
3. `ky_physics_step(pw, dt)` 推进重力 + 碰撞
4. 将 Role 刚体位置写回 Transform（`(pos.x, pos.y)`，忽略 z）
5. 相机跟随：`Camera2D.pos = (role.pos.x, role.pos.y + 2.0)`
6. `ky2d_render_world_auto(rd, world)` 输出

### Req 4: 落地语义

WHILE Role 与 Ground 重叠，物理 SHALL 通过 AABB 分离把 Role 顶到 Ground 顶面 +1 半径 以上，且 `vy` 归零（`inv_mass=0` 的 Ground 不移动）。

### Req 5: Headless smoke

WHEN 在无 GLFW 环境下运行，demo SHALL 以 `KY_RENDERER_CONSOLE` 后端建 RenderDevice，模拟 A→D→SPACE→重力 序列，最少跑 60 帧；跑完打印 "Headless smoke OK (role.x=..., role.y=...)"，然后干净退出（`return 0`）。

WHEN 有 GLFW、`ky_engine_init` 成功，demo SHALL 走 `ky_engine_run(update, render)` 直到窗口关闭。

### Req 6: 反篡改保留

WHEN demo `main` 入口执行，系统 SHALL 先调用 `ky_tamper_init(KY_TAMPER_MODE_ERROR, ...)`，失败则 `return 1`；成功后进入场景搭建。

## Non-goals

- 不引入帧动画、粒子、音频
- 不接脚本（G3）
- 不做 3D / 关节 / 骨骼
- 不重做物理，只用 `ky_physics_*`

## 验收

- `tests/test_demo_loop.c` 在 `ctest -j1` 下稳定通过（无窗口环境）
- `src/demo/main.c` 不再包含任何 `#include <GL/glew.h>`、`gl*` 调用或 `GLuint` 状态
- 有头环境 `./ky_demo` 能跑：A/D 移动，SPACE 跳，Esc/窗口关闭退出
