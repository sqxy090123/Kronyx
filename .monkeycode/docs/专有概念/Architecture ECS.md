# Archetype ECS 系统

## 概述

Kronyx Engine 采用基于 Archetype 的实体组件系统（ECS），这是一种数据导向的存储架构，特别适合游戏开发。与传统的组件-实体系统不同，Architecture ECS 将具有相同组件集合的实体分组存储，提供优秀的缓存友好性和性能。

## 核心概念

### Archetype

Archetype 是一个组件类型的集合，所有具有相同组件集合的实体属于同一个 Archetype。每个 Archetype 存储为结构化的数据（SoA - Structure of Arrays）。

```c
typedef struct kyArchetype {
    uint32_t *types;          // 组件类型数组
    uint32_t type_count;      // 组件类型数量
    size_t count;             // 当前实体数量
    size_t capacity;          // 容量
    void **columns;           // 组件列（SoA）
    size_t *strides;          // 每个类型的步长
    uint32_t *entity_ids;     // 实体ID数组
    uint64_t type_hash;       // 类型哈希（快速查找）
} kyArchetype;
```

### 实体槽位

每个实体维护一个版本化的槽位信息：

```c
typedef struct kyEntitySlot {
    uint32_t version;         // 版本号（实体销毁后递增）
    int32_t archetype_index;  // 所属 Archetype 索引
    uint32_t row;            // 在 Archetype 中的行号
} kyEntitySlot;
```

## 设计原理

### 缓存友好性

Architecture ECS 的核心优势在于缓存友好性：

1. **连续内存布局**：相同类型的组件数据在内存中连续存储
2. **批量处理**：可以高效地对大量实体进行相同操作
3. **内存局部性**：访问组件时充分利用 CPU 缓存

```c
// 高效的批量组件访问示例
void batch_transform_system(kyWorld *w, float dt) {
    uint32_t tid_transform = ky_world_component_type_by_name(w, "transform");
    
    // 遍历所有包含 Transform 的 Archetype
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = &((kyArchetype*)w->archetypes.data)[i];
        
        // 检查 Archetype 是否包含 Transform 组件
        int has_transform = 0;
        for (int j = 0; j < arch->type_count; j++) {
            if (arch->types[j] == tid_transform) {
                has_transform = 1;
                break;
            }
        }
        
        if (has_transform) {
            // 获取 Transform 列
            Transform *transforms = arch->columns[j];
            
            // 批量处理所有实体
            for (size_t k = 0; k < arch->count; k++) {
                Transform *t = &transforms[k];
                t->position.x += t->velocity.x * dt;
                t->position.y += t->velocity.y * dt;
            }
        }
    }
}
```

### 实体迁移

当实体添加或移除组件时，需要在 Archetype 之间迁移：

```c
// 实体迁移过程
int ky_entity_move_to_archetype(kyWorld *w, kyEntity e, uint32_t *target_types, uint32_t target_count) {
    // 1. 找到源 Archetype
    kyEntitySlot *slot = &((kyEntitySlot*)w->slots.data)[e.id];
    kyArchetype *src_arch = &((kyArchetype*)w->archetypes.data)[slot->archetype_index];
    
    // 2. 找到目标 Archetype
    kyArchetype *dst_arch = ky_archetype_find_or_create(w, target_types, target_count);
    
    // 3. 迁移实体数据
    size_t src_row = slot->row;
    size_t dst_row = dst_arch->count;
    
    // 4. 复制共享组件
    for (int i = 0; i < src_arch->type_count; i++) {
        uint32_t type_id = src_arch->types[i];
        if (ky_type_in_array(target_types, target_count, type_id)) {
            // 复制组件数据
            void *src_comp = src_arch->columns[i] + src_row * src_arch->strides[i];
            void *dst_comp = dst_arch->columns[i] + dst_row * dst_arch->strides[i];
            memcpy(dst_comp, src_comp, src_arch->strides[i]);
        }
    }
    
    // 5. 初始化新组件
    for (int i = 0; i < dst_arch->type_count; i++) {
        uint32_t type_id = dst_arch->types[i];
        if (!ky_type_in_array(src_arch->types, src_arch->type_count, type_id)) {
            void *new_comp = dst_arch->columns[i] + dst_row * dst_arch->strides[i];
            ky_world_invoke_component_ctor(w, type_id, new_comp);
        }
    }
    
    // 6. 更新槽位信息
    slot->archetype_index = ky_archetype_index(dst_arch);
    slot->row = dst_row;
    
    return 0;
}
```

## 性能特性

### 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| 实体创建 | O(1) | 从空闲ID池分配 |
| 组件添加 | O(log k) | 需要查找目标 Archetype |
| 组件移除 | O(log k) | 需要查找目标 Archetype |
| 组件访问 | O(1) | 直接通过 Archetype 和行列访问 |
| 视图迭代 | O(n + m) | n=实体数，m=Archetype 数 |

### 内存使用

```c
// 内存使用优化技巧
void optimize_memory_usage(kyWorld *w) {
    // 1. 定期压缩 Archetype
    ky_archetype_compact(w);
    
    // 2. 使用 Arena 分配器管理临时数据
    kyArena *temp_arena = ky_arena_create(&ky_default_allocator(), 1024 * 1024);
    
    // 3. 批量操作减少内存分配
    kyArray entities_to_process = ky_array_create(sizeof(kyEntity), temp_arena);
    
    // 4. 操作完成后释放 Arena
    ky_arena_destroy(temp_arena);
}
```

## 最佳实践

### 组件设计

```c
// 1. 轻量级组件
typedef struct Position {
    float x, y;
} Position;

// 2. 批处理友好组件
typedef struct Transform {
    float matrix[16];  // 使用矩阵而非单独字段
} Transform;

// 3. 组合组件（减少组件数量）
typedef struct PhysicsBody {
    struct {
        float x, y;
    } position;
    struct {
        float vx, vy;
    } velocity;
    float mass;
    float friction;
} PhysicsBody;
```

### 系统实现

```c
// 高效的系统实现
void rendering_system(kyWorld *w, float dt) {
    // 1. 按图层分组
    typedef struct RenderBatch {
        int layer;
        kyTexture *texture;
        kyArray sprites;
    } RenderBatch;
    
    kyArray batches = ky_array_create(sizeof(RenderBatch), &ky_default_allocator());
    
    // 2. 收集精灵数据
    uint32_t tid_sprite = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_transform = ky_world_component_type_by_name(w, "transform");
    
    // 3. 按视图遍历（性能最优）
    uint32_t types[] = {tid_transform, tid_sprite};
    kyViewIter it;
    if (ky_view_begin(w, types, 2, &it)) {
        while (ky_view_next(&it)) {
            Transform *t = ky_world_get_component(w, it.current, tid_transform);
            Sprite *s = ky_world_get_component(w, it.current, tid_sprite);
            
            // 添加到批次
            add_to_batch(&batches, s->layer, s->texture, t, s);
        }
    }
    
    // 4. 批量渲染
    for (int i = 0; i < batches.count; i++) {
        RenderBatch *batch = &((RenderBatch*)batches.data)[i];
        render_batch(batch);
    }
    
    ky_array_destroy(&batches);
}
```

### 性能监控

```c
// ECS 性能监控
void monitor_ecs_performance(kyWorld *w) {
    static clock_t last_time = 0;
    clock_t current_time = clock();
    
    if (last_time > 0) {
        double elapsed = ((double)(current_time - last_time)) / CLOCKS_PER_SEC;
        
        printf("ECS Stats:\n");
        printf("  Total entities: %d\n", ky_world_alive_count(w));
        printf("  Archetypes: %zu\n", w->archetypes.count);
        printf("  Components registered: %zu\n", w->component_types.count);
        printf("  Systems registered: %zu\n", w->systems.count);
        printf("  Frame time: %.3f ms\n", elapsed * 1000);
        
        // 计算每个 Archetype 的内存使用
        size_t total_memory = 0;
        for (int i = 0; i < w->archetypes.count; i++) {
            kyArchetype *arch = &((kyArchetype*)w->archetypes.data)[i];
            size_t arch_memory = 0;
            
            for (int j = 0; j < arch->type_count; j++) {
                arch_memory += arch->capacity * arch->strides[j];
            }
            
            total_memory += arch_memory;
            printf("  Archetype %d: %zu entities, %zu bytes\n", 
                   i, arch->count, arch_memory);
        }
        
        printf("  Total memory: %zu KB\n", total_memory / 1024);
    }
    
    last_time = current_time;
}
```

## 调试工具

### 视图调试

```c
// 调试视图内容
void debug_view(kyWorld *w, uint32_t *types, int type_count) {
    kyViewIter it;
    printf("View with %d components:\n", type_count);
    
    for (int i = 0; i < type_count; i++) {
        const kyComponentType *ct = ky_world_component_type(w, types[i]);
        printf("  %s\n", ct->name);
    }
    
    if (ky_view_begin(w, types, type_count, &it)) {
        int count = 0;
        while (ky_view_next(&it)) {
            printf("  Entity %d:%d\n", it.current.id, it.current.version);
            count++;
        }
        printf("  Total: %d entities\n", count);
    } else {
        printf("  No entities found\n");
    }
}
```

### Archetype 分析

```c
// 分析 Archetype 结构
void analyze_archetypes(kyWorld *w) {
    printf("Archetype Analysis:\n");
    printf("==================\n");
    
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = &((kyArchetype*)w->archetypes.data)[i];
        
        printf("Archetype %d:\n", i);
        printf("  Components: ");
        for (int j = 0; j < arch->type_count; j++) {
            const kyComponentType *ct = ky_world_component_type(w, arch->types[j]);
            printf("%s", ct->name);
            if (j < arch->type_count - 1) printf(", ");
        }
        printf("\n");
        printf("  Entities: %zu/%zu\n", arch->count, arch->capacity);
        printf("  Memory: %zu KB\n", calculate_archetype_memory(arch) / 1024);
        
        // 检查效率
        float efficiency = (float)arch->count / arch->capacity;
        if (efficiency < 0.5f) {
            printf("  WARNING: Low capacity utilization (%.1f%%)\n", efficiency * 100);
        }
    }
}
```

## 常见陷阱与解决方案

### 陷阱1：频繁的实体迁移

**问题**：频繁添加/移除组件导致实体在 Archetype 间频繁迁移。

**解决方案**：
1. 设计稳定的组件集合
2. 使用临时组件标记而非直接移除
3. 批量处理组件变更

```c
// 使用标记组件而非直接移除
typedef struct TemporalComponent {
    int marked_for_removal;
    int removal_timer;
} TemporalComponent;

// 批量移除组件
void batch_removal_system(kyWorld *w) {
    uint32_t tid_temporal = ky_world_component_type_by_name(w, "temporal");
    
    // 第一遍：标记要移除的实体
    kyViewIter it;
    if (ky_view_begin(w, &tid_temporal, 1, &it)) {
        while (ky_view_next(&it)) {
            TemporalComponent *temp = ky_world_get_component(w, it.current, tid_temporal);
            temp->removal_timer--;
            if (temp->removal_timer <= 0) {
                temp->marked_for_removal = 1;
            }
        }
    }
    
    // 第二遍：实际移除
    if (ky_view_begin(w, &tid_temporal, 1, &it)) {
        kyArray entities_to_remove = ky_array_create(sizeof(kyEntity), &ky_default_allocator());
        
        while (ky_view_next(&it)) {
            TemporalComponent *temp = ky_world_get_component(w, it.current, tid_temporal);
            if (temp->marked_for_removal) {
                ky_array_push(&entities_to_remove, &it.current);
            }
        }
        
        // 批量移除
        for (int i = 0; i < entities_to_remove.count; i++) {
            kyEntity e = ((kyEntity*)entities_to_remove.data)[i];
            ky_world_despawn(w, e);
        }
        
        ky_array_destroy(&entities_to_remove);
    }
}
```

### 陷阱2：缓存不友好的访问模式

**问题**：随机访问实体导致缓存未命中。

**解决方案**：
1. 使用视图进行批量处理
2. 保持组件访问的局部性
3. 考虑数据布局优化

```c
// 不好的访问模式（缓存不友好）
void bad_access_pattern(kyWorld *w) {
    // 随机访问不同类型的实体
    for (int i = 0; i < 1000; i++) {
        kyEntity e = get_random_entity(w);
        Transform *t = ky_world_get_component(w, e, tid_transform);
        Sprite *s = ky_world_get_component(w, e, tid_sprite);
        Physics *p = ky_world_get_component(w, e, tid_physics);
        
        // 每次访问都可能涉及不同的 Archetype
        process_entity(t, s, p);
    }
}

// 好的访问模式（缓存友好）
void good_access_pattern(kyWorld *w) {
    // 按组件类型批量处理
    uint32_t transform_types[] = {tid_transform};
    process_transforms(w, transform_types, 1);
    
    uint32_t sprite_types[] = {tid_sprite};
    process_sprites(w, sprite_types, 1);
    
    uint32_t physics_types[] = {tid_physics};
    process_physics(w, physics_types, 1);
}
```

### 陷阱3：过度使用字符串查找

**问题**：频繁的组件名称查找导致性能下降。

**解决方案**：
1. 缓存组件类型ID
2. 使用枚举或预定义常量
3. 避免运行时字符串查找

```c
// 缓存组件类型ID
typedef struct SystemContext {
    kyWorld *world;
    uint32_t tid_transform;
    uint32_t tid_sprite;
    uint32_t tid_physics;
    // ... 其他组件类型ID
} SystemContext;

// 初始化系统上下文
void init_system_context(SystemContext *ctx, kyWorld *w) {
    ctx->world = w;
    ctx->tid_transform = ky_world_component_type_by_name(w, "transform");
    ctx->tid_sprite = ky_world_component_type_by_name(w, "sprite");
    ctx->tid_physics = ky_world_component_type_by_name(w, "physics");
}

// 使用缓存的类型ID
void update_system(SystemContext *ctx) {
    // 避免重复的字符串查找
    uint32_t types[] = {ctx->tid_transform, ctx->tid_sprite};
    kyViewIter it;
    
    if (ky_view_begin(ctx->world, types, 2, &it)) {
        while (ky_view_next(&it)) {
            // 处理实体...
        }
    }
}
```

## 扩展与定制

### 自定义 Archetype 策略

```c
// 自定义 Archetype 管理器
typedef struct CustomArchetypeManager {
    kyArray archetypes;
    kyHashMap archetype_cache;  // 类型哈希 -> Archetype 索引
    kyAllocator *alloc;
} CustomArchetypeManager;

// 自定义查找逻辑
kyArchetype *custom_find_archetype(CustomArchetypeManager *mgr, uint32_t *types, int count) {
    // 1. 计算类型哈希
    uint64_t hash = compute_type_hash(types, count);
    
    // 2. 检查缓存
    uint32_t *index = ky_hashmap_get(&mgr->archetype_cache, &hash);
    if (index) {
        return &((kyArchetype*)mgr->archetypes.data)[*index];
    }
    
    // 3. 创建新 Archetype
    return create_new_archetype(mgr, types, count);
}
```

### 数据导向优化

```c
// 数据导向的组件处理
void data_oriented_processing(kyWorld *w) {
    // 1. 收集所有相关数据到连续数组
    typedef struct ComponentBatch {
        float *positions;
        float *velocities;
        int *healths;
        int count;
    } ComponentBatch;
    
    ComponentBatch batch = {0};
    
    // 2. 批量收集数据
    uint32_t tid_position = ky_world_component_type_by_name(w, "position");
    uint32_t tid_velocity = ky_world_component_type_by_name(w, "velocity");
    uint32_t tid_health = ky_world_component_type_by_name(w, "health");
    
    uint32_t types[] = {tid_position, tid_velocity, tid_health};
    kyViewIter it;
    
    if (ky_view_begin(w, types, 3, &it)) {
        // 预分配
        batch.count = estimate_view_size(w, types, 3);
        batch.positions = malloc(batch.count * sizeof(float) * 3);
        batch.velocities = malloc(batch.count * sizeof(float) * 3);
        batch.healths = malloc(batch.count * sizeof(int));
        
        // 批量收集
        int idx = 0;
        while (ky_view_next(&it)) {
            Position *p = ky_world_get_component(w, it.current, tid_position);
            Velocity *v = ky_world_get_component(w, it.current, tid_velocity);
            Health *h = ky_world_get_component(w, it.current, tid_health);
            
            // 连续存储
            memcpy(&batch.positions[idx * 3], &p->x, sizeof(float) * 3);
            memcpy(&batch.velocities[idx * 3], &v->x, sizeof(float) * 3);
            batch.healths[idx] = h->current;
            
            idx++;
        }
    }
    
    // 3. 批量处理（SIMD 优化友好）
    for (int i = 0; i < batch.count; i++) {
        // 向量化的物理计算
        batch.positions[i * 3] += batch.velocities[i * 3] * dt;
        batch.positions[i * 3 + 1] += batch.velocities[i * 3 + 1] * dt;
        
        // 健康值处理
        if (batch.healths[i] <= 0) {
            // 标记删除
        }
    }
    
    // 4. 批量写回
    writeback_components(w, &batch);
    
    // 清理
    free(batch.positions);
    free(batch.velocities);
    free(batch.healths);
}
```

---

*Architecture ECS 是 Kronyx Engine 的核心创新，通过数据导向的设计提供卓越的性能和可扩展性。正确使用这些模式可以显著提升游戏应用的运行效率。*