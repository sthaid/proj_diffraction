// XXX put scale on diagram so size of hole can be checked
// XXX experiment with less than 5000 for MAX_SCREEN_AMP, to improve performance

#include "common.h"

//
// defines
//

#define MAX_SCREEN_AMP            5000
#define MAX_SIM_THREAD            1
#define MAX_RECENT_SAMPLE_PHOTONS 1000

#define ELEMENT_NAME_STR(_elem) \
    ((_elem) == source_single_slit_hndlr ? "source_single_slit" : \
     (_elem) == source_double_slit_hndlr ? "source_double_slit" : \
     (_elem) == source_round_hole_hndlr  ? "source_round_hole"  : \
     (_elem) == mirror_hndlr             ? "mirror"             : \
     (_elem) == beam_splitter_hndlr      ? "beam_splitter"      : \
     (_elem) == screen_hndlr             ? "screen"             : \
     (_elem) == discard_hndlr            ? "discard"            : \
                                           "????")

#define SCREEN_AMP_ELEMENT_SIZE .01

#define SOURCE_ROUND_HOLE  1
#define SOURCE_SINGLE_SLIT 2
#define SOURCE_DOUBLE_SLIT 3

//
// typedefs
//

typedef struct element_s element_t;

//
// variables
//

static volatile bool run;

static double screen_amp1[MAX_SCREEN_AMP][MAX_SCREEN_AMP];
static double screen_amp2[MAX_SCREEN_AMP][MAX_SCREEN_AMP];

static unsigned long total_photon_count;
static double        photons_per_sec;

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
static void determine_photon_line(
                int source_type, 
                geo_plane_t *plane,
                double arg1, double arg2, double arg3, double arg4, double arg5,
                geo_line_t *photon_line);

static int mirror_hndlr(element_t *elem, photon_t *photon);
static int beam_splitter_hndlr(element_t *elem, photon_t *photon);
static void mirror_reflect(geo_line_t *line, geo_plane_t *plane);

static int screen_hndlr(element_t *elem, photon_t *photon);
static void determine_screen_coords(
                geo_plane_t *plane, geo_point_t *point_intersect, 
                double *screen_hpos, double *screen_vpos);

static int discard_hndlr(element_t *elem, photon_t *photon);

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
    sim_reset();
    current_config = &config[idx];
}

void sim_reset(void)
{
    sim_stop();
    memset(screen_amp1,0,sizeof(screen_amp1));
    memset(screen_amp2,0,sizeof(screen_amp2));
    max_recent_sample_photons = 0;
}

void sim_run(void)
{
    run = true;
    while (run == false) {
        usleep(1000);
    }
}

void sim_stop(void)
{
    run = false;
    while (run == true) {
        usleep(1000);
    }
}

void sim_get_state(bool *running, double *rate)
{
    if (running) {
        *running = run;
    }
    if (rate) {
        *rate = photons_per_sec;
    }
}

void sim_get_screen(double screen[MAX_SCREEN][MAX_SCREEN])
{
    int       i,j,ii,jj;
    double    max_screen_value;
    const int scale_factor = MAX_SCREEN_AMP / MAX_SCREEN;

    // using the screen_amp1, screen_amp2, and scale_factor as input,
    // compute the return screen buffer intensity values;
    for (i = 0; i < MAX_SCREEN; i++) {
        for (j = 0; j < MAX_SCREEN; j++) {
            double sum = 0;
            for (ii = i*scale_factor; ii < (i+1)*scale_factor; ii++) {
                for (jj = j*scale_factor; jj < (j+1)*scale_factor; jj++) {
                    sum += square(screen_amp1[ii][jj]) + square(screen_amp2[ii][jj]);
                }
            }
            screen[i][j] = sqrt(sum);
        }
    }

    // determine max_screen_value
    max_screen_value = -1;
    for (i = 0; i < MAX_SCREEN; i++) {
        for (j = 0; j < MAX_SCREEN; j++) {
            if (screen[i][j] > max_screen_value) {
                max_screen_value = screen[i][j];
            }
        }
    }
    DEBUG("max_screen_value %g\n", max_screen_value);

    // normalize screen values to range 0..1
    if (max_screen_value) {
        double max_screen_value_recipricol = 1 / max_screen_value;
        for (i = 0; i < MAX_SCREEN; i++) {
            for (j = 0; j < MAX_SCREEN; j++) {
                screen[i][j] *= max_screen_value_recipricol;
            }
        }
    }
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
    int           i,j;

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
        // - continue scanning for the element's other parameters, and
        // - set the handler
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

        } else if (strcmp(elem_type_str, "beam_splitter") == 0) {
            elem->hndlr = beam_splitter_hndlr;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf diam=%lf next=%d next2=%d",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c,
                   &elem->u.beam_splitter.diam, 
                   &elem->next, &elem->next2);
            if (cnt != 9) {
                ERROR("scanning element beam_splitter, line %d\n", line_num);
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

        } else if (strcmp(elem_type_str, "discard") == 0) {
            elem->hndlr = discard_hndlr;
            elem->next = -1;
            cnt = sscanf(line+char_count,
                   "ctr=%lf,%lf,%lf nrml=%lf,%lf,%lf diam=%lf",
                   &elem->plane.p.x, &elem->plane.p.y, &elem->plane.p.z,
                   &elem->plane.n.a, &elem->plane.n.b, &elem->plane.n.c,
                   &elem->u.discard.diam);
            if (cnt != 7) {
                ERROR("scanning element discard, line %d\n", line_num);
                goto error;
            }
            cfg->max_element++;

        } else {
            ERROR("invalid element '%s', line %d\n", elem_type_str, line_num);
            goto error;
        }
    }

    // set the magnitude of all config element plane normal vectors to 1
    for (i = 0; i < max_config; i++) {
        sim_config_t *cfg = &config[i];
        for (j = 0; j < cfg->max_element; j++) {
            set_vector_magnitude(&cfg->element[j].plane.n, 1);
        }
    }

    // validate the config; these are basic sanity checks and not 
    // an extensive validation; validations include:
    // - first element must be source
    // - there can be just one source and one screen
    // - all next must be valid
    // XXX

#if 0
    // debug print config
    for (i = 0; i < max_config; i++) {
        sim_config_t *cfg = &config[i];
        INFO("config %d - %s %f\n", i, cfg->name, cfg->wavelength);
        for (j = 0; j < cfg->max_element; j++) {
            element_t *elem = &cfg->element[j];
            INFO("  %d %s next=%d\n", 
                 j, ELEMENT_NAME_STR(elem->hndlr), elem->next);
        }
    }
#endif

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

    while (true) {
        start_us = microsec_timer();
        start_photon_count = total_photon_count;

        sleep(1);

        end_us = microsec_timer();
        end_photon_count = total_photon_count;

        photons_per_sec = (end_photon_count - start_photon_count) / ((end_us - start_us) / 1000000.);
        if (run) {
            INFO("RATE = %g million photons/sec\n", photons_per_sec/1000000);
        }
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

// -----------------  PHOTON SOURCE HANDLERS  ---------------------------------------- 

static int source_single_slit_hndlr(element_t *elem, photon_t *photon)
{
    struct source_single_slit_s *ss = &elem->u.source_single_slit;
    geo_line_t photon_line;

    // determine the path of the photon leaving this source
    determine_photon_line(SOURCE_SINGLE_SLIT,
                          &elem->plane,
                          ss->w, ss->h, ss->wspread, ss->hspread, 0,
                          &photon_line);
      
    // init the photon fields
    memset(photon, 0, sizeof(photon_t));
    photon->current = photon_line;
    photon->points[photon->max_points++] = photon->current.p;
    
    // return next element
    return elem->next;
}

static int source_double_slit_hndlr(element_t *elem, photon_t *photon)
{
    struct source_double_slit_s *ds = &elem->u.source_double_slit;
    geo_line_t photon_line;

    // determine the path of the photon leaving this source
    determine_photon_line(SOURCE_DOUBLE_SLIT,
                          &elem->plane,
                          ds->w, ds->h, ds->wspread, ds->hspread, ds->ctrsep,
                          &photon_line);
      
    // init the photon fields
    memset(photon, 0, sizeof(photon_t));
    photon->current = photon_line;
    photon->points[photon->max_points++] = photon->current.p;
    
    // return next element
    return elem->next;
}

static int source_round_hole_hndlr(element_t *elem, photon_t *photon)
{
    struct source_round_hole_s *rh = &elem->u.source_round_hole;
    geo_line_t photon_line;

    // determine the path of the photon leaving this source
    determine_photon_line(SOURCE_ROUND_HOLE,
                          &elem->plane,
                          rh->diam, rh->spread, 0, 0, 0,
                          &photon_line);
      
    // init the photon fields
    memset(photon, 0, sizeof(photon_t));
    photon->current = photon_line;
    photon->points[photon->max_points++] = photon->current.p;
    
    // return next element
    return elem->next;
}

// note: argN values depend on source_type, see code for detailes
static void determine_photon_line(
                int source_type, 
                geo_plane_t *plane,
                double arg1, double arg2, double arg3, double arg4, double arg5,
                geo_line_t *photon_line)
{
    geo_vector_t vect_horizontal, vect_vertical;
    double pos_horizontal, pos_vertical, spread_horizontal, spread_vertical;

    // based on source_type determine the position and direction offsets
    if (source_type == SOURCE_ROUND_HOLE) {
        double diameter = arg1;
        double spread   = arg2;
        double radius = diameter / 2;
        do {
            pos_horizontal = random_range(-radius, +radius);
            pos_vertical = random_range(-radius, +radius);
        } while (square(pos_horizontal) + square(pos_vertical) > square(radius));
        spread_horizontal = random_range(-DEG2RAD(spread/2),DEG2RAD(spread/2));
        spread_vertical = random_range(-DEG2RAD(spread/2),DEG2RAD(spread/2));
    } else if (source_type == SOURCE_SINGLE_SLIT) {
        double slit_horizontal         = arg1;
        double slit_vertical           = arg2;
        double slit_horizontal_spread  = arg3;
        double slit_vertical_spread    = arg4;
        double slit_center;
        slit_center = 0;
        pos_horizontal    = random_range(-slit_horizontal/2,slit_horizontal/2) + slit_center;
        pos_vertical      = random_range(-slit_vertical/2,slit_vertical/2);
        spread_horizontal = random_range(DEG2RAD(-slit_horizontal_spread/2),DEG2RAD(slit_horizontal_spread/2));
        spread_vertical   = random_range(DEG2RAD(-slit_vertical_spread/2),DEG2RAD(slit_vertical_spread/2));
    } else if (source_type == SOURCE_DOUBLE_SLIT) {
        double slit_horizontal         = arg1;
        double slit_vertical           = arg2;
        double slit_horizontal_spread  = arg3;
        double slit_vertical_spread    = arg4;
        double slit_center_seperation  = arg5;
        double slit_center;
        slit_center = (random_range(0,1) < 0.5) ? -slit_center_seperation/2 : +slit_center_seperation/2;
        pos_horizontal    = random_range(-slit_horizontal/2,slit_horizontal/2) + slit_center;
        pos_vertical      = random_range(-slit_vertical/2,slit_vertical/2);
        spread_horizontal = random_range(DEG2RAD(-slit_horizontal_spread/2),DEG2RAD(slit_horizontal_spread/2));
        spread_vertical   = random_range(DEG2RAD(-slit_vertical_spread/2),DEG2RAD(slit_vertical_spread/2));
    } else {
        assert(0);
    }

    // determine horizontal and vertical vectors, these vectors
    // are in the source's plane
    if (plane->n.a) {
        vect_horizontal.a = -plane->n.b / plane->n.a;
        vect_horizontal.b = 1;
        vect_horizontal.c = 0;
    } else {
        vect_horizontal.a = 1;
        vect_horizontal.b = -plane->n.a / plane->n.b;
        vect_horizontal.c = 0;
    }
    cross_product(&vect_horizontal, &plane->n, &vect_vertical);

    // determine photon_line->p
    set_vector_magnitude(&vect_horizontal, pos_horizontal);
    set_vector_magnitude(&vect_vertical, pos_vertical);
    photon_line->p = plane->p;
    point_plus_vector(&photon_line->p, &vect_horizontal, &photon_line->p);
    point_plus_vector(&photon_line->p, &vect_vertical, &photon_line->p);

    // determine photon_line->v
    set_vector_magnitude(&vect_horizontal, spread_horizontal);
    set_vector_magnitude(&vect_vertical, spread_vertical);
    photon_line->v = plane->n;
    vector_plus_vector(&photon_line->v, &vect_horizontal, &photon_line->v);
    vector_plus_vector(&photon_line->v, &vect_vertical, &photon_line->v);
}

// -----------------  MIRROR HANDLER  ------------------------------------------------ 

//DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));
//char s[100] __attribute__((unused));

static int mirror_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_intersect;

    // intersect the photon with the mirror,
    // update photon total_distance,
    // update the photon's position, and
    // add new current photon position to points array
    intersect(&photon->current, &elem->plane, &point_intersect);   
    photon->total_distance += distance(&photon->current.p, &point_intersect);
    photon->current.p = point_intersect;
    photon->points[photon->max_points++] = photon->current.p;

    // update the photon's direction for it's reflection by the mirror
    mirror_reflect(&photon->current, &elem->plane);

    // return next element
    return elem->next;
}

static int beam_splitter_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_intersect;
    double dotp;

    // intersect the photon with the mirror,
    // update photon total_distance,
    // update the photon's position, and
    // add new current photon position to points array
    intersect(&photon->current, &elem->plane, &point_intersect);   
    photon->total_distance += distance(&photon->current.p, &point_intersect);
    photon->current.p = point_intersect;
    photon->points[photon->max_points++] = photon->current.p;

    // update the photon's direction for it's reflection by the mirror
    if (random_range(0,1) < 0.5) {
        mirror_reflect(&photon->current, &elem->plane);
    }

    // if the current direction of the photon is within 90 degrees
    // of the beam splitter plane normal vector then return next, else next2
    dotp = dot_product(&photon->current.v, &elem->plane.n);
    return (dotp > 0) ? elem->next : elem->next2;
}

static void mirror_reflect(geo_line_t *line, geo_plane_t *plane)
{
    geo_point_t point_before;
    geo_point_t point_reflected;

    // create a point a little before the intesect point
    point_before.x = line->p.x - line->v.a;
    point_before.y = line->p.y - line->v.b;
    point_before.z = line->p.z - line->v.c;

    // reflect the point_before  over the mirror plane
    reflect(&point_before, plane, &point_reflected);

    // construct new path for the photon using the reflected point and 
    // the photons current position
    line->v.a = line->p.x - point_reflected.x;
    line->v.b = line->p.y - point_reflected.y;
    line->v.c = line->p.z - point_reflected.z;
}

// -----------------  SCREEN HANDLER  ------------------------------------------------ 

static int screen_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_intersect;
    int         scramp_hidx, scramp_vidx;
    double      scramp_hpos, scramp_vpos;
    char        s[100] __attribute__((unused));

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    // intersect the photon with the screen
    intersect(&photon->current, &elem->plane, &point_intersect);
    DEBUG("point_intersect = %s\n", point_str(&point_intersect,s));

    // update photon total_distance
    photon->total_distance += distance(&photon->current.p, &point_intersect);

    // determine screen coordinates of the intersect point
    determine_screen_coords(&elem->plane, &point_intersect, &scramp_hpos, &scramp_vpos);
    scramp_hidx = scramp_hpos /  SCREEN_AMP_ELEMENT_SIZE + MAX_SCREEN_AMP/2;
    scramp_vidx = scramp_vpos /  SCREEN_AMP_ELEMENT_SIZE + MAX_SCREEN_AMP/2;

    // XXX comment
    if (scramp_hidx >= 0 && scramp_hidx < MAX_SCREEN_AMP &&
        scramp_vidx >= 0 && scramp_vidx < MAX_SCREEN_AMP)
    {
        double n, angle;
        n = photon->total_distance / current_config->wavelength;
        angle = (n - floor(n)) * (2*M_PI);
        pthread_mutex_lock(&mutex);
        screen_amp1[scramp_vidx][scramp_hidx] += sin(angle);
        screen_amp2[scramp_vidx][scramp_hidx] += cos(angle);
        pthread_mutex_unlock(&mutex);
    }

    // set new photon position, and
    // add new current photon position to points array
    photon->current.p = point_intersect;
    photon->points[photon->max_points++] = photon->current.p;

    // return next element, which always equals -1 in this coase
    return elem->next;
}

static void determine_screen_coords(
                geo_plane_t *plane, geo_point_t *point_intersect, 
                double *screen_hpos, double *screen_vpos)
{
    geo_vector_t vect_intersect;
    geo_vector_t vect_horizontal;
    double magnitude_vect_intersect;
    double magnitude_vect_horizontal;
    double cos_theta;

    // XXX disable this later
    #define TEST_ENABLE

    // make vector from screen ctr point to point_intersect
    VECT_INIT(&vect_intersect,
              point_intersect->x - plane->p.x,
              point_intersect->y - plane->p.y,
              point_intersect->z - plane->p.z);

#ifdef TEST_ENABLE
    // verify vect_intersect is orthogonal to plane nrml
    double dotp;
    dotp = dot_product(&vect_intersect, &plane->n);
    if (fabs(dotp) > 1e-6) {
        ERROR("point_intersect is not on the screen - dotp = %g\n", dotp);
        *screen_hpos = 0;
        *screen_vpos = 0;
        return;
    }
#endif

    // determine the magnitude of vect_intersect, and 
    // if the magnitude is 0 then return screen coords 0,0
    magnitude_vect_intersect = magnitude(&vect_intersect);
    DEBUG("magnitude_vect_intersect = %g\n", magnitude_vect_intersect);
    if (magnitude_vect_intersect == 0) {
        *screen_hpos = 0;
        *screen_vpos = 0;
        return;
    }

    // find horizontal vector on the screen plane, 
    // this vector will // have c component equal 0
    //   Vhorizonatl DOT PlaneNrml = 0
    //   Vhorizontal.c = 0
    if (plane->n.a) {
        vect_horizontal.a = -plane->n.b / plane->n.a;
        vect_horizontal.b = 1;
        vect_horizontal.c = 0;
    } else {
        vect_horizontal.a = 1;
        vect_horizontal.b = -plane->n.a / plane->n.b;
        vect_horizontal.c = 0;
    }
    magnitude_vect_horizontal = magnitude(&vect_horizontal);

#ifdef TEST_ENABLE
    // verify vect_horizontal is orthoganl to plane nrml
    dotp = dot_product(&vect_horizontal, &plane->n);
    if (fabs(dotp) > 1e-6) {
        ERROR("vect_horizontal is not on the plane, dotp = %g\n", dotp);
        *screen_hpos = 0;
        *screen_vpos = 0;
        return;
    }
#endif

    // the dot produce of vect_horizontal and vect_intersect
    // is used to determine the angle between these two vectors
    cos_theta = dot_product(&vect_horizontal, &vect_intersect) /
                (magnitude_vect_horizontal * magnitude_vect_intersect);
    DEBUG("cos_theta = %g  theta = %g\n", cos_theta, RAD2DEG(acos(cos_theta)));

    // determine screen coords based on the cosine of the angle
    // between vect_horizontal and vect_intersect
    *screen_hpos = cos_theta * magnitude_vect_intersect;
    *screen_vpos = sqrt(square(magnitude_vect_intersect) - square(*screen_hpos));
    if (vect_intersect.c < 0) {
        *screen_vpos = -*screen_vpos;
    }
}

// -----------------  DISCARD HANDLER  ------------------------------------------------ 

static int discard_hndlr(element_t *elem, photon_t *photon)
{
    geo_point_t point_intersect;

    // intersect the photon with the discarder,
    // update photon total_distance,
    // update the photon's position, and
    // add new current photon position to points array
    intersect(&photon->current, &elem->plane, &point_intersect);   
    photon->total_distance += distance(&photon->current.p, &point_intersect);
    photon->current.p = point_intersect;
    photon->points[photon->max_points++] = photon->current.p;

    // return next element, which always equals -1 in this coase
    return elem->next;
}
