#ifndef KRONYX_LOG_H
#define KRONYX_LOG_H

#include "defines.h"

typedef enum kyLogLevel {
    KY_LOG_TRACE = 0,
    KY_LOG_DEBUG = 1,
    KY_LOG_INFO = 2,
    KY_LOG_WARN = 3,
    KY_LOG_ERROR = 4,
} kyLogLevel;

typedef void (*kyLogSink)(kyLogLevel level, const char *msg, void *user_data);

KY_API void ky_log_set_level(kyLogLevel level);
KY_API kyLogLevel ky_log_get_level(void);
KY_API void ky_log_set_sink(kyLogSink sink, void *user_data);
KY_API void ky_log_write(kyLogLevel level, const char *fmt, ...);
KY_API const char *ky_log_level_name(kyLogLevel level);

#define KY_LOG_TRACE(...) ky_log_write(KY_LOG_TRACE, __VA_ARGS__)
#define KY_LOG_DEBUG(...) ky_log_write(KY_LOG_DEBUG, __VA_ARGS__)
#define KY_LOG_INFO(...) ky_log_write(KY_LOG_INFO, __VA_ARGS__)
#define KY_LOG_WARN(...) ky_log_write(KY_LOG_WARN, __VA_ARGS__)
#define KY_LOG_ERROR(...) ky_log_write(KY_LOG_ERROR, __VA_ARGS__)

#endif
