#ifndef KRONYX_DEFINES_H
#define KRONYX_DEFINES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define KY_API

#define KY_PI 3.14159265358979323846
#define KY_TWO_PI 6.28318530717958647692
#define KY_DEG2RAD 0.01745329251994329577
#define KY_RAD2DEG 57.29577951308232087680

#define KY_UNUSED(x) ((void)(x))
#define KY_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define KY_ALIGN8(n) (((n) + 7u) & ~7u)

#if defined(_WIN32)
  #define KY_PLATFORM_WIN32 1
#elif defined(__APPLE__)
  #define KY_PLATFORM_MACOS 1
#else
  #define KY_PLATFORM_LINUX 1
#endif

#if defined(_MSC_VER)
  #define KY_INLINE __forceinline
#else
  #define KY_INLINE static inline
#endif

#endif
