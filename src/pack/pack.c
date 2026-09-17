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
    if (n < 0 || n >= (int)sizeof(cmd)) return pack_fail(err, err_size, "cmd overflow");
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
    if (n < 0 || n >= (int)sizeof(cmd)) return pack_fail(err, err_size, "cmd overflow");
    int rc = system(cmd);
    if (rc != 0) return pack_fail(err, err_size, "zip failed (is zip installed?)");
    return 0;
}

int ky_pack(const kyPackDesc *desc, kyPackTarget target, char *err, int err_size) {
    if (!desc || !desc->out_path) return pack_fail(err, err_size, "invalid pack desc");
    if (!desc->script) return pack_fail(err, err_size, "missing game script");
    if (err && err_size > 0) err[0] = '\0';

    switch (target) {
        case KY_PACK_EXE: return pack_exe(desc, err, err_size);
        case KY_PACK_NPM: return pack_npm(desc, err, err_size);
        case KY_PACK_JAR: return pack_jar(desc, err, err_size);
        case KY_PACK_APK:
            return pack_fail(err, err_size, "apk packaging not supported yet");
    }
    return pack_fail(err, err_size, "unknown pack target");
}
