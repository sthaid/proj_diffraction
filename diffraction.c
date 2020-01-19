/* XXX  comments on how this file works
https://en.wikipedia.org/wiki/Huygens%E2%80%93Fresnel_principle#See_also
https://courses.lumenlearning.com/physics/chapter/27-3-youngs-double-slit-experiment/
*/

#include "common.h"

//
// defines
//

#define SCREEN_SIZE          0.100    // 10 cm
#define SCREEN_ELEMENT_SIZE  1000e-9  // 1000 nm
#define GRAPH_ELEMENT_SIZE   0.1e-3   // .1 mm

#define MAX_SCREEN           ((int)(SCREEN_SIZE / SCREEN_ELEMENT_SIZE))
#define MAX_SAVE_RESULT      MAX_SCREEN

//
// typedefs
//

//
// variables
//

//
// prototypes
//

static void * calculate_screen_image_thread(void *cx);
static void amplitude_init(params_t *p);
static void amplitude_cleanup(params_t *p);
static void amplitude(params_t *p, double delta_y, double *amp1, double *amp2);
static char * stars(double value, int stars_max, double value_max);

// -----------------  CALCULATE_SCREEN_IMAGE  -----------------------------------

void calculate_screen_image(params_t *p)
{
    pthread_t thread_id;

    // if calculation in progress or already complete then just return
    if (p->graph) {
        return;
    }

    // set p->graph to (void*)1 to indicate the calculation is in progress,
    // and create a thread to do the calculation; when the thread completes it
    // will set p->graph equal to the result
    p->graph = (void*)1;
    pthread_create(&thread_id, NULL, calculate_screen_image_thread, p);
}

// -----------------  CALCULATE_SCREEN_IMAGE_THREAD  ----------------------------

static void * calculate_screen_image_thread(void *cx)
{
    params_t *p = cx;
    int slit_idx, screen_idx;
    double ysource, yscreen, ret_amp1, ret_amp2;
    double *screen1_amp, *screen2_amp;
    long progress=0, max_progress=0;

    DEBUG("SCREEN_SIZE          = %f meters\n", SCREEN_SIZE);
    DEBUG("SCREEN_ELEMENT_SIZE  = %f meters\n", SCREEN_ELEMENT_SIZE);
    DEBUG("GRAPH_ELEMENT_SIZE   = %f meters\n", GRAPH_ELEMENT_SIZE);
    DEBUG("MAX_SCREEN           = %d\n", MAX_SCREEN);

    // initialize status_str, which is used by the display software
    strcpy(p->status_str, "CALCULATING");

    // initialize max_progress, which is used in the calculation loop
    // below to update status_str with percentage complete
    for (slit_idx = 0; slit_idx < p->max_slit; slit_idx++) {
        for (ysource = p->slit[slit_idx].start; ysource <= p->slit[slit_idx].end; ysource += SCREEN_ELEMENT_SIZE) {
            max_progress++;
        }
    }

    // XXX comments
    screen1_amp = calloc(MAX_SCREEN, sizeof(double));
    screen2_amp = calloc(MAX_SCREEN, sizeof(double));

    amplitude_init(p);

    for (slit_idx = 0; slit_idx < p->max_slit; slit_idx++) {
        for (ysource = p->slit[slit_idx].start; ysource <= p->slit[slit_idx].end; ysource += SCREEN_ELEMENT_SIZE) {
            yscreen = -SCREEN_SIZE / 2;
            for (screen_idx = 0; screen_idx < MAX_SCREEN; screen_idx++) {
                amplitude(p, yscreen-ysource, &ret_amp1, &ret_amp2);
                screen1_amp[screen_idx] += ret_amp1;
                screen2_amp[screen_idx] += ret_amp2;
                yscreen += SCREEN_ELEMENT_SIZE;
            }
        }

        // update status_str
        progress++;
        sprintf(p->status_str, "CALCULATING - %2d PERCENT\n", 
               (int)(100. * progress / max_progress));
    }

    amplitude_cleanup(p);

    // most of the computational work was done above, 
    // below the values in screen1_amp and screen2_amp are
    // used to generate the p->graph which is displayed by the
    // display software

    // declare variables used below
    int i, j, k, max_graph, screen_elements_per_graph_element;
    double *graph, maximum_graph_element_value;

    // allocate memory for graph which will be displayed
    screen_elements_per_graph_element = GRAPH_ELEMENT_SIZE / SCREEN_ELEMENT_SIZE;
    max_graph = MAX_SCREEN / screen_elements_per_graph_element;
    DEBUG("screen_elements_per_graph_element = %d\n", screen_elements_per_graph_element);
    DEBUG("max_graph                         = %d\n", max_graph);
    graph = calloc(max_graph, sizeof(double));

    // create the graph elements by averaging the screen1_amp and screen2_amp
    // elements that are associated with each graph element
    k = 0;
    for (i = 0; i < max_graph; i++) {
        double sum = 0;
        for (j = 0; j < screen_elements_per_graph_element; j++) {
            sum += screen1_amp[k] * screen1_amp[k];
            sum += screen2_amp[k] * screen2_amp[k];
            k++;
        }
        graph[i] = sum / screen_elements_per_graph_element;
    }

    // normalize graph elements to range 0 to 1
    maximum_graph_element_value = 0;
    for (i = 0; i < max_graph; i++) {
        if (graph[i] > maximum_graph_element_value) {
            maximum_graph_element_value = graph[i];
        }
    }
    if (maximum_graph_element_value == 0) {
        p->graph = graph;
        strcpy(p->status_str, "ERROR - MAX_GRAPH_ELEMENT_VALUE IS ZERO");
        return NULL;
    }
    for (i = 0; i < max_graph; i++) {
        graph[i] /= maximum_graph_element_value;
    }

#if 0
    // debug print the graph
    for (i = 0; i < max_graph; i++) {
        DEBUG("#%6d %6.4f - %s\n", 
               i, graph[i], stars(graph[i], 50, 1));
    }
#endif

    // publish the graph, so that the code in display.c can display it
    p->graph = graph;
    strcpy(p->status_str, "COMPLETE");

    // exit thread
    return NULL;
}

static void amplitude_init(params_t *p)
{
    p->save_amplitude_result1     = calloc(MAX_SAVE_RESULT, sizeof(double));
    p->save_amplitude_result2     = calloc(MAX_SAVE_RESULT, sizeof(double));
    p->distance_to_screen_squared = p->distance_to_screen * p->distance_to_screen;
    p->amplitude_calls            = 0;
    p->amplitude_calcs            = 0;
}

static void amplitude_cleanup(params_t *p)
{
    DEBUG("amplitude_calls = %10ld\n", p->amplitude_calls);
    DEBUG("amplitude_calcs = %10ld\n", p->amplitude_calcs);

    free(p->save_amplitude_result1);
    free(p->save_amplitude_result2);

    p->save_amplitude_result1     = NULL;
    p->save_amplitude_result2     = NULL;
    p->distance_to_screen_squared = 0;
    p->amplitude_calls            = 0;
    p->amplitude_calcs            = 0;
}

static void amplitude(params_t *p, double delta_y, double *amp1, double *amp2)
{
    double d, n, angle;
    int save_idx;

    save_idx = fabs(delta_y) / SCREEN_ELEMENT_SIZE;

    if (save_idx < 0 || save_idx >= MAX_SAVE_RESULT) {
        FATAL("MAX_SAVE_RESULT=%d save_idx=%d\n", MAX_SAVE_RESULT, save_idx);
        exit(1);
    }

    p->amplitude_calls++;

    if (p->save_amplitude_result1[save_idx] == 0) {
        d = sqrt(p->distance_to_screen_squared + delta_y*delta_y);
        n = d / p->wavelength;
        angle = (n - floor(n)) * (2*M_PI);
        p->save_amplitude_result1[save_idx] = sin(angle);
        p->save_amplitude_result2[save_idx] = cos(angle);

        p->amplitude_calcs++;
    }

    *amp1 = p->save_amplitude_result1[save_idx];
    *amp2 = p->save_amplitude_result2[save_idx];
}

// -----------------  UTILS  ----------------------------------------------------

static char * stars(double value, int stars_max, double value_max)
{
    static char stars[1000];
    int len;
    bool overflow = false;

    len = nearbyint((value / value_max) * stars_max);
    if (len > stars_max) {
        len = stars_max;
        overflow = true;
    }

    memset(stars, '*', len);
    stars[len] = '\0';
    if (overflow) {
        strcpy(stars+len, "...");
    }

    return stars;
}

