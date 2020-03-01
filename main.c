// XXX -O2
// XXX check rc on geo routines
// XXX any way to not pass strings to print routines?
// XXX handle phase shift at mirrors if needed

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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
    int (*hndlr)(struct element_s *elm, photon_t *photon);
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
int source_single_slit_hndlr(element_t *elm, photon_t *photon);
int mirror_hndlr(element_t *elm, photon_t *photon);
int screen_hndlr(element_t *elm, photon_t *photon);

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
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, mirror,             500,0,0,     -1,1,0,    2);
    INIT_ELEM(2, screen,             500,500,0,   0,-1,0,    -1);

    // simulate photons
    #define MAX_PHOTONS 10000000
    start_us = microsec_timer();
    for (i = 0; i < MAX_PHOTONS; i++) {
        simulate_a_photon(&photon);
    }
    dur_us = microsec_timer() - start_us;
    INFO("duration for %d million photons = %ld ms\n",  MAX_PHOTONS/1000000, dur_us/1000);

    // print the photon
}

// XXX next steps TODO
//
// - use randomness at the source
// - collect the data at the screen
// - print the screen
//
// - display the screen using the 3 different formats
//
// - multi threads for photon simulations
//
// XXX next steps DONE
//
// - time 1 million photons   DONE
//    "duration for 10 million photons = 756 ms"   without O2
//    "duration for 10 million photons = 576 ms"   with -O2
//

void simulate_a_photon(photon_t *photon)
{
    int idx=0, next;
    char s1[100];

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

int source_single_slit_hndlr(element_t *elm, photon_t *photon)
{
    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // set the photons current location (current.p) and direction (current.v)
    photon->current.p = elm->plane.p;   // XXX work needed
    photon->current.v = elm->plane.n;   // XXX work needed

    // XXX comment tbd
    photon->point[photon->max_point] = photon->current.p;
    photon->max_point++;
    
    // return next element
    return elm->next;
}

int mirror_hndlr(element_t *elm, photon_t *photon)
{
    point_t point_tmp, point_intersect, point_reflected;
    char s[100];

    // intersect the photon with the mirror
    intersect(&photon->current, &elm->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // create a point a little before the intesect point
    point_tmp.x = point_intersect.x - photon->current.v.a;
    point_tmp.y = point_intersect.y - photon->current.v.b;
    point_tmp.z = point_intersect.z - photon->current.v.c;

    // reflect this point over the mirror plane
    reflect(&point_tmp, &elm->plane, &point_reflected);

    // construct new path for the photon using the reflected and intersect points
    photon->current.p = point_intersect;
    photon->current.v.a = point_intersect.x - point_reflected.x;
    photon->current.v.b = point_intersect.y - point_reflected.y;
    photon->current.v.c = point_intersect.z - point_reflected.z;

    // return next element
    return elm->next;
}

int screen_hndlr(element_t *elm, photon_t *photon)
{
    point_t point_intersect;
    char s[100];

    // intersect the photon with the screen
    intersect(&photon->current, &elm->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // set new photon position
    photon->current.p = point_intersect;

    // return next element
    return elm->next;
}
