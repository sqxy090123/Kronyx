# Requirements Document: G10 粒子系统

## Introduction

Kronyx 需要一个新的 2D 粒子系统模块（`src/render/particle2d.c` + `include/kronyx/particle2d.h`），
按既有垂直切片规范落地：粒子发射器作为 ECS 组件（`"emitter"`），粒子模拟为内置系统
（随 `ky_world_step` 推进），渲染复用现有 `ky2dContext` 的 `sprite_gen` 扩展点——
粒子在 C 侧生成 quad 顶点/索引写入调用方提供的缓冲区，经 RHI vtable 提交，
**console 与 GL 后端均无需任何改动**。

前置：G7（sprite 帧动画）已完成。本切片交付"有真实游戏可加的系统"中的 P3-G10。

## Glossary

- **Emitter**: ECS 组件（`kyEmitter`），描述一个粒子发射器的配置与运行时状态（发射速率、寿命、初速、重力、颜色、纹理、形状）。
- **Particle**: 单个模拟中的粒子实例，含位置、速度、年龄、颜色/尺寸参数。
- **Particle pool**: 进程内固定容量的粒子存储（`static`，与 `2d.c` 的 `g_items` 同模式），容量 `KY_PARTICLE_MAX`（默认 16384）。
- **Emit shape**: 发射点采样方式：点（POINT）、圆（CIRCLE，半径 r）、线（LINE，宽 w）。
- **2D render path**: `ky2d_render_world` → `ky2dContext.sprite_gen` → RHI `draw_pass` 的既有链路。

## Requirements

### Requirement 1: 组件注册与默认值

**User Story:** AS 游戏开发者, I want to 一个实体挂上 emitter 组件, so that 我可以配置并启动粒子发射。

#### Acceptance Criteria

1. WHEN 调用 `ky_particle2d_register(world)`, 模块 SHALL 注册名为 `"emitter"` 的组件类型（size 为 `sizeof(kyEmitter)`）以及一个名为 `"particle-update"` 的内置系统，并返回 0；world 为 NULL 时 SHALL 返回负值。
2. WHEN 调用 `ky_emitter_new()`, 模块 SHALL 返回全零初始化的 `kyEmitter`，其中 texture 为 NULL、enabled 为 0、emit_rate 为 0、emit_shape 为 `KY_EMIT_POINT`、texture_scale 为 0。
3. WHEN 对同一 world 重复调用 `ky_particle2d_register`, 模块 SHALL 幂等返回 0。

### Requirement 2: 发射与生命周期模拟

**User Story:** AS 游戏开发者, I want to 每帧按速率发射粒子并自动回收, so that 我不需要手动管理粒子池。

#### Acceptance Criteria

1. WHILE 一个 emitter 的 enabled 为 1, 系统 SHALL 在每次 `ky_world_step(w, dt)` 中按 `emit_rate * dt` 的期望值发射粒子，使用确定性计数器（每 emitter 一个 `float` 累加器，floor 取整发射，余数保留）。
2. IF 粒子池空闲数不足以满足本次应发射数量, 系统 SHALL 仅发射可用数量（静默截断），不返回错误、不崩溃。
3. WHEN 一个粒子年龄超过其寿命（`life0 + life1` 中随机取到的个体寿命）, 系统 SHALL 将其回收至空闲链表，且回收后粒子不得再参与模拟与渲染。
4. IF dt 为 0, 系统 SHALL 不发射新粒子且不推进任何粒子年龄（行为为无操作）。
5. IF emitter 的 emit_rate 为 0 或 enabled 为 0, 系统 SHALL 不发射粒子。
6. WHEN 个体寿命与个体初速取 `life0..life1` / `speed0..speed1` 区间, 模块 SHALL 使用每 emitter 的内部确定性 LCG 随机源（seed 可设置），同一 seed + 同一 step 序列下模拟结果可复现。

### Requirement 3: 运动学与颜色行为

**User Story:** AS 游戏开发者, I want to 粒子受重力与加速度影响, so that 我可以表现喷射、坠落、烟雾等效果。

#### Acceptance Criteria

1. WHILE 粒子年龄推进, 系统 SHALL 对速度应用 `gravity * dt`，对位置应用 `velocity * dt`，并对速度应用 `drag * dt`（drag 以 1/s 计）。
2. WHEN 粒子有初速区间配置, 系统 SHALL 在发射瞬间按 emit_shape 与朝向（`direction` 角度，弧度）叠加初速，朝向 0 为 +X 轴方向。
3. WHILE 粒子从出生到死亡, 系统 SHALL 按 `color_from` → `color_to`（RGBA 四通道）对颜色做线性插值，插值进度为 `age / life`。
4. WHILE 粒子从出生到死亡, 系统 SHALL 按 `size_from` → `size_to` 对尺寸做线性插值。

### Requirement 4: 渲染集成（render_frame 内置粒子 pass）

**User Story:** AS 引擎宿主, I want to 粒子与 sprite 走同一 RHI 链路且宿主零配置, so that console 与 GL 后端都无需改动，宿主一行代码不开粒子。

#### Acceptance Criteria

1. WHEN 调用 `ky_particle2d_render_pass(rd, cl, cam, ctx)`（模块提供的渲染 pass 函数，由 `2d.c` 的 `render_frame` 在 sprite 批处理之后、`ky_rd_submit` 之前调用）, 模块 SHALL 把当前所有存活粒子写为 quad 顶点（4 顶点/粒子）与索引（6 索引/粒子）到粒子模块内的 static staging 缓冲（`KY_PARTICLE_MAX*4` 顶点容量），并经 RHI vtable（`ky_rd_update_buffer` + `ky_cmd_*` + `ky_cmd_draw_indexed`）提交；粒子为 0 时不提交 draw 调用，返回 0。
2. IF 存活粒子数产生的顶点或索引超出 staging 容量, 模块 SHALL 静默截断至容量内，不越界写。
3. WHEN 粒子纹理为 NULL, 模块 SHALL 使用引擎内置白色纹理（与 sprite 的 `texture == NULL` 行为一致）。
4. WHEN `render_frame` 在 sprite pass 完成后调用粒子 pass, 粒子 SHALL 使用粒子自身的 texture（emitter.texture 或内置 white），与 sprite 批处理独立成批，不竞争 sprite 的 staging 容量；console 与 GL 后端均无需任何改动。
5. 粒子 pass 的提交 SHALL 复用 sprite pass 已设置的 pipeline 与 vertex/index buffer slot，仅切换纹理与更新粒子自己的 vbo/ibo 数据，避免额外 pipeline switch。

### Requirement 5: 实体销毁与安全性

**User Story:** AS 引擎宿主, I want to 粒子系统在所有异常路径下保持内存与逻辑安全, so that 引擎长期运行不产生悬空引用。

#### Acceptance Criteria

1. WHEN 一个携带 emitter 组件的实体被 despawn（ECS 自动走 dtor）, 系统 SHALL 通过组件 dtor 回调将其名下所有存活粒子立即回收，实体不再发射。
2. IF `ky_particle2d_render_gen` 收到 NULL 的 out 缓冲指针而 out_count > 0, 模块 SHALL 返回负值，不写任何内存。
3. IF 调用方传入的缓冲区容量小于单个粒子所需（4 顶点/6 索引）, 模块 SHALL 发射 0 个粒子并返回 0（与 Acceptance Criteria 2 的截断语义一致：容量不足一个粒子则零输出）。
4. WHEN 粒子池空闲链表为空（全部存活）, 系统 SHALL 不分配、不报错（见 Requirement 2 AC2）。
5. IF `ky_particle2d_register` 的 world 为 NULL, 模块 SHALL 返回负值，不触碰全局粒子池状态。

### Requirement 6: 公共 API 形态

**User Story:** AS 引擎 API 维护者, I want to particle 模块的 API 与邻模块（2d.h / anim2d.h）风格一致, so that 使用者无需学习新范式。

#### Acceptance Criteria

1. WHEN 模块发布头文件, 头文件 SHALL 包含：`kyEmitter` 结构体、`KY_EMIT_POINT/CIRCLE/LINE` 枚举、`ky_emitter_new()`、`ky_particle2d_register(world)`、`ky_particle2d_render_pass(rd, cl, cam, tr)`（由 `2d.c` 的 `render_frame` 调用，宿主无感知）、以及 `ky_particle2d_alive_count()`（返回当前存活粒子总数，供测试与调试）。
2. WHEN 宿主注册粒子系统后, `ky_particle2d_register` SHALL 与 `ky2d_register_components` 可叠加使用，互不覆盖既有组件类型。
3. WHEN 模块暴露容量宏 `KY_PARTICLE_MAX`, 其值 SHALL 为 16384（与 2D 帧级 sprite 上限同量级）。

## Out of Scope（本切片明确不做）

- 3D 粒子、GPU 侧粒子（compute shader / VBO 实例化）
- 粒子间碰撞、粒子曲线/轨迹系统
- 粒子序列帧（粒子用整张纹理 quad，不用 anim2d 的帧切分）
- 脚本（kyx）侧绑定
- 持久化/序列化 emitter 配置
- 编辑器面板集成
- 粒子与 sprite 的 per-sprite `sprite_gen` 集成（保持 `sprite_gen` 现有 per-sprite 语义不变；粒子走 `render_frame` 内置独立 pass）

## Validation Notes

- 确定性可测：LCG 随机源 seed 可设；同 seed 两次 `ky_world_step` 序列的粒子轨迹/颜色必须逐帧相等（测试断言）。
- 无渲染依赖：模拟与生成逻辑全部为纯 C（console 后端可跑），GL/EGL 环境不改变模拟数值。
- 验收测试：`tests/test_particle.c` 挂入 CMake/ctest，覆盖上述 6 组 AC 的核心路径（注册幂等、发射计数、寿命回收、重力/颜色插值数值、池满截断、despawn 回收、缓冲区越界保护）。
