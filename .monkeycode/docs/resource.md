# 资源加载（文件层 + 资源管理器 + 2D 纹理）

最小闭环：磁盘原始字节 → `ky_resource` → `kyTexture` → `kySprite.texture` → `ky2d_render_world`。
不含 PNG 解码、热重载、依赖图。

## 文件层（`include/kronyx/file.h`）

```c
kyFileCode ky_file_read(const char *path, void **buf_out, size_t *len_out);
void       ky_free_read(void *buf);
```

- 用 `fstat + fopen/fread`，跨平台；不依赖 `kyAllocator`（用 `malloc`，便于工具/测试直接调用）。
- `buf_out`/`len_out` 在失败时保持原值，不半写。
- 上限 256 MiB；越界返回 `KY_FILE_ERR_BAD_SIZE`。
- 错误码：`KY_FILE_OK` / `KY_FILE_ERR_NOT_FOUND` / `KY_FILE_ERR_BAD_SIZE` / `KY_FILE_ERR_IO` / `KY_FILE_ERR_NOMEM`。

## 资源管理器（`include/kronyx/resource.h`）

沿用既有 `ky_resmgr_create/register/find/acquire/release/destroy/count`，未改签名。
新增成员：

```c
kyResource {
    ...
    void  *payload;                        /* per-kind 载荷 */
    void (*on_destroy)(kyAllocator*, void *payload); /* refcount 0 / destroy 时回调 */
};

KY_RES_PIXELBUFFER + kyPixelBuffer { pixels, byte_count, width, height, channels, name };
```

便捷构造（内部拷贝输入、挂 `on_destroy`、幂等）：

```c
kyResource *ky_resmgr_make_pixelbuffer(m, path, pixels, byte_count, w, h, channels);
kyResource *ky_resmgr_make_raw_bytes(m, path, data, byte_count);
```

- `byte_count < w*h*channels` / `w<=0` / `h<=0` / `channels∉[1,4]` → 返回 NULL，不分配。
- 重复 path 返回 NULL（不二次接管）。
- 返回的资源自带 1 次引用；用 `ky_resmgr_release(m, r)` 减。
- `on_destroy(a, payload)` 走传入的 allocator，追踪 allocator 下也正确。

## 2D 便利（`include/kronyx/2d.h`）

```c
kyTexture *ky2d_make_texture(kyRenderDevice *rd, int w, int h, int channels, const void *pixels);
```

参数校验后转 `ky_rd_create_texture_2d`（后端均为 `GL_UNSIGNED_BYTE`）。失败返回 NULL，调用者负责 `ky_rd_destroy_texture`。

## 典型用法

```c
kyAllocator a = ky_default_allocator();
kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
kyWorld *w = ky_world_create(&a); ky2d_register_components(w);

/* 1) 读盘 */
uint8_t *px; size_t n;
if (ky_file_read("assets/hero_8x8.bin", (void**)&px, &n) == KY_FILE_OK) {
    /* 2) 建纹理（可选走 resmgr；demo 里直接建） */
    kyTexture *t = ky2d_make_texture(rd, 8, 8, 4, px);
    if (t) { /* 挂到 sprite */ sp->texture = t; }
    if (px && /* 非经 resmgr 拷贝时 */) ky_free_read(px);
}

/* 3) 画 */
int drawn = ky2d_render_world_auto(rd, w);

/* 清理：destroy texture -> rd -> world（顺序勿颠倒，payload 在 destroy 时回调 on_destroy） */
```

## 已知约束

- 8-bit `GL_UNSIGNED_BYTE` 路径；浮点纹理未接（后端 `glTexImage2D` 目前固定 `UBYTE`）。
- 文件层用 `malloc`/`free`，与 `kyAllocator` 并行存在；工具/测试场景可接受。
- 追踪 allocator 下：`ky_resmgr_make_*` 走 `ky_mem_alloc`，`on_destroy` 走 `ky_mem_free`，一致。
