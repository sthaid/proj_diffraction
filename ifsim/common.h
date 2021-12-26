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

#define ELEM_TYPE_SRC_SS    1
#define ELEM_TYPE_SRC_DS    2
#define ELEM_TYPE_SRC_RH    3
#define ELEM_TYPE_SRC_RING  4
#define ELEM_TYPE_MIRROR    5
#define ELEM_TYPE_BEAM_SPL  6
#define ELEM_TYPE_SCREEN    7
#define ELEM_TYPE_DISCARD   8

#define ELEM_SOURCE_FLAG_MASK_BEAMFINDER       (1 << 0)
#define ELEM_MIRROR_FLAG_MASK_DISCARD          (1 << 0)

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
        int type;
        char *type_str;
        int (*hndlr)(struct  element_s *elem, photon_t *photon);
        geo_plane_t plane;
        geo_plane_t initial_plane;
        double x_offset;
        double y_offset;
        double pan_offset;
        double tilt_offset;
        int flags;
        int max_flags;
        union {
            struct source_single_slit_s {
                double w;
                double h;
                double wspread;
                double hspread;
                int next;
            } source_single_slit;
            struct source_double_slit_s {
                double w;
                double h;
                double wspread;
                double hspread;
                double ctrsep;
                int next;
            } source_double_slit;
            struct source_round_hole_s {
                double diam;
                double spread;
                int next;
            } source_round_hole;
            struct source_ring_s {
                double id;
                double od;
                double spread;
                int next;
            } source_ring;
            struct mirror_s {
                int next;
            } mirror;
            struct beam_splitter_s {
                int next1;  // these next values are with respect
                int next2;  // to the beam_splitter's normal vector;
                int next3;  // considerint the nrml vector as x axis, the
                int next4;  // next1 is first quadrant, next2 second quadrant, etc
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
void sim_reset(bool start_running);
void sim_run(void);
void sim_stop(void);

void sim_get_state(bool *running, double *rate);
void sim_get_screen(double screen[MAX_SCREEN][MAX_SCREEN]);
void sim_get_recent_sample_photons(photon_t **photons, int *max_photons);

void sim_toggle_element_flag(struct element_s *elem, int flag_idx);

void sim_randomize_element(struct element_s *elem, double xy_span, double pan_tilt_span);
void sim_randomize_all_elements(sim_config_t *cfg, double xy_span, double pan_tilt_span);
void sim_reset_element(struct element_s *elem);
void sim_reset_all_elements(sim_config_t *cfg);
void sim_adjust_element_x(struct element_s *elem, double delta_x);
void sim_adjust_element_y(struct element_s *elem, double delta_y);
void sim_adjust_element_pan(struct element_s *elem, double delta_pan);
void sim_adjust_element_tilt(struct element_s *elem, double delta_tilt);

int display_init(bool swap_white_black);
void display_hndlr(void);
