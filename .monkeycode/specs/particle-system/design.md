# 2D 粒子系统（G10）

Feature Name: particle-system
Updated: 2026-09-29

## Description

新增 2D 粒子系统模块（`src/render/particle2d.c` + `include/kronyx/particle2d.h`），
按 ECS 组件（`"emitter"`）+ 内置系统（`"particle-update"`）+ `render_frame` 内置粒子
pass 三件套落地。粒子模拟为确定性 LCG 随机源，池容量 16384 静态；渲染走既有 RHI
vtable，console / GL 后端零改动，宿主零配置（粒子 pass 由 `2d.c` 的 `render_frame`
在 sprite 批处理之后自动驱动）。

前置：G7（sprite 帧动画）已完成。

## Architecture

```mermaid
graph TD
    subgraph ECS
        E["emitter 组件 (kyEmitter)"] --> P["particle-update 系统 (ky_world_step 驱动)"]
    end
    subgraph Particle2D 模块
        P --> SIM["模拟: 发射 / 寿命 / 重力 / 颜色插值"]
        SIM --> POOL["静态粒子池 KY_PARTICLE_MAX=16384"]
        POOL --> RENDER_PASS["ky_particle2d_render_pass"]
    end
    subgraph 2D 渲染 (2d.c 改动)
        RENDER_PASS --> RF["render_frame"]
        SPR["sprite 批处理 pass"] --> RF
        RF --> SUB["ky_rd_submit / ky_rd_present"]
    end
    subgraph RHI vtable (console + GL, 零改动)
        SUB --> BACKEND["draw_pass -> glDrawElements / console"]
    end
    DTOR["emitter 组件 dtor"] --> RECLAIM["回收该 emitter 名下存活粒子"]
```

## Components and Interfaces

### `include/kronyx/particle2d.h`（新增）

```c
typedef enum kyEmitShape { KY_EMIT_POINT, KY_EMIT_CIRCLE, KY_EMIT_LINE } kyEmitShape;

typedef struct kyEmitter {
    int      enabled;       /* 0/1 */
    float    emit_rate;     /* 粒子/秒, >0 */
    kyEmitShape shape;
    float    shape_scale;   /* 半径(CIRCLE)/宽(LINE), 0=单点 */
    float    direction;     /* 弧度, 0 = +X */
    float    speed0, speed1;/* 初速区间 EU/s */
    float    life0, life1;  /* 寿命区间 秒 */
    float    size0, size1;  /* 尺寸区间 EU */
    kyVec4   color_from, color_to; /* RGBA 线性插值端点 */
    kyVec2   gravity;       /* EU/s^2 */
    float    drag;          /* 1/s, 速度衰减系数 */
    kyTexture *texture;     /* NULL -> 内置 white */
    uint32_t seed;          /* LCG 随机源种子 */
    /* 运行时状态（模块内部读写） */
    float    emit_acc;      /* 发射余量累加器 */
    uint32_t rng_state;     /* LCG 当前状态 */
} kyEmitter;
```

```c
KY_API int ky_particle2d_register(kyWorld *w);
KY_API kyEmitter ky_emitter_new(void);
KY_API int  ky_particle2d_render_pass(kyRenderDevice *rd, void *cl,
                                      const kyCamera2D *cam, const kyTransform *cam_tr);
KY_API int  ky_particle2d_alive_count(void);
#define KY_PARTICLE_MAX 16384
```

emitter 与存活粒子的绑定：每个池条目带 `owner` 字段（实体 id）；despawn 时该实体的
emitter 组件 dtor 线性扫池、回收 `owner == 本实体` 的存活条目（16384 规模可接受，
无额外堆分配，符合进程内静态池模式）。

### `src/render/2d.c`（小改）

`render_frame` 在 `while (i < g_item_count)` sprite 批循环之后、`ky_rd_submit(rd, cl)`
之前，插入：

```c
if (ky_particle2d_registered()) ky_particle2d_render_pass(rd, cl, cam, cam_tr);
```

`ky_particle2d_registered()` 为模块内全局标志（`ky_particle2d_register` 置位），
未注册时跳过，既有 demo / 测试路径行为不变（零回归风险）。

### ECS 注册（`ky_particle2d_register` 内部）

- 组件 `"emitter"`：ctor = 写 `ky_emitter_new()` 默认值，dtor = 回收 `owner_id == 本实体`
  的存活粒子（线性扫粒子池 alive 位图）。
- 系统 `"particle-update"`：order 0，update 函数遍历 `["emitter"]` 视图，
  按 `dt` 推进每个 emitter 的发射累加器与存活粒子运动学。

## Data Models

**Particle 池条目（模块内部，不外露）**

```c
typedef struct KyParticle {
    float pos[2], vel[2];
    float age, life;
    float size0, size1, size_inv_life; /* 预计算 1/life 避免每帧除法 */
    uint8_t cr, cg, cb, ca,  fr, fg, fb, fa; /* from/to RGBA (0-255) */
    uint32_t owner;          /* 发射该粒子的实体 id, despawn 回收用 */
    uint32_t free_next;     /* 空闲链表 */
} KyParticle;
```

池 = `static KyParticle g_pool[KY_PARTICLE_MAX]` + `static uint32_t g_free_list[KY_PARTICLE_MAX]`
（与 `2d.c` 的 `g_items`/`g_vbo` 同模式：文件级静态，进程内一份）。

**粒子顶点**：复用 `2d.c` 已有的 `ky2dVertex` 布局（pos xy + uv xy + color rgba），
与 sprite 同一 pipeline，保证 `render_pass` 可复用 sprite pass 的 pipeline/texture slot。

## Correctness Properties

1. **池不变式**：`alive_count + free_count == KY_PARTICLE_MAX` 恒成立（每次发射从 free 摘除，
   寿命到/despawn 归还；测试断言）。
2. **确定性**：同 seed + 同 step 序列 => 逐帧粒子状态逐位相等（测试断言）。
3. **dt=0 无操作**：不发射、不推进年龄、不推进累加器（测试断言）。
4. **截断语义**：free 不足时发射数 = min(应发, free)；staging 容量不足时截断至容量。
5. **despawn 回收**：实体消失后，其 `owner == id` 的粒子在本帧模拟结束后不再存活
   （dtor 在 `ky_world_despawn` 同步触发；测试断言 despawn 后 `alive_count` 归零）。
6. **浮点确定性**：颜色/尺寸插值用 `from + (to-from)*t`（t 单变量），不用分步累加，
   保证同 t 必同值。

## Error Handling

- `ky_particle2d_register(NULL)` 返回 -1，不触碰全局池。
- 粒子纹理 NULL -> 内置 white（`2d.c` 已创建 `g_cache.white`；粒子 pass 直接复用）。
- staging 容量不足 -> 静默截断 + 单次 WARN log（不每帧刷）。
- `render_pass` 在 `cl == NULL` 或 `rd == NULL` 时返回 -1 并跳过绘制。
- emitter 配置非法（emit_rate<=0、life0<0 等）-> 该 emitter 不发射，模拟继续。

## Test Strategy

`tests/test_particle.c`（挂 CMake/ctest，目标名 `particle`，断言 ~40 条）：

| 覆盖点 | 断言 |
|--------|------|
| 注册 | `ky_particle2d_register(w)==0`；重复调用幂等；NULL world 返回负值 |
| 默认值 | `ky_emitter_new()` 各字段符合 spec |
| 发射计数 | rate=10, dt=0.1 x 5 step => 恰好 5 个存活（确定性 floor 累加器） |
| 寿命回收 | life0=life1=1.0, 1.5 step 后存活数回到 0；free 列表长度恢复 |
| 重力数值 | gravity=(0,-9.8), life=1s 的粒子 vy 精确等于 -9.8*age（逐帧比对，确定性） |
| 颜色插值 | t=0.5 时 color = (from+to)*0.5（逐通道比对） |
| 尺寸插值 | t=0 为 size0，t=1 为 size1 |
| 池满截断 | 灌满 16384 后发射 rate=100 不崩溃，存活数保持 16384 |
| dt=0 | 不发射不推进 |
| despawn 回收 | spawn 实体+emitter，发射后 despawn => alive_count 归零 |
| 确定性 | 同 seed 两次 world 序列逐帧 alive 集合与颜色相等 |
| render_pass 冒烟 | console rd + 粒子 1000 个，`ky2d_render_world` 走粒子 pass 不崩溃，返回绘制数含粒子 |

回归门限：全量 `ctest -j1` 22/22（原 21 + 新增 particle）绿；ASan `detect_leaks=0`
同样全绿（粒子池为 static，不产生堆泄漏）。

## References

[^1]: [src/render/2d.c#L453-L495](src/render/2d.c#L453) - `render_frame` sprite 批循环与 submit 位置（粒子 pass 插入点）
[^2]: [include/kronyx/2d.h#L94-L101](include/kronyx/2d.h#L94) - `ky2dContext` / `ky2d_render_with`（保持语义不变）
[^3]: [include/kronyx/anim2d.h#L52-L56](include/kronyx/anim2d.h#L52) - `ky_anim2d_register` 注册模式（组件+系统），粒子注册参照
[^4]: [src/render/anim2d.c#L89-L105](src/render/anim2d.c#L89) - 系统注册实现细节
[^5]: [progress.md G10 行](progress.md) - 前置 G7 已满足，本切片为"下一刀"
