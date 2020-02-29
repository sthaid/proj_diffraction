/*
Copyright (c) 2020 Steven Haid

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <math.h>

#include "util_geometry.h"

// XXX todo - check for error conditions, such as denominator==0

// -----------------  INTERSECT LINE AND PLANE  -----------------------------

int intersect(line_t *line, plane_t *plane, point_t *point_result)
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
    T = (PD - PA * LXo - PB * LYo - PC * LZo) / denominator;

    RX = LXo + T * LA;
    RY = LYo + T * LB;
    RZ = LZo + T * LC;

    return 0;
}

// -----------------  REFLECT POINT ABOUT PLANE  ----------------------------

int reflect(point_t *point, plane_t *plane, point_t *point_result)
{
    // construct line through point, normal to the plane
    line_t Line = { *point, plane->n };

    // using defines from the above routine determine the value of T
    // for the intersection of this line with the plane
    line_t *line = &Line;
    double PD, T, denominator;

    PD = PA * PXo + PB * PYo + PC * PZo;
    denominator = (PA * LA + PB * LB + PC * LC);
    T = (PD - PA * LXo - PB * LYo - PC * LZo) / denominator;

    // the reflection point is at 2*T along the line constructed above
    point_result->x = line->p.x + 2 * T * line->v.a;
    point_result->y = line->p.y + 2 * T * line->v.b;
    point_result->z = line->p.z + 2 * T * line->v.c;

    // success
    return 0;
}

// -----------------  VARIOUS VECTOR OPERATIONS  ----------------------------

double magnitude(vector_t *v)
{
    return sqrt(v->a * v->a + v->b * v->b + v->c * v->c);
}

int normalize(vector_t *v)
{
    double m;

    m = magnitude(v);
    if (m == 0) {
        return -1;
    }

    v->a /= m;
    v->b /= m;
    v->c /= m;

    return 0;
}

void cross_product(vector_t *v1, vector_t *v2, vector_t *v_result)
{
    v_result->a = v1->b * v2->c - v1->c * v2->b;
    v_result->b = v1->c * v2->a - v1->a * v2->c;
    v_result->c = v1->a * v2->b - v1->b * v2->a;
}

double dot_product(vector_t *v1, vector_t *v2)
{
    return v1->a*v2->a + v1->b*v2->b + v1->c*v2->c;
}

// -----------------  DEBUG SUPPORT  ----------------------------------------

char *vector_str(vector_t *v, char *s)
{
    sprintf(s, "(%0.2f,%0.2f,%0.2f)", v->a, v->b, v->c);
    return s;
}

char *point_str(point_t *p, char *s)
{
    sprintf(s, "(%0.2f,%0.2f,%0.2f)", p->x, p->y, p->z);
    return s;
}

// -----------------  UNIT TEST  ------------------------------------

// to build unit test program:
//    gcc -Wall -DUNIT_TEST -o t1 util_geometry.c -lm

//#define UNIT_TEST

#ifdef UNIT_TEST
int main(int argc, char **argv)
{
    char s1[100], s2[100], s3[100];
    double cosine;
    vector_t v, v1, v2, v_cross;
    line_t  line;
    plane_t plane;
    point_t point, point_result;

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

    #define RAD2DEG(angle_in_radians) (angle_in_radians * (180 / M_PI))

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
}
#endif
