# 2D 粒子模块 (Particle2D)

## 概述

`particle2d` 是 2D 渲染层的粒子子系统，G10 切片交付。它以 ECS `"emitter"` 组件 +
`"particle-update"` 内置系统的形式接入，模拟为确定性 LCG 随机源（可设 seed），池容量
`KY_PARTICLE_MAX=16384`（进程内静态，与 `2d.c` 的 `g_items`/`g_vbo` 同模式）。渲染走
`render_frame` 内置粒子 pass，经 RHI vtable 提交，**console 与 GL 后端均无需改动**，
宿主零配置。

## 头文件

- `include/kronyx/particle2d.h`

## API

| 函数 | 说明 |
|------|------|
| `kyEmitter ky_emitter_new(void)` | 返回默认 emitter（enabled=0, rate=0, life=1s, seed=0x12345678） |
| `int ky_particle2d_register(kyWorld *w)` | 注册 "emitter" 组件 + "particle-update" 系统，幂等；NULL 返回 -1 |
| `int ky_particle2d_alive_count(void)` | 当前存活粒子总数 |
| `int ky_particle2d_registered(void)` | 注册标志（`render_frame` 据此决定是否跑粒子 pass） |
| `int ky_particle2d_render_pass(rd, cl, cam, cam_tr)` | 把存活粒子绘制为 quad；`rd`/`cl` 为 NULL 返回 -1 |
| `void ky_particle2d_set_texture(kyTexture *tex)` | 设置全粒子共享纹理；NULL 回落内置 white（V1 单纹理策略） |

## 组件字段

```c
kyEmitter {
    enabled, emit_rate, shape, shape_scale, direction,
    speed0/1, life0/1, size0/1, color_from/to, gravity, drag,
    texture, seed,
    /* 运行时 */ emit_acc, rng_state
}
```

发射形状：`KY_EMIT_POINT`（原点）、`KY_EMIT_CIRCLE`（半径 shape_scale 环）、
`KY_EMIT_LINE`（沿 direction 的线，半宽 shape_scale）。

## 使用范例

```c
ky2d_register_components(w);
ky_particle2d_register(w);

kyEntity e = ky_world_spawn(w);
kyTransform *tr = ky_world_add_component(w, e, tid_transform);
tr->pos = (kyVec2){2, 2};
kyEmitter *em = ky_world_add_component(w, e, tid_emitter);
*em = ky_emitter_new();
em->enabled   = 1;
em->emit_rate = 120;
em->life0     = 0.3f;  em->life1 = 0.7f;
em->speed0    = 1.0f;  em->speed1 = 3.0f;
em->direction = -1.5707963f; /* 向上 */
em->gravity   = (kyVec2){0, -9.8f};
em->color_from = (kyVec4){1, 1, 0.4f, 1};
em->color_to   = (kyVec4){1, 0.4f, 0, 0};
em->seed       = 42;

ky_world_sort_systems(w);   /* 粒子系统 order=1，需在物理/动画之后 */
for (int i = 0; i < 60; i++) {
    ky_world_step(w, 1.0f/60.0f);
    ky2d_render_world(rd, w, cam); /* 粒子 pass 自动追加 */
}
```

## 行为契约

- **确定性**：同 seed + 同 step 序列 => 逐帧粒子状态逐位相等。
- **dt<=0**：不发射、不推进。
- **池满**：发射静默截断到可用槽位；不报错、不崩溃。
- **despawn 回收**：实体消失后，下一次 `ky_world_step` 的 reap 阶段回收其名下粒子
  （扫 `w->free_ids`，O(pool x free_ids)）。
- **颜色/尺寸插值**：`t = age*inv_life`，color/size 线性从 from 到 to。

## 测试

`tests/test_particle.c`（30 断言），ctest 目标 `particle`。覆盖：注册幂等、默认值、
发射计数、寿命回收、dt=0、禁用发射器、同 seed 确定性、池满截断、despawn 回收、
render pass 冒烟（console 后端）。

## 不做（本切片）

3D 粒子、GPU compute、粒子间碰撞、序列帧、kyx 脚本绑定、序列化、编辑器面板。
见 `.monkeycode/specs/particle-system/`。
