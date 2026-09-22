# Kronyx Engine API 接口文档

## 核心接口

### 数学库 (`kronyx/math.h`)

```c
// 向量类型
typedef struct { float x, y, z, w; } kyVec4;
typedef struct { float x, y, z; } kyVec3;
typedef struct { float x, y; } kyVec2;
typedef struct { float x, y, z, w; } kyQuat;

// 矩阵类型
typedef struct { float m[16]; } kyMat4;

// 基础数学函数
KY_INLINE float ky_vec3_dot(const kyVec3 *a, const kyVec3 *b);
KY_INLINE kyVec3 ky_vec3_cross(const kyVec3 *a, const kyVec3 *b);
KY_INLINE kyVec3 ky_vec3_normalize(const kyVec3 *v);
KY_INLINE kyMat4 ky_mat4_identity(void);
KY_INLINE kyMat4 ky_mat4_ortho(float left, float right, float bottom, float top, float near, float far);
KY_INLINE kyMat4 ky_mat4_multiply(const kyMat4 *a, const kyMat4 *b);
```

### 内存管理 (`kronyx/memory.h`)

```c
// 内存分配器接口
typedef struct kyAllocator {
    void *(*alloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t size);
    void (*free)(void *ctx, void *ptr);
    void *ctx;
} kyAllocator;

// 默认分配器
extern const kyAllocator ky_default_allocator;

// Arena分配器（快速分配，整块释放）
typedef struct kyArena kyArena;
KY_API kyArena *ky_arena_create(kyAllocator *alloc, size_t capacity);
KY_API void ky_arena_destroy(kyArena *arena);
KY_API void ky_arena_reset(kyArena *arena);
KY_API void *ky_arena_alloc(kyArena *arena, size_t size);
```

### 容器类型

#### 数组 (`kronyx/array.h`)
```c
typedef struct kyArray {
    void *data;
    size_t count;
    size_t capacity;
    size_t item_size;
    kyAllocator *alloc;
} kyArray;

KY_API kyArray ky_array_create(size_t item_size, kyAllocator *alloc);
KY_API void ky_array_push(kyArray *arr, const void *item);
KY_API void ky_array_pop(kyArray *arr);
KY_API void ky_array_clear(kyArray *arr);
KY_API void ky_array_destroy(kyArray *arr);
```

#### 哈希映射 (`kronyx/hashmap.h`)
```c
typedef struct kyHashMap kyHashMap;
typedef uint32_t (*kyHashFn)(const void *key);
typedef int (*kyCompareFn)(const void *a, const void *b);

KY_API kyHashMap *ky_hashmap_create(kyHashFn hash, kyCompareFn compare, size_t key_size, size_t value_size, kyAllocator *alloc);
KY_API void *ky_hashmap_get(kyHashMap *map, const void *key);
KY_API int ky_hashmap_set(kyHashMap *map, const void *key, const void *value);
KY_API int ky_hashmap_remove(kyHashMap *map, const void *key);
KY_API void ky_hashmap_destroy(kyHashMap *map);
```

## ECS 接口 (`kronyx/ecs.h`)

### 核心类型
```c
typedef struct kyEntity {
    uint32_t id;
    uint32_t version;
} kyEntity;

typedef struct kyComponentType {
    const char *name;
    size_t size;
    uint32_t type_id;
    void (*ctor)(void *comp);
    void (*dtor)(void *comp);
} kyComponentType;

typedef struct kyWorld kyWorld;
```

### 世界管理
```c
// 创建/销毁世界
KY_API kyWorld *ky_world_create(kyAllocator *alloc);
KY_API void ky_world_destroy(kyWorld *w);

// 组件注册
KY_API uint32_t ky_world_register_component(kyWorld *w, const kyComponentType *t);
KY_API uint32_t ky_world_component_type_by_name(const kyWorld *w, const char *name);

// 实体操作
KY_API kyEntity ky_world_spawn(kyWorld *w);
KY_API void ky_world_despawn(kyWorld *w, kyEntity e);
KY_API int ky_entity_valid(const kyWorld *w, kyEntity e);
```

### 组件操作
```c
// 组件管理
KY_API void *ky_world_add_component(kyWorld *w, kyEntity e, uint32_t type_id);
KY_API void *ky_world_get_component(const kyWorld *w, kyEntity e, uint32_t type_id);
KY_API int ky_world_has_component(const kyWorld *w, kyEntity e, uint32_t type_id);
KY_API void ky_world_remove_component(kyWorld *w, kyEntity e, uint32_t type_id);

// 视图迭代器
typedef struct kyViewIter {
    const kyWorld *w;
    const uint32_t *types;
    uint32_t type_count;
    int32_t arch_index;
    size_t row;
    kyEntity current;
} kyViewIter;

KY_API int ky_view_begin(const kyWorld *w, const uint32_t *types, uint32_t type_count, kyViewIter *it);
KY_API int ky_view_next(kyViewIter *it);
```

## 渲染接口 (`kronyx/render.h`)

### 渲染设备
```c
typedef enum kyRendererBackend {
    KY_RENDERER_NONE = 0,
    KY_RENDERER_CONSOLE,
    KY_RENDERER_GL,
    KY_RENDERER_VULKAN
} kyRendererBackend;

typedef struct kyRenderDevice kyRenderDevice;

// 创建/销毁渲染设备
KY_API kyRenderDevice *ky_rd_create(kyRendererBackend backend, void *platform_win);
KY_API void ky_rd_destroy(kyRenderDevice *rd);

// 渲染管线
typedef struct kyPipelineDesc {
    void *shader;
    kyVertexLayout layout;
    kyPrimitiveTopology topology;
    int depth_test;
    int depth_write;
    int cull_mode;
    struct { int on; kyVec4 color; } blend;
} kyPipelineDesc;

KY_API kyPipeline *ky_rd_create_pipeline(kyRenderDevice *rd, const kyPipelineDesc *desc);
KY_API void ky_rd_destroy_pipeline(kyRenderDevice *rd, kyPipeline *p);
```

### 命令列表
```c
// 命令列表操作
KY_API void *ky_rd_begin(kyRenderDevice *rd);
KY_API void ky_cmd_set_pipeline(void *cl, kyPipeline *p);
KY_API void ky_cmd_set_vertex_buffer(void *cl, kyBuffer *vb, uint32_t stride);
KY_API void ky_cmd_set_index_buffer(void *cl, kyBuffer *ib, uint32_t index_size);
KY_API void ky_cmd_set_texture(void *cl, int slot, kyTexture *tex);
KY_API void ky_cmd_set_uniform(void *cl, int location, const void *data, int bytes);
KY_API void ky_cmd_draw_indexed(void *cl, uint32_t count, uint32_t instances);
KY_API void ky_cmd_draw_array(void *cl, uint32_t vertex_count, uint32_t instances);

// 提交和呈现
KY_API void ky_rd_submit(kyRenderDevice *rd, void *cl);
KY_API void ky_rd_present(kyRenderDevice *rd);
KY_API void ky_rd_clear(kyRenderDevice *rd, kyVec4 clear_color, float clear_depth);
```

## 2D 渲染 (`kronyx/2d.h`)

```c
// 精灵组件
typedef struct kySprite {
    kyTexture *texture;
    kyVec4 color;            // tint color (RGBA 0-1)
    kyVec2 pivot;            // center in local space
    float rotation;          // radians
    int layer;               // render layer (lower = first)
    int z;                   // layer-internal sort key
    int id;                  // unique sprite ID for batching
} kySprite;

// 精灵渲染函数
KY_API int ky2d_render_world_auto(kyRenderDevice *rd, kyWorld *w);
KY_API int ky2d_render_world(kyRenderDevice *rd, kyWorld *w, const kyMat4 *view_proj);

// 注册2D组件
KY_API void ky2d_register_components(kyWorld *w);

// 2D纹理创建
KY_API kyTexture *ky2d_make_texture(kyRenderDevice *rd, int w, int h, int channels, const void *pixels);
```

## 物理接口 (`kronyx/physics.h`)

```c
// 物理世界
typedef struct kyPhysicsWorld kyPhysicsWorld;

// 创建/销毁物理世界
KY_API kyPhysicsWorld *ky_physics_world_create(kyAllocator *alloc);
KY_API void ky_physics_world_destroy(kyPhysicsWorld *pw);

// 刚体组件
typedef struct kyRigidBody {
    int type;                // KY_STATIC_KINEMATIC / KY_DYNAMIC
    float mass;
    float friction;
    float restitution;
    kyVec2 linear_velocity;
    float angular_velocity;
    float inv_mass;
    float inv_inertia;
    int is_sleeping;
} kyRigidBody;

// 物理操作
KY_API void ky_physics_world_add_body(kyPhysicsWorld *pw, kyEntity e, const kyRigidBody *desc);
KY_API void ky_physics_world_remove_body(kyPhysicsWorld *pw, kyEntity e);
KY_API void ky_physics_world_step(kyPhysicsWorld *pw, float dt);
KY_API void ky_physics_world_ray_cast(kyPhysicsWorld *pw, kyVec2 start, kyVec2 end, void *user, 
                                     int (*callback)(kyEntity, kyVec2, kyVec2, void*));

// 事件系统
#define KY_EVENT_COLLIDE "collide"
```

## 脚本接口 (`kronyx/script.h`)

```c
// 虚拟机
typedef struct kyVM kyVM;

// 创建/销毁VM
KY_API kyVM *ky_vm_create(kyAllocator *alloc);
KY_API void ky_vm_destroy(kyVM *vm);

// 脚本编译与执行
KY_API int ky_vm_compile(kyVM *vm, const char *source);
KY_API int ky_vm_execute(kyVM *vm);
KY_API int ky_vm_call(kyVM *vm, const char *function_name, int argc, const kyValue *argv);

// 值类型
typedef enum {
    KYT_NIL,
    KYT_BOOL,
    KYT_INT,
    KYT_FLOAT,
    KYT_STRING,
    KYT_ENTITY  // 64-bit entity handle
} kyValueType;

typedef struct kyValue {
    kyValueType type;
    union {
        int64_t i;
        double f;
        char *str;
        kyEntity entity;
    } as;
} kyValue;
```

## 资源管理 (`kronyx/resource.h`)

```c
// 资源类型
typedef enum {
    KY_RES_RAW_BYTES,
    KY_RES_PIXELBUFFER
} kyResourceKind;

typedef struct kyResource {
    const char *path;
    kyResourceKind kind;
    uint32_t refcount;
    void *payload;
    void (*on_destroy)(kyAllocator*, void *payload);
} kyResource;

// 资源管理器接口
typedef struct kyResourceManager kyResourceManager;

KY_API kyResourceManager *ky_resmgr_create(kyAllocator *alloc);
KY_API void ky_resmgr_destroy(kyResourceManager *m);

// 资源操作
KY_API kyResource *ky_resmgr_register(kyResourceManager *m, const char *path, kyResourceKind kind, 
                                     void *payload, size_t size, void (*on_destroy)(kyAllocator*, void*));
KY_API kyResource *ky_resmgr_acquire(kyResourceManager *m, const char *path);
KY_API void ky_resmgr_release(kyResourceManager *m, kyResource *r);
KY_API int ky_resmgr_count(kyResourceManager *m, const char *path);

// 便捷函数
KY_API kyResource *ky_resmgr_make_pixelbuffer(kyResourceManager *m, const char *path, 
                                             const void *pixels, size_t byte_count, 
                                             int w, int h, int channels);
KY_API kyResource *ky_resmgr_make_raw_bytes(kyResourceManager *m, const char *path, 
                                           const void *data, size_t byte_count);
```

## 场景序列化 (`kronyx/scene.h`)

```c
// 场景加载与保存
KY_API int ky_scene_save(kyWorld *w, const char *filename);
KY_API int ky_scene_load(kyWorld *w, const char *filename);

// 场景组件要求
typedef struct kySceneConfig {
    uint32_t transform_component;  // required component type_id
    int max_entities;             // maximum entities (64)
} kySceneConfig;

KY_API void ky_scene_set_config(kyWorld *w, const kySceneConfig *config);
```

## 输入系统 (`kronyx/input.h`)

```c
// 输入状态查询
typedef enum kyKey {
    KY_KEY_A, KY_KEY_B, KY_KEY_C, KY_KEY_D, KY_KEY_SPACE,
    KY_KEY_LEFT, KY_KEY_RIGHT, KY_KEY_UP, KY_KEY_DOWN,
    // ... more keys
} kyKey;

KY_API int ky_key_pressed(kyKey key);
KY_API int ky_key_down(kyKey key);
KY_API int ky_key_released(kyKey key);

// 按钮状态
typedef struct kyButtonState {
    int pressed;
    int down;
    int released;
} kyButtonState;

KY_API const kyButtonState *ky_button_state(kyKey key);
```

## 打包系统 (`kronyx/pack.h`)

```c
// 打包引擎与脚本
KY_API int ky_pack_engine_and_script(const char *engine_root, const char *script_path, 
                                     const char *output_path, const char *format);

// 输出格式枚举
typedef enum kyPackFormat {
    KY_PACK_EXE,
    KY_PACK_NPM,
    KY_PACK_JAR
} kyPackFormat;
```

## 运行时 (`kronyx/engine.h`)

```c
// 游戏循环
typedef struct kyEngineConfig {
    int window_width;
    int window_height;
    const char *window_title;
    int headless;               // 无头模式（无窗口）
    kyTamperMode antitamper_mode;
} kyEngineConfig;

KY_API int ky_engine_run(const kyEngineConfig *config, 
                        void (*update)(kyWorld*, float), 
                        void (*render)(kyWorld*, kyRenderDevice*));
```

## 错误处理

```c
// 错误码
typedef enum kyErrorCode {
    KY_OK = 0,
    KY_ERROR_OUT_OF_MEMORY,
    KY_ERROR_INVALID_PARAMETER,
    KY_ERROR_FILE_NOT_FOUND,
    KY_ERROR_IO_ERROR,
    // ... more error codes
} kyErrorCode;

// 错误回调
typedef void (*kyErrorCallback)(kyErrorCode code, const char *message);

KY_API void ky_set_error_callback(kyErrorCallback callback);
```

## 版本信息

```c
// 版本查询
KY_API const char *ky_version(void);
KY_API int ky_major_version(void);
KY_API int ky_minor_version(void);
KY_API int ky_patch_version(void);
```

## 使用示例

### 基本渲染循环
```c
kyAllocator alloc = ky_default_allocator();
kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);
kyWorld *w = ky_world_create(&alloc);

// 创建实体
kyEntity e = ky_world_spawn(w);
Transform *t = ky_world_add_component(w, e, tid_transform);
t->pos = (kyVec3){0, 0, 0};

// 游戏循环
while (!should_quit) {
    ky_rd_clear(rd, (kyVec4){0.1f, 0.1f, 0.1f, 1.0f}, 1.0f);
    ky2d_render_world_auto(rd, w);
    ky_rd_present(rd);
}

ky_rd_destroy(rd);
ky_world_destroy(w);
```

### 2D 平台游戏示例
```c
// 注册组件
typedef struct Player {
    kyVec2 velocity;
    float speed;
    float jump_force;
} Player;

// 创建玩家实体
kyEntity player = ky_world_spawn(w);
Transform *t = ky_world_add_component(w, player, tid_transform);
t->pos = (kyVec3){0, 0, 0};
Player *p = ky_world_add_component(w, player, tid_player);
p->speed = 200.0f;
p->jump_force = 300.0f;

// 添加精灵
Sprite *s = ky_world_add_component(w, player, tid_sprite);
s->texture = hero_texture;
s->layer = 1;

// 更新循环
void update(kyWorld *w, float dt) {
    // 处理输入
    if (ky_key_pressed(KY_KEY_RIGHT)) {
        p->velocity.x = p->speed;
    } else if (ky_key_pressed(KY_KEY_LEFT)) {
        p->velocity.x = -p->speed;
    } else {
        p->velocity.x = 0;
    }
    
    // 重力
    p->velocity.y += 980.0f * dt;  // gravity
    
    // 更新位置
    t->pos.x += p->velocity.x * dt;
    t->pos.y += p->velocity.y * dt;
}
```