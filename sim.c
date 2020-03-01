#include "common.h"

static int read_config_file(void);

// ----------------------------------------------------------------------------------

int sim_init(void)
{
    if (read_config_file() < 0) {
        return -1;
    }

    return 0;
}

void sim_select_config(int config_idx)
{
}

void sim_reset(void)
{
}

void sim_run(void)
{
}

void sim_stop(void)
{
}

bool sim_is_running(void)
{
    return false;
}

// ----------------------------------------------------------------------------------

static int read_config_file(void)
{
#if 0
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, mirror,             500,0,0,     -1,1,0,    2);
    INIT_ELEM(2, screen,             500,500,0,   0,-1,0,    -1);
#endif
#if 0
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, screen,             2000,0,0,    -1,0,0,    -1);
#endif

    return 0;
}
