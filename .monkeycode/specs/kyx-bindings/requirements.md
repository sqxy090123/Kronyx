# G3 · kyx 游戏绑定 · 需求

## 目标

让 kyx 脚本能驱动引擎世界：脚本调用 `std.*` native，完成「建实体 → 读写 Transform → 读输入 → 打日志 → 销毁」的最小闭环。不引入类/metatable、完整 stdlib、GC。

## 角色

- **游戏脚本（kyx）**：调用 `std.*`、传/收实体句柄与数值。
- **引擎绑定层（`KyxBinds`）**：校验参数，转调 ECS / Transform / input。
- **C 宿主**：拥有 `KyxBinds` 与 `kyWorld` 生命周期。

## 需求

### R1 绑定生命周期

- R1.1 当宿主以 `ky_bind_create(vm, world)` 建立绑定，绑定层必须把自身注册为该 `vm` 的 `user` 上下文。
- R1.2 当宿主调用 `ky_bind_destroy(b)`，绑定层不得再被脚本引用；此后调用 native 视为未定义（宿主负责顺序）。
- R1.3 若 `world` 为 `NULL`，每个 native 必须返回 `KYT_STRING` 错误信息，而不是崩溃。

### R2 实体管理

- R2.1 当脚本调用 `std.spawn()`，绑定层必须在世界中新建实体并返回打包句柄（`int64 = (version<<32) | id`）。
- R2.2 当脚本调用 `std.despawn(handle)` 且句柄有效，绑定层必须移除实体的全部组件并释放槽位。
- R2.3 当脚本调用 `std.alive(handle)`，绑定层必须在该实体仍有效时返回非零，已无效时返回 `0`（经 VM `op_return` 强转为 FLOAT）。

### R3 Transform 读写

- R3.1 当脚本调用 `std.setpos(handle, x, y)` 且实体持有 `transform` 组件，绑定层必须写入 `pos.x/pos.y`。
- R3.2 当脚本调用 `std.getpos(handle, axis)`，绑定层必须返回对应分量（`axis=0 → x`，`axis=1 → y`）。
- R3.3 当脚本调用 `std.setscale / std.getscale`，语义同 R3.1/R3.2，作用于 `scale`。
- R3.4 若实体无 `transform` 组件，set/get 必须返回 `KYT_STRING` 错误而非崩溃。

### R4 输入读取

- R4.1 当脚本调用 `std.poll_input()`，绑定层必须调用 `ky_input_*` 并缓存本帧 `axis_x` 与 `last_key`。
- R4.2 `std.axis_x()` 返回缓存的水平轴（左 `-1`、右 `+1`、无输入 `0`）。
- R4.3 `std.last_key()` 返回缓存的最近一次按键枚举值（无则 `0`）。

### R5 日志

- R5.1 当脚本调用 `std.log(...)`，绑定层必须按参数个数打印（字符串/数值均可），不抛错。

### R6 句柄跨边界约定（不变式）

- R6.1 实体句柄为 64 位 int；脚本侧**通过 `ky_vm_call` 实参**传入 native（实参原样拷贝进局部变量）。
- R6.2 脚本侧**不得**把整型句柄写成源码字面量（`OP_LOADINT` 截断 32 位），**不得**依赖脚本 `return` 值承载 64 位句柄（`OP_RETURN` 将 INT 强转 FLOAT）。
- R6.3 C 侧读回时接受 `KYT_FLOAT`（`double` 53 位尾数，在 `version/id` 合理取值内可精确表示 64 位打包值）。

## 验收

- `tests/test_kyx_bindings.c` 30 断言全绿（覆盖 R1.3、R2.1~3、R3.1~4、R4.1~3、R5.1）。
- 全量 `ctest`（ASan, `detect_leaks=0`）15/15 通过。
- `ky_demo` 无头 exit=0。

## 非目标

类/metatable 与表、math 全家桶、GC、动画/音频/粒子/joint。
