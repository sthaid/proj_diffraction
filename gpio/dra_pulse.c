#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <sys/time.h>

#include <dra_gpio.h>
#include <util_misc.h>

// XXX TODO
// - real time ?

// defines
#define GPIO_PIN  26

// variables
volatile bool done;

// prototypes
void get_pulse_rate(unsigned int intvl_us, double *gpio_read_rate, double *pulse_rate);
void sigalrm_handler(int signum);
char * stars(double value, int stars_max, double value_max);

// -----------------  MAIN  --------------------------------------------

int main(int argc, char **argv)
{
    struct sigaction act;
    double gpio_read_rate, pulse_rate;

    // init dra_gpio capability
    if (gpio_init() != 0) {
        return 1;
    }

    // register for SIGALRM
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &act, NULL);

    // loop forever
    while (true) {
        // get pulse rate, and print results
        get_pulse_rate(1000000, &gpio_read_rate, &pulse_rate);
#if 0
        printf("gpio_read_rate = %.3f  pulse_rate = %.6f   (units million/sec)\n",
               gpio_read_rate/1000000, pulse_rate/1000000);
#else
        printf("%6.3f - %s\n",
              pulse_rate/1000000, stars(pulse_rate/1000000, 160, 1.0));
#endif

        // short delay
        usleep(500000);
    }

    // done
    return 0;
}

// -----------------  GET PULSE RATE -----------------------------------

void get_pulse_rate(unsigned int intvl_us, double *gpio_read_rate, double *pulse_rate)
{
    struct itimerval new_value, old_value;
    uint64_t start_us, end_us;
    double intvl_secs;
    int v, v_last=0, pulse_cnt=0, read_cnt=0;
    
    // set interval timer, which will set the done flag
    memset(&new_value, 0, sizeof(new_value));
    new_value.it_value.tv_sec  = intvl_us / 1000000;
    new_value.it_value.tv_usec = intvl_us % 1000000;
    setitimer(ITIMER_REAL, &new_value, &old_value);

    // loop until done
    start_us = microsec_timer();
    while (!done) {
        // read gpio input
        v = gpio_read(GPIO_PIN);
        read_cnt++;

        // if detected rising edge then increment pulse_cnt
        if (v != 0 && v_last == 0) {
            pulse_cnt++;
        }
        v_last = v;
    }
    end_us = microsec_timer();
    done = false;

    // calculate return rates
    intvl_secs = (end_us - start_us) / 1000000.;
    *gpio_read_rate = read_cnt / intvl_secs;
    *pulse_rate = pulse_cnt / intvl_secs;
}

void sigalrm_handler(int signum)
{
    done = true;
}

char * stars(double value, int stars_max, double value_max)
{
    static char stars[1000];
    int len;
    bool overflow = false;

    len = nearbyint((value / value_max) * stars_max);
    if (len > stars_max) {
        len = stars_max;
        overflow = true;
    }

    memset(stars, '*', len);
    stars[len] = '\0';
    if (overflow) {
        strcpy(stars+len, "...");
    }

    return stars;
}

