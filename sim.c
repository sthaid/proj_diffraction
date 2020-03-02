#include "common.h"

//
// defines
//

#define MAX_SCREEN 5000

//
// typedefs
//

typedef struct photon_s {
    geo_line_t current;
    geo_point_t point[10];
    int max_point;
    double total_distance;
} photon_t;

typedef struct element_s element_t;


//
// variables
//

static volatile bool run;

static double screen_amp1[MAX_SCREEN][MAX_SCREEN];
static double screen_amp2[MAX_SCREEN][MAX_SCREEN];

//
// prototypes
//

static int read_config_file(void);

static void *sim_thread(void *cx);
static void simulate_a_photon(void);

static int source_single_slit_hndlr(element_t *elem, photon_t *photon);
static int mirror_hndlr(element_t *elem, photon_t *photon);
static int screen_hndlr(element_t *elem, photon_t *photon);

//
// inline procedures
//

static inline double square(double x)
{
    return x * x;
}

// -----------------  SIM APIS  -----------------------------------------------------

int sim_init(void)
{
    pthread_t thread_id;

    if (read_config_file() < 0) {
        return -1;
    }

    pthread_create(&thread_id, NULL, sim_thread, NULL);

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

void sim_get_screen(int zoom_factor, double **screen_arg, int *max_wh_arg, double *wh_mm_arg)
{
    int i,j,k,ii,jj,max_wh;
    double max_screen_value;
    double *screen;

    // allocate memory for screen return buffer;
    // caller must free it when done
    max_wh = MAX_SCREEN / zoom_factor;
    screen = malloc(max_wh*max_wh*sizeof(double));
    INFO("screen = %p  max_wh=%d\n", screen, max_wh);

    // using the screen_amp1, screen_amp2, and zoom_factor as input,
    // compute the return screen buffer intensity values;
    // these values will be normalized later
    k = 0;
    for (i = 0; i < max_wh; i++) {
        for (j = 0; j < max_wh; j++) {
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
    assert(k == max_wh*max_wh);

    // determine max_screen_value
    max_screen_value = -1;
    for (i = 0; i < max_wh*max_wh; i++) {
        if (screen[i] > max_screen_value) {
            max_screen_value = screen[i];
        }
    }
    INFO("max_screen_value %g\n", max_screen_value);

    // normalize screen values to range 0..1
    if (max_screen_value) {
        double max_screen_value_recipricol = 1 / max_screen_value;
        for (i = 0; i < max_wh*max_wh; i++) {
            screen[i] *= max_screen_value_recipricol;
        }
    }

    // return values to caller
    *screen_arg = screen;
    *max_wh_arg = max_wh;
    *wh_mm_arg  = max_wh * .01;  // XXX needs define
}

// -----------------  READ CONFIG FILE  ---------------------------------------------

static int read_config_file(void)
{
    #define INIT_CONFIG(_config_idx, _name, _wavelength) \
        do { \
            sim_config_t *cfg = &config[_config_idx]; \
            strcpy(cfg->name, #_name); \
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
    INIT_ELEM(0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_ELEM(1, mirror,             500,0,0,     -1,1,0,    2);
    INIT_ELEM(2, screen,             500,500,0,   0,-1,0,    -1);
#endif
#if 1
    INIT_CONFIG(max_config, "test1", NM2MM(532));
    INIT_CONFIG_ELEM(max_config, 0, source_single_slit, 0,0,0,       1,0,0,     1);
    INIT_CONFIG_ELEM(max_config, 1, screen,             2000,0,0,    -1,0,0,    -1);
    max_config++;

    current_config = &config[0];

    INFO("WAVELENGTH %g\n", current_config->wavelength);
#endif

    return 0;
}

// -----------------  SIMULATION THREAD  --------------------------------------------

static void *sim_thread(void *cx)
{
    while (true) {
        // wait for run flag to be set
        INFO("WAITING FOR UN\n");
        while (!run) {
            usleep(10000);
        }

        // while run flag is set, simulate photons
        INFO("RUN IS SET\n");
        while (run) {
            sleep(1);
            INFO("CALLING SIMULATE\n");
            simulate_a_photon();
            __sync_synchronize();
        }
    }

    return NULL;
}

static void simulate_a_photon(void)
{
    int idx=0, next;
    element_t *elem;
    char s1[100] __attribute__((unused));
    photon_t photon;

    while (true) {
        elem = &current_config->element[idx];

        next = (elem->hndlr)(elem, &photon);

        if (next != -1) {
            INFO("photon leaving %s %d, next %s %d - %s\n",
                  elem->name, idx, current_config->element[next].name, next, line_str(&photon.current,s1));
        } else {
            INFO("photon done at %s %d, - %s\n",
                  elem->name, idx, point_str(&photon.current.p,s1));
        }

        idx = next;
        if (idx == -1) {
            break;
        }
    }
}

// -----------------  OPTICAL ELEMENT HANDLERS  -----------------------------------

// XXX this is also constrained, must be aligned and z = 0
static int source_single_slit_hndlr(element_t *elem, photon_t *photon)
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

static int mirror_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_tmp, point_intersect, point_reflected;
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

static int screen_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_intersect;
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


    if (screen_x_idx >= 0 && screen_x_idx < MAX_SCREEN &&
        screen_z_idx >= 0 && screen_z_idx < MAX_SCREEN)
    {
        double n, angle, amp1, amp2;

        n = photon->total_distance / current_config->wavelength;
        angle = (n - floor(n)) * (2*M_PI);
        amp1 = sin(angle);
        amp2 = cos(angle);
        screen_amp1[screen_x_idx][screen_z_idx] += amp1;
        screen_amp2[screen_x_idx][screen_z_idx] += amp2;
        INFO("screen_amp1/2 [%d] = %g %g\n",
             screen_x_idx,
             screen_amp1[screen_x_idx][screen_z_idx],
             screen_amp2[screen_x_idx][screen_z_idx]);
    } else {
        INFO("SKIPPLING\n");
    }

    INFO("screen_x = %g screen_z = %g   %d %d  - td = %g\n", 
         screen_x, screen_z, screen_x_idx, screen_z_idx,
         photon->total_distance);

    // set new photon position
    photon->current.p = point_intersect;

    // return next element
    return elem->next;
}


#if 0
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
#endif
