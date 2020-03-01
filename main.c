#include "common.h"

int main(int argc, char **argv)
{
    // initialize
    if (sim_init() < 0) {
        return 1;
    }
    if (display_init() < 0) {
        return 1;
    }

    // runtime
    display_hndlr();

    // done
    return 0;
}
