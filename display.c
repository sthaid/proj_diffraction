#include "common.h"

//
// defines
//

#define DEFAULT_WIN_WIDTH  1920
#define DEFAULT_WIN_HEIGHT 1080

#define FONTSZ 24

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

// -----------------  PANE HANDLERS  --------------------------------------------

static int screen_image_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
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
        int ytop, ybottom, xbase, first_graph_idx, last_graph_idx, i;
        double ycenter;

        // print the selected param status_str
        sdl_render_printf(
                pane, COL2X(30,FONTSZ), pane->h-ROW2Y(2,FONTSZ), FONTSZ,
                WHITE, BLACK,
                "%s",
                p->status_str);

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
        if (p->graph) {
            int    y;
            int    max_points = 0;
            int    graph_idx = first_graph_idx;
            static point_t points[10000];

            for (y = ytop; y <= ybottom; y++) {
                points[max_points].x = xbase-10 - p->graph[graph_idx] * 700;
                points[max_points].y = y;
                graph_idx++;
                max_points++;
            }

            if (graph_idx-1 != last_graph_idx) {
                FATAL("BUG graph_idx=%d last_graph_idx=%d\n", graph_idx, last_graph_idx);
            }

            sdl_render_lines(pane, points, max_points, WHITE);
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

        // draw the slits by overwriting the slit vertical reference line drawn
        // above with black lines representing the slits
        for (i = 0; i < p->max_slit; i++) {
            int yslit_start, yslit_end;
            yslit_start =  nearbyint(ycenter + p->slit[i].start / SOURCE_ELEMENT_SIZE);
            yslit_end   =  nearbyint(ycenter + p->slit[i].end / SOURCE_ELEMENT_SIZE);
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
        for (i = 0; i < max_param; i++) {
            sdl_render_text_and_register_event(
                pane, COL2X(0,FONTSZ), ROW2Y(i,FONTSZ), FONTSZ,
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
