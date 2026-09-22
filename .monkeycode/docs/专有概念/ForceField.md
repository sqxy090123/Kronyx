# Force Field 力场系统

## 概述

力场系统是 Kronyx Engine 物理引擎的重要组成部分，提供各种物理力场的创建和管理功能。支持重力场、恒力场和径向力场等多种力场类型，能够模拟复杂的物理交互和游戏效果。

## 架构设计

### 力场类型

#### 力场枚举
```c
typedef enum kyForceFieldType {
    KY_FORCE_FIELD_GRAVITY = 0,    // 重力场
    KY_FORCE_FIELD_CONSTANT,      // 恒力场
    KY_FORCE_FIELD_RADIAL,        // 径向力场
    KY_FORCE_FIELD_SPRING,        // 弹簧力场
    KY_FORCE_FIELD_VORTEX,        // 漩涡力场
    KY_FORCE_FIELD_WIND           // 风力场
} kyForceFieldType;
```

#### 力场结构
```c
typedef struct kyForceField {
    kyForceFieldType type;        // 力场类型
    
    // 基础属性
    kyVec3 position;              // 力场位置
    kyVec3 direction;             // 力场方向 (重力场/恒力场)
    float strength;               // 力场强度
    float radius;                 // 影响半径
    
    // 时间属性
    float duration;               // 持续时间 (<= 0 = 永久)
    float elapsed;                // 已经过时间
    
    // 额外参数 (根据类型不同)
    union {
        struct {
            float damping;       // 阻尼系数
        } gravity;
        
        struct {
            float frequency;     // 频率
            float amplitude;     // 振幅
        } spring;
        
        struct {
            float rotation_speed; // 旋转速度
            float attraction;    // 吸引力
        } vortex;
        
        // 其他类型的参数...
    } params;
    
    // 用户数据
    void *user_data;
    
    // 链表指针
    struct kyForceField *next;
} kyForceField;
```

### 力场系统

#### 物理世界集成
```c
// 在物理世界中管理力场
typedef struct kyPhysicsWorld {
    // ... 其他物理世界字段 ...
    kyForceField *force_fields;  // 力场链表
    int force_field_count;       // 力场数量
} kyPhysicsWorld;
```

#### 力场应用
```c
// 在物理模拟中应用力场
static void ky_physics_apply_force_fields(kyPhysicsWorld *pw, float dt) {
    kyForceField *field = pw->force_fields;
    
    while (field != NULL) {
        // 更新力场时间
        field->elapsed += dt;
        
        // 检查力场是否过期
        if (field->duration > 0 && field->elapsed >= field->duration) {
            // 移除过期力场 (在调用处处理)
            continue;
        }
        
        // 应用力场效果
        ky_force_field_apply(field, pw, dt);
        
        field = field->next;
    }
}
```

## API 参考

### 力场管理

#### 创建力场
```c
// 创建重力场
KY_API uint32_t ky_physics_add_gravity_field(kyPhysicsWorld *pw, 
                                            const kyVec3 position, 
                                            float strength,
                                            float radius);

// 创建恒力场
KY_API uint32_t ky_physics_add_constant_field(kyPhysicsWorld *pw,
                                            const kyVec3 position,
                                            const kyVec3 direction,
                                            float strength,
                                            float radius);

// 创建径向力场
KY_API uint32_t ky_physics_add_radial_field(kyPhysicsWorld *pw,
                                            const kyVec3 position,
                                            float strength,
                                            float radius,
                                            int attractive); // 1=吸引, 0=排斥
```

#### 力场获取和更新
```c
// 获取力场数量
KY_API int ky_physics_get_force_field_count(const kyPhysicsWorld *pw);

// 获取特定力场
KY_API kyForceField ky_physics_get_force_field(const kyPhysicsWorld *pw, uint32_t id);

// 设置力场参数
KY_API void ky_physics_set_force_field(kyPhysicsWorld *pw, uint32_t id, kyForceField field);

// 移除力场
KY_API int ky_physics_remove_force_field(kyPhysicsWorld *pw, uint32_t id);
```

### 力场操作

#### 弹簧力场
```c
// 创建弹簧力场
KY_API uint32_t ky_physics_add_spring_field(kyPhysicsWorld *pw,
                                            const kyVec3 anchor,
                                            float spring_constant,
                                            float damping);

// 更新弹簧锚点
KY_API void ky_spring_field_set_anchor(kyPhysicsWorld *pw, uint32_t id, const kyVec3 anchor);
```

#### 漩涡力场
```c
// 创建漩涡力场
KY_API uint32_t ky_physics_add_vortex_field(kyPhysicsWorld *pw,
                                            const kyVec3 center,
                                            float radius,
                                            float rotation_speed,
                                            float attraction);
```

#### 风力场
```c
// 创建风力场
KY_API uint32_t ky_physics_add_wind_field(kyPhysicsWorld *pw,
                                         const kyVec3 direction,
                                         float strength,
                                         float turbulence);
```

### 力场查询

#### 力场影响查询
```c
// 查询某点受到的力
KY_API kyVec3 ky_physics_get_force_at(const kyPhysicsWorld *pw, const kyVec3 position);

// 查询力场影响范围
KY_API int ky_physics_is_in_force_field(const kyPhysicsWorld *pw, 
                                       const kyVec3 position, 
                                       uint32_t field_id);

// 获取力场统计信息
typedef struct kyForceFieldStats {
    int active_count;
    float total_strength;
    float average_radius;
} kyForceFieldStats;

KY_API void ky_physics_get_force_field_stats(const kyPhysicsWorld *pw, kyForceFieldStats *stats);
```

## 使用示例

### 基本力场使用
```c
#include "kronyx/physics.h"
#include "kronyx/force_field.h"

void basic_force_fields_example() {
    // 创建物理世界
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, -9.81f, 0});
    
    // 添加重力场
    uint32_t gravity_id = ky_physics_add_gravity_field(pw, 
                                                      (kyVec3){0, 0, 0}, 
                                                      50.0f,      // 强度
                                                      10.0f);     // 半径
    
    // 添加恒力场（风力）
    kyVec3 wind_direction = {1, 0, 0};
    uint32_t wind_id = ky_physics_add_constant_field(pw,
                                                    (kyVec3){0, 5, 0},
                                                    wind_direction,
                                                    20.0f,      // 风力强度
                                                    15.0f);     // 影响范围
    
    // 添加径向力场（爆炸效果）
    uint32_t explosion_id = ky_physics_add_radial_field(pw,
                                                        (kyVec3){0, 0, 0},
                                                        100.0f,     // 爆炸强度
                                                        8.0f,       // 影响半径
                                                        0);         // 0=排斥
    
    // 物理模拟
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 600; i++) {  // 10秒模拟
        ky_physics_step(pw, dt);
        
        // 检查力场是否过期
        if (i == 300) {  // 5秒后移除爆炸效果
            ky_physics_remove_force_field(pw, explosion_id);
        }
    }
    
    // 清理
    ky_physics_destroy(pw);
}
```

### 动态力场创建
```c
void dynamic_force_fields_example(void) {
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, -9.81f, 0});
    
    // 动态创建力场
    void create_impact_effect(kyVec3 position) {
        // 创建临时力场
        uint32_t impact_id = ky_physics_add_radial_field(pw,
                                                         position,
                                                         50.0f,    // 强度
                                                         3.0f,     // 半径
                                                         0);       // 排斥力
        
        // 设置力场持续时间为 1 秒
        kyForceField field = ky_physics_get_force_field(pw, impact_id);
        field.duration = 1.0f;
        ky_physics_set_force_field(pw, impact_id, field);
    }
    
    // 使用示例
    create_impact_effect((kyVec3){5, 0, 0});
    create_impact_effect((kyVec3){-3, 2, 1});
    
    // 运行物理
    ky_physics_step(pw, 1.0f / 60.0f);
}
```

### 力场效果预览
```c
void force_field_visualization_example(void) {
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, 0, 0});
    
    // 创建多个力场
    ky_physics_add_gravity_field(pw, (kyVec3){0, 0, 0}, 30.0f, 5.0f);
    ky_physics_add_vortex_field(pw, (kyVec3){5, 0, 0}, 3.0f, 2.0f, 10.0f);
    ky_physics_add_wind_field(pw, (kyVec3){1, 0, 0}, 15.0f, 0.5f);
    
    // 可视化力场效果
    for (int x = -10; x <= 10; x += 2) {
        for (int y = -10; y <= 10; y += 2) {
            kyVec3 pos = {x, y, 0};
            kyVec3 force = ky_physics_get_force_at(pw, pos);
            
            // 绘制力向量
            draw_vector(pos, force);
        }
    }
    
    ky_physics_destroy(pw);
}
```

### 游戏应用示例
```c
void game_force_field_example(void) {
    kyPhysicsWorld *pw = ky_physics_create((kyVec3){0, -9.81f, 0});
    
    // 玩家技能：引力场
    uint32_t player_gravity_field = 0;
    int skill_active = 0;
    
    // 技能激活
    void activate_gravity_skill(void) {
        if (!skill_active) {
            player_gravity_field = ky_physics_add_gravity_field(pw,
                                                              player_position,
                                                              40.0f,
                                                              8.0f);
            skill_active = 1;
        }
    }
    
    // 技能取消
    void deactivate_gravity_skill(void) {
        if (skill_active) {
            ky_physics_remove_force_field(pw, player_gravity_field);
            skill_active = 0;
        }
    }
    
    // 敌人AI：创建力场陷阱
    void create_trap_field(kyVec3 position) {
        // 创建弹簧陷阱
        uint32_t trap_id = ky_physics_add_spring_field(pw,
                                                      position,
                                                      100.0f,    // 弹簧常数
                                                      5.0f);     // 阻尼
        
        // 陷阱持续 3 秒
        kyForceField field = ky_physics_get_force_field(pw, trap_id);
        field.duration = 3.0f;
        ky_physics_set_force_field(pw, trap_id, field);
    }
    
    // 主循环
    float skill_cooldown = 0;
    while (game_running) {
        float dt = get_delta_time();
        
        // 更新技能冷却
        if (skill_cooldown > 0) {
            skill_cooldown -= dt;
        }
        
        // 检测技能按键
        if (input_pressed(SKILL_KEY) && skill_cooldown <= 0) {
            activate_gravity_skill();
            skill_cooldown = 5.0f;  // 5秒冷却
        }
        
        // 敌人AI逻辑
        if (random_chance(0.01f)) {  // 1% 概率每帧
            create_trap_field(get_random_position());
        }
        
        // 物理模拟
        ky_physics_step(pw, dt);
        
        // 更新游戏逻辑...
    }
    
    ky_physics_destroy(pw);
}
```

## 力场类型详解

### 重力场

#### 特性
- 模拟重力效果
- 强度恒定，方向向下
- 影响指定半径内的所有物体

#### 应用场景
- 游戏基础重力
- 某些技能效果（重力井）
- 特殊区域效果

#### 参数配置
```c
// 基础重力场
ky_physics_add_gravity_field(world, position, strength, radius);

// 高级重力场（带阻尼）
kyForceField gravity_field = {
    .type = KY_FORCE_FIELD_GRAVITY,
    .position = position,
    .strength = strength,
    .radius = radius,
    .params.gravity.damping = 0.1f  // 阻尼系数
};
```

### 恒力场

#### 特性
- 持续施加恒定方向的力
- 可用于风力、推进力等效果
- 支持动态调整强度

#### 应用场景
- 环境风力
- 武器后坐力
- 推进器效果

#### 参数配置
```c
// 恒力场
ky_physics_add_constant_field(world, position, direction, strength, radius);

// 风力场示例
kyVec3 wind_direction = {1, 0.2f, 0};  // 略微向上的风
ky_physics_add_constant_field(world, wind_origin, wind_direction, 20.0f, 15.0f);
```

### 径向力场

#### 特性
- 从中心点向外或向内施加力
- 可用于爆炸、吸引、排斥效果
- 力的大小随距离衰减

#### 应用场景
- 爆炸效果
- 磁力吸引
- 反弹效果

#### 参数配置
```c
// 径向力场
// attractive=1 吸引, attractive=0 排斥
ky_physics_add_radial_field(world, center, strength, radius, attractive);

// 爆炸效果
ky_physics_add_radial_field(world, explosion_center, 100.0f, 8.0f, 0);

// 吸引效果
ky_physics_add_radial_field(world, magnet_position, 50.0f, 10.0f, 1);
```

### 弹簧力场

#### 特性
- 类似弹簧的胡克定律效果
- 物体会被拉向锚点
- 支持阻尼系数

#### 应用场景
- 弹性平台
- 橡胶效果
- 约束效果

#### 参数配置
```c
// 弹簧力场
ky_physics_add_spring_field(world, anchor, spring_constant, damping);

// 弹性平台
ky_physics_add_spring_field(world, platform_position, 80.0f, 3.0f);
```

### 漩涡力场

#### 特性
- 旋转的力场
- 结合径向力和切向力
- 可模拟龙卷风、漩涡效果

#### 应用场景
- 龙卷风效果
- 漩涡陷阱
- 旋涡武器

#### 参数配置
```c
// 漩涡力场
ky_physics_add_vortex_field(world, center, radius, rotation_speed, attraction);

// 龙卷风效果
ky_physics_add_vortex_field(world, tornado_center, 10.0f, 3.0f, 20.0f);
```

## 性能优化

### 力场剔除

#### 空间划分
```c
// 使用空间网格快速检测力场影响
typedef struct kyForceFieldGrid {
    kyForceField *cells[GRID_SIZE][GRID_SIZE];
    int cell_size;
} kyForceFieldGrid;

// 添加力场到网格
void ky_force_field_grid_add(kyForceFieldGrid *grid, kyForceField *field) {
    int cell_x = (int)(field->position.x / grid->cell_size);
    int cell_y = (int)(field->position.y / grid->cell_size);
    
    if (cell_x >= 0 && cell_x < GRID_SIZE && cell_y >= 0 && cell_y < GRID_SIZE) {
        field->next = grid->cells[cell_x][cell_y];
        grid->cells[cell_x][cell_y] = field;
    }
}

// 快速查询影响
kyVec3 ky_force_field_grid_get_force(kyForceFieldGrid *grid, kyVec3 position) {
    kyVec3 total_force = {0};
    
    int cell_x = (int)(position.x / grid->cell_size);
    int cell_y = (int)(position.y / grid->cell_size);
    
    // 检查周围网格
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int x = cell_x + dx;
            int y = cell_y + dy;
            
            if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                kyForceField *field = grid->cells[x][y];
                while (field != NULL) {
                    if (ky_vec3_dist(position, field->position) <= field->radius) {
                        kyVec3 force = ky_force_field_calculate(field, position);
                        total_force = ky_vec3_add(total_force, force);
                    }
                    field = field->next;
                }
            }
        }
    }
    
    return total_force;
}
```

### 距离计算优化

#### 简化检测
```c
// 简化的距离检测
static int ky_force_field_affects_simple(const kyForceField *field, const kyVec3 *position) {
    // 使用简单的包围盒检测
    float dx = position->x - field->position.x;
    float dy = position->y - field->position.y;
    float dz = position->z - field->position.z;
    
    return (dx * dx + dy * dy + dz * dz) <= (field->radius * field->radius);
}
```

### 批量处理

#### 多物体应用
```c
// 批量应用力场到多个物体
void ky_force_field_apply_batch(kyPhysicsWorld *pw, kyForceField *field, 
                               kyEntity *entities, int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (ky_force_field_affects(field, &entities[i].position)) {
            kyVec3 force = ky_force_field_calculate(field, &entities[i].position);
            ky_physics_apply_force(pw, entities[i].id, force, entities[i].position);
        }
    }
}
```

## 故障排除

### 常见问题

#### 力场效果过强
```bash
# 问题：力场效果过于强烈
# 解决方案：
# 1. 降低强度参数
ky_physics_add_radial_field(world, position, 25.0f, radius, attractive);

# 2. 添加阻尼
field.params.gravity.damping = 0.2f;

# 3. 限制最大力
void clamp_force(kyVec3 *force, float max_force) {
    float len = ky_vec3_len(*force);
    if (len > max_force) {
        *force = ky_vec3_normalize(*force);
        *force = ky_vec3_scale(*force, max_force);
    }
}
```

#### 力场范围问题
```bash
# 问题：力场范围过大或过小
# 解决方案：
# 1. 调整半径参数
ky_physics_add_gravity_field(world, position, strength, 5.0f);  // 缩小范围

# 2. 使用渐变效果
float falloff = 1.0f - (distance / field->radius);
if (falloff < 0) falloff = 0;
```

#### 力场不生效
```bash
# 问题：力场没有效果
# 解决方案：
# 1. 检查力场是否成功创建
uint32_t field_id = ky_physics_add_gravity_field(...);
if (field_id == 0) {
    printf("Failed to create force field\n");
}

# 2. 检查物理世界是否正确更新
ky_physics_step(pw, dt);

# 3. 验证物体质量
// 确保物体有质量 (inv_mass > 0)
```

### 调试工具

#### 力场可视化
```c
// 可视化力场范围和强度
void visualize_force_field(kyForceField *field) {
    // 绘制力场边界球
    draw_sphere(field->position, field->radius, COLOR_BLUE);
    
    // 绘制力场中心
    draw_sphere(field->position, 0.5f, COLOR_RED);
    
    // 绘制力向量
    for (int angle = 0; angle < 360; angle += 30) {
        kyVec3 test_pos = {
            field->position.x + cos(angle * KY_DEG2RAD) * field->radius * 0.8f,
            field->position.y + sin(angle * KY_DEG2RAD) * field->radius * 0.8f,
            field->position.z
        };
        
        kyVec3 force = ky_force_field_calculate(field, &test_pos);
        draw_vector(test_pos, force);
    }
}
```

#### 力场统计
```c
// 力场性能统计
typedef struct kyForceFieldStats {
    int active_fields;
    int total_applications;
    float average_force;
    float max_force;
} kyForceFieldStats;

void ky_force_field_monitor(kyPhysicsWorld *pw) {
    kyForceFieldStats stats = {0};
    
    kyForceField *field = pw->force_fields;
    while (field != NULL) {
        stats.active_fields++;
        
        // 统计应用次数
        stats.total_applications++;
        
        // 计算力和
        kyVec3 force = ky_force_field_calculate(field, &field->position);
        float force_magnitude = ky_vec3_len(force);
        stats.average_force += force_magnitude;
        
        if (force_magnitude > stats.max_force) {
            stats.max_force = force_magnitude;
        }
        
        field = field->next;
    }
    
    if (stats.active_fields > 0) {
        stats.average_force /= stats.active_fields;
    }
    
    printf("Force Field Stats:\n");
    printf("  Active fields: %d\n", stats.active_fields);
    printf("  Total applications: %d\n", stats.total_applications);
    printf("  Average force: %.2f\n", stats.average_force);
    printf("  Max force: %.2f\n", stats.max_force);
}
```

## 扩展功能

### 自定义力场

#### 自定义力场类型
```c
typedef struct kyCustomForceField {
    kyForceFieldType type;
    
    // 自定义计算函数
    kyVec3 (*calculate_force)(const kyCustomForceField *field, 
                            const kyVec3 *position, 
                            const kyVec3 *velocity);
    
    // 自定义参数
    void *custom_data;
    
    // 基础属性
    kyVec3 position;
    float strength;
    float radius;
} kyCustomForceField;

// 自定义力场应用
kyVec3 custom_force_calculate(const kyCustomForceField *field, 
                              const kyVec3 *position, 
                              const kyVec3 *velocity) {
    // 自定义计算逻辑
    kyVec3 direction = ky_vec3_sub(*position, field->position);
    float distance = ky_vec3_len(direction);
    
    if (distance == 0 || distance > field->radius) {
        return (kyVec3){0};
    }
    
    // 自定义力计算
    kyVec3 force = ky_vec3_normalize(direction);
    force = ky_vec3_scale(force, field->strength * (1.0f - distance / field->radius));
    
    // 添加速度相关效果
    kyVec3 drag = ky_vec3_scale(*velocity, -field->strength * 0.1f);
    force = ky_vec3_add(force, drag);
    
    return force;
}
```

### 命令行控制

#### 运行时控制力场
```c
// 命令行控制接口
void ky_force_field_cli_init(kyPhysicsWorld *pw) {
    // 注册命令
    cli_register_command("add_gravity", "添加重力场", 
                       (CLICommandFunc)cli_add_gravity_field);
    cli_register_command("add_wind", "添加风力场", 
                       (CLICommandFunc)cli_add_wind_field);
    cli_register_command("remove_field", "移除力场", 
                       (CLICommandFunc)cli_remove_force_field);
    cli_register_command("list_fields", "列出所有力场", 
                       (CLICommandFunc)cli_list_force_fields);
}

// 命令实现
void cli_add_gravity_field(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: add_gravity <x> <y> <strength> [radius]\n");
        return;
    }
    
    kyVec3 pos = {atof(argv[1]), atof(argv[2]), 0};
    float strength = atof(argv[3]);
    float radius = argc > 4 ? atof(argv[4]) : 5.0f;
    
    ky_physics_add_gravity_field(current_world, pos, strength, radius);
    printf("Gravity field added at (%.1f, %.1f)\n", pos.x, pos.y);
}
```

---

**相关文档**：
- [ARCHITECTURE.md](../ARCHITECTURE.md) - 系统架构设计
- [Physics.md](../模块/Physics.md) - 物理引擎
- [ECS.md](./ECS.md) - 实体组件系统
- [Script.md](../模块/Script.md) - 脚本系统