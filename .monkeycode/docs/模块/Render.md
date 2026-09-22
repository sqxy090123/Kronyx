# 渲染模块 (Render)

## 概述

渲染模块是 Kronyx Engine 的图形核心层，提供跨平台的渲染硬件抽象（RHI）和2D渲染管线。它支持多种渲染后端，包括控制台后端和OpenGL ES 3后端，为上层应用提供统一的渲染接口。

### 模块组成

```
Render/
├── render.h          # RHI 核心
├── 2d.h              # 2D 渲染
├── anim2d.h          # 帧动画
├── render_backend.h  # 后端接口
├── console_backend.c  # 控制台后端
├── gl_backend.c      # OpenGL 后端
└── sprite.h          # 精灵渲染
```

## 渲染设备 (Render Device)

### 创建和销毁

```c
// 渲染后端类型
typedef enum kyRendererBackend {
    KY_RENDERER_NONE = 0,      // 无渲染后端
    KY_RENDERER_CONSOLE,       // 控制台文本后端
    KY_RENDERER_GL,            // OpenGL 后端
    KY_RENDERER_VULKAN         // Vulkan 后端（预留）
} kyRendererBackend;

// 创建渲染设备
kyRenderDevice *ky_rd_create(kyRendererBackend backend, void *platform_win);

// 销毁渲染设备
void ky_rd_destroy(kyRenderDevice *rd);

// 查询后端信息
const char *ky_rd_backend_name(const kyRenderDevice *rd);
kyRendererBackend ky_rd_backend(const kyRenderDevice *rd);
```

### 命令列表

```c
// 命令列表操作
typedef struct kyCommandList kyCommandList;

// 开始命令列表
void *ky_rd_begin(kyRenderDevice *rd);

// 结束命令列表
void ky_rd_submit(kyRenderDevice *rd, void *cmdlist);

// 呈现
void ky_rd_present(kyRenderDevice *rd);

// 清除颜色和深度
void ky_rd_clear(kyRenderDevice *rd, kyVec4 clear_color, float clear_depth);

// 设置管线
void ky_cmd_set_pipeline(void *cmdlist, kyPipeline *pipeline);

// 设置顶点缓冲区
void ky_cmd_set_vertex_buffer(void *cmdlist, kyBuffer *vb, uint32_t stride);

// 设置索引缓冲区
void ky_cmd_set_index_buffer(void *cmdlist, kyBuffer *ib, uint32_t index_size);

// 设置纹理
void ky_cmd_set_texture(void *cmdlist, int slot, kyTexture *tex);

// 设置uniform
void ky_cmd_set_uniform(void *cmdlist, int location, const void *data, int bytes);

// 绘制命令
void ky_cmd_draw_array(void *cmdlist, uint32_t vertex_count, uint32_t instances);
void ky_cmd_draw_indexed(void *cmdlist, uint32_t count, uint32_t instances);
```

## 渲染管线 (Pipeline)

### 管线描述符

```c
typedef enum kyPrimitiveTopology {
    KY_TRIANGLES = 0,
    KY_TRIANGLE_STRIP,
    KY_LINES,
    KY_POINTS
} kyPrimitiveTopology;

typedef struct kyVertexAttrib {
    uint32_t location;     // 顶点属性位置
    uint32_t offset;      // 在顶点结构中的偏移
    uint32_t size;        // 属性分量数（1,2,3,4）
    uint8_t normalized;    // 是否归一化
    uint8_t type;         // 属性类型（KY_ATTRIB_FLOAT/UBYTE）
} kyVertexAttrib;

typedef struct kyVertexLayout {
    const kyVertexAttrib *attribs;  // 顶点属性数组
    uint32_t count;                // 属性数量
    uint32_t stride;               // 顶点结构大小
} kyVertexLayout;

typedef struct kyPipelineDesc {
    void *shader;                  // 着色器
    kyVertexLayout layout;         // 顶点布局
    kyPrimitiveTopology topology;  // 拓扑类型
    int depth_test;                // 深度测试
    int depth_write;               // 深度写入
    int cull_mode;                 // 剔除模式
    struct {
        int on;                    // 是否启用混合
        kyVec4 color;              // 混合颜色
    } blend;
} kyPipelineDesc;

// 创建和销毁管线
kyPipeline *ky_rd_create_pipeline(kyRenderDevice *rd, const kyPipelineDesc *desc);
void ky_rd_destroy_pipeline(kyRenderDevice *rd, kyPipeline *pipeline);
```

### 着色器系统

```c
// 着色器源码
typedef struct kyShaderSource {
    const char *vs;                // 顶点着色器源码
    const char *fs;                // 片段着色器源码
    const char *cs;                // 计算着色器源码（可选）
    const char *entry;             // 入口点名称
} kyShaderSource;

// 着色器创建和销毁
kyShader *ky_rd_create_shader(kyRenderDevice *rd, const kyShaderSource *src);
void ky_rd_destroy_shader(kyRenderDevice *rd, kyShader *shader);

// 获取uniform位置
int ky_rd_shader_uniform(const kyRenderDevice *rd, const kyShader *shader, const char *name);
```

### 资源创建

```c
// 缓冲区
kyBuffer *ky_rd_create_buffer(kyRenderDevice *rd, size_t size, const void *data, int dynamic);
void ky_rd_destroy_buffer(kyRenderDevice *rd, kyBuffer *buffer);
int ky_rd_update_buffer(kyRenderDevice *rd, kyBuffer *buffer, size_t offset, size_t size, const void *data);

// 纹理
kyTexture *ky_rd_create_texture_2d(kyRenderDevice *rd, int width, int height, int channels, const void *pixels);
void ky_rd_destroy_texture(kyRenderDevice *rd, kyTexture *texture);

// 自定义渲染过程
typedef void (*kyDrawPassFn)(kyRenderDevice *rd, void *cmdlist, void *user);
void ky_rd_set_draw_pass(kyRenderDevice *rd, kyDrawPassFn fn, void *user);
```

## 2D 渲染系统

### 精灵组件

```c
// 精灵组件
typedef struct kySprite {
    kyTexture *texture;            // 纹理
    kyVec4 color;                 // 染色颜色 (RGBA 0-1)
    kyVec2 pivot;                 // 中心点（本地坐标）
    float rotation;                // 旋转角度（弧度）
    int layer;                    // 渲染层级（数值越小越先渲染）
    int z;                        // 层级内的排序键
    int id;                       // 精灵唯一ID（用于批处理）
} kySprite;

// 注册2D组件
void ky2d_register_components(kyWorld *w);

// 创建2D纹理
kyTexture *ky2d_make_texture(kyRenderDevice *rd, int width, int height, int channels, const void *pixels);

// 2D渲染函数
int ky2d_render_world(kyRenderDevice *rd, kyWorld *w, const kyMat4 *view_proj);
int ky2d_render_world_auto(kyRenderDevice *rd, kyWorld *w);
```

### 批处理渲染

```c
// 批处理状态
typedef struct ky2d_batch {
    kyTexture *texture;            // 当前纹理
    int layer;                    // 当前层级
    kyArray vertices;            // 顶点数据
    kyArray indices;             // 索引数据
    uint32_t vertex_offset;       // 顶点偏移
} ky2d_batch;

// 批处理管理
void ky2d_batch_begin(ky2d_batch *batch);
void ky2d_batch_flush(ky2d_batch *batch);
void ky2d_batch_end(ky2d_batch *batch);

// 添加精灵到批处理
void ky2d_batch_add_sprite(ky2d_batch *batch, const kySprite *sprite, 
                          const kyVec2 *pos, const kyVec2 *scale);
```

### 渲染层级管理

```c
// 渲染层级排序函数
int ky2d_compare_entities(const void *a, const void *b);

// 批处理排序
void ky2d_sort_batches(ky2d_batch *batches, int count);

// 按层级渲染
void ky2d_render_by_layer(kyRenderDevice *rd, kyWorld *w, const kyMat4 *view_proj) {
    // 1. 按层级分组收集精灵
    typedef struct layer_group {
        int layer;
        kyArray sprites;
    } layer_group;
    
    kyArray groups = ky_array_create(sizeof(layer_group), &ky_default_allocator);
    
    // 2. 收集所有精灵
    uint32_t tid_sprite = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_transform = ky_world_component_type_by_name(w, "transform");
    
    uint32_t types[] = {tid_transform, tid_sprite};
    kyViewIter it;
    
    if (ky_view_begin(w, types, 2, &it)) {
        while (ky_view_next(&it)) {
            Transform *t = ky_world_get_component(w, it.current, tid_transform);
            Sprite *s = ky_world_get_component(w, it.current, tid_sprite);
            
            // 添加到对应的层级组
            add_sprite_to_layer_group(&groups, s, t);
        }
    }
    
    // 3. 按层级顺序渲染
    for (int i = 0; i < groups.count; i++) {
        layer_group *group = &((layer_group*)groups.data)[i];
        
        // 创建批处理
        ky2d_batch batch;
        ky2d_batch_begin(&batch);
        
        // 批量渲染当前层级
        for (int j = 0; j < group->sprites.count; j++) {
            Sprite *s = &((Sprite*)group->sprites.data)[j];
            ky2d_batch_add_sprite(&batch, s, &t->position, &t->scale);
        }
        
        ky2d_batch_flush(&batch);
        ky2d_batch_end(&batch);
    }
    
    ky_array_destroy(&groups);
}
```

## 帧动画系统

### 动画组件

```c
// 动画状态
typedef struct kyAnimator {
    kyTexture **frames;            // 动画帧纹理数组
    int frame_count;               // 总帧数
    int current_frame;            // 当前帧
    float frame_time;             // 当前帧已显示时间
    float frame_duration;         // 每帧持续时间
    int looping;                  // 是否循环
    int playing;                  // 是否正在播放
    int ping_pong;               // 是否来回播放
} kyAnimator;

// 注册动画组件
void ky2d_register_animation_components(kyWorld *w);

// 创建动画器
kyAnimator *ky2d_animator_create(kyWorld *w, const char **frame_paths, int frame_count, 
                                 float frame_duration, int looping);

// 更新动画
void ky2d_animator_update(kyAnimator *animator, float dt);

// 播放控制
void ky2d_animator_play(kyAnimator *animator);
void ky2d_animator_stop(kyAnimator *animator);
void ky2d_animator_reset(kyAnimator *animator);
```

### 动画序列化

```c
// 动画配置
typedef struct kyAnimationClip {
    char name[64];
    int *frames;                  // 帧索引数组
    int frame_count;
    float duration;
    int looping;
    int ping_pong;
} kyAnimationClip;

// 动画剪辑器
typedef struct kyAnimationController {
    kyAnimator *current_animator;
    kyArray clips;               // 动画剪辑列表
    char current_clip[64];
    float transition_time;       // 过渡时间
} kyAnimationController;

// 创建动画控制器
kyAnimationController *ky2d_animation_controller_create(kyWorld *w);

// 添加动画剪辑
void ky2d_animation_controller_add_clip(kyAnimationController *ctrl, 
                                       const kyAnimationClip *clip);

// 切换动画
void ky2d_animation_controller_play(kyAnimationController *ctrl, const char *clip_name);

// 更新动画控制器
void ky2d_animation_controller_update(kyAnimationController *ctrl, float dt);
```

## 使用示例

### 基本渲染设置

```c
// 1. 创建渲染设备
kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
KY_ASSERT(rd != NULL);

// 2. 创建着色器
kyShaderSource shader_src = {
    .vs = "attribute vec2 a_pos; attribute vec2 a_uv; varying vec2 v_uv; uniform mat4 u_mvp; void main() { gl_Position = u_mvp * vec4(a_pos, 0, 1); v_uv = a_uv; }",
    .fs = "varying vec2 v_uv; uniform sampler2D u_texture; void main() { gl_FragColor = texture2D(u_texture, v_uv); }",
    .entry = "main"
};
kyShader *shader = ky_rd_create_shader(rd, &shader_src);

// 3. 创建管线
kyVertexAttrib attribs[] = {
    {0, 0, 2, 0, KY_ATTRIB_FLOAT},  // 位置
    {1, 8, 2, 0, KY_ATTRIB_FLOAT}   // UV坐标
};

kyVertexLayout layout = {
    .attribs = attribs,
    .count = 2,
    .stride = 16  // 4 * 4 bytes (vec2 + vec2)
};

kyPipelineDesc pipeline_desc = {
    .shader = shader,
    .layout = layout,
    .topology = KY_TRIANGLES,
    .depth_test = 0,
    .depth_write = 0,
    .cull_mode = 0,
    .blend = {0, {0, 0, 0, 1}}
};

kyPipeline *pipeline = ky_rd_create_pipeline(rd, &pipeline_desc);

// 4. 创建顶点缓冲区
float vertices[] = {
    // 位置    // UV坐标
    -1, -1,    0, 1,
     1, -1,    1, 1,
    -1,  1,    0, 0,
     1,  1,    1, 0
};

kyBuffer *vb = ky_rd_create_buffer(rd, sizeof(vertices), vertices, 0);

// 5. 渲染循环
void render_loop(void) {
    // 开始渲染
    void *cmdlist = ky_rd_begin(rd);
    
    // 设置管线和缓冲区
    ky_cmd_set_pipeline(cmdlist, pipeline);
    ky_cmd_set_vertex_buffer(cmdlist, vb, 16);
    
    // 清除屏幕
    ky_rd_clear(rd, (kyVec4){0.1f, 0.1f, 0.1f, 1.0f}, 1.0f);
    
    // 绘制
    ky_cmd_draw_array(cmdlist, 4, 1);
    
    // 提交和呈现
    ky_rd_submit(rd, cmdlist);
    ky_rd_present(rd);
}
```

### 2D 渲染示例

```c
// 1. 创建世界和组件
kyWorld *world = ky_world_create(&ky_default_allocator);
ky2d_register_components(world);

// 2. 创建实体和组件
kyEntity player = ky_world_spawn(world);

// 变换组件
typedef struct Transform {
    kyVec3 position;
    kyVec2 scale;
    float rotation;
} Transform;

uint32_t tid_transform = ky_world_register_component(world, &transform_type);
Transform *t = ky_world_add_component(world, player, tid_transform);
t->position = (kyVec3){0, 0, 0};
t->scale = (kyVec2){1, 1};
t->rotation = 0;

// 精灵组件
typedef struct Sprite {
    kyTexture *texture;
    kyVec4 color;
    int layer;
} Sprite;

uint32_t tid_sprite = ky_world_register_component(world, &sprite_type);
Sprite *s = ky_world_add_component(world, player, tid_sprite);

// 3. 创建纹理
uint8_t *pixels = load_texture_data("assets/hero.png");
s->texture = ky2d_make_texture(rd, 64, 64, 4, pixels);
s->color = (kyVec4){1, 1, 1, 1};
s->layer = 1;

// 4. 相机矩阵
kyMat4 view_proj = ky_mat4_ortho(-16, 16, -9, 9, -1, 1);

// 5. 2D 渲染
void render_2d_scene(kyWorld *world, kyRenderDevice *rd, kyMat4 *view_proj) {
    // 使用2D自动渲染函数
    int drawn_sprites = ky2d_render_world_auto(rd, world);
    KY_LOG_INFO("Rendered %d sprites", drawn_sprites);
    
    // 或者手动控制渲染流程
    // ky2d_render_by_layer(rd, world, view_proj);
}
```

### 帧动画示例

```c
// 1. 创建动画序列
const char *walk_frames[] = {
    "assets/walk_1.png",
    "assets/walk_2.png",
    "assets/walk_3.png",
    "assets/walk_4.png"
};

// 2. 创建动画实体
kyEntity animated_entity = ky_world_spawn(world);

// 变换组件
Transform *t = ky_world_add_component(world, animated_entity, tid_transform);
t->position = (kyVec3){0, 0, 0};

// 精灵组件
Sprite *s = ky_world_add_component(world, animated_entity, tid_sprite);
s->texture = NULL;  // 动画会管理纹理
s->layer = 1;

// 动画器组件
Animator *a = ky_world_add_component(world, animated_entity, tid_animator);
a->frames = NULL;  // 将由动画控制器管理

// 3. 创建动画控制器
kyAnimationController *anim_ctrl = ky2d_animation_controller_create(world);

// 添加行走动画
kyAnimationClip walk_clip = {
    .name = "walk",
    .frame_count = 4,
    .duration = 0.5f,
    .looping = 1,
    .ping_pong = 0
};
// 设置帧索引...
ky2d_animation_controller_add_clip(anim_ctrl, &walk_clip);

// 4. 播放动画
ky2d_animation_controller_play(anim_ctrl, "walk");

// 5. 更新循环
void animation_update(float dt) {
    ky2d_animation_controller_update(anim_ctrl, dt);
    
    // 更新精灵纹理
    if (anim_ctrl->current_animator) {
        Animator *a = anim_ctrl->current_animator;
        if (a->frames[a->current_frame]) {
            Sprite *s = ky_world_get_component(world, animated_entity, tid_sprite);
            s->texture = a->frames[a->current_frame];
        }
    }
}
```

## 高级特性

### 自定义渲染过程

```c
// 自定义渲染函数
void custom_render_pass(kyRenderDevice *rd, void *cmdlist, void *user_data) {
    CustomRenderData *data = (CustomRenderData*)user_data;
    
    // 设置自定义着色器
    kyShader *custom_shader = data->shader;
    kyPipeline *custom_pipeline = data->pipeline;
    
    ky_cmd_set_pipeline(cmdlist, custom_pipeline);
    
    // 设置自定义uniform
    ky_cmd_set_uniform(cmdlist, data->uniform_mvp, &data->mvp, sizeof(kyMat4));
    
    // 绘制自定义几何体
    kyBuffer *vb = data->vertex_buffer;
    kyBuffer *ib = data->index_buffer;
    
    ky_cmd_set_vertex_buffer(cmdlist, vb, data->vertex_stride);
    ky_cmd_set_index_buffer(cmdlist, ib, sizeof(uint16_t));
    
    ky_cmd_draw_indexed(cmdlist, data->index_count, 1);
}

// 注册自定义渲染过程
void setup_custom_render(kyRenderDevice *rd) {
    CustomRenderData data = {
        .shader = load_custom_shader(),
        .pipeline = create_custom_pipeline(),
        .uniform_mvp = get_uniform_location("u_mvp"),
        .vertex_buffer = load_vertex_data(),
        .index_buffer = load_index_data(),
        // ... 其他数据
    };
    
    ky_rd_set_draw_pass(rd, custom_render_pass, &data);
}
```

### 多渲染目标

```c
// 渲染目标描述符
typedef struct kyRenderTarget {
    int width;
    int height;
    kyTexture *color_attachments[8];  // 最多8个颜色附件
    kyTexture *depth_attachment;      // 深度附件
} kyRenderTarget;

// 创建渲染目标
kyRenderTarget *ky_rd_create_render_target(kyAllocator *alloc, const kyRenderTargetDesc *desc);

// 设置渲染目标
void ky_rd_set_render_target(kyRenderDevice *rd, kyRenderTarget *target);

// 恢复默认渲染目标
void ky_rd_restore_default_target(kyRenderDevice *rd);
```

### 后处理效果

```c
// 后处理系统
typedef struct kyPostProcessEffect {
    const char *name;
    kyShader *shader;
    kyRenderTarget *target;
    int enabled;
} kyPostProcessEffect;

// 后处理链
typedef struct kyPostProcessChain {
    kyArray effects;
    kyRenderTarget *default_target;
    kyRenderTarget *temp_target;
} kyPostProcessChain;

kyPostProcessChain *ky_post_process_chain_create(kyAllocator *alloc, int width, int height);

void ky_post_process_chain_add_effect(kyPostProcessChain *chain, 
                                    const char *name, kyShader *shader);

void ky_post_process_chain_render(kyPostProcessChain *chain, kyRenderDevice *rd, 
                                 kyTexture *input_texture);

// 示例：模糊效果
void setup_blur_effect(kyPostProcessChain *chain) {
    const char *blur_vs = "attribute vec2 a_pos; varying vec2 v_uv; void main() { gl_Position = vec4(a_pos, 0, 1); v_uv = a_uv; }";
    const char *blur_fs = "varying vec2 v_uv; uniform sampler2D u_texture; void main() { vec4 color = texture2D(u_texture, v_uv); gl_FragColor = color; }";
    
    kyShaderSource shader_src = {blur_vs, blur_fs, NULL, "main"};
    kyShader *blur_shader = ky_rd_create_shader(rd, &shader_src);
    
    ky_post_process_chain_add_effect(chain, "blur", blur_shader);
}
```

## 性能优化

### 批处理优化

```c
// 批处理优化策略
void optimize_batch_rendering(kyWorld *world, kyRenderDevice *rd) {
    // 1. 按纹理分组
    typedef struct texture_batch {
        kyTexture *texture;
        int vertex_count;
        int index_count;
        kyArray entities;
    } texture_batch;
    
    kyArray texture_batches = ky_array_create(sizeof(texture_batch), &ky_default_allocator);
    
    // 2. 收集并分组精灵
    uint32_t tid_sprite = ky_world_component_type_by_name(world, "sprite");
    uint32_t tid_transform = ky_world_component_type_by_name(world, "transform");
    
    uint32_t types[] = {tid_transform, tid_sprite};
    kyViewIter it;
    
    if (ky_view_begin(world, types, 2, &it)) {
        while (ky_view_next(&it)) {
            Transform *t = ky_world_get_component(world, it.current, tid_transform);
            Sprite *s = ky_world_get_component(world, it.current, tid_sprite);
            
            // 查找或创建纹理批次
            texture_batch *batch = find_texture_batch(&texture_batches, s->texture);
            if (!batch) {
                batch = create_texture_batch(&texture_batches, s->texture);
            }
            
            // 添加实体到批次
            ky_array_push(&batch->entities, &it.current);
            batch->vertex_count += 4;  // 每个精灵4个顶点
            batch->index_count += 6;   // 每个精灵6个索引
        }
    }
    
    // 3. 批量渲染
    for (int i = 0; i < texture_batches.count; i++) {
        texture_batch *batch = &((texture_batch*)texture_batches.data)[i];
        
        // 创建批处理缓冲区
        create_batch_buffers(batch);
        
        // 填充批处理数据
        fill_batch_data(batch);
        
        // 渲染批次
        render_batch(rd, batch);
    }
    
    ky_array_destroy(&texture_batches);
}
```

### 实例化渲染

```c
// 实例化渲染
void instanced_rendering(kyWorld *world, kyRenderDevice *rd) {
    // 1. 收集实例数据
    typedef struct instance_data {
        kyVec3 position;
        kyVec2 scale;
        float rotation;
        kyVec4 color;
    } instance_data;
    
    kyArray instances = ky_array_create(sizeof(instance_data), &ky_default_allocator);
    
    // 2. 收集所有同类型的实体
    uint32_t tid_transform = ky_world_component_type_by_name(world, "transform");
    uint32_t tid_sprite = ky_world_component_type_by_name(world, "sprite");
    
    uint32_t types[] = {tid_transform, tid_sprite};
    kyViewIter it;
    
    if (ky_view_begin(world, types, 2, &it)) {
        while (ky_view_next(&it)) {
            Transform *t = ky_world_get_component(world, it.current, tid_transform);
            Sprite *s = ky_world_get_component(world, it.current, tid_sprite);
            
            instance_data data = {
                .position = t->position,
                .scale = t->scale,
                .rotation = t->rotation,
                .color = s->color
            };
            
            ky_array_push(&instances, &data);
        }
    }
    
    // 3. 创建实例缓冲区
    kyBuffer *instance_buffer = ky_rd_create_buffer(rd, 
        instances.count * sizeof(instance_data), 
        instances.data, 1);  // 动态缓冲区
    
    // 4. 设置实例化渲染
    ky_cmd_set_vertex_buffer(cmdlist, vertex_buffer, vertex_stride);
    ky_cmd_set_instance_buffer(cmdlist, instance_buffer, sizeof(instance_data));
    
    // 5. 绘制实例
    ky_cmd_draw_array(cmdlist, 4, instances.count);
    
    // 清理
    ky_array_destroy(&instances);
}
```

### 纹理图集

```c
// 纹理图集管理
typedef struct kyTextureAtlas {
    kyTexture *atlas;              // 合并后的纹理
    int width, height;             // 图集尺寸
    struct {
        int x, y;                 // 在图集中的位置
        int w, h;                 // 尺寸
    } *rects;                     // 子纹理位置
    int rect_count;               // 子纹理数量
    kyAllocator *alloc;
} kyTextureAtlas;

// 创建纹理图集
kyTextureAtlas *ky_texture_atlas_create(kyAllocator *alloc, int width, int height, int max_textures) {
    kyTextureAtlas *atlas = ky_alloc(alloc, sizeof(kyTextureAtlas));
    atlas->width = width;
    atlas->height = height;
    atlas->rects = ky_alloc(alloc, max_textures * sizeof(typeof(atlas->rects[0])));
    atlas->rect_count = 0;
    atlas->alloc = alloc;
    return atlas;
}

// 添加纹理到图集
int ky_texture_atlas_add(kyTextureAtlas *atlas, int width, int height) {
    if (atlas->rect_count >= MAX_TEXTURES) {
        return -1;
    }
    
    // 使用简单的打包算法（这里可以替换为更复杂的算法）
    int x = 0, y = 0;
    
    // 找到合适的位置
    for (int i = 0; i < atlas->rect_count; i++) {
        if (atlas->rects[i].x + atlas->rects[i].w + width <= atlas->width) {
            x = atlas->rects[i].x + atlas->rects[i].w;
            y = atlas->rects[i].y;
            break;
        }
    }
    
    // 检查是否超出边界
    if (x + width > atlas->width || y + height > atlas->height) {
        return -1;  // 没有足够空间
    }
    
    // 添加纹理矩形
    atlas->rects[atlas->rect_count].x = x;
    atlas->rects[atlas->rect_count].y = y;
    atlas->rects[atlas->rect_count].w = width;
    atlas->rects[atlas->rect_count].h = height;
    
    return atlas->rect_count++;
}

// 获取纹理UV坐标
void ky_texture_atlas_get_uv(kyTextureAtlas *atlas, int index, float *uvs) {
    if (index < 0 || index >= atlas->rect_count) return;
    
    typeof(atlas->rects[0]) *rect = &atlas->rects[index];
    
    uvs[0] = (float)rect->x / atlas->width;          // u0
    uvs[1] = (float)rect->y / atlas->height;         // v0
    uvs[2] = (float)(rect->x + rect->w) / atlas->width;  // u1
    uvs[3] = (float)(rect->y + rect->h) / atlas->height; // v1
}
```

## 调试工具

### 渲染调试信息

```c
// 渲染统计
typedef struct kyRenderStats {
    int draw_calls;               // 绘制调用次数
    int triangles;               // 渲染的三角形数量
    int textures_bound;           // 绑定的纹理数量
    float total_draw_time;        // 总绘制时间
    float gpu_memory_usage;       // GPU 内存使用量
} kyRenderStats;

// 获取渲染统计
void ky_rd_get_stats(kyRenderDevice *rd, kyRenderStats *stats);

// 渲染调试信息
void ky_rd_debug_draw_aabb(kyRenderDevice *rd, const kyAABB *aabb, const kyVec4 *color);
void ky_rd_debug_draw_sphere(kyRenderDevice *rd, const kySphere *sphere, const kyVec4 *color);
void ky_rd_debug_draw_frustum(kyRenderDevice *rd, const kyMat4 *frustum, const kyVec4 *color);

// 纹理可视化调试
void ky_rd_debug_draw_texture_info(kyRenderDevice *rd, kyTexture *tex, int x, int y);
```

### 性能分析

```c
// 渲染性能分析器
typedef struct kyRenderProfiler {
    struct {
        const char *name;
        uint64_t time;
        uint64_t calls;
    } *passes;
    int pass_count;
    uint64_t frame_start;
    uint64_t total_time;
} kyRenderProfiler;

void ky_render_profile_begin(kyRenderProfiler *profiler, const char *pass_name);
void ky_render_profile_end(kyRenderProfiler *profiler);

void ky_render_profile_print(kyRenderProfiler *profiler) {
    printf("Render Performance:\n");
    for (int i = 0; i < profiler->pass_count; i++) {
        printf("  %s: %.2f ms (%.1f%%)\n", 
               profiler->passes[i].name,
               profiler->passes[i].time / 1000000.0,
               (profiler->passes[i].time * 100.0) / profiler->total_time);
    }
}
```

---

*渲染模块为 Kronyx Engine 提供了强大的2D图形渲染能力，通过合理的批处理优化和高级特性，可以高效地渲染复杂的2D游戏场景。*