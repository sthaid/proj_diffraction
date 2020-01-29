// to build:
//   gcc -o rate -Wall -O2 rate.c -lm

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

int events_in_this_interval(double lambda);

// ----------------------------------------------------------------------

int main(int argc, char **argv)
{
    double true_rate;
    double interval_duration;
    double lambda;
    int intervals_per_sec;

    long n, total_events, total_intervals;

    true_rate = 20000;                           // events/sec
    interval_duration = 1 / true_rate / 1000;    // seconds
    lambda = true_rate * interval_duration;
    intervals_per_sec = 1 / interval_duration;

    printf("true_rate          = %.0f events/sec\n", true_rate);
    printf("interval_duration  = %.3f microsecs\n", interval_duration*1e6);
    printf("lambda             = %.6f events\n", lambda);
    printf("intervals_per_sec  = %d\n", intervals_per_sec);
    printf("\n");

    total_events = 0;
    total_intervals = 0;

    while (true) {
        n = events_in_this_interval(lambda);

        total_events += n;
        total_intervals ++;

        if ((total_intervals % (intervals_per_sec/10)) == 0) {
            double total_time,observed_rate;

            total_time = total_intervals * interval_duration;
            observed_rate = total_events / total_time;
            printf("time = %6.3f   observed_rate = %.3f   percent_deviation = %.3f\n",   
                   total_time, 
                   observed_rate,
                   fabs(observed_rate - true_rate) / true_rate * 100);
        }
    }

    return 0;
}

int events_in_this_interval(double lambda)
{
    int k;
    double p, x;
    int factorial[] = { 1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, };
    
    // probability of k events in this interval
    //     pow(lambda, k) * exp(-lambda) / factorial(k)

    x = drand48();
    p = 0;

    for (k = 0; k < 5; k++) {
        p += pow(lambda,k) * exp(-lambda) / factorial[k];
        if (x < p) {
            return k;
        }
    }

    printf("OOPS\n");
    return k;
}

