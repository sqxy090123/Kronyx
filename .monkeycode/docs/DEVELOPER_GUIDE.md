# Kronyx Engine 开发者指南

## 目录

1. [项目概览](#项目概览)
2. [构建系统](#构建系统)
3. [开发环境配置](#开发环境配置)
4. [代码结构](#代码结构)
5. [API 使用指南](#api-使用指南)
6. [测试框架](#测试框架)
7. [调试技巧](#调试技巧)
8. [性能优化](#性能优化)
9. [跨平台开发](#跨平台开发)
10. [贡献指南](#贡献指南)

## 项目概览

Kronyx Engine 是一个用 C11 编写的轻量级游戏引擎，专为希望拥有完全控制权的游戏开发者设计。引擎采用数据导向架构，提供：

- **高性能 ECS**：Archetype-based 存储，缓存友好
- **跨平台渲染**：控制台后端 + OpenGL ES 3 支持
- **内置脚本语言**：kyx 脚本 VM，支持分代 GC
- **模块化设计**：每个子系统独立，便于静态链接
- **反篡改保护**：可选的安全验证机制

### 技术栈

- **语言**：C11（核心），C++（反篡改和编辑器）
- **构建系统**：CMake ≥ 3.20
- **测试框架**：CTest + 自研断言宏
- **平台支持**：Linux、Windows、macOS
- **图形后端**：控制台、OpenGL ES 3、EGL Pbuffer

## 构建系统

### 基本构建

```bash
# 创建构建目录
cmake -B build

# 编译项目
cmake --build build -j$(nproc)

# 运行测试
ctest --test-dir build --output-on-failure
```

### 构建配置选项

```bash
# Debug 构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Release 构建（默认）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 开启 ASan/UBSan
cmake -B build -DKYR_ENABLE_SANITIZERS=ON

# 构建编辑器
cmake -B build -DKYR_BUILD_EDITOR=ON

# 仅构建核心库（无演示和测试）
cmake -B build -DKYR_BUILD_TESTS=OFF -DKYR_BUILD_DEMO=OFF
```

### 编译器标志

```bash
# GCC/Clang 默认警告
-Wall -Wextra -Wno-unused-parameter

# MSVC 默认警告
/W3 /WX-

# Sanitizer 构建
-fsanitize=address,undefined -fno-omit-frame-pointer

# 静态分析
-fsanitize=memory -fsanitize=thread
```

## 开发环境配置

### VS Code 配置

创建 `.vscode/c_cpp_properties.json`：

```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/include",
                "${workspaceFolder}/src"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "c11",
            "intelliSenseMode": "linux-gcc"
        }
    ]
}
```

### CMake Tools 扩展

推荐安装 CMake Tools 扩展，提供：

- 可视化 CMake 配置
- 快速切换构建类型
- 集成测试运行

### Git 配置

```bash
# 忽略构建产物
echo "build/" >> .gitignore
echo "*.so" >> .gitignore
echo "*.dylib" >> .gitignore
echo "*.dll" >> .gitignore
echo "*.exe" >> .gitignore
```

### IDE 集成

#### Visual Studio 2022

```cmake
# 生成 Visual Studio 项目
cmake -B build -G "Visual Studio 17 2022" -A x64
```

#### CLion

1. 打开 `CMakeLists.txt`
2. 配置 CMake 工具链
3. 设置包含路径和链接器选项

## 代码结构

### 目录布局

```
Kronyx/
├── CMakeLists.txt                 # 顶层构建配置
├── cmake/
│   └── options.cmake              # 构建选项定义
├── include/kronyx/                # 公开头文件
│   ├── defines.h                 # 基础宏定义
│   ├── math.h                    # 数学库
│   ├── memory.h                  # 内存管理
│   ├── ecs.h                     # 实体组件系统
│   ├── render.h                  # 渲染接口
│   ├── script.h                  # 脚本 VM
│   └── ...                       # 其他子系统
├── src/
│   ├── core/                     # 核心工具
│   │   ├── math.c                # 数学实现
│   │   ├── memory.c              # 内存管理实现
│   │   ├── array.c               # 动态数组
│   │   └── hashmap.c             # 哈希映射
│   ├── ecs/                      # 实体组件系统
│   │   └── ecs.c                 # ECS 实现
│   ├── render/                   # 渲染系统
│   │   ├── render.c              # RHI 核心
│   │   ├── console_backend.c     # 控制台后端
│   │   ├── gl_backend.c          # OpenGL 后端
│   │   ├── 2d.c                   # 2D 渲染
│   │   └── anim2d.c              # 帧动画
│   ├── script/                   # 脚本系统
│   │   ├── script.c              # VM 核心
│   │   ├── parser.c              # 词法/语法分析
│   │   ├── vm.c                  # 虚拟机实现
│   │   ├── gc.c                  # 垃圾回收
│   │   └── bindings.c            # 脚本绑定
│   └── demo/                     # 演示程序
│       └── main.c                # 主程序入口
├── tests/                        # 测试套件
├── tools/                        # 开发工具
└── assets/                       # 资源文件
```

### 命名约定

#### 文件命名

- 头文件：`kebab-case.h`（如 `entity-component-system.h`）
- 源文件：`kebab-case.c`（如 `entity-component-system.c`）
- 测试文件：`test_*.c`（如 `test_ecs.c`）

#### 函数命名

- 公共 API：`ky_module_function()`（如 `ky_world_create()`）
- 内部函数：`ky_module_internal_function()`（如 `ky_world_internal_create()`）
- 静态函数：`static void function()`

#### 类型命名

- 结构体：`kyPascalCase`（如 `kyWorld`）
- 枚举：`kyModulePascalCase`（如 `kyRendererBackend`）
- 函数指针：`kyModuleFunctionPtr`（如 `kyUpdateFunctionPtr`）

#### 变量命名

- 全局变量：`g_global_variable`
- 成员变量：`m_member_variable`
- 局部变量：`local_variable`
- 静态变量：`s_static_variable`

## API 使用指南

### 基本初始化

```c
#include "kronyx/kronyx.h"

int main(void) {
    // 创建分配器
    kyAllocator alloc = ky_default_allocator();
    
    // 创建世界
    kyWorld *world = ky_world_create(&alloc);
    
    // 注册组件类型
    typedef struct Transform {
        kyVec3 position;
        kyVec3 scale;
    } Transform;
    
    kyComponentType transform_type = {
        .name = "transform",
        .size = sizeof(Transform),
        .ctor = NULL,
        .dtor = NULL
    };
    
    uint32_t tid_transform = ky_world_register_component(world, &transform_type);
    
    // 创建实体
    kyEntity entity = ky_world_spawn(world);
    
    // 添加组件
    Transform *transform = ky_world_add_component(world, entity, tid_transform);
    transform->position = (kyVec3){0, 0, 0};
    transform->scale = (kyVec3){1, 1, 1};
    
    // ... 游戏逻辑 ...
    
    // 清理
    ky_world_destroy(world);
    return 0;
}
```

### ECS 最佳实践

#### 组件设计

```c
// 轻量级组件（适合频繁创建/销毁）
typedef struct Health {
    float current;
    float max;
} Health;

// 数据导向组件（适合批量操作）
typedef struct Transform {
    kyVec3 position;
    kyVec2 scale;
    float rotation;
} Transform;

// 行为组件（包含逻辑函数）
typedef struct Movement {
    kyVec2 velocity;
    float speed;
    void (*update)(struct Movement*, float dt);
} Movement;
```

#### 视图模式使用

```c
// 获取所有带有 Transform 和 Health 的实体
uint32_t types[] = {tid_transform, tid_health};
kyViewIter it;
if (ky_view_begin(world, types, 2, &it)) {
    while (ky_view_next(&it)) {
        Transform *t = ky_world_get_component(world, it.current, tid_transform);
        Health *h = ky_world_get_component(world, it.current, tid_health);
        
        // 批量处理所有实体
        printf("Entity %d: pos=(%.1f,%.1f), health=%.1f\n", 
               it.current.id, t->position.x, t->position.y, h->current);
    }
}
```

#### 系统实现

```c
// 系统更新函数
void movement_system(kyWorld *world, float dt, void *user) {
    uint32_t tid_movement = *(uint32_t*)user;
    
    kyViewIter it;
    if (ky_view_begin(world, &tid_movement, 1, &it)) {
        while (ky_view_next(&it)) {
            Movement *m = ky_world_get_component(world, it.current, tid_movement);
            if (m && m->update) {
                m->update(m, dt);
            }
        }
    }
}

// 注册系统
kySystem system = {
    .name = "movement",
    .order = 1,
    .update = movement_system,
    .user = &tid_movement
};
ky_world_register_system(world, &system);
```

### 渲染系统使用

#### 基本渲染设置

```c
// 创建渲染设备
kyRenderDevice *rd = ky_rd_create(KY_RENDERER_CONSOLE, NULL);

// 创建着色器
kyShaderSource shader_src = {
    .vs = "attribute vec2 a_pos; void main() { gl_Position = vec4(a_pos, 0, 1); }",
    .fs = "void main() { gl_FragColor = vec4(1, 0, 0, 1); }",
    .entry = "main"
};
kyShader *shader = ky_rd_create_shader(rd, &shader_src);

// 创建管线
kyVertexLayout layout = {
    .attribs = (kyVertexDescriptor[]){
        {0, 0, 2, 0, KY_ATTRIB_FLOAT}
    },
    .count = 1,
    .stride = 8
};

kyPipelineDesc pipeline_desc = {
    .shader = shader,
    .layout = layout,
    .topology = KY_TRIANGLES
};
kyPipeline *pipeline = ky_rd_create_pipeline(rd, &pipeline_desc);
```

#### 2D 渲染

```c
// 注册2D组件
ky2d_register_components(world);

// 创建精灵实体
kyEntity sprite = ky_world_spawn(world);
Transform *t = ky_world_add_component(world, sprite, tid_transform);
t->position = (kyVec3){0, 0, 0};

Sprite *s = ky_world_add_component(world, sprite, tid_sprite);
s->texture = texture;
s->layer = 1;
s->color = (kyVec4){1, 1, 1, 1};

// 渲染循环
void render(kyWorld *world, kyRenderDevice *rd) {
    ky_rd_clear(rd, (kyVec4){0.1f, 0.1f, 0.1f, 1.0f}, 1.0f);
    ky2d_render_world_auto(rd, world);
    ky_rd_present(rd);
}
```

### 脚本系统集成

#### 脚本绑定

```c
// 注册原生函数
void kyx_bindings_register(kyVM *vm) {
    // 注册数学函数
    ky_vm_register(vm, "vec3_add", kyx_vec3_add, 2);
    
    // 注册实体操作
    ky_vm_register(vm, "spawn_entity", kyx_spawn_entity, 0);
    
    // 注册全局变量
    ky_vm_set_global(vm, "delta_time", KYT_FLOAT, &delta_time);
}

// 脚本执行
int execute_script(kyWorld *world, const char *script_path) {
    kyVM *vm = ky_vm_create(&ky_default_allocator());
    
    // 加载并编译脚本
    if (ky_vm_compile(vm, script_path) != 0) {
        printf("Script compilation failed\n");
        return -1;
    }
    
    // 绑定世界到脚本
    kyx_bind_world(vm, world);
    
    // 执行脚本
    int result = ky_vm_execute(vm);
    
    ky_vm_destroy(vm);
    return result;
}
```

## 测试框架

### 测试结构

测试框架基于 CTest，使用自研断言宏 `kytest.h`：

```c
#include "kronyx/kytest.h"

int test_ecs_basic(void) {
    kyWorld *w = ky_world_create(&ky_default_allocator());
    
    // 注册测试组件
    typedef struct TestComponent { int value; } TestComponent;
    kyComponentType ct = {
        .name = "test",
        .size = sizeof(TestComponent),
        .ctor = NULL,
        .dtor = NULL
    };
    uint32_t tid = ky_world_register_component(w, &ct);
    
    // 测试实体创建
    kyEntity e = ky_world_spawn(w);
    KY_ASSERT(ky_entity_valid(w, e) == 1);
    
    // 测试组件添加
    TestComponent *tc = ky_world_add_component(w, e, tid);
    tc->value = 42;
    KY_ASSERT(tc->value == 42);
    
    ky_world_destroy(w);
    return 0;
}
```

### 运行测试

```bash
# 运行所有测试
ctest --test-dir build --output-on-failure

# 运行特定测试
ctest --test-dir build -R "ecs"

# 并行运行测试
ctest --test-dir build --parallel 4

# 生成测试报告
ctest --test-dir build --output-on-failure --verbose > test_report.txt
```

### 性能测试

```c
#include <time.h>

void test_ecs_performance(void) {
    kyWorld *w = ky_world_create(&ky_default_allocator());
    
    // 注册组件
    typedef struct Pos { float x, y; } Pos;
    kyComponentType ct = {
        .name = "position",
        .size = sizeof(Pos),
        .ctor = NULL,
        .dtor = NULL
    };
    uint32_t tid = ky_world_register_component(w, &ct);
    
    // 性能测试：创建10000个实体
    clock_t start = clock();
    for (int i = 0; i < 10000; i++) {
        kyEntity e = ky_world_spawn(w);
        Pos *p = ky_world_add_component(w, e, tid);
        p->x = i;
        p->y = i * 2;
    }
    clock_t end = clock();
    
    double time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Created 10000 entities in %.3f seconds\n", time_used);
    
    ky_world_destroy(w);
}
```

## 调试技巧

### 编译时调试

#### 启用断言

```bash
# Debug 构建自动启用断言
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 强制启用断言
cmake -B build -DKY_ENABLE_ASSERTS=ON
```

#### 启用日志

```c
// 设置日志级别
ky_log_set_level(KY_LOG_DEBUG);

// 自定义日志输出
ky_log_set_callback(my_log_function);
```

### 运行时调试

#### 内存调试

```bash
# 使用 ASan
cmake -B build -DKYR_ENABLE_SANITIZERS=ON
cmake --build build

# 运行测试
LSAN_OPTIONS=suppressions=$(pwd)/lsan.supp ctest --test-dir build
```

#### 调试宏

```c
// 打印实体信息
#define KY_DEBUG_ENTITY(w, e) \
    printf("Entity %d:%d valid=%d\n", e.id, e.version, ky_entity_valid(w, e))

// 打印组件数据
#define KY_DEBUG_COMPONENT(w, e, tid, type) \
    type *comp = ky_world_get_component(w, e, tid); \
    if (comp) printf("Component: %s\n", #type)
```

### GDB 调试

```bash
# 编译调试版本
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-g3"

# 使用 GDB 调试
gdb build/ky_demo

# GDB 命令
(gdb) break main
(gdb) run
(gdb) print w->alive_count
(gdb) bt
```

### Valgrind 内存检查

```bash
# 内存检查
valgrind --leak-check=full --show-leak-kinds=all build/ky_demo

# 线程检查
valgrind --tool=helgrind build/ky_demo
```

## 性能优化

### ECS 优化

#### Archetype 利用

```c
// 批量创建相似组件的实体
void create_batch_entities(kyWorld *w, int count, uint32_t component_types[]) {
    for (int i = 0; i < count; i++) {
        kyEntity e = ky_world_spawn(w);
        for (int j = 0; j < 3; j++) {
            if (component_types[j] != UINT32_MAX) {
                ky_world_add_component(w, e, component_types[j]);
            }
        }
    }
}
```

#### 视图迭代优化

```c
// 缓存查询结果
typedef struct CachedView {
    kyViewIter iter;
    uint32_t types[4];
    uint32_t type_count;
    int valid;
} CachedView;

CachedView *cache_view(kyWorld *w, uint32_t *types, int count) {
    CachedView *cv = malloc(sizeof(CachedView));
    cv->type_count = count;
    memcpy(cv->types, types, count * sizeof(uint32_t));
    cv->valid = ky_view_begin(w, types, count, &cv->iter);
    return cv;
}
```

### 渲染优化

#### 批次渲染

```c
// 按图层和纹理批处理
void batch_render(kyWorld *w, kyRenderDevice *rd) {
    // 创建批次表
    typedef struct Batch {
        kyTexture *texture;
        int layer;
        kyArray vertices;
        kyArray indices;
    } Batch;
    
    kyArray batches = ky_array_create(sizeof(Batch), &ky_default_allocator());
    
    // 收集精灵到批次
    uint32_t tid_sprite = ky_world_component_type_by_name(w, "sprite");
    kyViewIter it;
    if (ky_view_begin(w, &tid_sprite, 1, &it)) {
        while (ky_view_next(&it)) {
            Sprite *s = ky_world_get_component(w, it.current, tid_sprite);
            // ... 分组逻辑 ...
        }
    }
    
    // 渲染批次
    for (int i = 0; i < batches.count; i++) {
        Batch *batch = &((Batch*)batches.data)[i];
        // ... 渲染批次 ...
    }
    
    ky_array_destroy(&batches);
}
```

#### 纹理图集

```c
// 创建纹理图集
typedef struct TextureAtlas {
    kyTexture *atlas;
    int width, height;
    struct { int x, y, w, h; } *rects;
    int rect_count;
} TextureAtlas;

TextureAtlas *create_atlas(kyAllocator *alloc, int width, int height, int max_textures) {
    TextureAtlas *atlas = ky_alloc(alloc, sizeof(TextureAtlas));
    atlas->width = width;
    atlas->height = height;
    atlas->rects = ky_alloc(alloc, max_textures * sizeof(struct { int x, y, w, h; }));
    atlas->rect_count = 0;
    return atlas;
}
```

### 内存优化

#### Arena 分配器

```c
// 使用 Arena 分配临时数据
void process_frame(kyWorld *w) {
    static kyArena *frame_arena = NULL;
    if (!frame_arena) {
        frame_arena = ky_arena_create(&ky_default_allocator(), 1024 * 1024);
    }
    
    ky_arena_reset(frame_arena);
    
    // 在 arena 中分配临时数据
    kyArray entities = ky_array_create(sizeof(kyEntity), frame_arena);
    // ... 处理逻辑 ...
    
    // Arena 会在下一帧自动重置，无需手动释放
}
```

#### 对象池

```c
typedef struct GameObjectPool {
    kyArray active;
    kyArray inactive;
    kyAllocator *alloc;
} GameObjectPool;

GameObjectPool *create_pool(kyAllocator *alloc, int initial_size) {
    GameObjectPool *pool = ky_alloc(alloc, sizeof(GameObjectPool));
    pool->alloc = alloc;
    pool->active = ky_array_create(sizeof(GameObject*), alloc);
    pool->inactive = ky_array_create(sizeof(GameObject*), alloc);
    
    // 预分配对象
    for (int i = 0; i < initial_size; i++) {
        GameObject *obj = ky_alloc(alloc, sizeof(GameObject));
        ky_array_push(&pool->inactive, &obj);
    }
    
    return pool;
}
```

## 跨平台开发

### 平台差异处理

```c
// 平台特定的实现
#if KY_PLATFORM_WIN32
    #include <windows.h>
    #define KY_PLATFORM_SLEEP(ms) Sleep(ms)
#elif KY_PLATFORM_LINUX
    #include <unistd.h>
    #define KY_PLATFORM_SLEEP(ms) usleep((ms) * 1000)
#elif KY_PLATFORM_MACOS
    #include <unistd.h>
    #define KY_PLATFORM_SLEEP(ms) usleep((ms) * 1000)
#endif

// 平台特定的头文件包含
#if KY_PLATFORM_WIN32
    #include <bcrypt.h>
    typedef BCRYPT_ALG_HANDLE HmacHandle;
#elif KY_PLATFORM_LINUX
    #include <openssl/evp.h>
    typedef EVP_PKEY_CTX* HmacHandle;
#endif
```

### 构建 配置

#### Windows 特定配置

```cmake
# CMakeLists.txt
if(WIN32)
    # Windows 特定设置
    target_compile_definitions(ky_engine PRIVATE WIN32_LEAN_AND_MEAN)
    target_link_libraries(ky_engine PRIVATE bcrypt crypt32)
endif()
```

#### Linux 特定配置

```cmake
if(UNIX AND NOT APPLE)
    target_link_libraries(ky_runtime PUBLIC X11 Xi Xrandrt)
    target_compile_definitions(ky_engine PRIVATE LINUX)
endif()
```

#### macOS 特定配置

```cmake
if(APPLE)
    target_link_libraries(ky_runtime PRIVATE "-framework Cocoa" "-framework OpenGL")
    target_compile_definitions(ky_engine PRIVATE MACOS)
endif()
```

### 交叉编译

#### Linux 到 Windows

```bash
# 使用 MinGW-w64 交叉编译器
apt install gcc-mingw-w64

# 配置交叉编译
cmake -B build_win64 -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++

# 构建
cmake --build build_win64
```

#### 构建脚本

```bash
#!/bin/bash
# cross-build.sh

echo "Building for Linux..."
cmake -B build_linux && cmake --build build_linux

echo "Building for Windows..."
cmake -B build_win64 -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
cmake --build build_win64

echo "Building for macOS..."
cmake -B build_mac -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build_mac

echo "Build complete!"
```

## 贡献指南

### 代码风格

#### 格式化规则

- 使用 4 空格缩进（不使用制表符）
- 最大行长度 80 字符
- 函数参数换行时对齐
- 运算符前后加空格

#### 代码示例

```c
// 良好的格式
int ky_world_register_component(kyWorld *w, const kyComponentType *t) {
    KY_ASSERT(w != NULL);
    KY_ASSERT(t != NULL);
    
    if (w->component_types.count >= KY_MAX_COMPONENT_TYPES) {
        KY_LOG_ERROR("Component type limit reached");
        return UINT32_MAX;
    }
    
    // 注册组件类型
    kyComponentType *copy = ky_array_push(&w->component_types);
    *copy = *t;
    copy->type_id = w->component_types.count - 1;
    
    // 添加到名称缓存
    if (t->name) {
        ky_hashmap_set(&w->name_cache, t->name, &copy->type_id);
    }
    
    return copy->type_id;
}
```

### 提交规范

#### 提交消息格式

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

#### 类型说明

- `feat`: 新功能
- `fix`: 修复bug
- `docs`: 文档更新
- `style`: 代码格式化
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建或工具相关

#### 示例

```
feat(ecs): add archetype component migration API

This adds support for moving entities between archetypes while preserving
existing components. The migration uses O(1) lookup and efficient memory
swapping.

Fixes memory leaks when removing components during migration.

BREAKING CHANGE: ky_world_add_component signature changed to return the
component pointer directly.
```

### 开发流程

1. ** Fork 仓库**
2. ** 创建特性分支**
   ```bash
   git checkout -b feature/ecs-component-migration
   ```
3. ** 编写代码和测试**
4. ** 运行测试套件**
   ```bash
   ctest --test-dir build --output-on-failure
   ```
5. ** 提交变更**
   ```bash
   git add .
   git commit -m "feat(ecs): add archetype component migration API"
   ```
6. ** 推送到分支**
   ```bash
   git push origin feature/ecs-component-migration
   ```
7. ** 创建 Pull Request**

### 代码审查清单

- [ ] 代码符合项目风格指南
- [ ] 添加了适当的测试
- [ ] 文档已更新
- [ ] 性能测试通过
- [ ] 内存泄漏检查通过
- [ ] 跨平台兼容性验证
- [ ] API 向后兼容性检查

### 问题报告

使用 GitHub Issues 报告问题，包含：

1. **问题描述**
2. **重现步骤**
3. **期望行为**
4. **实际行为**
5. **环境信息**
6. **最小可复现示例**

### 版本发布

版本号遵循 SemVer 规范：

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能添加
- **PATCH**: 向后兼容的 bug 修复

发布流程：

1. 更新版本号
2. 更新 CHANGELOG.md
3. 创建发布标签
4. 构建、测试所有平台
5. 发布到 GitHub Releases

### 常见问题

#### Q: 如何调试内存泄漏？

A: 使用 ASan/UBSan 和 Valgrind：

```bash
# 启用 AddressSanitizer
cmake -B build -DKYR_ENABLE_SANITIZERS=ON
cmake --build build

# 使用 Valgrind 检查
valgrind --leak-check=full --show-leak-kinds=all build/ky_demo
```

#### Q: 如何添加新的渲染后端？

A: 1. 实现后端特定的 RHI 函数
2. 在 `kyRendererBackend` 枚举中添加新后端
3. 在 `ky_rd_create` 中添加创建逻辑
4. 更新构建配置

#### Q: 如何自定义组件系统？

A: 继承基础 ECS 并扩展：

```c
typedef struct CustomWorld {
    kyWorld base;
    kyCustomSystem custom_systems[MAX_CUSTOM_SYSTEMS];
} CustomWorld;

CustomWorld *custom_world_create(kyAllocator *alloc) {
    CustomWorld *cw = ky_alloc(alloc, sizeof(CustomWorld));
    cw->base = *ky_world_create(alloc);
    return cw;
}
```

### 相关资源

- **文档**: [ARCHITECTURE.md](./ARCHITECTURE.md)
- **API 参考**: [INTERFACES.md](./INTERFACES.md)
- **测试框架**: [GitHub Wiki](https://github.com/your-org/kronyx/wiki/Testing)
- **贡献指南**: [GitHub Contributing](https://github.com/your-org/kronyx/CONTRIBUTING.md)

---

*本文档基于 Kronyx Engine 0.1.0 版本编写，如有更新请参考最新代码和文档。*