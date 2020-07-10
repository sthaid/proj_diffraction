#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "util_geometry.h"
#include "util_misc.h"

#define TWO_PI (2*M_PI)

static inline double square(double x)
{
    return x * x;
}

static inline bool is_close(double a, double b, double fraction)
{
    return fabs(a/b - 1) < fraction;
}

// -----------------  INTERSECT LINE AND PLANE  -----------------------------

int intersect(geo_line_t *line, geo_plane_t *plane, geo_point_t *point_result)
{
    // line
    //   LX = LXo + T LA
    //   LY = LYo + T LB
    //   LZ = LZo + T LC
    //
    // plane
    //   PA(PX - PXo) + PB(PY - PYo) + PC(PZ - PZo) = 0
    //     OR
    //   PA PX + PB PY + PC PZ = PD
    //   PD = PA * PXo + PB * PYo + PC * PZo
    //
    // substitute line LX,LY,LZ into equation of plane
    //   PA * (LXo + T LA) + PB * (LYo + T LB) + PC * (LZo + T LC) = PD
    //
    // solve for T
    //       (PD - PA * LXo - PB * LYo - PC * LZo)
    //  T = -------------------------------------
    //         (PA * LA + PB * LB + PC * LC)              <== must not be 0
    //
    // substitute T into equation of line to get the intersection point
    //
    // error conditions occur when denominator is 0
    // - line is on the plane
    // - line is parallel to the plane

    #define LXo   (line->p.x)
    #define LYo   (line->p.y)
    #define LZo   (line->p.z)
    #define LA    (line->v.a)
    #define LB    (line->v.b)
    #define LC    (line->v.c)

    #define PXo   (plane->p.x)
    #define PYo   (plane->p.y)
    #define PZo   (plane->p.z)
    #define PA    (plane->n.a)
    #define PB    (plane->n.b)
    #define PC    (plane->n.c)

    #define RX    (point_result->x)
    #define RY    (point_result->y)
    #define RZ    (point_result->z)

    double PD, T, denominator;

    PD = PA * PXo + PB * PYo + PC * PZo;

    denominator = (PA * LA + PB * LB + PC * LC);
    if (denominator == 0) {
        return -1;
    }
    T = (PD - PA * LXo - PB * LYo - PC * LZo) / denominator;

    RX = LXo + T * LA;
    RY = LYo + T * LB;
    RZ = LZo + T * LC;

    return 0;
}

// -----------------  REFLECT POINT ABOUT PLANE  ----------------------------

int reflect(geo_point_t *point, geo_plane_t *plane, geo_point_t *point_result)
{
    // construct line through point, normal to the plane
    geo_line_t Line = { *point, plane->n };

    // using defines from the above routine determine the value of T
    // for the intersection of this line with the plane
    geo_line_t *line = &Line;
    double PD, T, denominator;

    PD = PA * PXo + PB * PYo + PC * PZo;
    denominator = (PA * LA + PB * LB + PC * LC);
    if (denominator == 0) {
        return -1;
    }
    T = (PD - PA * LXo - PB * LYo - PC * LZo) / denominator;

    // the reflection point is at 2*T along the line constructed above
    point_result->x = line->p.x + 2 * T * line->v.a;
    point_result->y = line->p.y + 2 * T * line->v.b;
    point_result->z = line->p.z + 2 * T * line->v.c;

    // success
    return 0;
}

// -----------------  VECTOR OPERATIONS  ------------------------------------

double magnitude(geo_vector_t *v)
{
    return sqrt( square(v->a) + square(v->b) + square(v->c) );
}

int set_vector_magnitude(geo_vector_t *v, double new_magnitude)
{
    double current_magnitude, factor;

    current_magnitude = magnitude(v);
    if (current_magnitude == 0) {
        return -1;
    }

    factor = new_magnitude / current_magnitude;

    v->a *= factor;
    v->b *= factor;
    v->c *= factor;

    return 0;
}

int normalize(geo_vector_t *v)
{
    return set_vector_magnitude(v, 1.0);
}

void cross_product(geo_vector_t *v1, geo_vector_t *v2, geo_vector_t *v_result)
{
    v_result->a = v1->b * v2->c - v1->c * v2->b;
    v_result->b = v1->c * v2->a - v1->a * v2->c;
    v_result->c = v1->a * v2->b - v1->b * v2->a;
}

double dot_product(geo_vector_t *v1, geo_vector_t *v2)
{
    return v1->a*v2->a + v1->b*v2->b + v1->c*v2->c;
}

// -----------------  VECTOR HORIZONTAL AND VERTICAL ANGLE  -----------------

void vector_to_angle(geo_vector_t *v, double *h_angle, double *v_angle)
{
    double a = v->a;  // x component
    double b = v->b;  // y component
    double c = v->c;  // z component

    *h_angle = atan2(b,a);
    *v_angle = asin(c / sqrt(square(a) + square(b) + square(c)));
}

void angle_to_vector(double h_angle, double v_angle, geo_vector_t *v)
{
    // a = x component
    // b = y component
    // c = z component

    // first check for special case where h_angle is 90 or 270 degrees
    double tmp = (h_angle / TWO_PI) - floor(h_angle / TWO_PI);
    if (is_close(tmp, 0.25, 1e-6)) {
        v->a = 0;
        v->b = 1;
        v->c = tan(v_angle);
        return;
    }
    if (is_close(tmp, 0.75, 1e-6)) {
        v->a = 0;
        v->b = -1;
        v->c = tan(v_angle);
        return;
    }

    // now it is save to call tan(h_angle)
    double tan_h_angle = tan(h_angle);
    double sin_v_angle = sin(v_angle);
    int h_angle_quadrant = get_quadrant(h_angle);
    v->c = sin(v_angle);
    if (h_angle_quadrant == 1 || h_angle_quadrant == 4) {
        v->a = sqrt( (1 - square(sin_v_angle)) / (1 + square(tan_h_angle)) );
    } else {
        v->a = -sqrt( (1 - square(sin_v_angle)) / (1 + square(tan_h_angle)) );
    }
    v->b = v->a * tan_h_angle;
}

// returns quadrant 1 = first, 2 = second , etc.
int get_quadrant(double angle)
{
    double tmp;
    int quadrant;

    tmp = (angle / TWO_PI) - floor(angle / TWO_PI);

    if (tmp == 1) {
        quadrant = 4;
    } else {
        quadrant = tmp * 4 + 1;
    }

    if (quadrant < 1 || quadrant > 4) {
        FATAL("incorrect quadrant %d, angle = %g\n", quadrant, angle);
    }

    return quadrant;
}

// -----------------  MISCELLANEOUS OPERATIONS  -----------------------------

double distance(geo_point_t *p1, geo_point_t *p2)
{
    return sqrt( square(p1->x - p2->x) +
                 square(p1->y - p2->y) +
                 square(p1->z - p2->z) );
}

void point_plus_vector(geo_point_t *p, geo_vector_t *v, geo_point_t *p_result)
{
    p_result->x = p->x + v->a;
    p_result->y = p->y + v->b;
    p_result->z = p->z + v->c;
}

void point_minus_vector(geo_point_t *p, geo_vector_t *v, geo_point_t *p_result)
{
    p_result->x = p->x - v->a;
    p_result->y = p->y - v->b;
    p_result->z = p->z - v->c;
}

void vector_plus_vector(geo_vector_t *v1, geo_vector_t *v2, geo_vector_t *v_result)
{
    v_result->a = v1->a + v2->a;
    v_result->b = v1->b + v2->b;
    v_result->c = v1->c + v2->c;
}

// -----------------  DEBUG SUPPORT  ----------------------------------------

char *vector_str(geo_vector_t *v, char *s)
{
    sprintf(s, "vect=(%g,%g,%g)", v->a, v->b, v->c);
    return s;
}

char *point_str(geo_point_t *p, char *s)
{
    sprintf(s, "point=(%g,%g,%g)", p->x, p->y, p->z);
    return s;
}

char *line_str(geo_line_t *l, char *s)
{
    char s1[100], s2[100];
    sprintf(s, "%s %s",  point_str(&l->p,s1), vector_str(&l->v,s2));
    return s;
}

// -----------------  UNIT TEST  ------------------------------------

// to build unit test program:
//    gcc -Wall -DUNIT_TEST -o t1 util_geometry.c util_misc.c -lm

#ifdef UNIT_TEST
int main(int argc, char **argv)
{
    geo_vector_t v, v1;
    char s[100], s1[100];
    double ha, va, m;

#if 0
    while (printf("is_close a b? "), fgets(s,sizeof(s),stdin) != NULL) {
        double a,b;
        if (sscanf(s, "%lf %lf", &a, &b) != 2) {
            ERROR("invalid input\n");
            printf("\n\n");
            continue;
        }
        printf("is_close(%g %g) = %d\n", a,b,is_close(a,b,1e-6));
    }
    return 0;

    int q = get_quadrant(-5.42101e-20);
    printf("q = %d\n", q);
#endif

    while (printf("vect? "), fgets(s,sizeof(s),stdin) != NULL) {
        if (sscanf(s, "%lf %lf %lf", &v.a, &v.b, &v.c) != 3) {
            ERROR("invalid input\n");
            printf("\n\n");
            continue;
        }
        printf("input vect  %s\n", vector_str(&v ,s1));
        m = magnitude(&v);

        vector_to_angle(&v, &ha, &va);
        printf("ha = %g va = %g\n", RAD2DEG(ha), RAD2DEG(va));

        angle_to_vector(ha, va, &v1);
        set_vector_magnitude(&v1, m);
        printf("back to vector %s\n", vector_str(&v1,s1));

        printf("\n\n");
    }

    return 0;

#if 0
    char s1[100], s2[100], s3[100];
    double cosine;
    geo_vector_t v, v1, v2, v_cross;
    geo_line_t  line;
    geo_plane_t plane;
    geo_point_t point, point_result;

    printf("TEST normalize and magnitude\n");
    VECT_INIT(&v, 1, -1, 1);
    printf("  v = %s  magnitude = %0.2f\n", vector_str(&v,s1), magnitude(&v));
    normalize(&v);
    printf("  v = %s  magnitude = %0.2f\n", vector_str(&v,s1), magnitude(&v));

    printf("\nTEST dot_product\n");
    VECT_INIT(&v, 1, 1, 1);
    printf("  sqrt( %s DOT %s ) = %0.2f\n", vector_str(&v,s1), vector_str(&v,s2), sqrt(dot_product(&v,&v)));
    printf("  magnitude(%s) = %0.2f\n", vector_str(&v, s1), magnitude(&v));
    VECT_INIT(&v1, 1, 0, 0);
    VECT_INIT(&v2, 1, 1, 0);
    cosine = dot_product(&v1,&v2) / (magnitude(&v1) * magnitude(&v2));
    printf("  cosine = %0.2f  angle = %0.2f\n", cosine, RAD2DEG(acos(cosine)));

    printf("\nTEST intersect line with plane\n");
    LINE_INIT(&line, 0,0,0, 1,0,0);
    PLANE_INIT(&plane, 5,5,5, 1,1,1);
    intersect(&line, &plane, &point);
    printf("  intersect point = %s\n", point_str(&point, s1));
    
    printf("\nTEST reflect point across plane\n");
    POINT_INIT(&point, 0,0,0);
    PLANE_INIT(&plane, 1,1,1, 1,1,1);
    reflect(&point, &plane, &point_result);
    printf("  reflected point = %s\n", point_str(&point_result,s1));

    printf("\nTEST cross product\n");
    VECT_INIT(&v1, 1, 0, 0);
    VECT_INIT(&v2, -1, 1, 0);
    cross_product(&v1, &v2, &v_cross);
    printf("  %s CROSS %s = %s\n", vector_str(&v1,s1), vector_str(&v2,s2), vector_str(&v_cross,s3));
    return 0;
#endif
}
#endif
