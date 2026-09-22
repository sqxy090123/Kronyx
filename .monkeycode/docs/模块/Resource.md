# Resource (资源管理)

Kronyx Resource 系统提供统一的资源管理和加载机制，支持多种资源类型和自动内存管理。系统采用引用计数模式，确保资源在不再被使用时自动释放，同时支持资源缓存和异步加载。

## 概述

### 核心功能

- **统一资源接口**: 支持纹理、音频、模型等多种资源类型
- **引用计数管理**: 自动资源释放和内存管理
- **资源缓存**: 提供缓存机制，避免重复加载
- **异步加载**: 支持后台资源加载
- **内存池化**: 优化的内存分配策略

### 设计理念

Resource 系统遵循以下设计原则：

1. **类型无关**: 统一的资源接口，支持不同类型的资源
2. **自动管理**: 基于引用计数的自动资源生命周期管理
3. **高性能**: 缓存和内存池优化
4. **可扩展**: 易于添加新的资源类型

## 数据结构

### 资源描述符
```c
typedef enum kyResourceType {
    KY_RESOURCE_TEXTURE = 0,
    KY_RESOURCE_AUDIO,
    KY_RESOURCE_MODEL,
    KY_RESOURCE_SHADER,
    KY_RESOURCE_RAW,
    KY_RESOURCE_COUNT
} kyResourceType;

typedef struct kyResource {
    kyResourceType type;             // 资源类型
    char path[256];                  // 资源路径
    void *payload;                   // 资源数据指针
    size_t payload_size;             // 资源数据大小
    void (*on_destroy)(void *payload); // 资源销毁回调
    uint32_t ref_count;              // 引用计数
    uint64_t timestamp;              // 加载时间戳
    uint32_t flags;                 // 资源标志
} kyResource;
```

### 资源管理器
```c
typedef struct kyResourceManager {
    kyAllocator alloc;               // 内存分配器
    kyArray resources;               // 资源数组
    kyHashMap path_map;             // 路径到资源的映射
    kyHashMap type_map;             // 类型到资源的映射
    kyArray loading_queue;           // 加载队列
    kyArray loading_resources;       // 正在加载的资源
    uint32_t max_cache_size;         // 最大缓存大小
    uint64_t total_memory;           // 已用内存
    uint32_t load_flags;            // 加载标志
} kyResourceManager;
```

### 资源回调
```c
// 资源加载完成回调
typedef void (*ResourceLoadComplete)(kyResource *resource, int success, void *user_data);

// 资源进度回调
typedef void (*ResourceProgress)(float progress, void *user_data);
```

## 资源创建和管理

### 1. 资源管理器创建

```c
kyResourceManager *ky_resmgr_create(kyAllocator *alloc) {
    kyResourceManager *rm = ky_alloc(sizeof(kyResourceManager));
    
    // 初始化分配器
    rm->alloc = alloc ? *alloc : ky_default_allocator();
    
    // 初始化容器
    ky_array_create(&rm->resources, sizeof(kyResource), &rm->alloc);
    ky_hashmap_create(&rm->path_map, sizeof(char*), sizeof(kyResource*), &rm->alloc);
    ky_hashmap_create(&rm->type_map, sizeof(uint32_t), 
                      sizeof(kyArray), &rm->alloc);
    ky_array_create(&rm->loading_queue, sizeof(kyResource*), &rm->alloc);
    ky_array_create(&rm->loading_resources, sizeof(kyResource*), &rm->alloc);
    
    // 初始化配置
    rm->max_cache_size = KY_DEFAULT_CACHE_SIZE;
    rm->total_memory = 0;
    rm->load_flags = KY_LOAD_FLAG_SYNC;
    
    return rm;
}

void ky_resmgr_destroy(kyResourceManager *rm) {
    // 销毁所有资源
    for (int i = 0; i < rm->resources.count; i++) {
        kyResource *res = (kyResource*)rm->resources.data + i;
        ky_resmgr_release(rm, res);
    }
    
    // 清理容器
    ky_array_destroy(&rm->resources);
    ky_hashmap_destroy(&rm->path_map);
    ky_hashmap_destroy(&rm->type_map);
    ky_array_destroy(&rm->loading_queue);
    ky_array_destroy(&rm->loading_resources);
    
    ky_free(rm);
}
```

### 2. 资源创建

```c
// 创建像素缓冲区资源
kyResource *ky_resmgr_make_pixelbuffer(kyResourceManager *rm, 
                                      int w, int h, int channels, 
                                      const void *pixels) {
    // 验证输入
    if (w <= 0 || h <= 0 || channels < 1 || channels > 4) {
        KY_LOG_ERROR("Invalid pixel buffer dimensions: %dx%d, %d channels", w, h, channels);
        return NULL;
    }
    
    // 计算内存大小
    size_t pixel_size = w * h * channels;
    size_t total_size = pixel_size;
    
    // 分配内存
    uint8_t *pixel_data = ky_alloc(total_size);
    memcpy(pixel_data, pixels, pixel_size);
    
    // 创建资源
    kyResource resource = {0};
    resource.type = KY_RESOURCE_TEXTURE;
    resource.payload = pixel_data;
    resource.payload_size = total_size;
    resource.ref_count = 1;
    resource.timestamp = ky_time_ticks();
    
    // 设置销毁回调
    resource.on_destroy = pixelbuffer_destroy;
    
    // 添加到管理器
    return ky_resmgr_add_resource(rm, &resource);
}

// 原始字节数据资源
kyResource *ky_resmgr_make_raw_bytes(kyResourceManager *rm, size_t size, const void *data) {
    if (size == 0) {
        KY_LOG_ERROR("Raw resource size cannot be zero");
        return NULL;
    }
    
    // 复制数据
    void *payload_data = ky_alloc(size);
    memcpy(payload_data, data, size);
    
    // 创建资源
    kyResource resource = {0};
    resource.type = KY_RESOURCE_RAW;
    resource.payload = payload_data;
    resource.payload_size = size;
    resource.ref_count = 1;
    resource.timestamp = ky_time_ticks();
    
    // 设置销毁回调
    resource.on_destroy = raw_bytes_destroy;
    
    return ky_resmgr_add_resource(rm, &resource);
}

// 添加资源到管理器
static kyResource *ky_resmgr_add_resource(kyResourceManager *rm, kyResource *resource) {
    // 检查缓存限制
    if (rm->total_memory + resource->payload_size > rm->max_cache_size) {
        // 尝试清理缓存
        if (!ky_resmgr_evict_cache(rm, resource->payload_size)) {
            KY_LOG_ERROR("Resource too large for cache");
            if (resource->on_destroy) {
                resource->on_destroy(resource->payload);
            }
            ky_free(resource);
            return NULL;
        }
    }
    
    // 添加到资源数组
    ky_array_push(&rm->resources, resource);
    
    // 添加到路径映射
    char *path_copy = ky_strdup(resource->path, &rm->alloc);
    ky_hashmap_set(&rm->path_map, &path_copy, resource);
    
    // 添加到类型映射
    kyArray *type_array = (kyArray*)ky_hashmap_get(&rm->type_map, &resource->type);
    if (!type_array) {
        type_array = ky_alloc(sizeof(kyArray));
        ky_array_create(type_array, sizeof(kyResource*), &rm->alloc);
        ky_hashmap_set(&rm->type_map, &resource->type, type_array);
    }
    ky_array_push(type_array, resource);
    
    // 更新内存统计
    rm->total_memory += resource->payload_size;
    
    return resource;
}
```

### 3. 资源获取和释放

```c
// 获取资源
kyResource *ky_resmgr_get(kyResourceManager *rm, const char *path) {
    // 检查缓存
    kyResource *resource = (kyResource*)ky_hashmap_get(&rm->path_map, &path);
    if (resource) {
        // 增加引用计数
        resource->ref_count++;
        KY_LOG_DEBUG("Resource '%s' ref count: %d", path, resource->ref_count);
        return resource;
    }
    
    // 资源不存在，需要加载
    if (rm->load_flags & KY_LOAD_FLAG_AUTO_LOAD) {
        return ky_resmgr_load(rm, path);
    }
    
    return NULL;
}

// 释放资源引用
void ky_resmgr_release(kyResourceManager *rm, kyResource *resource) {
    if (!resource) return;
    
    // 减少引用计数
    resource->ref_count--;
    KY_LOG_DEBUG("Resource '%s' ref count: %d", resource->path, resource->ref_count);
    
    // 如果引用计数为 0，销毁资源
    if (resource->ref_count == 0) {
        ky_resmgr_destroy_resource(rm, resource);
    }
}

// 销毁资源
void ky_resmgr_destroy_resource(kyResourceManager *rm, kyResource *resource) {
    if (!resource) return;
    
    // 调用销毁回调
    if (resource->on_destroy) {
        resource->on_destroy(resource->payload);
        resource->payload = NULL;
    }
    
    // 从映射中移除
    ky_hashmap_remove(&rm->path_map, &resource->path);
    
    // 从类型映射中移除
    kyArray *type_array = (kyArray*)ky_hashmap_get(&rm->type_map, &resource->type);
    if (type_array) {
        for (int i = 0; i < type_array->count; i++) {
            if (*(kyResource**)ky_array_get(type_array, i) == resource) {
                ky_array_remove_at(type_array, i);
                break;
            }
        }
        
        // 如果类型数组为空，清理它
        if (type_array->count == 0) {
            ky_array_destroy(type_array);
            ky_free(type_array);
            ky_hashmap_remove(&rm->type_map, &resource->type);
        }
    }
    
    // 更新内存统计
    rm->total_memory -= resource->payload_size;
    
    // 从资源数组中移除
    for (int i = 0; i < rm->resources.count; i++) {
        if ((kyResource*)rm->resources.data + i == resource) {
            ky_array_remove_at(&rm->resources, i);
            break;
        }
    }
    
    // 释放资源结构
    ky_free(resource->path);
    ky_free(resource);
}
```

## 纹理资源管理

### 1. 纹理创建

```c
// 从像素数据创建纹理
kyTexture *ky_resmgr_load_texture(kyResourceManager *rm, kyRenderDevice *rd, 
                                 const char *path) {
    // 获取或创建资源
    kyResource *res = ky_resmgr_get(rm, path);
    if (!res) {
        return NULL;
    }
    
    // 如果资源已经是纹理，直接返回
    if (res->type == KY_RESOURCE_TEXTURE && res->payload) {
        return (kyTexture*)res->payload;
    }
    
    // 需要从像素数据创建纹理
    if (res->type == KY_RESOURCE_RAW) {
        // 解析纹理信息
        TextureInfo info = parse_texture_info(path);
        if (!info.valid) {
            ky_resmgr_release(rm, res);
            return NULL;
        }
        
        // 创建纹理
        kyTexture *texture = ky_rd_create_texture_2d(rd, info.width, info.height, 
                                                  info.channels, res->payload);
        
        // 更新资源
        if (texture) {
            res->payload = texture;
            res->type = KY_RESOURCE_TEXTURE;
            res->on_destroy = texture_destroy;
        }
        
        ky_resmgr_release(rm, res);
        return texture;
    }
    
    ky_resmgr_release(rm, res);
    return NULL;
}

// 纹理信息解析
typedef struct TextureInfo {
    int width;
    int height;
    int channels;
    int valid;
} TextureInfo;

static TextureInfo parse_texture_info(const char *path) {
    TextureInfo info = {0};
    info.valid = 1;
    
    // 根据文件扩展名解析
    const char *ext = strrchr(path, '.');
    if (!ext) {
        info.valid = 0;
        return info;
    }
    
    // 基础信息（实际实现需要根据具体格式解析）
    if (strcmp(ext, ".png") == 0) {
        info.width = 256;  // 示例值
        info.height = 256;
        info.channels = 4;
    } else if (strcmp(ext, ".jpg") == 0) {
        info.width = 512;
        info.height = 512;
        info.channels = 3;
    } else {
        info.valid = 0;
    }
    
    return info;
}
```

### 2. 纹理缓存

```c
// 纹理缓存优化
typedef struct TextureCache {
    kyArray textures;                // 纹理数组
    kyHashMap texture_map;          // 路径映射
    uint32_t max_textures;          // 最大纹理数量
    uint32_t current_textures;      // 当前纹理数量
    kyArray lru_list;               // LRU 列表
} TextureCache;

// 纹理缓存初始化
void texture_cache_init(TextureCache *cache, uint32_t max_textures) {
    cache->max_textures = max_textures;
    cache->current_textures = 0;
    
    ky_array_create(&cache->textures, sizeof(kyTexture*), NULL);
    ky_hashmap_create(&cache->texture_map, sizeof(char*), sizeof(kyTexture*), NULL);
    ky_array_create(&cache->lru_list, sizeof(uint64_t), NULL);
}

// 获取缓存的纹理
kyTexture *texture_cache_get(TextureCache *cache, const char *path) {
    kyTexture *texture = (kyTexture*)ky_hashmap_get(&cache->texture_map, &path);
    if (texture) {
        // 更新 LRU
        texture_cache_update_lru(cache, texture);
    }
    return texture;
}

// 添加纹理到缓存
void texture_cache_add(TextureCache *cache, const char *path, kyTexture *texture) {
    if (cache->current_textures >= cache->max_textures) {
        // 移除最久未使用的纹理
        texture_cache_evict_lru(cache);
    }
    
    // 添加到缓存
    char *path_copy = ky_strdup(path, NULL);
    ky_hashmap_set(&cache->texture_map, &path_copy, texture);
    
    ky_array_push(&cache->textures, &texture);
    cache->current_textures++;
    
    texture_cache_update_lru(cache, texture);
}

// 更新 LRU
static void texture_cache_update_lru(TextureCache *cache, kyTexture *texture) {
    // 查找并移除现有条目
    for (int i = 0; i < cache->lru_list.count; i++) {
        if (*(uint64_t*)ky_array_get(&cache->lru_list, i) == (uint64_t)texture) {
            ky_array_remove_at(&cache->lru_list, i);
            break;
        }
    }
    
    // 添加到列表头部
    uint64_t ptr = (uint64_t)texture;
    ky_array_push(&cache->lru_list, &ptr);
}
```

## 内存管理

### 1. 资源清理

```c
// 清理缓存
int ky_resmgr_evict_cache(kyResourceManager *rm, size_t required_size) {
    if (rm->total_memory - required_size <= 0) {
        return 0;  // 没有足够空间
    }
    
    // 按最近最少使用策略清理
    kyArray *lru_list = ky_array_create(sizeof(kyResource*), &rm->alloc);
    
    // 创建 LRU 列表
    for (int i = 0; i < rm->resources.count; i++) {
        kyResource *res = (kyResource*)rm->resources.data + i;
        if (res->ref_count == 0) {  // 只清理未引用的资源
            ky_array_push(lru_list, &res);
        }
    }
    
    // 按时间戳排序（旧到新）
    qsort(lru_list->data, lru_list->count, sizeof(kyResource*), 
          compare_resource_timestamp);
    
    // 清理直到有足够空间
    size_t freed = 0;
    for (int i = 0; i < lru_list->count && freed < required_size; i++) {
        kyResource *res = *(kyResource**)ky_array_get(lru_list, i);
        
        // 销毁资源
        ky_resmgr_destroy_resource(rm, res);
        freed += res->payload_size;
    }
    
    ky_array_destroy(lru_list);
    KY_LOG_INFO("Freed %zu bytes from cache", freed);
    
    return 1;
}

// 比较资源时间戳
static int compare_resource_timestamp(const void *a, const void *b) {
    kyResource *res_a = *(kyResource**)a;
    kyResource *res_b = *(kyResource**)b;
    
    if (res_a->timestamp < res_b->timestamp) return -1;
    if (res_a->timestamp > res_b->timestamp) return 1;
    return 0;
}

// 销毁所有资源
void ky_resmgr_destroy_all(kyResourceManager *rm) {
    // 复制资源数组以避免迭代时修改
    kyArray resources_copy;
    ky_array_create(&resources_copy, sizeof(kyResource*), &rm->alloc);
    
    for (int i = 0; i < rm->resources.count; i++) {
        kyResource *res = (kyResource*)rm->resources.data + i;
        ky_array_push(&resources_copy, &res);
    }
    
    // 销毁所有资源
    for (int i = 0; i < resources_copy.count; i++) {
        kyResource *res = *(kyResource**)ky_array_get(resources_copy, i);
        ky_resmgr_destroy_resource(rm, res);
    }
    
    ky_array_destroy(&resources_copy);
}
```

### 2. 内存统计

```c
// 资源统计信息
typedef struct ResourceStats {
    uint32_t total_resources;       // 总资源数
    uint32_t active_resources;      // 活跃资源数
    size_t total_memory;            // 总内存使用
    uint32_t resources_by_type[KY_RESOURCE_COUNT]; // 按类型统计
} ResourceStats;

// 获取统计信息
void ky_resmgr_get_stats(kyResourceManager *rm, ResourceStats *stats) {
    memset(stats, 0, sizeof(ResourceStats));
    
    stats->total_resources = rm->resources.count;
    stats->total_memory = rm->total_memory;
    
    for (int i = 0; i < rm->resources.count; i++) {
        kyResource *res = (kyResource*)rm->resources.data + i;
        
        if (res->ref_count > 0) {
            stats->active_resources++;
        }
        
        if (res->type < KY_RESOURCE_COUNT) {
            stats->resources_by_type[res->type]++;
        }
    }
}

// 打印统计信息
void ky_resmgr_print_stats(kyResourceManager *rm) {
    ResourceStats stats;
    ky_resmgr_get_stats(rm, &stats);
    
    KY_LOG_INFO("=== Resource Manager Statistics ===");
    KY_LOG_INFO("Total resources: %u", stats.total_resources);
    KY_LOG_INFO("Active resources: %u", stats.active_resources);
    KY_LOG_INFO("Total memory: %.2f MB", (float)stats.total_memory / (1024 * 1024));
    
    for (int i = 0; i < KY_RESOURCE_COUNT; i++) {
        if (stats.resources_by_type[i] > 0) {
            const char *type_names[] = {"Texture", "Audio", "Model", "Shader", "Raw"};
            KY_LOG_INFO("%s resources: %u", type_names[i], stats.resources_by_type[i]);
        }
    }
}
```

## 异步加载

### 1. 异步加载队列

```c
// 异步加载请求
typedef struct ResourceLoadRequest {
    char path[256];                 // 资源路径
    kyResourceType type;            // 资源类型
    ResourceLoadComplete callback;  // 完成回调
    void *user_data;                // 用户数据
    uint64_t request_id;            // 请求 ID
    float progress;                 // 进度
    int completed;                  // 是否完成
} ResourceLoadRequest;

// 异步加载资源
int ky_resmgr_load_async(kyResourceManager *rm, const char *path, 
                        kyResourceType type, ResourceLoadComplete callback,
                        void *user_data) {
    // 创建加载请求
    ResourceLoadRequest request = {0};
    strncpy(request.path, path, sizeof(request.path) - 1);
    request.type = type;
    request.callback = callback;
    request.user_data = user_data;
    request.request_id = ky_time_ticks();
    request.progress = 0.0f;
    request.completed = 0;
    
    // 添加到加载队列
    ky_array_push(&rm->loading_queue, &request);
    
    KY_LOG_DEBUG("Queued async load: %s", path);
    return 0;
}

// 更新异步加载
void ky_resmgr_update_async(kyResourceManager *rm) {
    // 处理完成的加载
    for (int i = 0; i < rm->loading_resources.count; i++) {
        ResourceLoadRequest *request = 
            (ResourceLoadRequest*)rm->loading_resources.data + i;
        
        if (request->completed) {
            // 调用回调
            if (request->callback) {
                request->callback(NULL, 0, request->user_data);
            }
            
            // 从加载数组中移除
            ky_array_remove_at(&rm->loading_resources, i);
            i--;
        }
    }
    
    // 处理加载队列
    for (int i = 0; i < rm->loading_queue.count; i++) {
        ResourceLoadRequest *request = 
            (ResourceLoadRequest*)rm->loading_queue.data + i;
        
        // 开始加载资源
        if (!ky_resmgr_is_loading(rm, request->path)) {
            // 开始加载
            start_resource_loading(rm, request);
            
            // 从队列中移除并添加到加载数组
            ky_array_remove_at(&rm->loading_queue, i);
            ky_array_push(&rm->loading_resources, request);
            i--;
        }
    }
}

// 检查资源是否正在加载
int ky_resmgr_is_loading(kyResourceManager *rm, const char *path) {
    for (int i = 0; i < rm->loading_resources.count; i++) {
        ResourceLoadRequest *request = 
            (ResourceLoadRequest*)rm->loading_resources.data + i;
        
        if (strcmp(request->path, path) == 0 && !request->completed) {
            return 1;
        }
    }
    return 0;
}
```

### 2. 后台加载线程

```c
// 后台加载线程函数
typedef struct ResourceLoadThread {
    ThreadHandle thread;
    kyArray *requests;
    kyMutex mutex;
    int running;
} ResourceLoadThread;

// 加载线程
static void resource_load_thread(ResourceLoadThread *thread_data) {
    while (thread_data->running) {
        kyMutexLock(&thread_data->mutex);
        
        if (thread_data->requests->count > 0) {
            ResourceLoadRequest *request = 
                (ResourceLoadRequest*)thread_data->requests->data;
            
            // 执行加载
            load_resource_from_disk(request);
            
            // 标记完成
            request->completed = 1;
            
            ky_array_remove_at(thread_data->requests, 0);
        }
        
        kyMutexUnlock(&thread_data->mutex);
        
        // 休眠一段时间
        ky_sleep_msec(16);  // ~60 FPS
    }
}

// 启动后台加载线程
void start_background_loading(ResourceLoadThread *thread_data) {
    thread_data->running = 1;
    thread_create(&thread_data->thread, 
                 (ThreadFunc)resource_load_thread, thread_data);
}

// 停止后台加载线程
void stop_background_loading(ResourceLoadThread *thread_data) {
    thread_data->running = 0;
    thread_join(thread_data->thread);
}
```

## 资源预加载

### 1. 预加载管理

```c
// 预加载组
typedef struct Resource preloadGroup {
    char name[64];                  // 组名称
    kyArray resources;              // 资源列表
    float progress;                 // 总体进度
    int completed;                  // 是否完成
} ResourcePreloadGroup;

// 预加载管理器
typedef struct ResourcePreloadManager {
    kyHashMap preload_groups;       // 预加载组
    kyArray current_preloads;      // 当前正在预加载的组
    ResourceLoadThread load_thread; // 后台加载线程
} ResourcePreloadManager;

// 创建预加载组
ResourcePreloadGroup *preload_group_create(ResourcePreloadManager *pm, 
                                         const char *name) {
    ResourcePreloadGroup *group = ky_alloc(sizeof(ResourcePreloadGroup));
    strncpy(group->name, name, sizeof(group->name) - 1);
    ky_array_create(&group->resources, sizeof(char*), NULL);
    group->progress = 0.0f;
    group->completed = 0;
    
    // 添加到管理器
    char *name_copy = ky_strdup(name, NULL);
    ky_hashmap_set(&pm->preload_groups, &name_copy, group);
    
    return group;
}

// 添加资源到预加载组
void preload_group_add(ResourcePreloadManager *pm, ResourcePreloadGroup *group,
                      const char *path, kyResourceType type) {
    char *path_copy = ky_strdup(path, NULL);
    ky_array_push(&group->resources, &path_copy);
}

// 开始预加载
void preload_group_start(ResourcePreloadManager *pm, ResourcePreloadGroup *group,
                       ResourceLoadComplete callback, void *user_data) {
    group->completed = 0;
    group->progress = 0.0f;
    
    // 将资源添加到加载队列
    for (int i = 0; i < group->resources.count; i++) {
        char *path = *(char**)ky_array_get(&group->resources, i);
        
        ResourceLoadRequest request = {0};
        strncpy(request.path, path, sizeof(request.path) - 1);
        request.type = KY_RESOURCE_RAW;  // 根据实际情况调整
        request.callback = callback;
        request.user_data = user_data;
        request.request_id = ky_time_ticks();
        
        ky_array_push(&pm->load_thread.requests, &request);
    }
    
    ky_array_push(&pm->current_preloads, &group);
}
```

### 2. 预加载状态监控

```c
// 更新预加载状态
void preload_manager_update(ResourcePreloadManager *pm) {
    // 更新后台加载
    ky_resmgr_update_async(NULL);  // 使用全局资源管理器
    
    // 更新预加载组进度
    for (int i = 0; i < pm->current_preloads.count; i++) {
        ResourcePreloadGroup *group = *(ResourcePreloadGroup**)ky_array_get(&pm->current_preloads, i);
        
        // 计算总体进度
        float total_progress = 0.0f;
        int completed_count = 0;
        
        for (int j = 0; j < group->resources.count; j++) {
            char *path = *(char**)ky_array_get(&group->resources, j);
            if (ky_resmgr_is_loaded(NULL, path)) {  // 检查是否已加载
                completed_count++;
            }
        }
        
        group->progress = (float)completed_count / group->resources.count;
        
        // 检查是否完成
        if (group->progress >= 1.0f) {
            group->completed = 1;
            ky_array_remove_at(&pm->current_preloads, i);
            i--;
            
            KY_LOG_INFO("Preload group '%s' completed", group->name);
        }
    }
}

// 获取预加载进度
float preload_manager_get_progress(ResourcePreloadManager *pm, const char *group_name) {
    ResourcePreloadGroup *group = (ResourcePreloadGroup*)ky_hashmap_get(&pm->preload_groups, &group_name);
    return group ? group->progress : 0.0f;
}
```

## 最佳实践

### 1. 资源管理策略

```c
// 资源管理策略
typedef struct ResourceStrategy {
    uint32_t max_cache_size;        // 最大缓存大小
    uint32_t preload_distance;      // 预加载距离
    uint32_t streaming_threshold;   // 流式传输阈值
    uint32_t priority_levels;       // 优先级级别
    int enable_async;               // 启用异步加载
    int enable_background_loading;  // 启用后台加载
} ResourceStrategy;

// 优化内存使用
void optimize_memory_usage(kyResourceManager *rm, ResourceStrategy *strategy) {
    // 设置合理的缓存大小
    rm->max_cache_size = strategy->max_cache_size;
    
    // 根据内存压力调整策略
    if (rm->total_memory > strategy->max_cache_size * 0.8f) {
        // 内存压力大，启用激进清理
        enable_aggressive_eviction(rm);
    } else if (rm->total_memory < strategy->max_cache_size * 0.3f) {
        // 内存压力小，保持缓存
        enable_cache_preservation(rm);
    }
}

// 优先级管理
void set_resource_priority(kyResourceManager *rm, const char *path, uint32_t priority) {
    kyResource *resource = (kyResource*)ky_hashmap_get(&rm->path_map, &path);
    if (resource) {
        resource->flags = (resource->flags & ~KY_RESOURCE_PRIORITY_MASK) | 
                          (priority << KY_RESOURCE_PRIORITY_SHIFT);
    }
}
```

### 2. 资源监控

```c
// 资源监控
typedef struct ResourceMonitor {
    kyArray memory_snapshots;       // 内存快照
    uint32_t snapshot_interval;     // 快照间隔（帧）
    uint32_t frame_count;           // 帧计数
    ResourceStats last_stats;      // 最后统计
} ResourceMonitor;

// 添加内存快照
void resource_monitor_snapshot(ResourceMonitor *monitor, kyResourceManager *rm) {
    if (monitor->frame_count % monitor->snapshot_interval == 0) {
        ResourceSnapshot snapshot = {0};
        ky_resmgr_get_stats(rm, &snapshot.stats);
        snapshot.timestamp = ky_time_ticks();
        
        ky_array_push(&monitor->memory_snapshots, &snapshot);
        
        // 限制快照数量
        if (monitor->memory_snapshots.count > 100) {
            ky_array_remove_at(&monitor->memory_snapshots, 0);
        }
    }
    monitor->frame_count++;
}

// 生成内存报告
void resource_monitor_report(ResourceMonitor *monitor) {
    if (monitor->memory_snapshots.count < 2) return;
    
    ResourceSnapshot *first = (ResourceSnapshot*)monitor->memory_snapshots.data;
    ResourceSnapshot *last = (ResourceSnapshot*)monitor->memory_snapshots.data + 
                           monitor->memory_snapshots.count - 1;
    
    uint64_t time_diff = last->timestamp - first->timestamp;
    size_t memory_diff = last->stats.total_memory - first->stats.total_memory;
    
    KY_LOG_INFO("Resource Memory Report:");
    KY_LOG_INFO("  Time span: %.2f seconds", (float)time_diff / 1000.0f);
    KY_LOG_INFO("  Memory change: %.2f MB", (float)memory_diff / (1024.0f * 1024.0f));
    KY_LOG_INFO("  Peak memory: %.2f MB", 
               (float)last->stats.total_memory / (1024.0f * 1024.0f));
}
```

## 总结

Kronyx Resource 系统为游戏开发提供了强大的资源管理能力：

### 核心优势

1. **统一接口**: 支持多种资源类型的统一管理
2. **自动管理**: 基于引用计数的自动资源生命周期管理
3. **性能优化**: 缓存、预加载、异步加载等多种优化策略
4. **内存监控**: 完整的内存使用监控和报告功能

### 实际应用

1. **纹理管理**: 高效的纹理缓存和加载机制
2. **资源预加载**: 智能的预加载策略，减少加载等待时间
3. **内存管理**: 自动的内存清理和优化
4. **异步加载**: 支持后台加载，保持游戏流畅

### 设计建议

1. **合理设置缓存大小**: 根据游戏类型和内存状况设置合适的缓存大小
2. **使用预加载**: 在游戏空闲时预加载下一阶段资源
3. **监控内存使用**: 定期检查内存使用情况，及时调整策略
4. **异步加载**: 优先使用异步加载，避免阻塞主线程

通过合理使用 Resource 系统，开发者可以构建高效、稳定的游戏资源管理方案。