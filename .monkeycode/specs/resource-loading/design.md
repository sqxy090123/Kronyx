# G2 资源加载最小集 — 设计

## 模块切分

```
include/kronyx/file.h        ← 新增。ky_file_read / ky_free_read
include/kronyx/resource.h    ← 保留。追加 KY_RES_PIXELBUFFER + 新成员
include/kronyx/resource.h    ← 保留。追加 ky_resmgr_make_pixelbuffer (可选便捷函数)
src/core/file.c              ← 新增。ky_file_read / ky_free_read 实现
src/render/2d.c              ← 修改。新增出口 ky2d_make_texture
src/demo/platformer_world.c  ← 修改。角色挂纹理；缺文件回落到白色 sprite
tools/mk_demo_asset.c        ← 新增。生成 assets/hero_8x8.bin
tests/test_resource_flow.c   ← 新增。端到端 + release 计数
```

## 数据结构

```c
/* 文件层：与 kyAllocator 无关，用 malloc，保证跨模块可移植 */
typedef enum {
    KY_FILE_OK            = 0,
    KY_FILE_ERR_NOT_FOUND = -1,
    KY_FILE_ERR_BAD_SIZE  = -2,   /* > 256 MiB */
    KY_FILE_ERR_IO        = -3,
    KY_FILE_ERR_NOMEM     = -4,
} kyFileCode;

int   ky_file_read(const char *path, void **buf_out, size_t *len_out);
void  ky_free_read(void *buf);
```

```c
/* 资源层：在既有 kyResource 上追加 kind 分支，不破坏 ABI（ky_resource.h 内部） */
typedef struct {
    uint8_t *b;
    size_t   n;
    int      w, h, channels;
    char    *name;
} kyPixelBuffer;

/* kyResource.kind == KY_RES_PIXELBUFFER 时，kyResource->payload 指向 kyPixelBuffer */
```

## 关键契约

- `ky_resmgr_register(m, r)`：若 path 已存在，返回 0，不动 r；否则接管 r 与 r->path 所有权（r->path 指向 resmgr 内部分配的字符串）。与 G1 现有签名一致。
- `ky_resmgr_release`：引用减到 0 时，若 `r->kind == KY_RES_PIXELBUFFER`，同时销毁 `kyPixelBuffer.b` 与 `name`。
- `ky2d_make_texture`：channels 从 [1,4]，w/h 允许 ≤ 2048，超界返回 NULL；不做 float 化，交给 GL 端 `GL_UNSIGNED_BYTE` 上传（现有 `create_texture_2d` 已经是这个契约）。

## 错误码映射

| 场景                                   | 返回码                          |
|----------------------------------------|--------------------------------|
| 路径不存在 / 非文件                      | `KY_FILE_ERR_NOT_FOUND`        |
| 大小 > 256 MiB                          | `KY_FILE_ERR_BAD_SIZE`         |
| fread / 中途错                          | `KY_FILE_ERR_IO`               |
| malloc 失败                            | `KY_FILE_ERR_NOMEM`            |

## Demo 集成

`platformer_setup` 在现有 role sprite 之前调用 `demo_load_texture(pw->rd)`；返回 NULL 时保持 `sp->texture = NULL` 走白色路径。这样 `ky_demo` 无资产也能跑（现有测试不变）。

## 测试策略

`test_resource_flow.c` 覆盖：

1. **文件层**：写一个 3 字节文件 → `ky_file_read` OK → 长度/内容一致 → 路径不存在 → -1 → 空路径 → 负码
2. **资源层**：`ky_resmgr_register` × 2 同路径 → count == 1，重复返回 0
3. **引用计数**：acquire → acquire → release → release → 资源消失
4. **2D 集成**：注册 → acquire → `ky2d_make_texture` → Sprite.texture 挂载 → 一帧 → release→0 → resmgr destroy；ASan 无 UAF/leak
5. **失败路径**：`ky2d_make_texture(rd, 0, 0, 4, data)` 返回 NULL

`test_demo_loop.c` 保留（走 fallback 白色 sprite），并新增一条断言：`platformer_setup` 在 `assets/hero_8x8.bin` 存在时 role texture 非空。
