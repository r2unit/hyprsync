#ifndef HS_LOG_H
#define HS_LOG_H

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum {
    HS_LOG_TRACE,
    HS_LOG_DEBUG,
    HS_LOG_INFO,
    HS_LOG_WARN,
    HS_LOG_ERROR,
    HS_LOG_OFF
} hs_log_level;

static hs_log_level hs_current_log_level = HS_LOG_INFO;

static inline void hs_log_set_level(hs_log_level level) {
    hs_current_log_level = level;
}

static inline hs_log_level hs_log_level_from_string(const char *s) {
    if (!s) return HS_LOG_INFO;
    if (strcmp(s, "trace") == 0) return HS_LOG_TRACE;
    if (strcmp(s, "debug") == 0) return HS_LOG_DEBUG;
    if (strcmp(s, "info") == 0) return HS_LOG_INFO;
    if (strcmp(s, "warn") == 0) return HS_LOG_WARN;
    if (strcmp(s, "error") == 0) return HS_LOG_ERROR;
    if (strcmp(s, "off") == 0) return HS_LOG_OFF;
    return HS_LOG_INFO;
}

static inline const char *hs_log_level_name(hs_log_level level) {
    switch (level) {
        case HS_LOG_TRACE: return "trace";
        case HS_LOG_DEBUG: return "debug";
        case HS_LOG_INFO:  return "info";
        case HS_LOG_WARN:  return "warn";
        case HS_LOG_ERROR: return "error";
        case HS_LOG_OFF:   return "off";
    }
    return "info";
}

#define hs_log(level, fmt, ...) do { \
    if ((level) >= hs_current_log_level) { \
        time_t hs_log_t_ = time(NULL); \
        struct tm hs_log_tm_; \
        localtime_r(&hs_log_t_, &hs_log_tm_); \
        char hs_log_ts_[20]; \
        strftime(hs_log_ts_, sizeof(hs_log_ts_), "%H:%M:%S", &hs_log_tm_); \
        fprintf(stderr, "[%s] [%s] " fmt "\n", \
                hs_log_ts_, hs_log_level_name(level), ##__VA_ARGS__); \
    } \
} while (0)

#define hs_trace(fmt, ...) hs_log(HS_LOG_TRACE, fmt, ##__VA_ARGS__)
#define hs_debug(fmt, ...) hs_log(HS_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define hs_info(fmt, ...)  hs_log(HS_LOG_INFO, fmt, ##__VA_ARGS__)
#define hs_warn(fmt, ...)  hs_log(HS_LOG_WARN, fmt, ##__VA_ARGS__)
#define hs_error(fmt, ...) hs_log(HS_LOG_ERROR, fmt, ##__VA_ARGS__)

#endif
