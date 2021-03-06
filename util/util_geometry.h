#ifndef __UTIL_GEOMETRY_H__
#define __UTIL_GEOMETRY_H__

//
// defines
//

#define VECT_INIT(_v, _a, _b, _c) \
    do { \
        (_v)->a = (_a); \
        (_v)->b = (_b); \
        (_v)->c = (_c); \
    } while (0)

#define POINT_INIT(_p, _x, _y, _z) \
    do { \
        (_p)->x = (_x); \
        (_p)->y = (_y); \
        (_p)->z = (_z); \
    } while (0)

#define LINE_INIT(_l, _x,_y,_z, _a,_b,_c) \
    do { \
        POINT_INIT(&(_l)->p, _x, _y, _z); \
        VECT_INIT(&(_l)->v, _a, _b, _c); \
    } while(0)

#define PLANE_INIT(_plane, _x,_y,_z, _a,_b,_c) \
    do { \
        POINT_INIT(&(_plane)->p, _x, _y, _z); \
        VECT_INIT(&(_plane)->n, _a, _b, _c); \
    } while (0)

#define RAD2DEG(angle_in_radians) ((angle_in_radians) * (180 / M_PI))
#define DEG2RAD(angle_in_degrees) ((angle_in_degrees) * (M_PI / 180))

//
// typedefs
//

typedef struct {
    double x;
    double y;
    double z;
} geo_point_t;

typedef struct {
    double a;
    double b;
    double c;
} geo_vector_t;

typedef struct {
    geo_point_t p;
    geo_vector_t v;
} geo_line_t;

typedef struct {
    geo_point_t p;
    geo_vector_t n;
} geo_plane_t;

//
// prototypes
//

int intersect(geo_line_t *line, geo_plane_t *plane, geo_point_t *point_result);
int reflect(geo_point_t *point, geo_plane_t *plane, geo_point_t *point_result);

double magnitude(geo_vector_t *v);
int set_vector_magnitude(geo_vector_t *v, double new_magnitude);
int normalize(geo_vector_t *v);
void cross_product(geo_vector_t *v1, geo_vector_t *v2, geo_vector_t *v_result);
double dot_product(geo_vector_t *v1, geo_vector_t *v2);

void vector_to_angle(geo_vector_t *v, double *h_angle, double *v_angle);
void angle_to_vector(double h_angle, double v_angle, geo_vector_t *v);
int get_quadrant(double angle);

double distance(geo_point_t *p1, geo_point_t *p2);
void point_plus_vector(geo_point_t *p, geo_vector_t *v, geo_point_t *p_result);
void point_minus_vector(geo_point_t *p, geo_vector_t *v, geo_point_t *p_result);
void vector_plus_vector(geo_vector_t *v1, geo_vector_t *v2, geo_vector_t *v_result);

char *vector_str(geo_vector_t *v, char *s);
char *point_str(geo_point_t *p, char *s);
char *line_str(geo_line_t *l, char *s);

#endif

