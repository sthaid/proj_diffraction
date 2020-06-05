#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

// logging macros
#define INFO(fmt, args...) \
    do { \
        logmsg("INFO", __func__, fmt, ## args); \
    } while (0)
#define ERROR(fmt, args...) \
    do { \
        logmsg("ERROR", __func__, fmt, ## args); \
    } while (0)
#define FATAL(fmt, args...) \
    do { \
        logmsg("FATAL", __func__, fmt, ## args); \
        exit(1); \
    } while (0)

// utils.c
void logmsg(char * lvl, const char * func, char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
uint64_t microsec_timer(void);

// audio.c
void audio_init(void);
void audio_exit(void);
void audio_say_text(char *text);
void audio_set_volume(int volume);
int audio_get_volume(void);
void audio_set_mute(bool mute);
bool audio_get_mute(void);

// sipm.c
void sipm_init(void);
void sipm_exit(void);
void sipm_get_rate(int *pulse_rate, int *gpio_read_rate, int *gpio_read_and_analyze_rate);

#endif
