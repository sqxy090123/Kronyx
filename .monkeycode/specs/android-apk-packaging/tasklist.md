# G14 APK 打包 — 实施任务列表

## 任务 1：引擎 CMake 的 Android 分支（R3.2）

- [ ] `CMakeLists.txt`：`if(ANDROID)` 包裹 `ky_runtime`（215）、`ky_antitamper`（239-257）、`KyAntiTamper`（259-289）、`ky_demo`（297-299）四块；ANDROID 下跳过 `find_package(OpenSSL REQUIRED)` 与 `find_library(glfw)`
- [ ] 验收：`ANDROID` 为真时配置期不再 FATAL_ERROR；`ky_core`/`ky_engine` 仍全量编译

## 任务 2：`pack.c` 的 `pack_apk`（R1/R2/R5）

- [ ] `apk_escape_ident(const char *)`：`[A-Za-z0-9_]` 白名单，其余替 `_`，空返默认
- [ ] `apk_gradle_str(FILE*, const char*)`：双引号/反斜杠转义后写进 C 字符串字面量
- [ ] `pack_apk(const kyPackDesc *desc, char *err, int err_size)`：
  - `path_is_safe(out_path)` 校验（复用 `pack.c:24`）
  - 派生 `pkg`（`com.kronyx` + title 标识符）与 `Activity`（`KronyxActivity`）
  - `mkdir_p` 逐级建目录；`write_file` 写 settings.gradle / 顶层 build.gradle / app/build.gradle / app/src/main/AndroidManifest.xml / KronyxActivity.java / jni/CMakeLists.txt / jni/kronyx_jni.c / jniLibs/README.txt / assets/game.kyx
  - `kronyx_jni.c` 经 `write_c_string` 嵌入 `desc->script`，调 `ky_vm_*` API（与 EXE 模板 L99-126 同源）
  - 返回 0；任何 `write_file` 失败 → `pack_fail`
- [ ] `ky_pack` switch（`pack.c:246-253`）：`KY_PACK_APK` 由 `pack_fail(...not supported)` 改调 `pack_apk(desc, err, err_size)`
- [ ] 验收：`rc==0`，文件布局齐全，脚本已嵌入

## 任务 3：测试翻转 + 断言（R4）

- [ ] `tests/test_pack.c` L69-73 的 "apk returns not-supported" 断言删除/翻转
- [ ] 新增 APK 小节：`ky_pack(&desc, KY_PACK_APK, …)` 到 `/tmp/kypack_test_apk`，断言 rc==0 + R1.1 文件存在 + `game.kyx` 含 `function main` + `kronyx_jni.c` 含 `JNI_OnLoad`/`ky_vm_call`
- [ ] 全量 `ctest -j1` 23/23；ASan `detect_leaks=0` 23/23；`ky_demo` exit=0

## 任务 4：回归 + 文档回写

- [ ] 全量普通 + ASan ctest + ky_demo 冒烟
- [ ] `progress.md` G14 标记完成，"下一刀"指向 G12 Vulkan（无头受阻，需真 GPU）
- [ ] 提交：CMake + pack.c + test_pack.c + spec + progress 分别归入一次代码 commit 与一次 progress commit

## 备注

- APK 目标是**纯文件生成器**（零 `system` 调用），与 `pack_npm` 同风格，headless 可测。
- 真实 NDK 交叉编译 + gradle + 签名 + 设备安装 deferred（requirements.md"不在范围"）；本刀只保证工程骨架 + 引擎 CMake 的 ANDROID 分支配置期可过。
- `desc->script` 经 `write_c_string` 嵌入 C 源，含引号/换行/控制字符安全。
