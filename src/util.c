#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>

#include "util.h"
#include "constant.h"
#include "broker.h"

#define MIN_PROB (1.0 / ((INT64)RAND_MAX + 1))

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

char *create_string_obj(char *ptr, size_t len)
{
    char *strobj = (char *) malloc(len + 1);
    memcpy(strobj, ptr, len);
    strobj[len] = '\0';

    return strobj;
}

/* Convert a string into a long long. Returns 1 if the string could be parsed
 * into a (non-overflowing) long long, 0 otherwise. The value will be set to
 * the parsed value when appropriate. */
int string2ll(const char *s, size_t slen, long long *value) {
    const char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    if (plen == slen)
        return 0;

    /* Special case: first and only digit is 0. */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        /* Abort on only a negative sign. */
        if (plen == slen)
            return 0;
    }

    /* First digit should be 1-9, otherwise the string should just be 0. */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return 1;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) /* Overflow. */
            return 0;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0]-'0'))) /* Overflow. */
            return 0;
        v += p[0]-'0';

        p++; plen++;
    }

    /* Return if not all bytes were used. */
    if (plen < slen)
        return 0;

    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN+1))+1)) /* Overflow. */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* Overflow. */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}

/* Get current Unix timestamp in milliseconds */
void get_time_millisec(INT64 *assigned_timestamp)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *assigned_timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Get current Unix timestamp in seconds */
void get_time_sec(int *assigned_timestamp)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *assigned_timestamp = tv.tv_sec % INT32_MAX;
}

/* Make sure oid has alloced 24 sizeof char at least */
void create_objectid(char *oid, int seq)
{
    int cur_ts, pid;

    get_time_sec(&cur_ts);
    sprintf(oid, "%.8x", cur_ts);

    sprintf(oid+8, "%.6x", 0xffffff);

    pid = getpid() % INT16_MAX;
    sprintf(oid+14, "%.4x", pid);

    if (seq < 0 || seq >= 1 << 24) {
        seq = 0;
    }
    sprintf(oid+18, "%.6x", seq);
}

static int happened(double probability) //probability 0~1
{
    long rmax = (long) RAND_MAX;
    if(probability <= 0)
        return 0;
    if(probability < MIN_PROB)
        return rand() == 0 && happened(probability*(rmax+1));
    if(rand() <= probability * (rmax+1))
        return 1;
    return 0;
}

/* generate a random number between 0 and n-1, including n-1 */
INT64 rand_int64(INT64 n)
{
    INT64 r, t;
    INT64 rmax = (INT64) RAND_MAX;
    if(n <= rmax)
    {
        r = rmax - (rmax+1)%n; // tail
        t = rand();
        while(t > r)
            t = rand();
        return t % n;
    }
    else
    {
        r = n % (rmax+1);   // reminder
        if(happened((double)r/n))   // probability of get the reminder
            return n-r+rand_int64(r);
        else
            return rand()+rand_int64(n/(rmax+1)) * (rmax+1);
    }
}
