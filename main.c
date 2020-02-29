#include <stdio.h>
#include <stdbool.h>

#include <util_geometry.h>

typedef struct {
    line_t current;
    point_t point[10];
    int max_point;
    double total_distance;
} photon_t;

typedef struct element_s {
    int (*hndlr)(struct element_s *elm, photon_t *photon);
    point_t center;
    vector_t normal;
    int next;

    //struct source_single_slit_s {
    //} source_single_slit;
} element_t;

element_t element[20];

void simulate_a_photon(photon_t *photon);
int source_single_slit_hndlr(element_t *elm, photon_t *photon);

// --------------------------------------------------------------------------------

int main(int argc, char **argv)
{
    photon_t photon;
    element_t *elm;

    // read config file
    // - first element must be a source
    // - must be just one source and just one screen
    // - have a routine for each element
    elm = &element[0];
    elm->hndlr = source_single_slit_hndlr;
    POINT_INIT(&elm->center, 0,500,0);
    VECT_INIT(&elm->normal, 1,0,0);
    elm->next = 1;

    // simulate_a_photon
    simulate_a_photon(&photon);

    // print the photon
}

void simulate_a_photon(photon_t *photon)
{
    int idx = 0;

    while (true) {
        idx = (element[idx].hndlr)(&element[idx], photon);
        if (idx == -1) {
            break;
        }
    }
}

int source_single_slit_hndlr(element_t *elm, photon_t *photon)
{
    photon->current.p = elm->center;   // XXX work needed
    photon->current.v = elm->normal;   // XXX work needed

    photon->point[photon->max_point] = photon->current.p;
    photon->max_point++;

    photon->total_distance = 0;

    return elm->next;
}
