#include "common.h"

FILE *logfp;

// -----------------  INIT & EXIT  --------------------------------------

void utils_init(void)
{
    logfp = fopen("xrpm.log", "a");
    if (logfp == NULL) {
        fprintf(stderr, "failed to open xrpm.log, %s\n", strerror(errno));
        exit(1);
    }

    setlinebuf(logfp);

    fprintf(logfp, "\n\n");
    INFO("STARTING\n");
}

void utils_exit(void)
{
    INFO("EXITTING\n");
}

// -----------------  LOGGING  ------------------------------------------

void logmsg(char *lvl, const char *func, char *fmt, ...)
{
    va_list ap;
    char    msg[1000];
    int     len;
    char    time_str[MAX_TIME_STR];
    bool    print_prefix;

    // if fmt begins with '#' then do not print the prefix
    print_prefix = (fmt[0] != '#');
    if (print_prefix == false) {
        fmt++;
    }

    // construct msg
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    // remove terminating newline
    len = strlen(msg);
    if (len > 0 && msg[len-1] == '\n') {
        msg[len-1] = '\0';
        len--;
    }

    // print to logfp or stderr
    if (print_prefix) {
        fprintf(logfp ? logfp : stderr, "%s %s %s: %s\n",
            time2str(time_str, get_real_time_us(), false, true, true),
            lvl, func, msg);
    } else {
        fprintf(logfp ? logfp : stderr, "%s\n", msg);
    }
}

// -----------------  TIME  ---------------------------------------------

uint64_t microsec_timer(void)
{   
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

uint64_t get_real_time_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME,&ts);
    return ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

char * time2str(char * str, int64_t us, bool gmt, bool display_ms, bool display_date)
{
    struct tm tm;
    time_t secs;
    int32_t cnt;
    char * s = str;

    secs = us / 1000000;

    if (gmt) {
        gmtime_r(&secs, &tm);
    } else {
        localtime_r(&secs, &tm);
    }

    if (display_date) {
        cnt = sprintf(s, "%2.2d/%2.2d/%2.2d ",
                         tm.tm_mon+1, tm.tm_mday, tm.tm_year%100);
        s += cnt;
    }

    cnt = sprintf(s, "%2.2d:%2.2d:%2.2d",
                     tm.tm_hour, tm.tm_min, tm.tm_sec);
    s += cnt;

    if (display_ms) {
        cnt = sprintf(s, ".%3.3"PRId64, (us % 1000000) / 1000);
        s += cnt;
    }

    if (gmt) {
        strcpy(s, " GMT");
    }

    return str;
}

