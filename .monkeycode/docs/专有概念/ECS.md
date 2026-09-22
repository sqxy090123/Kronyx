# ECS (实体组件系统)

Kronyx Engine 采用基于 Archetype 的实体组件系统 (ECS) 架构，专为高性能游戏开发设计。这种架构提供了优秀的缓存局部性、高吞吐量以及组件的灵活组合能力。

## 概述

### ECS 核心概念

- **实体 (Entity)**: 唯一标识符，包含 `id` 和 `version` 字段用于处理实体复用
- **组件 (Component)**: 纯数据结构，包含实体的状态信息
- **系统 (System)**: 操作组件数据的函数，实现游戏逻辑

### Architecture 特性

- **Archetype 存储**: 同类型组件连续存储，实现缓存友好访问
- **SoA 布局**: 结构化数组存储，避免缓存未命中
- **O(1) 查找**: 实体到组件的高效映射
- **组件迁移**: 实体在不同 archetype 间的高效迁移

## 数据结构

### 实体标识符
```c
typedef struct kyEntity {
    uint32_t id;         // 实体 ID
    uint32_t version;    // 版本号（用于处理实体复用）
} kyEntity;
```

### 组件类型定义
```c
typedef struct kyComponentType {
    const char *name;            // 组件名称
    size_t size;                 // 组件大小
    uint32_t type_id;            // 类型 ID（自动分配）
    void (*ctor)(void *comp);    // 构造函数
    void (*dtor)(void *comp);    // 析构函数
} kyComponentType;
```

### Archetype 结构
```c
typedef struct kyArchetype {
    uint32_t *types;             // 组件类型数组
    uint32_t type_count;         // 组件类型数量
    size_t count;                // 当前实体数量
    size_t capacity;             // 容量
    void **columns;              // 组件列（SoA 存储）
    size_t *strides;            // 组件步长
    uint32_t *entity_ids;        // 实体 ID 数组
    uint64_t type_hash;         // 类型哈希
} kyArchetype;
```

### 实体槽位
```c
typedef struct kyEntitySlot {
    uint32_t version;            // 版本号
    int32_t archetype_index;     // 所属 archetype 索引
    uint32_t row;               // 在 archetype 中的行号
} kyEntitySlot;
```

### ECS 世界
```c
typedef struct kyWorld {
    kyAllocator alloc;           // 内存分配器
    kyArray component_types;    // 组件类型注册表
    kyArray archetypes;        // archetype 数组
    kyArray slots;             // 实体槽位
    kyArray free_ids;          // 可复用的 ID 池
    kyArray systems;            // 系统注册表
    uint32_t next_version;     // 下一个版本号
    kyHashMap name_cache;      // 组件名称缓存
} kyWorld;
```

## 使用方法

### 1. 创建 ECS 世界

```c
kyWorld *ky_world_create(kyAllocator *alloc);
void ky_world_destroy(kyWorld *w);
```

```c
// 示例：创建世界
kyAllocator al = ky_default_allocator();
kyWorld *world = ky_world_create(&al);
```

### 2. 注册组件类型

```c
uint32_t ky_world_register_component(kyWorld *w, const kyComponentType *t);
const kyComponentType *ky_world_component_type(const kyWorld *w, uint32_t type_id);
uint32_t ky_world_component_type_by_name(const kyWorld *w, const char *name);
```

```c
// 示例：注册组件类型
typedef struct Position {
    kyVec3 pos;
} Position;

typedef struct Velocity {
    kyVec3 vel;
} Velocity;

static void position_ctor(void *comp) {
    Position *p = (Position *)comp;
    p->pos = ky_vec3_zero();
}

static void position_dtor(void *comp) {
    // 清理资源（如果有）
}

// 注册组件
kyComponentType position_type = {
    .name = "position",
    .size = sizeof(Position),
    .ctor = position_ctor,
    .dtor = position_dtor
};
uint32_t tid_position = ky_world_register_component(world, &position_type);

// 注册第二个组件
kyComponentType velocity_type = {
    .name = "velocity", 
    .size = sizeof(Velocity),
    .ctor = NULL,
    .dtor = NULL
};
uint32_t tid_velocity = ky_world_register_component(world, &velocity_type);
```

### 3. 实体管理

```c
kyEntity ky_world_spawn(kyWorld *w);
void ky_world_despawn(kyWorld *w, kyEntity e);
int ky_entity_valid(const kyWorld *w, kyEntity e);
int ky_world_alive_count(const kyWorld *w);
kyEntity ky_world_get_alive_entity(const kyWorld *w, int idx);
```

```c
// 示例：创建实体
kyEntity player = ky_world_spawn(world);
kyEntity enemy = ky_world_spawn(world);

// 检查实体有效性
if (ky_entity_valid(world, player)) {
    // 操作实体...
}

// 销毁实体
ky_world_despawn(world, player);
```

### 4. 组件管理

```c
void *ky_world_add_component(kyWorld *w, kyEntity e, uint32_t type_id);
void *ky_world_get_component(const kyWorld *w, kyEntity e, uint32_t type_id);
int ky_world_has_component(const kyWorld *w, kyEntity e, uint32_t type_id);
void ky_world_remove_component(kyWorld *w, kyEntity e, uint32_t type_id);
void ky_world_remove_all_components(kyWorld *w, kyEntity e);
```

```c
// 示例：添加和访问组件
// 添加组件
Position *pos = ky_world_add_component(world, player, tid_position);
pos->pos = ky_vec3(0, 0, 0);

// 访问组件
Position *current_pos = ky_world_get_component(world, player, tid_position);
if (current_pos) {
    current_pos->pos.x += 1.0f;
}

// 检查组件存在性
if (ky_world_has_component(world, player, tid_velocity)) {
    Velocity *vel = ky_world_get_component(world, player, tid_velocity);
    vel->vel = ky_vec3(10, 0, 0);
}

// 移除组件
ky_world_remove_component(world, player, tid_velocity);
```

### 5. 系统管理

```c
typedef struct kySystem {
    const char *name;           // 系统名称
    uint32_t order;             // 执行顺序
    void (*update)(kyWorld *w, float dt, void *user); // 更新函数
    void *user;                 // 用户数据
} kySystem;

void ky_world_register_system(kyWorld *w, const kySystem *sys);
void ky_world_sort_systems(kyWorld *w);
void ky_world_step(kyWorld *w, float dt);
```

```c
// 示例：创建和注册系统
static void movement_system(kyWorld *w, float dt, void *user) {
    (void)user;
    
    // 获取 position 和 velocity 组件类型
    uint32_t tid_pos = ky_world_component_type_by_name(w, "position");
    uint32_t tid_vel = ky_world_component_type_by_name(w, "velocity");
    
    // 遍历同时拥有 position 和 velocity 的实体
    kyViewIter it;
    uint32_t types[] = {tid_pos, tid_vel};
    if (ky_view_begin(w, types, 2, &it)) {
        while (ky_view_next(&it)) {
            Position *pos = ky_view_get_component(&it, tid_pos);
            Velocity *vel = ky_view_get_component(&it, tid_vel);
            
            // 更新位置
            pos->pos.x += vel->vel.x * dt;
            pos->pos.y += vel->vel.y * dt;
        }
    }
}

// 注册系统
kySystem movement = {
    .name = "movement",
    .order = 1,
    .update = movement_system,
    .user = NULL
};
ky_world_register_system(world, &movement);
```

### 6. 视图迭代

```c
typedef struct kyViewIter {
    const kyWorld *w;
    const uint32_t *types;      // 需要的组件类型
    uint32_t type_count;        // 组件类型数量
    int32_t arch_index;        // 当前 archetype 索引
    size_t row;                // 当前行号
    kyEntity current;          // 当前实体
} kyViewIter;

int ky_view_begin(const kyWorld *w, const uint32_t *types, uint32_t type_count, kyViewIter *it);
int ky_view_next(kyViewIter *it);
void *ky_view_get_component(const kyViewIter *it, uint32_t type_id);
```

```c
// 示例：使用视图迭代
static void render_system(kyWorld *w, float dt, void *user) {
    (void)dt; (void)user;
    
    // 获取 render 相关组件
    uint32_t tid_transform = ky_world_component_type_by_name(w, "transform");
    uint32_t tid_sprite = ky_world_component_type_by_name(w, "sprite");
    uint32_t tid_camera = ky_world_component_type_by_name(w, "camera");
    
    // 渲染相机
    kyEntity camera = ky_entity_null;
    kyViewIter cam_it;
    if (ky_view_begin(w, &tid_camera, 1, &cam_it)) {
        while (ky_view_next(&cam_it)) {
            camera = cam_it.current;
            break; // 使用第一个激活的相机
        }
    }
    
    if (!camera) return;
    
    // 渲染精灵
    kyViewIter sprite_it;
    uint32_t sprite_types[] = {tid_transform, tid_sprite};
    if (ky_view_begin(w, sprite_types, 2, &sprite_it)) {
        while (ky_view_next(&sprite_it)) {
            Transform *t = ky_view_get_component(&sprite_it, tid_transform);
            Sprite *s = ky_view_get_component(&sprite_it, tid_sprite);
            
            // 渲染精灵...
            render_sprite(s, t);
        }
    }
}
```

## 高级特性

### 1. 组件构造和析构

```c
// 自定义组件构造函数
static void mesh_ctor(void *comp) {
    Mesh *mesh = (Mesh *)comp;
    mesh->vertices = ky_alloc(1024 * sizeof(Vertex));
    mesh->vertex_count = 0;
    mesh->capacity = 1024;
}

static void mesh_dtor(void *comp) {
    Mesh *mesh = (Mesh *)comp;
    ky_free(mesh->vertices);
    mesh->vertices = NULL;
}

// 注册时提供构造/析构函数
kyComponentType mesh_type = {
    .name = "mesh",
    .size = sizeof(Mesh),
    .ctor = mesh_ctor,
    .dtor = mesh_dtor
};
```

### 2. 实体迁移

Kronyx 实现了高效的实体迁移机制，当实体在不同 archetype 间移动时：

1. **查找目标 archetype**: 基于新的组件组合
2. **创建新 archetype**（如果不存在）
3. **分配行空间**: 扩展目标 archetype
4. **复制组件数据**: 保留现有组件，创建新组件
5. **清理旧 archetype**: 调用析构函数，删除行

```c
// 演示实体迁移
void add_health_component(kyWorld *w, kyEntity e) {
    uint32_t tid_health = ky_world_component_type_by_name(w, "health");
    
    // 添加健康组件
    Health *health = ky_world_add_component(w, e, tid_health);
    health->value = 100.0f;
    
    // 如果之前没有位置组件，也会自动创建 archetype
    uint32_t tid_pos = ky_world_component_type_by_name(w, "position");
    if (!ky_world_has_component(w, e, tid_pos)) {
        Position *pos = ky_world_add_component(w, e, tid_pos);
        pos->pos = ky_vec3(0, 0, 0);
    }
}
```

### 3. 批量操作

```c
// 批量创建实体
kyEntity create_entities(kyWorld *w, int count, uint32_t *component_types) {
    kyEntity entities[KY_MAX_BATCH_SIZE];
    
    for (int i = 0; i < count; i++) {
        entities[i] = ky_world_spawn(w);
        
        // 添加组件
        for (int j = 0; component_types[j]; j++) {
            ky_world_add_component(w, entities[i], component_types[j]);
        }
    }
    
    return entities[0]; // 返回第一个实体
}
```

## 性能考虑

### 1. 缓存友好性

- **连续存储**: 同类型组件在内存中连续存储
- **SoA 布局**: 避免缓存未命中，提高访问效率
- **批量处理**: 相同 archetype 的实体可以被批量处理

### 2. 内存布局示例

```c
// Archetype 存储（示意图）
struct PositionArchetype {
    uint32_t entity_ids[KY_MAX_ENTITIES];     // 实体 ID
    float positions_x[KY_MAX_ENTITIES];       // X 坐标
    float positions_y[KY_MAX_ENTITIES];       // Y 坐标
    float positions_z[KY_MAX_ENTITIES];       // Z 坐标
    size_t count;                              // 当前实体数量
};

struct PositionVelocityArchetype {
    uint32_t entity_ids[KY_MAX_ENTITIES];     // 实体 ID
    float positions_x[KY_MAX_ENTITIES];       // X 坐标
    float positions_y[KY_MAX_ENTITIES];       // Y 坐标
    float positions_z[KY_MAX_ENTITIES];       // Z 坐标
    float velocities_x[KY_MAX_ENTITIES];      // X 速度
    float velocities_y[KY_MAX_ENTITIES];      // Y 速度
    float velocities_z[KY_MAX_ENTITIES];      // Z 速度
    size_t count;                              // 当前实体数量
};
```

### 3. 性能优化建议

```c
// 1. 减少组件迁移 - 合理设计组件组合
typedef struct Player {
    Transform transform;    // 高频更新组件
    Sprite sprite;         // 渲染组件
    PlayerController controller; // 输入处理
    Stats stats;           // 低频更新数据
} Player;

// 2. 使用合适的组件大小
// 避免过大的组件，考虑分离高频和低频更新的数据

// 3. 批量处理相同 archetype 的实体
void process_moving_entities(kyWorld *w) {
    uint32_t tid_pos = ky_world_component_type_by_name(w, "position");
    uint32_t tid_vel = ky_world_component_type_by_name(w, "velocity");
    
    kyViewIter it;
    uint32_t types[] = {tid_pos, tid_vel};
    if (ky_view_begin(w, types, 2, &it)) {
        // 批量处理
        update_positions_batch(&it, dt);
        update_physics_batch(&it, dt);
    }
}
```

## 调试和诊断

### 1. 实体和组件状态查询

```c
// 查询实体信息
void entity_debug_info(kyWorld *w, kyEntity e) {
    printf("Entity %u (version %u):\n", e.id, e.version);
    
    // 列出所有组件
    for (int i = 0; i < w->component_types.count; i++) {
        kyComponentType *ct = (kyComponentType*)w->component_types.data + i;
        if (ky_world_has_component(w, e, ct->type_id)) {
            printf("  - %s\n", ct->name);
        }
    }
}

// 统计信息
void world_stats(kyWorld *w) {
    printf("World Statistics:\n");
    printf("  Total entities: %zu\n", w->slots.count);
    printf("  Alive entities: %d\n", ky_world_alive_count(w));
    printf("  Component types: %zu\n", w->component_types.count);
    printf("  Archetypes: %zu\n", w->archetypes.count);
    
    // 各 archetype 统计
    for (int i = 0; i < w->archetypes.count; i++) {
        kyArchetype *arch = (kyArchetype*)w->archetypes.data + i;
        printf("  Archetype %d: %zu entities\n", i, arch->count);
    }
}
```

### 2. 视图调试

```c
// 调试视图迭代
void debug_view(kyWorld *w, uint32_t *types, uint32_t type_count) {
    kyViewIter it;
    if (ky_view_begin(w, types, type_count, &it)) {
        printf("View contains %zu archetypes:\n", w->archetypes.count);
        
        int entity_count = 0;
        while (ky_view_next(&it)) {
            entity_count++;
            printf("  Entity %u (archetype %d, row %zu)\n", 
                   it.current.id, it.arch_index, it.row);
        }
        
        printf("Total entities in view: %d\n", entity_count);
    } else {
        printf("No entities match the view criteria\n");
    }
}
```

## 实际应用示例

### 1. 简单的游戏对象

```c
// 游戏对象类型定义
typedef struct GameObject {
    uint32_t archetype_id;    // 所属 archetype ID
    void **components;       // 组件数据指针
} GameObject;

// 创建游戏对象
GameObject create_game_object(kyWorld *w, uint32_t *component_types) {
    GameObject obj;
    
    // 创建实体
    kyEntity entity = ky_world_spawn(w);
    
    // 添加组件
    obj.archetype_id = 0;
    for (int i = 0; component_types[i]; i++) {
        void *comp = ky_world_add_component(w, entity, component_types[i]);
        if (comp) {
            obj.components[i] = comp;
        }
    }
    
    return obj;
}

// 更新游戏对象
void update_game_object(GameObject *obj, float dt) {
    // 根据 archetype 更新不同类型的对象
    switch (obj->archetype_id) {
        case PLAYER_ARCHETYPE:
            update_player(obj, dt);
            break;
        case ENEMY_ARCHETYPE:
            update_enemy(obj, dt);
            break;
        case PROJECTILE_ARCHETYPE:
            update_projectile(obj, dt);
            break;
    }
}
```

### 2. 事件驱动的 ECS

```c
// 事件驱动的系统
typedef struct EventSystem {
    kyWorld *world;
    kyArray event_queue;
    kyArray event_handlers;
} EventSystem;

// 注册事件处理器
void register_event_handler(EventSystem *es, int event_type, 
                           void (*handler)(kyWorld*, const void*)) {
    EventHandler handler = {
        .event_type = event_type,
        .handler = handler
    };
    ky_array_push(&es->event_handlers, &handler);
}

// 处理事件队列
void process_events(EventSystem *es) {
    while (es->event_queue.count > 0) {
        Event *event = (Event*)es->event_queue.data + es->event_queue.count - 1;
        
        // 查找对应的事件处理器
        for (int i = 0; i < es->event_handlers.count; i++) {
            EventHandler *h = (EventHandler*)es->event_handlers.data + i;
            if (h->event_type == event->type) {
                h->handler(es->world, event);
            }
        }
        
        ky_array_pop(&es->event_queue);
    }
}
```

### 3. 池化实体管理

```c
typedef struct EntityPool {
    kyEntity *entities;
    int count;
    int capacity;
    kyArray free_indices;
} EntityPool;

EntityPool* entity_pool_create(int capacity) {
    EntityPool *pool = ky_alloc(sizeof(EntityPool));
    pool->entities = ky_alloc(capacity * sizeof(kyEntity));
    pool->count = 0;
    pool->capacity = capacity;
    pool->free_indices = ky_array_create(sizeof(int), NULL);
    return pool;
}

kyEntity entity_pool_alloc(EntityPool *pool) {
    if (pool->free_indices.count > 0) {
        // 从空闲列表获取
        int index = *(int*)ky_array_pop(&pool->free_indices);
        pool->entities[index].version++;
        return pool->entities[index];
    } else if (pool->count < pool->capacity) {
        // 创建新实体
        kyEntity e = {pool->count, 1};
        pool->entities[pool->count++] = e;
        return e;
    }
    
    // 池已满
    return (kyEntity){0, 0};
}

void entity_pool_free(EntityPool *pool, kyEntity e) {
    // 添加到空闲列表
    int index = e.id;
    ky_array_push(&pool->free_indices, &index);
}
```

## 总结

Kronyx 的 ECS 系统提供了以下优势：

1. **高性能**: 基于 Archetype 和 SoA 的存储实现优秀的缓存局部性
2. **灵活性**: 组件可以动态组合，支持复杂的游戏对象类型
3. **可扩展性**: 系统可以独立添加和优化
4. **内存效率**: 实体复用和组件池化管理减少内存碎片

通过合理使用 ECS 模式，开发者可以构建高性能、可维护的游戏系统。记住关键原则：

- 保持组件数据简单和专注
- 优化 archetype 设计以减少迁移
- 使用视图迭代进行批量处理
- 利用系统的顺序性管理依赖关系