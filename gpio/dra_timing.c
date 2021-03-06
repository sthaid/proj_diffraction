#include <stdio.h>
#include <dra_gpio.h>
#include <util_misc.h>

#define MAX_CNT 10000000  // 10 million

#define GPIO_INPUT_PIN       20
#define GPIO_OUTPUT_LED_PIN  21

int main(int argc, char **argv)
{
    unsigned long cnt, cnt_v_set;
    unsigned long start_us, end_us;
    double duration_secs;
    int v;

    if (gpio_init() != 0) {
        return 1;
    }
    set_gpio_pin_mode(GPIO_INPUT_PIN, PIN_MODE_INPUT);

    cnt_v_set = 0;
    start_us = microsec_timer();
    for (cnt = 0; cnt < MAX_CNT; cnt++) {
        v = gpio_read(25);
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

