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
