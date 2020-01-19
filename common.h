#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
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

// XXX use param
// XXX check for MAX_PARAMS when reading config
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

    bool calc_inprog_or_complete;
    double *graph;
    int max_graph;
    double graph_element_size;
    char status_str[100];


    // xxx comments
    double *save_amplitude_result1;
    double *save_amplitude_result2;
    double distance_to_screen_squared;
    long amplitude_calls;
    long amplitude_calcs;
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
