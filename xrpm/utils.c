#include "common.h"

void logmsg(char *lvl, const char *func, char *fmt, ...)
{
    va_list ap;
    char    msg[1000];
    int     len;

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

    // log to stderr 
    fprintf(stderr, "%s: %s\n", lvl, msg);
}

uint64_t microsec_timer(void)
{   
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

