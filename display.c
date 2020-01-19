// XXX
//#define ENABLE_LOGGING_AT_DEBUG_LEVEL

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
int xxx = win_width * .70;
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        100000,         // 0=continuous, -1=never, else us 
        3,              // number of pane handler varargs that follow
        screen_image_pane_hndlr, NULL, 0, 0, xxx, win_height, PANE_BORDER_STYLE_MINIMAL,
        param_select_pane_hndlr, NULL, xxx, 0, win_width-xxx, win_height/2, PANE_BORDER_STYLE_MINIMAL,
        param_values_pane_hndlr, NULL, xxx, win_height/2, win_width-xxx, win_height/2, PANE_BORDER_STYLE_MINIMAL
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
        param_t *p = &param[param_select_idx];
        int ytop, ybottom, xbase, first_graph_idx, last_graph_idx;

        sdl_render_printf(
                pane, COL2X(20,fontsz), pane->h-ROW2Y(1,fontsz), fontsz,
                WHITE, BLACK,
                "%s",
                p->status_str);

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
        //INFO("y-range  %d %d  graph-range  %d %d \n", ytop, ybottom,  first_graph_idx, last_graph_idx);

        sdl_render_line(pane, xbase, ytop, xbase, ybottom, WHITE);

        // make array of graph points
        if (p->graph) {
            int y;
            int max_points;
            int graph_idx;
            static point_t points[10000];

            max_points = 0;
            graph_idx = first_graph_idx;
            for (y = ytop; y <= ybottom; y++) {
                points[max_points].x = xbase-10 - p->graph[graph_idx] * 700; //XXX AAA
                points[max_points].y = y;
                graph_idx++;
                max_points++;
            }
            //INFO("max_points %d\n", max_points);

            if (graph_idx-1 != last_graph_idx) {
                FATAL("BUG graph_idx=%d last_graph_idx=%d\n", graph_idx, last_graph_idx);
            }

            sdl_render_lines(pane, points, max_points, WHITE);
        }

        // XXX test line in ctr

        // label the graph
        int ctr, n, ylabel, i;
        ctr = (ybottom + ytop) / 2;
        //XXX del sdl_render_line(pane, xbase-100, ctr, xbase, ctr, RED);
        n = (ctr - ytop) / 100;
        //INFO("ctr=%d  n=%d\n", ctr, n);
        ylabel = ctr - 100 * n;
        for (i = 0; i <= 2*n; i++) {
            sdl_render_line(pane, xbase, ylabel, xbase+10, ylabel, WHITE);
            sdl_render_printf(
                pane, xbase+20, ylabel-sdl_font_char_height(fontsz)/2, fontsz,
                WHITE, BLACK,
                "%+0.2f mm", 
                (n-i) * 100 * GRAPH_ELEMENT_SIZE * 1e3);
            ylabel += 100;
        }

        // AAAAAAAAAAAAA
//AAA
        xbase = 140;
        sdl_render_line(pane, xbase, ytop, xbase, ybottom, WHITE);

        // label the graph
        //int ctr, n, ylabel, i;
        ctr = (ybottom + ytop) / 2;
        //XXX del sdl_render_line(pane, xbase-100, ctr, xbase, ctr, RED);
        n = (ctr - ytop) / 100;
        //INFO("ctr=%d  n=%d\n", ctr, n);
        ylabel = ctr - 100 * n;
        for (i = 0; i <= 2*n; i++) {
            sdl_render_line(pane, xbase-10, ylabel, xbase, ylabel, WHITE);
            sdl_render_printf(
                pane, 0, ylabel-sdl_font_char_height(fontsz)/2, fontsz,
                WHITE, BLACK,
                "%+0.2f mm", 
                (n-i) * 100 * GRAPH_ELEMENT_SIZE/10 * 1e3);  // XXX define for 10
            ylabel += 100;
        }


        for (i = 0; i < p->max_slit; i++) {
            int yslit_start, yslit_end;
            yslit_start =  (ytop + ybottom) / 2 + p->slit[i].start / (GRAPH_ELEMENT_SIZE/10);
            yslit_end   =  (ytop + ybottom) / 2 + p->slit[i].end / (GRAPH_ELEMENT_SIZE/10);
            sdl_render_line(pane, xbase, yslit_start, xbase, yslit_end, BLACK);
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
        for (i = 0; i < max_param; i++) {
            sdl_render_text_and_register_event(
                pane, COL2X(0,fontsz), ROW2Y(i,fontsz), fontsz,
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
        param_t *p = &param[param_select_idx];
        int i;

        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(0,fontsz), fontsz,
                WHITE, BLACK,
                "%s", 
                p->name);
        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(2,fontsz), fontsz,
                WHITE, BLACK,
                "distance   = %.3f m", 
                p->distance_to_screen);
        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(3,fontsz), fontsz,
                WHITE, BLACK,
                "wavelength = %.0f nm", 
                p->wavelength*1e9);
        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(5,fontsz), fontsz,
                WHITE, BLACK,
                " start    end  width center");
        for (i = 0; i < p->max_slit;i++) {
            double start_mm  = p->slit[i].start * 1e3;
            double end_mm    = p->slit[i].end * 1e3;
            double width_mm  = (end_mm - start_mm);
            double center_mm = (end_mm + start_mm) / 2;
            sdl_render_printf(
                    pane, COL2X(0,fontsz), ROW2Y(6+i,fontsz), fontsz,
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
