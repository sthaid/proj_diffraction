#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include "sipm.h"
#include "utils.h"
#include "gpio.h"

//
// defines
//

#define GPIO_INPUT_PIN        20

#define GPIO_READ_INTVL_US    100000    // 100ms
#define MAX_GPIO_DATA_BUFFER  10000000  // 10 million
#define MAX_SIPM              20        // 2 secs of history in 100 ms chunks

//
// typedes
//

typedef struct {
    int      max_data;
    uint64_t start_us;
    uint64_t end_us;
    int      pulse_count;
} sipm_t;

//
// variables
//

sipm_t        sipm[MAX_SIPM];
int           sipm_count;
volatile bool sigalrm_rcvd;

unsigned int gpio_data_buffer[MAX_GPIO_DATA_BUFFER];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//
// prototypes
//

static void sigalrm_handler(int signum);
static void * sipm_thread(void *cs);
static void read_sipm(sipm_t *x);
static void analyze_sipm(sipm_t *x);
static void count_consecutive_reset(sipm_t *x);
static bool count_consecutive_is_at_eod(void);
static int count_consecutive_zero(void);
static int count_consecutive_one(void);

// -----------------  INIT / EXIT  ----------------------------------------

void sipm_init(void)
{
    int rc;
    pthread_t tid;

    // init dra_gpio capability
    rc = gpio_init();
    if (rc != 0) {
        FATAL("gpio_init failed\n");
    }
    set_gpio_pin_mode(GPIO_INPUT_PIN, PIN_MODE_INPUT);

    // create sipm_thread
    pthread_create(&tid, NULL, sipm_thread, NULL);
}

void sipm_exit(void)
{
    // nothing needed
}

static void sigalrm_handler(int signum)
{
    sigalrm_rcvd = true;
}

// -----------------  APIS  -----------------------------------------------

void sipm_get_rate(int *pulse_rate, int *gpio_read_rate, int *gpio_read_and_analyze_rate)
{
    uint64_t one_sec_ago_us;
    uint64_t analyze_start_us = 0;
    uint64_t analyze_end_us = 0;
    int      pulse_count;
    int      gpio_read_count;
    int      gpio_read_duration_us;
    int      i;
    double   gpio_read_duration_secs;
    double   analyze_duration_secs;

    // if sipm_count < MAX_SIPM then wait
    while (sipm_count < MAX_SIPM) {
        my_usleep(1000);
    }

    // acquire mutex so that sipm_count will not change 
    pthread_mutex_lock(&mutex);

    // starting at sipm_count-1, scan sipm entries until 
    // encounter an entry whose start time is earlier than 
    // 1 second ago
    one_sec_ago_us = microsec_timer() - 1000000;
    pulse_count = 0;
    gpio_read_count = 0;
    gpio_read_duration_us = 0;
    for (i = sipm_count-1; i >= sipm_count-MAX_SIPM; i--) {
        sipm_t *x = &sipm[i % MAX_SIPM];

        pulse_count           += x->pulse_count;
        gpio_read_count       += (x->max_data * 32);
        gpio_read_duration_us += (x->end_us - x->start_us);

        if (i == sipm_count-1) {
            analyze_end_us = x->end_us;
        }
        if (x->start_us < one_sec_ago_us) {
            analyze_start_us = x->start_us;
            break;
        }
    }

    // the above code should always set analyze_start_us;
    // if not it is a fatal error
    if (analyze_start_us == 0 || analyze_end_us == 0) {
        FATAL("analyze_start/end_us equals zero\n");
    }

    // convert durations to seconds (double)
    gpio_read_duration_secs = (double)gpio_read_duration_us / 1000000;
    analyze_duration_secs      = (double)(analyze_end_us - analyze_start_us) / 1000000;

    // calculate return values
    // - pulse_rate
    // - gpio_read_rate
    // - gpio_read_and_analyze_rate
    *pulse_rate = pulse_count / gpio_read_duration_secs;
    *gpio_read_rate = gpio_read_count / gpio_read_duration_secs;
    *gpio_read_and_analyze_rate = gpio_read_count / analyze_duration_secs;

    // release mutex
    pthread_mutex_unlock(&mutex);
}

// -----------------  GPIO READ THREAD  -----------------------------------

static void * sipm_thread(void *cs)
{
    int rc;
    struct sched_param sched_param;
    struct sigaction act;

    // register for SIGALRM  XXX dont use this
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &act, NULL);

    // set realtime prio
    // note: to verify rtprio, use:
    //   ps -eLo rtprio,comm | grep xrpm
    sched_param.sched_priority = 50;
    rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sched_param);
    if (rc != 0) {
        ERROR("pthread_setschedparam, %s\n", strerror(rc));
    }

    // loop forever
    while (true) {
        sipm_t *x = &sipm[sipm_count % MAX_SIPM];

        // read sipm gpio bit for 100 ms;
        // this sets the following fields in sipm_t
        // - max_data
        // - start_us
        // - end_us
        read_sipm(x);

        // analyze the sipm gpio data determine the number of pulses;
        // this sets the following fields in sipm_t
        // - pulse_count
        analyze_sipm(x);

        // update sipm_count
        pthread_mutex_lock(&mutex);
        sipm_count++;
        pthread_mutex_unlock(&mutex);

        // 5 ms sleep because this thread is running at realtime prio
        my_usleep(5000);
    }

    return NULL;
}

static void read_sipm(sipm_t *x)
{
    struct itimerval new_value, old_value;
    int max_data;

    // set interval timer, which will set the sigalrm_rcvd flag after 100 ms interval
    memset(&new_value, 0, sizeof(new_value));
    new_value.it_value.tv_sec  = GPIO_READ_INTVL_US / 1000000;
    new_value.it_value.tv_usec = GPIO_READ_INTVL_US % 1000000;
    setitimer(ITIMER_REAL, &new_value, &old_value);

    // loop until sigalrm_rcvd
    max_data = 0;
    x->start_us = microsec_timer();
    while (!sigalrm_rcvd) {
        int i;
        unsigned int v32;

        for (v32 = 0, i = 0; i < 32; i++) {
            v32 <<= 1;
            v32 |= gpio_read(GPIO_INPUT_PIN);
        }

        gpio_data_buffer[max_data++] = v32;
    }
    x->end_us = microsec_timer();
    x->max_data = max_data;

    // clear sigalrm_rcvd
    sigalrm_rcvd = false;
}

static void analyze_sipm(sipm_t *x)
{
    int cnt0, cnt1, i;
    int sum_cnt0=0, sum_cnt1=0;
    int pulse_count=0;

    count_consecutive_reset(x);
    sum_cnt0 += count_consecutive_zero();

    while (true) {
        // XXX more comments in here
        // First look for consecutive 1s followed by consecutive 0s.
        // If there are few consecutive 0s following the consecutive 1s then 
        //  these 0s are not considered the end of the pulse, instead they are considered
        //  to be part of the pulse. We allow for up to 4 short groups of consecutive
        //  0s to be considered part of the pulse.
        cnt1 = count_consecutive_one();
        cnt0 = count_consecutive_zero(); 
        for (i = 0; i < 4; i++) {
            if (cnt0 >= 8) {
                break;
            }
            cnt1 += cnt0;
            cnt1 += count_consecutive_one();
            cnt0 = count_consecutive_zero();
        }

        sum_cnt0 += cnt0;
        sum_cnt1 += cnt1;

        if (count_consecutive_is_at_eod()) {
            break;
        }

        pulse_count++;
    }

    if (sum_cnt0 + sum_cnt1 != x->max_data * 32) {
        FATAL("BUG sum_cnt0=%d sum_cnt1=%d gpio.max_data=%d\n",
               sum_cnt0, sum_cnt1, x->max_data*32);
    }

    x->pulse_count = pulse_count;
}

// -----------------  COUNT CONSECUTICE BITS OF GPIO.DATA  -------------

#define GET_BIT_AT_CCIDX \
    ((gpio_data_buffer[ccidx>>5] & (1 << (31 - (ccidx & 0x1f)))) != 0)

static int ccidx;
static int max_ccidx;

static void count_consecutive_reset(sipm_t *x)
{
    ccidx     = 0;
    max_ccidx = x->max_data * 32;
}

static bool count_consecutive_is_at_eod(void)
{
    return ccidx >= max_ccidx;
}

static int count_consecutive_zero(void)
{
    int cnt=0;

    while (true) {
        if (ccidx >= max_ccidx) {
            return cnt;
        }

        if ((ccidx & 31) == 0) {
            if (gpio_data_buffer[ccidx>>5] == 0) {
                cnt += 32;
                ccidx += 32;
                continue;
            }
        }

        if (GET_BIT_AT_CCIDX == 0) {
            cnt++;
            ccidx++;
        } else {
            return cnt;
        }
    }
}

static int count_consecutive_one(void)
{
    int cnt=0;

    while (true) {
        if (ccidx >= max_ccidx) {
            return cnt;
        }

        if (GET_BIT_AT_CCIDX == 1) {
            cnt++;
            ccidx++;
        } else {
            return cnt;
        }
    }
}
