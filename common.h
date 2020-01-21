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

#define MAX_PARAM                         20
#define MAX_SLIT                          4

#define SCREEN_SIZE                       0.050    // 5 cm
#define SCREEN_ELEMENT_SIZE               1000e-9  // 1000 nm
#define MAX_GRAPH                         1000

#define MAX_SCREEN                        ((int)(SCREEN_SIZE / SCREEN_ELEMENT_SIZE))
#define GRAPH_ELEMENT_SIZE                (MAX_SCREEN * SCREEN_ELEMENT_SIZE / MAX_GRAPH)
#define SCREEN_ELEMENTS_PER_GRAPH_ELEMENT (GRAPH_ELEMENT_SIZE / SCREEN_ELEMENT_SIZE)

#define SOURCE_ELEMENT_SIZE               (GRAPH_ELEMENT_SIZE * 0.10)

//
// typedefs
//

typedef struct {
    // param values read from config vile
    char name[100];
    struct {
        double start;
        double end;
    } slit[MAX_SLIT];
    int    max_slit;
    double distance_to_screen;
    double wavelength;

    // published by diffraction.c, for use by display.c
    bool    calc_inprog_or_complete;
    double *graph;
    char    status_str[100];

    // private use by diffraction.c
    double *save_amplitude_result1;
    double *save_amplitude_result2;
    double  distance_to_screen_squared;
    long    amplitude_calls;
    long    amplitude_calcs;
} param_t;

//
// global variables
//

param_t param[MAX_PARAM];
int     max_param;

//
// prototypes
//

void display_handler(void);
void calculate_screen_image(param_t *p);

#endif
