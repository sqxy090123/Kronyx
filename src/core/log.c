#include "kronyx/log.h"
#include <stdarg.h>
#include <time.h>

static kyLogLevel g_log_level = KY_LOG_INFO;
static kyLogSink g_sink = NULL;
static void *g_sink_ud = NULL;

static void default_sink(kyLogLevel level, const char *msg, void *ud) {
    KY_UNUSED(ud);
    const char *name = ky_log_level_name(level);
    FILE *out = (level >= KY_LOG_ERROR) ? stderr : stdout;
    time_t t = time(NULL);
    struct tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm_buf);
    fprintf(out, "[%s] [%s] %s\n", ts, name, msg);
    fflush(out);
}

void ky_log_set_level(kyLogLevel level) {
    g_log_level = level;
}

kyLogLevel ky_log_get_level(void) {
    return g_log_level;
}

void ky_log_set_sink(kyLogSink sink, void *user_data) {
    g_sink = sink;
    g_sink_ud = user_data;
}

const char *ky_log_level_name(kyLogLevel level) {
    switch (level) {
        case KY_LOG_TRACE: return "TRACE";
        case KY_LOG_DEBUG: return "DEBUG";
        case KY_LOG_INFO: return "INFO";
        case KY_LOG_WARN: return "WARN";
        case KY_LOG_ERROR: return "ERROR";
    }
    return "?";
}

void ky_log_write(kyLogLevel level, const char *fmt, ...) {
    if (level < g_log_level) return;
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (g_sink) {
        g_sink(level, buf, g_sink_ud);
    } else {
        default_sink(level, buf, NULL);
    }
}
