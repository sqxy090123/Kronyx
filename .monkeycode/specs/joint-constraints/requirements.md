# Requirements Document: G9 关节 / 物理约束

## Introduction

Kronyx 物理模块（`src/physics/physics.c` + `include/kronyx/physics.h`）已交付刚体 + SAP(X) + AABB 窄相
+ 力场 + raycast + 碰撞回调，但**无 joint / constraint API**。P3 阶段的 G9 要求补齐"2D 复杂
碰撞体或物理动画"所需的关节约束能力，落地在物理模块内，新增约束创建/移除/查询 API，
不改动既有 SAP 窄相与碰撞事件管线。

本切片交付"有真实游戏可加的系统"中的 P3-G9。前置：G1 可玩、G8 碰撞回调已完成。

## Glossary

- **Joint / Constraint**: 两条刚体（或一条刚体与静止世界）之间的运动学约束，限制二者
  相对位置/角度。在 Kronyx 中以"每 step 求解一次位置 + 一次速度"的脉冲迭代实现，
  与 Box2D 风格的距离/铰链约束同范式。
- **Distance joint**: 两个锚点（anchor）间距离恒定的约束。求解时把相对位置投影到
  目标距离，速度端消除沿连线方向的相对速度。
- **Hinge / revolute joint**: 两个锚点位置重合且角度差恒定的约束。求解时做位置
  投影（重合锚点）+ 角度差修正，速度端消除锚点处相对速度。
- **Anchor**: 刚体上以刚体局部坐标定义的锚点（`local_a` / `local_b` 为 body 局部坐标），
  求解时旋转至世界坐标。
- **Constraint pair**: 由 anchor 定义的一对刚体（可以是同一 world 内两个不同 body，
  或一端为"静止世界"——`body_b == 0` 表示约束到无穷质量静止参考）。
- **Solver step**: `ky_physics_step` 末尾新增的约束求解阶段，在所有 force/gravity/
  collision 推进完成之后运行，使 body 最终位置/速度满足约束。

## Requirements

### Requirement 1: 约束类型与注册

**User Story:** AS 游戏开发者, I want to 给两个刚体挂上距离或铰链约束, so that 我可以做绳索、
  关节、杠杆类物理动画。

#### Acceptance Criteria

1. WHEN 调用 `ky_physics_add_constraint(pw, &desc)`, 模块 SHALL 分配一个 1-based 约束 id，
   内部存储约束参数（类型、双 body id、双 anchor 局部坐标、目标距离、角度差、enabled 标志），
   并返回该 id；world 满容量时 SHALL 返回 0。
2. WHEN `desc->body_a == 0` 或 `desc->body_b == 0`, 模块 SHALL 把该端视为"静止世界"
   （无穷质量，anchor 为世界坐标），求解时只对非静止端施加修正。
3. IF 约束类型非法（既非 distance 也非 hinge）或 `desc == NULL` 或 `pw == NULL`,
   模块 SHALL 返回 0。
4. WHEN 调用 `ky_physics_remove_constraint(pw, id)`, 模块 SHALL 把该约束标记为不活跃，
   其 id 不得再被 `ky_physics_step` 的求解阶段使用；同一 id 重复 remove SHALL 为无害空操作。
5. WHEN 调用 `ky_physics_get_constraint_count(pw)`, 模块 SHALL 返回当前活跃约束总数。

### Requirement 2: 距离约束求解

**User Story:** AS 游戏开发者, I want to 两个刚体被距离约束绑定时保持锚点距离恒定,
  so that 绳索/弹簧链类动画可复现。

#### Acceptance Criteria

1. WHILE 一条 distance 约束处于活跃状态, `ky_physics_step` SHALL 在所有 force/gravity/
   collision 推进完成后求解该约束：把两个 anchor 的世界坐标距离投影到 `desc->distance`，
   按 `inv_mass` 权重分配位置修正到两端（静止端权重为 0）。
2. WHILE 同一条约束在速度端求解, 模块 SHALL 消除两个 anchor 世界速度中沿连线方向的
   分量（使相对速度沿连线为零），同样按 `inv_mass` 权重分配。
3. IF 两 anchor 当前距离为 0（完全重合）, 模块 SHALL 不做方向修正（避免除零），仅记录
   不活跃并跳过该约束本次求解。
4. 求解顺序 SHALL 对 `desc` 指定的双端对称处理，与 body_a/body_b 的 id 顺序无关。

### Requirement 3: 铰链 / 旋转约束求解

**User Story:** AS 游戏开发者, I want to 两个刚体在锚点处铰接且角度差保持恒定,
  so that 可以做杠杆、门、可转动关节类动画。

#### Acceptance Criteria

1. WHILE 一条 hinge 约束处于活跃状态, `ky_physics_step` SHALL 把所有 force/gravity/
   collision 推进完成后的求解阶段执行：(a) 位置端把两 anchor 投影到重合（按 `inv_mass`
   权重分配，静止端权重为 0）；(b) 角度端把 `body_a->rotation` 与 `body_b->rotation`
   的角度差投影到 `desc->angle_offset`（按 `inv_mass` 权重分配，静止端权重为 0）。
2. WHILE 铰链约束速度端求解, 模块 SHALL 消除两 anchor 世界速度（含刚体绕 anchor 的
   角速度贡献）的相对分量，使 anchor 处无相对滑动。
3. IF 一端为静止世界（`body == 0`）, 角度修正 SHALL 全部施加到活动端；位置修正
   全部施加到活动端。

### Requirement 4: 求解阶段集成

**User Story:** AS 引擎宿主, I want to 约束求解在既有 step 管线末尾自动运行,
  so that 我不需要手动驱动约束，且碰撞/事件管线不受影响。

#### Acceptance Criteria

1. WHEN `ky_physics_step(pw, dt)` 完成 force/重力/碰撞推进后, 模块 SHALL 调用约束
   求解阶段，按注册顺序逐条处理活跃约束（distance 与 hinge 各自的求解器），
   最后重新计算受约束 body 的 AABB（`phys_body_update_aabb`）。
2. WHILE 约束求解阶段, 模块 SHALL 不触发 `KY_EVENT_COLLIDE` 事件（约束不是碰撞，
   碰撞事件仍只在 SAP 窄相阶段发出）。
3. IF 一条约束在 step 中途被 `remove_constraint` 移除, 该 step 的求解阶段 SHALL 跳过
   它（求解前读 `alive` 标志）。
4. 约束求解 SHALL 为纯数值迭代，单次 step 内位置端 1 次 + 速度端 1 次（Kronyx 简化
   求解器，非 Box2D 多子步迭代）；测试断言用"1 step 后距离偏差在阈值内"而非
   逐帧稳定。

### Requirement 5: 公共 API 形态

**User Story:** AS 引擎 API 维护者, I want to 约束 API 与邻模块（`ky_physics_add_body` /
  `ky_physics_add_force_field`）风格一致, so that 使用者无需学习新范式。

#### Acceptance Criteria

1. WHEN 模块发布头文件, 头文件 SHALL 包含：`kyConstraintDesc` 结构体（`type`、`body_a`、
   `body_b`、`local_a`、`local_b`、`distance`、`angle_offset`、`enabled`）、
   `KY_CONSTRAINT_DISTANCE` / `KY_CONSTRAINT_HINGE` 枚举、
   `ky_physics_add_constraint(pw, &desc)`、`ky_physics_remove_constraint(pw, id)`、
   `ky_physics_get_constraint_count(pw)` 以及容量宏 `KY_PHYSICS_MAX_CONSTRAINTS`（值为 256）。
2. WHEN 宿主对同一 world 多次 `add_constraint` 后 `get_constraint_count`, 返回值
   SHALL 等于累计成功添加数（remove 后不再计入）。
3. `ky_physics_destroy` SHALL 释放所有已分配约束存储，无泄漏（ASan 干净）。

## Out of Scope（本切片明确不做）

- 摩擦/旋转锁定的通用 hinge（仅做位置 + 角度差 + 速度消除，不做扭矩约束）
- 弹簧约束（distance 带 k/damping 系数）
- 电机（motor / angular motor）
- 约束的热重载（序列化 / 反序列化到场景格式）
- 约束与脚本（kyx）绑定
- 编辑器面板集成
- 多子步 / 多迭代 Box2D 风格求解器

## Validation Notes

- 数值可测：两 body 各 inv_mass=1，distance=2.0 约束下，初始位置距离 4.0，1 step 后
  距离偏差在 0.1 以内（1 次迭代精度）；角度约束同理。
- 静止端语义：`body_b == 0` 时 anchor 为世界坐标，修正全施加到 `body_a`。
- 无 ASan 泄漏：`ky_physics_destroy` 释放约束存储；`add_constraint` 满容量时返回 0 不越界。
- 验收测试：`tests/test_physics.c` 扩"约束"小节（~15 断言）：add/remove 计数、distance
  位置修正、hinge 角度修正、静止端、满容量返回 0、step 后 AABB 更新。
