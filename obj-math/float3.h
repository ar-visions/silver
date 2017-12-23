#ifndef _FLOAT3_
#define _FLOAT3_

#include <math.h>
#include <stdbool.h>

#ifndef sqr
#define sqr(x) ((x)*(x))
#endif

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

#define radians(degrees) ((float)(degrees) * M_PI / 180.0)
#define degrees(radians) ((float)(radians) * 180.0 / M_PI)

typedef struct _float3 {
	float x, y, z;
} float3;

typedef struct _GfxRect {
	float x, y;
	float w, h;
} GfxRect;

static inline float3 float3_add(float3 a, float3 b) { return (float3){ a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline float3 float3_sub(float3 a, float3 b) { return (float3){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline float3 float3_scale(float3 a, float b) { return (float3){ a.x * b, a.y * b, a.z * b }; }
static inline float float3_dot(float3 a, float3 b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }
static inline float float3_len(float3 a) {
	return sqrt(sqr(a.x) + sqr(a.y) + sqr(a.z));
}
static inline float float3_len_sqr(float3 a) { return sqr(a.x) + sqr(a.y) + sqr(a.z); }
static inline float3 float3_normalize(float3 a) {
	float len = float3_len(a);
	if (len <= 0.0)
		return (float3){ 0, 0, 0 };
	return float3_scale(a, 1.0 / len);
}
static inline float3 float3_cross(float3 u, float3 v) {
	float3 r;
	r.x = u.y*v.z - u.z*v.y;
	r.y = u.z*v.x - u.x*v.z;
	r.z = u.x*v.y - u.y*v.x;
	return r;
}
static inline float float3_dist(float3 a, float3 b) { return float3_len(float3_sub(a, b)); }
static inline float float3_dist_sqr(float3 a, float3 b) { return float3_len_sqr(float3_sub(a, b)); }
static inline float3 float3_interpolate(float3 a, float3 b, float f) {
	return (float3){ 
		f * a.x + (1.0 - f) * b.x,
		f * a.y + (1.0 - f) * b.y,
		f * a.z + (1.0 - f) * b.z };
}
static inline bool float3_equals(float3 a, float3 b) {
	return a.x == b.x && a.y == b.y && a.z == b.z;
}
#endif