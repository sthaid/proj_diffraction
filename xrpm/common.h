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
#include <math.h>
#include <inttypes.h>
#include <sys/time.h>

// for calls to time2str
#define MAX_TIME_STR 50

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
void utils_init(void);
void utils_exit(void);
void logmsg(char * lvl, const char * func, char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
uint64_t microsec_timer(void);
uint64_t get_real_time_us(void);
char * time2str(char * str, int64_t us, bool gmt, bool display_ms, bool display_date);
void my_usleep(uint64_t us);

// audio.c
void audio_init(void);
void audio_exit(void);
void audio_say_text(char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
int audio_change_volume(int delta);
void audio_set_volume(int volume);
int audio_get_volume(void);
void audio_set_mute(bool mute);
bool audio_get_mute(void);

// sipm.c
void sipm_init(void);
void sipm_exit(void);
void sipm_get_rate(int *pulse_rate, int *gpio_read_rate, int *gpio_read_and_analyze_rate);

// xrail.c
void xrail_init(void);
void xrail_exit(void);
void xrail_cal_move(double mm);
void xrail_cal_complete(void);
void xrail_goto_location(double mm, bool wait);
bool xrail_reached_location(void);
void xrail_get_status(bool *okay, bool *calibrated, double *curr_loc_mm, double *tgt_loc_mm, double *voltage, char *status_str);

#endif
