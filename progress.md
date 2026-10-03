# Kronyx 后续生成大纲

本文件是后续 Agent 的工作入口。先读本文件，再动手。`progress.log` 只作历史流水，不作为优先级来源。

更新日期：2026-09-08

---

## 1. 当前快照

| 层 | 状态 | 活文件 |
|----|------|--------|
| Core | 可用 | math / memory / arena / pool / array / hashmap / string / log / time / event / input / file |
| ECS | 可用 | archetype ECS，O(1) 组件查找 |
| 2D 渲染 | 可用 | Transform / Sprite / Camera2D + sprite batch + `ky2d_make_texture`；console + EGL/GLES3 |
| 资源 | 可用 | `ky_file_read/free` + `ky_resmgr_make_pixelbuffer/raw_bytes` + payload/on_destroy；28 断言 |
| 物理 | 可用 | 刚体 + SAP(X) + AABB 窄相 + 力场(64) + raycast |
| 脚本 kyx | 可用、有债 | lexer/parser/VM，113 测通过；无 GC；算术统一 double；stdlib 仅 `std.print` |
| 打包 | 部分 | exe / npm / jar 可用；apk 占位 |
| 运行时 | 可用 | GLFW 窗口循环 + ky_demo（2D 平台人，走 ECS+`ky2d_render_world`；无头冒烟 OK） |
| 反篡改 | 完成，停手 | `anti_tamper_game.c` + `anti_tamper_dll.cpp` + `anti_tamper.h`；21 断言；勿再扩 |
| 编辑器 | 骨架 | `tools/editor`，`KYR_BUILD_EDITOR` 默认 OFF |
| 音频 / 动画 / 关节 / Vulkan | 不存在 | 不要提前生成 |

已删死文件：`src/engine/anti_tamper.c`、`src/engine/anti_tamper_export.c`、`include/kronyx/anti_tamper_dll.h`。

构建：`cmake -B build && cmake --build build -j2`  
测试：`ctest --test-dir /workspace/build --output-on-failure -j1`  
`ky_test_render` 在 EGL 软渲染下可能偶发 SIGSEGV，单独重跑即可，与业务无关。

---

## 2. 生成理念

后续生成只走**产品闭环**，不走平行功能堆砌。

1. **先接上，再做深。** 已有模块必须先被 demo 真正调用。禁止在 demo 仍用裸 GL 时再开 3D / Vulkan / 音频。
2. **一次一个垂直切片。** 每个对象必须按 `规格 → 实现 → 测试 → 回写本文件` 走完，再开下一个。规格落在 `.monkeycode/specs/{name}/`（`requirements.md` + `design.md`）。
3. **生成对象必须可测。** 新 API 必须有 `tests/test_*.c` 并挂进 CMake/`ctest`。无测试的模块视为未完成。
4. **复用现有抽象。** 渲染走 RHI vtable（console / GL），2D 走 `ky2d_*`，输入走 `ky_input_*`，事件走 `ky_event_*`。禁止在 demo 里再写一套矩阵/着色器。
5. **债比新功能优先，当它挡住闭环时。** 脚本无 GC、算术丢 int——只有挡住当前切片才修；否则记在「已知债」，不单独开坑。
6. **反篡改冻结。** 算法、密钥解析、双组件架构已对齐。除非验证失败回归，否则不改 HMAC / TOTP / 密钥格式。
7. **环境约束。** 无显示器；GLFW demo 启动失败是预期。可测路径是 ctest + EGL 无头。新渲染验证用 `glReadPixels`，不用弹窗。
8. **文档只写活契约。** 新模块补 `.monkeycode/docs/{module}.md`。不要把过程日记写进文档。

切片完成判定（四条全满足才勾）：

- 公共头文件 API 稳定，命名与邻模块一致
- 至少一条端到端测试在 `ctest -j1` 下稳定绿
- `progress.md` 本切片状态改为完成，并写清下一刀切哪里
- 没有引入未挂构建的新 `.c/.h`

---

## 3. 生成对象（按优先级）

只生成下表中的对象。表外想法先写进「候选」，经确认再升级。

### P0 — 把引擎跑成游戏循环（立刻做）

| ID | 对象 | 生成什么 | 不生成什么 | 完成标准 |
|----|------|----------|------------|----------|
| G1 | **2D 平台人 demo** ✅ | 用 ECS + Transform/Sprite/Camera2D + `ky2d_render_world` + `ky_input_*`，替换 `src/demo/main.c` 的裸 GL 方块；无头可跑的逻辑测试（输入模拟 → 位置变化） | 地图编辑器、动画帧、音频、粒子 | 完成：demo 走 2D 管线；`tests/test_demo_loop.c` 13 断言绿；`ky_demo` 无头冒烟 exit=0 |
| G2 | **资源加载最小集** ✅ | `ky_file_read/free`（malloc/free，跨平台）+ `ky_resmgr_make_pixelbuffer/raw_bytes`（payload+on_destroy，重复 path 幂等）+ `ky2d_make_texture` 校验层；demo 挂 hero 纹理（资产缺失回落白色） | PNG 解码器、热重载、材质系统 | 完成：`tests/test_resource_flow.c`（28 断言）+ demo_loop 断言 `role_tex`；ASan+Leak 干净 |

G1 是整条产品线的主轴。G2 已在 G1 绿后落地最小集。

### P1 — 让脚本驱动场景（G1 绿之后）

| ID | 对象 | 生成什么 | 不生成什么 |
|----|------|----------|------------|
| G3 | **kyx 游戏绑定** ✅ | native（`KyxBinds`）：`std.spawn/despawn/alive`、`std.setpos/getpos`、`std.setscale/getscale`、`std.poll_input/axis_x/last_key`、`std.log`；实体句柄用 int64(version<<32\|id) 跨边界 | 类/metatable、完整 stdlib、GC |
| G4 | ~~脚本债~~ ⏸ | 未挡路：G3 绑定通过 double 语义正常工作；ADD/SUB/MUL int 丢失、VM GC 待长驻 VM 场景触发时再修 | 不单独开坑 |

### P2 — 编辑器接到真世界（G3 可玩之后）

| ID | 对象 | 生成什么 | 不生成什么 |
|----|------|----------|------------|
| G5 | **编辑器接 ECS 世界** ✅ | `editor_core.h/c`：Hierarchy/Properties/Viewport 三面板核心；实体句柄跨边界 + spawn/despawn 追踪；`ky_world_alive_count` + `ky_world_get_alive_entity` 新增 ECS API；`test_editor_world.c`（30+ 断言） | 可视化脚本、插件市场、独立渲染器 |
| G6 | **场景序列化** ✅ | `kyScene` 把实体+Transform+Sprite+Camera2D 写成可读格式并读回 | 通用反射、二进制版本迁移 |

打开编辑器构建：`KYR_BUILD_EDITOR=ON`，并给一条可在无窗环境跑的面板/序列化测试。

### P3 — 有真实游戏后再加的系统（不要提前）

| ID | 对象 | 前置 | 备注 |
|----|------|------|------|
| G7 | **Sprite 帧动画** ✅ | G1 | 时间轴 + 图集 UV，不接骨骼 |
| G8 | **碰撞回调导出** ✅ | G1 | 物理 contact → `ky_event_*`，供 demo/脚本 |
| G9 | 关节 / constraints | 3D 或复杂 2D 需要时 | 现物理无 joint API |
| G10 | **粒子** ✅ | G7 之后 | `particle2d.h/c`：emitter 组件 + particle-update 系统 + 16384 静态池 + xorshift32 确定性模拟 + `render_frame` 内置粒子 pass（console/GL 零改动）；`test_particle.c` 30 断言；demo role 跳跃/落地脉冲 |
| G11 | **音频** ✅ | 至少 G1 可玩 | `audio.h/c`：参数化 SFX 合成器（正弦扫频/方波/噪声/单击）+ 32 并发 voice + null-sink mix buffer（进程内 float，零系统库依赖）+ ECS `"sound"` 组件 + `audio-update` 系统；`test_audio.c` 61 断言；demo 跳跃/落地 SFX 脉冲 |
| G12 | Vulkan 后端 | 真 GPU 环境 | 枚举已预留 `KY_RENDERER_VULKAN`；软渲染环境不做 |
| G13 | 3D Renderer 组件 | Vulkan 或真 GL 3D 需求 | 2D 管线不冒充 3D |
| G14 | APK 打包 | 移动目标明确时 | 现返回 not-supported |
| G15 | 脚本 GC / 对象模型 | 长驻 VM 泄漏成为事实 | 现在是 strdup/free |

### 明确不做

- 继续改反篡改协议、密钥格式、轮次时序
- 为反篡改再写第三套实现
- 无规格的「通用框架」重构
- 在 demo 里绕过 RHI/2D 再写一套渲染
- 把 `progress.log` 里的虚勾当需求源

---

## 4. 已知债（挡住当前切片才修）

| 债 | 位置 | 何时修 |
|----|------|--------|
| 脚本无 GC，字符串/全局名 strdup 累积 | `src/script/` | 长驻 VM 测试泄漏时 |
| ADD/SUB/MUL 运行时输出 double，int 不保留 | `vm.c` | 绑定或脚本测试需要 int 语义时 |
| stdlib 仅 `std.print` | script | G3 绑定里按需加，不先做 table/math 全家桶 |
| 命令列表无 cancel，begin 不 submit 泄漏 | `render.c` | 编辑器/demo 出现中途丢帧时 |
| 2d GPU 资源按单 RenderDevice 缓存 | `src/render/2d.c` | 多设备或销毁后复用时 |
| SAP 只扫 X 轴 | physics | 漏碰撞成为 demo 事实时 |
| 无 joints / 音频设备后端 | — | G11 音频合成+混音已完成（null sink），设备输出与 joints 见 P3 |
| 编辑器默认不进 CI | CMake | G5 开工时打开 |
| `ky_test_render` EGL 偶发 segfault | 环境 | 不修业务；`ctest -j1`，失败重跑 |
| `ky_demo` 无显示时 GLFW init 失败 | 环境 | 预期；逻辑测试走 ctest |

---

## 5. 下一刀（现在就做这个）

**已完成：G2 资源加载最小集。**
交付：
- `include/kronyx/file.h` + `src/core/file.c`：`ky_file_read/free`（`fstat`+`fopen/fread`，malloc/free，256 MiB 上限；挂进 `ky_core`）
- `include/kronyx/resource.h` + `src/resource/resource.c`：`kyResource` 加 `payload/on_destroy(alloc,payload)`，`KY_RES_PIXELBUFFER` + `kyPixelBuffer`，`make_pixelbuffer/make_raw_bytes` 便捷构造（幂等 + 引用计数 + 追踪 allocator 一致）
- `include/kronyx/2d.h` + `src/render/2d.c`：`ky2d_make_texture`（参数校验 + 转 `ky_rd_create_texture_2d`）
- `tools/mk_demo_asset.c` 生成 `assets/hero_8x8.bin`（256 B RGBA8）；CMake 加 `demo_asset` 目标
- `src/demo/platformer_world.{h,c}`：`role_tex` 字段 + `demo_load_texture`（资产缺失回落白色 sprite）+ teardown 释放
- `tests/test_resource_flow.c`（28 断言）+ `tests/test_demo_loop.c` 加 `role_tex != NULL` 断言

ASan+Leak 干净；全量 `ctest -j1` 14/14 绿；`ky_demo` 无头 exit=0，角色带纹理。

**已完成：G3 kyx 游戏绑定。**
交付：
- `include/kronyx/bindings.h` + `src/script/bindings.c`：`KyxBinds` 上下文 + `ky_bind_create(vm, world)` / `ky_bind_destroy`；11 个 native（`std.spawn/despawn/alive/setpos/getpos/setscale/getscale/poll_input/axis_x/last_key/log`）
- 实体句柄跨边界用 `int64(version<<32 | id)` 打包；**作为 `ky_vm_call` 实参传入**（`op_loadint` 只有 32 位，`op_return` 会把 int 强转 double，故字面量与 return 值都不可承载 64 位句柄）
- `std.log`：`vm.c` 修复 native 调用分发；测试覆盖脚本 spawn→C 侧读回一致性、setpos/getpos、setscale/getscale、input 模拟→axis_x/last_key、null world 错误串
- `tests/test_kyx_bindings.c`（30 断言）+ CMake `ky_test_kyx_bindings`
- 附带修复：`src/ecs/ecs.c` `move_entity` 迁移时**不重复调用旧组件的 ctor**（对齐 `test_ecs.c::test_ctor_not_recalled_on_migrate`）

ASan（`detect_leaks=0`）全量 `ctest` 15/15 绿；`ky_demo` 无头 exit=0。

**已完成：G5 编辑器接 ECS 世界。**
交付：
- `include/kronyx/editor_core.h` + `tools/editor/src/editor_core.c`：`KyEditorApp` 核心（Hierarchy 列表、Properties 读写 Transform/Sprite/Camera2D、Viewport 渲染、输入轮询）；实体追踪用 spawned 列表避免空洞计数
- `src/ecs/ecs.c` 新增：`ky_world_alive_count` / `ky_world_get_alive_entity` / `ky_world_component_type_by_name`
- `tests/test_editor_world.c`（30 断言）：null 安全、Hierarchy 计数、Transform 读写、Sprite layer、Camera zoom/viewport、spawn/despawn、render 冒烟、input poll、world 同步验证
- CMake：`editor_core` 静态库 + `ky_test_editor_world` 测试目标（无条件编译，无需 ImGui/GLFW）

ASan（`detect_leaks=0`）全量 `ctest` 16/16 绿。

**已完成：G6 场景序列化。**
交付：
- `include/kronyx/scene.h` + `src/scene/scene.c`：`kyScene` 结构体 + `ky_scene_create/destroy/reinit_world/load/save` API
- `.ksn` 文本格式：scene header (`scene "name" v1`)、entity 块（`transform` / `sprite` / `camera2d` 组件，属性用 `key="value"` 格式）
- 状态机解析器（`ST_OUTSIDE` → `ST_ENTITY`），逐行处理，组件声明行内解析全部属性，引号值正确剥离
- 加载时校验每实体必须含 Transform，否则返回 -4
- `tests/test_scene_serialize.c`（50 断言）：roundtrip、save 可读性、malformed 场景错误处理
- CMake：`ky_test_scene_serialize` 测试目标

ASan（`detect_leaks=0`）全量 `ctest` 18/18 绿。

**已完成：G8 碰撞回调导出。**
交付：
- `include/kronyx/physics.h`：公开碰撞事件契约 `#define KY_EVENT_COLLIDE "collide"` + `kyCollision { body_a, body_b }` payload
- `src/physics/physics.c`：`ky_physics_step` 对每个存活 contact pair 同步触发 `KY_EVENT_COLLIDE`，payload 由内部 `kyContactPair*` 改为公开 `kyCollision` 值（回调内有效，需即时拷贝）
- `tests/test_physics.c`（+7 断言）：注册 `ky_event_register(KY_EVENT_COLLIDE, ...)`，两重叠静态 box step 后断言事件触发 ≥1 次、payload 携带两 body id 且与 body 匹配
- 消费方：demo/宿主代码 `ky_event_register(KY_EVENT_COLLIDE, fn, user)` 即可收到每 step 每存活接触对的回调；脚本侧桥接留待绑定层（kyx 尚无 event native）

**已完成：代码体检——`src/script/vm.c`（967→929 行，-38 净减，行为等价）。**
交付（全部为死代码/冗余消除，普通 + ASan 全量 18/18 保持全绿）：
- VM 结构瘦身：删 `protos_code[KYX_MAX_PROTOS*256]`（约 1 MiB calloc 空间，实际从未使用，代码走动态 malloc）、`frames[]/frame_count`（写不读）、`locals[]`、`strings[]/string_count`、`native_names[]`（strdup 冗余，`natives[i].ns` 已是 64B 副本）、`error_flag`/`running`（写不读）
- 删死类型：`kyFrame`、`kyArray`；`kyClosure.upvals/n_upvals`、`proto.max_stack`（两处 `=16` 死赋值）
- `ky_vm_create` 精简：calloc 全零即 KYT_NIL/0（`KYT_NIL==0`），删除全部显式清零与两个初始化循环
- `ky_vm_register_native` 不再 `strdup` namespace（消除每次注册 1 次小分配 + destroy 释放路径）
- `OP_GETGLOBAL` / `OP_NATIVECALL` native 查找改用独立 `native_count` 计数，修正原代码用 `proto_count` 做上限的越界风险
- `compile_expression` 赋值操作符检测从 5 次 `strcmp` 链改为两字符直接比较

**已完成：G10 2D 粒子系统。**
交付：
- `include/kronyx/particle2d.h` + `src/render/particle2d.c`：`kyEmitter` 组件（发射速率/形状/初速/寿命/颜色插值/重力/drag/纹理/seed）+ `ky_particle2d_register(w)`（注册 "emitter" 组件 + "particle-update" 系统，幂等）
- 模拟：xorshift32 LCG（可设 seed，确定性可复现）、静态粒子池 `KY_PARTICLE_MAX=16384`、寿命/重力/drag 推进、池满静默截断
- 渲染：`ky_particle2d_render_pass(rd,cl,cam,cam_tr)` 生成 quad 顶点/索引到模块 static staging，per-device GPU 资源 lazy init；`2d.c` `render_frame` 在 sprite 批处理后、submit 前调用（`ky_particle2d_registered()` 守卫）；console / GL 后端零改动
- despawn 回收：ECS dtor 无实体 id，`reap_dead_owners` 在每次 step 前扫池，owner id 出现在 `w->free_ids` 即回收
- `src/demo/platformer_world.{h,c}`：role 挂 emitter，跳跃/落地脉冲（emit_pulses 计时器），`ky_world_sort_systems` + `ky_world_step` 纳入 tick；`ky_demo` 无头冒烟 exit=0
- `tests/test_particle.c`（30 断言）：注册幂等 / 默认值 / 发射计数 / 寿命回收 / dt=0 / 禁用发射器 / 同 seed 确定性 / 池满截断 / despawn 回收 / render pass 冒烟
- CMake：`particle2d.c` 入 `ky_engine`，`ky_test_particle` 挂 ctest（目标 21→22）

普通构建 22/22 绿；ASan（`detect_leaks=0`）22/22 绿。

**已完成：G11 音频模块（合成 + 混音，null sink）。**
交付：
- `include/kronyx/audio.h` + `src/audio/audio.c`：`ky_audio_synthesize()` 参数化合成 4 种 SFX（`KY_SFX_SINE_SWEEP` / `SQUARE` / `NOISE` / `CLICK`），xorshift32 LCG 确定性、软限幅到 [-1,1]；voice 表 `KY_AUDIO_MAX_VOICES=32` 并发；null-sink mix buffer（进程内 `float[44100*2]`，零系统音频库依赖，与 console 渲染同策略）
- ECS 集成：`kySoundEmitter` 组件 + `audio-update` 系统，`trigger` 帧置位自动播放；`ky_audio_register_components(w)` 幂等注册
- **关键设计**：clip 被宿主提前 `ky_audio_clip_free` 时 voice 仍持有指针，`alive` 标志在已释放结构体里不可读（UAF）。改用**模块自有 dead-clip 集合**（`g_dead_clips[32]`）在模块内存中跟踪已释放指针，`ky_audio_mix` 查表而非解引用，ASan 干净
- `src/demo/platformer_world.{h,c}`：跳跃 SINE_SWEEP(300→900Hz, 0.08s) + 落地 CLICK(0.04s) 脉冲；每 tick `ky_audio_mix(dt)`；teardown 释放 clip + `ky_audio_shutdown()`
- `tests/test_audio.c`（61 断言）：合成合法/非法/确定性/限幅、播放参数校验、voice 满 32 截断、mix 帧数/回收/dt<=0、ECS trigger 自动播放、提前 free clip 安全跳过、shutdown 幂等
- CMake：`audio.c` 入 `ky_engine`；`ky_test_audio` 挂 ctest（目标 22→23）

普通构建 23/23 绿；ASan（`detect_leaks=0`）23/23 绿；`ky_demo` 无头冒烟 exit=0（ASan 下 demo 残留 LLVM/GL 间接泄漏 112 B 为既有 G15 债，非音频引入；音频测试自身 0 泄漏）。

**已完成：代码体检——`src/render/render.c`（`ky_rd_submit` 去重，-3 行，行为等价）。**
交付：
- `ky_rd_submit` 原对 `draw_pass_hook` 分支与无 hook 分支各调一次 `rd->vt->submit`；合并为单条 `if (hook) hook(...); submit(...)`，`return` 冗余消除（原本就在函数尾）
- `tests/test_hooks.c::test_render_hook` 已守护 hook 契约（submit 时 hook 必被调用），去重后全量回归无损

体检中识别但未改的 2d.c 隐患（保留现状，行为正确）：
- `cache_init` 切设备路径里 `cache_destroy_with_rd`（已 `memset 0`）后又调 `cache_destroy()`（再 `memset 0`）——**重复清零**
- 若删除 `cache_destroy()`，"同设备部分失败重试"场景会泄漏半成品 shader（`cache_destroy` 承担了丢弃未完成资源的路径），故现状为正确行为，非可无损删除的死代码

普通 + ASan 全量 `ctest -j1` 23/23 绿；`ky_demo` 无头冒烟 exit=0。

**已完成：代码体检（第一轮）——`src/core` 全 12 文件扫描，改动 2 处。**
交付：
- `src/core/string.c`：`ky_string_reserve` 的 `while (nc < cap) nc *= 2;` 无 SIZE_MAX 保护，巨大 `cap` 可整数溢出/死循环——对齐 `array.c` 既有写法加 `if (nc > SIZE_MAX/2) { nc = SIZE_MAX; break; }` 溢出闸
- `src/core/event.c`：`ky_event_register` 原扫 32 槽 registry 两遍（dup-check 一遍 + `find_name_count` 一遍）——合并为单趟（同时算 `prior` 计数与 `dup` 判定），删死函数 `find_name_count`
- 体检中识别但保留现状的隐患（行为正确，非本轮无损范围）：
  - `math.c::ky_ray_aabb` 形参 `o`/`d` 经 `&o.x`/`&d.x` 取址使用，并非死参（已复核，不改）
  - `event.c::ky_event_trigger` 每次对 32 槽全 `strcmp` 匹配；因 `g_registry` 全局 + `name_count` 同名多监听器语义，O(1) 定位需按 name 索引化（行为大改，留待后续）
  - `pool.c::pool_grow` 用 `old_head` 局部别名而非 `p->head`（纯可读性，行为等价，不动）

普通 + ASan 全量 `ctest -j1` 23/23 绿；`ky_test_core`(95 断言)/`core_quality`(437)/`event`(12) 定向全绿；`ky_demo` 无头冒烟 exit=0。

**已完成：全项目体积 + 算法优化（2026-10-01）。**
两个并行体检 agent 扫遍 script/render/physics/scene/ecs 全部模块，挑出"行为等价或纯修复"项落地：
- `src/ecs/ecs.c`：`move_entity` 原型移动里 `new_types ∩ old_types` 是 O(a×b) 嵌套查找——两数组均已排序，改为双指针归并单趟 O(a+b)，同趟完成"新列 ctor / 共享列 memcpy / 旧列 dtor"（组件增删热路径）
- `src/render/particle2d.c`：`emit_one` 每次发射都查一次 `"transform"` 类型 ID——提升到 `step_particles` 查一次传参，N 粒爆发省 N-1 次 hash 查表；删死宏 `KY_PARTICLE_OWNER_NONE`
- `src/script/gc.c`：`ky_gc_run_nursery`/`run_full` 的 `malloc` OOM 降级路径 `calloc(1,1)` 后 `root_cap=1`，但 `collect_roots` 按原 `root_cap` 写 `out[0..cap-1]` 会越界——改为 `roots=NULL; root_cap=0`，`collect_roots` 的 `*count<cap` 边界自然跳过，安全降级（真安全修复）
- `src/scene/scene.c`：save/load 各扫一遍组件类型找 transform/sprite/camera 三 ID——提取 `scene_lookup_type_ids` 共用；删死变量 `cur_ent`/`has_transform`/`saved`
- `src/render/console_backend.c`：`kyConsoleCmdList` 的 `pipeline_id`/`vbo_stride`/`depth_write` 只写不读——删 3 死字段；buffer 的 `b->data` 堆内存分配但 draw 从不读回（console 后端是纯打印后端）——create 不再分配 backing buffer，省 ~32KB 堆
- `src/render/gl_backend.c`：真/stub 两变体的 `frame_count` 只 `++` 从不读——删
- demo 体积 115888 → 111792 字节（省 ~4KB）

普通 + ASan 全量 `ctest -j1` 23/23 绿；`ky_demo` 无头冒烟 exit=0。

**体检中识别但保留现状的项**（需测试验证或涉及公开 API，非本轮无损范围）：
- `physics.c::sap_find_pairs` O(n²) active 扫描（SAP 核心路径，n≤1024，改 active list 需大量碰撞测试）
- `bindings.c::tid_transform` 每 native 调用 O(n) 扫组件类型（可缓存，涉及世界生命周期失效边界）
- `vm.c` 魔术数字已枚举化 + 7 死 opcode 已清理（见上方各"已完成"小节）

**已完成：`src/ecs` + `src/engine` 代码体检（2026-10-01）。**
交付（行为等价，仅优化）：
- `src/ecs/ecs.c::ky_world_add_component` / `ky_world_remove_component`：原每次调用堆分配一个 `kyArray tmp` 收集"旧类型±新类型"——改为栈 64 槽缓冲（覆盖绝大多数小原型实体，零堆分配），仅当组件数 >64 时才堆降级。消除组件增删热路径的每调用堆分配
- `src/ecs/ecs.c::move_entity`：（上轮已改）原型移动 O(a×b) 嵌套查找 → 双指针归并 O(a+b)

体检后保留现状的项（行为正确，非本轮无损范围）：
- `engine.c` 与 `glfw.c` 各有独立 `g_key_state` 数组 + `key_callback`：`ky_engine_key_pressed` 读 engine 的数组，`ky_glfw_key_pressed` 读 glfw 的数组，二者互不影响、各自独立——非重叠，不动
- `engine.c::ky_engine_run` 的主循环 `update→render→swap` 结构紧凑，`dt` 钳制 0.1s 合理
- `anti_tamper_dll.cpp`（独立库编译单元，不进主库）与 `anti_tamper_game.c`（集成层）均干净；`#define ROUND_DELAY_MS 800` × 5 轮 = 4s 是设计选择

普通 + ASan 全量 `ctest -j1` 23/23 绿；`ky_demo` 无头冒烟 exit=0。

**已完成：`src/physics` + `src/script`（gc 标记循环提取 + vm 魔术数字）代码体检（2026-10-01）。**
交付（行为等价，仅优化；普通 + ASan 全量 `ctest -j1` 23/23 绿，`ky_demo` 无头冒烟 exit=0）：
- `src/physics/physics_internal.h`：`kyPhysicsWorld` 加 `collider_index[KY_PHYSICS_MAX_COLLIDERS+1]` 反查表（1-based 存储索引，0 = 无）；`add_collider` 时填充；`phys_body_update_aabb`（每帧每 body）+ `cast_ray`（每 body 内层）的 O(collider_count) 扫描改为 O(1) 查表
- `src/physics/physics.c`：`broad_fn` 自定义 broadphase 路径原每帧 `calloc/free` 32KB scratch（exts + ids）——改为 `ky_physics_create` 时预分配、`destroy` 时释放，消除每帧堆抖动
- `src/script/gc.c`：提取 `gc_clear_marks(slab, used)` 辅助函数，替换 `run_nursery`/`run_full` 中 4 处重复的标记清除扫描循环（-51 净减行）
- `src/script/vm.c`：编译器段 `compile_emit` 全部魔术数字 opcode 首参（`0/1/4/5/6..26/30/32/33/41/43/50/52/61/62`）替换为 `OP_*` 枚举常量，字节码数值逐位不变（23/23 全绿守护）；涉及 var_decl/return/if/while/for/literal/ident/binop 赋值+运算符表/unop/call/field 全部发射点

体检后保留现状的项（非本轮无损范围）：
- `physics.c::sap_find_pairs` O(n²) active 扫描：SAP 核心路径，n≤1024，改 active-list 需大量碰撞测试覆盖
- `bindings.c::tid_transform` 每 native 调用 O(n) 扫组件类型：可缓存，但涉及世界生命周期失效边界

**已完成：`vm.c` 死 opcode 清理（2026-10-01）。**
删除 7 个"三无一" opcode（无编译器发射、无运行时 case、无外部引用）：`OP_NEWARRAY/OP_GETINDEX/OP_SETINDEX/OP_SETFIELD/OP_CLOSURE/OP_TAILCALL/OP_INVOKE`（`873941d`）。
- 关键陷阱：旧枚举 `OP_GETINDEX=32`/`OP_SETINDEX=33` 与 `OP_GETGLOBAL=32`/`OP_SETGLOBAL=33` 数值冲突（索引访问语义从未真正实现），删除后冲突自然消除。
- 全部显式 `=N` 锚点保留，已用独立枚举展开脚本逐位核对：所有活 opcode 数值逐位不变（普通+ASan 各 23/23 绿、`ky_demo` exit=0）。
- `OP_LOADINT`/`OP_LOADFLOAT` 保留：运行时确有 case 分支（L140/143）且测试注释引用其"32 位截断"设计语义。

**下一刀：G12 Vulkan 后端（需真 GPU，本无头环境受阻）；或用户指派维护 / 新一轮模块代码体检。**

**已完成：`src/render` + `src/audio` 模块代码体检（2026-10-02）。**
逐一读 `audio.c`/`2d.c`/`particle2d.c`/`render.c`/`gl_backend.c`/`console_backend.c`/`anim2d.c`，定位唯一热路径性能问题并修复：
- `src/render/particle2d.c`：`pool_alloc` 每次 emit 全池线性扫 `KY_PARTICLE_MAX`(16384) 找空槽，改为 lazy-seed free-list 栈（`g_free_stack`/`g_free_top`）O(1) 弹/压（`3d7e176`）。行为等价：粒子测试只断言 `ky_particle2d_alive_count()` 相对差值，从不依赖具体槽位索引，故 LIFO 分配顺序变化安全。
- 体检后保留现状的项：
  - `audio.c::clip_is_dead` O(dead) 扫描：dead 集上界 `KY_AUDIO_MAX_VOICES`(32) 极小，且是防 use-after-free 的正确性机制，不动
  - `2d.c::render_frame` 每帧 `qsort` 排序 sprite（最多 16384）：渲染固有序，`item_cmp` 三级排序必要，不动
  - `2d.c` 批次内 `sp->texture ? ... : white` 表达式重复计算：微优化，不值得动
  - `gl_backend.c`/`console_backend.c` 资源创建/销毁配对正确，无泄漏

**下一刀：G12 Vulkan 后端（需真 GPU，本无头环境受阻）；或用户指派维护 / 剩余模块代码体检（`src/engine`、`src/ecs` 之外的大模块已覆盖；`src/math`、`src/resource`、`src/scene` 尚未做体检）。**

**已完成：安全漏洞扫描与修复——`src/script/vm.c` + `src/script/parser.c`**
发现并修复 3 个严重 bug：
- **Bug 1（CRITICAL）**：深度递归导致 C 栈溢出崩溃。`call_proto` 仅检查堆栈溢出，未限制 C 调用栈深度。恶意脚本 `function deep(n) { return deep(n-1); } deep(10000);` 触发栈溢出。修复：添加 `call_depth` 字段 + `KY_MAX_CALL_DEPTH=256` 限制。
- **Bug 2**：AST 解析器内存泄漏。`ast_free_node` 缺少 `KY_AST_PROGRAM` 分支及多个叶子节点类型（BINOP、UNOP、IDENT、CALL、INDEX），导致每次 `ky_vm_load_string` 解析时泄漏约 4.5KB。修复：补全所有 AST 节点类型的正确释放逻辑。
- **Bug 3**：编译器资源泄漏。`kyx_compile` 在 proto 分配失败时未正确清理 compile state 资源，导致 double-free 风险。修复：添加空指针守卫 + 统一清理路径。
- **Bonus**：`ky_vm_destroy` 未释放 `closures[]` 数组，添加 `free(vm->closures[i])`。

测试结果：普通构建 18/18 绿，ASan 构建 18/18 绿。
Issue: https://github.com/sqxy090123/Kronyx/issues/2（已关闭）

**已完成：安全漏洞审计与修复——VM OOB/UB + AST 内存泄漏**
从攻击者视角设计了利用游戏脚本引擎漏洞的 PoC 攻击场景，发现并修复 3 个安全漏洞：
- **AST 内存泄漏 (LOW)**: `ast_free_node` 对 BINOP/UNOP/CALL.callee/INDEX 子节点使用 `free()` 而非 `ast_free()`，导致每次脚本解析泄漏 4-12 字节。已修复为递归释放。
- **移位操作 UB (HIGH)**: `OP_BSHL/OP_BSHR` 未检查移位量边界，`1 << 100` 等触发 signed shift overflow UB。已修复为无符号移位 + [0,62] 范围限制。
- **OP_CALL 越界读取 (MEDIUM)**: `fn_reg` 参数缺少栈边界验证，恶意字节码可导致越界读。已添加 `fn_reg` 和 `base+fn_reg+nargs` 的范围检查。

测试：普通构建 18/18 绿，ASan 构建 18/18 绿。
审计报告：`.monkeycode/docs/SECURITY_AUDIT.md`

**已完成：to_float/to_int 类型处理漏洞修复** (Issue #4)
- **to_float 缺少 KYT_BOOL 分支 (MEDIUM, 真实 bug)**: 所有位运算 (`&`,`|`,`^`,`~`,`<<`,`>>`) 对 boolean 都错误返回 0。PoC: `function f(a){return a & 1;} f(true)` → 0 (应为 1)。已添加 `KYT_BOOL → as.ival ? 1.0 : 0.0` 分支。
- **to_int 缺少饱和保护 (安全加固)**: `(int64_t)1e300` 在 C 标准中是 UB。虽 lexer 不支持科学计数法，但 `to_int` 作为通用辅助函数必须防御。已添加 INT64_MAX/INT64_MIN 饱和。
- **load_const 短路顺序 UB (安全加固)**: `val == (double)(int64_t)val && fabs(val) < 1e15` 中 cast 子表达式先求值，对大值触发 UB。已调整为 `fabs(val) < 1e15` 在前短路。
- **OP_BNOT 独立走 to_float+cast** 路径也有同样的 UB 风险。已重写为 `~to_int(...)`。

测试：18/18 ctest 在普通和 ASan+UBSan 构建下全部通过，PoC 矩阵 (9 用例) 全部返回正确值。

**已完成：parse_expression 错误路径 UAF/双释放修复** (Issue #5)
- 漏洞：`parse_expression` 在二元操作右操作数解析失败时 `ast_free(n); return left;`，
  `ast_free(n)` 已递归释放挂在 `n->as.binop.left` 上的 `left`，返回悬垂指针给调用方，
  上层 `kyx_parser_destroy` 在二次释放时触发 ASan SEGV/double-free。
- 最小 PoC：`function f(){return 1 + ;}`、`var a = =;`、`function f(){return (1 + ;)}`
- 根因：归所有权转移给 BINOP 节点后，错误恢复路径错误地递归释放了仍归调用方所有的左子树。
- 修复：错误路径改为 `free(n)`，仅释放 BINOP 节点，保留 `left` 的所有权。
- 测试：PoC 现在返回干净 parse error，18/18 ctest 在普通和 ASan 构建下全部通过。
