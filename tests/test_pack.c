#include "kronyx/pack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
static int file_exists(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
}
static int cmd_exists(const char *cmd) {
    char buf[512];
    snprintf(buf, sizeof(buf), "where %s >NUL 2>&1", cmd);
    return system(buf) == 0;
}
#else
#include <sys/stat.h>
#include <unistd.h>
static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}
static int cmd_exists(const char *cmd) {
    char buf[512];
    snprintf(buf, sizeof(buf), "which %s > /dev/null 2>&1", cmd);
    return system(buf) == 0;
}
#endif

static int assertions = 0;
static int failures = 0;

#define ASSERT(cond, msg) do { \
    assertions++; \
    if (!(cond)) { \
        failures++; \
        printf("FAIL: %s at line %d\n", msg, __LINE__); \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while(0)

#define SKIP(msg) do { \
    printf("SKIP: %s\n", msg); \
} while(0)

static const char *GAME_SCRIPT =
    "function main() {\n"
    "    std.print(\"hello from packaged game\");\n"
    "    std.print(42);\n"
    "    return 0;\n"
    "}\n";

int main(void) {
    printf("=== Pack Test ===\n");

    ASSERT(strcmp(ky_pack_target_name(KY_PACK_EXE), "exe") == 0, "target name exe");
    ASSERT(strcmp(ky_pack_target_name(KY_PACK_APK), "apk") == 0, "target name apk");

    char err[512];
    int rc;

    /* invalid args */
    ASSERT(ky_pack(NULL, KY_PACK_EXE, err, sizeof(err)) != 0, "null desc rejected");
    kyPackDesc bad = { .title = "t", .version = "1.0", .script = NULL, .out_path = "/tmp/x" };
    ASSERT(ky_pack(&bad, KY_PACK_EXE, err, sizeof(err)) != 0, "missing script rejected");

    /* apk: now succeeds, emitting a deployable Android project skeleton */
    kyPackDesc desc = { .title = "demo-game", .version = "0.1.0",
                         .script = GAME_SCRIPT, .out_path = "/tmp/kypack_test.apk" };

    /* exe: real compile+link, then run it.
     * Requires `cc` on PATH and a prebuilt static engine lib at
     * $KY_PACK_ENGINE_ROOT/build/libky_engine.a. On Windows CI neither
     * is available, so skip gracefully instead of failing. */
#if !defined(_WIN32)
    if (!cmd_exists("cc")) {
        SKIP("exe pack: cc compiler not available on this platform");
    } else {
        const char *exe_path = "/tmp/kypack_test_game";
        desc.out_path = exe_path;
        rc = ky_pack(&desc, KY_PACK_EXE, err, sizeof(err));
        ASSERT(rc == 0, "exe pack succeeds");
        if (rc != 0) printf("  err: %s\n", err);
        ASSERT(file_exists(exe_path), "exe file exists");
        if (file_exists(exe_path)) {
            char run_cmd[512];
            snprintf(run_cmd, sizeof(run_cmd), "%s > /tmp/kypack_exe_out.txt 2>&1", exe_path);
            int exit_code = system(run_cmd);
            ASSERT(exit_code == 0, "packaged exe runs successfully");
            FILE *f = fopen("/tmp/kypack_exe_out.txt", "r");
            char buf[256] = {0};
            if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); (void)n; }
            ASSERT(strstr(buf, "hello from packaged game") != NULL, "exe output contains game print");
            ASSERT(strstr(buf, "42") != NULL, "exe output contains number");
        }
    }
#else
    SKIP("exe pack: requires Unix toolchain (cc + static lib)");
#endif

    /* npm */
    const char *npm_dir = "/tmp/kypack_test_npm";
    desc.out_path = npm_dir;
    rc = ky_pack(&desc, KY_PACK_NPM, err, sizeof(err));
    ASSERT(rc == 0, "npm pack succeeds");
    char path[512];
    snprintf(path, sizeof(path), "%s/package.json", npm_dir);
    ASSERT(file_exists(path), "npm package.json exists");
    snprintf(path, sizeof(path), "%s/bin/cli.js", npm_dir);
    ASSERT(file_exists(path), "npm bin/cli.js exists");
    snprintf(path, sizeof(path), "%s/assets/game.kyx", npm_dir);
    ASSERT(file_exists(path), "npm assets/game.kyx exists");
    if (file_exists(path)) {
        FILE *f = fopen(path, "r");
        char buf[256] = {0};
        if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); (void)n; }
        ASSERT(strstr(buf, "function main") != NULL, "npm game.kyx contains script");
    }

    /* jar: requires `zip` on PATH. Skip gracefully when missing. */
#if !defined(_WIN32)
    if (!cmd_exists("zip")) {
        SKIP("jar pack: zip not installed on this platform");
    } else {
        const char *jar_path = "/tmp/kypack_test.jar";
        desc.out_path = jar_path;
        rc = ky_pack(&desc, KY_PACK_JAR, err, sizeof(err));
        ASSERT(rc == 0, "jar pack succeeds");
        if (rc != 0) printf("  err: %s\n", err);
        ASSERT(file_exists(jar_path), "jar file exists");
        if (file_exists(jar_path)) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "unzip -p %s assets/game.kyx > /tmp/kypack_jar_out.txt 2>/dev/null", jar_path);
            int has_unzip = system("which unzip > /dev/null 2>&1") == 0;
            if (has_unzip) {
                system(cmd);
                FILE *f = fopen("/tmp/kypack_jar_out.txt", "r");
                char buf[256] = {0};
                if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); (void)n; }
                ASSERT(strstr(buf, "function main") != NULL, "jar contains game.kyx with script");
            } else {
                printf("SKIP: unzip not installed, jar content check skipped\n");
            }
        }
    }
#else
    SKIP("jar pack: requires zip on PATH (Unix tooling)");
#endif

    /* apk: full Android project skeleton.  Pure file assertions — no external
     * toolchain — so it runs on every platform including Windows CI.
     * Use a PID-unique dir: a fresh dir is guaranteed clean on every run, so
     * no rm -rf needed (which would also shell out under ctest). */
    {
        char unique_dir[128];
        snprintf(unique_dir, sizeof(unique_dir), "/tmp/kypack_test_apk_%d",
                 (int)getpid());
        const char *apk_root = unique_dir;
        desc.out_path = apk_root;
        rc = ky_pack(&desc, KY_PACK_APK, err, sizeof(err));
        ASSERT(rc == 0, "apk pack succeeds");
        if (rc != 0) printf("  err: %s\n", err);

        char apath[1100];
        snprintf(apath, sizeof(apath), "%s/settings.gradle", apk_root);
        ASSERT(file_exists(apath), "apk settings.gradle exists");
        snprintf(apath, sizeof(apath), "%s/build.gradle", apk_root);
        ASSERT(file_exists(apath), "apk top build.gradle exists");
        snprintf(apath, sizeof(apath), "%s/app/build.gradle", apk_root);
        ASSERT(file_exists(apath), "apk app/build.gradle exists");
        snprintf(apath, sizeof(apath), "%s/app/src/main/AndroidManifest.xml", apk_root);
        ASSERT(file_exists(apath), "apk AndroidManifest.xml exists");
        /* activity class name is PascalCase(title) with '_' segments joined,
         * e.g. "demo-game" -> "Demo_Game"; file = <that>.java in the slash-sep
         * package dir. */
        snprintf(apath, sizeof(apath), "%s/app/src/main/java/com/kronyx/demo_game/Demo_Game.java", apk_root);
        ASSERT(file_exists(apath), "apk Java activity exists");
        snprintf(apath, sizeof(apath), "%s/app/src/main/jni/CMakeLists.txt", apk_root);
        ASSERT(file_exists(apath), "apk jni CMakeLists.txt exists");
        snprintf(apath, sizeof(apath), "%s/app/src/main/jni/kronyx_jni.c", apk_root);
        ASSERT(file_exists(apath), "apk kronyx_jni.c exists");
        snprintf(apath, sizeof(apath), "%s/app/src/main/jniLibs/README.txt", apk_root);
        ASSERT(file_exists(apath), "apk jniLibs README exists");
        snprintf(apath, sizeof(apath), "%s/app/src/main/assets/game.kyx", apk_root);
        ASSERT(file_exists(apath), "apk assets/game.kyx exists");

        /* script embedded verbatim into assets */
        if (file_exists(apath)) {
            FILE *f = fopen(apath, "r");
            char buf[512] = {0};
            if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); (void)n; }
            ASSERT(strstr(buf, "function main") != NULL, "apk game.kyx contains script");
        }
        /* JNI bridge references the engine VM API + JNI_OnLoad */
        snprintf(apath, sizeof(apath), "%s/app/src/main/jni/kronyx_jni.c", apk_root);
        if (file_exists(apath)) {
            FILE *f = fopen(apath, "r");
            char buf[8192] = {0};
            if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); (void)n; }
            ASSERT(strstr(buf, "JNI_OnLoad") != NULL, "apk jni has JNI_OnLoad");
            ASSERT(strstr(buf, "ky_vm_call") != NULL, "apk jni calls ky_vm_call");
            ASSERT(strstr(buf, "GAME_SCRIPT") != NULL, "apk jni embeds script");
        }
        /* jni CMake links android libs, no host deps */
        snprintf(apath, sizeof(apath), "%s/app/src/main/jni/CMakeLists.txt", apk_root);
        if (file_exists(apath)) {
            FILE *f = fopen(apath, "r");
            char buf[4096] = {0};
            if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); (void)n; }
            ASSERT(strstr(buf, "kronyx_jni.c") != NULL, "apk CMake refs jni source");
            ASSERT(strstr(buf, "log") != NULL, "apk CMake links android log lib");
        }
    }

    printf("\n=== %d tests ran, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
