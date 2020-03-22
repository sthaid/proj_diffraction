#include "common.h"

char *filename = "ifsim.config";

int main(int argc, char **argv)
{
    // filename is optional arg
    if (argc > 1) {
        filename = argv[1];
    }

    // initialize
    if (sim_init(filename) < 0) {
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
