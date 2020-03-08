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

#define ELEMENT_NAME_STR(_elem) \
    ((_elem) == source_single_slit_hndlr ? "source_single_slit" : \
     (_elem) == source_double_slit_hndlr ? "source_double_slit" : \
     (_elem) == source_round_hole_hndlr  ? "source_round_hole"  : \
     (_elem) == mirror_hndlr             ? "mirror"             : \
     (_elem) == screen_hndlr             ? "screen"             : \
                                           "????")

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

static int read_config_file(char *config_filename);
static bool is_comment_or_blank_line(char *s);
static void remove_trailing_newline_char(char *s);

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

int sim_init(char *config_filename)
{
    pthread_t thread_id;
    long i;

    if (read_config_file(config_filename) < 0) {
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
            screen[k++] = sqrt(sum);
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

static int read_config_file(char *config_filename)
{
    FILE         *fp;
    char          line[1000];
    int           line_num;
    bool          expecting_config_definition_line = true;
    sim_config_t *cfg;

    fp = NULL;
    cfg = &config[0];
    max_config = 0;
    expecting_config_definition_line = true;
    line_num = 0;

    // open config file
    fp = fopen(config_filename, "r");
    if (fp == NULL) {
        ERROR("failed to open config file '%s'\n", config_filename);
        goto error;
    }

    // read lines from config file
    while (fgets(line,sizeof(line),fp) != NULL) {
        // keep track of line number,
        // remove newline char at end of line, and
        // discard comment lines or blank lines
        line_num++;
        remove_trailing_newline_char(line);
        if (is_comment_or_blank_line(line)) {
            continue;
        }

        // if expecting config definition line, then
        // scanf for the config name and wavelength, and continue
        if (expecting_config_definition_line) {
            if (sscanf(line, "%s %lf", cfg->name, &cfg->wavelength) != 2) {
                ERROR("scan for config name and wavelength failed, line %d\n", line_num);
                goto error;
            }
            expecting_config_definition_line = false;
            continue;
        }

        // --- the following code is dealing with element definition lines ---
        int elem_id, char_count, cnt;
        char elem_type_str[100];
        element_t *elem;

        // check for line begining with '.' which indicates that this cfg definition 
        // is complete
        if (line[0] == '.') {
            max_config++;
            cfg = &config[max_config];
            expecting_config_definition_line = true;
            continue;
        }

        // scan for the element id and type-string; and ensure the element id is sequential
        if (sscanf(line, "%d %s %n", &elem_id, elem_type_str, &char_count) != 2) {
            ERROR("scan for element id and type failed, line %d\n", line_num);
            goto error;
        }

        // based on element type-string:
        // - continue scanning for the element's other parameters
        // - set the handler
        // - some elements have special requirements, which are validated  XXX TBD
        elem = &cfg->element[cfg->max_element];

        if (strcmp(elem_type_str, "source_single_slit") == 0) {
            elem->hndlr = source_single_slit_hndlr;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf w=%lf h=%lf wspread=%lf hspread=%lf next=%d",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c,
                   &elem->u.source_single_slit.w, 
                   &elem->u.source_single_slit.h, 
                   &elem->u.source_single_slit.wspread, 
                   &elem->u.source_single_slit.hspread, 
                   &elem->next);
            if (cnt != 11) {
                ERROR("scanning element source_single_slit, line %d\n", line_num);
                goto error;
            }
            cfg->max_element++;

        } else if (strcmp(elem_type_str, "source_double_slit") == 0) {
            elem->hndlr = source_double_slit_hndlr;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf w=%lf h=%lf wspread=%lf hspread=%lf ctrsep=%lf next=%d",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c,
                   &elem->u.source_double_slit.w, 
                   &elem->u.source_double_slit.h, 
                   &elem->u.source_double_slit.wspread, 
                   &elem->u.source_double_slit.hspread, 
                   &elem->u.source_double_slit.ctrsep,
                   &elem->next);
            if (cnt != 12) {
                ERROR("scanning element source_double_slit, line %d\n", line_num);
                goto error;
            }
            cfg->max_element++;

        } else if (strcmp(elem_type_str, "source_round_hole") == 0) {
            elem->hndlr = source_round_hole_hndlr;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf diam=%lf spread=%lf next=%d",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c,
                   &elem->u.source_round_hole.diam, 
                   &elem->u.source_round_hole.spread, 
                   &elem->next);
            if (cnt != 9) {
                ERROR("scanning element source_round_hole, line %d\n", line_num);
                goto error;
            }
            cfg->max_element++;

        } else if (strcmp(elem_type_str, "mirror") == 0) {
            elem->hndlr = mirror_hndlr;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf diam=%lf next=%d",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c,
                   &elem->u.mirror.diam, 
                   &elem->next);
            if (cnt != 8) {
                ERROR("scanning element mirror, line %d\n", line_num);
                goto error;
            }
            cfg->max_element++;

        } else if (strcmp(elem_type_str, "screen") == 0) {
            elem->hndlr = screen_hndlr;
            elem->next = -1;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c);
            if (cnt != 6) {
                ERROR("scanning element screen, line %d\n", line_num);
                goto error;
            }
            cfg->max_element++;
        }
    }

    // validate the config; these are basic sanity checks and not 
    // an extensive validation; validations include:
    // - first element must be source
    // - there can be just one source and one screen
    // - all next must be valid
    // XXX

    // debug print config
    int i,j;
    for (i = 0; i < max_config; i++) {
        sim_config_t *cfg = &config[i];
        INFO("config %d - %s %f\n", i, cfg->name, cfg->wavelength);
        for (j = 0; j < cfg->max_element; j++) {
            element_t *elem = &cfg->element[j];
            INFO("  %d %s next=%d\n", 
                 j, ELEMENT_NAME_STR(elem->hndlr), elem->next);
        }
    }

    // close config file
    fclose(fp);
    fp = NULL;
    return 0;

error:
    // error return
    if (fp) {
        fclose(fp);
        fp = NULL;
    }
    return -1;
}

static bool is_comment_or_blank_line(char *s)
{
    while (*s) {
        if (*s == '#') return true;
        if (*s != ' ') return false;
        s++;
    }
    return true;
}

static void remove_trailing_newline_char(char *s)
{
    size_t len;

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
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
                  ELEMENT_NAME_STR(elem->hndlr), idx, 
                  ELEMENT_NAME_STR(current_config->element[next].hndlr), next, 
                  line_str(&photon->current,s1));
        } else {
            DEBUG("photon done at %s %d, - %s\n",
                  ELEMENT_NAME_STR(elem->hndlr), idx, point_str(&photon->current.p,s1));
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
    struct source_single_slit_s *ss = &elem->u.source_single_slit;
    double width_pos;
    double height_pos;
    double width_spread_angle;
    double height_spread_angle;

    // orientation must be in the +/-x or +/-y direction
    assert(elem->plane.n.c == 0);
    assert((elem->plane.n.a == 0 && fabs(elem->plane.n.b) == 1) ||
           (elem->plane.n.b == 0 && fabs(elem->plane.n.a) == 1));

    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // set the photons current location (current.p) and direction (current.v)
    width_pos           = random_range(-ss->w/2, ss->w/2);
    height_pos          = random_range(-ss->h/2, ss->h/2);
    width_spread_angle  = random_range(DEG2RAD(-ss->wspread/2),DEG2RAD(ss->wspread/2));
    height_spread_angle = random_range(DEG2RAD(-ss->hspread/2),DEG2RAD(ss->hspread/2));
    // - set photon z position and vector c component
    photon->current.p.z = height_pos;
    photon->current.v.c = 1 * tan(height_spread_angle);
    if (elem->plane.n.a != 0) {
        // - photon leaving source in eihter the +x or -x direction
        photon->current.p.x = 0;
        photon->current.p.y = width_pos;
        photon->current.v.a = elem->plane.n.a;
        photon->current.v.b = 1 * tan(width_spread_angle);
    } else {
        // - photon leaving source in eihter the +y or -y direction
        photon->current.p.x = width_pos;
        photon->current.p.y = 0;
        photon->current.v.a = 1 * tan(width_spread_angle);
        photon->current.v.b = elem->plane.n.a;
    }

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;
    
    // return next element
    return elem->next;
}

static int source_double_slit_hndlr(element_t *elem, photon_t *photon)
{
    struct source_double_slit_s *ds = &elem->u.source_double_slit;
    double slit_center;
    double width_pos;
    double height_pos;
    double width_spread_angle;
    double height_spread_angle;

    // orientation must be in the +/-x or +/-y direction
    assert(elem->plane.n.c == 0);
    assert((elem->plane.n.a == 0 && fabs(elem->plane.n.b) == 1) ||
           (elem->plane.n.b == 0 && fabs(elem->plane.n.a) == 1));

    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // choose the slit
    slit_center = (random_range(0,1) < 0.5) ? -ds->ctrsep/2 : +ds->ctrsep/2;

    // set the photons current location (current.p) and direction (current.v)
    width_pos           = random_range(-ds->w/2, ds->w/2) + slit_center;
    height_pos          = random_range(-ds->h/2, ds->h/2);
    width_spread_angle  = random_range(DEG2RAD(-ds->wspread/2),DEG2RAD(ds->wspread/2));
    height_spread_angle = random_range(DEG2RAD(-ds->hspread/2),DEG2RAD(ds->hspread/2));
    // - set photon z position and vector c component
    photon->current.p.z = height_pos;
    photon->current.v.c = 1 * tan(height_spread_angle);
    if (elem->plane.n.a != 0) {
        // - photon leaving source in eihter the +x or -x direction
        photon->current.p.x = 0;
        photon->current.p.y = width_pos;
        photon->current.v.a = elem->plane.n.a;
        photon->current.v.b = 1 * tan(width_spread_angle);
    } else {
        // - photon leaving source in eihter the +y or -y direction
        photon->current.p.x = width_pos;
        photon->current.p.y = 0;
        photon->current.v.a = 1 * tan(width_spread_angle);
        photon->current.v.b = elem->plane.n.a;
    }

    // add new current photon position to points array
    photon->points[photon->max_points++] = photon->current.p;

    // return next element
    return elem->next;
}

static int source_round_hole_hndlr(element_t *elem, photon_t *photon)
{
    struct source_round_hole_s *rh = &elem->u.source_round_hole;
    double width_pos;
    double height_pos;
    double width_spread_angle;
    double height_spread_angle;
    double radius;

    // orientation must be in the +/-x or +/-y direction
    assert(elem->plane.n.c == 0);
    assert((elem->plane.n.a == 0 && fabs(elem->plane.n.b) == 1) ||
           (elem->plane.n.b == 0 && fabs(elem->plane.n.a) == 1));

    // init the photon to all fields 0
    memset(photon, 0, sizeof(photon_t));

    // set the photons current location (current.p) and direction (current.v)
    radius = rh->diam/2;
    do {
        width_pos  = random_range(-radius, +radius);
        height_pos = random_range(-radius, +radius);
    } while (square(width_pos) + square(height_pos) > square(radius));
    // XXX this looks wrong vvv
    width_spread_angle  = random_range(DEG2RAD(-rh->spread/2),DEG2RAD(rh->spread/2));
    height_spread_angle = random_range(DEG2RAD(-rh->spread/2),DEG2RAD(rh->spread/2));
    // - set photon z position and vector c component
    photon->current.p.z = height_pos;
    photon->current.v.c = 1 * tan(height_spread_angle);
    if (elem->plane.n.a != 0) {
        // - photon leaving source in eihter the +x or -x direction
        photon->current.p.x = 0;
        photon->current.p.y = width_pos;
        photon->current.v.a = elem->plane.n.a;
        photon->current.v.b = 1 * tan(width_spread_angle);
    } else {
        // - photon leaving source in eihter the +y or -y direction
        photon->current.p.x = width_pos;
        photon->current.p.y = 0;
        photon->current.v.a = 1 * tan(width_spread_angle);
        photon->current.v.b = elem->plane.n.a;
    }

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
    intersect(&photon->current, &elem->plane, &point_intersect);   // XXX maybe should also return T to check the direction
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
    //struct screen_s *scr = &elem->u.screen;
    geo_point_t      point_intersect;
    int              screen_horizontal_idx, screen_vertical_idx;

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    //XXX INFO("elem %p\n", elem);

    // orientation must be in the +/-x or +/-y direction
    assert(elem->plane.n.c == 0);
    assert((elem->plane.n.a == 0 && fabs(elem->plane.n.b) == 1) ||
           (elem->plane.n.b == 0 && fabs(elem->plane.n.a) == 1));

    // intersect the photon with the screen
    intersect(&photon->current, &elem->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // update photon total_distance
    photon->total_distance += distance(&photon->current.p, &point_intersect);

    // determine screen coordinates of the intersect point
    // XXX needs more comments
    // XXX need define for 100
    if (elem->plane.n.a) {
        screen_horizontal_idx = (point_intersect.y - elem->plane.p.y) * 100 + MAX_SCREEN/2;
    } else {
        screen_horizontal_idx = (point_intersect.x - elem->plane.p.x) * 100 + MAX_SCREEN/2;
    }
    screen_vertical_idx = (point_intersect.z - elem->plane.p.z) * 100 + MAX_SCREEN/2;

    // XXX comment
    if (screen_horizontal_idx >= 0 && screen_horizontal_idx < MAX_SCREEN &&
        screen_vertical_idx >= 0 && screen_vertical_idx < MAX_SCREEN)
    {
        double n, angle;
        n = photon->total_distance / current_config->wavelength;
        angle = (n - floor(n)) * (2*M_PI);
        pthread_mutex_lock(&mutex);
        screen_amp1[screen_vertical_idx][screen_horizontal_idx] += sin(angle);
        screen_amp2[screen_vertical_idx][screen_horizontal_idx] += cos(angle);
        pthread_mutex_unlock(&mutex);
    }

    // set new photon position, and
    // add new current photon position to points array
    photon->current.p = point_intersect;
    photon->points[photon->max_points++] = photon->current.p;

    // return next element, which always equals -1 in this coase
    return elem->next;
}
