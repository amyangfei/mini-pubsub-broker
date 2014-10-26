#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "util.h"
#include "constant.h"
#include "broker.h"

static int srv_log_level = LOG_DEBUG;
static FILE *srv_log_fp  = NULL;

void log_set_level(char *log_level)
{
    if (strcasecmp(log_level, LOG_DEBUG_STR) == 0) {
        srv_log_level = LOG_DEBUG;
    } else if (strcasecmp(log_level, LOG_INFO_STR) == 0) {
        srv_log_level = LOG_INFO;
    } else if (strcasecmp(log_level, LOG_WARN_STR) == 0) {
        srv_log_level = LOG_WARN;
    } else if (strcasecmp(log_level, LOG_ERROR_STR) == 0) {
        srv_log_level = LOG_ERROR;
    } else {
        /* invalid log level, do nothing */
    }
}

void log_set_fp(FILE *fp)
{
    srv_log_fp = fp;
}

void srv_log(int level, const char *fmt, ...)
{
    if (level < srv_log_level) {
        return;
    }

    char *level_str = NULL;
    switch (level) {
        case LOG_DEBUG:
            level_str = LOG_DEBUG_STR;
            break;
        case LOG_INFO:
            level_str = LOG_INFO_STR;
            break;
        case LOG_WARN:
            level_str = LOG_WARN_STR;
            break;
        case LOG_ERROR:
            level_str = LOG_ERROR_STR;
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }

    char msg[SIZE1024];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    size_t offset = strftime(msg, sizeof(msg), "%F %T", localtime(&tv.tv_sec));
    offset += snprintf(msg + offset, sizeof(msg), ".%03.f ", (float)tv.tv_usec/1000);
    offset += snprintf(msg + offset, sizeof(msg) - offset, "[%s] ", level_str);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg + offset, sizeof(msg) - offset, fmt, ap);
    va_end(ap);

    FILE *fp = (srv_log_fp == NULL) ? stderr : srv_log_fp;
    fprintf(fp, "%s\n", msg);
    fflush(fp);
}

