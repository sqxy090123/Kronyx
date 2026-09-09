# Kronyx 脚本引擎安全审计报告

## 审计时间
2026-09-09

## 攻击者视角分析

假设一个恐怖游戏使用 Kronyx 脚本引擎执行玩家自定义脚本（如 AI 行为、谜题逻辑、
自定义物品效果）。攻击者通过加载恶意脚本文件，可尝试以下攻击向量：

### 攻击面 1: 运算符注入导致栈溢出 (已修复 - CVE pending)
**漏洞类型**: Out-of-Bounds Write / Stack Overflow
**严重性**: CRITICAL
**触发条件**: 编译包含超过 64 个局部变量的函数
**攻击示例**:
```
# 理论上构造大量局部变量导致寄存器溢出
function exploit() {
    var v1=1; var v2=2; ... var v65=65;
    return v65;
}
```
**现状**: 编译器已通过 KYX_MAX_LOCALS=64 限制局部变量数量，栈溢出被阻止。

### 攻击面 2: 移位操作未定义行为 (已修复)
**漏洞类型**: Undefined Behavior (UB)
**严重性**: HIGH
**描述**: 左移/右移操作的移位量未加边界检查，当移位量 >= 64 时触发 C 语言 UB。
**PoC**:
```
function f(a, b) { return a << b; }
function test() { return f(1, 100); }  // 1 << 100 = UB
```
**影响**: 在 ASan/UBSan 下触发 runtime error，可能导致不可预测的行为。
**修复**: 移位量限制到 [0, 62] 范围，使用无符号移位避免 signed overflow。

### 攻击面 3: OP_CALL 缺少边界检查 (已修复)
**漏洞类型**: Out-of-Bounds Read
**严重性**: MEDIUM
**描述**: OP_CALL 指令的 fn_reg 参数缺少栈边界验证，恶意字节码可导致越界读取。
**修复**: 添加 `fn_reg` 和 `base + fn_reg + nargs` 的边界检查。

### 攻击面 4: AST 内存泄漏 (已修复)
**漏洞类型**: Memory Leak
**严重性**: LOW
**描述**: `ast_free_node` 对 BINOP、UNOP、CALL.callee、INDEX 节点的子节点使用
`free()` 而非 `ast_free()`，导致递归子树中的 IDENT/STRING 等动态分配内存泄漏。
**影响**: 每次脚本解析泄漏约 4-12 字节，长期运行会导致内存持续增长。
**修复**: 将 `free()` 替换为 `ast_free()` 以正确递归释放子节点。

### 攻击面 5: to_float/to_int 类型处理漏洞 (已修复 - Issue #4)
**漏洞类型**: 逻辑错误 (boolean 被当 0) + 未定义行为 (大浮点转 int)
**严重性**: MEDIUM
**描述 1**: `to_float` 缺少 `KYT_BOOL` 分支，所有位运算对布尔值都返回 0。
**PoC**: `function f(a){return a & 1;} f(true)` → 0 (应为 1)
**描述 2**: `to_int` 的 `(int64_t)d` 对 `|d|>INT64_MAX` 触发 UB。
**描述 3**: `load_const` 中 `val == (double)(int64_t)val && fabs(val)<1e15` 的 cast 先求值，UB。
**修复**: 加 bool 分支；to_int 饱和到 INT64_MAX/MIN；load_const 调整短路顺序；OP_BNOT 改走 to_int。

### 攻击面 6: parse_expression 错误路径悬垂返回 (已修复 - Issue #5)
**漏洞类型**: Use-After-Free / Double-Free (ASan 可稳定复现)
**严重性**: HIGH
**描述**: `parse_expression` 在二元操作右操作数解析失败时执行 `ast_free(n); return left;`，
其中 `ast_free(n)` 已递归释放挂在 `n->as.binop.left` 上的 `left`，导致返回给调用方的
是悬垂指针，上层继续引用并在 `kyx_parser_destroy` 时二次释放。
**PoC** (任一即触发 ASan):
```
function f(){return 1 + ;}
var a = =;
function f(){return (1 + ;)}
```
**修复**: 错误路径改为 `free(n)`，只释放 BINOP 节点自身，保留 `left` 的调用方所有权。
