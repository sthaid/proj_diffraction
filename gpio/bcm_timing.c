#include <stdio.h>
#include <bcm2835.h>
#include <util_misc.h>

#define MAX_CNT 10000000  // 10 million

int main(int argc, char **argv)
{
    unsigned long cnt, cnt_v_set;
    unsigned long start_us, end_us;
    double duration_secs;
    int v;

    if (!bcm2835_init())
	return 1;

    bcm2835_gpio_fsel(25, BCM2835_GPIO_FSEL_INPT);

    cnt_v_set = 0;
    start_us = microsec_timer();
    for (cnt = 0; cnt < MAX_CNT; cnt++) {
        v = bcm2835_gpio_lev(25);
        if (v == HIGH) {
            cnt_v_set++;
        }
    }
    end_us = microsec_timer();
    duration_secs = (end_us - start_us) / 1000000.;

    printf("million_cnt/sec = %.2f\n", (double)cnt/1000000/duration_secs);
    printf("cnt=%ld  cnt_v_set=%ld\n", cnt, cnt_v_set);

    return 0;
}
