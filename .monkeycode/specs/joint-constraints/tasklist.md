# G9 关节 / 物理约束 — 实施任务列表

按 design.md 的顺序拆解。每个任务含明确交付物与验收测试。

## 任务 1：API + 存储骨架

- [ ] `include/kronyx/physics.h`：`kyConstraintType` 枚举（DISTANCE/HINGE）、`kyConstraintDesc` 结构体（type/body_a/body_b/local_a/local_b/distance/angle_offset/enabled）、`KY_PHYSICS_MAX_CONSTRAINTS 256`、`ky_physics_add_constraint` / `remove_constraint` / `get_constraint_count` 声明
- [ ] `src/physics/physics_internal.h`：`kyPhysConstraint`（desc/alive/id）、`kyPhysicsWorld` 加 `constraints[256]` / `constraint_count` / `next_constraint_id`
- [ ] `physics.c`：`add_constraint`（校验 pw/desc/type/enabled/双端 body id 合法/满容量 → 1-based id → 写存储）、`remove_constraint`（线性找 alive+id 匹配 → alive=0，未找到返回 0；重复调用无害）、`get_constraint_count`（返回累计 count）
- [ ] 验收：编译通过；add 满 256 后返回 0；非法 type/body 返回 0；remove 幂等

## 任务 2：求解器 + step 集成

- [ ] `constraint_anchor_world(b, local)`：`b==NULL` 返 `*local`（静止端世界坐标）；否则 `b->position + ky_quat_rotate(b->rotation, *local)`
- [ ] `solve_distance(pw, &c)`：anchor 世界坐标 → 方向 n / 误差 err；除零守卫（len<1e-6 跳过）；位置端按 inv_mass 权重投影到 distance；速度端消除沿 n 的相对速度分量
- [ ] `solve_hinge(pw, &c)`：位置端两 anchor 投影重合（按 inv_mass 权重）；角度端 `diff = angle_a - angle_b - angle_offset`（`atan2f` 归一），`ky_quat_axis_angle(Z, ±diff*inv/w)` 修正 `kyQuat`（`ky_quat_normalize` 防漂移）；速度端消除 anchor 相对速度
- [ ] `phys_apply_constraints(pw, dt)`：逐条活跃约束按 type 调用求解器，受约束 body 重算 AABB（`phys_body_update_aabb`）
- [ ] `ky_physics_step`：碰撞事件 emit 之后调用 `phys_apply_constraints`
- [ ] 验收：`test_physics.c` distance 位置收敛 / hinge 角度收敛 / 静止端对称 / 除零守卫断言全绿

## 任务 3：测试 + 回归 + 文档回写

- [ ] `tests/test_physics.c` 扩"约束"小节（~15 断言）：add 成功 id、满容量返回 0、非法 type/body 返回 0、remove 幂等、distance 位置偏差 <0.1、distance 对称（body_b=0 vs body_a=0）、hinge 角度收敛、hinge 静止端、anchor 重合除零守卫、step 后 AABB 自洽、仅约束无碰撞事件
- [ ] 全量 `ctest -j1` 23/23 绿（约束断言挂在 `test_physics` 内，测试数不变）；ASan `detect_leaks=0` 全绿；`ky_demo` 无头冒烟 exit=0
- [ ] `progress.md` G9 标记完成，"下一刀"指向 命令列表 cancel 路径 / APK 打包

## 备注

- 约束存储为 `kyPhysicsWorld` 内静态数组（256 项），`ky_physics_destroy` 路径不变，无堆泄漏。
- 求解器为单次迭代（位置 1 次 + 速度 1 次），测试断言"1 step 后偏差在阈值内"而非逐帧稳定。
- `count` 语义为"累计成功添加数"（remove 后不变），与"活跃数"区分。
- 不动 SAP 窄相 / 碰撞事件 / 力场管线；约束求解在碰撞事件 emit 之后。
