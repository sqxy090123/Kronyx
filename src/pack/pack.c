#include "kronyx/pack.h"
#include "kronyx/script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
static int ky_mkdir(const char *p, int m) { (void)m; return _mkdir(p); }
#define mkdir ky_mkdir
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifndef KY_PACK_ENGINE_ROOT
#define KY_PACK_ENGINE_ROOT "."
#endif

/* Refuse shell metacharacters in user-supplied paths to prevent command
 * injection via the shell invocations below. */
static int path_is_safe(const char *p) {
    if (!p) return 0;
    for (const char *c = p; *c; c++) {
        if (*c == ';' || *c == '&' || *c == '|' || *c == '$' ||
            *c == '`' || *c == '"' || *c == '\'' || *c == '(' ||
            *c == ')' || *c == '<' || *c == '>' || *c == '\n')
            return 0;
    }
    return 1;
}

static const char *target_name(kyPackTarget t) {
    switch (t) {
        case KY_PACK_EXE: return "exe";
        case KY_PACK_NPM: return "npm";
        case KY_PACK_JAR: return "jar";
        case KY_PACK_APK: return "apk";
    }
    return "unknown";
}

const char *ky_pack_target_name(kyPackTarget target) {
    return target_name(target);
}

static int pack_fail(char *err, int err_size, const char *msg) {
    if (err && err_size > 0) snprintf(err, (size_t)err_size, "%s", msg);
    return -1;
}

static void mkdir_p(const char *path) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t len = strlen(content);
    int ok = fwrite(content, 1, len, f) == len;
    fclose(f);
    return ok ? 0 : -1;
}

static int write_c_string(FILE *f, const char *s) {
    fputs("static const char GAME_SCRIPT[] = \"", f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (*p < 0x20 || *p >= 0x7f) fprintf(f, "\\x%02x\"\"", *p);
                else fputc(*p, f);
        }
    }
    fputs("\";\n", f);
    return 0;
}

static const char *EXE_MAIN_TEMPLATE =
    "#include \"kronyx/script.h\"\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "static kyValue native_print(kyVM *vm, kyValue *args, int argc, void *user) {\n"
    "    KY_UNUSED(vm); KY_UNUSED(user);\n"
    "    for (int i = 0; i < argc; i++) {\n"
    "        if (args[i].type == KYT_FLOAT) printf(\"%g\", args[i].as.fval);\n"
    "        else if (args[i].type == KYT_INT) printf(\"%lld\", (long long)args[i].as.ival);\n"
    "        else if (args[i].type == KYT_STRING) printf(\"%s\", args[i].as.sval);\n"
    "        else if (args[i].type == KYT_BOOL) printf(args[i].as.ival ? \"true\" : \"false\");\n"
    "        else if (args[i].type == KYT_NIL) printf(\"nil\");\n"
    "    }\n"
    "    printf(\"\\n\");\n"
    "    kyValue v; memset(&v, 0, sizeof(v)); return v;\n"
    "}\n"
    "int main(void) {\n"
    "    kyVM *vm = ky_vm_create(NULL);\n"
    "    if (!vm) return 1;\n"
    "    ky_vm_register_native(vm, \"std\", \"print\", native_print, NULL);\n"
    "    if (ky_vm_load_string(vm, GAME_SCRIPT, \"game\") != 0) {\n"
    "        fprintf(stderr, \"load error: %s\\n\", ky_vm_last_error(vm));\n"
    "        ky_vm_destroy(vm); return 1;\n"
    "    }\n"
    "    kyValue ret; memset(&ret, 0, sizeof(ret));\n"
    "    int rc = ky_vm_call(vm, \"main\", NULL, 0, &ret);\n"
    "    ky_vm_destroy(vm);\n"
    "    if (rc != 0) return 1;\n"
    "    if (ret.type == KYT_INT || ret.type == KYT_BOOL) return (int)ret.as.ival;\n"
    "    if (ret.type == KYT_FLOAT) return (int)ret.as.fval;\n"
    "    return 0;\n"
    "}\n";

static int pack_exe(const kyPackDesc *desc, char *err, int err_size) {
    char tmpdir[512];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/kypack_%d", (int)getpid());
    mkdir_p(tmpdir);
    char cpath[600];
    snprintf(cpath, sizeof(cpath), "%s/game_embed.c", tmpdir);

    FILE *f = fopen(cpath, "w");
    if (!f) return pack_fail(err, err_size, "cannot create temp embed file");
    write_c_string(f, desc->script);
    fprintf(f, "%s", EXE_MAIN_TEMPLATE);
    fclose(f);

    char cmd[2048];
    int n = snprintf(cmd, sizeof(cmd),
        "cc -O2 -I\"%s/include\" \"%s\" \"%s/build/libky_engine.a\" -lm -o \"%s\" 2>&1",
        KY_PACK_ENGINE_ROOT, cpath, KY_PACK_ENGINE_ROOT, desc->out_path);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return pack_fail(err, err_size, "cmd overflow");
    int rc = system(cmd);
    if (rc != 0) {
        pack_fail(err, err_size, "exe compile/link failed");
        return -1;
    }
    return 0;
}

static int pack_npm(const kyPackDesc *desc, char *err, int err_size) {
    mkdir_p(desc->out_path);
    char sub[1024];
    char path[1100];

    snprintf(sub, sizeof(sub), "%s/bin", desc->out_path);
    mkdir_p(sub);
    snprintf(sub, sizeof(sub), "%s/assets", desc->out_path);
    mkdir_p(sub);

    snprintf(path, sizeof(path), "%s/package.json", desc->out_path);
    char json[1024];
    snprintf(json, sizeof(json),
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"description\": \"Kronyx game package\",\n"
        "  \"bin\": { \"%s\": \"bin/cli.js\" },\n"
        "  \"files\": [\"bin\", \"assets\"],\n"
        "  \"license\": \"MIT\"\n"
        "}\n",
        desc->title ? desc->title : "kronyx-game",
        desc->version ? desc->version : "0.1.0",
        desc->title ? desc->title : "kronyx-game");
    if (write_file(path, json) != 0) return pack_fail(err, err_size, "write package.json failed");

    snprintf(path, sizeof(path), "%s/bin/cli.js", desc->out_path);
    const char *cli =
        "#!/usr/bin/env node\n"
        "const { spawn } = require('child_process');\n"
        "const path = require('path');\n"
        "const fs = require('fs');\n"
        "const plat = process.platform === 'win32' ? 'win' : process.platform === 'darwin' ? 'mac' : 'linux';\n"
        "const bin = path.join(__dirname, '..', 'bin', 'game-' + plat);\n"
        "if (fs.existsSync(bin)) {\n"
        "  spawn(bin, { stdio: 'inherit' }).on('close', c => process.exit(c));\n"
        "} else {\n"
        "  console.error('native engine binary not found for platform: ' + plat);\n"
        "  console.error('run: kyx-pack --target exe to generate it first');\n"
        "  process.exit(1);\n"
        "}\n";
    if (write_file(path, cli) != 0) return pack_fail(err, err_size, "write cli.js failed");

    snprintf(path, sizeof(path), "%s/assets/game.kyx", desc->out_path);
    if (write_file(path, desc->script ? desc->script : "") != 0)
        return pack_fail(err, err_size, "write game.kyx failed");

    return 0;
}

static int pack_jar(const kyPackDesc *desc, char *err, int err_size) {
    char tmpdir[512];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/kypack_%d", (int)getpid());
    mkdir_p(tmpdir);

    char path[1100];
    snprintf(path, sizeof(path), "%s/META-INF", tmpdir);
    mkdir_p(path);
    snprintf(path, sizeof(path), "%s/META-INF/MANIFEST.MF", tmpdir);
    char manifest[512];
    snprintf(manifest, sizeof(manifest),
        "Manifest-Version: 1.0\r\n"
        "Created-By: kyx-pack\r\n"
        "Implementation-Title: %s\r\n"
        "Implementation-Version: %s\r\n"
        "\r\n",
        desc->title ? desc->title : "kronyx-game",
        desc->version ? desc->version : "0.1.0");
    if (write_file(path, manifest) != 0) return pack_fail(err, err_size, "write manifest failed");

    snprintf(path, sizeof(path), "%s/assets", tmpdir);
    mkdir_p(path);
    snprintf(path, sizeof(path), "%s/assets/game.kyx", tmpdir);
    if (write_file(path, desc->script ? desc->script : "") != 0)
        return pack_fail(err, err_size, "write game.kyx failed");

    char cmd[1600];
    int n = snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && zip -q -r \"%s\" META-INF assets 2>&1", tmpdir, desc->out_path);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return pack_fail(err, err_size, "cmd overflow");
    int rc = system(cmd);
    if (rc != 0) return pack_fail(err, err_size, "zip failed (is zip installed?)");
    return 0;
}

/* R5.2: turn a user-supplied title into a safe Java package segment:
 * lowercased, non-identifier chars to '_', so the on-disk dir layout and
 * the .java package line stay in sync. */
static void apk_ident(const char *in, char *out, size_t out_size) {
    size_t o = 0;
    out[0] = 0;
    if (!in || !in[0]) in = "kronyx";
    for (size_t i = 0; in[i] && o + 1 < out_size; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (!( (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ))
            c = '_';
        out[o++] = c;
    }
    if (o == 0) out[o++] = 'k';
    out[o] = 0;
}

/* R5.2: build the Activity class name (PascalCase) from the same title. */
static void apk_activity(const char *in, char *out, size_t out_size) {
    char lower[128];
    apk_ident(in, lower, sizeof(lower));
    size_t n = strlen(lower);
    out[0] = 0;
    size_t o = 0;
    for (size_t i = 0; lower[i] && o + 1 < out_size; i++) {
        char c = lower[i];
        if (i == 0 || (i > 0 && lower[i - 1] == '_'))
            c = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        out[o++] = c;
    }
    if (o > 0 && lower[o - 1] == '_') out[--o] = 0;
    out[o] = 0;
    (void)n;
}

/* R1/R2/R3.1: emit a deployable Android project skeleton.  Pure file
 * generator (no system()/shell) so it is testable headless.  NDK build of
 * the .so and gradle+signing are out of scope (requirements.md). */
static int pack_apk(const kyPackDesc *desc, char *err, int err_size) {
    const char *root = desc->out_path;
    char pkgseg[128], act[192];
    apk_ident(desc->title, pkgseg, sizeof(pkgseg));
    apk_activity(desc->title, act, sizeof(act));
    char pkg[192];
    snprintf(pkg, sizeof(pkg), "com.kronyx.%s", pkgseg);

    char path[1400];
    char tmp[2048];

    /* --- project root: settings.gradle + top-level build.gradle --- */
    snprintf(path, sizeof(path), "%s", root);
    mkdir_p(path);
    snprintf(tmp, sizeof(tmp), "include ':app'\nrootProject.name = \"%s\"\n", pkgseg);
    snprintf(path, sizeof(path), "%s/settings.gradle", root);
    if (write_file(path, tmp) != 0)
        return pack_fail(err, err_size, "write settings.gradle failed");
    snprintf(path, sizeof(path), "%s/build.gradle", root);
    if (write_file(path,
        "buildscript {\n"
        "    repositories { google(); mavenCentral() }\n"
        "    dependencies { classpath 'com.android.tools.build:gradle:8.1.0' }\n"
        "}\n"
        "allprojects { repositories { google(); mavenCentral() } }\n") != 0)
        return pack_fail(err, err_size, "write build.gradle failed");

    /* --- app module layout: java source root mirrors the dotted package
     * using '/' separators (Android convention), plus jni / jniLibs / assets. */
    char jdir[1400];
    snprintf(jdir, sizeof(jdir), "%s/app/src/main/java/%s", root, pkg);
    /* Convert 'com.kronyx.demo_game' -> 'com/kronyx/demo_game' on disk. */
    for (char *c = jdir + strlen(root) + strlen("/app/src/main/java/"); *c; c++)
        if (*c == '.') *c = '/';
    mkdir_p(jdir);
    snprintf(path, sizeof(path), "%s/app/src/main/jni", root);
    mkdir_p(path);
    snprintf(path, sizeof(path), "%s/app/src/main/jniLibs/arm64-v8a", root);
    mkdir_p(path);
    snprintf(path, sizeof(path), "%s/app/src/main/assets", root);
    mkdir_p(path);

    /* --- app/build.gradle (NDK externalNativeBuild + abiFilters) --- */
    snprintf(path, sizeof(path), "%s/app/build.gradle", root);
    snprintf(tmp, sizeof(tmp),
        "plugins { id 'com.android.application' }\n"
        "android {\n"
        "    namespace '%s'\n"
        "    compileSdk 34\n"
        "    defaultConfig {\n"
        "        applicationId \"%s\"\n"
        "        minSdk 24\n"
        "        targetSdk 34\n"
        "        externalNativeBuild { cmake { arguments '-DKRONYX_ROOT=${KRONYX_ROOT}' } }\n"
        "        ndk { abiFilters 'arm64-v8a' }\n"
        "    }\n"
        "    externalNativeBuild { cmake { path 'src/main/jni/CMakeLists.txt' } }\n"
        "    buildTypes { release { minifyEnabled false } }\n"
        "}\n",
        pkg, pkg);
    if (write_file(path, tmp) != 0)
        return pack_fail(err, err_size, "write app/build.gradle failed");

    /* --- AndroidManifest.xml --- */
    snprintf(path, sizeof(path), "%s/app/src/main/AndroidManifest.xml", root);
    snprintf(tmp, sizeof(tmp),
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" "
        "package=\"%s\">\n"
        "    <application android:label=\"%s\" android:hasCode=\"true\">\n"
        "        <activity android:name=\".%s\" android:exported=\"true\">\n"
        "            <intent-filter>\n"
        "                <action android:name=\"android.intent.action.MAIN\"/>\n"
        "                <category android:name=\"android.intent.category.LAUNCHER\"/>\n"
        "            </intent-filter>\n"
        "        </activity>\n"
        "    </application>\n"
        "</manifest>\n",
        pkg, pkgseg, act);
    if (write_file(path, tmp) != 0)
        return pack_fail(err, err_size, "write AndroidManifest.xml failed");

    /* --- JNI bridge: embeds the script, runs it through the Kronyx VM (R2) --- */
    snprintf(path, sizeof(path), "%s/app/src/main/jni/kronyx_jni.c", root);
    {
        FILE *f = fopen(path, "w");
        if (!f) return pack_fail(err, err_size, "cannot write kronyx_jni.c");
        fputs(
            "#include <jni.h>\n"
            "#include <string.h>\n"
            "#include <android/log.h>\n"
            "#include \"kronyx/script.h\"\n"
            "#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, \"kronyx\", __VA_ARGS__)\n"
            "\n"
            "/* Embedded Kronyx game script (G14): */\n", f);
        fputs("static const char GAME_SCRIPT[] = \"", f);
        for (const unsigned char *p = (const unsigned char *)desc->script; *p; p++) {
            switch (*p) {
                case '"':  fputs("\\\"", f); break;
                case '\\': fputs("\\\\", f); break;
                case '\n': fputs("\\n", f);  break;
                case '\r': fputs("\\r", f);  break;
                case '\t': fputs("\\t", f);  break;
                default:
                    if (*p < 0x20 || *p >= 0x7f) fprintf(f, "\\x%02x", *p);
                    else fputc(*p, f);
            }
        }
        fputs(
            "\";\n"
            "\n"
            "JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *unused) {\n"
            "    (void)unused;\n"
            "    return JNI_VERSION_1_6;\n"
            "}\n"
            "\n"
            "/* Native entry: load + run the embedded script, return its code. */\n", f);
        fprintf(f, "JNIEXPORT jint JNICALL Java_%s_nativeRun(JNIEnv *env, jobject thiz) {\n", act);
        fputs(
            "    (void)env; (void)thiz;\n"
            "    kyVM *vm = ky_vm_create(NULL);\n"
            "    if (!vm) { LOGI(\"vm create failed\"); return -1; }\n"
            "    kyValue ret;\n"
            "    memset(&ret, 0, sizeof(ret));\n"
            "    if (ky_vm_load_string(vm, GAME_SCRIPT, \"game\") != 0) {\n"
            "        LOGI(\"load: %s\", ky_vm_last_error(vm));\n"
            "        ky_vm_destroy(vm);\n"
            "        return -1;\n"
            "    }\n"
            "    int rc = ky_vm_call(vm, \"main\", NULL, 0, &ret);\n"
            "    ky_vm_destroy(vm);\n"
            "    if (rc != 0) return -1;\n"
            "    if (ret.type == KYT_INT || ret.type == KYT_BOOL) return (jint)ret.as.ival;\n"
            "    if (ret.type == KYT_FLOAT) return (jint)ret.as.fval;\n"
            "    return 0;\n"
            "}\n", f);
        fclose(f);
    }

    /* --- jniLibs placeholder (R1.1) --- */
    snprintf(path, sizeof(path), "%s/app/src/main/jniLibs/README.txt", root);
    if (write_file(path,
        "After the NDK build (externalNativeBuild in app/build.gradle), the\n"
        "produced libkronyx.so lands here for arm64-v8a, or let AAB\n"
        "packaging collect it from the build output tree.\n") != 0)
        return pack_fail(err, err_size, "write jniLibs/README.txt failed");

    /* --- jni CMakeLists (R3.1): links android platform libs, no GLFW/OpenSSL --- */
    snprintf(path, sizeof(path), "%s/app/src/main/jni/CMakeLists.txt", root);
    if (write_file(path,
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(kronyx_android C)\n"
        "# Build the embedded engine script subsystem + JNI bridge as libkronyx.so.\n"
        "# KRONYX_ROOT is passed in from app/build.gradle (externalNativeBuild).\n"
        "if(NOT KRONYX_ROOT)\n"
        "    message(FATAL_ERROR \"KRONYX_ROOT must point at the Kronyx engine source tree\")\n"
        "endif()\n"
        "add_library(kronyx SHARED\n"
        "    kronyx_jni.c\n"
        "    ${KRONYX_ROOT}/src/script/script.c\n"
        "    ${KRONYX_ROOT}/src/script/gc.c\n"
        ")\n"
        "target_include_directories(kronyx PUBLIC\n"
        "    ${KRONYX_ROOT}/include\n"
        "    ${KRONYX_ROOT}/src/script\n"
        ")\n"
        "find_library(log-lib log)\n"
        "find_library(android-lib android)\n"
        "target_link_libraries(kronyx ${log-lib} ${android-lib})\n") != 0)
        return pack_fail(err, err_size, "write jni/CMakeLists.txt failed");

    /* --- assets/game.kyx (R1.1): the script, verbatim --- */
    snprintf(path, sizeof(path), "%s/app/src/main/assets/game.kyx", root);
    if (write_file(path, desc->script) != 0)
        return pack_fail(err, err_size, "write assets/game.kyx failed");

    /* --- Java activity (R1.1/R2): file lives in the slash-separated package
     * dir; class name is `act`, matching the manifest's activity name. --- */
    snprintf(path, sizeof(path), "%s/%s.java", jdir, act);
    snprintf(tmp, sizeof(tmp),
        "package %s;\n"
        "\n"
        "import android.app.Activity;\n"
        "import android.os.Bundle;\n"
        "\n"
        "public class %s extends Activity {\n"
        "    static {\n"
        "        System.loadLibrary(\"kronyx\");\n"
        "    }\n"
        "\n"
        "    @Override\n"
        "    protected void onCreate(Bundle savedInstanceState) {\n"
        "        super.onCreate(savedInstanceState);\n"
        "        int code = nativeRun();\n"
        "        if (code != 0) finish();\n"
        "    }\n"
        "\n"
        "    private native int nativeRun();\n"
        "}\n",
        pkg, act);
    if (write_file(path, tmp) != 0)
        return pack_fail(err, err_size, "write Java activity failed");

    return 0;
}

int ky_pack(const kyPackDesc *desc, kyPackTarget target, char *err, int err_size) {
    if (!desc || !desc->out_path) return pack_fail(err, err_size, "invalid pack desc");
    if (!desc->script) return pack_fail(err, err_size, "missing game script");
    if (!path_is_safe(desc->out_path))
        return pack_fail(err, err_size, "out_path contains unsafe shell metacharacters");
    if (err && err_size > 0) err[0] = '\0';

    switch (target) {
        case KY_PACK_EXE: return pack_exe(desc, err, err_size);
        case KY_PACK_NPM: return pack_npm(desc, err, err_size);
        case KY_PACK_JAR: return pack_jar(desc, err, err_size);
        case KY_PACK_APK: return pack_apk(desc, err, err_size);
    }
    return pack_fail(err, err_size, "unknown pack target");
}
