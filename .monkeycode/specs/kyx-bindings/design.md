# G3 · kyx 游戏绑定 · 设计

## 概览

`KyxBinds` 是绑定层的唯一上下文，作为 `vm->user` 传给所有 native。native 签名固定为
`void (void *user, kyValue *arg_values, uint32_t arg_count, kyValue *result)`。
所有 native 通过 `ky_vm_register_native(vm, ns, name, fn)` 注册到同一命名空间 `std`。

```
C 宿主 ──▶ ky_bind_create(vm, world) ──▶ KyxBinds{vm, world, input}
脚本 ────▶ std.spawn / std.setpos / std.getpos / std.setscale / std.getscale
          std.poll_input / std.axis_x / std.last_key / std.alive / std.despawn / std.log
          └──▶ kyVM::native_dispatch ──▶ KyxBinds 的 native 函数 ──▶ ECS / Transform / input
```

## 文件

| 文件 | 职责 |
|------|------|
| `include/kronyx/bindings.h` | `KyxBinds` 不透明类型 + `ky_bind_create/destroy` |
| `src/script/bindings.c` | 11 个 native + 句柄打包/解包 + input 缓存 |
| `tests/test_kyx_bindings.c` | 30 断言端到端逻辑测 |
| `CMakeLists.txt` | 把 `bindings.c` 挂进 `kronyx_bindings`；注册 `ky_test_kyx_bindings` |

## 关键约定

### 实体句柄（int64 打包）
- 打包：`int64 = ((int64)version << 32) | id`；解包逆运算。
- **进入 native**：脚本把句柄作为 `ky_vm_call` 的实参（`kv_int(packed)`）。`OP_CALL` → 实参数组原样拷贝进局部变量槽，`kyValue.as.ival` 保留 64 位。
- **离开脚本**：经 `OP_RETURN` 的 INT→FLOAT 强转后，`double` 尾数 53 位可精确表示 `version/id` 在正常量级下的 64 位打包值；C 侧 `(long long)` 收回。
- **禁止**：脚本内 `2^32` 量级字面量（`OP_LOADINT` 截断）；脚本 return 值承载大句柄。

### Transform 访问
- 通过 `ky_world_get_component(world, entity, ty_transform)` 取组件；`ty_transform` 在 `ky_bind_create` 时用 `ky_world_component_type_by_name` 缓存进 `KyxBinds`（缺失则 native 报 NULL-world 风格错误串）。
- `setpos`：`comp->pos.x = f(arg0); comp->pos.y = f(arg1);`
- `getpos`：`result = kv_f(axis==0 ? pos.x : pos.y)`。

### 输入缓存
- `std.poll_input()` 调 `ky_input_poll` 事件循环，聚合出 `axis_x`（左 `-1`/右 `+1`/无 `0`）与 `last_key`（最近非 0 按键），存 `KyxBinds->input`。
- `std.axis_x()/std.last_key()` 直接读缓存（不重复 poll，保证帧内一致）。

### 错误传播
- 参数不足 / 无世界 / 无组件 → `*result = kv_str("error: ...")`（`KYT_STRING`），不 assert、不崩溃。
- 成功数值 → `kv_f`/`kv_int`。

## 依赖与不变式

- 依赖：`kronyx` 核心（`vm`）、`ecs`（`ky_world_*`）、`2d`（`kyTransform`）、`input`。
- 不变：不修改 `vm.c`/`ecs.c` 既有语义之外的行为；`vm` 仍无 GC（字符串 `strdup` 随宿主进程回收，见 progress 已知债 G15）。

## 测试策略

`tests/test_kyx_bindings.c`（无头、逻辑层）：
1. `std.spawn` → 得句柄 → `std.alive(handle)==1`。
2. `std.despawn(handle)` → `std.alive(handle)==0`。
3. C 侧建带 Transform 实体 → 脚本 `std.setpos(handle,1.5,-2.5)` → C 侧读回一致。
4. `std.getpos` 读 x/y；`std.setscale/getscale` 同理。
5. `ky_input_simulate`(左/右) → 脚本 `std.poll_input()` → `std.axis_x()` == ∓1。
6. `std.log` 打印（不断言输出，只验证不崩）。
7. 传 0 句柄 / 无 Transform 实体的 set/get → 返回 `KYT_STRING` 错误串。

## 退出标准

- 30 断言全绿；ASan(Leak)全量 `ctest` 15/15；`ky_demo` 无头 exit=0。
