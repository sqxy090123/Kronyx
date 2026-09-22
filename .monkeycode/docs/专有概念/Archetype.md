# Archetype 存储模式

Archetype 是 Kronyx ECS 系统的核心设计概念，它基于 Unity 的 Archetype ECS 模式，提供了一种高效的实体组件存储和访问方式。这种模式在现代游戏引擎中被广泛采用，能够显著提升性能并减少内存碎片。

## 概述

### Archetype 定义

Archetype 是组件类型的唯一组合集合。每个实体都属于一个特定的 archetype，该 archetypes 由实体拥有的组件类型决定。例如：

- Archetype 1: 只包含 `Transform` 组件
- Archetype 2: 包含 `Transform` + `Sprite` 组件  
- Archetype 3: 包含 `Transform` + `Sprite` + `RigidBody` 组件

### 核心优势

- **缓存友好性**: 同类型组件连续存储，最大化缓存命中率
- **内存效率**: 只存储实际存在的组件，避免浪费空间
- **访问速度**: O(1) 复杂度的实体到组件映射
- **批量处理**: 相同 archetype 的实体可以高效批量处理

## Archetype 数据结构

### Archetype 结构体

```c
typedef struct kyArchetype {
    uint32_t *types;             // 组件类型 ID 数组
    uint32_t type_count;         // 组件类型数量
    size_t count;                // 当前实体数量
    size_t capacity;             // 分配的容量
    void **columns;              // 组件列数据（SoA）
    size_t *strides;            // 每个组件的步长
    uint32_t *entity_ids;       // 实体 ID 数组
    uint64_t type_hash;         // 组件类型的哈希值（用于快速查找）
} kyArchetype;
```

### 组件列布局

Archetype 使用结构化数组 (Structure of Arrays, SoA) 布局存储组件数据：

```c
// 示例：Transform + Sprite archetype
struct TransformSpriteArchetype {
    uint32_t entity_ids[1024];       // 实体 ID
    
    // Transform 组件（SoA）
    float transform_positions_x[1024];  // X 位置
    float transform_positions_y[1024];  // Y 位置
    float transform_positions_z[1024];  // Z 位置
    float transform_rotations[1024];    // 旋转
    float transform_scales_x[1024];     // X 缩放
    float transform_scales_y[1024];     // Y 缩放
    
    // Sprite 组件（SoA）
    uint32_t sprite_texture_ids[1024];  // 纹理 ID
    float sprite_colors_r[1024];        // 红色通道
    float sprite_colors_g[1024];        // 绿色通道
    float sprite_colors_b[1024];        // 蓝色通道
    float sprite_colors_a[1024];        // 透明度通道
    float sprite_sizes_x[1024];         // X 尺寸
    float sprite_sizes_y[1024];         // Y 尺寸
    
    size_t count;                      // 当前实体数量
};
```

## Archetype 管理

### 1. Archetype 创建

```c
// 创建新 archetype
kyArchetype *archetype_create(const uint32_t *types, uint32_t type_count, 
                              size_t initial_capacity, kyAllocator *alloc) {
    kyArchetype *arch = ky_alloc(sizeof(kyArchetype));
    
    // 初始化组件类型
    arch->types = ky_alloc(type_count * sizeof(uint32_t));
    memcpy(arch->types, types, type_count * sizeof(uint32_t));
    arch->type_count = type_count;
    
    // 计算类型哈希
    arch->type_hash = 0;
    for (int i = 0; i < type_count; i++) {
        arch->type_hash = arch->type_hash * 31 + types[i];
    }
    
    // 分配数据空间
    arch->count = 0;
    arch->capacity = initial_capacity;
    
    // 分配列数据
    arch->columns = ky_alloc(type_count * sizeof(void*));
    arch->strides = ky_alloc(type_count * sizeof(size_t));
    arch->entity_ids = ky_alloc(initial_capacity * sizeof(uint32_t));
    
    // 初始化每个组件列
    for (int i = 0; i < type_count; i++) {
        kyComponentType *ct = ky_world_component_type(world, types[i]);
        size_t elem_size = ct->size;
        
        arch->columns[i] = ky_alloc(initial_capacity * elem_size);
        arch->strides[i] = elem_size;
        
        // 初始化组件数据
        void *data = arch->columns[i];
        for (size_t j = 0; j < initial_capacity; j++) {
            if (ct->ctor) {
                ct->ctor((char*)data + j * elem_size);
            }
        }
    }
    
    return arch;
}
```

### 2. Archetype 查找

```c
// 查找或创建 archetype
kyArchetype *archetype_find_or_create(kyWorld *w, const uint32_t *types, 
                                     uint32_t type_count) {
    uint64_t hash = 0;
    for (int i = 0; i < type_count; i++) {
        hash = hash * 31 + types[i];
    }
    
    // 查找现有 archetype
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = (kyArchetype*)w->archetypes.data + i;
        if (arch->type_hash == hash && arch->type_count == type_count) {
            // 验证类型顺序是否相同
            int match = 1;
            for (int j = 0; j < type_count; j++) {
                if (arch->types[j] != types[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                return arch;
            }
        }
    }
    
    // 创建新 archetype
    size_t initial_capacity = KY_ARCHETYPE_INITIAL_CAPACITY;
    kyArchetype *new_arch = archetype_create(types, type_count, initial_capacity, &w->alloc);
    ky_array_push(&w->archetypes, &new_arch);
    
    return new_arch;
}
```

### 3. Archetype 扩容

```c
// 扩展 archetype 容量
void archetype_grow(kyArchetype *arch, size_t new_capacity) {
    // 重新分配实体 ID 数组
    arch->entity_ids = ky_realloc(arch->entity_ids, new_capacity * sizeof(uint32_t));
    
    // 扩展每个组件列
    for (int i = 0; i < arch->type_count; i++) {
        size_t elem_size = arch->strides[i];
        void *old_data = arch->columns[i];
        void *new_data = ky_realloc(old_data, new_capacity * elem_size);
        arch->columns[i] = new_data;
        
        // 初始化新空间
        if (kyComponentType *ct = ky_world_component_type(world, arch->types[i])) {
            if (ct->ctor) {
                char *base = (char*)new_data;
                size_t old_count = arch->count;
                for (size_t j = old_count; j < new_capacity; j++) {
                    ct->ctor(base + j * elem_size);
                }
            }
        }
    }
    
    arch->capacity = new_capacity;
}
```

## 实体迁移

### 迁移流程

当实体需要添加或删除组件时，会发生 archetype 迁移：

1. **源 archetype**: 实体当前所在的 archetype
2. **目标 archetype**: 包含新组件组合的 archetype
3. **数据迁移**: 将现有组件数据复制到目标 archetype
4. **清理**: 在源 archetype 中删除实体并调用析构函数

### 实现示例

```c
// 实体迁移的核心逻辑
void archetype_move_entity(kyWorld *w, kyEntity e, uint32_t target_type_count, 
                          const uint32_t *target_types) {
    // 1. 找到源 archetype 和实体位置
    kyEntitySlot *slot = ky_array_get(&w->slots, e.id);
    kyArchetype *src_arch = (kyArchetype*)w->archetypes.data + slot->archetype_index;
    
    // 2. 找到或创建目标 archetype
    kyArchetype *dst_arch = archetype_find_or_create(w, target_types, target_type_count);
    
    // 3. 在目标 archetype 中找到位置
    size_t dst_row;
    if (dst_arch->count >= dst_arch->capacity) {
        archetype_grow(dst_arch, dst_arch->capacity * 2);
    }
    dst_row = dst_arch->count++;
    
    // 4. 复制实体 ID
    dst_arch->entity_ids[dst_row] = e.id;
    
    // 5. 复制现有组件数据
    for (int i = 0; i < src_arch->type_count; i++) {
        uint32_t src_type = src_arch->types[i];
        size_t src_elem_size = src_arch->strides[i];
        void *src_data = (char*)src_arch->columns[i] + slot->row * src_elem_size;
        
        // 在目标 archetype 中找到对应组件列
        int dst_col = -1;
        for (int j = 0; j < dst_arch->type_count; j++) {
            if (dst_arch->types[j] == src_type) {
                dst_col = j;
                break;
            }
        }
        
        if (dst_col >= 0) {
            // 复制组件数据
            void *dst_data = (char*)dst_arch->columns[dst_col] + dst_row * dst_arch->strides[dst_col];
            memcpy(dst_data, src_data, src_elem_size);
        }
    }
    
    // 6. 创建新组件（在目标 archetype 中但不在源 archetype 中的）
    for (int i = 0; i < dst_arch->type_count; i++) {
        uint32_t dst_type = dst_arch->types[i];
        int found = 0;
        
        // 检查是否在源 archetype 中存在
        for (int j = 0; j < src_arch->type_count; j++) {
            if (src_arch->types[j] == dst_type) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            // 调用构造函数
            void *dst_data = (char*)dst_arch->columns[i] + dst_row * dst_arch->strides[i];
            kyComponentType *ct = ky_world_component_type(w, dst_type);
            if (ct->ctor) {
                ct->ctor(dst_data);
            }
        }
    }
    
    // 7. 在源 archetype 中删除实体
    archetype_remove_entity(src_arch, slot->row);
    
    // 8. 更新实体的槽位信息
    slot->archetype_index = dst_arch - (kyArchetype*)w->archetypes.data;
    slot->row = dst_row;
}

// 从 archetype 中删除实体
void archetype_remove_entity(kyArchetype *arch, size_t row) {
    if (row >= arch->count) return;
    
    // 调用析构函数
    for (int i = 0; i < arch->type_count; i++) {
        void *data = (char*)arch->columns[i] + row * arch->strides[i];
        kyComponentType *ct = ky_world_component_type(world, arch->types[i]);
        if (ct->dtor) {
            ct->dtor(data);
        }
    }
    
    // 如果不是最后一行，需要移动数据
    if (row < arch->count - 1) {
        size_t elem_size = sizeof(uint32_t);
        memmove(arch->entity_ids + row, arch->entity_ids + row + 1, 
               (arch->count - row - 1) * elem_size);
        
        for (int i = 0; i < arch->type_count; i++) {
            elem_size = arch->strides[i];
            memmove((char*)arch->columns[i] + row * elem_size,
                   (char*)arch->columns[i] + (row + 1) * elem_size,
                   (arch->count - row - 1) * elem_size);
        }
    }
    
    arch->count--;
}
```

## 性能优化

### 1. 内存预分配

```c
// Archetype 初始容量配置
#define KY_ARCHETYPE_INITIAL_CAPACITY 64
#define KY_ARCHETYPE_GROW_FACTOR 2

// 批量操作优化
void archetype_reserve(kyArchetype *arch, size_t new_capacity) {
    if (new_capacity <= arch->capacity) return;
    
    // 直接扩容到所需大小，避免多次扩容
    while (arch->capacity < new_capacity) {
        archetype_grow(arch, arch->capacity * KY_ARCHETYPE_GROW_FACTOR);
    }
}
```

### 2. 缓存优化

```c
// 考虑缓存行大小的数据布局
#define KY_CACHE_LINE_SIZE 64

typedef struct CacheAlignedComponent {
    float data[KY_CACHE_LINE_SIZE / sizeof(float)];
} CacheAlignedComponent;

// 确保 archetype 数据按缓存行对齐
void archetype_optimize_layout(kyArchetype *arch) {
    for (int i = 0; i < arch->type_count; i++) {
        size_t elem_size = arch->strides[i];
        size_t alignment = KY_CACHE_LINE_SIZE;
        
        if (elem_size % alignment != 0) {
            // 调整步长到缓存行大小
            arch->strides[i] = ((elem_size / alignment) + 1) * alignment;
        }
    }
}
```

### 3. 批量操作

```c
// 批量添加实体到 archetype
void archetype_bulk_add(kyArchetype *arch, const uint32_t *entity_ids, 
                       size_t count, void **component_data) {
    if (arch->count + count > arch->capacity) {
        archetype_grow(arch, arch->count + count);
    }
    
    // 批量添加实体 ID
    memcpy(arch->entity_ids + arch->count, entity_ids, count * sizeof(uint32_t));
    
    // 批量添加组件数据
    for (int i = 0; i < arch->type_count; i++) {
        size_t elem_size = arch->strides[i];
        void *src = component_data[i];
        void *dst = (char*)arch->columns[i] + arch->count * elem_size;
        memcpy(dst, src, count * elem_size);
    }
    
    arch->count += count;
}
```

## 调试和分析

### 1. Archetype 统计

```c
void archetype_debug_stats(kyWorld *w) {
    printf("=== Archetype Statistics ===\n");
    
    size_t total_entities = 0;
    size_t total_memory = 0;
    
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = (kyArchetype*)w->archetypes.data + i;
        
        size_t archetype_memory = 0;
        archetype_memory += arch->capacity * sizeof(uint32_t); // entity_ids
        archetype_memory += arch->capacity * sizeof(size_t);   // strides
        archetype_memory += arch->capacity * sizeof(void*);   // columns
        
        for (int j = 0; j < arch->type_count; j++) {
            archetype_memory += arch->capacity * arch->strides[j];
        }
        
        total_entities += arch->count;
        total_memory += archetype_memory;
        
        printf("Archetype %d (%zu entities):\n", i, arch->count);
        printf("  Types: ");
        for (int j = 0; j < arch->type_count; j++) {
            printf("%u ", arch->types[j]);
        }
        printf("\n");
        printf("  Memory: %.2f KB\n", (float)archetype_memory / 1024);
        printf("  Capacity: %zu\n", arch->capacity);
        printf("\n");
    }
    
    printf("Total: %zu entities, %.2f KB memory\n", 
           total_entities, (float)total_memory / 1024);
}
```

### 2. 内存分析

```c
void archetype_memory_analysis(kyWorld *w) {
    printf("=== Memory Analysis ===\n");
    
    kyHashMap archetype_sizes;
    ky_hashmap_create(&archetype_sizes, sizeof(uint32_t), sizeof(size_t), NULL);
    
    // 统计各 archetype 的内存使用
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = (kyArchetype*)w->archetypes.data + i;
        
        size_t archetype_size = 0;
        for (int j = 0; j < arch->type_count; j++) {
            archetype_size += arch->capacity * arch->strides[j];
        }
        
        uint32_t key = arch->type_hash;
        void *value = ky_hashmap_get(&archetype_sizes, &key);
        if (value) {
            size_t *existing = (size_t*)value;
            *existing += archetype_size;
        } else {
            ky_hashmap_set(&archetype_sizes, &key, &archetype_size);
        }
    }
    
    // 输出统计结果
    kyHashMapIterator it;
    ky_hashmap_iter_begin(&archetype_sizes, &it);
    while (ky_hashmap_iter_next(&it)) {
        uint32_t type_hash = *(uint32_t*)it.key;
        size_t total_size = *(size_t*)it.value;
        
        printf("Archetype 0x%08x: %.2f KB\n", type_hash, (float)total_size / 1024);
    }
    
    ky_hashmap_destroy(&archetype_sizes);
}
```

## 实际应用案例

### 1. 游戏对象类型管理

```c
// 定义游戏对象 archetype
enum {
    ARCHETYPE_NULL = 0,
    ARCHETYPE_TRANSFORM,              // 只有 Transform
    ARCHETYPE_TRANSFORM_SPRITE,       // Transform + Sprite
    ARCHETYPE_TRANSFORM_SPRITE_BODY,  // Transform + Sprite + RigidBody
    ARCHETYPE_TRANSFORM_SPRITE_BODY_AUDIO, // 完整对象
    ARCHETYPE_CAMERA,                 // 相机组件
    ARCHETYPE_LIGHT,                  // 光照组件
    ARCHETYPE_COUNT
};

// 预定义组件类型组合
static uint32_t transform_archetype[] = {TID_TRANSFORM, 0};
static uint32_t transform_sprite_archetype[] = {TID_TRANSFORM, TID_SPRITE, 0};
static uint32_t transform_sprite_body_archetype[] = {TID_TRANSFORM, TID_SPRITE, TID_RIGID_BODY, 0};

// 创建游戏对象
kyEntity create_game_object(kyWorld *w, GameObjectType type) {
    switch (type) {
        case GAME_OBJECT_PLAYER:
            return archetype_spawn_entity(w, transform_sprite_body_archetype);
        case GAME_OBJECT_ENEMY:
            return archetype_spawn_entity(w, transform_sprite_body_archetype);
        case GAME_OBJECT_PROJECTILE:
            return archetype_spawn_entity(w, transform_sprite_archetype);
        case GAME_OBJECT_PARTICLE:
            return archetype_spawn_entity(w, transform_archetype);
        default:
            return ky_entity_null;
    }
}
```

### 2. 批量对象创建

```c
// 批量创建粒子系统
void create_particle_system(kyWorld *w, int count, kyVec3 position) {
    kyEntity entities[KY_MAX_BATCH_SIZE];
    void *component_data[TID_COUNT] = {0};
    
    // 准备组件数据
    Transform *transforms = ky_alloc(count * sizeof(Transform));
    Sprite *sprites = ky_alloc(count * sizeof(Sprite));
    
    for (int i = 0; i < count; i++) {
        // 随机偏移
        float angle = (float)i / count * 2.0f * KY_PI;
        float radius = 1.0f;
        transforms[i].pos = ky_vec3(
            position.x + cos(angle) * radius,
            position.y + sin(angle) * radius,
            position.z
        );
        
        sprites[i].color = (kyVec4){
            (float)rand() / RAND_MAX,
            (float)rand() / RAND_MAX,
            (float)rand() / RAND_MAX,
            1.0f
        };
    }
    
    component_data[TID_TRANSFORM] = transforms;
    component_data[TID_SPRITE] = sprites;
    
    // 批量创建
    kyArchetype *arch = archetype_find_or_create(w, transform_sprite_archetype, 2);
    archetype_bulk_add(arch, (uint32_t*)entities, count, component_data);
    
    // 保存实体引用以便后续更新
    for (int i = 0; i < count; i++) {
        entities[i] = arch->entity_ids[arch->count - count + i];
    }
}
```

### 3. 性能关键路径优化

```c
// 性能关键：只遍历需要的 archetype
void render_visible_objects(kyWorld *w, const Frustum *frustum) {
    // 只遍历需要渲染的 archetype
    uint32_t render_types[] = {TID_TRANSFORM, TID_SPRITE, TID_CAMERA};
    kyArchetype *render_arch = archetype_find_or_create(w, render_types, 3);
    
    // 批量处理可见对象
    size_t visible_count = 0;
    for (size_t i = 0; i < render_arch->count; i++) {
        kyEntity entity = render_arch->entity_ids[i];
        Transform *t = (Transform*)((char*)render_arch->columns[0] + i * sizeof(Transform));
        
        if (frustum_contains_point(frustum, t->pos)) {
            // 批量收集可见对象
            visible_objects[visible_count++] = entity;
        }
    }
    
    // 批量渲染
    render_batch(visible_objects, visible_count);
}
```

## 最佳实践

### 1. Archetype 设计原则

```c
// 1. 减少组件迁移 - 合理设计组件组合
typedef struct WellDesignedComponents {
    // 高频更新组件放在一起
    Transform transform;      // 每帧更新
    Velocity velocity;        // 每帧更新
    Sprite sprite;           // 每帧渲染
    
    // 低频更新组件分开
    Stats stats;             // 每秒更新
    Inventory inventory;     // 玩家交互时更新
    Equipment equipment;     // 装备变更时更新
} WellDesignedComponents;

// 2. 避免过大的组件
// 不好的设计：包含所有可能数据的巨大组件
typedef struct BadHugeComponent {
    Transform transform;
    Sprite sprite;
    RigidBody rigidbody;
    Audio audio;
    Animation animation;
    ParticleSystem particles;
    // ... 更多字段
} BadHugeComponent;

// 好的设计：分离相关组件
typedef struct PhysicsComponents {
    Transform transform;
    RigidBody rigidbody;
    Collider collider;
} PhysicsComponents;

typedef struct RenderComponents {
    Transform transform;
    Sprite sprite;
    Animation animation;
} RenderComponents;
```

### 2. 内存管理

```c
// 1. 预估容量，减少扩容
void optimize_capacity(kyWorld *w) {
    // 基于游戏类型预估需要的容量
    const size_t initial_capacity = 1000;  // 基础容量
    
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = (kyArchetype*)w->archetypes.data + i;
        
        // 根据 archetype 类型设置不同的初始容量
        size_t estimated_capacity = initial_capacity;
        if (arch->type_count >= 3) {
            estimated_capacity *= 2;  // 复杂 archetype 需要更多空间
        }
        
        archetype_reserve(arch, estimated_capacity);
    }
}

// 2. 内存池管理
typedef struct ArchetypeMemoryPool {
    void *memory_blocks[10];
    size_t block_sizes[10];
    int block_count;
} ArchetypeMemoryPool;

void archetype_pool_free(ArchetypeMemoryPool *pool, void *ptr) {
    // 实现内存池回收逻辑
}
```

### 3. 性能监控

```c
// Archetype 性能监控
typedef struct ArchetypeMetrics {
    uint64_t migration_count;
    uint64_t migration_time;
    uint64_t query_count;
    uint64_t query_time;
    uint64_t batch_count;
    uint64_t batch_time;
} ArchetypeMetrics;

void archetype_performance_metrics(kyWorld *w) {
    static ArchetypeMetrics metrics = {0};
    
    // 定期输出性能统计
    if (metrics.migration_count % 100 == 0) {
        printf("Archetype Performance:\n");
        printf("  Migrations: %lu (%.2f ms avg)\n", 
               metrics.migration_count, 
               metrics.migration_time / (double)metrics.migration_count);
        printf("  Queries: %lu (%.2f ms avg)\n",
               metrics.query_count,
               metrics.query_time / (double)metrics.query_count);
        printf("  Batches: %lu (%.2f ms avg)\n",
               metrics.batch_count,
               metrics.batch_time / (double)metrics.batch_count);
    }
}
```

## 总结

Archetype 存储模式是 Kronyx ECS 系统的核心优势所在，通过合理的设计可以带来显著的性能提升：

### 关键优势

1. **缓存友好**: 连续存储的数据结构最大化 CPU 缓存利用率
2. **内存效率**: 只存储实际使用的组件，避免内存浪费
3. **访问速度**: O(1) 复杂度的实体到组件映射
4. **批量处理**: 相同 archetype 的实体可以高效批量处理

### 设计原则

1. **组件组合优化**: 减少 archetype 迁移，将相关组件放在一起
2. **内存预分配**: 合理预估容量，减少运行时扩容开销
3. **批量操作**: 优先使用批量操作而不是单个实体操作
4. **监控分析**: 持续监控 archetype 性能，识别优化机会

### 应用建议

- 根据游戏类型设计合适的组件组合
- 为高频更新的 archetype 分配更多内存空间
- 定期分析 archetype 使用情况，优化设计
- 在性能关键路径中使用批量操作

通过深入理解和使用 Archetype 模式，开发者可以构建高性能、可扩展的游戏系统。