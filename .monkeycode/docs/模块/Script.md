# Script 脚本系统

## 概述

Script 层是 Kronyx Engine 的脚本子系统，包含自定义脚本语言 `kyx` 的完整实现。提供词法分析、语法分析、编译、虚拟机、垃圾回收和游戏绑定功能，支持高性能的游戏逻辑编写。

## 架构设计

### 语言特性

#### 语言定位
`kyx` 是一种面向游戏开发的轻量级脚本语言，具有以下特点：
- **语法简单**：类似 Lua，易于学习
- **动态类型**：支持多种数据类型和灵活的类型转换
- **高性能**：寄存器虚拟机设计，支持 JIT 编译预留接口
- **游戏友好**：内置游戏开发所需的 native 函数

#### 核心数据类型
```c
typedef enum kyValType {
    KYT_NIL = 0,      // 空值
    KYT_BOOL,         // 布尔值
    KYT_INT,          // 整数
    KYT_FLOAT,        // 浮点数
    KYT_STRING,       // 字符串
    KYT_FUNCTION,     // 函数
    KYT_ARRAY,        // 数组
    KYT_NATIVE,       // 原生函数
} kyValType;
```

### 虚拟机架构

#### 寄存器 VM
基于寄存器的虚拟机设计，每个函数调用使用固定数量的寄存器：
```c
#define KYX_REG_COUNT    32
#define KYX_STACK_SIZE  256
```

#### 值栈管理
虚拟机使用值栈进行数据存储和函数调用：
```c
typedef struct kyVM kyVM;

// 函数调用
KY_API int ky_vm_call(kyVM *vm, const char *func_name, kyValue *args, int argc, kyValue *ret);

// 原生函数注册
typedef kyValue (*kyNativeFn)(struct kyVM *vm, kyValue *args, int argc, void *user);
KY_API void ky_vm_register_native(kyVM *vm, const char *ns, const char *name, kyNativeFn fn, void *user);
```

### 垃圾回收

#### 分代 GC
使用分代垃圾回收算法，提高性能：
- **年轻代**：频繁回收的小对象
- **年老代**：存活较久的对象
- **根集合**：全局引用的对象

#### GC 配置
```c
// GC 根配置
#define KY_MAX_GC_ROOTS  256

// GC 回调
typedef void (*kyGCCallback)(void *obj, void *user_data);
```

### 编译器架构

#### 词法分析器
处理源代码到词法单元的转换：
```c
typedef struct kyToken {
    kyTokenKind kind;     // 词法单元类型
    const char *start;    // 源代码起始位置
    size_t      len;      // 长度
    int         line;     // 行号
    int         col;      // 列号
    union {
        int64_t  ival;    // 整数值
        double   fval;    // 浮点数值
        const char *sval; // 字符串值
    } as;
} kyToken;

// 词法分析器API
typedef struct kyLexer kyLexer;
KY_API kyLexer *kyx_lexer_create(const char *src, const char *name);
KY_API void kyx_lexer_destroy(kyLexer *lx);
KY_API kyToken kyx_lexer_next(kyLexer *lx);
```

#### 语法分析器
构建抽象语法树：
```c
typedef struct kyAstNode kyAstNode;
typedef struct kyParser kyParser;

// 语法分析器API
KY_API kyParser *kyx_parser_create(void *stream);
KY_API kyAstNode *kyx_parser_parse(Parser *p);
```

#### 代码生成
将 AST 转换为字节码：
```c
typedef struct kyProto kyProto;

// 编译函数
KY_API kyProto *kyx_compile(kyVM *vm, kyAstNode *root, char *err_buf, int err_buf_size);
```

## 语言特性

### 词法单元
```c
typedef enum kyTokenKind {
    KYX_TK_EOF = 0,
    KYX_TK_IDENT,        // 标识符
    KYX_TK_INT_LIT,      // 整数
    KYX_TK_FLOAT_LIT,    // 浮点数
    KYX_TK_STRING_LIT,   // 字符串
    KYX_TK_BOOL_LIT,     // 布尔字面量
    KYX_TK_NIL_LIT,      // nil 字面量
    KYX_TK_PLUS, KYX_TK_MINUS,    // 运算符
    KYX_TK_STAR, KYX_TK_SLASH,    // 算术运算
    KYX_TK_MOD,          // 取模
    KYX_TK_EQ, KYX_TK_NEQ,        // 比较运算
    KYX_TK_LT, KYX_TK_LE,         // 小于/小于等于
    KYX_TK_GT, KYX_TK_GE,         // 大于/大于等于
    KYX_TK_AND, KYX_TK_OR,        // 逻辑运算
    KYX_TK_NOT,          // 逻辑非
    KYX_TK_BNOT, KYX_TK_BAND,     // 位运算
    KYX_TK_BOR, KYX_TK_BXOR,
    KYX_TK_SHL, KYX_TK_SHR,
    KYX_TK_ASSIGN,      // 赋值
    KYX_TK_PLUSEQ,      // += 复合赋值
    KYX_TK_MINUSEQ,
    KYX_TK_STAREQ,
    KYX_TK_DIVEQ,
    KYX_TK_MODEQ,
    KYX_TK_INC, KYX_TK_DEC,       // 自增自减
    KYX_TK_LPAREN, KYX_TK_RPAREN,  // 括号
    KYX_TK_LBRACE, KYX_TK_RBRACE,  // 花括号
    KYX_TK_LBRACK, KYX_TK_RBRACK,  // 方括号
    KYX_TK_DOT, KYX_TK_COMMA,      // 点和逗号
    KYX_TK_SEMI, KYX_TK_COLON,     // 分号和冒号
    KYX_TK_QUESTION,               // 问号
    KYX_TK_FUNCTION, KYX_TK_FUN,   // 函数定义
    KYX_TK_CLASS,                  // 类定义
    KYX_TK_VAR, KYX_TK_LET, KYX_TK_CONST, // 变量声明
    KYX_TK_IF, KYX_TK_ELSE,        // 条件语句
    KYX_TK_WHILE, KYX_TK_FOR,      // 循环语句
    KYX_TK_BREAK, KYX_TK_CONTINUE, // 流程控制
    KYX_TK_RETURN,                // 返回语句
    KYX_TK_NEW,                   // 对象创建
    KYX_TK_USE, KYX_TK_NAMESPACE, // 命名空间
    KYX_TK_IMPORT, KYX_TK_EXPORT,  // 模块导入导出
    KYX_TK_SELF, KYX_TK_SUPER,     // 自引用和超类
    KYX_TK_TRUE, KYX_TK_FALSE,     // 布尔值
    KYX_TK_ERROR,                 // 错误
} kyTokenKind;
```

### 语法特性

#### 变量和作用域
```c
// 变量声明
var name = "value";
let constant = 42;
const pi = 3.14159;

// 变量赋值
x = 10;
x += 5;
x.y = "property";
```

#### 函数定义
```c
// 函数声明
function add(a, b) {
    return a + b;
}

// 箭头函数
add = (a, b) => a + b;

// 类定义
class Player {
    var x, y;
    
    function update(dt) {
        x += dt * speed;
    }
}
```

#### 控制流
```c
// 条件语句
if condition > 0 {
    // true 分支
} else {
    // false 分支
}

// 循环
for i = 0, i < 10, i++ {
    // 循环体
}

while condition {
    // 循环体
}

// 流程控制
break;
continue;
return value;
```

## API 参考

### 虚拟机管理

#### 创建和销毁
```c
// 创建虚拟机
KY_API kyVM *ky_vm_create(const void *info);

// 销毁虚拟机
KY_API void ky_vm_destroy(kyVM *vm);
```

#### 脚本加载
```c
// 从字符串加载脚本
KY_API int ky_vm_load_string(kyVM *vm, const char *src, const char *name);

// 从文件加载脚本
KY_API int ky_vm_load_file(kyVM *vm, const char *path);

// 设置导入根目录
KY_API void ky_vm_set_import_root(kyVM *vm, const char *dir);
```

#### 错误处理
```c
// 获取最后错误信息
KY_API const char *ky_vm_last_error(kyVM *vm);
```

### 函数调用

#### 原生函数注册
```c
// 注册原生函数
typedef kyValue (*kyNativeFn)(struct kyVM *vm, kyValue *args, int argc, void *user);

KY_API void ky_vm_register_native(kyVM *vm, const char *ns, const char *name, 
                                 kyNativeFn fn, void *user);
```

#### 脚本函数调用
```c
// 调用脚本函数
typedef struct kyValue {
    kyValType type;
    union {
        int64_t     ival;
        double      fval;
        const char *sval;
        void       *closure;
        void       *arr;
        void       *native;
    } as;
} kyValue;

KY_API int ky_vm_call(kyVM *vm, const char *func_name, kyValue *args, int argc, kyValue *ret);
```

### 编译器工具

#### 词法分析
```c
// 创建词法分析器
KY_API kyLexer *kyx_lexer_create(const char *src, const char *name);

// 销毁词法分析器
KY_API void kyx_lexer_destroy(kyLexer *lx);

// 获取下一个词法单元
KY_API kyToken kyx_lexer_next(kyLexer *lx);
KY_API kyToken kyx_lexer_peek(kyLexer *lx);
```

#### 语法分析
```c
// 创建语法分析器
KY_API kyParser *kyx_parser_create(void *stream);

// 销毁语法分析器
KY_API void kyx_parser_destroy(kyParser *p);

// 解析AST
KY_API kyAstNode *kyx_parser_parse(kyParser *p);
```

#### 代码编译
```c
// 编译AST到字节码
KY_API kyProto *kyx_compile(kyVM *vm, kyAstNode *root, char *err_buf, int err_buf_size);
```

## 游戏绑定

### std 命名空间
引擎预定义了 `std` 命名空间，包含游戏开发常用的函数：

#### 实体操作
```c
// 创建实体
function std.spawn(entity_type)

// 设置位置
function std.setpos(entity_id, x, y)

// 获取位置
function std.getpos(entity_id)

// 轮询输入
function std.poll_input()

// 输入常量
std.INPUT_LEFT, std.INPUT_RIGHT, std.INPUT_UP, std.INPUT_DOWN
```

#### 渲染函数
```c
// 加载纹理
function std.load_texture(path)

// 绘制精灵
function std.draw_sprite(x, y, texture, width, height)

// 设置相机
function std.set_camera(x, y, zoom)
```

#### 物理函数
```c
// 施加力
function std.apply_force(entity_id, fx, fy)

// 获取速度
function std.get_velocity(entity_id)

// 设置速度
function std.set_velocity(entity_id, vx, vy)
```

### 绑定示例
```c
// 原生函数绑定示例
static kyValue spawn_entity(kyVM *vm, kyValue *args, int argc, void *user) {
    const char *entity_type = args[0].as.sval;
    ky_entity_t entity = ky_entity_from_type(world, entity_type);
    
    // 创建 kyValue 返回
    kyValue result;
    result.type = KYT_NATIVE;
    result.as.native = (void*)entity;
    return result;
}

// 注册到 VM
ky_vm_register_native(vm, "std", "spawn", spawn_entity, world);
```

## 配置选项

### 语言配置
```c
// 最大值限制
#define KYX_MAX_TOKENS   8192     // 最大词法单元数
#define KYX_MAX_NESTING  64       // 最大嵌套深度
#define KYX_MAX_UPVALS   32       // 最大上值数量
#define KYX_MAX_FIELDS   64       // 最大字段数量
#define KYX_MAX_PARAMS   32       // 最大参数数量
#define KYX_MAX_LOCALS   64       // 最大局部变量数
#define KYX_MAX_VARS     256      // 最大变量数
#define KYX_MAX_STRINGS  4096     // 最大字符串数
#define KYX_MAX_PROTOS   256      // 最大原型数
#define KYX_MAX_REGISTRY 512      // 最大注册表大小
#define KYX_MAX_CALLS    256      // 最大调用深度
```

### VM 配置
```c
// 寄存器和栈大小
#define KYX_REG_COUNT     32      // 寄存器数量
#define KYX_STACK_SIZE    256     // 栈大小
```

## 性能优化

### 垃圾回收优化
- **分代收集**：年轻代频繁回收，年老代减少回收频率
- **增量标记**：支持增量标记减少卡顿
- **内存池**：预分配内存减少碎片

### 字节码优化
- **寄存器分配**：静态寄存器分配提高访问效率
- **指令优化**：常用指令特殊处理
- **字符串常量池**：减少字符串内存占用

### 运行时优化
- **内联缓存**：方法调用内联缓存
- **类型特化**：热点代码类型特化
- **即时编译**：预留 JIT 编译接口

## 使用示例

### 基本使用
```c
// 创建虚拟机
kyVM *vm = ky_vm_create(NULL);

// 加载脚本
ky_vm_load_string(vm, 
    "function add(a, b) { return a + b; }"
    "function player_update(dt) { "
    "    pos.x += dt * speed; "
    "    if pos.x > 800 then "
    "        pos.x = 0; "
    "    end "
    "end"
);

// 调用函数
kyValue args[2] = {
    {KYT_INT, .as.ival = 10},
    {KYT_INT, .as.ival = 20}
};
kyValue result;
ky_vm_call(vm, "add", args, 2, &result);
printf("Result: %lld\n", result.as.ival);
```

### 游戏脚本
```c
// 游戏脚本示例
function player_spawn(x, y) {
    var player = std.spawn("player");
    std.setpos(player, x, y);
    return player;
}

function player_update(dt, player_id) {
    // 输入处理
    if std.poll_input() == std.INPUT_LEFT then
        std.apply_force(player_id, -100, 0)
    elseif std.poll_input() == std.INPUT_RIGHT then
        std.apply_force(player_id, 100, 0)
    end
}

// 注册原生函数
ky_vm_register_native(vm, "std", "spawn", spawn_native, world);
ky_vm_register_native(vm, "std", "setpos", setpos_native, world);
```

### 错误处理
```c
// 错误处理示例
if (ky_vm_load_file(vm, "game.kyx") != 0) {
    const char *error = ky_vm_last_error(vm);
    printf("Script error: %s\n", error);
    // 处理错误
    return;
}
```

## 调试和开发

### 调试工具
```c
// 获取虚拟机状态
typedef struct kyVMStats {
    int call_stack_depth;
    int memory_usage;
    int gc_count;
    float execution_time_ms;
} kyVMStats;

// 获取统计信息
KY_API void ky_vm_get_stats(kyVM *vm, kyVMStats *stats);
```

### 内存管理
```c
// 手动触发垃圾回收
KY_API void ky_vm_collect_garbage(kyVM *vm);

// 内存统计
KY_API size_t ky_vm_get_memory_usage(kyVM *vm);
```

## 已知限制

### 语言限制
- **字符串处理**：NUL 终止串，不支持二进制安全
- **递归调用**：栈窗口共享，`stack_top` 恒为 0
- **性能限制**：无 JIT 编译，纯解释执行

### 计划功能
- **字节码缓存**：预编译字节码缓存
- **JIT 编译**：即时编译优化
- **更完整的标准库**：扩展字符串、数组、数学函数

---

**相关文档**：
- [ARCHITECTURE.md](../ARCHITECTURE.md) - 系统架构设计
- [GC.md](../专有概念/GC.md) - 垃圾回收系统
- [ECS.md](../专有概念/ECS.md) - 实体组件系统
- [Bindings.md](./Bindings.md) - 脚本绑定系统