# Garbage Collection 垃圾回收

## 概述

垃圾回收系统是 Kronyx Engine 脚本子系统的重要组成部分，采用分代垃圾回收算法管理 `kyx` 脚本语言对象的内存生命周期。系统支持自动内存管理，减少内存泄漏和手动内存管理的复杂性。

## 架构设计

### 垃圾回收算法

#### 分代回收 (Generational GC)
采用经典的分代垃圾回收算法，将对象分为年轻代和年老代：

1. **年轻代 (Young Generation)**
   - 存储新创建的小对象
   - 频繁回收，使用复制算法
   - 分为两个半空间：From Space 和 To Space

2. **年老代 (Old Generation)**
   - 存储存活时间较长的对象
   - 偶尔回收，使用标记-清除算法
   - 包含永久对象（如字符串常量）

#### GC 周期
```c
typedef struct kyGC {
    // 年轻代
    kyGCGeneration young_gen;
    
    // 年老代  
    kyGCGeneration old_gen;
    
    // 根集合
    kyGCObject *roots[KY_MAX_GC_ROOTS];
    int root_count;
    
    // 统计信息
    size_t total_allocated;
    size_t total_freed;
    int collection_count;
} kyGC;
```

### 对象系统

#### 对象类型
```c
typedef enum kyGCObjectType {
    KY_GC_STRING = 0,      // 字符串对象
    KY_GC_ARRAY,          // 数组对象
    KY_GC_CLOSURE,        // 闭包对象
    KY_GC_TABLE,          // 表对象
    KY_GC_USERDATA,       // 用户数据
    KY_GC_NATIVE,         // 原生函数对象
} kyGCObjectType;
```

#### 对象结构
```c
typedef struct kyGCObject {
    // 对象头
    kyGCObjectType type;
    uint32_t size;         // 对象大小
    uint32_t flags;        // 对象标志
    struct kyGCObject *next; // 下一个对象
    
    // 分代信息
    int generation;        // 0=年轻代, 1=年老代
    uint32_t age;          // 存活年龄
    
    // 引用计数 (可选)
    uint32_t ref_count;
    
    // 对象数据 (根据类型不同)
    union {
        struct {
            char *data;
            uint32_t length;
        } string;
        
        struct {
            kyGCObject **elements;
            uint32_t capacity;
            uint32_t count;
        } array;
        
        // 其他类型...
    } data;
} kyGCObject;
```

## API 参考

### GC 初始化和销毁

#### 初始化 GC
```c
// 创建垃圾回收器
KY_API kyGC* ky_gc_create(size_t max_heap_size);

// 销毁垃圾回收器
KY_API void ky_gc_destroy(kyGC *gc);
```

#### 手动触发回收
```c
// 执行垃圾回收
KY_API void ky_gc_collect(kyGC *gc);

// 执行年轻代回收
KY_API void ky_gc_collect_young(kyGC *gc);

// 执行年老代回收  
KY_API void ky_gc_collect_old(kyGC *gc);

// 增量回收
KY_API void ky_gc_incremental_collect(kyGC *gc);
```

### 对象管理

#### 对象分配
```c
// 分配对象
KY_API kyGCObject* ky_gc_allocate(kyGC *gc, kyGCObjectType type, size_t size);

// 分配字符串对象
KY_API kyGCObject* ky_gc_alloc_string(kyGC *gc, const char *data, uint32_t length);

// 分配数组对象
KY_API kyGCObject* ky_gc_alloc_array(kyGC *gc, uint32_t capacity);

// 分配闭包对象
KY_API kyGCObject* ky_gc_alloc_closure(kyGC *gc, kyFunction *func);
```

#### 对象引用
```c
// 增加引用计数
KY_API void ky_gc_incref(kyGC *gc, kyGCObject *obj);

// 减少引用计数
KY_API void ky_gc_decref(kyGC *gc, kyGCObject *obj);

// 获取引用计数
KY_API uint32_t ky_gc_get_ref_count(kyGC *gc, kyGCObject *obj);
```

### 根集合管理

#### 添加根对象
```c
// 添加根对象
KY_API void ky_gc_add_root(kyGC *gc, kyGCObject *obj);

// 移除根对象
KY_API void ky_gc_remove_root(kyGC *gc, kyGCObject *obj);

// 清空根集合
KY_API void ky_gc_clear_roots(kyGC *gc);
```

### 内存管理

#### 内存统计
```c
typedef struct kyGCStats {
    size_t total_allocated;      // 总分配内存
    size_t total_freed;         // 总释放内存  
    size_t current_usage;       // 当前使用内存
    size_t peak_usage;          // 峰值使用内存
    size_t young_gen_size;      // 年轻代大小
    size_t old_gen_size;        // 年老代大小
    int collection_count;       // 回收次数
    float average_collection_time; // 平均回收时间
} kyGCStats;

// 获取 GC 统计信息
KY_API void ky_gc_get_stats(kyGC *gc, kyGCStats *stats);
```

#### 内存限制
```c
// 设置内存限制
KY_API void ky_gc_set_memory_limit(kyGC *gc, size_t max_memory);

// 获取内存限制
KY_API size_t ky_gc_get_memory_limit(kyGC *gc);

// 检查是否需要回收
KY_API int ky_gc_needs_collection(kyGC *gc);
```

## 工作原理

### 分代回收流程

#### 年轻代回收 (Scavenge)
1. **标记阶段**：遍历 From Space 中的对象，标记所有可达对象
2. **复制阶段**：将存活对象复制到 To Space，并更新引用
3. **交换阶段**：交换 From Space 和 To Space
4. **重置阶段**：清空原来的 From Space

```c
// 年轻代回收实现
static void ky_gc_scavenge(kyGCGeneration *gen) {
    // 1. 标记可达对象
    for (int i = 0; i < gen->object_count; i++) {
        mark_object(gen->objects[i]);
    }
    
    // 2. 复制存活对象
    kyGCObject **to_space = gen->space[1];
    int to_index = 0;
    
    for (int i = 0; i < gen->object_count; i++) {
        kyGCObject *obj = gen->objects[i];
        if (obj->marked) {
            copy_object(obj, &to_space[to_index++]);
        }
    }
    
    // 3. 交换空间
    gen->objects = to_space;
    gen->object_count = to_index;
}
```

#### 年老代回收 (Mark-Sweep)
1. **根标记**：从根集合开始遍历，标记所有可达对象
2. **标记阶段**：递归标记所有引用的对象
3. **清除阶段**：遍历整个堆，清除未标记的对象

```c
// 年老代回收实现
static void ky_gc_mark_sweep(kyGCGeneration *gen) {
    // 1. 根标记
    for (int i = 0; i < gc->root_count; i++) {
        mark_object(gc->roots[i]);
    }
    
    // 2. 递归标记
    for (int i = 0; i < gen->object_count; i++) {
        mark_reachable(gen->objects[i]);
    }
    
    // 3. 清除未标记对象
    int new_count = 0;
    for (int i = 0; i < gen->object_count; i++) {
        kyGCObject *obj = gen->objects[i];
        if (obj->marked) {
            obj->marked = 0;  // 重置标记
            gen->objects[new_count++] = obj;
        } else {
            free_object(obj);
        }
    }
    gen->object_count = new_count;
}
```

### 引用计数与 GC 结合

#### 双重策略
- **引用计数**：用于快速清理可立即回收的对象
- **垃圾回收**：处理循环引用和批量清理

```c
// 对象释放
void ky_gc_release(kyGC *gc, kyGCObject *obj) {
    // 减少引用计数
    obj->ref_count--;
    
    // 如果引用计数为 0，立即回收
    if (obj->ref_count == 0) {
        free_object(obj);
    }
}
```

### 增量回收

#### 增量标记
为了避免长时间停顿，支持增量标记：

```c
// 增量标记步骤
#define MARK_BATCH_SIZE 100

static void ky_gc_incremental_mark(kyGC *gc) {
    static int mark_index = 0;
    int processed = 0;
    
    while (mark_index < gc->root_count && processed < MARK_BATCH_SIZE) {
        mark_object(gc->roots[mark_index++]);
        processed++;
    }
    
    if (mark_index >= gc->root_count) {
        // 标记完成，开始标记阶段
        mark_phase_2(gc);
    }
}
```

## 配置选项

### GC 配置参数
```c
// 分代配置
#define KY_GC_YOUNG_GEN_SIZE (1024 * 1024)      // 1MB
#define KY_GC_OLD_GEN_SIZE (16 * 1024 * 1024)   // 16MB
#define KY_GC_MAX_AGE 10                        // 最大年龄阈值

// 回收配置
#define KY_GC_COLLECTION_THRESHOLD 0.8           // 回收阈值
#define KY_GC_INCREMENTAL_STEPS 1000             // 增量步骤
#define KY_GC_MAX_PAUSE_TIME 0.001              // 最大暂停时间(秒)

// 对象配置
#define KY_GC_STRING_POOL_SIZE 4096             // 字符串池大小
#define KY_GC_ARRAY_INITIAL_CAPACITY 16          // 数组初始容量
```

### 性能调优
```c
// 启用/禁用 GC 功能
#define KY_GC_DISABLE_REFERENCING 0
#define KY_GC_DISABLE_INCREMENTAL 0
#define KY_GC_DISABLE_FINALIZERS 0

// 调试选项
#define KY_GC_ENABLE_LOGGING 1
#define KY_GC_ENABLE_STATS 1
```

## 使用示例

### 基本使用
```c
// 创建 GC
kyGC *gc = ky_gc_create(16 * 1024 * 1024);  // 16MB 限制

// 分配字符串对象
kyGCObject *str = ky_gc_alloc_string(gc, "Hello World", 11);

// 使用字符串
printf("String: %s\n", str->data.string.data);

// 释放对象 (引用计数管理)
ky_gc_decref(gc, str);

// 手动触发回收
ky_gc_collect(gc);

// 销毁 GC
ky_gc_destroy(gc);
```

### 复杂对象管理
```c
// 创建数组对象
kyGCObject *arr = ky_gc_alloc_array(gc, 10);

// 填充数组
for (int i = 0; i < 10; i++) {
    kyGCObject *elem = ky_gc_allocate(gc, KY_GC_INT, sizeof(int));
    *(int*)elem->data = i * 2;
    arr->data.array.elements[i] = elem;
}

// 创建闭包
kyGCObject *closure = ky_gc_alloc_closure(gc, function);
closure->data.closure.upvalues = ...;

// 设置引用
ky_gc_incref(gc, arr);
ky_gc_incref(gc, closure);

// 使用完成后减少引用
ky_gc_decref(gc, arr);
ky_gc_decref(gc, closure);
```

### 监控和调试
```c
// 获取 GC 统计信息
kyGCStats stats;
ky_gc_get_stats(gc, &stats);

printf("Total allocated: %zu MB\n", stats.total_allocated / (1024 * 1024));
printf("Current usage: %zu MB\n", stats.current_usage / (1024 * 1024));
printf("Collection count: %d\n", stats.collection_count);

// 监控内存使用
while (1) {
    if (ky_gc_needs_collection(gc)) {
        printf("GC collection triggered\n");
        ky_gc_collect(gc);
    }
    
    // 继续运行...
}
```

## 性能优化

### 内存分配优化

#### 对象池
```c
// 为常用对象类型使用对象池
typedef struct kyGCObjectPool {
    kyGCObject *pool;
    int capacity;
    int used;
} kyGCObjectPool;

// 快速分配
kyGCObject* ky_gc_pool_alloc(kyGCObjectPool *pool) {
    if (pool->used < pool->capacity) {
        return &pool->pool[pool->used++];
    }
    return NULL;  // 池已满
}
```

#### 内存对齐
```c
// 确保对象内存对齐以提高性能
#define KY_GC_OBJECT_ALIGN 16

typedef struct __attribute__((aligned(KY_GC_OBJECT_ALIGN))) kyGCObject {
    // 对象数据
} kyGCObject;
```

### 回收优化

#### 分代大小调整
```c
// 动态调整分代大小
void ky_gc_adjust_generations(kyGC *gc) {
    float young_usage = (float)gc->young_gen.object_count / gc->young_gen.capacity;
    float old_usage = (float)gc->old_gen.object_count / gc->old_gen.capacity;
    
    if (young_usage > 0.8) {
        // 年轻代使用率高，增加大小
        gc->young_gen.capacity *= 1.5;
    }
    
    if (old_usage < 0.3) {
        // 年老代使用率低，减少大小
        gc->old_gen.capacity *= 0.8;
    }
}
```

#### 增量 GC 优化
```c
// 基于时间的增量回收
void ky_gc_time_based_collect(kyGC *gc, float dt) {
    static float accumulator = 0;
    accumulator += dt;
    
    if (accumulator >= 0.016f) {  // 每帧执行一部分
        ky_gc_incremental_collect(gc);
        accumulator = 0;
    }
}
```

## 故障排除

### 常见问题

#### 内存泄漏
```bash
# 问题：内存使用持续增长
# 解决方案：
# 1. 检查对象引用计数
# 2. 确保根集合清理
# 3. 启用 GC 日志
#define KY_GC_ENABLE_LOGGING 1

# 检查内存泄漏
kyGCStats stats;
ky_gc_get_stats(gc, &stats);
if (stats.total_allocated - stats.total_freed > 0) {
    printf("Potential memory leak detected\n");
}
```

#### 性能问题
```bash
# 问题：GC 回收频率过高
# 解决方案：
# 1. 调整分代大小
ky_gc_set_memory_limit(gc, 32 * 1024 * 1024);  // 增加内存限制

# 2. 优化对象分配
// 预分配大数组避免频繁分配
```

#### 循环引用
```bash
# 问题：循环引用导致对象无法回收
# 解决方案：
# 1. 使用弱引用
// 定义弱引用类型
#define KY_GC_WEAK_REF 0x01

// 弱引用分配
kyGCObject* ky_gc_alloc_weak_ref(kyGC *gc, kyGCObject *target) {
    obj->flags |= KY_GC_WEAK_REF;
    ky_gc_incref(gc, target);
    return obj;
}

# 2. 手动管理引用
```

### 调试工具

#### 内存分析
```c
// 对象遍历器
typedef void (*kyGCObjectVisitor)(kyGCObject *obj, void *user_data);

void ky_gc_walk_objects(kyGC *gc, kyGCObjectVisitor visitor, void *user_data) {
    // 遍历所有对象
    for (int gen = 0; gen < 2; gen++) {
        for (int i = 0; i < gc->generations[gen].object_count; i++) {
            visitor(gc->generations[gen].objects[i], user_data);
        }
    }
}

// 示例：统计对象类型
void count_objects(kyGCObject *obj, void *user_data) {
    int *counts = (int*)user_data;
    counts[obj->type]++;
}

// 使用示例
int type_counts[KY_GC_OBJECT_TYPE_COUNT];
memset(type_counts, 0, sizeof(type_counts));
ky_gc_walk_objects(gc, count_objects, type_counts);
```

#### 内存转储
```c
// 转储 GC 状态到文件
void ky_gc_dump_to_file(kyGC *gc, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) return;
    
    // 写入统计信息
    kyGCStats stats;
    ky_gc_get_stats(gc, &stats);
    fprintf(file, "GC Statistics:\n");
    fprintf(file, "Total allocated: %zu\n", stats.total_allocated);
    fprintf(file, "Current usage: %zu\n", stats.current_usage);
    fprintf(file, "Collection count: %d\n", stats.collection_count);
    
    // 写入对象信息
    ky_gc_walk_objects(gc, dump_object_info, file);
    
    fclose(file);
}
```

## 扩展功能

### 自定义回收策略

#### 优先级回收
```c
typedef enum kyGCObjectPriority {
    KY_GC_PRIORITY_LOW = 0,     // 低优先级
    KY_GC_PRIORITY_NORMAL,      // 普通优先级
    KY_GC_PRIORITY_HIGH         // 高优先级
} kyGCObjectPriority;

typedef struct kyGCObjectWithPriority {
    kyGCObject *obj;
    kyGCObjectPriority priority;
    struct kyGCObjectWithPriority *next;
} kyGCObjectWithPriority;

// 优先级回收器
void ky_gc_collect_by_priority(kyGC *gc, kyGCObjectPriority min_priority) {
    // 回收指定优先级及以下的所有对象
}
```

#### 延迟回收
```c
// 延迟回收队列
typedef struct kyGCDelayedFree {
    kyGCObject *obj;
    uint64_t free_time;
    struct kyGCDelayedFree *next;
} kyGCDelayedFree;

// 延迟释放对象
void ky_gc_delayed_free(kyGC *gc, kyGCObject *obj, float delay_seconds) {
    kyGCDelayedFree *df = malloc(sizeof(kyGCDelayedFree));
    df->obj = obj;
    df->free_time = current_time + delay_seconds * 1000;
    df->next = gc->delayed_free_list;
    gc->delayed_free_list = df;
}
```

---

**相关文档**：
- [ARCHITECTURE.md](../ARCHITECTURE.md) - 系统架构设计
- [Script.md](../模块/Script.md) - 脚本系统
- [Memory.md](../模块/Memory.md) - 内存管理
- [ECS.md](./ECS.md) - 实体组件系统