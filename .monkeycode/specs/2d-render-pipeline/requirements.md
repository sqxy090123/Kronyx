# Requirements Document: 2D Render Pipeline

Feature Name: 2d-render-pipeline
Updated: 2026-09-05
Status: Draft v1

## Introduction

为 Kronyx 引擎补齐 P2 缺口：在纯框架 ECS 与 RHI vtable 抽象之上，提供 Transform / Sprite / Camera 三个核心 2D 组件和一个 2D batch 渲染管线。目标是让"spawn 一批带 Sprite 的实体 → 设一台正交相机 → 调一次 ky2d_render_world"即可在 GL/EGL 后端出画面，console 后端可调试观察。该管线是 P6 "2D platformer demo" 的硬前置。

## Glossary

- **World**: `kyWorld`（ecs.h），实体 + 组件 + 系统容器
- **RHI**: `ky_rd_*`（render.h）渲染设备抽象，22 API vtable，后端实现有 console 与 GL/EGL
- **Transform**: 实体空间状态组件（位置/旋转/缩放）
- **Sprite**: 2D 显示组件（纹理引用/颜色/尺寸/排序）
- **Camera2D**: 正交视图组件（位置/缩放/视口/清除色）
- **Batch Renderer**: 每帧把所有 Sprite 变换为顶点、合并进共享顶点缓冲、经 RHI 提交的模块
- **Engine Unit (EU)**: 引擎世界单位，1 EU = 1 米；相机正交范围与 Sprite 尺寸均以 EU 计
- **White Texture**: 管线自带的 1x1 纯白纹理，供无纹理 Sprite 着色

## Requirements

### Requirement 1: Transform 组件

**User Story:** AS 引擎使用者，I WANT 实体携带位置/旋转/缩放，SO THAT 系统能以统一空间状态驱动渲染与物理。

#### Acceptance Criteria

1. WHEN 使用者向 World 注册并挂载 Transform 组件，系统 SHALL 以 `{pos(x,y), rotation_z(弧度), scale(x,y), z(排序/深度用 float)}` 结构存储状态，pos 默认 (0,0)、rotation 默认 0、scale 默认 (1,1)、z 默认 0。
2. WHEN 同一实体同时挂载 Transform 与 Sprite，Batch Renderer SHALL 读取该 Transform 计算 Sprite 四角顶点。
3. IF 实体挂载 Sprite 但缺少 Transform，Batch Renderer SHALL 使用恒等 Transform（原点、零旋转、1 缩放）渲染，并产生一条 ky_log 警告（每帧每类最多一条）。
4. WHERE Transform 的 scale 任一分量为 0，Batch Renderer SHALL 将该 Sprite 的四角退化为缩线段/点后跳过提交（不产生退化三角形）。

### Requirement 2: Sprite 组件

**User Story:** AS 引擎使用者，I WANT 实体通过 Sprite 组件声明 2D 外观，SO THAT Batch Renderer 能按声明绘制。

#### Acceptance Criteria

1. WHEN 创建 Sprite 组件，系统 SHALL 以 `{texture(可空指针), color(kyVec4), size(kyVec2, EU), uv0/uv1(kyVec2), layer(int), flip(u8 bitfield)}` 存储，color 默认白 (1,1,1,1)、uv 默认全图 (0,0)-(1,1)。
2. IF texture 为空，Batch Renderer SHALL 绑定 White Texture，使 Sprite 呈现为纯色矩形。
3. WHEN 两个 Sprite 的 layer 值不同，Batch Renderer SHALL 按 layer 从小到大绘制（大 layer 覆盖小 layer）。
4. WHERE 两个 Sprite 的 layer 相同，Batch Renderer SHALL 按 z 分量从小到大绘制；z 亦相同时按实体 id 升序，保证输出确定。

### Requirement 3: Camera2D 组件

**User Story:** AS 引擎使用者，I WANT 在场景中放置正交相机并控制视野，SO THAT 世界坐标能正确映射到屏幕。

#### Acceptance Criteria

1. WHEN 创建 Camera2D 组件，系统 SHALL 以 `{pos(kyVec2), rotation_z(弧度), zoom(float), viewport(宽高 EU), clear_color(kyVec4), active(int)}` 存储，zoom 默认 1.0、clear_color 默认深灰 (0.1,0.1,0.1,1)、active 默认 0。
2. WHEN 每帧渲染开始时，Batch Renderer SHALL 选用 active==1 的 Camera2D 之一（取实体 id 最小者）；IF 无 active 相机，Batch Renderer SHALL 跳过本帧渲染并返回 0。
3. WHILE 相机 active，Batch Renderer SHALL 由 pos/rotation/zoom/viewport 构造正交视图投影矩阵：世界点先减相机 pos、绕 Z 旋转 -rotation、除以 zoom，再经 ortho(-vw/2..vw/2, -vh/2..vh/2) 投影。
4. IF viewport 宽或高为 0，Batch Renderer SHALL 视为本帧无效相机并跳过渲染。

### Requirement 4: 2D Batch Renderer

**User Story:** AS 引擎使用者，I WANT 调用一个函数即可把整个 World 的 Sprite 绘制出来，SO THAT 无需手写顶点与 RHI 命令。

#### Acceptance Criteria

1. WHEN 调用 `ky2d_render_world(rd, world, cam_entity)`（显式相机实体）或 `ky2d_render_world_auto(rd, world)`（自动选相机），系统 SHALL 在一次 RHI begin..submit 区间内完成：clear → 计算 view-proj → 收集 Sprite 顶点 → 排序 → 提交 draw。
2. WHEN 收集顶点，Batch Renderer SHALL 为每个 Sprite 生成 4 顶点 + 6 索引（两个三角形），顶点格式为 `{pos(2f), uv(2f), color(4ub normalized)}`，stride 20 字节。
3. WHEN Sprite 数量超过单批容量（默认 4096），Batch Renderer SHALL 分多次 draw 提交且输出结果与单批一致。
4. WHILE 使用 GL 后端，Batch Renderer SHALL 使用内置 GLES3 shader（vs: MVP 变换传 uv/color；fs: 采样纹理乘顶点色，uv 越界 clamp）并通过 blend 开启 alpha 混合（src=SRC_ALPHA, dst=ONE_MINUS_SRC_ALPHA）。
5. WHILE 使用 console 后端，Batch Renderer SHALL 输出排序后的 Sprite 清单（id/pos/size/color/layer）供调试。
6. IF World 中不存在任何 Sprite，Batch Renderer SHALL 正常完成 clear 与空提交，返回 0 个绘制体。
7. IF rd 或 world 为空指针，系统 SHALL 返回负错误码且不崩溃。

### Requirement 5: 纹理与白纹理

**User Story:** AS 引擎使用者，I WANT 用 ky_rd_create_texture_2d 自备纹理并开箱即得纯色 Sprite，SO THAT 无 PNG 依赖即可跑通渲染。

#### Acceptance Criteria

1. WHEN Batch Renderer 首次运行且当前帧存在 Sprite，系统 SHALL 延迟创建并缓存 White Texture（1x1 RGBA 白），复用于所有无纹理 Sprite。
2. WHEN Sprite 持有使用者自建纹理（含棋盘格等程序化纹理），Batch Renderer SHALL 按 uv0/uv1 采样该纹理。
3. 本阶段（V1）系统 SHALL 依赖 ky_rd_create_texture_2d 的 pixels 参数获取纹理数据，PNG/文件解码能力延后到 asset pipeline（P5）。

### Requirement 6: 与 ECS 集成

**User Story:** AS 引擎使用者，I WANT 一行代码完成组件注册，SO THAT 三组件接入任何 kyWorld。

#### Acceptance Criteria

1. WHEN 调用 `ky2d_register_components(world)`，系统 SHALL 依次注册 Transform、Sprite、Camera2D 三个 kyComponentType（含 size 与空 ctor/dtor），返回 0 表示全部成功。
2. IF 同一组件类型重复注册，系统 SHALL 复用已有 type_id 并返回成功。
3. WHERE 使用者自行管理组件生命周期，2d 模块 SHALL 提供组件构造辅助函数（ky_transform_new / ky_sprite_new / ky_camera2d_new）返回带默认值的组件值。

## Out of Scope (V1)

- Transform 父子层级（嵌套变换）——延后
- PNG/磁盘纹理加载——延后到 P5 asset pipeline
- 视口裁剪（viewport culling）与图集（atlas）——延后
- 文本渲染、9-slice——延后
- 3D 管线与透视相机——非本模块
