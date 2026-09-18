# 需求实施计划

- [x] 1. 创建 GC 堆数据结构与分配器（R1）
    - 新建 src/script/gc.h，定义 kyGcObject、KyGcHeap、kyGcObjType
    - 实现 ky_gc_heap_init / ky_gc_heap_free（src/script/gc.c）
    - 实现 ky_gc_heap_alloc(vm, byte_len) — bump-pointer，16 字节对齐，nursery 优先
    - 实现 ky_gc_heap_alloc_nursery / ky_gc_heap_alloc_tenured 内部函数
    - 实现分配失败路径（GC 重试一次，仍失败写 nil + 设 error_msg）
    - 修改 include/kronyx/script.h：在 kyVM struct 中嵌入 KyGcHeap gc 字段
    - 修改 src/script/vm.c：ky_vm_create 初始化 GC 堆，ky_vm_destroy 释放 GC 堆
    - 实现 ky_vm_set_heap_size（R1.1，VM 首次运行前调整 nursery/tenured 容量）

- [x] 2. 实现分代 GC 核心（R2）
    - 实现 ky_gc_mark_object(vm, ptr) — 遍历 kyGcObject.data 区，标记子对象
    - 实现 ky_gc_run_nursery(vm) — 标记-整理：扫描 nursery，标记存留对象，age+1
    - 实现 ky_gc_promote(vm, obj) — age >= 3 时迁移到 tenured，nursery 压缩
    - 实现 ky_gc_run_full(vm) — 全堆标记-整理（nursery + tenured），旧世代 75% 触发
    - 实现降级路径：tenured 满时不晋升，nursery_degraded_count++（R2.7）
    - 修改 vm.c 执行循环：每条 while(pc) 迭代顶部插入安全点，检查 nursery 阈值
    - 实现 vm->gc_collect_requested 标志（R2.4/R2.5），宿主调用时置位，下一安全点执行
    - 实现 ky_vm_gc_collect(vm) — 置 gc_collect_requested，在安全点执行全堆 GC，返回释放字节数

- [x] 3. 实现字符串堆化（R3）
    - 扩展 kyValue 语义：as.sval 可指向 GC 堆字符串（通过指针范围判断）
    - 实现 ky_gc_is_gc_str(vm, ptr) 辅助函数（判定是否指向 gc.nursery/tenured 内）
    - 实现 OP_NEWWSTR（op 81）— 分配 GC 堆字符串，写入目标寄存器
    - 实现 OP_CONCAT（op 85）— 在 GC 堆分配拼接结果字符串
    - 实现 OP_LOADSTRING 兼容：proto 字符串池（非 GC）仍直接写入，不参与 GC
    - 实现 ky_vm_gc_mark_native_ref(vm, ptr)（R3.4，native user ctx 保护）
    - 修改 vm.c 的 ky_vm_destroy：释放 GC 堆中所有字符串
    - OP_ADD 运行时检测：两个操作数都是字符串时走 GC 堆 concat 路径

- [x] 4. 实现闭包堆化（R5）
    - 扩展 kyClosure：增加 upvals 指针和 upval_count 字段
    - 实现 OP_CLOSURE（op 84）— 在 GC 堆分配闭包 + 捕获区，upval_count 个 kyValue
    - 捕获区按值捕获：从外层栈帧拷贝局部变量到捕获区
    - 修改 OP_CALL：若 fn 是堆闭包（GC 指针），直接调用 proto，不再拷贝
    - 实现 ky_vm_gc_mark(vm, ptr) / ky_vm_gc_unmark(vm, ptr)（R4.4，宿主登记 GC 根）

- [x] 5. 实现 GC 统计与配置 API（R6）
    - 在 gc.h 定义 KyGcStats 结构体
    - 实现 ky_vm_gc_stats(vm, &stats) — 填充全部字段（R6.1）
    - 实现 ky_vm_gc_set_auto_threshold(vm, trigger_bytes)（R6.2/R6.3，0=禁用自动）
    - 修改 ky_vm_gc_collect：返回释放字节数（R1.8）

- [x] 6. 实现写屏障（R4）
    - 扩展 KyGcHeap：增加 barrier_log（uint8_t* 数组）和 barrier_count/cap
    - 实现 OP_SETGLOBAL 写屏障：目标槽存旧世代对象，源是新世代时记录反向边
    - 实现 ky_gc_handle_barrier(vm) — 新世代 GC 前处理 barrier_log，标记被引用的旧世代对象
    - 实现 ky_gc_barrier_append(vm, ptr) — barrier log 自动扩展（容量翻倍）
    - 实现 ky_gc_mark_global(vm, slot_idx) — 旧世代 GC 扫描全局变量表

- [x] 7. 实现动态数组（R1 扩展）
    - 实现 OP_HEAPARR（op 83）— 在 GC 堆分配 kyValue 数组（elem_count 个元素）
    - 实现 OP_NEWARRAY 兼容：若数组来自 GC 堆，参与 GC；若来自 C 宿主，不参与
    - 修改 ky_vm_destroy：释放 GC 堆中所有数组及其元素

- [x] 8. 更新 CMakeLists.txt
    - 添加 src/script/gc.c 到 script 库构建
    - 添加 tests/test_kyx_gc.c 到测试目标（KYR_BUILD_TESTS=ON）

- [x] 9. 编写 GC 单元测试（R1~R8 全部验收项）
    - 新建 tests/test_kyx_gc.c，40 断言
    - R1.1：ky_vm_create 后 GC 堆已初始化，nursery 256KB / tenured 4MB
    - R1.2/R1.3：字符串 concat 分配成功，堆满触发 nursery GC 后重试
    - R1.5：GC 后仍不足 → 写 nil + error_msg="out of heap"
    - R1.7：ky_vm_destroy 无 ASan 泄漏
    - R2.1：nursery 达到阈值自动触发 GC
    - R2.3：tenured 75% 触发全堆 GC
    - R2.4/R2.5：ky_vm_gc_collect 在安全点执行，返回释放字节数
    - R2.7：nursery_degraded_count 在 tenured 满时递增
    - R3.1/R3.3：OP_CONCAT 结果进 GC 堆，GC 后回收
    - R4.1/R4.4：写屏障日志正确，ky_vm_gc_mark 保护宿主指针
    - R5.3：闭包捕获区在不可达时被 GC 回收
    - R6.1/R6.2：KyGcStats 字段正确，ky_vm_gc_set_auto_threshold(0) 禁用自动
    - R7.1/R7.2：现有 API 行为不变，native user ctx 不被 GC 回收
    - R8.2：GC 分配失败（模拟 malloc 返回 NULL）→ 写 nil，VM 继续运行
    - R8.4：两次 GC 均 freed==0 → 跳过下次 nursery GC（设置抑制标志）

- [x] 10. 全量 ctest 验证 + CI 推送
    - 本地 ctest 全绿（20/20）
    - 待 push：git add + commit + push，监控 GitHub Actions 三平台 6/6 通过
