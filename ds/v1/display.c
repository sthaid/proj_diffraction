#include "common.h"

//
// defines
//

#define DEFAULT_WIN_WIDTH  1920
#define DEFAULT_WIN_HEIGHT 1080

#define FONTSZ 24

#define MAX_GRAPH                         1000
#define GRAPH_ELEMENT_SIZE                (graph_size / MAX_GRAPH)
#define SCREEN_ELEMENTS_PER_GRAPH_ELEMENT (GRAPH_ELEMENT_SIZE / SCREEN_ELEMENT_SIZE)
#define SOURCE_ELEMENT_SIZE               (GRAPH_ELEMENT_SIZE)

//
// typedefs
//

//
// variables
//

static int param_select_idx;

static double graph_size = SCREEN_SIZE;

static bool   graph_is_avail;
static double graph[MAX_GRAPH];
static int    graph_fringe_idx1;
static int    graph_fringe_idx2;
static double program_fringe_sep;
static double equation_fringe_sep;

//
// prototypes
//

static int screen_image_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static void convert_screen_inten_to_graph(param_t *p);
#ifdef ENABLE_LOGGING_AT_DEBUG_LEVEL
static char * stars(double value, int stars_max, double value_max);
#endif
static int param_select_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int param_values_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  DISPLAY_HANDLER  ------------------------------------------

void display_handler(void)
{
    int win_width, win_height;
    int screen_image_pane_width;

    // init sdl, and get actual window width and height
    win_width  = DEFAULT_WIN_WIDTH;
    win_height = DEFAULT_WIN_HEIGHT;
    if (sdl_init(&win_width, &win_height, true) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }
    INFO("REQUESTED win_width=%d win_height=%d\n", DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT);
    INFO("ACTUAL    win_width=%d win_height=%d\n", win_width, win_height);

    // call the pane manger; 
    // this will not return except when it is time to terminate the program
    screen_image_pane_width = win_width * .70;
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        100000,         // 0=continuous, -1=never, else us 
        3,              // number of pane handler varargs that follow
        screen_image_pane_hndlr, NULL, 
            0, 0, screen_image_pane_width, win_height, 
            PANE_BORDER_STYLE_MINIMAL,
        param_select_pane_hndlr, NULL, 
            screen_image_pane_width, 0, win_width-screen_image_pane_width, win_height/2, 
            PANE_BORDER_STYLE_MINIMAL,
        param_values_pane_hndlr, NULL, 
            screen_image_pane_width, win_height/2, win_width-screen_image_pane_width, win_height/2, 
            PANE_BORDER_STYLE_MINIMAL
        );
}

// -----------------  SCREEN IMAGE PANE HANDLER  --------------------------------

static int screen_image_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

   #define SDL_EVENT_ZOOM     (SDL_EVENT_USER_DEFINED + 0)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        DEBUG("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        param_t *p = &param[param_select_idx];
        int ytop, ybottom, xbase, first_graph_idx, last_graph_idx, i, y;
        double ycenter;

        // process screen_inten and set the following global vars:
        // - graph_is_avail,graph - the intensity graph to be plotted
        // - graph_fringe_idx1    - location of the highest intensity fringe
        // - graph_fringe_idx2    - location of fringe adjacent to the highest intensity fringe
        // - program_fringe_sep   - fringe separation based upon the 2 fringe indexes
        //                          described above
        // - equation_fringe_sep  - fringe seperation based upon an equation
        //
        // note that this routine really only need be called when a new param
        // has been selected, or when the graph_size changes as a result of a 
        // zoom event; however this routine takes only 3ms, so it is simple to just
        // call it every time
        convert_screen_inten_to_graph(p);

        // set local variables used to display the screen intensity graph;
        // this graph is displayed vertically on the right side of the pane
        xbase           = pane->w - 160;
        ytop            = (pane->h - MAX_GRAPH) / 2;
        ybottom         = ytop + MAX_GRAPH - 1;
        first_graph_idx = 0;
        last_graph_idx  = MAX_GRAPH-1;
        if (ytop < 0) {
            first_graph_idx -= ytop;
            ytop = 0;
        }
        if (ybottom >= pane->h) {
            last_graph_idx -= (ybottom - pane->h + 1);
            ybottom = pane->h - 1;
        }
        ycenter = (ybottom + ytop) / 2.0;

        // draw vertical reference coordinate line for the screen, and
        // draw the scale labels on this line
        sdl_render_line(pane, xbase, ytop, xbase, ybottom, WHITE);

        { int n, ylabel;
        n = (ycenter - ytop) / 100;
        ylabel = nearbyint(ycenter - 100 * n);
        for (i = 0; i <= 2*n; i++) {
            sdl_render_line(pane, xbase, ylabel, xbase+10, ylabel, WHITE);
            sdl_render_printf(
                pane, xbase+20, ylabel-sdl_font_char_height(FONTSZ)/2, FONTSZ,
                WHITE, BLACK,
                "%+0.2f mm", 
                (n-i) * 100 * GRAPH_ELEMENT_SIZE * 1e3);
            ylabel += 100;
        } }

        // if the selected param's graph is available then plot it just to the left 
        // of the vertical reference displayed by the code above
        if (graph_is_avail) {
            int    max_points = 0;
            int    graph_idx = first_graph_idx;
            static point_t points[10000];

            for (y = ytop; y <= ybottom; y++) {
                points[max_points].x = xbase-10 - graph[graph_idx] * 700;
                points[max_points].y = y;
                graph_idx++;
                max_points++;
            }

            if (graph_idx-1 != last_graph_idx) {
                FATAL("BUG graph_idx=%d last_graph_idx=%d\n", graph_idx, last_graph_idx);
            }

            sdl_render_lines(pane, points, max_points, WHITE);
        }

        // if graph fringe indexes have been located by the diffraction.c code 
        // then display horizontal red lines at the location of these 2 fringes
        if (graph_fringe_idx1 != 0 && graph_fringe_idx2 != 0) {
            y = graph_fringe_idx1 - first_graph_idx;
            sdl_render_line(pane, xbase, y, xbase-800, y, RED);
            y = graph_fringe_idx2 - first_graph_idx;
            sdl_render_line(pane, xbase, y, xbase-800, y, RED);
        }

        // draw vertical reference coordinate line for the slit source(s), and
        // draw the scale labels on this line; this will be the same length
        // as the vertical reference line drawn above for the screen
        xbase = 140;
        sdl_render_line(pane, xbase, ytop, xbase, ybottom, WHITE);

        { int n, ylabel;
        n = (ycenter - ytop) / 100;
        ylabel = nearbyint(ycenter - 100 * n);
        for (i = 0; i <= 2*n; i++) {
            sdl_render_line(pane, xbase-10, ylabel, xbase, ylabel, WHITE);
            sdl_render_printf(
                pane, 0, ylabel-sdl_font_char_height(FONTSZ)/2, FONTSZ,
                WHITE, BLACK,
                "%+0.2f mm", 
                (n-i) * 100 * SOURCE_ELEMENT_SIZE * 1e3);
            ylabel += 100;
        } }

        // draw GREEN lines to indicate the location of the slits
        for (i = 0; i < p->max_slit; i++) {
            int yslit_start, yslit_end;
            yslit_start =  nearbyint(ycenter + p->slit[i].start / SOURCE_ELEMENT_SIZE);
            yslit_end   =  nearbyint(ycenter + p->slit[i].end / SOURCE_ELEMENT_SIZE);
            for (y = yslit_start; y <= yslit_end; y++) {
                sdl_render_line(pane, xbase-5, y, xbase+5, y, GREEN);
            }
        }

        // print the selected param status_str
        sdl_render_printf(
                pane, COL2X(25,FONTSZ), pane->h-ROW2Y(4,FONTSZ), FONTSZ,
                WHITE, BLACK,
                "%s   ",
                p->status_str);
        if (program_fringe_sep > 0) {
            sdl_render_printf(
                    pane, COL2X(25,FONTSZ), pane->h-ROW2Y(3,FONTSZ), FONTSZ,
                    RED, BLACK,
                    "PROGRAM FRINGE SEP  %4.2f mm   ", program_fringe_sep * 1e3);
        }
        if (equation_fringe_sep > 0) {
            sdl_render_printf(
                    pane, COL2X(25,FONTSZ), pane->h-ROW2Y(2,FONTSZ), FONTSZ,
                    RED, BLACK,
                    "EQUATION FRINGE SEP %4.2f mm   ", equation_fringe_sep * 1e3);
        }

        // register for zoom events
        sdl_register_event(pane, pane, SDL_EVENT_ZOOM, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);
            
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        #define EPSILON (SCREEN_SIZE/100)
        #define ZOOM_STEP (SCREEN_SIZE * .1)

        switch (event->event_id) {
        case SDL_EVENT_ZOOM:
            if (event->mouse_wheel.delta_y > 0) {
                if (graph_size > ZOOM_STEP+EPSILON) {
                    graph_size -= ZOOM_STEP;
                }
            } else if (event->mouse_wheel.delta_y < 0) {
                if (graph_size < SCREEN_SIZE-EPSILON) {
                    graph_size += ZOOM_STEP;
                }
            }
            break;
        }
        return PANE_HANDLER_RET_DISPLAY_REDRAW;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        free(vars);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    assert(0);
    return PANE_HANDLER_RET_NO_ACTION;
}

static void convert_screen_inten_to_graph(param_t *p)
{
    int    i, k, max_screen, offset;
    double maximum_graph_element_value;
    double *screen_inten = p->screen_inten;

    // sanity check graph_size
    if (graph_size > SCREEN_SIZE || graph_size <= 0) {
        FATAL("graph_size=%f SCREEN_SIZE=%f\n", graph_size, SCREEN_SIZE);
    }

    // preset return values to indicate 'no-data'
    graph_is_avail = false;
    graph_fringe_idx1 = 0;
    graph_fringe_idx2 = 0;
    program_fringe_sep = 0;
    equation_fringe_sep = 0;

    // if p->screen_inten is not available then return
    if (screen_inten == NULL) {
        return;
    }

    // adjust screen_inten and MAX_SCREEN to account for graph_size
    // being less than SCREEN_SIZE; if graph_size==SCREEN_SIZE then
    // there is no adjustment; otherwise (when graph_size<SCREEN_SIZE)
    // the span of the screen_inten array is adjusted to be
    // equal to the graph_size
    offset = nearbyint(MAX_SCREEN * (1. - graph_size/SCREEN_SIZE) / 2);
    screen_inten += offset;
    max_screen = MAX_SCREEN - 2 * offset;

    // create the graph elements by averaging the screen_inten
    // elements that make up each graph element
    k = 0;
    for (i = 0; i < MAX_GRAPH; i++) {
        double sum = 0;
        int cnt = 0;
        while (k < nearbyint((i + 1) * SCREEN_ELEMENTS_PER_GRAPH_ELEMENT)) {
            if (k == max_screen) {
                FATAL("BUG MAX_GRAPH=%d max_screen=%d i=%d k=%d\n", MAX_GRAPH, max_screen, i, k);
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
        FATAL("maximum_graph_element_value == 0\n");
    }
    for (i = 0; i < MAX_GRAPH; i++) {
        graph[i] /= maximum_graph_element_value;
    }

    // debug print the graph
    for (i = 0; i < MAX_GRAPH; i++) {
        DEBUG("#%6d %6.4f - %s\n", 
              i, graph[i], stars(graph[i], 50, 1));
    }

    // set graph_is_avail flag
    graph_is_avail = true;

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

    for (i = 0; i < max_screen; i++) {
        double fudge;
        // this fudge factor is used to give slight emphasis to the fringes in the
        // center of the screen; the fudge factor ranges from a value of 1.0 at both
        // ends of the screen to a value of 1.001 at the center of the screen
        fudge = 1.0 + (double)(max_screen/2 - abs(i - max_screen/2)) / (max_screen/2) * .001;
        if (screen_inten[i]*fudge > max_inten) {
            max_inten_idx = i;
            max_inten = screen_inten[i]*fudge;
        }
    }

    if (max_inten_idx != -1) {
        if (max_inten_idx >= max_screen/2) {
            for (i = max_inten_idx-10; i >= 1; i--) {
                if (screen_inten[i] >= screen_inten[i-1] && screen_inten[i] >= screen_inten[i+1]) {
                    adjacent_fringe_idx = i;
                    break;
                }
            }
        } else {
            for (i = max_inten_idx+10; i <= max_screen-2; i++) {
                if (screen_inten[i] >= screen_inten[i-1] && screen_inten[i] >= screen_inten[i+1]) {
                    adjacent_fringe_idx = i;
                    break;
                }
            }
        }
    }

    //
    // return:
    // - the location of the 2 fringes as indexes into the graph array
    // - the fringe separation as determined by this software simulation, and
    // - the fringe separation as determined by equation
    //

    if (max_inten_idx != -1 && adjacent_fringe_idx != -1) {
        graph_fringe_idx1 = nearbyint(max_inten_idx / SCREEN_ELEMENTS_PER_GRAPH_ELEMENT);
        graph_fringe_idx2 = nearbyint(adjacent_fringe_idx / SCREEN_ELEMENTS_PER_GRAPH_ELEMENT);
    }

    if (max_inten_idx != -1 && adjacent_fringe_idx != -1) {
        program_fringe_sep = abs(max_inten_idx-adjacent_fringe_idx) * SCREEN_ELEMENT_SIZE;
    }

    if (p->max_slit == 2) {
        double slit0_center = (p->slit[0].start + p->slit[0].end) / 2.;
        double slit1_center = (p->slit[1].start + p->slit[1].end) / 2.;
        double slit_center_distance = fabs(slit0_center - slit1_center);
        equation_fringe_sep = p->distance_to_screen * p->wavelength / slit_center_distance;
    }
}

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

// -----------------  PARAM SELECT PANE HANDLER  --------------------------------

static int param_select_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;
    int i;

   #define SDL_EVENT_PARAM_SELECT (SDL_EVENT_USER_DEFINED + 0)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        DEBUG("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        // display names and register events for all params
        sdl_render_text(pane, 0, 0, FONTSZ, "SELECT ...", WHITE, BLACK);
        for (i = 0; i < max_param; i++) {
            sdl_render_text_and_register_event(
                pane, COL2X(0,FONTSZ), ROW2Y(i+2,FONTSZ), FONTSZ,
                param[i].name,
                (i != param_select_idx ? LIGHT_BLUE : WHITE), BLACK,
                SDL_EVENT_PARAM_SELECT+i,
                SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        }

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_PARAM_SELECT...SDL_EVENT_PARAM_SELECT+MAX_PARAM-1:
            // set the selected param as the active param being displayed, and
            // start the calculation of the screen image for this selected param
            param_select_idx = event->event_id - SDL_EVENT_PARAM_SELECT;
            calculate_screen_image(&param[param_select_idx]);
            DEBUG("set param_select_idx = %d\n", param_select_idx);
            break;
        }
        return PANE_HANDLER_RET_DISPLAY_REDRAW;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        free(vars);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    assert(0);
    return PANE_HANDLER_RET_NO_ACTION;
}

// -----------------  PARAM VALUES PANE HANDLER  --------------------------------

static int param_values_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        DEBUG("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        param_t *p = &param[param_select_idx];
        int i;

        // print the selected param values:
        // - name
        // - distance to screen
        // - wavelength
        // - slit info table
        sdl_render_printf(
                pane, COL2X(0,FONTSZ), ROW2Y(0,FONTSZ), FONTSZ,
                WHITE, BLACK,
                "%s", 
                p->name);
        sdl_render_printf(
                pane, COL2X(0,FONTSZ), ROW2Y(2,FONTSZ), FONTSZ,
                WHITE, BLACK,
                "distance   = %.3f m", 
                p->distance_to_screen);
        sdl_render_printf(
                pane, COL2X(0,FONTSZ), ROW2Y(3,FONTSZ), FONTSZ,
                WHITE, BLACK,
                "wavelength = %.0f nm", 
                p->wavelength*1e9);
        sdl_render_printf(
                pane, COL2X(0,FONTSZ), ROW2Y(5,FONTSZ), FONTSZ,
                WHITE, BLACK,
                " start    end  width center");
        for (i = 0; i < p->max_slit;i++) {
            double start_mm  = p->slit[i].start * 1e3;
            double end_mm    = p->slit[i].end * 1e3;
            double width_mm  = (end_mm - start_mm);
            double center_mm = (end_mm + start_mm) / 2;
            sdl_render_printf(
                    pane, COL2X(0,FONTSZ), ROW2Y(6+i,FONTSZ), FONTSZ,
                    WHITE, BLACK,
                    "%6.2f %6.2f %6.2f %6.2f",
                    start_mm, end_mm, width_mm, center_mm);
        }
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        return PANE_HANDLER_RET_DISPLAY_REDRAW;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        free(vars);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    assert(0);
    return PANE_HANDLER_RET_NO_ACTION;
}
