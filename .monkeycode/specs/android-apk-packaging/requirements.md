# G14 APK 打包 — 需求规格

## 背景

`ky_pack`（`src/pack/pack.c`）现支持 4 个目标：`KY_PACK_EXE` / `KY_PACK_NPM` / `KY_PACK_JAR` 已实现，`KY_PACK_APK` 在 `pack.c:250-251` 仅返回 "apk packaging not supported yet"。`progress.md:95` 把 G14 记为"现返回 not-supported"。

G14 的目标是让 `ky_pack(desc, KY_PACK_APK, …)` 产出一个**可部署的 Android 工程骨架**——包含一个能跑 Kronyx 脚本引擎的 Activity + JNI 桥 + NDK 构建脚本——使得把 `game.kyx` 脚本打进 APK 后，游戏逻辑可在 Android 上运行。

## 范围（EARS）

本规格约束的是 `ky_pack` 的 `KY_PACK_APK` 目标。**无头可测**是硬约束：产出的是工程文件布局，不依赖 NDK/gradle/签名等外部工具链（与本 headless 回归门一致）。

### R1 产出 Android 工程布局

- R1.1 [EVENT] When 调用方以 `desc->out_path`（目录）+ `desc->script`（Kronyx 脚本）调用 `ky_pack(&desc, KY_PACK_APK, …)`，The system shall 在 `out_path` 下生成一个完整 Android 工程目录：
  - `AndroidManifest.xml`
  - `app/src/main/java/<pkg>/<Activity>.java`
  - `app/src/main/jniLibs/`（占位，注释说明 NDK 构建后放 .so）
  - `app/src/main/jni/CMakeLists.txt`（NDK 用的 CMake，构建 `libkronyx.so`）
  - `app/src/main/jni/kronyx_jni.c`（JNI 桥 + 脚本内嵌 + 引擎调用）
  - `app/build.gradle` + `app/src/main/java/.../AndroidManifest` 所需 `build.gradle`（app + project 两级）
  - `settings.gradle`
  - `app/src/main/assets/game.kyx`（desc->script 原样）
- R1.2 [STATE] While `desc->script` 为空或 `desc->out_path` 为空，The system shall 返回 -1 并填写 `err`（与 EXE/NPM 目标同契约）。
- R1.3 [EVENT] When 生成成功，The system shall 返回 0 且 `err` 清空。

### R2 JNI 桥 + 引擎调用

- R2.1 [STATE] The `kronyx_jni.c` shall 通过 JNI 注册 native 方法，并调用 Kronyx 引擎 API 创建 VM、加载内嵌脚本、执行 `main`，把结果经 `nativeOnResult(code)` 回调回 Java 侧。
- R2.2 [STATE] The 内嵌脚本使用与 `pack_exe` 相同的 `write_c_string` 转义机制嵌入 C 源，保证脚本含引号/换行/控制字符时仍可编译。
- R2.3 [STATE] The `kronyx_jni.c` 调用引擎脚本 API（`ky_vm_create` / `ky_vm_load_string` / `ky_vm_call` / `ky_vm_destroy`，与 `pack.c` EXE 模板 L99-126 同一套 API），不引入引擎未导出的符号。

### R3 与引擎 NDK 可编译性对齐

- R3.1 [STATE] The 生成的 `jni/CMakeLists.txt` 仅链 `log`/`android`（Android 平台库）与 `libkronyx`，不依赖 GLFW/OpenSSL/X11；引擎侧 G14 配套改动（见 design.md）保证 `ky_core`/`ky_engine` 在 Android toolchain 下可编译。
- R3.2 [DEF] If 引擎源码中存在 Android 无法链接的 host-only 依赖，Then 引擎 CMake 的 `ANDROID` 分支 shall 排除/桩化这些目标（`ky_runtime` 的 GLFW 依赖、`ky_antitamper` 的 OpenSSL 依赖），使 `ky_core`/`ky_engine` 独立可编。

### R4 测试可无头验证

- R4.1 [EVENT] When 运行 `tests/test_pack.c`，The system shall 新增 APK 小节：调用 `ky_pack(&desc, KY_PACK_APK, …)` 到 `/tmp/kypack_test_apk`，断言 `rc==0` 且 R1.1 列举的关键文件均 `file_exists`，且 `game.kyx` 内容含脚本、`kronyx_jni.c` 含 `JNI_OnLoad`/`kronyx` 调用。
- R4.2 [STATE] The 现有 "apk returns not-supported" 断言（test_pack.c L69-73）shall 被替换为"apk 现在成功"断言。

### R5 安全

- R5.1 [STATE] The APK 目标 shall 复用 `path_is_safe` 校验 `out_path`（与 EXE 一致，`pack.c:243`），拒绝含 shell 元字符的路径。
- R5.2 [STATE] 生成的 `.java` / `.gradle` / `CMakeLists.txt` 中，`desc->title`/`version` 写入前 shall 做最小转义（双引号/反斜杠），避免注入进 JSON 或 C 字符串。

## 不在范围（明确 deferred）

- 实际 NDK 交叉编译产出 `.so`（需 NDK 工具链，headless 环境不保证可用）——G14 只产出**可喂给 NDK 构建的工程文件**，不保证 `cmake -DANDROID` 在本机跑通。
- 真实 gradle 构建 + 签名 + 安装到设备/模拟器（需 Android SDK + 设备）。
- 引擎 CMake 的 `ANDROID` 分支在本无头 CI 上实际编译验证（NDK 不在 CI 矩阵内）——本刀只落 CMake 改动，编译验证标注为"需 NDK 环境"。

## 验收

- `tests/test_pack.c` APK 小节全绿（R4.1），且原 not-supported 断言已翻转（R4.2）。
- 全量 `ctest -j1` 23/23（pack 断言挂在 test_pack 内，测试数不变）；ASan `detect_leaks=0` 23/23；`ky_demo` exit=0。
- 生成物可被人工 review 出"是一个可 NDK 构建的 Android 工程"（文件齐全、脚本已嵌入、JNI 桥调引擎 API）。
