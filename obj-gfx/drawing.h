enum StrokeCap {
	STROKE_CAP_NONE,
	STROKE_CAP_BLUNT,
	STROKE_CAP_ROUNDED
};

enum StrokeJoin {
	STROKE_JOIN_MITER,
	STROKE_JOIN_ROUNDED,
	STROKE_JOIN_BEVEL
};

enum SegmentType {
	SEGMENT_RECT,
	SEGMENT_LINE,
	SEGMENT_BEZIER,
	SEGMENT_ARC
};

struct Segment {
	enum SegmentType type;
	Vec2 a, b;
	Vec2 normal;
	Vec2 center;
	Vec2 cp1, cp2;
	float rads_from;
	float rads;
	float radius;
	bool moved;
	bool close;
	bool no_feather;
};
