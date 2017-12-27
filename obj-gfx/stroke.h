#ifndef _GFX_STROKE_
#define _GFX_STROKE_

typedef struct _StrokePoly {
	LineSegment seg;
	LineSegment left;
	LineSegment right;
	Vec2 normal;
	Vec2 dir;
	Vec2 left_intersect;
	Vec2 right_intersect;
	float rads;
	bool moved;
	bool close;
	bool loop;
	int wedge;
	int start_cap;
	int end_cap;
} StrokePoly;

float angle_diff(float a, float b);

#endif