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
        sdl_render_printf(
                pane, COL2X(0,fontsz), ROW2Y(0,fontsz), fontsz,
                WHITE, BLACK,
                "distance = %.3f m   wavelength = %.0f nm",
                p->distance_to_screen, p->wavelength*1e9);
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
