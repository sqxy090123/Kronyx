# kyx 脚本语言

## 概述

kyx 是 Kronyx Engine 内置的轻量级脚本语言，专为游戏开发设计。它采用寄存器虚拟机架构，支持分代垃圾回收，并提供与 C 代码的无缝集成。kyx 语言的设计目标是提供：

- **高性能执行**：寄存器 VM + 即时编译优化
- **与 C 代码深度集成**：直接访问 C 组件和函数
- **自动内存管理**：分代垃圾回收器
- **快速开发迭代**：热重载支持
- **跨平台一致**：统一的运行时行为

## 语言特性

### 基本语法

kyx 采用类似 C 的语法，但更加简洁和现代化：

```kyx
// 变量声明
var x = 42
var name = "Player"
var is_active = true

// 基本运算
var result = x + 10 * 2
var ratio = value / max_value

// 条件语句
if score > 100 {
    print("Excellent!")
} else if score > 50 {
    print("Good job")
} else {
    print("Try harder")
}

// 循环
for i = 0 to 10 {
    print(i)
}

while health > 0 {
    update()
    render()
}
```

### 函数定义

```kyx
// 简单函数
function add(a, b) {
    return a + b
}

// 带默认参数的函数
function spawn_entity(type = "default", x = 0, y = 0) {
    var entity = std.spawn()
    std.setpos(entity, x, y)
    return entity
}

// 递归函数
function factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
```

### 数据结构

```kyx
// 数组
var numbers = [1, 2, 3, 4, 5]
var first = numbers[0]
numbers[2] = 10

// 对象
var player = {
    name = "Hero",
    health = 100,
    position = {x = 0, y = 0},
    inventory = []
}

// 访问对象属性
var name = player.name
player.health = 80
player.position.x = 10

// 数组操作
numbers.push(6)          // 添加元素
numbers.pop()           // 移除最后一个元素
var count = numbers.length

// 对象操作
var keys = Object.keys(player)
var values = Object.values(player)
```

### 函数式编程

```kyx
// 高阶函数
function apply_operation(numbers, op) {
    var result = []
    for i = 0 to numbers.length {
        result.push(op(numbers[i]))
    }
    return result
}

// 使用 Lambda 表达式
var doubled = apply_operation([1, 2, 3], function(x) { return x * 2 })
var filtered = apply_operation([1, 2, 3, 4, 5], function(x) { return x > 2 })
```

## 虚拟机架构

### 字节码指令集

kyx VM 使用基于栈的指令集，支持以下操作类别：

#### 算术运算
```c
// 算术指令
OP_ADD      // a + b
OP_SUB      // a - b
OP_MUL      // a * b
OP_DIV      // a / b
OP_MOD      // a % b
OP_NEG      // -a
OP_ABS      // abs(a)
```

#### 比较运算
```c
// 比较指令
OP_EQ       // a == b
OP_NE       // a != b
OP_LT       // a < b
OP_LE       // a <= b
OP_GT       // a > b
OP_GE       // a >= b
```

#### 逻辑运算
```c
// 逻辑指令
OP_AND      // a && b
OP_OR       // a || b
OP_NOT      // !a
```

#### 数据访问
```c
// 数据访问指令
OP_LOAD_VAR    // 加载局部变量
OP_STORE_VAR   // 存储局部变量
OP_LOAD_FIELD  // 加载对象字段
OP_STORE_FIELD // 存储对象字段
OP_LOAD_ARRAY  // 加载数组元素
OP_STORE_ARRAY // 存储数组元素
```

#### 控制流
```c
// 控制流指令
OP_JUMP       // 无条件跳转
OP_JUMP_IF    // 条件跳转
OP_LOOP       // 循环控制
OP_CALL       // 函数调用
OP_RETURN     // 函数返回
```

#### 对象操作
```c
// 对象操作指令
OP_OBJ_CREATE   // 创建对象
OP_OBJ_GET      // 获取属性
OP_OBJ_SET      // 设置属性
OP_OBJ_DELETE   // 删除属性
```

### 寄存器组织

```c
// 虚拟机状态
typedef struct kyVM {
    // 寄存器文件
    kyValue registers[256];        // 通用寄存器
    kyValue *stack_base;           // 栈基址
    kyValue *stack_top;            // 栈顶
    kyValue *stack_end;            // 栈结束
    
    // 执行状态
    kyBytecode *code;              // 当前字节码
    uint32_t ip;                   // 指令指针
    uint32_t frame_count;          // 调用帧数量
    
    // 内存管理
    kyGC *gc;                      // 垃圾回收器
    kyHashMap *globals;            // 全局变量表
    
    // 调试信息
    kyDebugInfo *debug;            // 调试信息
} kyVM;
```

### 执行引擎

```c
// VM 执行循环
int ky_vm_execute(kyVM *vm) {
    kyBytecode *code = vm->code;
    uint32_t ip = 0;
    
    while (ip < code->size) {
        kyInstruction instr = code->instructions[ip++];
        
        switch (instr.opcode) {
            case OP_ADD: {
                // 弹出两个操作数，相加，结果压栈
                kyValue b = vm->stack[--vm->stack_top];
                kyValue a = vm->stack[--vm->stack_top];
                kyValue result = ky_value_add(a, b);
                vm->stack[vm->stack_top++] = result;
                break;
            }
            
            case OP_CALL: {
                // 函数调用
                kyValue *args = &vm->stack[vm->stack_top - instr.argc];
                kyValue result = ky_vm_call_function(vm, args, instr.argc);
                vm->stack_top -= instr.argc;
                vm->stack[vm->stack_top++] = result;
                break;
            }
            
            case OP_RETURN: {
                // 函数返回
                return 0;
            }
            
            // ... 其他指令处理
        }
    }
    
    return 0;
}
```

## 垃圾回收

### 分代回收策略

kyx 使用分代垃圾回收器，将对象分为三代：

1. **新生代（Young Generation）**：新创建的对象
2. **老年代（Old Generation）**：存活一次以上的对象
3. **永久代（Permanent Generation）**：系统对象和常量

```c
// 分代 GC 结构
typedef struct kyGeneration {
    kyObject **objects;            // 对象数组
    size_t count;                  // 对象数量
    size_t capacity;               // 容量
    size_t survivor_count;         // 存活对象数量
} kyGeneration;

typedef struct kyGC {
    kyGeneration young;            // 新生代
    kyGeneration old;              // 老年代
    kyGeneration permanent;       // 永久代
    size_t max_heap_size;          // 最大堆大小
    double gc_threshold;           // GC 触发阈值
} kyGC;
```

### 标记-清除算法

```c
// 标阶阶段
void ky_gc_mark(kyGC *gc, kyValue *root) {
    // 使用深度优先搜索标记可达对象
    if (root->type == KYT_OBJECT) {
        kyObject *obj = root->as.object;
        if (obj->marked) return;
        
        obj->marked = 1;
        
        // 递归标记对象字段
        for (int i = 0; i < obj->field_count; i++) {
            ky_gc_mark(gc, &obj->fields[i]);
        }
    }
}

// 清除阶段
void ky_gc_sweep(kyGC *gc, kyGeneration *gen) {
    size_t new_count = 0;
    
    for (size_t i = 0; i < gen->count; i++) {
        kyObject *obj = gen->objects[i];
        
        if (obj->marked) {
            // 存活的对象，清除标记
            obj->marked = 0;
            gen->objects[new_count++] = obj;
        } else {
            // 不可达对象，释放内存
            ky_object_destroy(obj);
        }
    }
    
    gen->count = new_count;
}

// 执行 GC
void ky_gc_collect(kyGC *gc, kyValue *roots, size_t root_count) {
    // 1. 标记阶段
    for (size_t i = 0; i < root_count; i++) {
        ky_gc_mark(gc, &roots[i]);
    }
    
    // 2. 清除各代
    ky_gc_sweep(gc, &gc->young);
    ky_gc_sweep(gc, &gc->old);
    
    // 3. 对象晋升
    promote_survivors(gc);
    
    // 4. 压缩内存（可选）
    if (gc->old.count > gc->old.capacity / 2) {
        ky_gc_compact(&gc->old);
    }
}
```

### 内存管理最佳实践

```c
// 避免在循环中创建临时对象
function bad_example() {
    for i = 0 to 1000 {
        var temp = {x = i, y = i * 2}  // 每次循环都创建新对象
        use_object(temp)
    }
}

// 好的实践：对象重用
function good_example() {
    var temp = {}
    for i = 0 to 1000 {
        temp.x = i
        temp.y = i * 2
        use_object(temp)
    }
}

// 使用对象池
function use_object_pool() {
    var pool = ObjectPool.create()
    for i = 0 to 1000 {
        var obj = pool.get()
        obj.x = i
        obj.y = i * 2
        use_object(obj)
        pool.release(obj)
    }
}
```

## C 语言集成

### 绑定系统

kyx 提供完整的 C 语言绑定系统，允许：

1. 从 kyx 调用 C 函数
2. 从 C 调用 kyx 函数
3. 在两种语言之间传递数据

#### C 函数注册

```c
// 注册 C 函数到 kyx
void kyx_bindings_register(kyVM *vm) {
    // 注册数学函数
    ky_vm_register(vm, "vec3_add", kyx_vec3_add, 2);
    ky_vm_register(vm, "vec3_normalize", kyx_vec3_normalize, 1);
    ky_vm_register(vm, "mat4_multiply", kyx_mat4_multiply, 2);
    
    // 注册实体操作
    ky_vm_register(vm, "spawn_entity", kyx_spawn_entity, 0);
    ky_vm_register(vm, "destroy_entity", kyx_destroy_entity, 1);
    ky_vm_register(vm, "get_component", kyx_get_component, 2);
    ky_vm_register(vm, "set_component", kyx_set_component, 3);
    
    // 注册全局变量
    ky_vec3 gravity = {0, -9.8f, 0};
    ky_vm_set_global(vm, "GRAVITY", KYT_VEC3, &gravity);
}
```

#### 函数绑定示例

```c
// 数学函数绑定
static kyValue kyx_vec3_add(kyVM *vm, kyValue *args, int argc) {
    KY_ASSERT(argc == 2);
    
    kyVec3 a = args[0].as.vec3;
    kyVec3 b = args[1].as.vec3;
    
    kyVec3 result = {a.x + b.x, a.y + b.y, a.z + b.z};
    return ky_value_make_vec3(result);
}

// 实体操作绑定
static kyValue kyx_spawn_entity(kyVM *vm, kyValue *args, int argc) {
    kyWorld *w = (kyWorld*)vm->user_data;
    kyEntity entity = ky_world_spawn(w);
    return ky_value_make_entity(entity);
}

static kyValue kyx_set_component(kyVM *vm, kyValue *args, int argc) {
    KY_ASSERT(argc == 3);
    
    kyEntity entity = args[0].as.entity;
    uint32_t component_type = args[1].as.i;
    kyValue component_value = args[2];
    
    // 将 kyx 值转换为 C 组件结构
    void *comp = ky_world_add_component(w, entity, component_type);
    if (comp) {
        kyx_value_to_component(vm, component_value, comp);
    }
    
    return ky_value_make_nil();
}
```

#### 数据转换

```c
// kyx 值到 C 值的转换
void kyx_value_to_component(kyVM *vm, kyValue value, void *component) {
    switch (value.type) {
        case KYT_VEC3:
            memcpy(component, &value.as.vec3, sizeof(kyVec3));
            break;
            
        case KYT_INT:
            *(int*)component = (int)value.as.i;
            break;
            
        case KYT_FLOAT:
            *(float*)component = (float)value.as.f;
            break;
            
        case KYT_BOOL:
            *(bool*)component = value.as.b != 0;
            break;
            
        case KYT_OBJECT: {
            // 转换对象到结构体
            kyObject *obj = value.as.object;
            // 根据对象类型映射到组件字段
            break;
        }
    }
}
```

### 性能优化

#### 函数内联

```c
// 简单函数适合内联
static kyValue kyx_inline_function(kyVM *vm, kyValue *args, int argc) {
    KY_ASSERT(argc == 2);
    
    // 直接计算，避免函数调用开销
    kyValue a = args[0];
    kyValue b = args[1];
    
    if (a.type == KYT_FLOAT && b.type == KYT_FLOAT) {
        kyValue result = {KYT_FLOAT};
        result.as.f = a.as.f + b.as.f;
        return result;
    }
    
    // 复杂情况回退到标准调用
    return ky_vm_call_function(vm, args, argc);
}
```

#### 缓存优化

```c
// 缓存频繁访问的函数
typedef struct kyx_function_cache {
    kyHashMap cache;              // 函数名 -> 函数指针
    kyValue precompiled[32];      // 预编译的常用函数
    int precompiled_count;
} kyx_function_cache;

kyValue *kyx_cached_function(ky_function_cache *cache, const char *name) {
    // 检查预编译缓存
    for (int i = 0; i < cache->precompiled_count; i++) {
        if (strcmp(name, kyx_function_names[i]) == 0) {
            return &cache->precompiled[i];
        }
    }
    
    // 检查动态缓存
    kyValue *func = ky_hashmap_get(&cache->cache, name);
    if (func) {
        return func;
    }
    
    return NULL;
}
```

## 调试与开发

### 调试信息

```c
// 调试信息结构
typedef struct kyDebugInfo {
    struct {
        const char *name;
        uint32_t start_ip;
        uint32_t end_ip;
        uint32_t stack_size;
    } *functions;
    
    struct {
        const char *name;
        uint32_t ip;
    } *line_info;
    
    int function_count;
    int line_count;
} kyDebugInfo;
```

### 断点调试

```c
// 设置断点
void kyx_debug_set_breakpoint(kyVM *vm, uint32_t ip) {
    vm->breakpoints[vm->breakpoint_count++] = ip;
}

// 执行循环中的调试支持
int ky_vm_execute_debug(kyVM *vm) {
    while (vm->ip < vm->code->size) {
        // 检查断点
        for (int i = 0; i < vm->breakpoint_count; i++) {
            if (vm->ip == vm->breakpoints[i]) {
                kyx_debugger_break(vm, vm->ip);
                break;
            }
        }
        
        // 正常执行指令
        ky_execute_instruction(vm);
    }
    
    return 0;
}
```

### 性能分析

```c
// 性能计数器
typedef struct kyx_profiler {
    struct {
        const char *name;
        uint64_t call_count;
        uint64_t total_time;
        uint64_t min_time;
        uint64_t max_time;
    } *functions;
    
    int function_count;
    uint64_t start_time;
} kyx_profiler;

// 函数调用包装器
kyValue kyx_profiled_call(kyVM *vm, const char *func_name, kyValue *args, int argc) {
    kyx_profiler *prof = (kyx_profiler*)vm->profiler;
    uint64_t start = get_timestamp();
    
    kyValue result = ky_vm_call_function(vm, args, argc);
    
    uint64_t end = get_timestamp();
    uint64_t duration = end - start;
    
    // 更新统计信息
    update_profile_stats(prof, func_name, duration);
    
    return result;
}
```

## 热重载支持

### 模块加载

```c
// 模块系统
typedef struct kyx_module {
    char *name;
    char *source;
    kyBytecode *code;
    kyFunctionTable functions;
    kyObjectTable globals;
    int loaded;
} kyx_module;

// 加载模块
int kyx_module_load(kyVM *vm, const char *name, const char *source) {
    kyx_module *module = kyx_module_create(name, source);
    
    // 编译源码
    if (kyx_compile_module(module) != 0) {
        return -1;
    }
    
    // 加载到 VM
    kyx_vm_add_module(vm, module);
    
    return 0;
}

// 热重载模块
int kyx_module_reload(kyVM *vm, const char *name, const char *new_source) {
    kyx_module *module = kyx_vm_find_module(vm, name);
    if (!module) {
        return -1;
    }
    
    // 保存当前状态
    kyx_module_save_state(module);
    
    // 重新编译
    if (kyx_compile_module(module) != 0) {
        // 恢复状态
        kyx_module_restore_state(module);
        return -1;
    }
    
    // 更新 VM
    kyx_vm_update_module(vm, module);
    
    return 0;
}
```

### 状态迁移

```c
// 跨重载状态保持
typedef struct kyx_state_migrator {
    kyHashMap object_map;         // 旧对象 -> 新对象
    kyHashMap function_map;       // 旧函数 -> 新函数
    kyValue globals_backup;       // 全局变量备份
} kyx_state_migrator;

void migrate_module_state(kyx_module *old_module, kyx_module *new_module) {
    kyx_state_migrator migrator = {0};
    
    // 1. 备份全局变量
    kyx_backup_globals(old_module, &migrator.globals_backup);
    
    // 2. 迁移对象引用
    migrate_object_references(old_module, new_module, &migrator);
    
    // 3. 迁移函数引用
    migrate_function_references(old_module, new_module, &migrator);
    
    // 4. 恢复全局变量
    kyx_restore_globals(new_module, &migrator.globals_backup, &migrator);
}
```

## 实际应用示例

### 游戏逻辑脚本

```kyx
// 游戏实体脚本
function PlayerController() {
    this.entity = nil
    this.speed = 200
    this.jump_force = 300
    this.is_grounded = false
    
    function init(entity) {
        this.entity = entity
    }
    
    function update(dt) {
        var input = std.poll_input()
        
        // 水平移动
        if input.left {
            std.setpos_x(this.entity, std.getpos_x(this.entity) - this.speed * dt)
        }
        if input.right {
            std.setpos_x(this.entity, std.getpos_x(this.entity) + this.speed * dt)
        }
        
        // 跳跃
        if input.jump and this.is_grounded {
            std.setvel_y(this.entity, -this.jump_force)
            this.is_grounded = false
        }
        
        // 重力
        var vel_y = std.getvel_y(this.entity)
        std.setvel_y(this.entity, vel_y + 980 * dt)
        
        // 地面检测
        if std.getpos_y(this.entity) >= 0 {
            std.setpos_y(this.entity, 0)
            std.setvel_y(this.entity, 0)
            this.is_grounded = true
        }
    }
    
    return this
}

// 创建玩家
var player = PlayerController()
player.init(std.spawn())
```

### AI 行为脚本

```kyx
// AI 行为树
function BehaviorTree() {
    this.root = nil
    this.entity = nil
    
    function init(entity) {
        this.entity = entity
        this.root = SelectorNode {
            children = [
                PatrolNode {
                    waypoints = [{x=0,y=0}, {x=100,y=0}, {x=100,y=100}, {x=0,y=100}]
                    current = 0
                },
                ChaseNode {
                    target = "player"
                },
                IdleNode {
                    duration = 2.0
                }
            ]
        }
    }
    
    function update(dt) {
        this.root.execute(this.entity, dt)
    }
    
    return this
}

// 行为节点类型
function SelectorNode(params) {
    this.name = "selector"
    this.children = params.children || []
    
    function execute(entity, dt) {
        for i = 0 to this.children.length {
            var result = this.children[i].execute(entity, dt)
            if result == "success" {
                return "success"
            }
        }
        return "failure"
    }
    
    return this
}

function PatrolNode(params) {
    this.name = "patrol"
    this.waypoints = params.waypoints
    this.current = params.current || 0
    this.wait_time = 0
    
    function execute(entity, dt) {
        var target = this.waypoints[this.current]
        var pos = std.getpos(entity)
        
        // 移动到目标点
        var dx = target.x - pos.x
        var dy = target.y - pos.y
        var distance = math.sqrt(dx*dx + dy*dy)
        
        if distance < 5 {
            // 到达目标点
            this.current = (this.current + 1) % this.waypoints.length
            return "success"
        } else {
            // 移动向目标
            std.setvel(entity, dx/distance * 50, dy/distance * 50)
            return "running"
        }
    }
    
    return this
}
```

### UI 控制脚本

```kyx
// UI 控制器
function UIController() {
    this.menu_items = []
    this.selected_index = 0
    this.callbacks = {}
    
    function init(menu_data) {
        for i = 0 to menu_data.length {
            var item = {
                text = menu_data[i].text,
                x = menu_data[i].x,
                y = menu_data[i].y,
                width = menu_data[i].width,
                height = menu_data[i].height
            }
            this.menu_items.push(item)
            
            // 设置回调
            if menu_data[i].callback {
                this.callbacks[i] = menu_data[i].callback
            }
        }
    }
    
    function update(dt) {
        var input = std.poll_input()
        
        // 导航
        if input.up {
            this.selected_index = math.max(0, this.selected_index - 1)
        }
        if input.down {
            this.selected_index = math.min(this.menu_items.length - 1, this.selected_index + 1)
        }
        
        // 选择
        if input.select {
            if this.callbacks[this.selected_index] {
                this.callbacks[this.selected_index]()
            }
        }
    }
    
    function render(renderer) {
        for i = 0 to this.menu_items.length {
            var item = this.menu_items[i]
            var is_selected = (i == this.selected_index)
            
            // 绘制菜单项
            renderer.draw_text(item.text, item.x, item.y, 
                             is_selected ? "white" : "gray")
            
            // 绘制选择指示器
            if is_selected {
                renderer.draw_rect(item.x - 5, item.y, 3, 20, "yellow")
            }
        }
    }
    
    return this
}

// 使用 UI 控制器
var menu = [
    {text="Start Game", x=100, y=100, callback=start_game},
    {text="Options", x=100, y=150, callback=open_options},
    {text="Quit", x=100, y=200, callback=quit_game}
]

var ui = UIController()
ui.init(menu)

// 游戏循环
function game_loop(dt) {
    ui.update(dt)
    ui.render(renderer)
}
```

## 性能优化技巧

### JIT 编译支持

```c
// 简单的 JIT 编译器
typedef struct kyx_jit_compiler {
    kyHashMap hot_functions;     // 热点函数缓存
    void *executable_code;       // 生成的机器码
    size_t code_size;
} kyx_jit_compiler;

kyValue kyx_jit_call(kyVM *vm, kyFunction *func, kyValue *args, int argc) {
    // 检查是否已 JIT 编译
    kyCompiledFunction *jitted = ky_jit_compile(func);
    if (jitted) {
        // 直接执行机器码
        return jitted->entry_point(args, argc);
    }
    
    // 回退到解释执行
    return ky_vm_call_function(vm, args, argc);
}
```

### 内存池优化

```c
// 对象池管理
typedef struct kyx_object_pool {
    kyObject *pool;
    size_t pool_size;
    size_t used;
    kyObject *free_list;
} kyx_object_pool;

kyObject *kyx_object_pool_alloc(kyx_object_pool *pool) {
    if (pool->free_list) {
        kyObject *obj = pool->free_list;
        pool->free_list = obj->next;
        return obj;
    }
    
    if (pool->used < pool->pool_size) {
        return &pool->pool[pool->used++];
    }
    
    return NULL;
}

void kyx_object_pool_free(kyx_object_pool *pool, kyObject *obj) {
    obj->next = pool->free_list;
    pool->free_list = obj;
}
```

### 缓存局部性优化

```c
// 数据局部性优化
typedef struct kyx_component_chunk {
    kyEntity entities[256];      // 连续的实体数组
    Transform transforms[256];   // 连续的组件数组
    size_t count;
} kyx_component_chunk;

// 批量处理组件
void process_components_batch(kyx_component_chunk *chunk) {
    // 连续内存访问，CPU 缓存友好
    for (size_t i = 0; i < chunk->count; i++) {
        Transform *t = &chunk->transforms[i];
        // 处理变换...
    }
}
```

---

*kyx 脚本语言是 Kronyx Engine 的重要组成部分，通过其高效的虚拟机、自动内存管理和 C 语言集成能力，为游戏开发提供了强大的脚本支持。正确使用这些特性可以显著提升开发效率和运行性能。*