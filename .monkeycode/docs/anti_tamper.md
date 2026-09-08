# KyAntiTamper - 跨平台反篡改验证系统

## 架构

双组件设计：

| 组件 | 类型 | 说明 |
|------|------|------|
| `ky_antitamper` | 静态库 | 嵌入游戏 exe，负责加载验证库 |
| `KyAntiTamper.dll/.so/.dylib` | 共享库 | 独立验证库，执行多轮 HMAC+TOTP 挑战响应 |

## 验证流程

1. 游戏 exe 启动时调用 `ky_tamper_init()`
2. 从 exe 同目录动态加载验证库（`KyAntiTamper.dll` / `libKyAntiTamper.so` / `libKyAntiTamper.dylib`）
3. 验证库检查自身完整性（PE/ELF/Mach-O 签名验证）
4. 生成 256 字节随机 salt
5. 运行 5 轮 HMAC-SHA256 + TOTP 挑战响应（每轮 800ms，共约 4-5 秒）
6. 验证库将实际使用的 salt 通过 `salt_out` 返回给 exe
7. exe 侧用相同算法独立重建最后一轮 HMAC，与验证库返回值比对；不匹配时按 mode 处理（ERROR 中止 / WARNING 继续）
8. 验证通过返回 `KY_TAMPER_OK`

## 开发者密钥（环境变量）

| 值 | 行为 |
|----|------|
| 未设置或 `"0"` | 标准模式，salt 作为密钥 |
| `"release:<hex64>"` | 生产密钥，使用自定义密钥 |
| `"dev:<hex64>"` | 开发密钥，跳过部分检查 |
| `<hex64>` (64字符) | 直接写入 32 字节密钥 |

示例：
```bash
# 开发环境（使用简单密钥以便调试）
export KY_ANTITEMPER_DEV_KEY="dev:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
```

## API

```c
// 设置回调（可选，在 init 前调用）
ky_tamper_set_log_cb(KyTamperLogFn fn);        // 日志回调
ky_tamper_set_dialog_cb(KyTamperDialogFn fn);   // 弹窗回调
ky_tamper_set_alloc_cb(KyTamperAllocFn fn);     // 内存分配回调
ky_tamper_set_free_cb(KyTamperFreeFn fn);       // 内存释放回调

// 初始化（必须在 Init 前调用）
KyTamperResult ky_tamper_init(KyTamperMode mode, size_t prealloc_bytes);

// 关闭
void ky_tamper_shutdown(void);

// 查询状态
int  ky_tamper_is_verified(void);
KyTamperResult ky_tamper_last_result(void);
```

## 返回值

| 常量 | 值 | 说明 |
|------|----|------|
| `KY_TAMPER_OK` | 0 | 验证通过 |
| `KY_TAMPER_FAIL_LICENSE` | -1 | 许可证/密钥无效 |
| `KY_TAMPER_FAIL_INTEGRITY` | -2 | 验证库完整性校验失败 |
| `KY_TAMPER_FAIL_HMAC` | -3 | HMAC 校验不匹配 |
| `KY_TAMPER_FAIL_TOTP` | -4 | TOTP 校验不匹配 |
| `KY_TAMPER_FAIL_RANDOM` | -5 | Salt 生成失败 |
| `KY_TAMPER_FAIL_TIMEOUT` | -6 | 验证超时 |
| `KY_TAMPER_FAIL_MISSING_LIB` | -7 | 验证库未找到 |
| `KY_TAMPER_FAIL_NO_SYMBOL` | -8 | 验证库缺少导出函数 |
| `KY_TAMPER_FAIL_NOT_VERIFIED` | -9 | 尚未调用 init |
| `KY_TAMPER_FAIL_ALREADY_DONE` | -10 | 已初始化 |

## 构建与部署

### Linux

```bash
cmake -B build && cmake --build build -j2
# 产物：build/libKyAntiTamper.so + build/ky_demo
# 运行时必须在 exe 同目录：./ky_demo
```

### macOS

```bash
cmake -B build && cmake --build build -j2
# 产物：build/libKyAntiTamper.dylib + build/ky_demo
```

### Windows

```bat
cmake -B build && cmake --build build -j2
# 产物：build/KyAntiTamper.dll + build/ky_demo.exe
```

打包时将 `KyAntiTamper.dll/.so/.dylib` 与游戏 exe 放在同一目录即可。

## 平台实现

| 平台 | HMAC | 随机数 | 完整性检查 |
|------|------|--------|-----------|
| Windows | BCRYPT HMAC API | BCRYPT Random | PE MZ 签名 |
| macOS | CommonCrypto | CCRandomGenerateBytes | Mach-O 属性检查 |
| Linux | OpenSSL HMAC | RAND_bytes + /dev/urandom fallback | ELF 0x7F'E'L'F' 签名 |

## Demo 运行

```bash
cd build && ./ky_demo
# 验证通过（无论是否设置密钥，验证库均通过）
# exit: 1 为 GLFW 无显示器导致的预期错误
```

## 测试

```bash
ctest --test-dir build --output-on-failure -j1
# anti_tamper 测试覆盖：
#   - 公共 API 初始状态 / init / 重复 init / shutdown 复位
#   - 验证库直接加载（dlopen）：version、参数校验、完整验证、salt 返回、TOTP 格式
#   - 确定性：同 salt 重放产生相同 HMAC
# 注意：单次验证约 4 秒（5 轮 x 800ms），anti_tamper 测试总耗时约 12 秒
```
