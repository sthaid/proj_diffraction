#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <assert.h>

#include <util_misc.h>
#include <util_sdl.h>

//
// defines
//

#define MAX_PARAMS 20
#define MAX_SLIT   4

//
// typedefs
//

typedef struct {
    char name[100];
    struct {
        double start;
        double end;
    } slit[MAX_SLIT];
    int max_slit;
    double distance_to_screen;
    double wavelength;

    int calculating_percent_complete;
    double *screen;
} params_t;

//
// global variables
//

params_t params[MAX_PARAMS];
int      max_params;

//
// prototypes
//

void display_handler(void);
void calculate_screen_image(params_t *p);

#endif
