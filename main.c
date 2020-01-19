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

int read_param(void);

// -----------------  MAIN  -----------------------------------------------------

int main(int argc, char **argv)
{
    int rc;

    // debug print defined constants
    INFO("SCREEN_SIZE                       = %f\n", SCREEN_SIZE);
    INFO("SCREEN_ELEMENT_SIZE               = %f\n", SCREEN_ELEMENT_SIZE);
    INFO("MAX_GRAPH                         = %d\n", MAX_GRAPH);
    BLANK_LINE;

    // debug print derived constants
    INFO("MAX_SCREEN                        = %d\n", MAX_SCREEN);
    INFO("GRAPH_ELEMENT_SIZE                = %f\n", GRAPH_ELEMENT_SIZE);
    INFO("SCREEN_ELEMENTS_PER_GRAPH_ELEMENT = %d\n", SCREEN_ELEMENTS_PER_GRAPH_ELEMENT);
    BLANK_LINE;

    // get config_filename arg  XXX todo opt -f
    // XXX rename to param.txt
    config_filename = "config";

    // read program parameters
    rc = read_param();
    if (rc != 0) {
        ERROR("read_param failed, %s\n", strerror(-rc));
        return -1;
    }

    // calc XXX comment
    calculate_screen_image(&param[0]);

    // run time loop is in display_handler
    display_handler();

    // done
    return 0;
}

// -----------------  READ_CONFIG  ----------------------------------------------

int read_param(void)
{
    FILE *fp;
    char s[500];
    param_t *p;
    int cnt, i, j, line=0;
    char *sptr;

// XXX check MAX_param

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

        // scan the line into param
        p = &param[max_param];
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
//     and any others checks

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

        // increment the number of param
        max_param++;
    }

    // close the config file
    fclose(fp);

    // debug print the param
    INFO("CONFIG FILE param ...\n");
    for (i = 0; i < max_param; i++) {
        p = &param[i];
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
