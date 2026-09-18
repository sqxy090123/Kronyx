# kyx 分代垃圾回收 · 需求

## 概述

kyx 当前堆对象（`kyClosure`、`kyNativeEntry`、`kyProto` 的 code/constants/strings、全局变量名）均通过 C `malloc`/`calloc` 分配，宿主在 `ky_vm_destroy` 时手工逐一 `free`。脚本层无法创建动态数组、无法实现闭包捕获、对象图循环引用无法回收。本特性引入分代 GC，管理 VM 堆上的所有脚本对象，提供 Node.js 风格的双触发机制（增量式自动 + 宿主可手动触发），使脚本语言具备真正的动态数据结构和闭包能力。

## 术语

| 术语 | 定义 |
|------|------|
| **GC 堆（heap）** | VM 内部分代的内存池，所有脚本层堆对象必须经 GC 堆分配器获取 |
| **新世代（nursery）** | 新生代空间，接收新分配对象，触发频繁（分配阈值） |
| **旧世代（tenured）** | 老年代空间，存留经过至少一次 GC 存活的对象 |
| **根（GC root）** | 可达性分析的起点：寄存器、栈帧局部变量、全局变量、proto 字符串池、native 用户上下文指针 |
| **弱引用（weak ref）** | 不参与可达性分析、不影响对象存活判断的指针，仅由宿主手动登记 |
| **分代阈值** | 新世代已分配字节数 / 触发阈值（默认 256 KB，可配） |
| **晋升阈值** | 对象在新世代被标记存活的次数 / 晋升次数（默认 3 次） |
| **安全点（safe point）** | VM 执行循环中插入的 GC 检查位置，GC 仅在此处执行，避免执行中被中断 |
| **kyVM GC 堆** | 嵌在 `kyVM` 结构体内的分代堆，随 VM 创建销毁 |

## 需求

### R1 · 堆分配器

**用户故事：** 作为 kyx 脚本作者，我希望脚本能创建动态数组和函数闭包，使它们随对象生命周期自动管理，无需手动 free。

#### 验收标准

1. WHEN 宿主调用 `ky_vm_create`，系统 SHALL 在 VM 内部初始化分代堆，堆初始新世代 256 KB、旧世代 4 MB，并支持 `ky_vm_set_heap_size(vm, nursery_bytes, tenured_bytes)` 在 VM 首次运行脚本前调整大小。
2. WHEN 脚本执行到 `OP_NEWHDR`（新指令），系统 SHALL 从新世代分配 N 字节，对齐 16 字节，返回 GC 堆指针给目标寄存器。
3. IF 新世代剩余空间不足，系统 SHALL 在下一条 VM 指令执行前触发新世代 GC（见 R2），分配失败后重试一次。
4. IF 新世代 GC 后仍不足，系统 SHALL 触发旧世代 GC 并继续。
5. IF 旧世代 GC 后仍不足，系统 SHALL 在目标寄存器写入 `nil`，并在 `vm->error_msg` 写入 `"out of heap"`，不崩溃。
6. WHEN 脚本创建 `OP_NEWWSTR`（新字符串常量，见 R3），系统 SHALL 同样走 GC 堆，并支持 `OP_CONCAT` 拼接时分配新串。
7. WHEN 宿主调用 `ky_vm_destroy(vm)`，系统 SHALL 释放整个 GC 堆（含新世代、旧世代、元数据、写屏障日志），不依赖任何脚本层 free。
8. WHILE VM 正在运行且 GC 堆处于正常状态，宿主调用 `ky_vm_gc_collect(vm)` 返回当前堆占用字节数（含新世代 + 旧世代已用），不触发 GC 时返回 0。

### R2 · 分代 GC 触发

**用户故事：** 作为 kyx 引擎维护者，我希望 GC 在脚本执行过程中自动增量执行，停顿可控，且宿主能手动强制收集。

#### 验收标准

1. WHILE 新世代已分配字节数达到新世代阈值，系统 SHALL 在下一个安全点（函数调用 / 循环回跳 / 脚本顶层语句之间）触发新世代 GC。
2. WHEN 新世代 GC 执行，系统 SHALL 仅扫描新世代对象（标记-整理，写屏障保护旧世代指针）并晋升计数 +1，存留超过晋升阈值的对象迁移到旧世代。
3. WHILE 旧世代已分配字节数达到旧世代 75%，系统 SHALL 在下一个安全点触发旧世代 GC（全堆标记-整理）。
4. WHEN 宿主调用 `ky_vm_gc_collect(vm)`，系统 SHALL 立即执行全堆 GC 并返回释放字节数，无需等待安全点。
5. IF 宿主在脚本运行时调用 `ky_vm_gc_collect`，系统 SHALL 在当前指令执行完成后（下一个安全点）执行 GC，而非立即中断 VM 寄存器状态。
6. WHEN 新世代 GC 发生，系统 SHALL 将新世代中所有被存留次数达到晋升阈值的对象迁移到旧世代，并将新世代中剩余存活对象压缩对齐。
7. IF GC 过程中旧世代空间不足，系统 SHALL 把新世代全部存活对象直接保留在新世代（降级：不晋升），并记入 `vm->gc_stats` 的 `nursery_degraded` 计数。

### R3 · 字符串常量堆化

**用户故事：** 作为 kyx 脚本作者，我希望 `OP_CONCAT`（拼接）的结果字符串可被 GC 回收，避免字符串常量池无界增长。

#### 验收标准

1. WHEN 脚本执行 `OP_CONCAT`，系统 SHALL 在 GC 堆（新世代）分配结果字符串对象，不写入 proto 常量池。
2. IF 两个操作数均为 proto 字符串池常量（短生命周期），系统 SHALL 在栈上构造临时结果，仅在结果被赋值给全局 / 作为 `OP_NEWWSTR` 存储时才进堆。
3. WHEN 宿主调用 `ky_vm_gc_collect`，系统 SHALL 回收所有无引用的堆字符串，不影响 proto 字符串池。
4. IF GC 回收了被 native 函数用户上下文（`user` 指针）持有的字符串，系统 SHALL 保证该字符串在 native 函数返回值注册前仍然有效（通过 `ky_vm_gc_mark_native_ref(vm, ptr)` 登记弱引用，GC 时跳过该指针）。

### R4 · 全局变量写屏障

**用户故事：** 作为 kyx 引擎维护者，我希望全局变量赋值时，若目标对象是新世代，源对象是旧世代，系统能正确记录反向指针，避免新世代 GC 漏标。

#### 验收标准

1. WHEN 脚本执行 `OP_SETGLOBAL` 且目标全局槽位存的是新世代对象，源操作数是旧世代对象，系统 SHALL 在写屏障日志记录 `(old→new)` 反向边，新世代 GC 时沿反向边把旧世代指针拉入标记集。
2. WHILE 旧世代 GC 执行，系统 SHALL 扫描所有旧世代对象，标记其指向的全部子对象（不依赖写屏障日志）。
3. IF 全局变量槽位存的是旧世代对象，系统 SHALL 正常扫描该指针，不产生写屏障日志。
4. WHEN 宿主调用 `ky_vm_gc_mark(vm, ptr)` 将某指针加入 GC 根集，系统 SHALL 使该指针所指的整个对象子图在下次 GC 时存留。

### R5 · 闭包堆化

**用户故事：** 作为 kyx 脚本作者，我希望 `function(a) { ... }` 作为值传递和存储时，闭包对象可被 GC 回收，支持 `var f = fun(x){ return x; }; f(1);` 而不泄漏。

#### 验收标准

1. WHEN 脚本执行 `OP_CLOSURE`（新指令：把 proto 编号 B 包装为可传递的闭包值），系统 SHALL 在 GC 堆分配 `kyClosure` 对象（`proto` 指针 + 捕获的上层局部变量区），并返回该指针给目标寄存器。
2. IF 闭包内部引用了外层局部变量（`OP_CLOSURE` 时 `upval_count > 0`），系统 SHALL 在 GC 堆分配捕获区，每个捕获槽保存局部变量的当前值（按值捕获），闭包对象指针指向该捕获区。
3. WHEN 闭包值从栈上弹出且不再被引用，系统 SHALL 在下一次 GC 时回收 `kyClosure` 及捕获区。
4. WHILE `ky_vm_call` 执行原生函数（`OP_NATIVECALL`），系统 SHALL 把 native 返回值（如 `KYT_FUNCTION` 类型）的 GC 指针作为 GC 根处理，防止 GC 在 native 函数运行期间回收它。

### R6 · GC 统计与观测

**用户故事：** 作为性能调优者，我希望可以读取 GC 运行统计，判断堆压力是否正常。

#### 验收标准

1. WHEN 宿主调用 `ky_vm_gc_stats(vm, &stats)`，系统 SHALL 填充 `kyGcStats` 结构体，包含：
   - `nursery_used_bytes` / `nursery_capacity_bytes`
   - `tenured_used_bytes` / `tenured_capacity_bytes`
   - `total_gc_count`（新生代 GC 次数）
   - `total_t_gen_gc_count`（老年代 GC 次数）
   - `total_freed_bytes`（累计回收字节）
   - `nursery_degraded_count`（降级次数）
   - `last_gc_time_ms`（最近一次 GC 耗时）
2. WHEN 宿主调用 `ky_vm_gc_set_threshold(vm, nursery_trigger_bytes)`，系统 SHALL 在下次 GC 判断时采用新阈值，返回值 0。
3. IF 阈值设为 0，系统 SHALL 解释为"禁用自动 GC"（宿主只能手动触发），并在 `stats` 中反映 `auto_disabled = 1`。

### R7 · 与现有 API 的兼容

**用户故事：** 作为现有宿主，我希望不修改任何现有 `ky_vm_*` 调用代码，就能获得 GC 行为。

#### 验收标准

1. WHEN 宿主调用 `ky_vm_destroy(vm)`，系统 SHALL 自动释放 GC 堆，不产生新调用。
2. WHEN 宿主调用 `ky_vm_register_native(vm, ns, name, fn, user)`，系统 SHALL 把 `user` 指针登记为 GC 根（weak 标记），防止 GC 在 native 函数运行期间回收 `user` 指向的对象。
3. WHEN 现有 `OP_GETFIELD` / `OP_SETINDEX` 遇到 `KYT_ARRAY` 类型的 native 对象，系统 SHALL 行为不变（`KYT_ARRAY` 在 R3 前仍是 C 宿主管理）。
4. WHEN 脚本中 `std.log(...)` 调用时，系统 SHALL 把 `kyNativeEntry` 的 `ns`/`name` 字符串作为 proto 字符串池对象处理，不触发 GC 分配。
5. IF 宿主从未调用任何 GC API，脚本正常执行时，系统 SHALL 自动按 R2 触发条件执行 GC，不需要宿主主动调用。

### R8 · 安全保证

**用户故事：** 作为代码审计者，我希望 GC 实现不引入内存损坏、UB 或死锁。

#### 验收标准

1. WHILE GC 正在执行，系统 SHALL 不释放任何仍被寄存器、栈帧、全局变量或 native 用户上下文引用的对象。
2. IF GC 堆分配失败（系统 `malloc` 返回 NULL），系统 SHALL 在目标寄存器写入 `nil`，设置 `vm->error_msg = "gc alloc failed"`，不崩溃，VM 继续运行。
3. WHEN 执行到安全点，系统 SHALL 不跨越 native 函数调用的执行期做 GC（native 函数运行期间，其 `user` 指针和参数均视为 GC 根）。
4. IF 两次连续 GC 均未释放任何字节（`freed == 0`），系统 SHALL 跳过下次新世代 GC 直到新世代再次达到阈值（避免密集无效 GC），但旧世代 GC 不受此限制。

## 非目标（本迭代）

- 完整 `table`/`dict` 类型
- 对象继承（class 运行时语义）
- 跨 VM 共享堆（多个 `kyVM` 独立）
- 并发（非写屏障）GC
- V8 风格增量标记（目前是 Stop-the-World，分代只是分代触发频率，非增量）

## 验收标准（测试）

- `tests/test_kyx_gc.c`：20 个断言，覆盖 R1~R5 全部验收场景，全绿
- 全量 `ctest`（ASan + UBSan，`detect_leaks=0`）：19 项既有测试 + 1 项新 GC 测试 = 20/20
- `ky_demo` 无头退出码 0，堆占用不超限（旧世代 peak < 8 MB）
