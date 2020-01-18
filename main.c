#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "common.h"

//
// defines
//

//
// typedefs
//

//
// variables
//

char *config_filename;

//
// prototypes
//

int read_config(void);

// -----------------  MAIN  -----------------------------------------------------

int main(int argc, char **argv)
{
    int rc;

    // XXX todo arg
    config_filename = "config";

    rc = read_config();
    if (rc != 0) {
        ERROR("read_config failed, %s\n", strerror(-rc));
        return -1;
    }

    display_handler();

    return 0;
}

// -----------------  READ_CONFIG  ----------------------------------------------

int read_config(void)
{
    FILE *fp;
    char s[500];
    params_t *p;
    int cnt, i, line=0;

    fp = fopen(config_filename, "r");
    if (fp == NULL) {
        return -ENOENT;
    }

    while (fgets(s, sizeof(s), fp) != NULL) {
        line++;

        if (s[0] == '#') {
            continue;
        }
        if (strspn(s, " \r\n\t") == strlen(s)) {
            continue;  // s is all whitespace
        }

        p = &params[max_params];
        cnt = sscanf("%s %f %f %f %f %f %f %f %f %f %f",
                     p->name,
                     &p->distance_to_screen,
                     &p->wavelength,
                     &p->slit[0].start, &p->slit[0].end,
                     &p->slit[0].start, &p->slit[0].end,
                     &p->slit[0].start, &p->slit[0].end,
                     &p->slit[0].start, &p->slit[0].end);
        if (cnt != 5 && cnt != 7 && cnt != 9 && cnt != 11) {
            ERROR("config file '%s' error on line %d\n",
                  config_filename, line);
            return -EINVAL;
        }

        p->max_slit = (cnt - 3) / 2;
        p->wavelength *= 1e-9;
        for (i = 0; i < p->max_slit; i++) {
            p->slit[i].start *= 1e-3;
            p->slit[i].end   *= 1e-3;
        }

        max_params++;
// XXX check MAX_PARAMS
    }

    fclose(fp);

    return 0;
}
