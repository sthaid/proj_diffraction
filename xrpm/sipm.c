#include "common_includes.h"

#include "sipm.h"
#include "utils.h"
#include "gpio.h"

//
// defines
//

#define GPIO_INPUT_PIN        20

#define GPIO_READ_INTVL_US    100000    // 100ms
#define MAX_GPIO_DATA_BUFFER  10000000  // 10 million, 320 million samples
#define MAX_SIPM              20        // 2 secs of history in 100 ms chunks

// define this to replace real sipm pulse_rate data with sine curve
#define UNITTEST_SIPM_GET_RATE

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

static sipm_t            sipm[MAX_SIPM];
static int               sipm_count;
static uint64_t          sipm_get_rate_called_at_us;

static unsigned int      gpio_data_buffer[MAX_GPIO_DATA_BUFFER];

static pthread_mutex_t   mutex = PTHREAD_MUTEX_INITIALIZER;

static volatile bool     read_sipm_done;
static pthread_barrier_t read_sipm_set_done_barrier;

//
// prototypes
//

static void * sipm_thread(void *cs);
static void read_sipm(sipm_t *x);
static void analyze_sipm(sipm_t *x);
static void * read_sipm_set_done_thread(void *cs);
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

    // pthread initializations
    pthread_barrier_init(&read_sipm_set_done_barrier, NULL, 2);
    pthread_create(&tid, NULL, sipm_thread, NULL);
    pthread_create(&tid, NULL, read_sipm_set_done_thread, NULL);
}

void sipm_exit(void)
{
    // nothing needed
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

    // keep track of when this call is made
    sipm_get_rate_called_at_us = microsec_timer();

    // if sipm_count < MAX_SIPM then wait
    while (sipm_count < MAX_SIPM) {
        sipm_get_rate_called_at_us = microsec_timer();
        usleep(1000);
    }

    // acquire mutex so that sipm_count will not change 
    pthread_mutex_lock(&mutex);

    // starting at sipm_count-1, scan sipm entries until 
    // encounter an entry whose start time is earlier than 1 second ago
    one_sec_ago_us = microsec_timer() - 1000000;
    pulse_count = 0;
    gpio_read_count = 0;
    gpio_read_duration_us = 0;
    for (i = sipm_count-1; i >= sipm_count-MAX_SIPM; i--) {
        sipm_t *x = &sipm[i % MAX_SIPM];

        if (x->start_us < one_sec_ago_us) {
            break;
        }

        analyze_start_us = x->start_us;
        if (i == sipm_count-1) {
            analyze_end_us = x->end_us;
        }

        pulse_count           += x->pulse_count;
        gpio_read_count       += (x->max_data * 32);
        gpio_read_duration_us += (x->end_us - x->start_us);
    }

    // convert durations to seconds (double)
    gpio_read_duration_secs = (double)gpio_read_duration_us / 1000000;
    analyze_duration_secs   = (double)(analyze_end_us - analyze_start_us) / 1000000;
    INFO("xxx gpio_read_duration_secs = %0.1f  analyze_duration_secs = %0.1f\n", 
         gpio_read_duration_secs, analyze_duration_secs);

    // avoid a possible divide by 0
    if (gpio_read_duration_secs == 0 || analyze_duration_secs == 0) {
        ERROR("gpio_read_duration_secs = %0.1f  analyze_duration_secs = %0.1f\n",
              gpio_read_duration_secs, analyze_duration_secs);
        *pulse_rate                 = 0;
        *gpio_read_rate             = 0;
        *gpio_read_and_analyze_rate = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }

    // calculate return values
    // - pulse_rate
    // - gpio_read_rate
    // - gpio_read_and_analyze_rate
#ifdef UNITTEST_SIPM_GET_RATE
    double x = microsec_timer() * ((2 * M_PI) / 60000000);
    *pulse_rate                 = 25000 + 5000 * sin(x);
#else
    *pulse_rate                 = pulse_count / gpio_read_duration_secs;
#endif
    *gpio_read_rate             = gpio_read_count / gpio_read_duration_secs;
    *gpio_read_and_analyze_rate = gpio_read_count / analyze_duration_secs;

    // release mutex
    pthread_mutex_unlock(&mutex);
}

// -----------------  GPIO READ THREAD  -----------------------------------

static void * sipm_thread(void *cs)
{
    int rc;
    struct sched_param sched_param;

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
        sipm_t *x;

        // if sipm_get_rate has not been called recently then
        // wait here until it is being called 
        if ((microsec_timer() - sipm_get_rate_called_at_us > 10000000) ||
            (sipm_get_rate_called_at_us == 0))
        {
            INFO("client is not active - stop collecting sipm values\n");
            do {
                pthread_mutex_lock(&mutex);
                sipm_count = 0;
                pthread_mutex_unlock(&mutex);
                usleep(100000);
            } while ((microsec_timer() - sipm_get_rate_called_at_us > 10000000) ||
                     (sipm_get_rate_called_at_us == 0));
            INFO("client is active - start collecting sipm values\n");
        }

        // init ptr to the sipm struct to be used by the code below
        x = &sipm[sipm_count % MAX_SIPM];

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
        usleep(5000);
    }

    return NULL;
}

static void read_sipm(sipm_t *x)
{
    int max_data;

    // clear the read_sipm_done flag, and set the barrier;
    // this will cause the read_sipm_done flag to be set to true in 100 ms
    read_sipm_done = false;
    pthread_barrier_wait(&read_sipm_set_done_barrier);

    // loop while read_sipm_done is false
    max_data = 0;
    x->start_us = microsec_timer();
    while (!read_sipm_done) {
        int i;
        unsigned int v32;

        for (v32 = 0, i = 0; i < 32; i++) {
            v32 <<= 1;
            v32 |= gpio_read(GPIO_INPUT_PIN);
        }

        if (max_data < MAX_GPIO_DATA_BUFFER) {
            gpio_data_buffer[max_data++] = v32;
        }
    }
    x->end_us = microsec_timer();
    x->max_data = max_data;
}

static void analyze_sipm(sipm_t *x)
{
    int cnt0, cnt1, i;
    int sum_cnt0=0, sum_cnt1=0;
    int pulse_count=0;

    // this routine uses the count_consecutive routines to 
    // determine the number of consecutive zeros and ones
    count_consecutive_reset(x);

    // first count consectuve zeros, this will leave 
    // count_consecutive pointing to the first one; 
    // i.e. the begining of a pulse
    sum_cnt0 += count_consecutive_zero();

    while (true) {
        // A pulse is ideally a sequence of 1s terminated by a 0.
        //         
        // However, if there are few consecutive 0s following the consecutive 1s then 
        //  these 0s are not considered the end of the pulse, instead they are considered
        //  to be part of the pulse. This code allows for up to 4 short groups of consecutive
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

        // for debug, keep track of the total number of 0s and 1s by summing
        // the number of 0s and 1s in the pulse just found above
        sum_cnt0 += cnt0;
        sum_cnt1 += cnt1;

        // if we're at the end of the data then break out of this loop
        if (count_consecutive_is_at_eod()) {
            break;
        }

        // increment the number of pulses located
        pulse_count++;
    }

    // debug check that all the 1s and 0s counted agrees with expected
    if (sum_cnt0 + sum_cnt1 != x->max_data * 32) {
        FATAL("BUG sum_cnt0=%d sum_cnt1=%d gpio.max_data=%d\n",
               sum_cnt0, sum_cnt1, x->max_data*32);
    }

    // return the pulse_count in ths sipm_t arg
    x->pulse_count = pulse_count;
}

static void *read_sipm_set_done_thread(void *cx)
{
    while (true) {
        pthread_barrier_wait(&read_sipm_set_done_barrier);
        usleep(100000);
        read_sipm_done = true;
    }

    return NULL;
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
