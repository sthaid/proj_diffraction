#include "common.h"

static char *filename = "ifsim.config";
static bool swap_white_black = true;

int main(int argc, char **argv)
{
    // get and process options
    while (true) {
        char opt_char = getopt(argc, argv, "x");
        if (opt_char == -1) {
            break;
        }
        switch (opt_char) {
        case 'x':
            swap_white_black = !swap_white_black;
            break;
        default:
            return 1;
            break;
        }
    }

    // filename is optional arg
    if (argc - optind >= 1) {
         filename = argv[optind];
    }

    // initialize simulation and display
    if (sim_init(filename) < 0) {
        return 1;
    }
    if (display_init(swap_white_black) < 0) {
        return 1;
    }

    // should be in display
    sim_select_config(0);
    sim_run();

    // runtime
    display_hndlr();

    // done
    return 0;
}
