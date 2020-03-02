#include "common.h"

void test(void);

int main(int argc, char **argv)
{
    // initialize
    if (sim_init() < 0) {
        return 1;
    }


    test();
    return 0;


    if (display_init() < 0) {
        return 1;
    }

    // runtime
    display_hndlr();

    // done
    return 0;
}

void test(void)
{
    double *screen;
    int max_wh;
    double wh_mm;

    sim_run();
    sleep(5);

    sim_get_screen(10, &screen, &max_wh, &wh_mm);
    INFO("screen=%p, max_wh=%d wh_mm=%g\n", screen, max_wh, wh_mm);
    free(screen);
}
