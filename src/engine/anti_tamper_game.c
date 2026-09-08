/*
 * KyAntiTamper - 游戏 exe 集成层（跨平台）
 *
 * 加载验证库（Windows: .dll, Linux: .so, macOS: .dylib）
 * 运行多轮 HMAC+TOTP 挑战响应验证
 *
 * 流程：
 *   1. 动态加载验证库
 *   2. 将 salt 传给验证库生成 HMAC/TOTP
 *   3. exe 用相同算法验证返回的 HMAC
 *   4. 验证通过则启动游戏
 */

#include "kronyx/anti_tamper.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#endif

#include <stdarg.h>
#include <time.h>

/* ---- 常量 ---------------------------------------------------------------- */

#define KY_ANTITEMPER_LIB_NAME_WIN   "KyAntiTamper.dll"
#define KY_ANTITEMPER_LIB_NAME_UNIX  "libKyAntiTamper.so"
#define KY_ANTITEMPER_LIB_NAME_MAC   "libKyAntiTamper.dylib"
#define KY_PATH_MAX                  4096
#define KY_HMAC_SIZE                 32
#define KY_SALT_SIZE                 256
#define KY_ROUND_COUNT               5

/* ---- 函数指针类型 -------------------------------------------------------- */

typedef int  (*PFN_verify)(const uint8_t *, size_t, const uint8_t *,
                           const uint8_t *, uint8_t *, uint8_t *, size_t,
                           uint8_t *);
typedef const char* (*PFN_version)(void);

/* ---- 状态 ---------------------------------------------------------------- */

static void       *g_hLib = NULL;
static PFN_verify  g_fnVerify = NULL;
static PFN_version g_fnVer = NULL;
static KyTamperLogFn     g_log     = NULL;
static KyTamperDialogFn  g_dialog  = NULL;
static KyTamperAllocFn   g_alloc   = NULL;
static KyTamperFreeFn    g_free    = NULL;
static int               g_done    = 0;
static KyTamperResult    g_result  = KY_TAMPER_FAIL_NOT_VERIFIED;

/* ---- 日志 ---------------------------------------------------------------- */

static void ky_tlog(int level, const char *fmt, ...) {
    (void)level; (void)fmt;
    if (g_log) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        g_log(level, buf);
    }
}

/* ---- 路径获取 ------------------------------------------------------------ */

#ifdef _WIN32
static int ky_get_exe_dir(char *buf, size_t size) {
    if (!GetModuleFileNameA(NULL, buf, (DWORD)size)) return 0;
    char *s = strrchr(buf, '\\');
    if (s) { *(s + 1) = '\0'; return 1; }
    return 0;
}
#else
static int ky_get_exe_dir(char *buf, size_t size) {
    char exe_path[4096];

#ifdef __APPLE__
    uint32_t len = (uint32_t)size;
    if (_NSGetExecutablePath(exe_path, &len) != 0) return 0;
#else
    ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n <= 0) return 0;
    exe_path[n] = '\0';
#endif

    char *s = strrchr(exe_path, '/');
    if (s) {
        size_t dir_len = (size_t)(s - exe_path) + 1;
        if (dir_len >= size) return 0;
        memcpy(buf, exe_path, dir_len);
        buf[dir_len] = '\0';
        return 1;
    }
    buf[0] = '.';
    buf[1] = '/';
    buf[2] = '\0';
    return 1;
}
#endif

/* ---- 库加载 -------------------------------------------------------------- */

static int ky_load_lib(void) {
    char dir[KY_PATH_MAX];
    char path[KY_PATH_MAX];

    if (!ky_get_exe_dir(dir, sizeof(dir))) return 0;

#ifdef _WIN32
    strcpy(path, dir);
    strcat(path, KY_ANTITEMPER_LIB_NAME_WIN);
    g_hLib = LoadLibraryA(path);
    if (!g_hLib) return 0;

    g_fnVerify = (PFN_verify)GetProcAddress((HMODULE)g_hLib, "ky_tamper_verify");
    g_fnVer    = (PFN_version)GetProcAddress((HMODULE)g_hLib, "ky_tamper_version");
#else
    const char *suffix =
#ifdef __APPLE__
        KY_ANTITEMPER_LIB_NAME_MAC;
#else
        KY_ANTITEMPER_LIB_NAME_UNIX;
#endif
    strcpy(path, dir);
    strcat(path, suffix);

    g_hLib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!g_hLib) return 0;

    g_fnVerify = (PFN_verify)dlsym(g_hLib, "ky_tamper_verify");
    g_fnVer    = (PFN_version)dlsym(g_hLib, "ky_tamper_version");
#endif

    return (g_fnVerify != NULL) ? 1 : 0;
}

static void ky_unload_lib(void) {
    if (g_hLib) {
#ifdef _WIN32
        FreeLibrary((HMODULE)g_hLib);
#else
        dlclose(g_hLib);
#endif
        g_hLib = NULL;
        g_fnVerify = NULL;
        g_fnVer = NULL;
    }
}

/* ---- HMAC 计算（exe 侧本地验证）------------------------------------------ */

#ifdef HAVE_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>

static int ky_local_hmac_sha256(const uint8_t *key, size_t key_len,
                                const uint8_t *msg, size_t msg_len,
                                uint8_t *out, size_t *out_len) {
    unsigned char *result = HMAC(EVP_sha256(), key, (int)key_len,
                                 msg, msg_len, out, NULL);
    if (!result) return 0;
    if (out_len) *out_len = (size_t)EVP_MAX_MD_SIZE;
    return 1;
}
#endif

/* ---- 公共 API ------------------------------------------------------------ */

void ky_tamper_set_log_cb(KyTamperLogFn fn)      { g_log = fn; }
void ky_tamper_set_dialog_cb(KyTamperDialogFn fn) { g_dialog = fn; }
void ky_tamper_set_alloc_cb(KyTamperAllocFn fn)   { g_alloc = fn; }
void ky_tamper_set_free_cb(KyTamperFreeFn fn)     { g_free = fn; }

KyTamperResult ky_tamper_init(KyTamperMode mode, size_t prealloc_bytes) {
    if (g_done) return KY_TAMPER_FAIL_ALREADY_DONE;

    if (!ky_load_lib()) {
        g_result = KY_TAMPER_FAIL_MISSING_LIB;
        ky_tlog(0, "验证库加载失败");

        if (mode == KY_TAMPER_MODE_ERROR) {
            if (g_dialog) g_dialog(0, "Tamper Check",
                                   "验证失败：找不到验证库\n程序无法启动");
        } else {
            if (g_dialog)
                g_dialog(1, "Tamper Check (Warning)",
                         "验证失败：找不到验证库\n程序将继续运行，但不保证安全性");
            g_done = 1;
            g_result = KY_TAMPER_OK;
            goto done;
        }
        return g_result;
    }

    if (!g_fnVerify) {
        g_result = KY_TAMPER_FAIL_NO_SYMBOL;
        ky_unload_lib();
        ky_tlog(0, "验证库缺少导出函数");

        if (mode == KY_TAMPER_MODE_ERROR) {
            if (g_dialog) g_dialog(0, "Tamper Check",
                                   "验证库缺少导出函数，程序无法启动");
        } else {
            if (g_dialog)
                g_dialog(1, "Tamper Check (Warning)",
                        "验证库缺少导出函数，继续运行但不保证安全性");
            g_done = 1;
            g_result = KY_TAMPER_OK;
            goto done;
        }
        return g_result;
    }

    uint8_t salt[256] = {0};
    uint8_t salt_used[256] = {0};  /* dll 实际使用的 salt */
    uint8_t hmac_in[64] = {0}, hmac_out[64];
    uint8_t totp_in[64] = {0}, totp_out[64];

    ky_tlog(2, "开始反篡改验证...");

    g_result = g_fnVerify(salt, sizeof(salt),
                          hmac_in, totp_in,
                          hmac_out, totp_out,
                          sizeof(hmac_out),
                          salt_used);

    ky_unload_lib();

    if (g_result != KY_TAMPER_OK) {
        ky_tlog(0, "验证库返回错误: %d", g_result);
        const char *msg = "验证失败";
        switch (g_result) {
            case KY_TAMPER_FAIL_LICENSE:
                msg = "许可证无效，游戏无法启动"; break;
            case KY_TAMPER_FAIL_INTEGRITY:
                msg = "验证库完整性校验失败\n文件可能被篡改，游戏无法启动"; break;
            case KY_TAMPER_FAIL_HMAC:
                msg = "HMAC 校验失败，数据可能被篡改，游戏无法启动"; break;
            case KY_TAMPER_FAIL_TOTP:
                msg = "TOTP 校验失败，时间不同步，游戏无法启动"; break;
            case KY_TAMPER_FAIL_RANDOM:
                msg = "随机数生成失败，游戏无法启动"; break;
            case KY_TAMPER_FAIL_TIMEOUT:
                msg = "验证超时，游戏无法启动"; break;
            default: break;
        }

        if (mode == KY_TAMPER_MODE_ERROR) {
            if (g_dialog) g_dialog(0, "Tamper Check", msg);
        } else {
            if (g_dialog)
                g_dialog(1, "Tamper Check (Warning)",
                        "验证失败，但将继续运行（警告模式）");
            g_done = 1;
            g_result = KY_TAMPER_OK;
            goto done;
        }
        return g_result;
    }

    /* exe 侧本地验证 HMAC：复现最后一轮计算，与验证库返回值比对 */
#ifdef HAVE_OPENSSL
    /* 解析开发者密钥（与验证库一致的规则：release:/dev: 前缀或 64 位 hex） */
    uint8_t dev_key[32];
    int has_key = 0;
    {
        const char *env = getenv("KY_ANTITEMPER_DEV_KEY");
        if (env && env[0]) {
            const char *hex = env;
            if (strncmp(env, "release:", 8) == 0) hex = env + 8;
            else if (strncmp(env, "dev:", 4) == 0) hex = env + 4;
            if (strlen(hex) == 64) {
                has_key = 1;
                for (int i = 0; i < 32; i++) {
                    unsigned int b = 0;
                    if (sscanf(hex + 2 * i, "%02x", &b) != 1) { has_key = 0; break; }
                    dev_key[i] = (uint8_t)b;
                }
            }
        }
    }

    /* 重建最后一轮（round = KY_ROUND_COUNT-1）的 round_key 与消息 */
    uint8_t round_key[32];
    if (has_key) {
        for (int i = 0; i < 32; i++)
            round_key[i] = (uint8_t)(dev_key[i] ^ salt_used[i]);
    } else {
        memcpy(round_key, salt_used, 32);
    }

    uint8_t verify_msg[KY_SALT_SIZE + 4];
    memcpy(verify_msg, salt_used, KY_SALT_SIZE);
    int last_round = KY_ROUND_COUNT - 1;
    memcpy(verify_msg + KY_SALT_SIZE, &last_round, 4);

    uint8_t local_hmac[KY_HMAC_SIZE];
    int computed = ky_local_hmac_sha256(round_key, 32, verify_msg,
                                        sizeof(verify_msg), local_hmac, NULL);
    int hmac_match = computed &&
                     (memcmp(local_hmac, hmac_out, KY_HMAC_SIZE) == 0);

    if (hmac_match) {
        ky_tlog(2, "HMAC 本地验证通过");
    } else {
        ky_tlog(0, "HMAC 本地验证失败（与验证库返回值不匹配）");
        if (mode == KY_TAMPER_MODE_ERROR) {
            g_result = KY_TAMPER_FAIL_HMAC;
            if (g_dialog) g_dialog(0, "Tamper Check",
                                   "HMAC 校验失败，数据可能被篡改\n游戏无法启动");
            return g_result;
        }
        if (g_dialog) g_dialog(1, "Tamper Check (Warning)",
                               "HMAC 本地验证失败，继续运行（警告模式）");
    }
#else
    ky_tlog(1, "跳过本地 HMAC 验证（未链接 OpenSSL）");
#endif

    g_done = 1;
    g_result = KY_TAMPER_OK;

done:
    if (prealloc_bytes > 0 && g_alloc) {
        void *pool = g_alloc(prealloc_bytes);
        (void)pool;
    }
    return g_result;
}

void ky_tamper_shutdown(void) {
    ky_unload_lib();
    g_done = 0;
    g_result = KY_TAMPER_FAIL_NOT_VERIFIED;
}

int  ky_tamper_is_verified(void) { return g_done; }
KyTamperResult ky_tamper_last_result(void) { return g_result; }
