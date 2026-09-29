# G10 粒子系统 — 实施任务列表

按 design.md 的顺序拆解。每个任务含明确交付物与验收测试。

## 任务 1：模块骨架 + 注册 API

- [x] 新建 `include/kronyx/particle2d.h`：`kyEmitShape` 枚举、`kyEmitter` 结构体、`KY_PARTICLE_MAX=16384`、`ky_emitter_new()` / `ky_particle2d_register(w)` / `ky_particle2d_render_pass(rd,cl,cam,cam_tr)` / `ky_particle2d_alive_count()` / `ky_particle2d_registered()` / `ky_particle2d_set_texture(tex)` 声明
- [x] 新建 `src/render/particle2d.c`：`ky_emitter_new()` 默认值、静态池 `g_pool`（16384）、xorshift32 LCG、`ky_particle2d_register` 注册 `"emitter"` 组件（ctor/dtor）+ `"particle-update"` 系统；NULL world 返回 -1；重复调用幂等（按 name 去重组件与系统）
- [x] CMakeLists.txt：`particle2d.c` 加入 `ky_engine` 源列表
- [x] 验收：编译通过；`ky_particle2d_register(NULL) == -1`；同 world 注册两次不崩溃

## 任务 2：模拟系统（发射 / 寿命 / 运动学 / 颜色）

- [x] xorshift32 随机源（`uint32_t` 状态，可设 seed，线性可复现）
- [x] `particle-update` 系统 update：遍历 `["emitter"]` 视图；对每个 enabled 且 emit_rate>0 的 emitter：`acc += emit_rate*dt`，floor 取整发射，余数保留；dt<=0 直接 return
- [x] 发射单个粒子：从 free 扫描摘除（为空则截断），按 emit_shape+direction 采样初位置与初速区间（LCG），age=0，life=life0+rand*(life1-life0)，owner=实体 id，颜色 from/to 端点 + gravity + drag 在发射时捕获
- [x] 推进存活粒子：`vel += gravity*dt`；`vel *= (1 - drag*dt)`；`pos += vel*dt`；`age += dt`；`age*inv_life >= 1` 则归还
- [x] 颜色/尺寸插值：`t = age*inv_life`；color = from + (to-from)*t（逐通道）；size = size0 + (size1-size0)*t（渲染 pass 内计算）
- [x] 实体回收：`reap_dead_owners` 在每次 step 前扫池，owner id 出现在 `w->free_ids` 即回收（ECS dtor 无实体 id，故用系统侧回收）
- [x] 验收：`test_particle.c` 发射计数、寿命回收、dt=0、池满截断、确定性、despawn 回收断言全绿

## 任务 3：渲染 pass（render_frame 集成）

- [x] `src/render/particle2d.c` 实现 `ky_particle2d_render_pass(rd, cl, cam, cam_tr)`：存活粒子生成 quad 顶点 + 索引到模块 static staging（`g_pverts`/`g_pidx`）；per-device GPU 资源（shader/pipeline/vbo/ibo/white）lazy init；`rd`/`cl` 为 NULL 返回 -1；粒子数 0 时跳过
- [x] `2d.c` `render_frame`：sprite 批循环之后、`ky_rd_submit` 之前插入 `if (ky_particle2d_registered()) ky_particle2d_render_pass(rd, cl, cam, cam_tr);`
- [x] demo 接入：`platformer_world.c` 注册粒子系统 + role 挂 emitter（跳跃/落地脉冲），`ky_world_step` 纳入 tick；`platformer_world.h` 加 `tid_emitter`/`emit_pulses` 字段
- [x] 验收：console rd 下 `ky2d_render_world` 触发粒子 pass 不崩溃；`ky_demo` 无头冒烟 exit=0；全量 ctest 22/22 绿

## 任务 4：测试 + 回归 + 文档回写

- [x] `tests/test_particle.c` 挂 CMake（目标名 `particle`，22 个目标）
- [x] 全量 `ctest -j1` 22/22 绿；ASan `detect_leaks=0` 全绿
- [x] `progress.md` G10 标记完成
- [x] 文档回写见下方备注

## 备注

- 渲染 pass 纹理策略：V1 单共享纹理（`ky_particle2d_set_texture`，默认 white），不做 per-emitter 批处理。
- 粒子模拟为纯 C，console 后端可驱动；GL/EGL 环境数值不变。
- ECS dtor 无实体 id 参数，despawn 回收走系统侧 `reap_dead_owners`（扫 `w->free_ids`），O(pool×free_ids) 可接受。
- `ky_world_step` 不排序系统；demo 在 tick 内 `ky_world_sort_systems` 保证粒子系统（order 1）在物理/动画（order 0）之后。
