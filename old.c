#if 0
main.c
  initializes other files
  calls display handler

simulation.c
  api
    init, among other things this will read_config
    select_config,  this will also start the sim         --|
    sim_ctrl  reset  run  stop                             | called by display
    get_screen                                           --|  

  threads
    wait for run cmd 
    start processing

display.c
  init
  handler
     start with just the one pane for screen, and
      place holders for other panes MAYBE
  
common.h
  apis

config file
  multiple configs in the one file

LATER
  sim.c
    adjust_element
  display.c  
    add other panes
#endif

// XXX get elems from config file

// XXX comment distances are all in mm
// XXX specify screen orientation constraints

// XXX next steps TODO
//
// - check and discard photons if exceeds elem diameter
//
// - circular source
//
// - display the screen using the 3 different formats
//
// - multi threads for photon simulations
//
//
// XXX next steps DONE
//
// - time 1 million photons   DONE
//    "duration for 10 million photons = 756 ms"   without O2
//    "duration for 10 million photons = 576 ms"   with -O2
//
// - use randomness at the source
// - collect the data at the screen
// - print the screen
//


// XXX -O2
// XXX check rc on geo routines
// XXX any way to not pass strings to print routines?
// XXX handle phase shift at mirrors if needed

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <util_misc.h>
#include <util_geometry.h>

//
// defines
//

//
// typedefs
//

typedef struct {
    line_t current;
    point_t point[10];
    int max_point;
    double total_distance;
} photon_t;

typedef struct element_s {
    char *name;
    int (*hndlr)(struct element_s *elem, photon_t *photon);
    plane_t plane;
    int next;

    //struct source_single_slit_s {
    //} source_single_slit;
} element_t;

//
// variables
//

element_t element[20];

//
// prototypes
//

void simulate_a_photon(photon_t *photon);
int source_single_slit_hndlr(element_t *elem, photon_t *photon);
int mirror_hndlr(element_t *elem, photon_t *photon);
int screen_hndlr(element_t *elem, photon_t *photon);

void print_screen_inten(void);

inline double square(double x)
{
    return x * x;
}


// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    photon_t photon;
    int i;
    unsigned long start_us, dur_us;

#define INIT_ELEM(_idx,_name,_ctrx,_ctry,_ctrz,_nrmlx,_nrmly,_nrmlz,_next) \
    do { \
        element_t *elem = &element[_idx]; \
        elem->name = #_name; \
        elem->hndlr = _name##_hndlr; \
        POINT_INIT(&elem->plane.p, _ctrx, _ctry, _ctrz); \
        VECT_INIT(&elem->plane.n, _nrmlx, _nrmly, _nrmlz); \
        elem->next = _next; \
    } while (0)

    // read config file
    // - first element must be a source
    // - must be just one source and just one screen
    // - have a routine for each element
#if 0
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, mirror,             500,0,0,     -1,1,0,    2);
    INIT_ELEM(2, screen,             500,500,0,   0,-1,0,    -1);
#else
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, screen,             2000,0,0,    -1,0,0,    -1);
#endif

    // simulate photons
    #define MAX_PHOTONS 100000000
    start_us = microsec_timer();
    for (i = 0; i < MAX_PHOTONS; i++) {
        simulate_a_photon(&photon);
    }
    dur_us = microsec_timer() - start_us;
    INFO("duration for %d million photons = %ld ms\n",  MAX_PHOTONS/1000000, dur_us/1000);

    // print the photon
    print_screen_inten();
}

void simulate_a_photon(photon_t *photon)
{
    int idx=0, next;
    char s1[100] __attribute__((unused));

    while (true) {
        next = (element[idx].hndlr)(&element[idx], photon);

        if (next != -1) {
            DEBUG("photon leaving %s %d, next %s %d - %s\n", 
                  element[idx].name, idx, element[next].name, next, line_str(&photon->current,s1));
        } else {
            DEBUG("photon done at %s %d, - %s\n",
                  element[idx].name, idx, point_str(&photon->current.p,s1));
        }

        idx = next;
        if (idx == -1) {
            break;
        }
    }
}

// -----------------  ELEMENT HANDLERS  -------------------------------------------

// XXX this is also constrained, must be aligned and z = 0
int source_single_slit_hndlr(element_t *elem, photon_t *photon)
{
    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // set the photons current location (current.p) and direction (current.v)
#if 0 // XXX
    photon->current.p = elem->plane.p;
    photon->current.v = elem->plane.n;
#else
    photon->current.p.x = 0;
    photon->current.p.y = random_range(-.05,+.05);
    photon->current.p.z = 0;

    double angle = random_range(DEG2RAD(-1),DEG2RAD(1));
    photon->current.v.a = 1;
    photon->current.v.b = 1 * tan(angle);
    photon->current.v.c = 0;
#endif

#if 0  // LATER
    photon->point[photon->max_point] = photon->current.p;
    photon->max_point++;
#endif
    
    // return next element
    return elem->next;
}

int mirror_hndlr(element_t *elem, photon_t *photon)
{
    point_t point_tmp, point_intersect, point_reflected;
    char s[100] __attribute__((unused));

    // intersect the photon with the mirror
    intersect(&photon->current, &elem->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // create a point a little before the intesect point
    point_tmp.x = point_intersect.x - photon->current.v.a;
    point_tmp.y = point_intersect.y - photon->current.v.b;
    point_tmp.z = point_intersect.z - photon->current.v.c;

    // reflect this point over the mirror plane
    reflect(&point_tmp, &elem->plane, &point_reflected);

    // construct new path for the photon using the reflected and intersect points
    photon->current.p = point_intersect;
    photon->current.v.a = point_intersect.x - point_reflected.x;
    photon->current.v.b = point_intersect.y - point_reflected.y;
    photon->current.v.c = point_intersect.z - point_reflected.z;

    // return next element
    return elem->next;
}

#define MAX_SCREEN 5000
static double screen_amp1[MAX_SCREEN][MAX_SCREEN];
static double screen_amp2[MAX_SCREEN][MAX_SCREEN];

char *stars(int n)
{
    static char s[50];

    if (n > sizeof(s)-1) {
        n = sizeof(s)-1;
    }

    memset(s, '*', n);
    s[n] = '\0';
    
    return s;
}

void print_screen_inten(void)
{
    int i;
    double inten[MAX_SCREEN];
    double max_inten;

    for (i = 0; i < MAX_SCREEN; i++) {
        inten[i] = square(screen_amp1[i][MAX_SCREEN/2]) + square(screen_amp2[i][MAX_SCREEN/2]);
    }

    max_inten = -1;
    for (i = 0; i < MAX_SCREEN; i++) {
        if (inten[i] > max_inten) {
            max_inten = inten[i];
        }
    }
    printf("max_inten %g\n", max_inten);

    for (i = 0; i < MAX_SCREEN; i++) {
        inten[i] /= max_inten;
    }

    for (i = 0; i < MAX_SCREEN; i++) {
        printf("%3d %0.3f - %s\n", i, inten[i], stars(nearbyint(50*inten[i])));
    }
}

int screen_hndlr(element_t *elem, photon_t *photon)
{
    point_t point_intersect;
    double screen_x, screen_z;
    int screen_x_idx, screen_z_idx;
    char s[100] __attribute__((unused));

    // intersect the photon with the screen
    intersect(&photon->current, &elem->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // determine screen coordinates of the intersect point
#if 0
    screen_x = point_intersect.x - elem->plane.p.x;
    screen_z = point_intersect.z - elem->plane.p.z;
#else
    screen_x = point_intersect.y - elem->plane.p.y;
    screen_z = point_intersect.z - elem->plane.p.z;

    screen_x_idx = nearbyint(screen_x * 100 + MAX_SCREEN/2);
    screen_z_idx = nearbyint(screen_z * 100 + MAX_SCREEN/2);
#endif

    photon->total_distance += distance(&photon->current.p, &point_intersect);

#define NM2MM(x) ((x) * 1e-6)
#define MM2NM(x) ((x) * 1e6)

#define WAVELENGTH NM2MM(532)

    if (screen_x_idx >= 0 && screen_x_idx < MAX_SCREEN &&
        screen_z_idx >= 0 && screen_z_idx < MAX_SCREEN)
    {
        double n, angle, amp1, amp2;

        n = photon->total_distance / WAVELENGTH;
        angle = (n - floor(n)) * (2*M_PI);
        amp1 = sin(angle);
        amp2 = cos(angle);
        screen_amp1[screen_x_idx][screen_z_idx] += amp1;
        screen_amp2[screen_x_idx][screen_z_idx] += amp2;
        //INFO("screen_amp1/2 [%d] = %g %g\n",
             //screen_x_idx,
             //screen_amp1[screen_x_idx][screen_z_idx],
             //screen_amp2[screen_x_idx][screen_z_idx]);
    } else {
        //INFO("SKIPPLING\n");
    }

    //INFO("screen_x = %g screen_z = %g   %d %d  - td = %g\n", 
         //screen_x, screen_z, screen_x_idx, screen_z_idx,
         //photon->total_distance);

    // set new photon position
    photon->current.p = point_intersect;

    // return next element
    return elem->next;
}
