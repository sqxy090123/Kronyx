# G2 资源加载最小集 — 需求

## 目标
为 demo/未来场景打通「磁盘/内存原始字节 → 引擎资源 → 2D 渲染」一条最短路径。范围刻意最小：不做 PNG 解码、不做热重载、不做材质。

## 1. 文件读取

- **R1.1** 系统提供 `int ky_file_read(const char *path, void **buf_out, size_t *len_out)`：成功返回 0 并输出可读缓冲 + 长度；失败返回负码并保证不改写 `buf_out/len_out`。
- **R1.2** 提供的 `ky_file_read` 必须对路径不存在、不可读目录、超大文件（>256 MiB）分别返回可区分的负码（`KY_FILE_ERR_NOT_FOUND` / `KY_FILE_ERR_NOT_REGEX` 不可，改用 `KY_FILE_ERR_BAD_SIZE`）。
- **R1.3** `ky_free_read(void *buf)` 必须与 `ky_file_read` 配套释放，且不触碰调用者栈。
- **R1.4** 读取路径使用 C `fopen`/`fread`（平台无关），不使用 `load_file` 之类的 POSIX-only API。

## 2. 资源注册

- **R2.1** `ky_resmgr` 已存在 `register/find/acquire/release/destroy`；不改变既有签名（避免破坏 G1/G3）。
- **R2.2** 新资源类型 `KY_RES_PIXELBUFFER`：包含 `{uint8_t *b; size_t n; int w; int h; int channels; const char *name;}` 与 `KY_RES_FILE`（仅原始字节）；两者均可被 `ky_resmgr_register/acquire/release` 管理。
- **R2.3** `ky_resmgr_register` 对重复 path 幂等（已存在则返回 0，不重复分配）。
- **R2.4** `ky_resmgr` 释放时若引用计数 > 0，仍须能干净释放（不要求强制断言，但不得泄漏）。

## 3. 2D 像素 → 纹理

- **R3.1** 提供 `kyTexture *ky2d_make_texture(kyRenderDevice *rd, int w, int h, int channels, const void *pixels)`：底层调用 `ky_rd_create_texture_2d`，并在 `channels == 4` 时自动把每通道值从 0–255 缩放到 0–1 的浮点。
- **R3.2** `ky2d_make_texture` 对 `w<=0 || h<=0 || channels not in {1,2,3,4}` 返回 NULL，不分配。
- **R3.3** 2D 管线消费路径保持不变：`ky2d_render_world` 仍通过 `kySprite.texture` 引用 `kyTexture*`。

## 4. Demo 端到端

- **R4.1** `src/demo/platformer_world.c` 新增 `demo_load_texture`：从 `assets/hero_8x8.bin`（RGBA8 raw）读入 → `ky2d_make_texture` → 挂到 `role.sprite.texture`。文件缺失时保持白色 sprite，**不失败**。
- **R4.2** 提供 `tools/mk_demo_asset.c`：生成同尺寸的 `hero_8x8.bin`（红白方格），供 CI 无外部文件可用。
- **R4.3** `tests/test_resource_flow.c`：注册 `KY_RES_PIXELBUFFER` → acquire 一次 → `ky2d_make_texture` 挂到 Sprite → 渲染一帧 → release 至 0 → resmgr destroy 无 ASan 报错。

## 5. 非目标

- 图片格式（PNG/JPEG）解码
- 热重载 / 版本 / 依赖图
- 跨设备资源迁移
- 场景序列化
- 脚本绑定（G3 单独做）
