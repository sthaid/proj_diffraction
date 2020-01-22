#include "common.h"

//
// defines
//

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
static void amplitude_init(param_t *p);
static void amplitude_cleanup(param_t *p);
static void amplitude(param_t *p, double delta_y, double *amp1, double *amp2);
#ifdef ENABLE_LOGGING_AT_DEBUG_LEVEL
static char * stars(double value, int stars_max, double value_max);
#endif

// -----------------  CALCULATE_SCREEN_IMAGE  -----------------------------------

void calculate_screen_image(param_t *p)
{
    pthread_t thread_id;

    // if calculation in progress or already complete then just return
    if (p->calc_inprog_or_complete) {
        return;
    }

    // set flag to indicate calculation has started, 
    // and create a thread to do the calculation, and create the p->graph
    p->calc_inprog_or_complete = true;
    pthread_create(&thread_id, NULL, calculate_screen_image_thread, p);
}

// -----------------  CALCULATE_SCREEN_IMAGE_THREAD  ----------------------------

static void * calculate_screen_image_thread(void *cx)
{
    param_t *p = cx;
    int slit_idx, screen_idx;
    double ysource, yscreen, ret_amp1, ret_amp2;
    double *screen1_amp, *screen2_amp, *screen_inten;
    long progress=0, max_progress=0;

    // initialize status_str, which is used by the display software
    strcpy(p->status_str, "CALCULATING");

    // initialize max_progress, which is used in the calculation loop
    // below to update status_str with percentage complete
    for (slit_idx = 0; slit_idx < p->max_slit; slit_idx++) {
        for (ysource = p->slit[slit_idx].start; ysource <= p->slit[slit_idx].end; ysource += SCREEN_ELEMENT_SIZE) {
            max_progress++;
        }
    }

    // initialization
    screen1_amp = calloc(MAX_SCREEN, sizeof(double));
    screen2_amp = calloc(MAX_SCREEN, sizeof(double));
    screen_inten = calloc(MAX_SCREEN, sizeof(double));
    amplitude_init(p);

    // determine the amplitude projected from each point of each slit to
    // each point of the screen; and sum these amplitudes into the 
    // screen1_amp and screen2_amp arrays; these 2 arrays contain amplitudes
    // that are 90 degrees out of phase
    for (slit_idx = 0; slit_idx < p->max_slit; slit_idx++) {
        for (ysource = p->slit[slit_idx].start; ysource <= p->slit[slit_idx].end; ysource += SCREEN_ELEMENT_SIZE) {
            yscreen = -SCREEN_SIZE / 2;
            for (screen_idx = 0; screen_idx < MAX_SCREEN; screen_idx++) {
                amplitude(p, yscreen-ysource, &ret_amp1, &ret_amp2);
                screen1_amp[screen_idx] += ret_amp1;
                screen2_amp[screen_idx] += ret_amp2;
                yscreen += SCREEN_ELEMENT_SIZE;
            }

            sprintf(p->status_str, "CALCULATING - %2d PERCENT", (int)(100. * ++progress / max_progress));
        }
    }

    // create screen_inten by summing the squared screen1/2_amp
    for (screen_idx = 0; screen_idx < MAX_SCREEN; screen_idx++) {
        screen_inten[screen_idx] = screen1_amp[screen_idx] * screen1_amp[screen_idx] + 
                                   screen2_amp[screen_idx] * screen2_amp[screen_idx];
    }

    //
    // Most of the computational work was done in the loop above.
    // The value in screen_inten is used below to 
    // generate the graph which is displayed by the display software.
    //

    int i, k=0;
    double *graph, maximum_graph_element_value;

    // allocate memory for the graph which will be displayed
    graph = calloc(MAX_GRAPH, sizeof(double));

    // create the graph elements by averaging the screen_inten
    // elements that make up each graph element
    for (i = 0; i < MAX_GRAPH; i++) {
        double sum = 0;
        int cnt = 0;
        while (k < nearbyint((i + 1) * SCREEN_ELEMENTS_PER_GRAPH_ELEMENT)) {
            if (k == MAX_SCREEN) {
                FATAL("BUG MAX_GRAPH=%d MAX_SCREEN=%d i=%d k=%d\n", MAX_GRAPH, MAX_SCREEN, i, k);
                break;
            }
            sum += screen_inten[k];
            cnt++;
            k++;
        }
        graph[i] = (cnt > 0 ? sum / cnt : 0);
    }

    // normalize graph elements to range 0 to 1
    maximum_graph_element_value = 0;
    for (i = 0; i < MAX_GRAPH; i++) {
        if (graph[i] > maximum_graph_element_value) {
            maximum_graph_element_value = graph[i];
        }
    }
    if (maximum_graph_element_value == 0) {
        strcpy(p->status_str, "ERROR - MAX_GRAPH_ELEMENT_VALUE IS ZERO");
        free(graph);
        goto cleanup;
    }
    for (i = 0; i < MAX_GRAPH; i++) {
        graph[i] /= maximum_graph_element_value;
    }

    // debug print the graph
    for (i = 0; i < MAX_GRAPH; i++) {
        DEBUG("#%6d %6.4f - %s\n", 
              i, graph[i], stars(graph[i], 50, 1));
    }

    //
    // locate the largest fringe, and the fringe adjacent to it, and
    // determine the separation of these 2 finges; and
    // determine the expected separation between adjacent fringes using the equation
    //
    //              distance-to-screen * wavelength
    //   delta-y = ---------------------------------
    //              distance-between-slit-centers
    //    

    int  max_inten_idx=-1, adjacent_fringe_idx=-1;
    double max_inten=0;

    for (i = 0; i < MAX_SCREEN; i++) {
        double fudge;
        // this fudge factor is used to give slight emphasis to the fringes in the
        // center of the screen; the fudge factor ranges from a value of 1.0 at both
        // ends of the screen to a value of 1.001 at the center of the screen
        fudge = 1.0 + (double)(MAX_SCREEN/2 - abs(i - MAX_SCREEN/2)) / (MAX_SCREEN/2) * .001;
        if (screen_inten[i]*fudge > max_inten) {
            max_inten_idx = i;
            max_inten = screen_inten[i]*fudge;
        }
    }

    if (max_inten_idx != -1) {
        if (max_inten_idx >= MAX_SCREEN/2) {
            for (i = max_inten_idx-10; i >= 1; i--) {
                if (screen_inten[i] >= screen_inten[i-1] && screen_inten[i] >= screen_inten[i+1]) {
                    adjacent_fringe_idx = i;
                    break;
                }
            }
        } else {
            for (i = max_inten_idx+10; i <= MAX_SCREEN-2; i++) {
                if (screen_inten[i] >= screen_inten[i-1] && screen_inten[i] >= screen_inten[i+1]) {
                    adjacent_fringe_idx = i;
                    break;
                }
            }
        }
    }

    //
    // publish the results for the code in display.c to display
    //

    // - the graph
    p->graph = graph;
    // - graph indexes for the 2 fringes located, and
    //   the program determined fringe separation
    if (max_inten_idx != -1 && adjacent_fringe_idx != -1) {
        p->graph_fringe_idx1 = nearbyint(max_inten_idx / SCREEN_ELEMENTS_PER_GRAPH_ELEMENT);
        p->graph_fringe_idx2 = nearbyint(adjacent_fringe_idx / SCREEN_ELEMENTS_PER_GRAPH_ELEMENT);
        p->program_fringe_sep = abs(max_inten_idx-adjacent_fringe_idx) * SCREEN_ELEMENT_SIZE;
    }
    // - the expected fringe separation determined by equation
    if (p->max_slit == 2) {
        double slit0_center = (p->slit[0].start + p->slit[0].end) / 2.;
        double slit1_center = (p->slit[1].start + p->slit[1].end) / 2.;
        double slit_center_distance = fabs(slit0_center - slit1_center);
        p->equation_fringe_sep = p->distance_to_screen * p->wavelength / slit_center_distance;
    }
    // - status
    strcpy(p->status_str, "CALCULATIONS COMPLETE");

    //
    // cleanup, and exit thread
    //

cleanup:
    amplitude_cleanup(p);
    free(screen1_amp);
    free(screen2_amp);
    free(screen_inten);
    return NULL;
}

static void amplitude_init(param_t *p)
{
    #define MAX_SAVE_RESULT MAX_SCREEN

    p->save_amplitude_result1     = calloc(MAX_SAVE_RESULT, sizeof(double));
    p->save_amplitude_result2     = calloc(MAX_SAVE_RESULT, sizeof(double));
    p->distance_to_screen_squared = p->distance_to_screen * p->distance_to_screen;
    p->amplitude_calls            = 0;
    p->amplitude_calcs            = 0;
}

static void amplitude_cleanup(param_t *p)
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

static void amplitude(param_t *p, double delta_y, double *amp1, double *amp2)
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

#ifdef ENABLE_LOGGING_AT_DEBUG_LEVEL
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
#endif
