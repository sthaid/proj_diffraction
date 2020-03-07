// XXX add constraint checks on source and screen
// XXX experiment with less than 5000 for MAX_SCREEN, to improve performance
// XXX draw diagram, and add ray tracing
// XXX pan and zoom for diagram and maybe for screen

#include "common.h"

//
// defines
//

#define MAX_SCREEN                5000
#define MAX_SIM_THREAD            1
#define MAX_RECENT_SAMPLE_PHOTONS 1000

//
// typedefs
//

typedef struct element_s element_t;

//
// variables
//

static volatile bool run;

static double screen_amp1[MAX_SCREEN][MAX_SCREEN];
static double screen_amp2[MAX_SCREEN][MAX_SCREEN];

static unsigned long total_photon_count;

static photon_t        recent_sample_photons[MAX_RECENT_SAMPLE_PHOTONS];
static int             max_recent_sample_photons;
static pthread_mutex_t recent_sample_photons_mutex = PTHREAD_MUTEX_INITIALIZER;

//
// prototypes
//

static int read_config_file(void);

static void *sim_thread(void *cx);
static void *sim_monitor_thread(void *cx);
static void simulate_a_photon(photon_t *photon);

static int source_single_slit_hndlr(element_t *elem, photon_t *photon);
static int source_double_slit_hndlr(element_t *elem, photon_t *photon);
static int source_round_hole_hndlr(element_t *elem, photon_t *photon);
static int mirror_hndlr(element_t *elem, photon_t *photon);
static int screen_hndlr(element_t *elem, photon_t *photon);

//
// inline procedures
//

static inline double square(double x)
{
    return x * x;
}

static inline int min(int a, int b) 
{
    return a < b ? a : b;
}

// -----------------  SIM APIS  -----------------------------------------------------

int sim_init(void)
{
    pthread_t thread_id;
    long i;

    if (read_config_file() < 0) {
        return -1;
    }

    for (i = 0; i < MAX_SIM_THREAD; i++) {
        pthread_create(&thread_id, NULL, sim_thread, (void*)i);
    }
    pthread_create(&thread_id, NULL, sim_monitor_thread, NULL);

    return 0;
}

void sim_select_config(int idx)
{
    sim_stop();
    current_config = &config[idx];
}

void sim_reset(void)
{
    sim_stop();

    // XXX tbd
}

void sim_run(void)
{
    run = true;
    // XXX and wait
}

void sim_stop(void)
{
    run = false;
    // XXX and wait
}

bool sim_is_running(void)
{
    return run;
}

void sim_get_screen(double **screen_ret, int *max_screen_ret, double *screen_width_and_height_ret)
{
    int i,j,k,ii,jj,max_screen;
    double max_screen_value;
    double *screen;

    int zoom_factor = 10; // XXX

    // XXX don't like MAX_SCREEN and max_screen, they are differnet

    // allocate memory for screen return buffer;
    // caller must free it when done
    max_screen = MAX_SCREEN / zoom_factor;
    screen = malloc(max_screen*max_screen*sizeof(double));
    DEBUG("screen = %p  max_screen=%d\n", screen, max_screen);

    // using the screen_amp1, screen_amp2, and zoom_factor as input,
    // compute the return screen buffer intensity values;
    // these values will be normalized later
    k = 0;
    for (i = 0; i < max_screen; i++) {
        for (j = 0; j < max_screen; j++) {
            //INFO("i,j %d %d\n", i,j);
            double sum = 0;
            for (ii = i*zoom_factor; ii < (i+1)*zoom_factor; ii++) {
                for (jj = j*zoom_factor; jj < (j+1)*zoom_factor; jj++) {
                    //if (i==456 && j==1) printf("ii jj %d %d\n", ii,jj);
                    sum += square(screen_amp1[ii][jj]) + square(screen_amp2[ii][jj]);
                }
            }
            screen[k++] = sum;
        }
    }
    assert(k == max_screen*max_screen);

    // determine max_screen_value
    max_screen_value = -1;
    for (i = 0; i < max_screen*max_screen; i++) {
        if (screen[i] > max_screen_value) {
            max_screen_value = screen[i];
        }
    }
    DEBUG("max_screen_value %g\n", max_screen_value);

    // normalize screen values to range 0..1
    if (max_screen_value) {
        double max_screen_value_recipricol = 1 / max_screen_value;
        for (i = 0; i < max_screen*max_screen; i++) {
            screen[i] *= max_screen_value_recipricol;
        }
    }

    // return values to caller
    *screen_ret = screen;
    *max_screen_ret = max_screen;
    *screen_width_and_height_ret = MAX_SCREEN * .01;
}

void sim_get_recent_sample_photons(photon_t **photons_out, int *max_photons_out)
{
    int max;

    // acquire mutex
    pthread_mutex_lock(&recent_sample_photons_mutex);

    // max is the smaller of MAX_RECENT_SAMPLE_PHOTONS or the 
    // number of recent sample photons acuumulated by the simulation
    max = min(MAX_RECENT_SAMPLE_PHOTONS, max_recent_sample_photons);

    // allocate the return buffer
    *photons_out = malloc(max*sizeof(photon_t));

    // copy the recent_sample_photons to the allocated return buffer
    memcpy(*photons_out, recent_sample_photons, max*sizeof(photon_t));
    *max_photons_out = max;

    // reset number of recent_sample_photons to 0, so that we'll
    // start accumulating them again
    max_recent_sample_photons = 0;

    // release mutex
    pthread_mutex_unlock(&recent_sample_photons_mutex);
}

// -----------------  READ CONFIG FILE  ---------------------------------------------

static int read_config_file(void)
{
    #define INIT_CONFIG(_config_idx, _name, _wavelength) \
        do { \
            sim_config_t *cfg = &config[_config_idx]; \
            strcpy(cfg->name, _name); \
            cfg->wavelength = _wavelength; \
        } while (0)

    #define INIT_CONFIG_ELEM(_config_idx,_elem_idx,_name,_ctrx,_ctry,_ctrz,_nrmlx,_nrmly,_nrmlz,_next) \
        do { \
            sim_config_t *cfg = &config[_config_idx]; \
            element_t *elem = &cfg->element[_elem_idx]; \
            strcpy(elem->name, #_name); \
            elem->hndlr = _name##_hndlr; \
            POINT_INIT(&elem->plane.p, _ctrx, _ctry, _ctrz); \
            VECT_INIT(&elem->plane.n, _nrmlx, _nrmly, _nrmlz); \
            elem->next = _next; \
        } while (0)

#if 0
    // XXX OLD 
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, mirror,             500,0,0,     -1,1,0,    2);
    INIT_ELEM(2, screen,             500,500,0,   0,-1,0,    -1);
#endif

    INIT_CONFIG(max_config, "test_round_hole", NM2MM(532));
    INIT_CONFIG_ELEM(max_config, 0, source_round_hole,  0,0,0,       1,0,0,     1);
    INIT_CONFIG_ELEM(max_config, 1, screen,             2000,0,0,    -1,0,0,    -1);
    max_config++;

    INIT_CONFIG(max_config, "test_single_slit", NM2MM(532));
    INIT_CONFIG_ELEM(max_config, 0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_CONFIG_ELEM(max_config, 1, screen,             2000,0,0,    -1,0,0,    -1);
    max_config++;

    INIT_CONFIG(max_config, "test_double_slit", NM2MM(532));
    INIT_CONFIG_ELEM(max_config, 0, source_double_slit, 0,0,0,       1,0,0,     1);
    INIT_CONFIG_ELEM(max_config, 1, screen,             2000,0,0,    -1,0,0,    -1);
    max_config++;

    INIT_CONFIG(max_config, "test_double_slit_and_mirror", NM2MM(532));
    INIT_CONFIG_ELEM(max_config, 0, source_double_slit, 0,0,0,         1,0,0,     1);
    INIT_CONFIG_ELEM(max_config, 1, mirror,             1000,0,0,      -1,1,0,    2);
    INIT_CONFIG_ELEM(max_config, 2, screen,             1000,1000,0,   0,-1,0,   -1);
    max_config++;

    current_config = &config[3];

    return 0;
}

// -----------------  SIMULATION THREAD  --------------------------------------------

static void *sim_thread(void *cx)
{
    long id = (long)cx;
    photon_t photon;

    INFO("STARTING id %ld\n", id);

    while (true) {
        // wait for run flag to be set
        INFO("waiting for start request\n");
        while (!run) {
            usleep(10000);
        }

        // while run flag is set, simulate photons
        INFO("starting simulation of %s\n", current_config->name);
        while (run) {
            // simulate a photon
            simulate_a_photon(&photon);

            // thread 0 accumulates recent sample photons, so that the
            // display code can display photon ray traces
            if (id == 0 && max_recent_sample_photons < MAX_RECENT_SAMPLE_PHOTONS) {
                pthread_mutex_lock(&recent_sample_photons_mutex);
                recent_sample_photons[max_recent_sample_photons++] = photon;
                pthread_mutex_unlock(&recent_sample_photons_mutex);
            }
        }
    }

    return NULL;
}

static void *sim_monitor_thread(void *cx)
{
    unsigned long start_us, end_us;
    unsigned long start_photon_count, end_photon_count;
    double photons_per_sec;

    while (true) {
        start_us = microsec_timer();
        start_photon_count = total_photon_count;

        sleep(1);

        end_us = microsec_timer();
        end_photon_count = total_photon_count;

        photons_per_sec = (end_photon_count - start_photon_count) / ((end_us - start_us) / 1000000.);
        INFO("RATE = %g million photons/sec\n", photons_per_sec/1000000);
    }

    return NULL;
}

static void simulate_a_photon(photon_t *photon)
{
    int idx=0, next;
    element_t *elem;
    char s1[100] __attribute__((unused));

    while (true) {
        elem = &current_config->element[idx];

        next = (elem->hndlr)(elem, photon);

        if (next != -1) {
            DEBUG("photon leaving %s %d, next %s %d - %s\n",
                  elem->name, idx, current_config->element[next].name, next, line_str(&photon->current,s1));
        } else {
            DEBUG("photon done at %s %d, - %s\n",
                  elem->name, idx, point_str(&photon->current.p,s1));
        }

        idx = next;
        if (idx == -1) {
            break;
        }
    }

    __sync_fetch_and_add(&total_photon_count,1);
}

// -----------------  OPTICAL ELEMENT HANDLERS  -----------------------------------

// XXX THESE all need to use config AND cleanup
// XXX this is also constrained, must be aligned and z = 0
static int source_single_slit_hndlr(element_t *elem, photon_t *photon)
{
    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // set the photons current location (current.p) and direction (current.v)
    photon->current.p.x = 0;
    photon->current.p.y = random_range(-.05,+.05);
    photon->current.p.z = random_range(-1,+1);

    double angle_width_spread = random_range(DEG2RAD(-1),DEG2RAD(1));
    double angle_height_spread = random_range(DEG2RAD(-.01),DEG2RAD(.01));
    photon->current.v.a = 1;
    photon->current.v.b = 1 * tan(angle_width_spread);
    photon->current.v.c = 1 * tan(angle_height_spread);

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;
    
    // return next element
    return elem->next;
}

static int source_double_slit_hndlr(element_t *elem, photon_t *photon)
{
    int which_slit;

    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // first choose the slit
    which_slit = (random_range(0,1) < 0.5) ? 0 : 1;

    // next choose the position within the slit
    photon->current.p.x = 0;
    photon->current.p.z = random_range(-1,+1);
    if (which_slit == 0) {
        //photon->current.p.y = random_range(-.15, -.05);
        photon->current.p.y = random_range(-.225, -.125);
    } else {
        //photon->current.p.y = random_range(+.05, +.15);
        photon->current.p.y = random_range(+.125, +.225);
    }

    // finally choose the direction
    double angle_width_spread = random_range(DEG2RAD(-1),DEG2RAD(1));
    double angle_height_spread = random_range(DEG2RAD(-.01),DEG2RAD(.01));
    photon->current.v.a = 1;
    photon->current.v.b = 1 * tan(angle_width_spread);
    photon->current.v.c = 1 * tan(angle_height_spread);

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;

    // return next element
    return elem->next;
}

static int source_round_hole_hndlr(element_t *elem, photon_t *photon)
{
    double y,z;

    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // set the photons current location (current.p) and direction (current.v)
    do {
        y = random_range(-.05,.05);
        z = random_range(-.05,.05);
    } while (square(y) + square(z) > square(.1));

    photon->current.p.x = 0;
    photon->current.p.y = y;
    photon->current.p.z = z;

    double angle_width_spread = random_range(DEG2RAD(-1),DEG2RAD(1));
    double angle_height_spread = random_range(DEG2RAD(-1),DEG2RAD(1));

    photon->current.v.a = 1;
    photon->current.v.b = 1 * tan(angle_width_spread);
    photon->current.v.c = 1 * tan(angle_height_spread);

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;

    // return next element
    return elem->next;
}

static int mirror_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_tmp, point_intersect, point_reflected;
    char s[100] __attribute__((unused));

    // intersect the photon with the mirror
    intersect(&photon->current, &elem->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // XXX comment
    photon->total_distance += distance(&photon->current.p, &point_intersect);

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

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;

    // return next element
    return elem->next;
}

static int screen_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_intersect;
    double screen_x, screen_z;
    int screen_x_idx, screen_z_idx;
    char s[100] __attribute__((unused));

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    // intersect the photon with the screen
    intersect(&photon->current, &elem->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // determine screen coordinates of the intersect point
    // call screen_x horizontal and screen_y vertical
#if 1  // XXX needs to be dynamic
    screen_x = point_intersect.x - elem->plane.p.x;
#else
    screen_x = point_intersect.y - elem->plane.p.y;
#endif
    screen_z = point_intersect.z - elem->plane.p.z;

    screen_x_idx = nearbyint(screen_x * 100 + MAX_SCREEN/2);  // XXX this makes .01 m  ??
    screen_z_idx = nearbyint(screen_z * 100 + MAX_SCREEN/2);

    photon->total_distance += distance(&photon->current.p, &point_intersect);

    if (screen_x_idx >= 0 && screen_x_idx < MAX_SCREEN &&
        screen_z_idx >= 0 && screen_z_idx < MAX_SCREEN)
    {
        double n, angle, amp1, amp2;

        n = photon->total_distance / current_config->wavelength;
        angle = (n - floor(n)) * (2*M_PI);
        amp1 = sin(angle);
        amp2 = cos(angle);
        pthread_mutex_lock(&mutex);
        screen_amp1[screen_z_idx][screen_x_idx] += amp1;
        screen_amp2[screen_z_idx][screen_x_idx] += amp2;
        pthread_mutex_unlock(&mutex);
        DEBUG("screen_amp1/2 [%d] = %g %g\n",
             screen_x_idx,
             screen_amp1[screen_z_idx][screen_x_idx],
             screen_amp2[screen_z_idx][screen_x_idx]);
    } else {
        DEBUG("SKIPPLING\n");
    }

    DEBUG("screen_x = %g screen_z = %g   %d %d  - td = %g\n", 
         screen_x, screen_z, screen_x_idx, screen_z_idx,
         photon->total_distance);

    // set new photon position
    photon->current.p = point_intersect;

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;

    // return next element
    return elem->next;
}

