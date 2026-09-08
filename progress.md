# Kronyx 后续生成大纲

本文件是后续 Agent 的工作入口。先读本文件，再动手。`progress.log` 只作历史流水，不作为优先级来源。

更新日期：2026-09-07

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
| G6 | **场景序列化** | `kyScene` 把实体+Transform+Sprite+Camera2D 写成可读格式并读回 | 通用反射、二进制版本迁移 |

打开编辑器构建：`KYR_BUILD_EDITOR=ON`，并给一条可在无窗环境跑的面板/序列化测试。

### P3 — 有真实游戏后再加的系统（不要提前）

| ID | 对象 | 前置 | 备注 |
|----|------|------|------|
| G7 | Sprite 帧动画 | G1 | 时间轴 + 图集 UV，不接骨骼 |
| G8 | 碰撞回调导出 | G1 | 物理 contact → `ky_event_*`，供 demo/脚本 |
| G9 | 关节 / constraints | 3D 或复杂 2D 需要时 | 现物理无 joint API |
| G10 | 粒子 | G7 之后 | |
| G11 | 音频 | 至少 G1 可玩 | 现无 `audio.h`，从零开模块需单独规格 |
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
| 无 joints / 动画 / 粒子 / 音频 | — | 见 P3 |
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

**下一刀：G6 场景序列化。**
