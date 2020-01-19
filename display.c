// XXX
// - status str text  CALCULATION COMPLETE
// - label graph
// - zoom in on graph

// XXX probably not
// - can we resize the window

#define ENABLE_LOGGING_AT_DEBUG_LEVEL

#include "common.h"

//
// defines
//

#define DEFAULT_WIN_WIDTH  1920
#define DEFAULT_WIN_HEIGHT 1080

//
// typedefs
//

//
// variables
//

static int param_select_idx;

//
// prototypes
//

static int screen_image_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int param_select_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int param_values_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  DISPLAY_HANDLER  ------------------------------------------

void display_handler(void)
{
    int win_width, win_height;

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
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        100000,         // 0=continuous, -1=never, else us 
        3,              // number of pane handler varargs that follow
        screen_image_pane_hndlr, NULL, 0, 0, win_width/2, win_height, PANE_BORDER_STYLE_MINIMAL,
        param_select_pane_hndlr, NULL, win_width/2, 0, win_width/2, win_height/2, PANE_BORDER_STYLE_MINIMAL,
        param_values_pane_hndlr, NULL, win_width/2, win_height/2, win_width/2, win_height/2, PANE_BORDER_STYLE_MINIMAL
        );
}

// -----------------  PANE HANDLERS  --------------------------------------------

static int screen_image_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;
    int fontsz = 24;

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
        params_t *p = &params[param_select_idx];
        int ytop, ybottom, xbase, first_graph_idx, last_graph_idx;

        sdl_render_printf(
                pane, COL2X(20,fontsz), pane->h-ROW2Y(1,fontsz), fontsz,
                WHITE, BLACK,
                "%s",
                p->status_str);

        xbase           = pane->w - 100;
        ytop            = (pane->h - p->max_graph) / 2;
        ybottom         = ytop + p->max_graph - 1;
        first_graph_idx = 0;
        last_graph_idx  = p->max_graph-1;
        if (ytop < 0) {
            first_graph_idx -= ytop;
            ytop = 0;
        }
        if (ybottom >= pane->h) {
            last_graph_idx -= (ybottom - pane->h + 1);
            ybottom = pane->h - 1;
        }
        INFO("y-range  %d %d  graph-range  %d %d \n", ytop, ybottom,  first_graph_idx, last_graph_idx);

        sdl_render_line(pane, xbase, ytop, xbase, ybottom, WHITE);

        // make array of graph points
        int i;
        int max_points;
        int graph_idx;
        static point_t points[10000];

        max_points = 0;
        graph_idx = first_graph_idx;
        for (i = ytop; i <= ybottom; i++) {
            points[i].x = xbase - p->graph[graph_idx] * 300;
            points[i].y = ytop + i;
            graph_idx++;
            max_points++;
        }
        INFO("max_points %d\n", max_points);

        if (graph_idx-1 != last_graph_idx) {
            FATAL("BUG graph_idx=%d last_graph_idx=%d\n", graph_idx, last_graph_idx);
        }

        sdl_render_lines(pane, points, max_points, WHITE);

        // label the graph
        int ctr, n, ylabel;
        ctr = (ybottom - ytop) / 2;
        n = (ctr - ytop) / 100;
        INFO("ctr=%d  n=%d\n", ctr, n);
        ylabel = ctr - 100 * n;
        for (i = 0; i <= 2*n; i++) {
            sdl_render_line(pane, xbase, ylabel, xbase+10, ylabel, WHITE);
            sdl_render_printf(
                pane, xbase+15, ylabel, fontsz,
                WHITE, BLACK,
                "%0.2f mm", 
                (n-i) * 100 * p->graph_element_size * 1e3);
            ylabel += 100;

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

static int param_select_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;
    int i;
    int fontsz = 24;

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
        for (i = 0; i < max_params; i++) {
            sdl_render_text_and_register_event(
                pane, COL2X(0,fontsz), ROW2Y(i,fontsz), fontsz,
                params[i].name,
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
        case SDL_EVENT_PARAM_SELECT...SDL_EVENT_PARAM_SELECT+MAX_PARAMS-1:
            param_select_idx = event->event_id - SDL_EVENT_PARAM_SELECT;
            calculate_screen_image(&params[param_select_idx]);
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

static int param_values_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;
    int fontsz = 24;  // XXX define

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
        params_t *p = &params[param_select_idx];
        int i;

        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(0,fontsz), fontsz,
                WHITE, BLACK,
                "distance = %.3f m   wavelength = %.0f nm",
                p->distance_to_screen, p->wavelength*1e9);
        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(2,fontsz), fontsz,
                WHITE, BLACK,
                " start    end  width center");
        for (i = 0; i < p->max_slit;i++) {
            double start_mm  = p->slit[i].start * 1e3;
            double end_mm    = p->slit[i].end * 1e3;
            double width_mm  = (end_mm - start_mm);
            double center_mm = (end_mm + start_mm) / 2;
            sdl_render_printf(
                    pane, COL2X(0,fontsz), ROW2Y(3+i,fontsz), fontsz,
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
