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

//
// inline procedures
//

static inline double square(double x)
{
    return x * x;
}

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

    // create screen_inten by summing the squared screen1/2_amp and then taking sqrt
    for (screen_idx = 0; screen_idx < MAX_SCREEN; screen_idx++) {
        screen_inten[screen_idx] = ( square(screen1_amp[screen_idx]) + square(screen2_amp[screen_idx]) );
    }

    // set status_str to indicate the calculations are complete
    strcpy(p->status_str, "CALCULATIONS COMPLETE");

    // publish 
    p->screen_inten = screen_inten;

    // cleanup and return
    amplitude_cleanup(p);
    free(screen1_amp);
    free(screen2_amp);
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

