#include "kronyx/time.h"

#if defined(KY_PLATFORM_WIN32)
  #include <windows.h>
#else
  #include <time.h>
#endif

#if defined(KY_PLATFORM_WIN32)

static double win_freq(void) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (double)f.QuadPart;
}

double ky_time_now(void) {
    static double freq = 0.0;
    if (freq == 0.0) freq = win_freq();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / freq;
}

uint64_t ky_time_now_us(void) {
    return (uint64_t)(ky_time_now() * 1e6);
}

#else

double ky_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

uint64_t ky_time_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)(ts.tv_nsec / 1000);
}

#endif
