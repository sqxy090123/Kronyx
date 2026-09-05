# User Instruction Memory

This file records user instructions, preferences, and teachings for reference in future interactions.

## Format

### User Instruction Entry
User instruction entries should follow this format:

[User Instruction Summary]
- Date: [YYYY-MM-DD]
- Context: [Mentioned scenario or time]
- Instructions:
  - [Content of user teaching or instruction, described line by line]

### Project Knowledge Entry
Entries discovered by the Agent during task execution should follow this format:

[Project Knowledge Summary]
- Date: [YYYY-MM-DD]
- Context: Discovered by Agent while performing [specific task description]
- Category: [Operations & Deployment|Build Methods|Testing Methods|Troubleshooting & Debugging|Workflow & Collaboration|Environment Configuration]
- Instructions:
  - [Specific knowledge points, described line by line]

## Deduplication Strategy
- Before adding a new entry, check for similar or identical instructions.
- If a duplicate is found, skip the new entry or merge it with the existing one.
- When merging, update the context or date information.
- This helps avoid redundant entries and keeps the memory file tidy.

## Entries

[Project Knowledge Summary]
- Date: 2026-08-29
- Context: Discovered by Agent while checking code for third-party license compliance
- Category: Operations & Deployment
- Instructions:
  - Kronyx 源代码为原创实现，未直接复制其他开源项目代码
  - 使用的 kyProto/kyClosure 等命名是字节码 VM 的通用设计概念
  - opcode 定义采用标准字节码 VM 模式，与 Lua 等有相似之处但为独立实现
  - 项目采用 MIT License

[Project Knowledge Summary]
- Date: 2026-08-29
- Context: Discovered by Agent while working on P4 script compiler
- Category: Build Methods
- Instructions:
  - 构建命令: cmake -B build && cmake --build build -j2
  - 测试命令: /workspace/build/ky_test_script
  - P4 脚本编译器当前状态: 93/106 测试通过 (88%)
  - 主要问题: 函数调用返回值机制有 bug

[Project Knowledge Summary]
- Date: 2026-08-29
- Context: Debugging P4 script compiler VM execution issue
- Category: Troubleshooting & Debugging
- Instructions:
  - VM call/return mechanism has bugs: function calls return incorrect values (-1.0 instead of expected results)
  - Bytecode generation appears correct (verified with debug output)
  - Issue seems to be in VM execution loop, possibly related to stack management or memory corruption
  - Tests hang when running full test suite, may be due to infinite loop in VM
  - Need to investigate OP_ADD and other arithmetic operations

[Project Knowledge Summary]
- Date: 2026-08-29
- Context: Added force field system to physics engine
- Category: Build Methods
- Instructions:
  - 引力场公式: F = G * m1 * m2 / (r^2 + epsilon^2)，方向指向场源
  - 斥力场公式: F = -G * m1 * m2 / (r^2 + epsilon^2)，方向远离场源
  - 涡流场: 切向力，用于旋转效果
  - 最大支持 64 个并发力场
  - 力场有半径限制，超过半径无力作用
  - 平方反比衰减 + 软化处理防止奇点

[Project Knowledge Summary]
- Date: 2026-08-29
- Context: Completed force field system implementation
- Category: Build Methods
- Instructions:
  - Force field test count: 30 tests, all passing
  - Formula: F = G * m1 * m2 / (r^2 + 0.01^2)
  - Softening factor ε = 0.01 prevents singularity
  - Max 64 concurrent force fields per physics world
  - Force field API: add, remove, get, set, count

[Project Knowledge Summary]
- Date: 2026-09-04
- Context: Discovered by Agent while checking code and fixing bugs across physics, script, and VM modules
- Category: Troubleshooting & Debugging
- Instructions:
  - physics.h 头文件中 force field API 声明在 #endif 之后，会导致编译错误；已修复为在 #endif 之前
  - script.c lexer 中 & | ^ 三个运算符都映射到 KYX_TK_BNOT，需分别映射到 KYX_TK_BAND/KYX_TK_BOR/KYX_TK_BXOR
  - vm.c ky_vm_register_native 之前不存储 ns/name，现已修复，支持按名称查找原生函数
  - vm.c OP_GETGLOBAL 现在同时查找 kyx 函数和原生函数（KYT_NATIVE）
  - vm.c OP_CALL 现在支持调用 KYT_NATIVE 类型值
  - vm.c 新增 OP_BAND(22)/OP_BOR(23)/OP_BXOR(24)/OP_BSHL(25)/OP_BSHR(26) 位运算指令
  - vm.c compile_expression 新增 KY_AST_EXPR_FIELD 编译支持和位运算编译
  - physics.c ky_physics_step 中 force field 应用在 position integration 之后（错误顺序），已修正为前置
  - physics.c ky_physics_cast_ray 从空 stub 实现为 sphere+AABB 射线检测
  - physics.c ky_physics_step 新增 SAP pair 后的窄相位碰撞解决（AABB 重叠推开）
  - 构建命令: cmake -B build && cmake --build build -j2
  - 测试命令: ctest --test-dir build --output-on-failure
  - 当前状态: 全部 6 个测试套件通过 (core, math, ecs, render, physics, script)
  - 安全审计完成(2026-09-04): 修复了1个UAF(lexer destroy前读取)、1个越界读取(loop条件+1)、5处缺少边界检查、1段死代码
  - vm.c中proto->strings数组访问必须检查 B/C < proto->str_count
  - call_proto的while循环条件应为 pc+4 <= proto->code_count
  - 安全审计+警告清理完成(2026-09-04): 修复UAF/OOB死代码/缺失边界检查; 清除全部编译器警告
  - parser.c find_binop开关需覆盖所有二元运算符token(KYX_TK_BAND/BOR/BXOR/MODEQ)
