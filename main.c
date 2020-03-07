#include "common.h"

int main(int argc, char **argv)
{
    // initialize
    if (sim_init("ifsim.config") < 0) {
        return 1;
    }
    if (display_init() < 0) {
        return 1;
    }

    // XXX should be in display
    sim_select_config(0);
    sim_run();

    // runtime
    display_hndlr();

    // done
    return 0;
}
