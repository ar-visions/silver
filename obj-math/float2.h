#ifndef _FLOAT2_
#define _FLOAT2_

#include <math.h>
#include <stdbool.h>
#include "float3.h"

typedef struct _float2 {
	float x, y;
} float2;

typedef struct _LineSegment {
	float2 a, b;
} LineSegment;

static inline float2 float2_add(float2 a, float2 b) { return (float2){ a.x + b.x, a.y + b.y }; }
static inline float2 float2_sub(float2 a, float2 b) { return (float2){ a.x - b.x, a.y - b.y }; }
static inline float2 float2_scale(float2 a, float b) { return (float2){ a.x * b, a.y * b }; }
static inline float float2_dot(float2 a, float2 b) { return (a.x * b.x) + (a.y * b.y); }
static inline float float2_len(float2 a) {
	return sqrt(sqr(a.x) + sqr(a.y));
}
static inline float float2_len_sqr(float2 a) { return sqr(a.x) + sqr(a.y); }
static inline float2 float2_normalize(float2 a) {
	float len = float2_len(a);
	if (len <= 0.0)
		return (float2){ 0, 0 };
	return float2_scale(a, 1.0 / len);
}
static inline float2 float2_cross(float2 u) {
	float2 r;
	r.x = u.y;
	r.y = -u.x;
	return r;
}
static inline float float2_dist(float2 a, float2 b) { return float2_len(float2_sub(a, b)); }
static inline float float2_dist_sqr(float2 a, float2 b) { return float2_len_sqr(float2_sub(a, b)); }
static inline float2 float2_interpolate(float2 a, float2 b, float f) {
	return (float2){ 
		f * a.x + (1.0 - f) * b.x,
		f * a.y + (1.0 - f) * b.y };
}
static inline bool float2_equals(float2 a, float2 b) {
	return a.x == b.x && a.y == b.y;
}
static inline float2 float2_rotate(float2 v, float rads) {
    return (float2){
        v.x * cos(rads) - v.y * sin(rads),
        v.x * sin(rads) + v.y * cos(rads)};
}
static inline float float2_angle_between(float2 v1, float2 v2) {
    float angle = atan2(v1.y, v1.x) - atan2(v2.y, v2.x);
    return angle;
}
static inline bool float2_intersect_line(LineSegment L1, LineSegment L2, float2 *p) {
    float denominator, a, b, numerator1, numerator2;
    denominator = ((L2.b.y - L2.a.y) * (L1.b.x - L1.a.x)) - ((L2.b.x - L2.a.x) * (L1.b.y - L1.a.y));
    if (denominator == 0)
        return false;
    a = L1.a.y - L2.a.y;
    b = L1.a.x - L2.a.x;
    numerator1 = ((L2.b.x - L2.a.x) * a) - ((L2.b.y - L2.a.y) * b);
    numerator2 = ((L1.b.x - L1.a.x) * a) - ((L1.b.y - L1.a.y) * b);
    a = numerator1 / denominator;
    b = numerator2 / denominator;
    *p = (float2){L1.a.x + (a * (L1.b.x - L1.a.x)),
                  L1.a.y + (a * (L1.b.y - L1.a.y))};
    return true;
}
#endif