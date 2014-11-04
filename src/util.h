#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>

#include "constant.h"

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

char *create_string_obj(char *ptr, size_t len);
int string2ll(const char *s, size_t slen, long long *value);
void get_time_millisec(INT64 *assigned_timestamp);
void get_time_sec(int *assigned_timestamp);
void create_objectid(char *oid, int seq);
INT64 rand_int64(INT64 n);

#endif
