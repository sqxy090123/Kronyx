# Engine 运行时

## 概述

Engine 运行时层是 Kronyx Engine 的运行时系统，提供窗口管理、游戏循环、输入处理和时间管理功能。基于 GLFW 3.3 构建，支持跨平台窗口创建和事件处理，与实体组件系统、渲染和物理系统集成。

## 架构设计

### 核心组件

#### 窗口管理 (`kyEngineWindow`)
```c
// 窗口句柄 (不透明结构)
typedef struct kyEngineWindow kyEngineWindow;
```

#### 初始化和清理
```c
// 引擎初始化
// title 可为 NULL，返回 0 表示成功
KY_API int ky_engine_init(int width, int height, const char *title);

// 引擎关闭
KY_API void ky_engine_shutdown(void);
```

#### 游戏循环
```c
// 游戏循环原型
typedef float (*kyEngineUpdate)(float dt);
typedef void (*kyEngineRender)(void);

// 主循环
// update(dt) 每帧调用，render() 每帧调用，直到窗口关闭
// 两个函数指针都可以为 NULL（视为无操作）
KY_API int ky_engine_run(float (*update)(float dt), void (*render)(void));
```

#### 窗口查询
```c
// 查询窗口尺寸（可能因调整大小而改变）
KY_API float ky_engine_width(void);
KY_API float ky_engine_height(void);
```

### 输入系统

#### 键盘输入
```c
// 轮询 GLFW 按键状态
KY_API int ky_engine_key_pressed(int key);

// 按键常量 (来自 GLFW)
#define GLFW_KEY_SPACE      32
#define GLFW_KEY_A          65
#define GLFW_KEY_ESCAPE      256
#define GLFW_KEY_UP          265
#define GLFW_KEY_DOWN        264
```

#### 输入更新
```c
// 输入轮询
KY_API void ky_input_poll(void);

// 输入更新
KY_API void ky_input_update(void);
```

### 反篡改集成
```c
// 包含反篡改 API
#include "anti_tamper.h"

// 反篡改初始化和验证
KY_API int ky_tamper_init(kyAntiTamperMode mode);
KY_API int ky_tamper_verify(void);
```

## API 参考

### 引擎生命周期

#### 初始化引擎
```c
// 基本初始化
int result = ky_engine_init(800, 600, "My Game");
if (result != 0) {
    printf("Engine initialization failed: %d\n", result);
    return -1;
}
```

#### 游戏循环
```c
// 自定义更新和渲染函数
float game_update(float dt) {
    // 游戏逻辑更新
    static float player_x = 0;
    player_x += dt * 100;  // 每秒移动 100 像素
    return dt;  // 返回时间增量
}

void game_render(void) {
    // 渲染游戏
    // 清除屏幕等
}

// 运行游戏循环
ky_engine_run(game_update, game_render);
```

#### 关闭引擎
```c
// 关闭引擎和销毁窗口
ky_engine_shutdown();
```

### 窗口管理

#### 窗口属性
```c
// 获取窗口尺寸
float w = ky_engine_width();
float h = ky_engine_height();

// 检查窗口是否应该关闭
int should_close = glfwWindowShouldGetCurrentWindow();
```

#### 窗口事件
```c
// 窗口大小改变回调
void on_window_resize(int width, int height) {
    // 更新视口等
}

// 设置窗口属性
void ky_engine_set_window_title(const char *title);
void ky_engine_set_window_icon(const char *icon_path);
```

### 输入处理

#### 键盘输入
```c
// 键盘状态检查
if (ky_engine_key_pressed(GLFW_KEY_SPACE)) {
    // 空格键被按下
}

if (ky_engine_key_pressed(GLFW_KEY_ESCAPE)) {
    // ESC 键被按下
}
```

#### 输入状态查询
```c
// 获取当前按键状态
int key_state = ky_input_key_pressed(GLFW_KEY_A);
if (key_state) {
    // A 键被按下
}

// 获取鼠标状态
int mouse_button = ky_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT);
int mouse_x, mouse_y;
ky_mouse_get_pos(&mouse_x, &mouse_y);
```

### 时间管理

#### 游戏时间
```c
// 获取当前时间
kyTime current_time = ky_time_now();

// 计算时间差
kyTime start_time = ky_time_now();
// ... 执行一些操作 ...
kyTime end_time = ky_time_now();
kyTime delta = ky_time_delta(start_time, end_time);
```

#### 帧率控制
```c
// 简单的帧率控制
void run_with_fps(float target_fps) {
    float frame_time = 1.0f / target_fps;
    float accumulator = 0;
    kyTime last_time = ky_time_now();
    
    while (!should_exit) {
        kyTime current_time = ky_time_now();
        float dt = ky_time_delta(last_time, current_time);
        last_time = current_time;
        
        accumulator += dt;
        
        while (accumulator >= frame_time) {
            update(frame_time);  // 固定时间步进
            accumulator -= frame_time;
        }
        
        render();
    }
}
```

## 配置选项

### 引擎配置
```c
// 窗口配置
#define KY_DEFAULT_WIDTH 800
#define KY_DEFAULT_HEIGHT 600
#define KY_DEFAULT_TITLE "Kronyx Engine"

// 反篡改配置
typedef enum kyAntiTamperMode {
    KY_ANTITEMPER_SILENT = 0,  // 静默模式
    KY_ANTITEMPER_WARNING,     // 警告模式
    KY_ANTITEMPER_ERROR        // 错误模式
} kyAntiTamperMode;
```

### 构建选项
```cmake
# GLFW 依赖配置
if(WIN32)
    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        target_link_libraries(ky_runtime PUBLIC glfw)
    else()
        message(WARNING "GLFW3 not found, ky_runtime will be stub")
    endif()
else()
    find_library(GLFW3_LIBRARY glfw PATHS /usr/lib/x86_64-linux-gnu /opt/homebrew/lib)
    find_path(GLFW3_INCLUDE_DIR GLFW/glfw3.h PATHS /usr/include /opt/homebrew/include)
    target_link_libraries(ky_runtime PUBLIC ${GLFW3_LIBRARY})
    target_include_directories(ky_runtime PUBLIC ${GLFW3_INCLUDE_DIR})
endif()
```

## 使用示例

### 基本游戏循环
```c
#include "kronyx/engine.h"
#include "kronyx/input.h"
#include "kronyx/math.h"

// 游戏状态
typedef struct {
    float player_x;
    float player_y;
    float velocity;
} GameState;

GameState game_state = {0};

// 更新函数
float update(float dt) {
    // 处理输入
    if (ky_engine_key_pressed(GLFW_KEY_LEFT)) {
        game_state.velocity = -200;
    } else if (ky_engine_key_pressed(GLFW_KEY_RIGHT)) {
        game_state.velocity = 200;
    } else {
        game_state.velocity = 0;
    }
    
    // 更新位置
    game_state.player_x += game_state.velocity * dt;
    
    // 边界检查
    float screen_width = ky_engine_width();
    if (game_state.player_x < 0) game_state.player_x = 0;
    if (game_state.player_x > screen_width) game_state.player_x = screen_width;
    
    return dt;
}

// 渲染函数
void render(void) {
    // 简单的渲染实现
    // 这里应该调用渲染系统的函数
    // 清除屏幕、绘制玩家等
    printf("Rendering player at: %f\n", game_state.player_x);
}

int main() {
    // 初始化引擎
    if (ky_engine_init(800, 600, "Simple Game") != 0) {
        printf("Failed to initialize engine\n");
        return -1;
    }
    
    // 设置窗口图标
    ky_engine_set_window_icon("assets/icon.png");
    
    // 运行游戏循环
    ky_engine_run(update, render);
    
    // 关闭引擎
    ky_engine_shutdown();
    
    return 0;
}
```

### 多窗口支持
```c
// 创建多个窗口
kyEngineWindow* window1, *window2;

// 窗口1
ky_engine_init_window(640, 480, "Window 1", &window1);

// 窗口2
ky_engine_init_window(320, 240, "Window 2", &window2);

// 多窗口更新
void multi_window_update(float dt) {
    // 更新窗口1的内容
    ky_engine_set_window(window1);
    update_window1(dt);
    
    // 更新窗口2的内容
    ky_engine_set_window(window2);
    update_window2(dt);
    
    return dt;
}

void multi_window_render(void) {
    // 渲染窗口1
    ky_engine_set_window(window1);
    render_window1();
    
    // 渲染窗口2
    ky_engine_set_window(window2);
    render_window2();
}
```

### 反篡改集成
```c
int main() {
    // 初始化反篡改
    int result = ky_tamper_init(KY_ANTITEMPER_WARNING);
    if (result != 0) {
        printf("Anti-tamper init failed: %d\n", result);
        return -1;
    }
    
    // 运行游戏
    if (ky_engine_init(800, 600, "Protected Game") != 0) {
        printf("Engine init failed\n");
        return -1;
    }
    
    // 定期验证
    float verification_timer = 0;
    float update(float dt) {
        verification_timer += dt;
        if (verification_timer >= 5.0f) {  // 每5秒验证一次
            verification_timer = 0;
            if (ky_tamper_verify() != 0) {
                printf("Tampering detected!\n");
                // 处理篡改
            }
        }
        return dt;
    }
    
    ky_engine_run(update, NULL);
    ky_engine_shutdown();
    
    return 0;
}
```

## 性能优化

### 游戏循环优化

#### 固定时间步进
```c
// 固定时间步进避免物理抖动
#define FIXED_TIMESTEP (1.0f / 60.0f)

void run_fixed_timestep() {
    float accumulator = 0;
    kyTime current_time = ky_time_now();
    
    while (!should_exit) {
        kyTime new_time = ky_time_now();
        float frame_time = ky_time_delta(current_time, new_time);
        current_time = new_time;
        
        accumulator += frame_time;
        
        while (accumulator >= FIXED_TIMESTEP) {
            update(FIXED_TIMESTEP);
            accumulator -= FIXED_TIMESTEP;
        }
        
        render();
    }
}
```

#### 帧率限制
```c
// 可配置的帧率限制
void run_with_frame_limit(float target_fps) {
    float target_frame_time = 1.0f / target_fps;
    float last_frame_time = ky_time_now();
    
    while (!should_exit) {
        kyTime start_time = ky_time_now();
        
        update(get_delta_time());
        render();
        
        // 帧率控制
        kyTime end_time = ky_time_now();
        float frame_time = ky_time_delta(start_time, end_time);
        float sleep_time = target_frame_time - frame_time;
        
        if (sleep_time > 0) {
            // 精确睡眠时间
            ky_sleep_us(sleep_time * 1000000);
        }
    }
}
```

### 内存管理优化

```c
// 使用内存池预分配
typedef struct {
    float x, y;
    float vx, vy;
    int active;
} GameObject;

#define MAX_GAME_OBJECTS 1024
GameObject game_objects[MAX_GAME_OBJECTS];
int object_count = 0;

GameObject* allocate_game_object(void) {
    if (object_count >= MAX_GAME_OBJECTS) {
        return NULL;
    }
    
    GameObject* obj = &game_objects[object_count++];
    obj->active = 1;
    return obj;
}
```

## 平台支持

### Windows 平台
```cmake
# Windows 平台特定配置
if(WIN32)
    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        target_link_libraries(ky_runtime PUBLIC glfw)
        target_compile_definitions(ky_runtime PUBLIC KY_BUILD_WINDOWS)
    else()
        message(WARNING "GLFW3 not found via vcpkg")
    endif()
endif()
```

### Linux 平台
```cmake
# Linux 平台特定配置
if(UNIX AND NOT APPLE)
    target_link_libraries(ky_runtime PUBLIC X11 Xi Xrandr)
    target_compile_definitions(ky_runtime PUBLIC KY_BUILD_LINUX)
endif()
```

### macOS 平台
```cmake
# macOS 平台特定配置
if(APPLE)
    target_link_libraries(ky_runtime PUBLIC "-framework Cocoa" "-framework OpenGL")
    target_compile_definitions(ky_runtime PUBLIC KY_BUILD_MACOS)
endif()
```

## 故障排除

### 常见问题

#### 窗口创建失败
```bash
# 问题：窗口创建失败
# 解决方案：
# 1. 检查 GLFW 是否正确安装
# 2. 验证图形驱动
# 3. 检查平台特定依赖

# Linux
sudo apt-get install libglfw3-dev

# Windows
# 使用 vcpkg 安装 GLFW
vcpkg install glfw3
```

#### 输入无响应
```bash
# 问题：按键没有响应
# 解决方案：
# 1. 确保调用 ky_input_poll()
# 2. 检查窗口焦点
# 3. 验证按键常量

// 确保输入轮询
ky_input_poll();
if (ky_engine_key_pressed(GLFW_KEY_SPACE)) {
    // 处理空格键
}
```

#### 反篡误报
```bash
# 问题：合法修改被误判为篡改
# 解决方案：
# 1. 使用 KY_ANTITEMPER_SILENT 模式
// 静默模式
ky_tamper_init(KY_ANTITEMPER_SILENT);

// 2. 配置白名单路径
```

### 调试工具

#### 性能分析
```c
// 启用性能日志
#define KY_ENABLE_PERFORMANCE_LOGGING 1

// 帧率显示
void show_fps(float dt) {
    static float fps_timer = 0;
    static int frame_count = 0;
    static float fps = 0;
    
    fps_timer += dt;
    frame_count++;
    
    if (fps_timer >= 1.0f) {
        fps = frame_count / fps_timer;
        printf("FPS: %.2f\n", fps);
        fps_timer = 0;
        frame_count = 0;
    }
}
```

#### 内存监控
```c
// 内存使用监控
void monitor_memory(void) {
    size_t memory_usage = ky_vm_get_memory_usage(vm);
    printf("Memory usage: %zu KB\n", memory_usage / 1024);
}
```

## 扩展功能

### 自定义输入处理
```c
// 自定义输入映射
typedef struct {
    int glfw_key;
    int game_action;
} InputMapping;

InputMapping mappings[] = {
    {GLFW_KEY_W, GAME_ACTION_MOVE_UP},
    {GLFW_KEY_S, GAME_ACTION_MOVE_DOWN},
    {GLFW_KEY_A, GAME_ACTION_MOVE_LEFT},
    {GLFW_KEY_D, GAME_ACTION_MOVE_RIGHT}
};

int get_game_action(int glfw_key) {
    for (int i = 0; i < 4; i++) {
        if (mappings[i].glfw_key == glfw_key) {
            return mappings[i].game_action;
        }
    }
    return GAME_ACTION_NONE;
}
```

### 多线程支持
```c
// 多线程渲染和更新
void run_multithreaded(void) {
    // 渲染线程
    pthread_t render_thread;
    pthread_create(&render_thread, NULL, render_thread_func, NULL);
    
    // 主线程处理更新和输入
    while (!should_exit) {
        float dt = get_delta_time();
        update(dt);
        process_input();
    }
    
    pthread_join(render_thread, NULL);
}
```

---

**相关文档**：
- [ARCHITECTURE.md](../ARCHITECTURE.md) - 系统架构设计
- [GLFW.md](./GLFW.md) - GLFW 窗口系统
- [Input.md](./Input.md) - 输入系统详细说明
- [Anti-Tamper.md](../anti_tamper.md) - 反篡改系统