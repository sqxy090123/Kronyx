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
- Date: 2026-09-08
- Context: Agent 完成 G3 kyx 游戏绑定、G5 编辑器核心和 G6 场景序列化，并修复 ASan SEGV 与 ecs 迁移 ctor 双重 bug
- Category: Troubleshooting & Debugging
- Instructions:
  - kyArray 持有 kyAllocator* 指针；make_world() 的栈上 allocator 在函数返回后悬空，ky_world_destroy→ky_array_deinit→ky_mem_free 触发 ASan SEGV；规则：allocator 必须比 world 活得久（static kyAllocator g_alloc 或平台世界结构体成员）
  - ECS move_entity 迁移时不应对已 memcpy 的旧组件重复调用 ctor；ky_test_ecs::test_ctor_not_recalled_on_migrate 验证此不变式
  - 实体句柄跨脚本边界用 int64(version<<32 | id) 打包；脚本侧必须通过 ky_vm_call 实参传入（OP_LOADINT 仅 32 位，OP_RETURN 强转 double），脚本内不可用字面量，return 值也可承载 53 位以内
  - ASan 全量 ctest 以 detect_leaks=0 为通过门限（VM 有预存 strdup 泄漏属已知债，G15 处理）
  - ky_view_begin 内部已调用一次 ky_view_next；不能在 begin 后再调 next 获取第一个结果，否则跳过一个实体
  - ECS despawn 会递增 slot version 但不清除 slot，导致 {id, old_ver+1} 通过 ky_entity_valid 但 archetype_index=KY_ARCH_NONE； hierarchy 遍历必须用 ky_world_alive_count + ky_world_get_alive_entity（检查 archetype_index != KY_ARCH_NONE）而非扫描 id/version 对
  - G5 editor_core 使用 spawned 列表追踪编辑器创建的实体，避免空洞版本导致的计数错误
  - G6 场景序列化 .ksn 格式：属性值用引号包裹时必须在解析时剥离首尾 " 再传给 atof/atoi，否则返回 0
  - G6 ky_scene_load 要求每个 entity 至少含一个 Transform 组件，否则返回 -4
  - ky_hashmap_deinit 内部已释放所有 owned key/value；持有方不得手动再 free（否则 double free）；ky_scene_destroy 依赖 ky_hashmap_deinit 做 entries 清理

[Project Knowledge Summary]
- Date: 2026-09-04
- Context: Agent 完成反篡改系统修复和文档编写
- Category: Operations & Deployment
- Instructions:
  - 反篡改系统使用双组件架构：静态库 ky_antitamper（嵌入exe）+ 共享库 KyAntiTamper.dll/.so/.dylib（验证库）
  - 验证库必须与游戏 exe 放在同一目录，否则 dlopen 失败
  - 运行时必须在 exe 所在目录执行（如 cd build && ./ky_demo），否则 /proc/self/exe 定位失败
  - 开发密钥：KY_ANTITEMPER_DEV_KEY 支持 "release:<hex64>"、"dev:<hex64>" 或纯 64 位 hex 三种格式
  - 测试命令：ctest --test-dir /workspace/build --output-on-failure（12/12 通过）
  - 文档位置：.monkeycode/docs/anti_tamper.md

[Project Knowledge Summary]
- Date: 2026-09-07
- Context: Agent 完成反篡改本地验证修复与测试套件
- Category: Troubleshooting & Debugging
- Instructions:
  - render 测试（ky_test_render）存在间歇性 segfault：EGL 软渲染上下文的偶发问题，单独重跑即可恢复，与业务代码无关
  - ctest 建议用 -j1 串行跑，并行时 render 更易触发
  - exe 侧本地 HMAC 验证必须与验证库算法逐字节一致：round_key = dev_key ^ salt[0..31]（无密钥时 = salt[0..31]），消息 = salt(256B) + round(int 4B)，比对 hmac_out 前 32 字节
  - 开发者密钥解析规则（exe 与 dll 两侧一致）："release:"/"dev:" 前缀剥离后取 64 位 hex，或纯 64 位 hex
  - 反篡改活文件：src/engine/anti_tamper_game.c（exe 侧）、src/engine/anti_tamper_dll.cpp（验证库）、include/kronyx/anti_tamper.h（公共 API）

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

[Project Knowledge Summary]
- Date: 2026-09-07
- Context: Discovered by Agent while completing the G1 2D platformer demo slice
- Category: Troubleshooting & Debugging
- Instructions:
  - kyArray 持有 `kyAllocator*` 指针；ky_world_create(&alloc) 传栈上局部 alloc，函数返回后指针悬空，ky_world_destroy→ky_array_deinit→ky_mem_free 跳坏地址 SEGV（ASan 报 stack-overflow/DEADLYSIGNAL）
  - 规则：跨生命周期调用 ky_world_create 的持有者，必须把 allocator 存进长命结构体（如 PlatformerWorld.alloc）再传指针
  - ky_rd_backend() 返回 kyRendererBackend 枚举值，printf %s 会把它当指针解引用 SEGV；打印名字用 ky_rd_backend_name()
  - ECS 组件 type_id 从 0 起，判「是否找到」要显式 seen 标志，别用 truthiness（0 是合法 id）
  - 角色物理用手动 2D 积分器（重力+落地钳制），不走刚体：内置窄相沿最小穿透轴推开，压不住垂直落到平面（x/z 穿透≈0）
  - 崩溃定位无 gdb 时用 ASan 构建：ASAN_OPTIONS=detect_leaks=0 halt_on_error=1 ./build_asan/<bin> 抓栈；ky_demo 崩溃栈直接指向 main.c 报错行

[Project Knowledge Summary]
- Date: 2026-09-07
- Context: Discovered by Agent while completing the G2 resource-loading slice
- Category: Build Methods
- Instructions:
  - 资源加载最小集（G2）已落地：文件层 ky_file_read/free（含 256 MiB 上限 + KY_FILE_OK/NOT_FOUND/BAD_SIZE/IO/NOMEM），资源层 ky_resmgr_make_pixelbuffer/raw_bytes（重复 path 幂等；on_destroy 走 kyAllocator，追踪分配器下也正确）
  - 2D 便利函数 ky2d_make_texture(rd,w,h,channels,pixels)：参数校验后转 ky_rd_create_texture_2d；channels 1/2/3/4，w/h>0，否则 NULL
  - Demo hero 纹理：assets/hero_8x8.bin（8×8 RGBA8，256 B）；缺失时角色回落白色 sprite，不报错
  - 测试新增：ky_test_resource_flow（28 断言），CMake add_test resource_flow；工具 ky_mk_demo_asset + `ctest -R demo_asset` 生成资产
  - 文档：.monkeycode/docs/resource.md（活契约）
   - 构建命令不变：cmake -B build && cmake --build build -j4；ctest --test-dir build -j1

[Project Knowledge Summary]
- Date: 2026-09-08
- Context: Discovered by Agent while completing G6 scene serialization and G7 sprite animation slices
- Category: Troubleshooting & Debugging
- Instructions:
  - ky_hashmap_deinit 内部已释放所有 owned key/value；持有方不得手动再 free（否则 double free）；ky_scene_destroy 依赖 ky_hashmap_deinit 做 entries 清理
  - ky_view_begin/ky_view_next 迭代模式：必须 `int more = ky_view_begin(...); while(more) { more = ky_view_next(&it); }`；错误写法 `while(ky_view_begin() || ky_view_next())` 每次迭代重置迭代器导致无限循环
  - ky_world_add_component 迁移 entity 到新模式 archetype 后，之前持有的 component 指针可能失效（archetype 数据搬移）；step 后必须用 ky_world_get_component 重新获取
  - ky_world_component_type_by_name 在组件注册前返回 UINT32_MAX；必须先 register 再 lookup
  - ECS system 的 kySystem.order 是 uint32_t；传 -1 会溢出为 4294967295，排在最后
