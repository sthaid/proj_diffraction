// XXX try to display last photon ray trace when stopped

#include "common.h"

//
// defines
//

#define DEFAULT_WIN_WIDTH  1920
#define DEFAULT_WIN_HEIGHT 1080

#define NM2MM(x) ((x) * 1e-6)
#define MM2NM(x) ((x) * 1e6)

#define LARGE_FONT 24
#define SMALL_FONT 16 

//
// typedefs
//

//
// variables
//

static struct element_s *selected_elem;

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
    int interference_pattern_pane_width;
    int interference_pattern_pane_height;

    // init sdl, and get actual window width and height
    win_width  = DEFAULT_WIN_WIDTH;
    win_height = DEFAULT_WIN_HEIGHT;
    if (sdl_init(&win_width, &win_height, true) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }
    INFO("REQUESTED win_width=%d win_height=%d\n", DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT);
    INFO("ACTUAL    win_width=%d win_height=%d\n", win_width, win_height);

    interference_pattern_pane_width = MAX_SCREEN + 4;
    interference_pattern_pane_height = (MAX_SCREEN+121) + 4;
    interferometer_diagram_pane_width = win_width - interference_pattern_pane_width;

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
            interference_pattern_pane_width, interference_pattern_pane_height,
            PANE_BORDER_STYLE_MINIMAL,
        control_pane_hndlr, NULL, 
            interferometer_diagram_pane_width, interference_pattern_pane_height,
            interference_pattern_pane_width, win_height-interference_pattern_pane_height,
            PANE_BORDER_STYLE_MINIMAL
        );
}

// -----------------  INTERFEROMETER DIAGRAM PANE HANDLER  --------------------------------

static void reset_pan_and_zoom(rect_t *pane);
static void draw_lines(rect_t *pane, geo_point_t *geo_point, int max_points, int color);
static void draw_line(rect_t *pane, geo_point_t *geo_p1, geo_point_t *geo_p2, int color);
static void transform(geo_point_t *geo_p, point_t *pixel_p);
static void draw_optical_element(rect_t *pane, struct element_s *elem, int color);

static int x_pixel_ctr;
static int y_pixel_ctr;
static double x_mm_ctr;
static double y_mm_ctr;
static double scale_pixel_per_mm;

static int interferometer_diagram_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int none;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

    #define SDL_EVENT_ZOOM          (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_PAN           (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_SIM_RUN       (SDL_EVENT_USER_DEFINED + 2)
    #define SDL_EVENT_SIM_STOP      (SDL_EVENT_USER_DEFINED + 3)
    #define SDL_EVENT_RESET         (SDL_EVENT_USER_DEFINED + 4)
    #define SDL_EVENT_RANDOMIZE     (SDL_EVENT_USER_DEFINED + 5)
    #define SDL_EVENT_SELECT_ELEM   (SDL_EVENT_USER_DEFINED + 10)

    #define ELEM_TYPE_TO_COLOR(et)  \
        ((et) == ELEM_TYPE_SRC_SS   ? YELLOW     : \
         (et) == ELEM_TYPE_SRC_DS   ? YELLOW     : \
         (et) == ELEM_TYPE_SRC_RH   ? YELLOW     : \
         (et) == ELEM_TYPE_MIRROR   ? BLUE       : \
         (et) == ELEM_TYPE_BEAM_SPL ? LIGHT_BLUE : \
         (et) == ELEM_TYPE_SCREEN   ? PINK       : \
         (et) == ELEM_TYPE_DISCARD  ? RED        : \
                                      WHITE)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        INFO("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        reset_pan_and_zoom(pane);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        int i;
        char title_str[500], state_str[100];
        photon_t *photons;
        int max_photons;
        bool running;
        double rate;

        // display title
        sim_get_state(&running, &rate);
        if (running) {
            sprintf(state_str, "RUNNING %0.1f M /s", rate/1e6);
        } else {
            sprintf(state_str, "STOPPED");
        }
        sprintf(title_str, "%s - %g nm - %s", 
                current_config->name, 
                MM2NM(current_config->wavelength),
                state_str);
        sdl_render_text(pane, 
                        pane->w/2 - COL2X(strlen(title_str),LARGE_FONT)/2, 0, 
                        LARGE_FONT, title_str, WHITE, BLACK);

        // get and display the ray trace for the recent sample photons
        sim_get_recent_sample_photons(&photons, &max_photons);
        for (i = 0; i < max_photons; i++) {
            draw_lines(pane, photons[i].points, photons[i].max_points, GREEN);
        }
        free(photons);

        // display optical elements: source, mirrors. beamsplitters, etc.
        for (i = 0; i < current_config->max_element; i++) {
            struct element_s *elem = &current_config->element[i];
            int color;

            if (elem->type == ELEM_TYPE_MIRROR && (elem->flags & ELEM_MIRROR_FLAG_MASK_DISCARD)) {
                color = RED;
            } else {
                color = ELEM_TYPE_TO_COLOR(elem->type);
            }

            if ((elem == selected_elem) && ((microsec_timer()/500000) & 1)) {
                color = ORANGE;
            }

            draw_optical_element(pane, &current_config->element[i], color);
        }

        // display element offset / info table
        sdl_render_printf(pane, 0, ROW2Y(2,LARGE_FONT), LARGE_FONT, WHITE, BLACK,
                          "    NAME ID       X       Y     PAN    TILT FLAG");
        for (i = 0; i < current_config->max_element; i++) {
            struct element_s *elem = &current_config->element[i];
            char flags_str[10]={0}, *p=flags_str;
            int j;

            sdl_render_printf(pane, 0, ROW2Y(3+i,LARGE_FONT), LARGE_FONT, 
                              ELEM_TYPE_TO_COLOR(elem->type), BLACK,
                              "%8s", elem->type_str);

            for (j = 0; j < elem->max_flags; j++) {
                p += sprintf(p, "%d", ((elem->flags >> (elem->max_flags-1-j)) & 1));
            }
            sdl_render_printf(pane, COL2X(10,LARGE_FONT), ROW2Y(3+i,LARGE_FONT), LARGE_FONT, 
                              elem == selected_elem ? ORANGE : WHITE, BLACK,
                              "%2d %7.3f %7.3f %7.3f %7.3f %4s",
                              i, 
                              elem->x_offset, elem->y_offset, 
                              RAD2DEG(elem->pan_offset), RAD2DEG(elem->tilt_offset),
                              flags_str);
        }

        // register for events ...
        // - pan and zoom the optical element and ray trace diagram
        sdl_register_event(pane, pane, SDL_EVENT_ZOOM, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);
        sdl_register_event(pane, pane, SDL_EVENT_PAN, SDL_EVENT_TYPE_MOUSE_MOTION, pane_cx);
        // - sim run/stop events
        if (running) {
            sdl_render_text_and_register_event(
                    pane, pane->w-COL2X(14,LARGE_FONT), ROW2Y(1,LARGE_FONT), LARGE_FONT,
                    "STOP", LIGHT_BLUE, BLACK,
                    SDL_EVENT_SIM_STOP, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        } else {
            sdl_render_text_and_register_event(
                    pane, pane->w-COL2X(14,LARGE_FONT), ROW2Y(1,LARGE_FONT), LARGE_FONT,
                    "RUN", LIGHT_BLUE, BLACK,
                    SDL_EVENT_SIM_RUN, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        }
        // - reset event: resets element offsets and diagram pan/zoom
        sdl_render_text_and_register_event(
                pane, pane->w-COL2X(14,LARGE_FONT), ROW2Y(2,LARGE_FONT), LARGE_FONT,
                "RESET", LIGHT_BLUE, BLACK,
                SDL_EVENT_RESET, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        // - randomize event: randomize element position and angle offsets
        sdl_render_text_and_register_event(
                pane, pane->w-COL2X(14,LARGE_FONT), ROW2Y(3,LARGE_FONT), LARGE_FONT,
                "RANDOMIZE", LIGHT_BLUE, BLACK,
                SDL_EVENT_RANDOMIZE, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        // - element select events; both click on the element or click on the offset display line
        for (i = 0; i < current_config->max_element; i++) {
            struct element_s *elem = &current_config->element[i];
            point_t p;
            rect_t loc;

            transform(&elem->plane.p, &p);
            loc.x = p.x-20;
            loc.y = p.y-20;
            loc.w = 40;
            loc.h = 40;
            sdl_register_event(pane, &loc, SDL_EVENT_SELECT_ELEM+i, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

            loc.x = 0;
            loc.y = ROW2Y(3+i,LARGE_FONT) + sdl_font_char_height(LARGE_FONT)/3;  //xxx why the "+ ..."
            loc.w = ROW2Y(46,LARGE_FONT);
            loc.h = COL2X(1,LARGE_FONT);
            sdl_register_event(pane, &loc, SDL_EVENT_SELECT_ELEM+i, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        }

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_ZOOM:
            if (event->mouse_wheel.delta_y > 0) {
                scale_pixel_per_mm *= 1.2;
            } else if (event->mouse_wheel.delta_y < 0) {
                scale_pixel_per_mm /= 1.2;
            }
            break;
        case SDL_EVENT_PAN:
            x_mm_ctr -= (event->mouse_motion.delta_x / scale_pixel_per_mm);
            y_mm_ctr += (event->mouse_motion.delta_y / scale_pixel_per_mm);
            break;
        case SDL_EVENT_SIM_RUN:
            sim_run();
            break;
        case SDL_EVENT_SIM_STOP:
            sim_stop();
            break;
        case SDL_EVENT_RESET:
            reset_pan_and_zoom(pane);
            sim_reset_all_elements(current_config);
            break;
        case SDL_EVENT_RANDOMIZE:
            sim_randomize_all_elements(current_config, 2, DEG2RAD(.5729578));
            break;
        case 'R':
            sim_randomize_element(selected_elem, 2, DEG2RAD(.5729578));
            break;
        case SDL_EVENT_SELECT_ELEM ... SDL_EVENT_SELECT_ELEM+MAX_CONFIG_ELEMENT-1: {
            int idx = event->event_id - SDL_EVENT_SELECT_ELEM;
            if (idx >= current_config->max_element) {
                selected_elem = NULL;
                break;
            }
            selected_elem = &current_config->element[idx];
            INFO("XXX SEL %d\n", idx);
            break; }
        case SDL_EVENT_KEY_ESC:
            selected_elem = NULL;
            break;
        case SDL_EVENT_KEY_INSERT:
        case SDL_EVENT_KEY_HOME:
        case SDL_EVENT_KEY_INSERT + SDL_EVENT_KEY_CTRL:
        case SDL_EVENT_KEY_HOME + SDL_EVENT_KEY_CTRL: {
            bool   is_insert = (event->event_id & ~SDL_EVENT_KEY_CTRL) == SDL_EVENT_KEY_INSERT;
            bool   is_ctrl   = (event->event_id & SDL_EVENT_KEY_CTRL);
            double amount    = (is_insert ? -1 : 1) * (!is_ctrl ? .01 : .001);
            sim_adjust_element_pan(selected_elem, DEG2RAD(amount));
            break; }
        case SDL_EVENT_KEY_PGUP:
        case SDL_EVENT_KEY_PGDN:
        case SDL_EVENT_KEY_PGUP + SDL_EVENT_KEY_CTRL:
        case SDL_EVENT_KEY_PGDN + SDL_EVENT_KEY_CTRL: {
            bool   is_pgdn   = (event->event_id & ~SDL_EVENT_KEY_CTRL) == SDL_EVENT_KEY_PGDN;
            bool   is_ctrl   = (event->event_id & SDL_EVENT_KEY_CTRL);
            double amount    = (is_pgdn ? -1 : 1) * (!is_ctrl ? .01 : .001);
            sim_adjust_element_tilt( selected_elem, DEG2RAD(amount));
            break; }
        case SDL_EVENT_KEY_UP_ARROW:
        case SDL_EVENT_KEY_DOWN_ARROW:
        case SDL_EVENT_KEY_UP_ARROW + SDL_EVENT_KEY_CTRL:
        case SDL_EVENT_KEY_DOWN_ARROW + SDL_EVENT_KEY_CTRL: {
            bool   is_downarrow = (event->event_id & ~SDL_EVENT_KEY_CTRL) == SDL_EVENT_KEY_DOWN_ARROW;
            bool   is_ctrl      = (event->event_id & SDL_EVENT_KEY_CTRL);
            double amount       = (is_downarrow ? -1 : 1) * (!is_ctrl ? .1 : .01);
            sim_adjust_element_y(selected_elem, amount);
            break; }
        case SDL_EVENT_KEY_LEFT_ARROW:
        case SDL_EVENT_KEY_RIGHT_ARROW:
        case SDL_EVENT_KEY_LEFT_ARROW + SDL_EVENT_KEY_CTRL:
        case SDL_EVENT_KEY_RIGHT_ARROW + SDL_EVENT_KEY_CTRL: {
            bool   is_leftarrow = (event->event_id & ~SDL_EVENT_KEY_CTRL) == SDL_EVENT_KEY_LEFT_ARROW;
            bool   is_ctrl      = (event->event_id & SDL_EVENT_KEY_CTRL);
            double amount       = (is_leftarrow ? -1 : 1) * (!is_ctrl ? .1 : .01);
            sim_adjust_element_x(selected_elem, amount);
            break; }
        case 'r':
            sim_reset_element(selected_elem);
            break;
        case '0' ... '9':
            sim_toggle_element_flag(selected_elem, event->event_id - '0');
            break;
        }

        return PANE_HANDLER_RET_NO_ACTION;
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

static void reset_pan_and_zoom(rect_t *pane) 
{
    x_pixel_ctr        = pane->w / 2;
    y_pixel_ctr        = pane->h / 2;
    scale_pixel_per_mm = 0.5;
    x_mm_ctr           = (x_pixel_ctr - 250) / scale_pixel_per_mm;
    y_mm_ctr           = (y_pixel_ctr - 250) / scale_pixel_per_mm;
}

// XXX comment about 'z' being ignored
static void draw_lines(rect_t *pane, geo_point_t *geo_points, int max_points, int color)
{
    int i;
    point_t sdl_points[100];

    assert(max_points <= 100);
    for (i = 0; i < max_points; i++) {
        transform(&geo_points[i], &sdl_points[i]);
    }

    sdl_render_lines(pane, sdl_points, max_points, color);
}

static void draw_line(rect_t *pane, geo_point_t *geo_p1, geo_point_t *geo_p2, int color)
{
    point_t pixel_p1, pixel_p2;

    transform(geo_p1, &pixel_p1);
    transform(geo_p2, &pixel_p2);

    sdl_render_line(pane, pixel_p1.x, pixel_p1.y, pixel_p2.x, pixel_p2.y, color);
}

static void transform(geo_point_t *geo_p, point_t *pixel_p)
{
    pixel_p->x = nearbyint(x_pixel_ctr + (geo_p->x - x_mm_ctr) * scale_pixel_per_mm);
    pixel_p->y = nearbyint(y_pixel_ctr - (geo_p->y - y_mm_ctr) * scale_pixel_per_mm);
}

static void draw_optical_element(rect_t *pane, struct element_s *elem, int color)
{
    geo_vector_t v_plane, v_vertical={0,0,1}, v_line;
    geo_point_t p0, p1, p2;
    int i;

    v_plane = elem->plane.n;
    v_plane.c = 0;

    cross_product(&v_plane, &v_vertical, &v_line);
    set_vector_magnitude(&v_line, 25);

    set_vector_magnitude(&v_plane, 0.10/scale_pixel_per_mm);
    p0 = elem->plane.p;
    for (i = 0; i < 50 ; i++) {
        point_plus_vector(&p0, &v_line, &p1);
        point_minus_vector(&p0, &v_line, &p2);
        draw_line(pane, &p1, &p2, color);
        point_minus_vector(&p0, &v_plane, &p0);
    }
}

// -----------------  INTERFEROMETER PATTERN PANE HANDLER  ----------------------

static void render_interference_screen(
                int y_top, int y_span, double screen[MAX_SCREEN][MAX_SCREEN], 
                texture_t texture, unsigned int pixels[MAX_SCREEN][MAX_SCREEN], rect_t *pane);
static void render_intensity_graph(
                int y_top, int y_span, double screen[MAX_SCREEN][MAX_SCREEN],
                double sensor_width, double sensor_height, rect_t *pane);
static void render_scale(
                int y_top, int y_span, rect_t *pane);

static int interference_pattern_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        texture_t texture;
        unsigned int pixels[MAX_SCREEN][MAX_SCREEN];
        double sensor_width;
        double sensor_height;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

   #define SDL_EVENT_SENSOR_WIDTH    (SDL_EVENT_USER_DEFINED + 0)
   #define SDL_EVENT_SENSOR_HEIGHT   (SDL_EVENT_USER_DEFINED + 1)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        vars = pane_cx->vars = calloc(1,sizeof(*vars));
        vars->texture = sdl_create_texture(MAX_SCREEN, MAX_SCREEN);
        vars->sensor_width = 1.0;  // mm
        vars->sensor_height = 1.0;  // mm
        INFO("PANE x,y,w,h  %d %d %d %d\n",
            pane->x, pane->y, pane->w, pane->h);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        double screen[MAX_SCREEN][MAX_SCREEN];
        char   sensor_width_str[20], sensor_height_str[20];

        // this pane is vertically arranged as follows
        // y-range  y-span  description 
        // 0-499    500     interference screen
        // 500-500  1       horizontal line
        // 501-600  100     intensity graph
        // 601-620  20      scale in mm
        //          ----
        //          621

        // get the screen data
        sim_get_screen(screen);

        // render the sections that make up this pane, as described above
        render_interference_screen(0, MAX_SCREEN, screen, vars->texture, vars->pixels, pane);
        sdl_render_line(pane, 0,MAX_SCREEN, MAX_SCREEN-1,MAX_SCREEN, WHITE);
        render_intensity_graph(MAX_SCREEN+1, 100, screen, vars->sensor_width, vars->sensor_height, pane);
        render_scale(MAX_SCREEN+101, 20, pane);

        // register for events to adjust the simulated sensor's width and height
        sprintf(sensor_width_str, "W=%3.1f", vars->sensor_width);
        sdl_render_text_and_register_event(
                pane, 0, MAX_SCREEN+1, LARGE_FONT,
                sensor_width_str, LIGHT_BLUE, BLACK,
                SDL_EVENT_SENSOR_WIDTH, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(sensor_height_str, "H=%3.1f", vars->sensor_height);
        sdl_render_text_and_register_event(
                pane, COL2X(6,LARGE_FONT), MAX_SCREEN+1, LARGE_FONT,
                sensor_height_str, LIGHT_BLUE, BLACK,
                SDL_EVENT_SENSOR_HEIGHT, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_SENSOR_WIDTH:
            if (event->mouse_wheel.delta_y > 0) vars->sensor_width += .1;
            if (event->mouse_wheel.delta_y < 0) vars->sensor_width -= .1;
            if (vars->sensor_width < 0.1) vars->sensor_width = 0.1;
            if (vars->sensor_width > 5.0) vars->sensor_width = 5.0;
            break;
        case SDL_EVENT_SENSOR_HEIGHT:
            if (event->mouse_wheel.delta_y > 0) vars->sensor_height += .1;
            if (event->mouse_wheel.delta_y < 0) vars->sensor_height -= .1;
            if (vars->sensor_height < 0.1) vars->sensor_height = 0.1;
            if (vars->sensor_height > 5.0) vars->sensor_height = 5.0;
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

static void render_interference_screen(
                int y_top, int y_span, double screen[MAX_SCREEN][MAX_SCREEN], 
                texture_t texture, unsigned int pixels[MAX_SCREEN][MAX_SCREEN], rect_t *pane)
{
    int i,j;
    int texture_width, texture_height;

    // initialize pixels from screen data,
    // and update the texture with the new pixel values
    for (i = 0; i < MAX_SCREEN; i++) {
        for (j = 0; j < MAX_SCREEN; j++) {
            int green = 255.99 * screen[i][j];
            if (green < 0 || green > 255) {
                ERROR("green %d\n", green);
                green = (green < 0 ? 0 : 255);
            }
            pixels[i][j] = (green << 8) | (0xff << 24);
        }
    }
    sdl_update_texture(texture, (unsigned char*)pixels, MAX_SCREEN*sizeof(int));

    // render
    sdl_query_texture(texture, &texture_width, &texture_height);
    assert(y_span == texture_height);
    sdl_render_texture(pane, 0, y_top, texture);
}

static void render_intensity_graph(
                int y_top, int y_span, double screen[MAX_SCREEN][MAX_SCREEN],
                double sensor_width, double sensor_height, rect_t *pane)
{
    int i,j,k;
    int sensor_width_pixels, sensor_height_pixels;
    int sensor_min_x, sensor_max_x, sensor_min_y, sensor_max_y;
    point_t graph[MAX_SCREEN];
    int y_bottom = y_top + y_span - 1;

    // convert caller's sensor dimensions (in mm) to pixels
    sensor_width_pixels = sensor_width / SCREEN_ELEMENT_SIZE;
    sensor_height_pixels = sensor_height / SCREEN_ELEMENT_SIZE;

    // loop across the center of screen
    for (k = 0; k < MAX_SCREEN; k++) {
        double sum, sensor_value;
        int cnt;

        // determine sensor location for indexing into the screen array;
        // these min/max values are used to compute an average sensor_value
        // for different size sensors
        sensor_min_x = k - sensor_width_pixels/2;
        sensor_max_x = sensor_min_x + sensor_width_pixels - 1;
        sensor_min_y = MAX_SCREEN/2 - sensor_height_pixels/2;
        sensor_max_y = sensor_min_y + sensor_height_pixels - 1;

        // if the sensor min_x or max_x is off the screen then 
        // limit the min_x or max_x value to the screen boundary
        if (sensor_min_x < 0) sensor_min_x = 0;
        if (sensor_max_x > MAX_SCREEN-1) sensor_max_x = MAX_SCREEN-1;

        // compute the average sensor value
        sum = 0;
        cnt = 0;
        for (j = sensor_min_y; j <= sensor_max_y; j++) {
            for (i = sensor_min_x; i <= sensor_max_x; i++) {
                sum += screen[j][i];
                cnt++;
            }
        }
        sensor_value = cnt > 0 ? sum/cnt : 0;

        // add the sensor value to the array of points that this 
        // routine will render below
        graph[k].x = k;
        graph[k].y = y_bottom - sensor_value * y_span;
    }

    // render the graph
    sdl_render_lines(pane, graph, MAX_SCREEN, WHITE);

    // draw horizontal lines accross middle to represent the top and bottom of the sensor
    sensor_min_y = MAX_SCREEN/2 - sensor_height_pixels/2;
    sensor_max_y = sensor_min_y + sensor_height_pixels - 1;
    sdl_render_line(pane, 0, sensor_min_y, MAX_SCREEN-1, sensor_min_y, WHITE);
    sdl_render_line(pane, 0, sensor_max_y, MAX_SCREEN-1, sensor_max_y, WHITE);

#if 0
    // draw vertical lines accross middle to represent the left and right of the 
    // sensor, when the sensor is positioned in the center
    k = MAX_SCREEN/2;
    sensor_min_x = k - sensor_width_pixels/2;
    sensor_max_x = sensor_min_x + sensor_width_pixels - 1;
    sdl_render_line(pane, sensor_min_x, sensor_min_y-10, sensor_min_x, sensor_max_y+10, WHITE);
    sdl_render_line(pane, sensor_max_x, sensor_min_y-10, sensor_max_x, sensor_max_y+10, WHITE);
#endif
}

static void render_scale(int y_top, int y_span, rect_t *pane)
{
    double tick_intvl, tick_loc;
    char tick_value_str[20];
    int tick_value, tick_value_loc, slen;
    char screen_diam_str[50];

    // convert 5mm to tick_intvl in pixel units
    tick_intvl = 5 / SCREEN_ELEMENT_SIZE;

    // starting at the center, adjust tick_loc to the left to 
    // find the first tick location
    tick_loc = MAX_SCREEN / 2;
    while (tick_loc >= tick_intvl) {
        tick_loc -= tick_intvl;
    }

    // render the ticks and the tick value strings
    for (; tick_loc < MAX_SCREEN; tick_loc += tick_intvl) {
        // if tick location is too close to either side then skip it
        if (tick_loc < 5 || tick_loc >= MAX_SCREEN-5) continue;

        // render the tick
        sdl_render_line(pane, nearbyint(tick_loc), y_top, nearbyint(tick_loc), y_top+5, WHITE);

        // determine the tick value (in mm), and display it centered below the tick
        // - determine the tick_value based on the difference between tick_loc and the center
        tick_value = nearbyint((tick_loc - MAX_SCREEN/2) * SCREEN_ELEMENT_SIZE);
        sprintf(tick_value_str, "%d", tick_value);
        // - determine the tick_value_loc, accounting for the string length of the tick_value_str
        slen = strlen(tick_value_str);
        if (tick_value < 0) slen++;
        tick_value_loc = nearbyint(tick_loc - COL2X(slen,SMALL_FONT) / 2.);
        // - render the tick value
        sdl_render_text(pane, 
                        tick_value_loc, y_top+5,
                        SMALL_FONT, tick_value_str, WHITE, BLACK);
    }

    // render the axis line
    sdl_render_line(pane, 0,y_top, MAX_SCREEN-1,y_top, WHITE);

    // display the screen diameter at the top of the pane
    sprintf(screen_diam_str, "SCREEN DIAMETER %g mm", MAX_SCREEN*SCREEN_ELEMENT_SIZE);
    sdl_render_text(pane, 
                    (pane->w-COL2X(strlen(screen_diam_str),LARGE_FONT))/2, 0, 
                    LARGE_FONT, screen_diam_str, WHITE, BLACK);
}

// -----------------  CONTROL PANE HANDLER  --------------------------------------

static int control_pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    struct {
        int cfgsel_start;
    } * vars = pane_cx->vars;
    rect_t * pane = &pane_cx->pane;

   #define SDL_EVENT_SIM_MOUSE_WHEEL  (SDL_EVENT_USER_DEFINED + 0)
   #define SDL_EVENT_SIM_CFGSEL       (SDL_EVENT_USER_DEFINED + 10)

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
        int row, i;

        // register for events ...

        // - config select
        for (row = 0, i = vars->cfgsel_start; i < max_config; i++, row++) {
            if (ROW2Y(row,LARGE_FONT) > pane->h-sdl_font_char_height(LARGE_FONT)) {
                break;
            }
            sdl_render_text_and_register_event(
                    pane, 0, ROW2Y(row,LARGE_FONT), LARGE_FONT,
                    config[i].name, 
                    current_config == &config[i] ? GREEN : LIGHT_BLUE, BLACK,
                    SDL_EVENT_SIM_CFGSEL+i, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        }

        // - config select scrooling
        rect_t loc = {0,0,pane->w,pane->h};
        sdl_register_event(pane, &loc, SDL_EVENT_SIM_MOUSE_WHEEL, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_SIM_CFGSEL ... SDL_EVENT_SIM_CFGSEL+MAX_CONFIG-1: {
            selected_elem = NULL;
            sim_select_config(event->event_id - SDL_EVENT_SIM_CFGSEL);
            break; }
        case SDL_EVENT_KEY_UP_ARROW:
        case SDL_EVENT_KEY_DOWN_ARROW:
        case SDL_EVENT_SIM_MOUSE_WHEEL:
            if ((event->event_id == SDL_EVENT_KEY_UP_ARROW) ||
                (event->event_id == SDL_EVENT_SIM_MOUSE_WHEEL && event->mouse_wheel.delta_y > 0))
            {
                vars->cfgsel_start--;
            }
            if ((event->event_id == SDL_EVENT_KEY_DOWN_ARROW) ||
                (event->event_id == SDL_EVENT_SIM_MOUSE_WHEEL && event->mouse_wheel.delta_y < 0))
            {
                vars->cfgsel_start++;
            }
            if (vars->cfgsel_start > max_config-2) vars->cfgsel_start = max_config-2;
            if (vars->cfgsel_start < 0) vars->cfgsel_start = 0;
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
