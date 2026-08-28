#ifndef KRONYX_TEST_H
#define KRONYX_TEST_H

#include <stdio.h>

#define KY_TEST_MAIN() \
    int main(void) { \
        ky_test_run_all(); \
        printf("%s: %d assertions, %d failures\n", __FILE__, ky_test_assertions, ky_test_failures); \
        return ky_test_failures ? 1 : 0; \
    }

extern int ky_test_failures;
extern int ky_test_assertions;

static inline void ky_test_record(const char *file, int line, const char *expr, int ok) {
    ky_test_assertions++;
    if (!ok) {
        ky_test_failures++;
        printf("FAIL %s:%d: %s\n", file, line, expr);
    }
}

#define KY_CHECK(cond) ky_test_record(__FILE__, __LINE__, #cond, (cond) ? 1 : 0)
#define KY_CHECK_NEAR(a, b, eps) ky_test_record(__FILE__, __LINE__, #a " ~= " #b, \
    ((a) > (b) - (eps) && (a) < (b) + (eps)) ? 1 : 0)

void ky_test_run_all(void);

#endif
