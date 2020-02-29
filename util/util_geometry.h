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

#ifndef __UTIL_GEOMETRY_H__
#define __UTIL_GEOMETRY_H__

//
// typedefs
//

typedef struct {
    double x;
    double y;
    double z;
} point_t;

typedef struct {
    double a;
    double b;
    double c;
} vector_t;

typedef struct {
    point_t p;
    vector_t v;
} line_t;

typedef struct {
    point_t p;
    vector_t n;
} plane_t;

//
// prototypes
//

int intersect(line_t *line, plane_t *plane, point_t *point_result);
int reflect(point_t *point, plane_t *plane, point_t *point_result);

double magnitude(vector_t *v);
int normalize(vector_t *v);

void cross_product(vector_t *v1, vector_t *v2, vector_t *v_result);
double dot_product(vector_t *v1, vector_t *v2);

char *vector_str(vector_t *v, char *s);
char *point_str(point_t *p, char *s);

#endif

