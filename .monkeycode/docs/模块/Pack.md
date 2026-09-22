# Pack 打包工具

## 概述

Pack 工具是 Kronyx Engine 的跨平台打包系统，可以将 `kyx` 脚本与引擎打包为可执行文件、npm 包或 JAR 文件。支持静态链接引擎代码，生成独立运行的应用程序。

## 架构设计

### 打包目标

Pack 工具支持多种输出格式：

```c
typedef enum kyPackTarget {
    KY_PACK_EXE = 0,   // 可执行文件
    KY_PACK_NPM,       // npm 包
    KY_PACK_JAR,       // Java JAR 包
    KY_PACK_APK        // Android APK (占位未实现)
} kyPackTarget;
```

### 打包描述符

```c
typedef struct kyPackDesc {
    const char *title;      // 应用标题
    const char *version;    // 版本号
    const char *script;    // 主脚本文件路径
    const char *out_path;   // 输出路径
} kyPackDesc;
```

### 核心流程

打包过程包含以下步骤：
1. **资源收集**：收集脚本、资源文件和引擎库
2. **静态链接**：将引擎核心和运行时静态链接
3. **目标生成**：根据目标格式生成相应文件
4. **元数据注入**：注入应用信息和配置

## API 参考

### 打包函数

#### 获取目标名称
```c
// 获取打包目标的名称字符串
KY_API const char *ky_pack_target_name(kyPackTarget target);
```

#### 执行打包
```c
// 执行打包操作
KY_API int ky_pack(const kyPackDesc *desc, kyPackTarget target, char *err, int err_size);
```

## 使用示例

### 可执行文件打包
```c
// 创建打包描述符
kyPackDesc desc = {
    .title = "My Game",
    .version = "1.0.0",
    .script = "src/game/main.kyx",
    .out_path = "build/game.exe"
};

// 执行打包
char error_buffer[256];
if (ky_pack(&desc, KY_PACK_EXE, error_buffer, sizeof(error_buffer)) != 0) {
    printf("Pack failed: %s\n", error_buffer);
    return -1;
}

printf("Successfully packed to: %s\n", desc.out_path);
```

### npm 包打包
```c
// npm 打包示例
kyPackDesc desc = {
    .title = "my-kronyx-game",
    .version = "0.1.0",
    .script = "index.kyx",
    .out_path = "dist/my-game.js"
};

if (ky_pack(&desc, KY_PACK_NPM, error_buffer, sizeof(error_buffer)) != 0) {
    printf("npm pack failed: %s\n", error_buffer);
}

// 生成的结构：
// dist/
// ├── my-game.js      # 主要的 JavaScript 文件
// ├── package.json    # npm 包配置
// └── assets/         # 资源文件
```

### JAR 打包
```c
// JAR 打包示例
kyPackDesc desc = {
    .title = "KronyxGame",
    .version = "1.0.0",
    .script = "game.kyx",
    .out_path = "game.jar"
};

if (ky_pack(&desc, KY_PACK_JAR, error_buffer, sizeof(error_buffer)) != 0) {
    printf("JAR pack failed: %s\n", error_buffer);
}

// 生成的结构：
// game.jar
// - META-INF/
//   - MANIFEST.MF     # JAR 清单文件
// - kronyx/           # 引擎库
// - game.kyx         # 主脚本
// - assets/          # 资源文件
```

## 输出格式详解

### EXE 可执行文件

#### 结构
```
my_game.exe
├── 程序头区 (PE头)
├── 引擎代码 (ky_core + ky_engine + ky_runtime)
├── 脚本数据 (编译后的字节码)
├── 资源文件 (纹理、音频等)
└── 反篡保护 (KyAntiTamper.dll)
```

#### 特性
- **独立运行**：无需额外依赖
- **静态链接**：所有库代码内置
- **跨平台**：Windows、Linux、macOS 支持
- **大小优化**：支持 UPX 压缩

#### 部署
```bash
# 直接运行
./my_game.exe

# 或静默运行
./my_game.exe --no-window
```

### npm 包

#### 结构
```
dist/
├── my-game.js      # 主 JavaScript 文件
├── package.json    # npm 配置
├── README.md       # 项目说明
├── assets/         # 资源文件
└── node_modules/   # 依赖项
```

#### package.json 生成
```json
{
  "name": "my-kronyx-game",
  "version": "0.1.0",
  "description": "A 2D platformer game built with Kronyx Engine",
  "main": "my-game.js",
  "scripts": {
    "start": "node my-game.js",
    "dev": "node my-game.js --dev"
  },
  "keywords": ["game", "kyronyx", "2d"],
  "engines": {
    "node": ">=14.0.0"
  },
  "bin": {
    "my-game": "./my-game.js"
  }
}
```

#### 使用方式
```bash
# 安装为本地包
npm install ./dist

# 或发布到 npm
npm publish dist/

# 运行游戏
npx my-game
```

### JAR 包

#### 结构
```
game.jar
├── META-INF/
│   └── MANIFEST.MF
├── kronyx/
│   ├── kronyx.class
│   └── runtime.jar
├── assets/
│   ├── sprites.png
│   └── level.ksn
└── Game.class       # 主类
```

#### MANIFEST.MF
```
Manifest-Version: 1.0
Main-Class: Game
Title: My Kronyx Game
Version: 1.0.0
```

#### 运行方式
```bash
# Java 运行
java -jar game.jar

# 调试运行
java -jar game.jar --verbose
```

## 构建配置

### CMake 集成

Pack 工具通过 CMake 集成到构建系统中：

```cmake
# 查包工具
add_executable(ky_pack src/pack/pack.c)
target_link_libraries(ky_pack PRIVATE ky_engine ky_core)

# 依赖目标
add_custom_target(game_assets
    COMMAND ky_mk_demo_asset ${CMAKE_SOURCE_DIR}/assets/hero_8x8.bin
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 打包依赖
add_dependencies(ky_pack game_assets)
```

### 环境变量

```bash
# 编译器路径
export CC=clang
export CXX=clang++

# 打包工具路径
export KRONYX_PACK_ROOT=/path/to/kronyx

# 资源路径
export KRONYX_ASSETS_ROOT=assets
```

## 自定义打包

### 资源文件处理

Pack 工具支持自定义资源处理器：

```c
// 资源处理函数
typedef struct kyResource {
    const char *path;
    void *data;
    size_t size;
    const char *type;
} kyResource;

// 自定义资源加载
kyResource* load_custom_resource(const char *path) {
    // 加载逻辑
    return resource;
}
```

### 元数据注入

```c
// 自定义元数据
typedef struct kyPackMetadata {
    const char *author;
    const char *website;
    const char *license;
    const char *dependencies[10];
} kyPackMetadata;

// 注入元数据
kyPackMetadata metadata = {
    .author = "Your Name",
    .website = "https://example.com",
    .license = "MIT",
    .dependencies = { "kyronyx-runtime", "node:*" }
};
```

## 错误处理

### 错误代码

```c
// 错误代码枚举
typedef enum kyPackError {
    KY_PACK_OK = 0,                    // 成功
    KY_PACK_ERROR_INVALID_DESC,        // 无效描述符
    KY_PACK_ERROR_SCRIPT_NOT_FOUND,    // 脚本文件不存在
    KY_PACK_ERROR_ENGINE_LINK_FAILED,   // 引擎链接失败
    KY_PACK_ERROR_RESOURCE_LOAD_FAILED, // 资源加载失败
    KY_PACK_ERROR_TARGET_NOT_SUPPORTED, // 不支持的目标格式
    KY_PACK_ERROR_OUTPUT_WRITE_FAILED,  // 输出文件写入失败
    KY_PACK_ERROR_MEMORY_ALLOC_FAILED   // 内存分配失败
} kyPackError;
```

### 错误处理模式

```c
// 错误处理示例
kyPackDesc desc = {
    .title = "Test Game",
    .script = "missing.kyx",  // 不存在的文件
    .out_path = "test.exe"
};

char error[256];
int result = ky_pack(&desc, KY_PACK_EXE, error, sizeof(error));

if (result != 0) {
    printf("Pack error: %s\n", error);
    
    // 根据错误类型处理
    if (strstr(error, "not found")) {
        printf("请检查脚本文件路径\n");
    } else if (strstr(error, "link failed")) {
        printf("检查编译器和库文件\n");
    }
}
```

## 性能优化

### 打包优化

1. **增量打包**：仅打包变更的文件
2. **资源压缩**：自动压缩纹理和音频资源
3. **代码优化**：移除未使用的函数
4. **符号剥离**：减少可执行文件大小

### 内存优化

1. **流式处理**：大文件流式处理避免内存峰值
2. **资源缓存**：复用已加载的资源
3. **垃圾回收**：及时释放临时内存

## 故障排除

### 常见问题

#### 链接失败
```bash
# 问题：找不到引擎库
error: undefined reference to `ky_engine_create'

# 解决方案：
# 1. 确保静态库存在
# 2. 检查 CMake 链接配置
# 3. 验证编译器路径
```

#### 资源丢失
```bash
# 问题：运行时找不到资源文件
Error: asset 'player.png' not found

# 解决方案：
# 1. 检查资源路径配置
# 2. 确认文件在打包时包含
# 3. 验证相对路径
```

#### 平台兼容性
```bash
# Windows 问题：依赖缺失
# 解决方案：使用静态链接或提供 DLL

# Linux 问题：库路径
# 解决方案：设置 LD_LIBRARY_PATH

# macOS 问题：签名问题
# 解决方案：codesign 签名
```

### 调试模式

```c
// 启用调试输出
kyPackDesc desc = {
    .title = "Debug Game",
    .script = "game.kyx",
    .out_path = "debug.exe"
};

// 设置调试标志
desc.flags = KY_PACK_FLAG_VERBOSE | KY_PACK_FLAG_DEBUG;

// 执行打包
ky_pack(&desc, KY_PACK_EXE, NULL, 0);
```

## 扩展功能

### 插件系统

Pack 工具支持插件扩展：

```c
// 插件接口
typedef struct kyPackPlugin {
    const char *name;
    void* (*init)(void);
    void (*process)(kyPackDesc *desc);
    void (*cleanup)(void* data);
} kyPackPlugin;

// 注册插件
ky_pack_register_plugin("custom-resource", &my_plugin);
```

### 自定义目标

```c
// 注册自定义打包目标
kyPackTarget custom_target = KY_PACK_TARGET_CUSTOM;
ky_pack_register_target("web", "Web Bundle", pack_web_handler);
```

## 未来计划

### 计划功能

- **APK 支持**：Android 平台打包
- **WebAssembly**：Web 平台支持
- **热更新**：运行时脚本更新
- **资源加密**：资源文件加密保护
- **签名验证**：应用签名和验证

---

**相关文档**：
- [ARCHITECTURE.md](../ARCHITECTURE.md) - 系统架构设计
- [Script.md](./Script.md) - 脚本系统
- [Resource.md](./Resource.md) - 资源管理系统
- [Engine.md](./Engine.md) - 运行时引擎