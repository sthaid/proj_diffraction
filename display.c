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

//
// prototypes
//

static int interferometer_diagram_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int interference_pattern_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int control_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  DISPLAY_INIT  ---------------------------------------------

int display_init(void)
{
    return 0;
}

// -----------------  DISPLAY_HANDLER  ------------------------------------------

void display_hndlr(void)
{
    int win_width, win_height;
    int interferometer_diagram_pane_width;
    int interference_pattern_pane_width_height;

    // init sdl, and get actual window width and height
    win_width  = DEFAULT_WIN_WIDTH;
    win_height = DEFAULT_WIN_HEIGHT;
    if (sdl_init(&win_width, &win_height, true) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }
    INFO("REQUESTED win_width=%d win_height=%d\n", DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT);
    INFO("ACTUAL    win_width=%d win_height=%d\n", win_width, win_height);

    interference_pattern_pane_width_height = 500 + 4;
    interferometer_diagram_pane_width = win_width - interference_pattern_pane_width_height;

    // call the pane manger; 
    // this will not return except when it is time to terminate the program
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        100000,         // 0=continuous, -1=never, else us
        3,              // number of pane handler varargs that follow
        interferometer_diagram_pane_hndlr, NULL, 
            0, 0, 
            interferometer_diagram_pane_width, win_height, 
            PANE_BORDER_STYLE_MINIMAL,
        interference_pattern_pane_hndlr, NULL, 
            interferometer_diagram_pane_width, 0, 
            interference_pattern_pane_width_height, interference_pattern_pane_width_height,
            PANE_BORDER_STYLE_MINIMAL,
        control_pane_hndlr, NULL, 
            interferometer_diagram_pane_width, interference_pattern_pane_width_height,
            interference_pattern_pane_width_height, win_height-interference_pattern_pane_width_height,
            PANE_BORDER_STYLE_MINIMAL
        );
}

// -----------------  INTERFEROMETER DIAGRAM PANE HANDLER  --------------------------------

static void draw_lines(rect_t *pane, geo_point_t *geo_point, int max_points, int color);
static void draw_line(rect_t *pane, geo_point_t *geo_p1, geo_point_t *geo_p2, int color);
static void transform(geo_point_t *geo_p, point_t *pixel_p);

static int x_pixel_ctr;
static int y_pixel_ctr;
static double x_mm_ctr;
static double y_mm_ctr;
static double scale_pixel_per_mm;

static int interferometer_diagram_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    #define MAX_RSP 1000

    struct {
        photon_t photons[MAX_RSP];
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

    #define SDL_EVENT_ZOOM            (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_PAN             (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_RESET_PAN_ZOOM  (SDL_EVENT_USER_DEFINED + 2)

    #define DEFAULT_X_MM_CTR             (pane->w / 2 - 10)
    #define DEFAULT_Y_MM_CTR             0
    #define DEFAULT_SCALE_PIXEL_PER_MM   1.0

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        INFO("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);

        vars = pane_cx->vars = calloc(1,sizeof(*vars));

        x_pixel_ctr = pane->w / 2;
        y_pixel_ctr = pane->h / 2;
        x_mm_ctr = DEFAULT_X_MM_CTR;
        y_mm_ctr = DEFAULT_Y_MM_CTR;
        scale_pixel_per_mm = DEFAULT_SCALE_PIXEL_PER_MM;

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        int i, j;
        struct element_s *e;
        char title_str[200];
        int max_photons;

        sprintf(title_str, "%s - %g nm", current_config->name, MM2NM(current_config->wavelength));
        sdl_render_text(pane, 
                        pane->w/2 - COL2X(strlen(title_str),FONTSZ)/2, 0, 
                        FONTSZ, title_str, WHITE, BLACK);

        for (i = 0; (e = &current_config->element[i])->hndlr; i++) {
#if 0
            cross_product(&e->plane.n, &vertical, &result);
            set_vector_magnitude(&result, 25);  // XXX should be element diameter
            point_plus_vector(&e->plane.p, &result, &p1);
            point_minus_vector(&e->plane.p, &result, &p2);
            draw_line(pane, &p1, &p2, WHITE);
#else
            geo_vector_t v_plane, v_vertical={0,0,1}, v_line;
            geo_point_t p0, p1, p2;

            v_plane = e->plane.n;
            v_plane.c = 0;

            cross_product(&v_plane, &v_vertical, &v_line);
            set_vector_magnitude(&v_line, 25);  // XXX should be element diameter

            set_vector_magnitude(&v_plane, 0.5/scale_pixel_per_mm);
            p0 = e->plane.p;
            for (j = 0; j < 15; j++) {
                point_plus_vector(&p0, &v_line, &p1);
                point_minus_vector(&p0, &v_line, &p2);
                draw_line(pane, &p1, &p2, WHITE);
                point_minus_vector(&p0, &v_plane, &p0);
            }
#endif
        }

        max_photons = MAX_RSP;  // XXX may be better if the sim just returned a malloced buffer that we free here, and tell 
                                //     this routine what the size is
        sim_get_recent_sample_photons(vars->photons, &max_photons);
        //INFO("max_photon = %d\n", max_photons);
        for (i = 0; i < max_photons; i++) {
            draw_lines(pane, vars->photons[i].points, vars->photons[i].max_points, GREEN);
        }

        sdl_register_event(pane, pane, SDL_EVENT_ZOOM, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);
        sdl_register_event(pane, pane, SDL_EVENT_PAN, SDL_EVENT_TYPE_MOUSE_MOTION, pane_cx);
        sdl_render_text_and_register_event(
                pane, pane->w-COL2X(14,FONTSZ), ROW2Y(1,FONTSZ), FONTSZ,
                "RESET_PAN_ZOOM", LIGHT_BLUE, BLACK,
                SDL_EVENT_RESET_PAN_ZOOM, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_ZOOM:
            if (event->mouse_wheel.delta_y > 0) {
                scale_pixel_per_mm *= 1.1;
            } else if (event->mouse_wheel.delta_y < 0) {
                scale_pixel_per_mm /= 1.1;
            }
            break;
        case SDL_EVENT_PAN:
            x_mm_ctr -= (event->mouse_motion.delta_x / scale_pixel_per_mm);
            y_mm_ctr += (event->mouse_motion.delta_y / scale_pixel_per_mm);
            break;
        case SDL_EVENT_RESET_PAN_ZOOM:
            x_pixel_ctr = pane->w / 2;
            y_pixel_ctr = pane->h / 2;
            x_mm_ctr = DEFAULT_X_MM_CTR;
            y_mm_ctr = DEFAULT_Y_MM_CTR;
            scale_pixel_per_mm = DEFAULT_SCALE_PIXEL_PER_MM;
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

// XXX comment about 'z' being ignored

void draw_lines(rect_t *pane, geo_point_t *geo_points, int max_points, int color)
{
    int i;
    point_t sdl_points[100];

    // XXX error if more than 100, LATER,   or loop instead of error

    for (i = 0; i < max_points; i++) {
        transform(&geo_points[i], &sdl_points[i]);
    }

    sdl_render_lines(pane, sdl_points, max_points, color);
}

void draw_line(rect_t *pane, geo_point_t *geo_p1, geo_point_t *geo_p2, int color)
{
    point_t pixel_p1, pixel_p2;

    transform(geo_p1, &pixel_p1);
    transform(geo_p2, &pixel_p2);

    sdl_render_line(pane, pixel_p1.x, pixel_p1.y, pixel_p2.x, pixel_p2.y, color);
}

void transform(geo_point_t *geo_p, point_t *pixel_p)
{
    pixel_p->x = nearbyint(x_pixel_ctr + (geo_p->x - x_mm_ctr) * scale_pixel_per_mm);
    pixel_p->y = nearbyint(y_pixel_ctr - (geo_p->y - y_mm_ctr) * scale_pixel_per_mm);
}

// -----------------  INTERFEROMETER PATTERN PANE HANDLER  ----------------------

static int interference_pattern_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
        texture_t texture;
        unsigned int *pixels;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

   #define SDL_EVENT_XXX    (SDL_EVENT_USER_DEFINED + 0)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        INFO("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        double *screen;
        int     max_screen;
        double  screen_width_and_height;
        int     texture_w, texture_h;
        int     i;

        // XXX put a scale on the graph 
        //     empahsize lower intensities
        //     pan and zoom

        // get the screen data
        sim_get_screen(&screen, &max_screen, &screen_width_and_height);
        DEBUG("screen=%p  max_screen=%d  screen_width_and_height=%g mm\n",
            screen, max_screen, screen_width_and_height);

        // if texture is not allocated then 
        // allocate the texture and pixels
        if (vars->texture == NULL) {
            vars->texture = sdl_create_texture(max_screen, max_screen);
            vars->pixels = calloc(max_screen*max_screen, sizeof(int));
        }

        // ensure max_screen hasn't changed
        sdl_query_texture(vars->texture, &texture_w, &texture_h);
        assert(texture_w == max_screen && texture_h == max_screen);

        // initialize pixels from screen data,
        // and update the texture with the new pixel values
        for (i = 0; i < max_screen*max_screen; i++) {
            vars->pixels[i] = ((int)(screen[i] * 255.99) << 8) | (0xff << 24);
        }
        sdl_update_texture(vars->texture, (unsigned char*)vars->pixels, max_screen*sizeof(int));

        // render
#if 1
        sdl_render_texture(pane, 0, 0, vars->texture);
#else
        rect_t loc;
        loc.x = loc.y = 0;
        loc.w = loc.h = 500;
        sdl_render_scaled_texture(pane, &loc, vars->texture);
#endif

        // free screen
        free(screen);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_XXX:
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

// -----------------  CONTROL PANE HANDLER  --------------------------------------

static int control_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

   #define SDL_EVENT_XXX    (SDL_EVENT_USER_DEFINED + 0)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        INFO("PANE x,y,w,h  %d %d %d %d\n",
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
        switch (event->event_id) {
        case SDL_EVENT_XXX:
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
