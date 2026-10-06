# G14 APK 打包 — 技术设计

## 总体结构

`KY_PACK_APK` 是 `ky_pack` 的一个 target 分支（与 `pack_exe`/`pack_npm`/`pack_jar` 并列，`src/pack/pack.c`）。它不 shell 出外部工具（NDK/gradle 都不可依赖），而是**纯文件生成器**：把一套 Android 工程模板 + 用户脚本写成磁盘布局。这与 `pack_npm` 的生成方式一致（`pack.c:154-202` 纯 `write_file`/`mkdir_p`，无 system 调用），故 APK 目标也走纯文件路径，`system()` 零调用 → headless 可测、无 shell 注入面。

## 引擎 CMake 的 Android 分支（R3.2，配套改动）

为让 `kronyx_jni.c` 将来能被 NDK 编译，引擎 CMake 加一个 `ANDROID` 条件分支，排除 host-only 依赖：

1. **`ky_runtime`（`CMakeLists.txt:215`）**：`if(ANDROID)` 时不建（GLFW 头在 NDK 下不存在）。JNI 桥不经过 windowing，直接调 `ky_core`/`ky_engine` 的脚本 API，无需 runtime。
2. **`ky_antitamper`（`CMakeLists.txt:239-257`）+ `KyAntiTamper`（`259-289`）**：`if(ANDROID)` 时跳过两个目标 + 跳过 `find_package(OpenSSL REQUIRED)`（NDK 无 OpenSSL，会 FATAL_ERROR）。
3. **`ky_demo`（`297-299`）**：它链 `ky_runtime`+`ky_antitamper`，Android 下不建（demo 是 host 可执行，JNI 不走它）。
4. **`ky_core`/`ky_engine`**：保持全量。EGL/GLES 探测（`72-82`）在 NDK 下 `find_library(GLESv2)` 大概率落空 → `KY_HAS_EGL` 不定义 → GL 后端走 stub 分支（`gl_backend.c:453`），编译过但 GPU 渲染在 stub。这是可接受的降级：脚本引擎 + 逻辑不依赖 GPU。

实现方式：用 `if(ANDROID)` 包裹这三个目标块 + OpenSSL 分支。`ANDROID` 变量由 NDK 的 toolchain file（`android-ndk…/build/cmake/android.toolchain.cmake`）自动设置，无需手动。

## `kronyx_jni.c`（R2）

NDK 编译的目标是 `libkronyx.so`，由生成的 `jni/CMakeLists.txt` 驱动。该 CMakeLists 在 NDK 树里编译引擎源码（`kronyx_jni.c` + 引擎脚本子系统源文件）并链 `log`。`kronyx_jni.c` 结构：

```c
#include <jni.h>
#include "kronyx/script.h"
static const char *g_script;             /* 由 write_c_string 嵌入 */
Java_..._KronyxActivity_nativeOnResult   /* 回调 native->java */
JNIEXPORT jint JNICALL Java_..._nativeRun(JNIEnv *env, jobject obj) {
    kyVM *vm = ky_vm_create(NULL);
    /* 注册 std.print 的 native（与 pack.c EXE 模板同一套 L99-110） */
    ky_vm_load_string(vm, g_script, "game");
    kyValue ret; ky_vm_call(vm, "main", NULL, 0, &ret);
    ky_vm_destroy(vm);
    /* 经 JNI 回调 nativeOnResult(code) */
    return (jint)ret.as.ival;
}
```

- 包名 / Activity 名从 `desc->title` 派生（转 camelCase 标识符，去非字母数字，首字母大写；空则 `kronyx`/`KronyxActivity`）。
- `JNI_OnLoad` 注册 native 方法（`RegisterNatives`）避免依赖 Java 侧方法全限定名拼接。

## 生成文件布局（R1.1，相对 `out_path`）

```
<out_path>/
  settings.gradle                 # include ':app'
  build.gradle                    # 顶层（AGP 插件 classpath）
  AndroidManifest.xml             # 在 app/ 下（见下）
  app/
    build.gradle                # 编译级：ndk { abiFilters arm64-v8a }，jni 源
    src/main/
      AndroidManifest.xml        # 声明 Activity + INTERNET(可选)/硬件
      java/<pkg>/KronyxActivity.java   # 启动时调 nativeRun
      jni/
        CMakeLists.txt           # 驱动 NDK 编 libkronyx.so
        kronyx_jni.c             # JNI 桥（嵌入脚本）
      jniLibs/README.txt         # 说明：NDK 构建后 .so 放这里
      assets/game.kyx            # desc->script 原样
```

（`AndroidManifest.xml` 实际放在 `app/src/main/` 下，R1.1 列在顶层是叙述位置；生成时写到 `app/src/main/AndroidManifest.xml`。）

## 转义（R5.2）

`write_c_string`（`pack.c:76`）已能安全转义脚本进 C 字符串。对 `title`/`version` 进 `.java` 标识符与 `.gradle` 字符串，加一个 `apk_escape_ident`（白名单 `[A-Za-z0-9_]`，其余替 `_`）和 `apk_gradle_str`（双引号/反斜杠转义，进 JSON 风格字符串）。

## 测试（R4）

`tests/test_pack.c` 新增 APK 小节（紧接 jar 之后）：
- 翻转 L69-73 的 not-supported 断言为成功断言。
- `ky_pack(&desc, KY_PACK_APK, err, sizeof(err))` 到 `/tmp/kypack_test_apk`。
- 断言 `rc==0`；`settings.gradle`、`app/build.gradle`、`app/src/main/AndroidManifest.xml`、`.../KronyxActivity.java`、`.../jni/CMakeLists.txt`、`.../jni/kronyx_jni.c`、`.../assets/game.kyx` 均存在。
- `game.kyx` 内容含 `function main`；`kronyx_jni.c` 含 `JNI_OnLoad` 与 `ky_vm_call`。

纯文件断言，无 `system`/`cc`/`zip` 依赖 → 全平台（含 Windows CI）可跑，与现有 test_pack 的 headless 风格一致。

## 风险

- 生成的是**工程骨架**，非可签名 APK。真实 `cmake -DANDROID` + gradle + 签名在 headless CI 无法验证（deferred，见 requirements.md"不在范围"）。本刀只保证文件布局正确 + 引擎 CMake 的 `ANDROID` 分支配置期不 FATAL_ERROR（`find_package(OpenSSL REQUIRED)` 被 `if(ANDROID)` 跳过）。
- NDK 下 GL 后端走 stub（`KY_HAS_EGL` 落空），GPU 渲染失效——对"跑脚本逻辑"够用，对"渲染画面"不足（渲染是 G12/Vulkan 的领域）。
