#ifndef __UTIL_SDL_PREDEFINED_PANES_H__
#define __UTIL_SDL_PREDEFINED_PANES_H__

typedef struct {
    char title_str[32];
    char x_units_str[32];
    char y_units_str[32];
    double x_min;
    double x_max;
    double y_min;
    double y_max;
    int32_t max_points_alloced;
    int32_t max_points;
    struct pane_hndlr_display_graph_point_s {
        double x;
        double y;
    } points[0];
} pane_hndlr_display_graph_params_t;

int32_t pane_hndlr_display_text(pane_cx_t * pane_cx, int32_t request, void * init_params, sdl_event_t * event);
int32_t pane_hndlr_display_graph(pane_cx_t * pane_cx, int32_t request, void * init_params, sdl_event_t * event);

#endif
