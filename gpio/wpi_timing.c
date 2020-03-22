#include <stdio.h>
#include <wiringPi.h>
#include <util_misc.h>

#define MAX_CNT 10000000  // 10 million

int main (void)
{
    unsigned long cnt, cnt_v_set;
    unsigned long start_us, end_us;
    double duration_secs;
    int v;

    if (wiringPiSetupGpio () == -1)
        return 1;

    pinMode (25, INPUT);

    cnt_v_set = 0;
    start_us = microsec_timer();
    for (cnt = 0; cnt < MAX_CNT; cnt++) {
        v = digitalRead(25);
        if (v) {
            cnt_v_set++;
        }
    }
    end_us = microsec_timer();
    duration_secs = (end_us - start_us) / 1000000.;

    printf("million_cnt/sec = %.2f\n", (double)cnt/1000000/duration_secs);
    printf("cnt=%ld  cnt_v_set=%ld\n", cnt, cnt_v_set);

    return 0;
}
