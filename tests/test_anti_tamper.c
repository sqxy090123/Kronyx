/* Anti-tamper module tests: public API flow + shared library direct tests */

#include "kronyx/anti_tamper.h"
#include "kytest.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#endif

int ky_test_failures = 0;
int ky_test_assertions = 0;

/* ---- helpers ------------------------------------------------------------- */

typedef int  (*PFN_verify)(const uint8_t *, size_t, const uint8_t *,
                           const uint8_t *, uint8_t *, uint8_t *, size_t,
                           uint8_t *);
typedef const char* (*PFN_version)(void);

/* Build absolute path of a file next to this executable */
static void lib_path_in_exe_dir(const char *name, char *buf, size_t size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, (DWORD)size);
    char *s = strrchr(buf, '\\');
    if (s) *(s + 1) = '\0';
    strcat(buf, name);
#elif defined(__APPLE__)
    uint32_t len = (uint32_t)size;
    _NSGetExecutablePath(buf, &len);
    char *s = strrchr(buf, '/');
    if (s) *(s + 1) = '\0';
    strcat(buf, name);
#else
    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    if (n <= 0) { buf[0] = '\0'; return; }
    buf[n] = '\0';
    char *s = strrchr(buf, '/');
    if (s) *(s + 1) = '\0';
    strcat(buf, name);
#endif
}

/* ---- public API flow ------------------------------------------------------ */

static void test_initial_state(void) {
    KY_CHECK(ky_tamper_is_verified() == 0);
    KY_CHECK(ky_tamper_last_result() == KY_TAMPER_FAIL_NOT_VERIFIED);
}

static void test_init_flow(void) {
    /* Warning mode: runs full 5-round verification (~4s) */
    KyTamperResult r = ky_tamper_init(KY_TAMPER_MODE_WARNING, 1024);
    KY_CHECK(r == KY_TAMPER_OK);
    KY_CHECK(ky_tamper_is_verified() == 1);
    KY_CHECK(ky_tamper_last_result() == KY_TAMPER_OK);

    /* Double init is rejected */
    KY_CHECK(ky_tamper_init(KY_TAMPER_MODE_WARNING, 0) ==
             KY_TAMPER_FAIL_ALREADY_DONE);

    /* Shutdown resets state */
    ky_tamper_shutdown();
    KY_CHECK(ky_tamper_is_verified() == 0);
    KY_CHECK(ky_tamper_last_result() == KY_TAMPER_FAIL_NOT_VERIFIED);
}

/* ---- shared library direct tests ------------------------------------------ */

static void test_shared_library(void) {
    char path[4096];
#ifdef _WIN32
    lib_path_in_exe_dir("KyAntiTamper.dll", path, sizeof(path));
    HMODULE lib = LoadLibraryA(path);
    KY_CHECK(lib != NULL);
    if (!lib) return;
    PFN_verify verify = (PFN_verify)GetProcAddress(lib, "ky_tamper_verify");
    PFN_version version = (PFN_version)GetProcAddress(lib, "ky_tamper_version");
#else
#ifdef __APPLE__
    lib_path_in_exe_dir("libKyAntiTamper.dylib", path, sizeof(path));
#else
    lib_path_in_exe_dir("libKyAntiTamper.so", path, sizeof(path));
#endif
    void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    KY_CHECK(lib != NULL);
    if (!lib) {
        printf("  dlerror: %s\n", dlerror());
        return;
    }
    PFN_verify verify = (PFN_verify)dlsym(lib, "ky_tamper_verify");
    PFN_version version = (PFN_version)dlsym(lib, "ky_tamper_version");
#endif

    KY_CHECK(verify != NULL);
    KY_CHECK(version != NULL);
    if (!verify) return;

    /* version string */
    if (version) {
        const char *v = version();
        KY_CHECK(v != NULL && v[0] != '\0');
        KY_CHECK(strncmp(v, "1.", 2) == 0);
    }

    /* NULL salt is rejected */
    uint8_t hmac_out[64], totp_out[64], salt_out[256];
    KY_CHECK(verify(NULL, 0, NULL, NULL, hmac_out, totp_out,
                    sizeof(hmac_out), salt_out) == KY_TAMPER_FAIL_RANDOM);

    /* Invalid out buffer is rejected */
    KY_CHECK(verify(salt_out, sizeof(salt_out), NULL, NULL, NULL, totp_out,
                    sizeof(hmac_out), NULL) == KY_TAMPER_FAIL_RANDOM);

    /* Full verification pass (~4s): zero salt input -> library generates one */
    uint8_t salt_in[256];
    memset(salt_in, 0, sizeof(salt_in));
    memset(salt_out, 0xFF, sizeof(salt_out));
    memset(totp_out, 0, sizeof(totp_out));
    int r1 = verify(salt_in, sizeof(salt_in), NULL, NULL,
                    hmac_out, totp_out, sizeof(hmac_out), salt_out);
    if (r1 != KY_TAMPER_OK) {
        int sz = 0;
        for (size_t i = 0; i < sizeof(salt_out); i++)
            if (salt_out[i] != 0xFF) { sz = 1; break; }
        printf("  DIAG: r1=%d salt_out_written=%d totp[0]=%u\n",
               r1, sz, (unsigned)totp_out[0]);
    }
    KY_CHECK(r1 == KY_TAMPER_OK);

    /* Salt returned must be non-zero */
    int salt_nonzero = 0;
    for (size_t i = 0; i < sizeof(salt_out); i++)
        if (salt_out[i]) { salt_nonzero = 1; break; }
    KY_CHECK(salt_nonzero);

    /* TOTP must be 6 ASCII digits */
    int totp_digits = 1;
    for (int i = 0; i < 6; i++)
        if (totp_out[i] < '0' || totp_out[i] > '9') { totp_digits = 0; break; }
    KY_CHECK(totp_digits);

    /* Determinism: replay with the returned salt must reproduce same HMAC */
    uint8_t hmac_out2[64], totp_out2[64], salt_out2[256];
    int r2 = verify(salt_out, sizeof(salt_out), NULL, NULL,
                    hmac_out2, totp_out2, sizeof(hmac_out2), salt_out2);
    KY_CHECK(r2 == KY_TAMPER_OK);
    KY_CHECK(memcmp(hmac_out, hmac_out2, 32) == 0);
    KY_CHECK(memcmp(salt_out, salt_out2, sizeof(salt_out)) == 0);
}

void ky_test_run_all(void) {
    test_initial_state();
    test_shared_library();
    test_init_flow();
}

KY_TEST_MAIN()
