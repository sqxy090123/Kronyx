# 核心模块 (Core)

## 概述

Core 模块是 Kronyx Engine 的基础层，提供零外部依赖的核心工具和基础设施。所有其他模块都直接或间接依赖 Core 模块。

### 模块组成

```
Core/
├── math.h          # 数学库
├── memory.h        # 内存管理
├── array.h         # 动态数组
├── hashmap.h       # 哈希映射
├── string.h        # 字符串操作
├── log.h           # 日志系统
├── time.h          # 时间工具
├── event.h         # 事件系统
├── input.h         # 输入处理
├── arena.h         # Arena 分配器
├── pool.h          # 对象池
└── file.h          # 文件操作
```

## 数学库 (math.h)

### 核心数据类型

```c
// 向量类型
typedef struct { float x, y, z, w; } kyVec4;
typedef struct { float x, y, z; } kyVec3;
typedef struct { float x, y; } kyVec2;
typedef struct { float x; } kyVec1;

// 四元数
typedef struct { float x, y, z, w; } kyQuat;

// 矩阵
typedef struct { float m[16]; } kyMat4;
typedef struct { float m[9]; } kyMat3;
typedef struct { float m[4]; } kyMat2;

// 矩阵（行主序）
typedef struct { float m[4][4]; } kyMat4x4;

// 边界框
typedef struct { kyVec3 min, max; } kyAABB;
typedef struct { kyVec3 center; float radius; } kySphere;
```

### 向量运算

```c
// 向量创建
KY_INLINE kyVec3 ky_vec3(float x, float y, float z);
KY_INLINE kyVec3 ky_vec3_zero(void);
KY_INLINE kyVec3 ky_vec3_one(void);

// 向量运算
KY_INLINE kyVec3 ky_vec3_add(const kyVec3 *a, const kyVec3 *b);
KY_INLINE kyVec3 ky_vec3_sub(const kyVec3 *a, const kyVec3 *b);
KY_INLINE kyVec3 ky_vec3_mul(const kyVec3 *a, float s);
KY_INLINE kyVec3 ky_vec3_div(const kyVec3 *a, float s);

// 点积与叉积
KY_INLINE float ky_vec3_dot(const kyVec3 *a, const kyVec3 *b);
KY_INLINE kyVec3 ky_vec3_cross(const kyVec3 *a, const kyVec3 *b);

// 向量长度与归一化
KY_INLINE float ky_vec3_length(const kyVec3 *v);
KY_INLINE float ky_vec3_length_sq(const kyVec3 *v);
KY_INLINE kyVec3 ky_vec3_normalize(const kyVec3 *v);

// 插值
KY_INLINE kyVec3 ky_vec3_lerp(const kyVec3 *a, const kyVec3 *b, float t);
KY_INLINE kyVec3 ky_vec3_slerp(const kyVec3 *a, const kyVec3 *b, float t);
```

### 矩阵运算

```c
// 矩阵创建
kyMat4 ky_mat4_identity(void);
kyMat4 ky_mat4_ortho(float left, float right, float bottom, float top, float near, float far);
kyMat4 ky_mat4_perspective(float fovy, float aspect, float near, float far);
kyMat4 ky_mat4_lookat(const kyVec3 *eye, const kyVec3 *center, const kyVec3 *up);
kyMat4 ky_mat4_translate(const kyVec3 *translation);
kyMat4 ky_mat4_scale(const kyVec3 *scale);
kyMat4 ky_mat4_rotate_x(float angle);
kyMat4 ky_mat4_rotate_y(float angle);
kyMat4 ky_mat4_rotate_z(float angle);
kyMat4 ky_mat4_rotate_axis(const kyVec3 *axis, float angle);

// 矩阵运算
kyMat4 ky_mat4_multiply(const kyMat4 *a, const kyMat4 *b);
kyMat4 ky_mat4_inverse(const kyMat4 *m);
kyMat4 ky_mat4_transpose(const kyMat4 *m);

// 矩阵应用
kyVec3 ky_mat4_transform_vec3(const kyMat4 *m, const kyVec3 *v);
kyVec4 ky_mat4_transform_vec4(const kyMat4 *m, const kyVec4 *v);
```

### 几何运算

```c
// 边界框运算
kyAABB ky_aabb_union(const kyAABB *a, const kyAABB *b);
kyAABB ky_aabb_transform(const kyAABB *a, const kyMat4 *m);
int ky_aabb_contains_point(const kyAABB *a, const kyVec3 *p);
int ky_aabb_intersects_aabb(const kyAABB *a, const kyAABB *b);

// 球体运算
kySphere ky_sphere_union(const kySphere *a, const kySphere *b);
int ky_sphere_contains_point(const kySphere *s, const kyVec3 *p);
int ky_sphere_intersects_sphere(const kySphere *a, const kySphere *b);
int ky_sphere_intersects_aabb(const kySphere *s, const kyAABB *a);

// 射线检测
typedef struct {
    kyVec3 origin;
    kyVec3 direction;
} kyRay;

int ky_ray_intersects_aabb(const kyRay *ray, const kyAABB *aabb, float *t_min, float *t_max);
int ky_ray_intersects_sphere(const kyRay *ray, const kySphere *sphere, float *t_min, float *t_max);
```

### 使用示例

```c
// 3D 变换示例
void transform_object(kyMat4 *model_matrix, float time) {
    // 创建旋转矩阵
    kyMat4 rotation = ky_mat4_multiply(
        ky_mat4_rotate_y(time * 2.0f),
        ky_mat4_rotate_x(time * 1.5f)
    );
    
    // 创建缩放矩阵
    kyMat4 scale = ky_mat4_scale(&(kyVec3){2.0f, 2.0f, 2.0f});
    
    // 创建平移矩阵
    kyMat4 translation = ky_mat4_translate(&(kyVec3){
        sin(time) * 5.0f,
        cos(time) * 2.0f,
        0.0f
    });
    
    // 组合变换：T * R * S
    *model_matrix = ky_mat4_multiply(translation, 
        ky_mat4_multiply(rotation, scale));
}

// 相机投影示例
void setup_camera(kyMat4 *view_proj, const kyVec3 *camera_pos, const kyVec3 *target) {
    kyMat4 view = ky_mat4_lookat(camera_pos, target, &(kyVec3){0, 1, 0});
    kyMat4 proj = ky_mat4_perspective(45.0f * KY_DEG2RAD, 16.0f/9.0f, 0.1f, 100.0f);
    
    *view_proj = ky_mat4_multiply(proj, view);
}
```

## 内存管理 (memory.h)

### 分配器接口

```c
// 分配器结构
typedef struct kyAllocator {
    void *(*alloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t size);
    void (*free)(void *ctx, void *ptr);
    void *ctx;
} kyAllocator;

// 默认分配器（使用 malloc/free）
extern const kyAllocator ky_default_allocator;

// 自定义分配器示例
static void* my_alloc(void *ctx, size_t size) {
    printf("Allocating %zu bytes\n", size);
    return malloc(size);
}

static void my_free(void *ctx, void *ptr) {
    printf("Freeing pointer\n");
    free(ptr);
}

kyAllocator my_allocator = {
    .alloc = my_alloc,
    .free = my_free,
    .ctx = NULL
};
```

### 内存池

```c
typedef struct kyMemoryPool {
    void *memory;
    size_t block_size;
    size_t capacity;
    size_t used;
    void *free_list;
} kyMemoryPool;

// 创建内存池
kyMemoryPool *ky_pool_create(kyAllocator *alloc, size_t block_size, size_t capacity);

// 从池中分配
void *ky_pool_alloc(kyMemoryPool *pool);

// 释放回池
void ky_pool_free(kyMemoryPool *pool, void *ptr);

// 销毁池
void ky_pool_destroy(kyMemoryPool *pool);
```

### 跟踪分配器

```c
typedef struct kyTrackingAllocator {
    kyAllocator base;
    size_t total_allocated;
    size_t peak_allocated;
    size_t allocation_count;
} kyTrackingAllocator;

// 使用示例
void *tracked_alloc(void *ctx, size_t size) {
    kyTrackingAllocator *ta = (kyTrackingAllocator*)ctx;
    ta->total_allocated += size;
    ta->peak_allocated = ky_max(ta->peak_allocated, ta->total_allocated);
    ta->allocation_count++;
    return malloc(size);
}

void tracked_free(void *ctx, void *ptr) {
    kyTrackingAllocator *ta = (kyTrackingAllocator*)ctx;
    free(ptr);
    // 注意：这里需要知道分配的大小来更新 total_allocated
}
```

## 动态数组 (array.h)

### 基本操作

```c
typedef struct kyArray {
    void *data;
    size_t count;
    size_t capacity;
    size_t item_size;
    kyAllocator *alloc;
} kyArray;

// 创建数组
kyArray ky_array_create(size_t item_size, kyAllocator *alloc);

// 添加元素
void ky_array_push(kyArray *arr, const void *item);

// 移除元素
void ky_array_pop(kyArray *arr);

// 插入元素
void ky_array_insert(kyArray *arr, size_t index, const void *item);

// 删除元素
void ky_array_remove(kyArray *arr, size_t index);

// 清空数组
void ky_array_clear(kyArray *arr);

// 销毁数组
void ky_array_destroy(kyArray *arr);
```

### 类型安全宏

```c
// 类型安全的数组操作
#define KY_ARRAY_T(arr) ((typeof(arr))arr)
#define KY_ARRAY_PUSH(arr, item) ky_array_push(&(arr), &(item))
#define KY_ARRAY_POP(arr) ky_array_pop(&(arr))

// 遍历数组
#define KY_ARRAY_FOREACH(arr, item, index) \
    for (index = 0; index < (arr).count; index++) { \
        typeof(*(arr).data) *item = &((typeof(*(arr).data)*)((arr).data))[index]

// 使用示例
kyArray_int numbers = ky_array_create(sizeof(int), &ky_default_allocator);
KY_ARRAY_PUSH(numbers, 42);
KY_ARRAY_PUSH(numbers, 100);

KY_ARRAY_FOREACH(numbers, num, i) {
    printf("%d ", *num);
}
```

## 哈希映射 (hashmap.h)

### 基本操作

```c
typedef struct kyHashMap kyHashMap;

// 哈希函数类型
typedef uint32_t (*kyHashFn)(const void *key);
typedef int (*kyCompareFn)(const void *a, const void *b);

// 创建哈希映射
kyHashMap *ky_hashmap_create(kyHashFn hash, kyCompareFn compare, 
                             size_t key_size, size_t value_size, 
                             kyAllocator *alloc);

// 插入/更新值
int ky_hashmap_set(kyHashMap *map, const void *key, const void *value);

// 获取值
void *ky_hashmap_get(kyHashMap *map, const void *key);

// 删除键值对
int ky_hashmap_remove(kyHashMap *map, const void *key);

// 清空映射
void ky_hashmap_clear(kyHashMap *map);

// 销毁映射
void ky_hashmap_destroy(kyHashMap *map);

// 统计信息
size_t ky_hashmap_size(kyHashMap *map);
size_t ky_hashmap_capacity(kyHashMap *map);
```

### 内置哈希函数

```c
// 字符串哈希
uint32_t ky_hash_string(const void *key);

// 整数哈希
uint32_t ky_hash_int32(const void *key);

// 指针哈希
uint32_t ky_hash_ptr(const void *key);

// FNV-1a 哈希算法
uint32_t ky_hash_fnv1a(const void *key, size_t len);
```

### 使用示例

```c
// 字符串到整数的映射
kyHashMap *string_to_int = ky_hashmap_create(
    ky_hash_string,        // 字符串哈希函数
    strcmp,                // 字符串比较函数
    sizeof(char*),         // 键大小（字符串指针）
    sizeof(int),           // 值大小（整数）
    &ky_default_allocator
);

// 插入键值对
const char *key1 = "score";
int value1 = 100;
ky_hashmap_set(string_to_int, &key1, &value1);

// 获取值
const char *key2 = "score";
int *score = ky_hashmap_get(string_to_int, &key2);
if (score) {
    printf("Score: %d\n", *score);
}

// 自定义结构体映射
typedef struct {
    char name[32];
    int level;
    float health;
} Player;

// 使用自定义哈希和比较函数
uint32_t player_hash(const void *key) {
    const Player *p = (const Player*)key;
    return ky_hash_string(p->name);
}

int player_compare(const void *a, const void *b) {
    const Player *pa = (const Player*)a;
    const Player *pb = (const Player*)b;
    return strcmp(pa->name, pb->name);
}

kyHashMap *player_stats = ky_hashmap_create(
    player_hash, player_compare, 
    sizeof(Player), sizeof(float),
    &ky_default_allocator
);
```

## 日志系统 (log.h)

### 日志级别

```c
typedef enum kyLogLevel {
    KY_LOG_TRACE = 0,
    KY_LOG_DEBUG,
    KY_LOG_INFO,
    KY_LOG_WARN,
    KY_LOG_ERROR,
    KY_LOG_FATAL
} kyLogLevel;
```

### 日志配置

```c
// 日志回调函数
typedef void (*kyLogCallback)(kyLogLevel level, const char *message, void *user_data);

// 配置日志系统
void ky_log_set_level(kyLogLevel level);
void ky_log_set_callback(kyLogCallback callback, void *user_data);

// 日志输出宏
#define KY_LOG_TRACE(...) ky_log_write(KY_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define KY_LOG_DEBUG(...) ky_log_write(KY_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define KY_LOG_INFO(...)  ky_log_write(KY_LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define KY_LOG_WARN(...)  ky_log_write(KY_LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define KY_LOG_ERROR(...) ky_log_write(KY_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define KY_LOG_FATAL(...) ky_log_write(KY_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
```

### 自定义日志格式

```c
// 自定义日志格式化函数
typedef char *(*kyLogFormatter)(kyLogLevel level, const char *file, int line, const char *fmt, va_list args);

// 设置自定义格式化器
void ky_log_set_formatter(kyLogFormatter formatter);

// 示例：彩色日志格式化
char *colored_log_formatter(kyLogLevel level, const char *file, int line, const char *fmt, va_list args) {
    static char buffer[1024];
    const char *color = "";
    
    switch (level) {
        case KY_LOG_TRACE: color = "\033[36m"; break;    // Cyan
        case KY_LOG_DEBUG: color = "\033[32m"; break;    // Green
        case KY_LOG_INFO:  color = "\033[34m"; break;    // Blue
        case KY_LOG_WARN:  color = "\033[33m"; break;    // Yellow
        case KY_LOG_ERROR: color = "\033[31m"; break;    // Red
        case KY_LOG_FATAL: color = "\033[35m"; break;    // Magenta
    }
    
    const char *reset = "\033[0m";
    snprintf(buffer, sizeof(buffer), "%s[%s] %s:%d: %s%s", 
             color, ky_log_level_string(level), file, line, fmt, reset);
    
    vsnprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), fmt, args);
    return buffer;
}
```

### 使用示例

```c
// 基本日志使用
KY_LOG_INFO("Game initialized with %d entities", entity_count);
KY_LOG_ERROR("Failed to load texture: %s", texture_path);

// 条件日志
if (player_health <= 0) {
    KY_LOG_WARN("Player health is critical: %f", player_health);
}

// 性能日志
void performance_timer_start(void);
void performance_timer_end(const char *operation);

performance_timer_start();
// ... 执行耗时操作 ...
performance_timer_end("entity_update");
```

## 时间工具 (time.h)

### 时间类型

```c
// 时间点（毫秒）
typedef int64_t kyTime;

// 时间差
typedef struct {
    int64_t seconds;
    int32_t nanoseconds;
} kyDuration;

// 高精度计时器
typedef struct kyTimer {
    kyTime start;
    kyTime accumulated;
    int running;
} kyTimer;
```

### 时间函数

```c
// 获取当前时间
kyTime ky_time_now(void);                          // 返回毫秒
kyTime ky_time_now_highres(void);                 // 高精度时间（纳秒）

// 时间转换
float ky_time_to_seconds(kyTime ms);
float ky_time_to_milliseconds(kyTime ms);
float ky_time_to_microseconds(kyTime ms);
float ky_time_to_nanoseconds(kyTime ms);

kyTime ky_time_from_seconds(float seconds);
kyTime ky_time_from_milliseconds(float ms);
```

### 计时器操作

```c
// 计时器操作
void ky_timer_start(kyTimer *timer);
void ky_timer_stop(kyTimer *timer);
void ky_timer_reset(kyTimer *timer);
void ky_timer_accumulate(kyTimer *timer);

// 获取计时器值
kyTime ky_timer_elapsed_ms(kyTimer *timer);
float ky_timer_elapsed_seconds(kyTimer *timer);

// 游戏循环计时
void game_loop(void) {
    kyTimer frame_timer = {0};
    kyTimer total_timer = {0};
    kyTime last_time = ky_time_now();
    int frame_count = 0;
    
    ky_timer_start(&total_timer);
    
    while (!should_quit) {
        ky_timer_start(&frame_timer);
        
        // 计算帧时间
        kyTime current_time = ky_time_now();
        float dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;
        
        // 更新游戏逻辑
        update_game(dt);
        
        // 渲染
        render_game();
        
        // 限制帧率
        ky_timer_stop(&frame_timer);
        kyTime frame_time = ky_timer_elapsed_ms(&frame_timer);
        
        if (frame_time < 16.67f) {  // 60 FPS = 16.67ms per frame
            ky_sleep((uint32_t)(16.67f - frame_time));
        }
        
        frame_count++;
        
        // 每秒输出一次性能信息
        if (frame_count % 60 == 0) {
            float total_time = ky_timer_elapsed_seconds(&total_timer);
            KY_LOG_INFO("FPS: %.1f, Total time: %.1f s", 60.0f, total_time);
        }
    }
    
    ky_timer_stop(&total_timer);
    KY_LOG_INFO("Game ran for %.1f seconds", ky_timer_elapsed_seconds(&total_timer));
}
```

## 事件系统 (event.h)

### 事件类型

```c
// 事件ID
typedef enum kyEventType {
    KY_EVENT_KEY_PRESS,
    KY_EVENT_KEY_RELEASE,
    KY_EVENT_MOUSE_MOVE,
    KY_EVENT_MOUSE_BUTTON,
    KY_EVENT_WINDOW_RESIZE,
    KY_EVENT_COLLISION,
    KY_EVENT_CUSTOM,
    // ... 更多事件类型
} kyEventType;

// 事件数据
typedef struct kyEvent {
    kyEventType type;
    uint32_t entity_id;        // 相关实体ID（可选）
    union {
        struct {
            int key;
            int scancode;
            int mods;
        } key;
        
        struct {
            double x, y;
            double dx, dy;
        } mouse;
        
        struct {
            int width, height;
        } resize;
        
        struct {
            void *data;
            size_t size;
        } custom;
    } data;
    
    void *user_data;           // 用户自定义数据
} kyEvent;
```

### 事件监听器

```c
// 事件回调函数类型
typedef void (*kyEventCallback)(const kyEvent *event, void *user_data);

// 事件监听器
typedef struct kyEventListener {
    kyEventType type;
    kyEventCallback callback;
    void *user_data;
    int active;
} kyEventListener;

// 事件系统
typedef struct kyEventSystem {
    kyArray listeners;
    kyAllocator *alloc;
} kyEventSystem;

// 事件系统操作
kyEventSystem *ky_event_system_create(kyAllocator *alloc);
void ky_event_system_destroy(kyEventSystem *system);

// 事件监听
void ky_event_listen(kyEventSystem *system, kyEventType type, 
                    kyEventCallback callback, void *user_data);

// 移除监听
void ky_event_unlisten(kyEventSystem *system, kyEventCallback callback, void *user_data);

// 触发事件
void ky_event_trigger(kyEventSystem *system, const kyEvent *event);
```

### 使用示例

```c
// 输入事件处理
void on_key_press(const kyEvent *event, void *user_data) {
    GameContext *ctx = (GameContext*)user_data;
    
    switch (event->data.key.key) {
        case KY_KEY_ESCAPE:
            ctx->should_quit = 1;
            break;
        case KY_KEY_SPACE:
            // 玩家跳跃
            if (ctx->player.is_grounded) {
                ctx->player.velocity.y = ctx->player.jump_force;
            }
            break;
        case KY_KEY_LEFT:
            ctx->player.moving_left = 1;
            break;
        case KY_KEY_RIGHT:
            ctx->player.moving_right = 1;
            break;
    }
}

void setup_input_events(kyEventSystem *event_system, GameContext *ctx) {
    // 监听键盘事件
    ky_event_listen(event_system, KY_EVENT_KEY_PRESS, on_key_press, ctx);
    ky_event_listen(event_system, KY_EVENT_KEY_RELEASE, on_key_release, ctx);
    
    // 监听鼠标事件
    ky_event_listen(event_system, KY_EVENT_MOUSE_MOVE, on_mouse_move, ctx);
    ky_event_listen(event_system, KY_EVENT_MOUSE_BUTTON, on_mouse_button, ctx);
}

// 游戏事件处理
void on_collision(const kyEvent *event, void *user_data) {
    GameContext *ctx = (GameContext*)user_data;
    uint32_t entity_a = event->entity_id & 0xFFFF;
    uint32_t entity_b = (event->entity_id >> 16) & 0xFFFF;
    
    KY_LOG_DEBUG("Collision detected: entity %d <-> entity %d", entity_a, entity_b);
    
    // 特定碰撞处理
    if (entity_a == ctx->player.entity_id) {
        // 玩家碰撞处理
        process_player_collision(entity_b);
    } else if (entity_b == ctx->player.entity_id) {
        process_player_collision(entity_a);
    }
}

void setup_game_events(kyEventSystem *event_system, GameContext *ctx) {
    // 监听碰撞事件
    ky_event_listen(event_system, KY_EVENT_COLLISION, on_collision, ctx);
    
    // 自定义事件
    ky_event_listen(event_system, KY_EVENT_CUSTOM, on_custom_event, ctx);
}

// 触发自定义事件
void trigger_level_complete_event(kyEventSystem *system, int level_num) {
    kyEvent event = {
        .type = KY_EVENT_CUSTOM,
        .data.custom.data = &level_num,
        .data.custom.size = sizeof(level_num)
    };
    ky_event_trigger(system, &event);
}
```

## 性能优化建议

### 1. 内存池优化

```c
// 为频繁分配的对象使用内存池
typedef struct GameObject {
    kyEntity entity;
    kyVec3 position;
    kyVec3 velocity;
    int health;
    // ... 其他字段
} GameObject;

// 使用对象池管理游戏对象
typedef struct GameObjectPool {
    kyArray active_objects;
    kyArray free_objects;
    GameObject *pool;
    size_t pool_size;
} GameObjectPool;

GameObjectPool *create_game_object_pool(size_t capacity) {
    GameObjectPool *pool = ky_alloc(&ky_default_allocator, sizeof(GameObjectPool));
    pool->pool = ky_alloc(&ky_default_allocator, sizeof(GameObject) * capacity);
    pool->pool_size = capacity;
    
    // 初始化空闲对象列表
    for (size_t i = 0; i < capacity; i++) {
        ky_array_push(&pool->free_objects, &pool->pool[i]);
    }
    
    return pool;
}
```

### 2. 数学运算优化

```c
// 使用 SIMD 优化的数学运算
#if defined(__SSE__)
#include <xmmintrin.h>

// SIMD 优化的向量加法
static inline kyVec3 ky_vec3_add_simd(const kyVec3 *a, const kyVec3 *b) {
    __m128 va = _mm_load_ps((const float*)a);
    __m128 vb = _mm_load_ps((const float*)b);
    __m128 vr = _mm_add_ps(va, vb);
    
    kyVec3 result;
    _mm_store_ps((float*)&result, vr);
    return result;
}
#endif

// 编译时常量数学运算
#define KY_PI_F 3.1415926535f
#define KY_ONE_OVER_PI_F (1.0f / KY_PI_F)

// 使用快速倒数平方根
static inline float fast_rsqrt(float x) {
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);
    return x;
}
```

### 3. 缓存优化

```c
// 数据结构对齐到缓存行
typedef struct __attribute__((aligned(64))) CacheAlignedArray {
    void *data;
    size_t count;
    size_t capacity;
    size_t item_size;
} CacheAlignedArray;

// 批量处理数据
void batch_process_transforms(CacheAlignedArray *transforms, size_t count) {
    // 确保数据在内存中连续，CPU 缓存友好
    for (size_t i = 0; i < count; i++) {
        Transform *t = &((Transform*)transforms->data)[i];
        // ... 处理变换 ...
    }
}
```

### 4. 预计算和查找表

```c
// 预计算三角函数查找表
#define TRIG_TABLE_SIZE 360
static float sin_table[TRIG_TABLE_SIZE];
static float cos_table[TRIG_TABLE_SIZE];

void init_trig_tables(void) {
    for (int i = 0; i < TRIG_TABLE_SIZE; i++) {
        float angle = (float)i * KY_DEG2RAD;
        sin_table[i] = sinf(angle);
        cos_table[i] = cosf(angle);
    }
}

// 使用查找表的快速三角函数
float fast_sin_deg(int degrees) {
    degrees = degrees % 360;
    if (degrees < 0) degrees += 360;
    return sin_table[degrees];
}
```

## 调试工具

### 内存调试

```c
// 内存跟踪器
typedef struct kyMemoryTracker {
    struct {
        void *ptr;
        size_t size;
        const char *file;
        int line;
        const char *function;
    } *allocations;
    size_t allocation_count;
    size_t total_allocated;
} kyMemoryTracker;

void *tracked_alloc(void *ctx, size_t size, const char *file, int line, const char *function) {
    kyMemoryTracker *tracker = (kyMemoryTracker*)ctx;
    
    // 记录分配
    tracker->allocations[tracker->allocation_count].ptr = malloc(size);
    tracker->allocations[tracker->allocation_count].size = size;
    tracker->allocations[tracker->allocation_count].file = file;
    tracker->allocations[tracker->allocation_count].line = line;
    tracker->allocations[tracker->allocation_count].function = function;
    
    tracker->total_allocated += size;
    tracker->allocation_count++;
    
    return tracker->allocations[tracker->allocation_count - 1].ptr;
}

void print_memory_leaks(kyMemoryTracker *tracker) {
    printf("Memory leaks detected:\n");
    for (size_t i = 0; i < tracker->allocation_count; i++) {
        printf("  %p: %zu bytes (%s:%d in %s)\n",
               tracker->allocations[i].ptr,
               tracker->allocations[i].size,
               tracker->allocations[i].file,
               tracker->allocations[i].line,
               tracker->allocations[i].function);
    }
}
```

### 性能分析器

```c
typedef struct kyProfileSection {
    const char *name;
    kyTimer timer;
    int depth;
} kyProfileSection;

#define KY_PROFILE_BEGIN(name) ky_profile_begin(name)
#define KY_PROFILE_END() ky_profile_end()

void ky_profile_begin(const char *name) {
    static kyProfileSection sections[32];
    static int depth = 0;
    
    if (depth < 32) {
        sections[depth].name = name;
        ky_timer_start(&sections[depth].timer);
        sections[depth].depth = depth;
        depth++;
    }
}

void ky_profile_end(void) {
    static kyProfileSection sections[32];
    static int depth = 0;
    
    if (depth > 0) {
        depth--;
        ky_timer_stop(&sections[depth].timer);
        float time = ky_timer_elapsed_ms(&sections[depth].timer);
        
        for (int i = 0; i < sections[depth].depth; i++) {
            printf("  ");
        }
        printf("%s: %.3f ms\n", sections[depth].name, time);
    }
}
```

---

*Core 模块是整个引擎的基础，提供了数学、内存、数据结构等核心功能。合理使用这些工具可以显著提升开发效率和程序性能。*