#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>

#include <util_misc.h>
#include <util_geometry.h>
#include <util_sdl.h>

//
// notes
// - lengths are in mm unless otherwise indicated
//

//
// defines
//

#define MAX_CONFIG 100
#define MAX_CONFIG_ELEMENT 10

#define MAX_SCREEN 500
#define SCREEN_ELEMENT_SIZE  0.1   // mm

//
// typedefs
//

typedef struct {
    geo_line_t current;
    geo_point_t points[10];
    int max_points;
    double total_distance;
} photon_t;

typedef struct {
    char name[100];
    double wavelength;   // mm
    int max_element;
    struct element_s {
        int (*hndlr)(struct  element_s *elem, photon_t *photon);
        geo_plane_t plane;
        int next;
        int next2;  // only for beamsplitter
        union {
            struct source_single_slit_s {
                double w;
                double h;
                double wspread;
                double hspread;
            } source_single_slit;
            struct source_double_slit_s {
                double w;
                double h;
                double wspread;
                double hspread;
                double ctrsep;
            } source_double_slit;
            struct source_round_hole_s {
                double diam;
                double spread;
            } source_round_hole;
            struct mirror_s {
                int nothing_here;
            } mirror;
            struct beam_splitter_s {
                int nothing_here;
            } beam_splitter;
            struct screen_s {
                int nothing_here;
            } screen;
            struct discard_s {
                int nothing_here;
            } discard;
        } u;
    } element[MAX_CONFIG_ELEMENT];
} sim_config_t;

//
// variables
//

sim_config_t   config[MAX_CONFIG];
int            max_config;
sim_config_t * current_config;

//
// prototypes
//

int sim_init(char *config_filename);
void sim_select_config(int idx);
void sim_reset(void);
void sim_run(void);
void sim_stop(void);
void sim_get_state(bool *running, double *rate);
void sim_get_screen(double screen[MAX_SCREEN][MAX_SCREEN]);
void sim_get_recent_sample_photons(photon_t **photons, int *max_photons);

int display_init(void);
void display_hndlr(void);
