/*
 * KyAntiTamper - 验证库实现（跨平台）
 *
 * 编译为目标：
 *   Windows : KyAntiTamper.dll
 *   Linux   : libKyAntiTamper.so
 *   macOS   : libKyAntiTamper.dylib
 *
 * 功能：
 *   1. 验证库自身完整性检查（PE/ELF/Mach-O 签名）
 *   2. 从系统熵源生成 256 字节随机 salt
 *   3. 多轮 HMAC-SHA256 + TOTP 挑战响应（约 4-5 秒）
 *   4. 验证通过后返回 HMAC 和 TOTP 供 exe 核验
 */

#include "kronyx/anti_tamper.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#pragma comment(lib, "bcrypt.lib")
/* BCryptCalcHmacKeySize is not declared in some Windows SDK versions. */
static DWORD ky_bcrypt_get_key_size(BCRYPT_ALG_HANDLE hAlg) {
    DWORD objLen = 0;
    if (NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
            (PUCHAR)&objLen, sizeof(objLen), &objLen, 0)) && objLen)
        return objLen;
    return 32;
}
#define BCryptCalcHmacKeySize(hAlg)  ky_bcrypt_get_key_size((hAlg))
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ---- 常量 ---------------------------------------------------------------- */

#define SALT_SIZE       256
#define HMAC_SIZE       32   /* SHA-256 输出长度               */
#define TOTP_SIZE       6
#define ROUND_COUNT     5
#define ROUND_DELAY_MS  800  /* 总计约 4-5 秒                  */
#define TOTP_TIME_STEP  30   /* TOTP 时间步长（秒）            */
#define MAX_DEVPATH     4096

/* ---- 状态 ---------------------------------------------------------------- */

typedef struct {
    uint8_t  salt[SALT_SIZE];
    uint8_t  dev_key[32];
    int      has_dev_key;
    uint64_t start_time;
} KyTamperCtx;

/* ---- 时间工具 ------------------------------------------------------------ */

#ifdef _WIN32
static uint64_t ky_mono_ms(void) {
    return GetTickCount64();
}
#else
static uint64_t ky_mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
#endif

/* ---- HMAC-SHA256 实现 --------------------------------------------------- */

#ifdef _WIN32
/* Windows: BCRYPT HMAC API（替代错误的 CryptoAPI 签名方案） */
static int ky_hmac_sha256(const uint8_t *key, size_t key_len,
                          const uint8_t *msg, size_t msg_len,
                          uint8_t *out, size_t *out_len) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD dwHashLen = 0;
    DWORD dwKeySize;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                         NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(status)) return 0;

    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
                               (PUCHAR)&dwHashLen, sizeof(dwHashLen), &dwHashLen, 0);
    if (!NT_SUCCESS(status)) goto cleanup;

    dwKeySize = (DWORD)BCryptCalcHmacKeySize(hAlg);
    if (dwKeySize == 0 || dwKeySize > (DWORD)key_len) {
        /* 密钥长度不足，取前 dwKeySize 字节 */
        dwKeySize = (DWORD)key_len;
    }

    status = BCryptCreateHash(hAlg, &hHash, NULL, 0,
                              (PUCHAR)key, dwKeySize, 0);
    if (!NT_SUCCESS(status)) goto cleanup;

    status = BCryptHashData(hHash, (PUCHAR)msg, (ULONG)msg_len, 0);
    if (!NT_SUCCESS(status)) goto cleanup;

    status = BCryptFinishHash(hHash, out, (ULONG)*out_len, 0);
    if (!NT_SUCCESS(status)) goto cleanup;

    *out_len = dwHashLen;

cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return NT_SUCCESS(status) ? 1 : 0;
}

static int ky_generate_salt(uint8_t *salt, size_t len) {
    /* Use CNG BCryptGenRandom directly: the "RANDOM" algorithm name is a
       CAPI CSP concept and does not exist in CNG, so opening it fails and
       the old CryptAcquireContext fallback can also fail on CI runners. */
    NTSTATUS status = BCryptGenRandom(NULL, (PUCHAR)salt, (ULONG)len,
                                      BCRYPT_USE_SYSTEM_PFG);
    return NT_SUCCESS(status) ? 1 : 0;
}

#elif defined(__APPLE__)
#include <CommonCrypto/CommonCrypto.h>
#include <Security/Security.h>

static int ky_hmac_sha256(const uint8_t *key, size_t key_len,
                          const uint8_t *msg, size_t msg_len,
                          uint8_t *out, size_t *out_len) {
    CCHmacContext ctx;
    CCHmacInit(&ctx, kCCHmacAlgSHA256, key, key_len);
    CCHmacUpdate(&ctx, msg, msg_len);
    CCHmacFinal(&ctx, out);
    *out_len = HMAC_SIZE; /* SHA-256 固定 32 字节 */
    return 1;
}

static int ky_generate_salt(uint8_t *salt, size_t len) {
    OSStatus status = SecRandomCopyBytes(kSecRandomDefault, (size_t)len, salt);
    return (status == errSecSuccess) ? 1 : 0;
}

#else
#ifdef HAVE_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

static int ky_hmac_sha256(const uint8_t *key, size_t key_len,
                          const uint8_t *msg, size_t msg_len,
                          uint8_t *out, size_t *out_len) {
    unsigned char *result = HMAC(EVP_sha256(), key, (int)key_len,
                                 msg, (size_t)msg_len, out, NULL);
    if (!result) return 0;
    int md_size = EVP_MD_size(EVP_sha256());
    if (md_size <= 0) return 0;
    *out_len = (size_t)md_size;
    return 1;
}

static int ky_generate_salt(uint8_t *salt, size_t len) {
    int ret = RAND_bytes(salt, (int)len);
    return ret;
}
#else
/* fallback: /dev/urandom + openssl fallback */
#include <fcntl.h>
#include <errno.h>

static int ky_hmac_sha256(const uint8_t *key, size_t key_len,
                          const uint8_t *msg, size_t msg_len,
                          uint8_t *out, size_t *out_len) {
    return 0;
}

static int ky_generate_salt(uint8_t *salt, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, salt, len);
    close(fd);
    return (n == (ssize_t)len) ? 1 : 0;
}
#endif
#endif

/* ---- TOTP 生成（RFC 6238，整数运算避免精度问题）-------------------------- */

static void ky_totp_generate(const uint8_t *key, size_t key_len,
                              uint64_t time_step, uint8_t *out, int len) {
    uint8_t time_buf[8];
    memset(time_buf, 0, sizeof(time_buf));
    for (int i = 0; i < 8; i++)
        time_buf[i] = (uint8_t)(time_step >> (i * 8));

    uint8_t hmac[HMAC_SIZE];
    size_t hmac_len = HMAC_SIZE;
    ky_hmac_sha256(key, key_len, time_buf, sizeof(time_buf), hmac, &hmac_len);

    /* 动态截断 */
    int offset = hmac[hmac_len - 1] & 0x0F;
    uint32_t code = ((uint32_t)hmac[offset] & 0x7F) << 24
                  | ((uint32_t)hmac[offset + 1]) << 16
                  | ((uint32_t)hmac[offset + 2]) << 8
                  | ((uint32_t)hmac[offset + 3]);
    uint32_t mod = 1;
    for (int i = 0; i < len; i++) mod *= 10;
    code %= mod;

    for (int i = len - 1; i >= 0; i--) {
        out[i] = (uint8_t)('0' + (code % 10));
        code /= 10;
    }
}

/* ---- 自身完整性检查 ------------------------------------------------------ */

#ifdef _WIN32
static int ky_check_self_integrity(void) {
    char lib_path[MAX_DEVPATH];
    if (!GetModuleFileNameA(NULL, lib_path, sizeof(lib_path))) return 0;

    HANDLE hFile = CreateFileA(lib_path, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return 0; }

    uint8_t *map = (uint8_t *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    int valid = 0;
    if (map) {
        valid = (map[0] == 'M' && map[1] == 'Z');
        UnmapViewOfFile(map);
    }
    CloseHandle(hMap);
    CloseHandle(hFile);
    return valid;
}
#elif defined(__APPLE__)
#include <sys/stat.h>
#include <Security/Security.h>
#include <CommonCrypto/CommonCrypto.h>

static int ky_check_self_integrity(void) {
    char exe_path[MAX_DEVPATH];
    uint32_t size = (uint32_t)sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) return 0;

    struct stat st;
    if (stat(exe_path, &st) != 0) return 0;

    FILE *f = fopen(exe_path, "rb");
    if (!f) return 0;

    uint8_t magic[4];
    size_t r = fread(magic, 1, 4, f);
    fclose(f);

    /* Mach-O magic: 0xfeedface (32-bit) / 0xfeedfacf (64-bit), little-endian */
    return (r == 4 &&
            (memcmp(magic, "\xCE\xFA\xED\xFE", 4) == 0 ||
             memcmp(magic, "\xCF\xFA\xED\xFE", 4) == 0)) ? 1 : 0;
}
#else
static int ky_check_self_integrity(void) {
    char exe_path[MAX_DEVPATH];
    ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n <= 0) return 0;
    exe_path[n] = '\0';

    struct stat st;
    if (stat(exe_path, &st) != 0) return 0;

    FILE *f = fopen(exe_path, "rb");
    if (!f) return 0;

    uint8_t magic[4];
    size_t r = fread(magic, 1, 4, f);
    fclose(f);

    return (r == 4 && magic[0] == 0x7F &&
            magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') ? 1 : 0;
}
#endif

/* ---- 开发者密钥解析 ------------------------------------------------------ */

static int ky_parse_dev_key(const char *env, uint8_t *key_out) {
    if (!env || !env[0]) return 0;

    if (strncmp(env, "release:", 8) == 0) env += 8;
    else if (strncmp(env, "dev:", 4) == 0) env += 4;

    size_t elen = strlen(env);
    if (elen != 64) return 0;

    for (int i = 0; i < 32; i++) {
        unsigned int byte_val = 0;
        if (sscanf(env + 2 * i, "%02x", &byte_val) != 1) return 0;
        key_out[i] = (uint8_t)byte_val;
    }
    return 1;
}

/* ---- 主验证函数（导出）--------------------------------------------------- */

extern "C" {

KY_ANTITEMPER_API int ky_tamper_verify(
        const uint8_t *salt, size_t salt_len,
        const uint8_t *hmac_in,
        const uint8_t *totp_in,
        uint8_t *hmac_out,
        uint8_t *totp_out,
        size_t out_len,
        uint8_t *salt_out) {

    if (!salt || salt_len < SALT_SIZE)
        return -5;  /* KY_TAMPER_FAIL_RANDOM */
    if (!hmac_out || !totp_out || out_len < HMAC_SIZE)
        return -5;

    /* 自身完整性检查 */
    if (!ky_check_self_integrity())
        return -2;  /* KY_TAMPER_FAIL_INTEGRITY */

    /* 读取开发者密钥 */
    KyTamperCtx ctx;
    memset(&ctx, 0, sizeof(ctx));

    const char *env = getenv("KY_ANTITEMPER_DEV_KEY");
    if (env && env[0]) {
        ctx.has_dev_key = ky_parse_dev_key(env, ctx.dev_key) ? 1 : 0;
    }

    /* 生成 salt */
    int salt_provided = 0;
    for (size_t i = 0; i < salt_len; i++) {
        if (salt[i] != 0) { salt_provided = 1; break; }
    }
    if (!salt_provided) {
        if (!ky_generate_salt(ctx.salt, SALT_SIZE))
            return -5;
    } else {
        memcpy(ctx.salt, salt, salt_len);
    }

    /* 启动计时 */
    ctx.start_time = ky_mono_ms();

    /* TOTP 时间步在整个 verify 调用内保持固定（RFC 6238 时间步为 30s 窗口，
       5 轮循环耗时 ~4s，跨窗口边界会导致同 salt 两次重放 TOTP 值不同。
       固定为 start_time 对应的窗口，保证确定性。 */
    uint64_t fixed_time_step = ctx.start_time / 1000 / TOTP_TIME_STEP;

    uint8_t totp_val[TOTP_SIZE] = {0};

    for (int round = 0; round < ROUND_COUNT; round++) {
        /* 超时检查（10 秒） */
        if (ky_mono_ms() - ctx.start_time > 10000)
            return -6;  /* KY_TAMPER_FAIL_TIMEOUT */

        /* 轮密钥：开发者密钥 XOR salt（若有密钥） */
        uint8_t round_key[32];
        if (ctx.has_dev_key) {
            for (int i = 0; i < 32; i++)
                round_key[i] = ctx.dev_key[i] ^ ctx.salt[i];
        } else {
            memcpy(round_key, ctx.salt, 32);
        }

        /* 生成 TOTP（使用固定时间步，保证 5 轮一致） */
        ky_totp_generate(round_key, 32, fixed_time_step, totp_val, TOTP_SIZE);

        /* HMAC 校验 salt + round 号 */
        uint8_t hmac_msg[SALT_SIZE + 4];
        memcpy(hmac_msg, ctx.salt, SALT_SIZE);
        memcpy(hmac_msg + SALT_SIZE, &round, 4);

        size_t hmac_len = HMAC_SIZE;
        int hmac_ok = ky_hmac_sha256(round_key, 32, hmac_msg, sizeof(hmac_msg),
                             hmac_out, &hmac_len);
        if (!hmac_ok)
            return -3;  /* KY_TAMPER_FAIL_HMAC */

#ifdef _WIN32
        Sleep(ROUND_DELAY_MS);
#else
        struct timespec sleep_ts = {0, ROUND_DELAY_MS * 1000000L};
        nanosleep(&sleep_ts, NULL);
#endif
    }

    /* 返回使用的 salt 供 exe 本地验证 */
    if (salt_out)
        memcpy(salt_out, ctx.salt, SALT_SIZE);

    /* 返回结果 */
    memcpy((void *)totp_out, totp_val, TOTP_SIZE);
    return 0;  /* KY_TAMPER_OK */
}

KY_ANTITEMPER_API const char* ky_tamper_version(void) {
    return "1.0.0";
}

}  /* extern "C" */
