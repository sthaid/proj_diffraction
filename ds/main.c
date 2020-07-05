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

//
// prototypes
//

void help(void);
int read_param(char *filename);

// -----------------  MAIN  -----------------------------------------------------

int main(int argc, char **argv)
{
    int rc;
    char *filename = "dsparam";
    char opt_char;

    // get options
    while (true) {
        opt_char = getopt(argc, argv, "f:h");
        if (opt_char == -1) {
            break;
        }
        switch (opt_char) {
        case 'f':
            filename = optarg;
            break;
        case 'h':
            help();
            return 0;
        default:
            return 1;
        }
    }

    // read program parameters
    rc = read_param(filename);
    if (rc != 0) {
        return 1;
    }

    // debug print constants
    INFO("MAX_SCREEN                        = %d\n", MAX_SCREEN);
    INFO("SCREEN_SIZE                       = %f\n", SCREEN_SIZE);
    INFO("SCREEN_ELEMENT_SIZE               = %0.9f\n", SCREEN_ELEMENT_SIZE);
    BLANK_LINE;

    // run time loop 
    display_handler();

    // done
    return 0;
}

void help(void)
{
    INFO("usage: ds [-f <param_filename>]\n");
}

// -----------------  READ_PARAM  -----------------------------------------------

int read_param(char *filename)
{
    FILE *fp=NULL;
    char s[10000];
    param_t *p;
    int cnt, i, j, line=0, ret=0;
    char *sptr;

    // open param file
    fp = fopen(filename, "r");
    if (fp == NULL) {
        ERROR("param file '%s' failed to open, %s\n", filename, strerror(errno));
        ret = -ENOENT;
        goto error_return;
    }

    // read lines from param file
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

        // check for line specifying the default param_select_idx
        if (sscanf(s, "DEFAULT %d", &param_select_idx) == 1) {
            continue;
        }

        // if too many params then return error
        if (max_param == MAX_PARAM) {
            ERROR("param file '%s' has too many params\n", filename);
            ret = -EINVAL;
            goto error_return;
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
            ERROR("param file '%s' error on line %d, cnt=%d\n", filename, line, cnt);
            ret = -EINVAL;
            goto error_return;
        }

        // init other param fields
        // - number of slits defined
        // - status_str
        p->max_slit = (cnt - 3) / 2;
        strcpy(p->status_str, "NO DATA YET");

        // adjust the units, to meters:
        // - the param file wavelength is in nanometers
        // - the param file slit start/end are in millimeters
        // these need to be converted to meters
        p->wavelength *= 1e-9;
        for (i = 0; i < p->max_slit; i++) {
            p->slit[i].start *= 1e-3;
            p->slit[i].end   *= 1e-3;
        }

        // sanity check that slit start is <= slit end
        for (i = 0; i < p->max_slit; i++) {
            if (p->slit[i].start > p->slit[i].end) {
                ERROR("param file '%s' slit %d start must be <= end\n", filename, i+1);
                ret = -EINVAL;
                goto error_return;
            }
        }

        // increment the number of param
        max_param++;
    }

    // if no params then return error
    if (max_param == 0) {
        ERROR("param file '%s' contains no param lines\n", filename);
        ret = -EINVAL;
        goto error_return;
    }

    // if default param_select_idx is out of range then return error
    if (param_select_idx < 0 || param_select_idx >= max_param) {
        ERROR("invalid default param_select_idx %d\n", param_select_idx);
        ret = -EINVAL;
        goto error_return;
    }

    // debug print the param
    INFO("PARAM FILE ...\n");
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

    // success return
    fclose(fp);
    return 0;

    // error return
error_return:
    if (fp) {
        fclose(fp);
    }
    return ret;
}
