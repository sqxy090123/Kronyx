# Physics 物理引擎

## 概述

Physics 层是 Kronyx Engine 的刚体物理系统，提供基本的 2D 物理模拟功能。基于 SAP (Sweep and Prune) X 轴宽相和 AABB 最小穿透轴窄相算法，支持碰撞检测、力场系统和射线检测。

## 架构设计

### 核心组件

#### 物理世界 (`kyPhysicsWorld`)
物理世界管理所有刚体、碰撞器和力场的容器，提供物理模拟的统一接口：

```c
typedef struct kyPhysicsWorld kyPhysicsWorld;

// 创建物理世界
KY_API kyPhysicsWorld *ky_physics_create(kyVec3 gravity);

// 销毁物理世界
KY_API void ky_physics_destroy(kyPhysicsWorld *pw);

// 设置重力
KY_API void ky_physics_set_gravity(kyPhysicsWorld *pw, kyVec3 g);
```

#### 刚体 (`kyRigidBody`)
刚体表示具有物理属性的实体，包含位置、速度、质量和惯性等信息：

```c
typedef struct kyRigidBody {
    kyVec3   position;        // 位置
    kyQuat   rotation;        // 旋转
    kyVec3   linear_velocity; // 线速度
    kyVec3   angular_velocity; // 角速度
    float    inv_mass;        // 质量倒数 (0 = 无限质量)
    float    inv_inertia[3];  // 惯性倒数
    float    restitution;     // 弹性系数 (未使用)
    float    friction;        // 摩擦系数 (未使用)
    uint32_t collider_id;     // 关联碰撞器ID
    uint32_t flags;          // 刚体标志
    void    *user_data;      // 用户数据
} kyRigidBody;
```

#### 碰撞器 (`kyCollider`)
碰撞器定义刚体的几何形状，支持多种形状类型：

```c
typedef enum kyColliderShape {
    KY_SHAPE_SPHERE = 0,     // 球体
    KY_SHAPE_BOX,            // 立方体
    KY_SHAPE_CAPSULE,        // 胶囊体
    KY_SHAPE_PLANE,          // 平面
    KY_SHAPE_CONVEX_MESH,    // 凸包网格
    KY_SHAPE_TRI_MESH        // 三角网格
} kyColliderShape;

typedef struct kyCollider {
    kyColliderShape shape;   // 形状类型
    union {                 // 形状数据
        kySphere sphere;    // 球体
        struct { 
            kyVec3 center;       // 立方体中心
            kyVec3 half_extents; // 立方体半尺寸
        } box;
        struct { 
            float radius;     // 胶囊体半径
            float half_height; // 胶囊体半高
        } capsule;
        struct { 
            kyVec3 normal;    // 平面法向量
            float d;          // 平面常数
        } plane;
    } u;
} kyCollider;
```

### 物理算法

#### 宽相阶段 (Broadphase)
使用 SAP (Sweep and Prune) 算法进行 X 轴扫除：
- **单轴扫描**：仅沿 X 轴进行排序和扫描
- **轴包围盒**：使用 Axis-Aligned Bounding Boxes 进行碰撞检测
- **O(n log n)**：排序算法复杂度

```c
// SAP 宽相实现
static void ky_sap_broadphase(const kyExtents *extents, int count,
                              uint32_t *out_a, uint32_t *out_b, int *out_count) {
    // 按 minX 排序
    qsort(extents, count, sizeof(kyExtents), compare_extents_min_x);
    
    // 扫描 X 轴，重叠的物体对添加到碰撞对列表
    for (int i = 0; i < count; i++) {
        float max_x = extents[i].max.x;
        for (int j = i + 1; j < count; j++) {
            if (extents[j].min.x > max_x) break;
            // 添加碰撞对
        }
    }
}
```

#### 窄相阶段 (Narrowphase)
使用 AABB 最小穿透轴进行碰撞解决：
- **穿透检测**：检测两个 AABB 的重叠量
- **轴选择**：选择重叠最小的轴进行分离
- **推离**：沿最小穿透轴推离物体

```c
// AABB 窄相实现
static void ky_aabb_resolve(kyExtents a, kyExtents b, kyVec3 *separation) {
    kyVec3 overlap;
    overlap.x = fminf(a.max.x - b.min.x, a.max.x - b.min.x);
    overlap.y = fminf(a.max.y - b.min.y, a.max.y - b.min.y);
    
    // 选择重叠最小的轴
    if (overlap.x < overlap.y) {
        separation->x = (overlap.x > 0) ? overlap.x : -overlap.x;
        separation->y = 0;
    } else {
        separation->x = 0;
        separation->y = (overlap.y > 0) ? overlap.y : -overlap.y;
    }
}
```

## API 参考

### 世界管理

#### 创建和销毁
```c
// 创建物理世界
KY_API kyPhysicsWorld *ky_physics_create(kyVec3 gravity);

// 销毁物理世界
KY_API void ky_physics_destroy(kyPhysicsWorld *pw);
```

#### 属性设置
```c
// 设置重力
KY_API void ky_physics_set_gravity(kyPhysicsWorld *pw, kyVec3 g);

// 模拟步进
KY_API void ky_physics_step(kyPhysicsWorld *pw, float dt);
```

### 刚体管理

#### 添加和获取
```c
// 添加碰撞器并返回ID
KY_API uint32_t ky_physics_add_collider(kyPhysicsWorld *pw, const kyCollider *c);

// 添加刚体并返回ID
KY_API uint32_t ky_physics_add_body(kyPhysicsWorld *pw, const kyRigidBody *b);

// 获取刚体状态
KY_API void ky_physics_get_body(const kyPhysicsWorld *pw, uint32_t body, kyRigidBody *out);
```

#### 力和冲量
```c
// 应用冲量
KY_API void ky_physics_apply_impulse(kyPhysicsWorld *pw, uint32_t body, kyVec3 impulse, kyVec3 at);

// 获取AABB包围盒
KY_API void ky_physics_get_aabb(const kyPhysicsWorld *pw, uint32_t body, kyVec3 *min_out, kyVec3 *max_out);
```

### 碰撞检测

#### 射线检测
```c
// 射线投射
typedef struct kyRayHit {
    int    hit;             // 是否击中
    float  t;               // 碰撞距离
    kyVec3 normal;          // 碰撞法线
    uint32_t body_id;       // 碰撞物体ID
} kyRayHit;

KY_API void ky_physics_cast_ray(const kyPhysicsWorld *pw, kyVec3 origin, kyVec3 dir, float max_t, kyRayHit *out_hit);
```

#### 接触查询
```c
// 获取接触点数量
KY_API int ky_physics_get_contact_count(const kyPhysicsWorld *pw);

// 获取接触信息
KY_API int ky_physics_get_contact(const kyPhysicsWorld *pw, uint32_t idx,
                                  uint32_t *out_a, uint32_t *out_b);
```

### 力场系统

#### 力场管理
```c
// 力场类型
typedef enum kyForceFieldType {
    KY_FORCE_FIELD_GRAVITY,
    KY_FORCE_FIELD_CONSTANT,
    KY_FORCE_FIELD_RADIAL
} kyForceFieldType;

// 力场结构
typedef struct kyForceField {
    kyForceFieldType type;
    kyVec3 position;
    kyVec3 direction;
    float strength;
    float radius;
    void *user_data;
} kyForceField;

// 力场操作
KY_API uint32_t      ky_physics_add_force_field(kyPhysicsWorld *pw, const kyForceField *field);
KY_API int           ky_physics_remove_force_field(kyPhysicsWorld *pw, uint32_t id);
KY_API int           ky_physics_get_force_field_count(const kyPhysicsWorld *pw);
KY_API kyForceField  ky_physics_get_force_field(const kyPhysicsWorld *pw, uint32_t id);
KY_API void          ky_physics_set_force_field(kyWorld *pw, uint32_t id, kyForceField field);
```

### 自定义物理函数

#### 宽相函数
```c
// 自定义宽相函数
typedef void (*kyPhysicsBroadFn)(const kyExtents *extents, const uint32_t *ids, int count,
                                  uint32_t *out_a, uint32_t *out_b, int *out_count, int max_pairs);

// 设置自定义宽相
KY_API void ky_physics_set_broadphase(kyPhysicsWorld *pw, kyPhysicsBroadFn fn);
```

#### 窄相函数
```c
// 自定义窄相函数
typedef void (*kyPhysicsNarrowFn)(uint32_t a, uint32_t b, int *alive);

// 设置自定义窄相
KY_API void ky_physics_set_narrowphase(kyPhysicsWorld *pw, kyPhysicsNarrowFn fn);
```

## 碰撞事件

### 事件系统
```c
// 碰撞事件类型
#define KY_EVENT_COLLIDE "collide"

// 碰撞事件数据
typedef struct kyCollision {
    uint32_t body_a;     // 碰撞物体A的ID
    uint32_t body_b;     // 碰撞物体B的ID
} kyCollision;
```

使用方式：
```c
// 在实体系统中监听碰撞事件
ky_entity_t entity = ky_entity_from_id(world, 1001);
ky_entity_t collide_entity = ky_entity_from_id(world, 2001);

// 碰撞事件会自动触发 KY_EVENT_COLLIDE 事件
```

## 使用示例

### 基本物理设置
```c
// 创建物理世界
kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, -9.81f, 0});

// 创建地面碰撞器
kyCollider ground_collider = {
    .shape = KY_SHAPE_BOX,
    .u.box = {
        .center = {0, 0, 0},
        .half_extents = {10, 1, 10}
    }
};

uint32_t ground_collider_id = ky_physics_add_collider(pw, &ground_collider);

// 创建地面刚体（静态）
kyRigidBody ground_body = {
    .position = {0, 0, 0},
    .inv_mass = 0,  // 静体
    .collider_id = ground_collider_id
};
ky_physics_add_body(pw, &ground_body);

// 创建玩家碰撞器
kyCollider player_collider = {
    .shape = KY_SHAPE_BOX,
    .u.box = {
        .center = {0, 0, 0},
        .half_extents = {0.5f, 1.0f, 0.5f}
    }
};

uint32_t player_collider_id = ky_physics_add_collider(pw, &player_collider);

// 创建玩家刚体（动态）
kyRigidBody player_body = {
    .position = {0, 5, 0},
    .linear_velocity = {2, 0, 0},
    .inv_mass = 1.0f,
    .collider_id = player_collider_id
};
uint32_t player_id = ky_physics_add_body(pw, &player_body);

// 物理模拟
float dt = 1.0f / 60.0f;  // 60 FPS
ky_physics_step(pw, dt);

// 获取玩家位置
kyRigidBody updated_player;
ky_physics_get_body(pw, player_id, &updated_player);
```

### 碰撞检测示例
```c
// 射线检测
kyRayHit hit;
ky_physics_cast_ray(pw, 
                   (kyVec3){0, 10, 0},    // 起点
                   (kyVec3){0, -1, 0},   // 方向
                   20.0f,                // 最大距离
                   &hit);                // 结果

if (hit.hit) {
    printf("Ray hit at distance: %f\n", hit.t);
    printf("Hit normal: (%f, %f, %f)\n", 
           hit.normal.x, hit.normal.y, hit.normal.z);
}
```

### 力场示例
```c
// 创建引力场
kyForceField gravity_field = {
    .type = KY_FORCE_FIELD_GRAVITY,
    .position = {0, 0, 0},
    .strength = -50.0f,
    .radius = 5.0f
};
uint32_t field_id = ky_physics_add_force_field(pw, &gravity_field);

// 模拟时力场会自动影响刚体
ky_physics_step(pw, dt);
```

## 性能考虑

### 优化策略

1. **宽相优化**
   - SAP 算法复杂度 O(n log n)
   - 仅 X 轴扫描，减少计算量
   - 空间分区减少碰撞对数量

2. **窄相优化**
   - AABB 快速检测
   - 最小穿透轴推离算法
   - 简化的接触约束

3. **内存管理**
   - 固定大小的碰撞器池
   - 批量处理减少函数调用

### 性能监控
```c
// 获取接触信息进行性能分析
int contact_count = ky_physics_get_contact_count(pw);
for (int i = 0; i < contact_count; i++) {
    uint32_t body_a, body_b;
    ky_physics_get_contact(pw, i, &body_a, &body_b);
    // 分析碰撞对
}
```

## 故障排除

### 常见问题

1. **物体穿透**
   - 检查时间步长是否过小
   - 验证速度设置是否合理
   - 调整窄相算法参数

2. **不稳定模拟**
   - 减小时间步长
   - 增加阻尼系数
   - 使用更稳定的积分器

3. **性能问题**
   - 监控碰撞对数量
   - 优化碰撞器大小
   - 减少不必要的物理更新

### 调试技巧

#### 可视化调试
```c
// 获取AABB用于调试
Vec3 min, max;
ky_physics_get_aabb(pw, body_id, &min, &max);
// 可以渲染AABB边界框
```

#### 状态检查
```c
// 检查刚体状态
kyRigidBody body;
ky_physics_get_body(pw, body_id, &body);
printf("Position: (%f, %f, %f)\n", body.position.x, body.position.y, body.position.z);
```

## 已知限制

### 功能限制
- **摩擦力/弹性**：字段存在但未实现
- **多轴SAP**：仅支持 X 轴扫描
- **迭代求解**：无迭代碰撞解决
- **角色控制**：依赖手写积分器而非物理窄相

### 改进计划
- 实现完整的 GJK/EPA 算法
- 支持多轴宽相检测
- 添加迭代约束求解器
- 实现摩擦力和弹性

---

**相关文档**：
- [ARCHITECTURE.md](../ARCHITECTURE.md) - 系统架构设计
- [ECS.md](../专有概念/ECS.md) - 实体组件系统
- [Scene.md](./Scene.md) - 场景管理系统
- [Force Field.md](./ForceField.md) - 力场系统详细说明