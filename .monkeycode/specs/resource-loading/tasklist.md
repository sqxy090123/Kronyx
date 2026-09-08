# G2 资源加载最小集 — 任务清单

## T1. 文件层 API（新模块 `file`）
- [x] T1.1 新建 `include/kronyx/file.h`，定义 `kyFileCode` / `ky_file_read` / `ky_free_read`
- [x] T1.2 新建 `src/core/file.c` 实现（fopen/fread），256 MiB 上限
- [x] T1.3 CMakeLists 把 `src/core/file.c` 加入 `ky_core`，`file.h` 进 `install(TARGETS)`（或同 ky_core 头清单）

## T2. 资源层扩展（不破坏 ABI）
- [x] T2.1 `include/kronyx/resource.h` 加 `KY_RES_PIXELBUFFER` + `kyPixelBuffer` 结构 + `payload/on_destroy(a,payload)` 字段
- [x] T2.2 `src/resource/resource.c` destroy / release 分支里调 `r->on_destroy(&alloc, payload)` 销毁 payload
- [x] T2.3 便捷构造 `ky_resmgr_make_pixelbuffer` / `make_raw_bytes`（校验 + 拷贝 + 幂等重复返回 NULL）

## T3. 2D 便利函数
- [x] T3.1 `include/kronyx/2d.h` 声明 `ky2d_make_texture`
- [x] T3.2 `src/render/2d.c` 实现（参数校验 + 转 `ky_rd_create_texture_2d`，保留 8-bit `GL_UNSIGNED_BYTE`）

## T4. Demo 集成
- [x] T4.1 `tools/mk_demo_asset.c` 生成 8×8 RGBA 红白方格（256 B）
- [x] T4.2 `src/demo/platformer_world.{h,c}` 加 `role_tex` 字段 + `demo_load_texture`（缺失资产回落白色）+ setup 挂载 + teardown 释放
- [x] T4.3 已生成 `assets/hero_8x8.bin`（256 B），demo 无窗 smoke 仍 OK

## T5. 测试
- [x] T5.1 `tests/test_resource_flow.c`：文件层三例 + resmgr 幂等/计数 + 释放路径
- [x] T5.2 `tests/test_resource_flow.c`：2D 集成（注册 → acquire → make_texture → 画一帧 → release → destroy）ASan+Leak 干净（28 断言）
- [x] T5.3 CMake 挂 `ky_test_resource_flow`；`demo_asset` 自定义目标生成资产
- [x] T5.4 `test_demo_loop.c`：物化 CWD 资产后断言 `role_tex != NULL`（14 断言，无预置资产也不失败）

## T6. 回写
- [x] T6.1 `progress.md`：G2 改完成，快照「资源：可用 / 2D 渲染加 ky2d_make_texture」，下一刀指 G3
- [x] T6.2 `.monkeycode/docs/resource.md` 活契约（文件层 + 资源层 + 2D 便利 + 典型用法 + 约束）
- [x] T6.3 `.monkeycode/MEMORY.md` 追加 G2 资源/文件层 API 用法
