# Scene (场景管理)

Kronyx Scene 系统提供高效的游戏场景管理和序列化功能，支持基于文本的场景定义和元数据管理。场景系统与 ECS 深度集成，为游戏世界提供结构化的数据组织和持久化能力。

## 概述

### 核心功能

- **场景序列化**: 将 ECS 世界保存为 `.ksn` 文本格式
- **场景加载**: 从 `.ksn` 文件重建游戏世界
- **元数据管理**: 为实体添加自定义标签和属性
- **场景模板**: 支持预定义的场景模板和复用

### 设计理念

场景系统遵循以下设计原则：

1. **数据驱动**: 场景数据与游戏逻辑分离
2. **文本格式**: 使用人类可读的文本格式便于版本控制
3. **增量加载**: 支持场景的增量加载和部分更新
4. **元数据丰富**: 为实体提供丰富的描述信息

## 数据结构

### 场景头信息
```c
typedef struct kySceneHeader {
    char magic[8];                     // "KRONYX_KSN"
    uint32_t version;                  // 格式版本
    uint32_t entity_count;            // 实体数量
    uint32_t timestamp;               // 创建时间戳
    char description[256];             // 场景描述
} kySceneHeader;
```

### 实体描述
```c
typedef struct kySceneEntity {
    uint32_t id;                      // 实体 ID
    uint32_t version;                 // 实体版本
    char name[64];                    // 实体名称
    char archetype_name[64];          // Archetype 名称
    uint32_t component_count;         // 组件数量
    kySceneComponent *components;     // 组件数据
    uint32_t metadata_count;          // 元数据数量
    kySceneMetadata *metadata;         // 元数据
} kySceneEntity;
```

### 组件数据
```c
typedef struct kySceneComponent {
    char type_name[64];               // 组件类型名称
    uint32_t property_count;          // 属性数量
    kySceneProperty *properties;       // 属性数组
} kySceneComponent;
```

### 属性定义
```c
typedef struct kySceneProperty {
    char key[64];                     // 属性键
    char value[256];                  // 属性值（字符串形式）
    int is_array;                     // 是否为数组
    uint32_t array_size;              // 数组大小（如果是数组）
    char **array_values;              // 数组值
} kySceneProperty;
```

### 元数据
```c
typedef struct kySceneMetadata {
    char key[64];                     // 元数据键
    char value[256];                  // 元数据值
} kySceneMetadata;
```

## 场景创建和序列化

### 1. 场景序列化

```c
// 将 ECS 世界序列化为场景文件
int ky_scene_serialize(kyWorld *w, const char *filename) {
    // 1. 收集场景信息
    kyArray entities;
    ky_array_create(&entities, sizeof(kySceneEntity), NULL);
    
    // 2. 遍历所有实体
    int alive_count = ky_world_alive_count(w);
    for (int i = 0; i < alive_count; i++) {
        kyEntity e = ky_world_get_alive_entity(w, i);
        if (!ky_entity_valid(w, e)) continue;
        
        // 3. 序列化实体
        kySceneEntity scene_entity = serialize_entity(w, e);
        ky_array_push(&entities, &scene_entity);
    }
    
    // 4. 写入文件
    int result = write_scene_file(filename, entities.data, entities.count);
    
    // 5. 清理
    ky_array_destroy(&entities);
    
    return result;
}

// 序列化单个实体
static kySceneEntity serialize_entity(kyWorld *w, kyEntity e) {
    kySceneEntity scene_entity = {0};
    scene_entity.id = e.id;
    scene_entity.version = e.version;
    
    // 获取实体名称（如果有）
    const char *name = ky_scene_get_metadata(w, e, "name");
    if (name) {
        strncpy(scene_entity.name, name, sizeof(scene_entity.name) - 1);
    }
    
    // 序列化组件
    uint32_t component_count = 0;
    kyArray components;
    ky_array_create(&components, sizeof(kySceneComponent), NULL);
    
    // 获取所有组件类型
    for (int i = 0; i < w->component_types.count; i++) {
        kyComponentType *ct = (kyComponentType*)w->component_types.data + i;
        if (ky_world_has_component(w, e, ct->type_id)) {
            kySceneComponent scene_comp = serialize_component(w, e, ct);
            ky_array_push(&components, &scene_comp);
            component_count++;
        }
    }
    
    scene_entity.component_count = component_count;
    scene_entity.components = (kySceneComponent*)components.data;
    
    // 序列化元数据
    uint32_t metadata_count = 0;
    kyArray metadata;
    ky_array_create(&metadata, sizeof(kySceneMetadata), NULL);
    
    // 收集所有元数据
    kyHashMapIterator iter;
    ky_hashmap_iter_begin(&w->name_cache, &iter);
    while (ky_hashmap_iter_next(&iter)) {
        const char *key = (const char*)iter.key;
        const char *value = (const char*)iter.value;
        
        if (strcmp(key, "name") != 0) {  // 跳过已处理的 name
            kySceneMeta scene_meta = {0};
            strncpy(scene_meta.key, key, sizeof(scene_meta.key) - 1);
            strncpy(scene_meta.value, value, sizeof(scene_meta.value) - 1);
            ky_array_push(&metadata, &scene_meta);
            metadata_count++;
        }
    }
    
    scene_entity.metadata_count = metadata_count;
    scene_entity.metadata = (kySceneMetadata*)metadata.data;
    
    return scene_entity;
}

// 序列化组件数据
static kySceneComponent serialize_component(kyWorld *w, kyEntity e, kyComponentType *ct) {
    kySceneComponent scene_comp = {0};
    strncpy(scene_comp.type_name, ct->name, sizeof(scene_comp.type_name) - 1);
    
    // 获取组件数据
    void *comp_data = ky_world_get_component(w, e, ct->type_id);
    
    // 根据组件类型序列化不同属性
    if (strcmp(ct->name, "transform") == 0) {
        serialize_transform(comp_data, &scene_comp);
    } else if (strcmp(ct->name, "sprite") == 0) {
        serialize_sprite(comp_data, &scene_comp);
    } else if (strcmp(ct->name, "camera") == 0) {
        serialize_camera(comp_data, &scene_comp);
    }
    // 其他组件类型的序列化...
    
    return scene_comp;
}

// 序列化 Transform 组件
static void serialize_transform(void *comp_data, kySceneComponent *scene_comp) {
    Transform *t = (Transform*)comp_data;
    
    // 添加位置属性
    kySceneProperty pos_prop = {"pos", 0};
    char pos_value[256];
    snprintf(pos_value, sizeof(pos_value), "%.2f,%.2f,%.2f", t->pos.x, t->pos.y, t->pos.z);
    strncpy(pos_prop.value, pos_value, sizeof(pos_prop.value) - 1);
    
    // 添加旋转属性
    kySceneProperty rot_prop = {"rotation", 0};
    char rot_value[256];
    snprintf(rot_value, sizeof(rot_value), "%.2f", t->rotation_z);
    strncpy(rot_prop.value, rot_value, sizeof(rot_prop.value) - 1);
    
    // 添加缩放属性
    kySceneProperty scale_prop = {"scale", 0};
    char scale_value[256];
    snprintf(scale_value, sizeof(scale_value), "%.2f,%.2f", t->scale.x, t->scale.y);
    strncpy(scale_prop.value, scale_value, sizeof(scale_prop.value) - 1);
    
    // 添加 Z 属性
    kySceneProperty z_prop = {"z", 0};
    char z_value[256];
    snprintf(z_value, sizeof(z_value), "%.2f", t->z);
    strncpy(z_prop.value, z_value, sizeof(z_prop.value) - 1);
    
    scene_comp->property_count = 4;
    scene_comp->properties = ky_alloc(4 * sizeof(kySceneProperty));
    memcpy(scene_comp->properties, &pos_prop, sizeof(kySceneProperty));
    memcpy(scene_comp->properties + 1, &rot_prop, sizeof(kySceneProperty));
    memcpy(scene_comp->properties + 2, &scale_prop, sizeof(kySceneProperty));
    memcpy(scene_comp->properties + 3, &z_prop, sizeof(kySceneProperty));
}
```

### 2. 场景文件格式

`.ksn` 场景文件使用基于文本的格式，支持版本控制和手动编辑：

```kson
# KRONYX_KSN v1
# 2D Platformer Level - Starting Area
# Created: 2026-01-20 10:30:00

# Entities
entity {
    id = 0
    version = 1
    name = "player"
    transform {
        pos = "0.0,0.0,0.0"
        rotation = "0.0"
        scale = "1.0,1.0"
        z = "0.0"
    }
    sprite {
        texture = "textures/player.png"
        color = "1.0,1.0,1.0,1.0"
        size = "1.0,1.0"
        layer = "0"
        flip = "0"
    }
    rigid_body {
        mass = "1.0"
        restitution = "0.0"
        friction = "0.1"
    }
}

entity {
    id = 1
    version = 1
    name = "ground_1"
    transform {
        pos = "-5.0,-2.0,0.0"
        rotation = "0.0"
        scale = "10.0,0.5,1.0"
        z = "-1.0"
    }
    sprite {
        texture = "textures/platform.png"
        color = "0.5,0.3,0.2,1.0"
        size = "10.0,0.5"
        layer = "-1"
        flip = "0"
    }
    rigid_body {
        mass = "0.0"
        restitution = "0.0"
        friction = "0.8"
    }
}

entity {
    id = 2
    version = 1
    name = "enemy_1"
    transform {
        pos = "3.0,2.0,0.0"
        rotation = "0.0"
        scale = "0.8,0.8,1.0"
        z = "0.0"
    }
    sprite {
        texture = "textures/enemy.png"
        color = "1.0,0.2,0.2,1.0"
        size = "0.8,0.8"
        layer = "0"
        flip = "0"
    }
    ai_component {
        patrol_range = "2.0"
        speed = "1.0"
        health = "50"
    }
}
```

## 场景加载和反序列化

### 1. 场景加载

```c
// 从文件加载场景
int ky_scene_deserialize(kyWorld *w, const char *filename) {
    // 1. 读取文件内容
    char *file_content = read_text_file(filename);
    if (!file_content) return -1;
    
    // 2. 解析场景头
    kySceneHeader header;
    if (!parse_scene_header(file_content, &header)) {
        ky_free(file_content);
        return -2;
    }
    
    // 3. 验证版本兼容性
    if (header.version != KY_SCENE_CURRENT_VERSION) {
        KY_LOG_ERROR("Scene version mismatch: expected %u, got %u", 
                     KY_SCENE_CURRENT_VERSION, header.version);
        ky_free(file_content);
        return -3;
    }
    
    // 4. 解析实体
    kyArray scene_entities;
    ky_array_create(&scene_entities, sizeof(kySceneEntity), NULL);
    
    if (!parse_scene_entities(file_content, &scene_entities)) {
        ky_free(file_content);
        ky_array_destroy(&scene_entities);
        return -4;
    }
    
    // 5. 创建实体和组件
    int created_count = 0;
    for (int i = 0; i < scene_entities.count; i++) {
        kySceneEntity *scene_entity = (kySceneEntity*)scene_entities.data + i;
        
        // 创建实体
        kyEntity e = ky_world_spawn(w);
        e.id = scene_entity->id;
        e.version = scene_entity->version;
        
        // 创建组件
        for (int j = 0; j < scene_entity->component_count; j++) {
            kySceneComponent *scene_comp = &scene_entity->components[j];
            
            // 查找组件类型
            uint32_t tid = ky_world_component_type_by_name(w, scene_comp->type_name);
            if (tid != KY_INVALID_COMPONENT_TYPE) {
                // 添加组件并加载数据
                void *comp_data = ky_world_add_component(w, e, tid);
                load_component_data(comp_data, scene_comp);
            }
        }
        
        // 设置元数据
        for (int j = 0; j < scene_entity->metadata_count; j++) {
            kySceneMetadata *meta = &scene_entity->metadata[j];
            ky_scene_set_metadata(w, e, meta->key, meta->value);
        }
        
        created_count++;
    }
    
    KY_LOG_INFO("Loaded scene '%s' with %d entities", filename, created_count);
    
    // 清理
    ky_free(file_content);
    ky_array_destroy(&scene_entities);
    
    return 0;
}

// 加载组件数据
static void load_component_data(void *comp_data, kySceneComponent *scene_comp) {
    for (int i = 0; i < scene_comp->property_count; i++) {
        kySceneProperty *prop = &scene_comp->properties[i];
        
        // 根据属性类型加载不同数据
        if (strcmp(prop->key, "pos") == 0) {
            kyVec3 *pos = (kyVec3*)((char*)comp_data + offsetof(Transform, pos));
            sscanf(prop->value, "%f,%f,%f", &pos->x, &pos->y, &pos->z);
        } else if (strcmp(prop->key, "rotation") == 0) {
            float *rot = (float*)((char*)comp_data + offsetof(Transform, rotation_z));
            sscanf(prop->value, "%f", rot);
        } else if (strcmp(prop->key, "scale") == 0) {
            kyVec2 *scale = (kyVec2*)((char*)comp_data + offsetof(Transform, scale));
            sscanf(prop->value, "%f,%f", &scale->x, &scale->y);
        }
        // 其他属性加载...
    }
}
```

### 2. 场景解析器

```c
// 场景解析器状态
typedef struct SceneParser {
    const char *content;
    size_t pos;
    size_t len;
    int line;
    int column;
} SceneParser;

// 解析场景头
static int parse_scene_header(const char *content, kySceneHeader *header) {
    SceneParser parser = {content, 0, strlen(content), 1, 1};
    
    // 检查魔数
    if (!parse_string(&parser, header->magic, 8)) {
        KY_LOG_ERROR("Invalid scene header magic");
        return 0;
    }
    
    if (strncmp(header->magic, "KRONYX_KSN", 8) != 0) {
        KY_LOG_ERROR("Not a Kronyx scene file");
        return 0;
    }
    
    // 解析版本
    if (!parse_line(&parser)) {
        KY_LOG_ERROR("Failed to parse scene version");
        return 0;
    }
    
    // 解析描述行
    if (!parse_line(&parser)) {
        KY_LOG_ERROR("Failed to parse scene description");
        return 0;
    }
    
    return 1;
}

// 解析实体块
static int parse_entities(SceneParser *parser, kyArray *entities) {
    while (parser->pos < parser->len) {
        if (strncmp(parser->content + parser->pos, "entity", 6) == 0) {
            // 解析实体
            kySceneEntity entity = {0};
            if (!parse_entity(parser, &entity)) {
                KY_LOG_ERROR("Failed to parse entity at line %d", parser->line);
                return 0;
            }
            ky_array_push(entities, &entity);
        } else if (parser->content[parser->pos] == '#') {
            // 跳过注释
            skip_line(parser);
        } else if (parser->content[parser->pos] == '\0') {
            break;
        } else {
            // 跳过空白字符
            skip_whitespace(parser);
        }
    }
    
    return 1;
}

// 解析单个实体
static int parse_entity(SceneParser *parser, kySceneEntity *entity) {
    // 解析实体块
    if (!expect_token(parser, "{")) {
        return 0;
    }
    
    while (parser->pos < parser->len && parser->content[parser->pos] != '}') {
        char token[64];
        if (!parse_token(parser, token, sizeof(token))) {
            return 0;
        }
        
        if (strcmp(token, "id") == 0) {
            if (!expect_token(parser, "=")) return 0;
            parse_integer(parser, (int*)&entity->id);
        } else if (strcmp(token, "version") == 0) {
            if (!expect_token(parser, "=")) return 0;
            parse_integer(parser, (int*)&entity->version);
        } else if (strcmp(token, "name") == 0) {
            if (!expect_token(parser, "=")) return 0;
            parse_string(parser, entity->name, sizeof(entity->name));
        } else if (strcmp(token, "transform") == 0) {
            // 解析组件块
            kySceneComponent component = {0};
            strcpy(component.type_name, "transform");
            parse_component(parser, &component);
            
            entity->component_count = 1;
            entity->components = ky_alloc(sizeof(kySceneComponent));
            memcpy(entity->components, &component, sizeof(kySceneComponent));
        }
        // 其他组件解析...
        
        skip_whitespace(parser);
    }
    
    if (!expect_token(parser, "}")) {
        return 0;
    }
    
    return 1;
}
```

## 场景元数据管理

### 1. 元数据操作

```c
// 设置实体元数据
void ky_scene_set_metadata(kyWorld *w, kyEntity e, const char *key, const char *value) {
    // 在世界级别的元数据存储中设置
    char full_key[128];
    snprintf(full_key, sizeof(full_key), "%u:%s", e.id, key);
    
    ky_string_hashmap_set(&w->metadata_map, full_key, value);
}

// 获取实体元数据
const char *ky_scene_get_metadata(kyWorld *w, kyEntity e, const char *key) {
    char full_key[128];
    snprintf(full_key, sizeof(full_key), "%u:%s", e.id, key);
    
    return ky_string_hashmap_get(&w->metadata_map, full_key);
}

// 删除实体元数据
void ky_scene_remove_metadata(kyWorld *w, kyEntity e, const char *key) {
    char full_key[128];
    snprintf(full_key, sizeof(full_key), "%u:%s", e.id, key);
    
    ky_string_hashmap_remove(&w->metadata_map, full_key);
}

// 获取所有元数据
void ky_scene_get_all_metadata(kyWorld *w, kyEntity e, kyArray *metadata) {
    char prefix[32];
    snprintf(prefix, sizeof(prefix), "%u:", e.id);
    size_t prefix_len = strlen(prefix);
    
    kyHashMapIterator iter;
    ky_hashmap_iter_begin(&w->metadata_map, &iter);
    while (ky_hashmap_iter_next(&iter)) {
        const char *key = (const char*)iter.key;
        
        // 检查是否是该实体的元数据
        if (strncmp(key, prefix, prefix_len) == 0) {
            kySceneMeta meta = {0};
            strncpy(meta.key, key + prefix_len, sizeof(meta.key) - 1);
            strncpy(meta.value, (const char*)iter.value, sizeof(meta.value) - 1);
            ky_array_push(metadata, &meta);
        }
    }
}
```

### 2. 场景模板

```c
// 场景模板定义
typedef struct kySceneTemplate {
    char name[64];                    // 模板名称
    char description[256];            // 模板描述
    kyArray entity_templates;          // 实体模板
} kySceneTemplate;

// 实体模板
typedef struct kyEntityTemplate {
    char name[64];                    // 实体名称
    uint32_t archetype_id;            // Archetype ID
    uint32_t component_count;         // 组件数量
    kyComponentTemplate *components;  // 组件模板
    uint32_t metadata_count;          // 元数据数量
    kySceneMetadata *metadata;        // 元数据
} kyEntityTemplate;

// 组件模板
typedef struct kyComponentTemplate {
    char type_name[64];               // 组件类型
    uint32_t property_count;          // 属性数量
    kySceneProperty *properties;      // 属性模板
} kyComponentTemplate;

// 创建场景模板
kySceneTemplate* scene_template_create(const char *name, const char *description) {
    kySceneTemplate *template = ky_alloc(sizeof(kySceneTemplate));
    strncpy(template->name, name, sizeof(template->name) - 1);
    strncpy(template->description, description, sizeof(template->description) - 1);
    ky_array_create(&template->entity_templates, sizeof(kyEntityTemplate), NULL);
    return template;
}

// 添加实体模板
void scene_template_add_entity(kySceneTemplate *template, const char *entity_name,
                             uint32_t archetype_id, kyComponentTemplate *components,
                             int component_count, kySceneMetadata *metadata,
                             int metadata_count) {
    kyEntityTemplate entity_template = {0};
    strncpy(entity_template.name, entity_name, sizeof(entity_template.name) - 1);
    entity_template.archetype_id = archetype_id;
    entity_template.component_count = component_count;
    entity_template.components = components;
    entity_template.metadata_count = metadata_count;
    entity_template.metadata = metadata;
    
    ky_array_push(&template->entity_templates, &entity_template);
}

// 使用模板创建场景
int scene_template_instantiate(kyWorld *w, kySceneTemplate *template, const char *scene_name) {
    KY_LOG_INFO("Instantiating scene template: %s", template->name);
    
    for (int i = 0; i < template->entity_templates.count; i++) {
        kyEntityTemplate *entity_template = 
            (kyEntityTemplate*)template->entity_templates.data + i;
        
        // 创建实体
        kyEntity e = ky_world_spawn(w);
        
        // 添加组件
        for (int j = 0; j < entity_template->component_count; j++) {
            kyComponentTemplate *comp_template = &entity_template->components[j];
            
            uint32_t tid = ky_world_component_type_by_name(w, comp_template->type_name);
            if (tid != KY_INVALID_COMPONENT_TYPE) {
                void *comp_data = ky_world_add_component(w, e, tid);
                load_component_data(comp_data, comp_template);
            }
        }
        
        // 设置元数据
        for (int j = 0; j < entity_template->metadata_count; j++) {
            kySceneMetadata *meta = &entity_template->metadata[j];
            ky_scene_set_metadata(w, e, meta->key, meta->value);
        }
    }
    
    KY_LOG_INFO("Scene template '%s' instantiated with %d entities", 
                template->name, template->entity_templates.count);
    return 0;
}
```

## 场景管理工具

### 1. 场景编辑器接口

```c
// 场景编辑器命令
typedef enum SceneCommand {
    SCENE_CMD_CREATE_ENTITY,
    SCENE_CMD_DELETE_ENTITY,
    SCENE_CMD_ADD_COMPONENT,
    SCENE_CMD_REMOVE_COMPONENT,
    SCENE_CMD_SET_PROPERTY,
    SCENE_CMD_SET_METADATA,
    SCENE_CMD_LOAD_SCENE,
    SCENE_CMD_SAVE_SCENE,
    SCENE_CMD_DUPLICATE_ENTITY
} SceneCommand;

// 场景编辑器上下文
typedef struct SceneEditorContext {
    kyWorld *world;                   // 当前编辑的世界
    kySceneTemplate *current_template; // 当前模板
    kyArray undo_stack;              // 撤销栈
    kyArray redo_stack;              // 重做栈
    int selected_entity;             // 选中的实体
} SceneEditorContext;

// 执行场景命令
int scene_editor_execute_command(SceneEditorContext *ctx, SceneCommand cmd, void *data) {
    switch (cmd) {
        case SCENE_CMD_CREATE_ENTITY: {
            SceneCreateData *create_data = (SceneCreateData*)data;
            kyEntity e = ky_world_spawn(ctx->world);
            add_to_undo_stack(ctx, SCENE_CMD_DELETE_ENTITY, &e);
            return 0;
        }
        
        case SCENE_CMD_DELETE_ENTITY: {
            kyEntity *entity = (kyEntity*)data;
            // 从场景中删除实体
            ky_world_despawn(ctx->world, *entity);
            return 0;
        }
        
        case SCENE_CMD_ADD_COMPONENT: {
            SceneAddComponentData *add_data = (SceneAddComponentData*)data;
            uint32_t tid = ky_world_component_type_by_name(ctx->world, add_data->component_type);
            if (tid != KY_INVALID_COMPONENT_TYPE) {
                void *comp = ky_world_add_component(ctx->world, add_data->entity, tid);
                add_to_undo_stack(ctx, SCENE_CMD_REMOVE_COMPONENT, add_data);
                return 0;
            }
            return -1;
        }
        
        // 其他命令实现...
        
        default:
            KY_LOG_ERROR("Unknown scene command: %d", cmd);
            return -1;
    }
}
```

### 2. 场景预览和调试

```c
// 场景预览
void scene_preview(kyWorld *world, kyRenderDevice *rd) {
    // 创建预览相机
    kyEntity preview_camera = ky_world_spawn(world);
    kyCamera2D *cam = ky_world_add_component(world, preview_camera, ky_camera2d_type);
    cam->viewport = ky_vec2(16.0f, 9.0f);
    cam->zoom = 1.0f;
    cam->clear_color = (kyVec4){0.2f, 0.2f, 0.2f, 1.0f};
    cam->active = 1;
    
    // 渲染场景
    int sprite_count = ky2d_render_world_auto(rd, world);
    KY_LOG_INFO("Preview rendered %d sprites", sprite_count);
}

// 场景调试信息
void scene_debug_info(kyWorld *world) {
    KY_LOG_DEBUG("=== Scene Debug Info ===");
    
    int entity_count = ky_world_alive_count(world);
    KY_LOG_DEBUG("Total entities: %d", entity_count);
    
    // 统计各类型实体
    kyHashMap entity_type_counts;
    ky_hashmap_create(&entity_type_counts, sizeof(uint32_t), sizeof(uint32_t), NULL);
    
    for (int i = 0; i < entity_count; i++) {
        kyEntity e = ky_world_get_alive_entity(world, i);
        if (!ky_entity_valid(world, e)) continue;
        
        // 计算实体组件类型的哈希
        uint32_t type_hash = calculate_entity_type_hash(world, e);
        
        // 统计
        uint32_t *count = (uint32_t*)ky_hashmap_get(&entity_type_counts, &type_hash);
        if (count) {
            (*count)++;
        } else {
            uint32_t new_count = 1;
            ky_hashmap_set(&entity_type_counts, &type_hash, &new_count);
        }
    }
    
    // 输出统计结果
    kyHashMapIterator iter;
    ky_hashmap_iter_begin(&entity_type_counts, &iter);
    while (ky_hashmap_iter_next(&iter)) {
        uint32_t type_hash = *(uint32_t*)iter.key;
        uint32_t count = *(uint32_t*)iter.value;
        
        KY_LOG_DEBUG("Entity type 0x%08x: %d instances", type_hash, count);
    }
    
    ky_hashmap_destroy(&entity_type_counts);
}

// 计算实体类型哈希
static uint32_t calculate_entity_type_hash(kyWorld *world, kyEntity e) {
    uint32_t hash = 0;
    
    // 遍历所有组件类型
    for (int i = 0; i < world->component_types.count; i++) {
        kyComponentType *ct = (kyComponentType*)world->component_types.data + i;
        if (ky_world_has_component(world, e, ct->type_id)) {
            hash = hash * 31 + ct->type_id;
        }
    }
    
    return hash;
}
```

## 性能优化

### 1. 增量加载

```c
// 增量场景加载
int scene_load_incremental(kyWorld *w, const char *filename, 
                          kyEntity start_entity, int entity_count) {
    FILE *file = fopen(filename, "r");
    if (!file) return -1;
    
    char line[512];
    int loaded_count = 0;
    int current_entity = 0;
    
    while (fgets(line, sizeof(line), file) && loaded_count < entity_count) {
        if (strncmp(line, "entity", 6) == 0) {
            if (current_entity >= start_entity) {
                // 解析并加载实体
                kyEntity e = scene_parse_and_load_entity(w, file);
                if (ky_entity_valid(w, e)) {
                    loaded_count++;
                }
            }
            current_entity++;
        }
    }
    
    fclose(file);
    KY_LOG_INFO("Incrementally loaded %d entities", loaded_count);
    return loaded_count;
}
```

### 2. 场景缓存

```c
// 场景缓存
typedef struct SceneCache {
    kyHashMap scene_cache;          // 场景数据缓存
    kyArray lru_timestamps;         // LRU 时间戳
    size_t max_cache_size;         // 最大缓存大小
    uint64_t current_time;         // 当前时间
} SceneCache;

// 场景缓存管理
void scene_cache_init(SceneCache *cache, size_t max_size) {
    ky_hashmap_create(&cache->scene_cache, sizeof(char*), sizeof(kySceneData*), NULL);
    ky_array_create(&cache->lru_timestamps, sizeof(uint64_t), NULL);
    cache->max_cache_size = max_size;
    cache->current_time = 0;
}

// 从缓存加载场景
kySceneData* scene_cache_load(SceneCache *cache, const char *filename) {
    // 检查缓存
    kySceneData *scene = (kySceneData*)ky_hashmap_get(&cache->scene_cache, &filename);
    if (scene) {
        // 更新 LRU 时间戳
        update_lru_timestamp(cache, filename);
        return scene;
    }
    
    // 加载场景
    scene = scene_load_from_disk(filename);
    if (scene) {
        // 添加到缓存
        if (cache->scene_cache.count >= cache->max_cache_size) {
            evict_lru_cache(cache);
        }
        
        ky_hashmap_set(&cache->scene_cache, &filename, &scene);
        update_lru_timestamp(cache, filename);
    }
    
    return scene;
}

// 更新 LRU 时间戳
static void update_lru_timestamp(SceneCache *cache, const char *filename) {
    cache->current_time++;
    
    // 查找现有时间戳
    for (int i = 0; i < cache->lru_timestamps.count; i++) {
        if (strcmp((char**)cache->scene_cache.keys + i, filename) == 0) {
            *(uint64_t*)ky_array_get(&cache->lru_timestamps, i) = cache->current_time;
            return;
        }
    }
    
    // 添加新时间戳
    ky_array_push(&cache->lru_timestamps, &cache->current_time);
}
```

## 最佳实践

### 1. 场景设计原则

```c
// 1. 分层场景设计
typedef struct LayeredScene {
    kyWorld *game_world;            // 游戏逻辑世界
    kyWorld *render_world;          // 渲染世界
    kyWorld *physics_world;         // 物理世界
} LayeredScene;

// 2. 场景实例管理
typedef struct SceneManager {
    kyHashMap scene_cache;          // 场景缓存
    kyArray active_scenes;          // 活跃场景列表
    kyArray scene_dependencies;     // 场景依赖关系
} SceneManager;

// 3. 场景预加载
void preload_scene_dependencies(SceneManager *sm, const char *scene_path) {
    // 分析场景依赖并预加载
    kyScene *scene = scene_load(sm, scene_path);
    
    for (int i = 0; i < scene->dependencies.count; i++) {
        char *dep_path = (char*)scene->dependencies.data + i;
        if (!scene_is_loaded(sm, dep_path)) {
            preload_scene(sm, dep_path);
        }
    }
}
```

### 2. 场景调试工具

```c
// 场景调试工具
void scene_debug_tool(kyWorld *world) {
    if (!get_debug_flag(SCENE_DEBUG_ENABLED)) return;
    
    // 显示场景统计
    scene_debug_info(world);
    
    // 显示实体详细信息
    for (int i = 0; i < ky_world_alive_count(world); i++) {
        kyEntity e = ky_world_get_alive_entity(world, i);
        if (!ky_entity_valid(world, e)) continue;
        
        // 绘制实体轮廓
        debug_draw_entity_bounds(world, e);
        
        // 显示实体信息
        debug_draw_entity_info(world, e);
    }
}

// 场景性能分析
void scene_performance_analyzer(kyWorld *world) {
    static uint64_t frame_count = 0;
    frame_count++;
    
    if (frame_count % 60 == 0) {  // 每秒分析一次
        analyze_scene_performance(world);
    }
}

static void analyze_scene_performance(kyWorld *world) {
    KY_LOG_INFO("=== Scene Performance Analysis ===");
    
    // 分析 archetype 分布
    analyze_archetype_distribution(world);
    
    // 分析组件访问模式
    analyze_component_access_patterns(world);
    
    // 分析内存使用
    analyze_scene_memory_usage(world);
}
```

## 总结

Kronyx Scene 系统为游戏开发提供了强大的场景管理能力：

### 核心优势

1. **文本格式**: 基于 `.ksn` 的文本格式便于版本控制和手工编辑
2. **增量加载**: 支持场景的增量加载和部分更新
3. **元数据丰富**: 为实体提供丰富的描述信息和标签
4. **模板系统**: 支持场景模板的创建和复用

### 实际应用

1. **关卡设计**: 使用场景文件定义游戏关卡和对象布局
2. **对象管理**: 通过元数据管理游戏对象属性和行为
3. **场景模板**: 创建可复用的场景模板（如 UI、游戏模式等）
4. **调试工具**: 提供场景预览、调试信息和性能分析功能

### 设计建议

1. **合理分层**: 将逻辑、渲染、物理等不同层面分离
2. **缓存优化**: 使用场景缓存提高加载性能
3. **依赖管理**: 合理组织场景依赖关系
4. **调试支持**: 提供丰富的调试工具和可视化功能

通过合理使用 Scene 系统，开发者可以构建灵活、高效的游戏场景管理方案。