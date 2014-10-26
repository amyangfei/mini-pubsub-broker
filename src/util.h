#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>

#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERROR 3

#define LOG_DEBUG_STR "DEBUG"
#define LOG_INFO_STR "INFO"
#define LOG_WARN_STR "WARN"
#define LOG_ERROR_STR "ERROR"

void log_set_fp(FILE *fp);
void log_set_level(char* level);
void srv_log(int level, const char *fmt, ...);

#endif
