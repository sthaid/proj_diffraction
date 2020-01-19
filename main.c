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

int read_params(void);

// -----------------  MAIN  -----------------------------------------------------

int main(int argc, char **argv)
{
    int rc;

    // XXX todo arg
    // get config_filename arg
    config_filename = "config";

    // read program parameters
    rc = read_params();
    if (rc != 0) {
        ERROR("read_params failed, %s\n", strerror(-rc));
        return -1;
    }

    // calc XXX
    calculate_screen_image(&params[0]);

    // run time 
    display_handler();

    // done
    return 0;
}

// -----------------  READ_CONFIG  ----------------------------------------------

int read_params(void)
{
    FILE *fp;
    char s[500];
    params_t *p;
    int cnt, i, j, line=0;
    char *sptr;

    // open config file
    fp = fopen(config_filename, "r");
    if (fp == NULL) {
        ERROR("failed to open '%s', %s\n", config_filename, strerror(errno));
        return -ENOENT;
    }

    // read lines from config file
    while (fgets(s, sizeof(s), fp) != NULL) {
        // keep track of line number
        line++;

        // if line begins with '#' or is blank then continue
        if (s[0] == '#') {
            continue;
        }
        if (strspn(s, " \r\n\t") == strlen(s)) {
            continue;  // s is all whitespace
        }

        // scan the line into params
        p = &params[max_params];
        cnt = sscanf(s, "%s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                     p->name,
                     &p->distance_to_screen,
                     &p->wavelength,
                     &p->slit[0].start, &p->slit[0].end,
                     &p->slit[1].start, &p->slit[1].end,
                     &p->slit[2].start, &p->slit[2].end,
                     &p->slit[3].start, &p->slit[3].end);
        if (cnt != 5 && cnt != 7 && cnt != 9 && cnt != 11) {
            ERROR("config file '%s' error on line %d, cnt=%d\n", config_filename, line, cnt);
            fclose(fp);
            return -EINVAL;
        }

// XXX sanity check and put start < end

        // determine how many slits are defined by the scanned in line
        p->max_slit = (cnt - 3) / 2;

        // adjust the units, to meters:
        // - the config file wavelength is in nanometers
        // - the config file slit start/end are in millimeters
        // these need to be converted to meters
        p->wavelength *= 1e-9;
        for (i = 0; i < p->max_slit; i++) {
            p->slit[i].start *= 1e-3;
            p->slit[i].end   *= 1e-3;
        }

        // XXX
        strcpy(p->status_str, "NO DATA YET");

        // increment the number of params
        max_params++;
    }

    // close the config file
    fclose(fp);

    // XXX sanity check params

    // debug print the params
    INFO("CONFIG FILE PARAMS ...\n");
    for (i = 0; i < max_params; i++) {
        p = &params[i];
        sptr = s;
        sptr += sprintf(sptr, "%s: wl=%.0f nm  d=%.2f m",
                        p->name, p->wavelength*1e9, p->distance_to_screen);
        for (j = 0; j < p->max_slit; j++) {
            sptr += sprintf(sptr, "  slit[%d]=%.2f...%.2f", 
                            j, p->slit[j].start*1e3, p->slit[j].end*1e3);
        }
        INFO("%s\n", s);
    }
    BLANK_LINE;

    // return success
    return 0;
}
