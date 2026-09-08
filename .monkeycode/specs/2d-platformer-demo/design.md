# Design: 2D 平台人 Demo

## 文件清单

| 文件 | 动作 | 职责 |
|------|------|------|
| `src/demo/main.c` | 重写 | 反篡改 → 场景搭建 → GLFW 循环 或 headless smoke |
| `src/demo/platformer_world.h` | 新增 | 场景/角色/地面/相机的公共搭建函数，供 demo 与测试复用 |
| `src/demo/platformer_world.c` | 新增 | `platformer_setup(...)` / `platformer_tick(...)` / `platformer_teardown(...)` |
| `tests/test_demo_loop.c` | 新增 | 无头端到端：输入→物理→落地→跟随 |
| `CMakeLists.txt` | 修改 | `ky_platformer` 静态库 + `ky_demo` 依赖 + `add_test` |

## 类型与常量

```c
typedef struct PlatformerWorld {
    kyWorld        *w;
    kyPhysicsWorld *phys;
    kyRenderDevice *rd;
    /* entities + ids */
    kyEntity        role;
    kyEntity        camera;
    uint32_t        role_body;   /* ky_physics id */
    uint32_t        ground_body; /* inv_mass=0 */
    uint32_t        tid_transform;
    uint32_t        tid_sprite;
    uint32_t        tid_camera;
    /* input state */
    int             jump_ready;  /* true 当 |vy|<0.01 */
} PlatformerWorld;
```

常量：`PLATFORMER_ROLE_RADIUS=0.5f`，`PLATFORMER_GROUND_TOP=0.0f`，`PLATFORMER_MAX_SPEED=4.0f`，`PLATFORMER_JUMP_VY=6.0f`，`PLATFORMER_GRAVITY_Y=-10.0f`，`PLATFORMER_CAM_OFFSET_Y=2.0f`。

## 关键 API

```c
int  platformer_setup(PlatformerWorld *out, kyRendererBackend rd_backend,
                      const char *title, int win_w, int win_h);
int  platformer_tick(PlatformerWorld *pw, float dt,
                     float vx_target, int jump_requested);
void platformer_teardown(PlatformerWorld *pw);
```

`platformer_setup`：
1. 建 world、注册 2D 组件、建 RenderDevice
2. 建 Ground：box collider `(0,-1,0)` half `(20,1,1)`，RigidBody `inv_mass=0`
3. 建 Role entity + Transform + Sprite + sphere collider + RigidBody `inv_mass=1/restitution=0`
4. 建 Camera entity + Camera2D active

`platformer_tick` 每帧：
```
vx = ky_math_clampf(vx_target, -MAX_SPEED, MAX_SPEED)
cur = get_body(role); cur.linear_velocity.x = vx
if (jump_requested && pw->jump_ready) cur.linear_velocity.y = JUMP_VY
ky_physics_step(phys, dt)
cur = get_body(role)
set Transform.pos = (cur.position.x, cur.position.y)
pw->jump_ready = fabsf(cur.linear_velocity.y) < 0.01f && cur.position.y <= GROUND_TOP + ROLE_RADIUS + 0.05f
if (fabsf(cur.linear_velocity.y) < 0.01f && cur.position.y <= GROUND_TOP + ROLE_RADIUS + 0.05f)
    cur.linear_velocity.y = 0.0f;  /* 静止 */
set Camera2D.pos = (cur.position.x, cur.position.y + CAM_OFFSET_Y)
ky2d_render_world_auto(rd, world)
```

`platformer_teardown`：按创建逆序释放。

## 输入源解耦

- **有头模式**（demo 走）：`ky_engine_key_pressed(GLFW_KEY_A/D/…)` → `vx_target` / `jump_requested`
- **无头模式**（demo headless smoke、test_demo_loop 走）：直接调用 `platformer_tick(pw, dt, vx_target, jump_requested)` 传入期望值

两条路径走同一个 tick，测试不需要模拟 GLFW。

## GL vs Console 渲染设备

`ky_rd_create(backend, NULL)`：
- 后端为 `KY_RENDERER_GL` 且构建时 `KY_HAS_EGL=1`：走 pbuffer + GLES3，可 `glReadPixels` 验证
- 否则或 `KY_RENDERER_CONSOLE`：只输出调试清单，不占 GPU

demo `main()` 的选择策略：
```
try GL first (ky_rd_create(GL, NULL))
if 失败 fallback CONSOLE
```
`platformer_setup` 接受 backend 参数，调用者决定。

## 反篡改

`main()` 第一件事：
```c
KyTamperResult r = ky_tamper_init(KY_TAMPER_MODE_ERROR, 64u*1024u*1024u);
if (r != KY_TAMPER_OK) { fprintf(stderr, ...); return 1; }
```
`platformer_teardown` 后 `ky_tamper_shutdown()`。

## 无头 smoke 序列（demo main 尾部）

1. `A` 10 帧 → 记录 `role.x`
2. `D` 10 帧 → 记录
3. `SPACE` 3 帧 → 记录 `vy>0`
4. 空 30 帧 → 断言落地到 `y ≈ 0.55`
5. 打印 OK 后 `return 0`

## 测试断言

`tests/test_demo_loop.c`（console 后端，无 GL）：

1. setup 成功
2. 静止 tick 20 帧后 role 停在 `y ≈ ROLE_RADIUS` 附近（±0.1 EU）
3. 左移 10 帧后 `role.x < start_x`
4. 右移 10 帧后 `role.x > start_x`
5. 跳跃后 3 帧内 `role.y > GROUND_TOP + RADIUS`
6. 相机 `Camera2D.pos.x == role.pos.x`（同 tick 后）
7. teardown 无 crash

## 风险与回退

- 若 `ky_physics_step` 对静态刚体 push 分离后 Role 位置漂移到 `>1.0`：把 `jump_ready` 阈值放宽到 `y <= GROUND_TOP + ROLE_RADIUS + 0.3f`
- 无头时 `ky_rd_create(GL, ...)` EGL 报错：直接 fallback console，不视为失败
- GLFW 初始化失败：demo 走 headless smoke 路径，`return 0`；不再让"无显示器"造成 `exit 1`
