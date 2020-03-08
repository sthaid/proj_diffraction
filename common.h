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
#define MAX_ELEMENT 10

#define NM2MM(x) ((x) * 1e-6)
#define MM2NM(x) ((x) * 1e6)

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
                double diam;  // XXX this is hole diam
                double spread;
            } source_round_hole;
            struct mirror_s {
                double diam;   // XXX mirror diam
            } mirror;
            struct screen_s {
                double diam;   // XXX screen diam, not sure if this is used
            } screen;
        } u;
    } element[MAX_ELEMENT];
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
bool sim_is_running(void);
void sim_get_screen(double **screen, int *max_screen, double *screen_width_and_height);
void sim_get_recent_sample_photons(photon_t **photons, int *max_photons);

int display_init(void);
void display_hndlr(void);
