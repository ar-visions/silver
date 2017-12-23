#include <obj-poly2tri/poly2tri.h>
#include <assert.h>
#include <obj-poly2tri/primitives.h>

implement(Point)

Point Point_with_xy(float x, float y, bool init_edges) {
    Point self = auto(Point);
	self->x = x;
	self->y = y;
    if (init_edges)
        self->edge_list = new(List);
	return self;
}

void Point_free(Point self) {
	release(self->edge_list);
}

bool Point_cmp(Point self, Point b) {
	if (self->y < b->y)
		return true;
	else if (self->y == b->y && self->x < b->x)
		return true;
	
	return false;
}

Point Point_add(Point a, Point b) {
	return point(a->x + b->x, a->y + b->y);
}

Point Point_sub(Point a, Point b) {
	return point(a->x - b->x, a->y - b->y);
}

Point Point_scale(float s, Point a) {
	return point(s * a->x, s * a->y);
}

bool Point_equals(Point a, Point b) {
	return a->x == b->x && a->y == b->y;
}

bool Point_not_equal(Point a, Point b) {
	return !(a->x == b->x) && !(a->y == b->y);
}

float Point_dot(Point a, Point b) {
	return a->x * b->x + a->y * b->y;
}

float Point_cross(Point a, Point b) {
	return a->x * b->y - a->y * b->x;
}

Point Point_cross_scalar_sv(Point a, float s) {
	return point(s * a->y, -s * a->x);
}

Point Point_cross_scalar_vs(float s, Point a) {
	return point(-s * a->y, s * a->x);
}

implement(Edge)

Edge Edge_with_points(Point p1, Point p2, bool edge_list) {
	Edge self = auto(Edge);
	self->p = p1;
	self->q = p2;
	self->user_data = 0;
    if (p1->y > p2->y) {
      self->q = p1;
      self->p = p2;
    } else if (p1->y == p2->y) {
      if (p1->x > p2->x) {
        self->q = p1;
        self->p = p2;
      } else if (p1->x == p2->x) {
        // Repeat points
        assert(false);
      }
    }
    if (edge_list)
        list_push(self->q->edge_list, self);
	return self;
}

void Edge_free(Edge self) {
}

implement(Tri)

Tri Tri_with_points(Point a, Point b, Point c) {
	Tri self = auto(Tri);
	self->points[0] = a;
    self->points[1] = b;
    self->points[2] = c;
	return self;
}

Point Tri_get_point(Tri self, const int index) {
	return self->points[index];
}

Tri Tri_get_neighbor(Tri self, const int index) {
	return self->neighbors[index];
}

bool Tri_contains(Tri self, Point p) {
	return p == self->points[0] || p == self->points[1] || p == self->points[2];
}

bool Tri_contains_edge(Tri self, Edge e) {
	return Tri_contains(self, e->p) && Tri_contains(self, e->q);
}

bool Tri_contains_points(Tri self, Point p, Point q) {
	return Tri_contains(self, p) && Tri_contains(self, q);
}

void Tri_mark_neighbor(Tri self, Point p1, Point p2, Tri t) {
	if ((p1 == self->points[2] && p2 == self->points[1]) || (p1 == self->points[1] && p2 == self->points[2]))
		self->neighbors[0] = t;
	else if ((p1 == self->points[0] && p2 == self->points[2]) || (p1 == self->points[2] && p2 == self->points[0]))
		self->neighbors[1] = t;
	else if ((p1 == self->points[0] && p2 == self->points[1]) || (p1 == self->points[1] && p2 == self->points[0]))
		self->neighbors[2] = t;
	else
		assert(0);
}

void Tri_mark_neighbor_tri(Tri self, Tri t) {
	Point *points = self->points;
	if (Tri_contains_points(t, points[1], points[2])) {
		self->neighbors[0] = t;
		Tri_mark_neighbor(t, points[1], points[2], self);
	} else if (Tri_contains_points(t, points[0], points[2])) {
		self->neighbors[1] = t;
		Tri_mark_neighbor(t, points[0], points[2], self);
	} else if (Tri_contains_points(t, points[0], points[1])) {
		self->neighbors[2] = t;
		Tri_mark_neighbor(t, points[0], points[1], self);
	}
}

void Tri_clear(Tri self) {
    Tri t;
    for (int i = 0; i < 3; i++) {
        t = self->neighbors[i];
        if( t != NULL )
            Tri_clear_neighbor(t, self);
    }
    Tri_clear_neighbors(self);
	Tri_clear_points(self);
}

void Tri_clear_neighbor(Tri self, Tri t) {
    if (self->neighbors[0] == t)
        self->neighbors[0] = NULL;
    else if (self->neighbors[1] == t)
        self->neighbors[1] = NULL;            
    else
        self->neighbors[2] = NULL;
}

void Tri_clear_points(Tri self) {
	memset(self->points, 0, sizeof(self->points));
}

void Tri_clear_neighbors(Tri self) {
	memset(self->neighbors, 0, sizeof(self->neighbors));
}

void Tri_clear_delunay(Tri self) {
	memset(self->delaunay, 0, sizeof(self->delaunay));
}

Point Tri_opposite_point(Tri self, Tri t, Point p) {
	Point cw = Tri_point_cw(t, p);
	return Tri_point_cw(self, cw);
}

void Tri_legalize_p0(Tri self, Point point) {
	self->points[1] = self->points[0];
	self->points[0] = self->points[2];
	self->points[2] = point;
}

void Tri_legalize_opoint(Tri self, Point opoint, Point npoint) {
	if (opoint == self->points[0]) {
		self->points[1] = self->points[0];
		self->points[0] = self->points[2];
		self->points[2] = npoint;
	} else if (opoint == self->points[1]) {
		self->points[2] = self->points[1];
		self->points[1] = self->points[0];
		self->points[0] = npoint;
	} else if (opoint == self->points[2]) {
		self->points[0] = self->points[2];
		self->points[2] = self->points[1];
		self->points[1] = npoint;
	} else {
		assert(0);
	}
}

int Tri_index(Tri self, Point p) {
	if (p == self->points[0])
		return 0;
	else if (p == self->points[1])
		return 1;
	else if (p == self->points[2])
		return 2;
	assert(0);
}

int Tri_edge_index(Tri self, Point p1, Point p2) {
	if (self->points[0] == p1) {
		if (self->points[1] == p2)
			return 2;
		else if (self->points[2] == p2)
			return 1;
	} else if (self->points[1] == p1) {
		if (self->points[2] == p2)
			return 0;
		else if (self->points[0] == p2)
			return 2;
	} else if (self->points[2] == p1) {
		if (self->points[0] == p2)
			return 1;
		else if (self->points[1] == p2)
			return 0;
	}
  	return -1;
}

void Tri_mark_constrained_edge_index(Tri self, const int index) {
	self->constrained[index] = true;
}

void Tri_mark_constrained_edge(Tri self, Edge edge) {
	Tri_mark_constrained_edge_pq(self, edge->p, edge->q);
}

void Tri_mark_constrained_edge_pq(Tri self, Point p, Point q) {
	Point *points = self->points;
	if ((q == points[0] && p == points[1]) || (q == points[1] && p == points[0]))
		self->constrained[2] = true;
	else if ((q == points[0] && p == points[2]) || (q == points[2] && p == points[0]))
		self->constrained[1] = true;
	else if ((q == points[1] && p == points[2]) || (q == points[2] && p == points[1]))
		self->constrained[0] = true;
}

Point Tri_point_cw(Tri self, Point p) {
	Point *points = self->points;
	if (p == points[0])
		return points[2];
	else if (p == points[1])
		return points[0];
	else if (p == points[2])
		return points[1];
	assert(0);
}

Point Tri_point_ccw(Tri self, Point p) {
	Point *points = self->points;
	if (p == points[0])
		return points[1];
	else if (p == points[1])
		return points[2];
	else if (p == points[2])
		return points[0];
	assert(0);
}

Tri Tri_neighbor_cw(Tri self, Point p) {
	Point *points = self->points;
	Tri *neighbors = self->neighbors;
	if (p == points[0])
		return neighbors[1];
	else if (p == points[1])
		return neighbors[2];
	return neighbors[0];
}

Tri Tri_neighbor_ccw(Tri self, Point p) {
	Point *points = self->points;
	Tri *neighbors = self->neighbors;
	if (p == points[0])
		return neighbors[2];
	else if (p == points[1])
		return neighbors[0];
  	return neighbors[1];
}

bool Tri_get_constrained_edge_ccw(Tri self, Point p) {
	Point *points = self->points;
	if (p == points[0])
		return self->constrained[2];
	else if (p == points[1])
		return self->constrained[0];
	return self->constrained[1];
}

bool Tri_get_constrained_edge_cw(Tri self, Point p) {
	Point *points = self->points;
	if (p == points[0])
		return self->constrained[1];
	else if (p == points[1])
		return self->constrained[2];
	return self->constrained[0];
}

void Tri_set_constrained_edge_ccw(Tri self, Point p, bool ce) {
	Point *points = self->points;
	if (p == points[0])
		self->constrained[2] = ce;
	else if (p == points[1])
		self->constrained[0] = ce;
	else
		self->constrained[1] = ce;
}

void Tri_set_constrained_edge_cw(Tri self, Point p, bool ce) {
	Point *points = self->points;
	if (p == points[0])
		self->constrained[1] = ce;
	else if (p == points[1])
		self->constrained[2] = ce;
	else
		self->constrained[0] = ce;
}

bool Tri_get_delunay_edge_ccw(Tri self, Point p) {
	Point *points = self->points;
	if (p == points[0])
		return self->delaunay[2];
	else if (p == points[1])
		return self->delaunay[0];
	return self->delaunay[1];
}

bool Tri_get_delunay_edge_cw(Tri self, Point p) {
	Point *points = self->points;
	if (p == points[0])
		return self->delaunay[1];
	else if (p == points[1])
		return self->delaunay[2];
	return self->delaunay[0];
}

void Tri_set_delunay_edge_ccw(Tri self, Point p, bool e) {
	Point *points = self->points;
	if (p == points[0])
		self->delaunay[2] = e;
	else if (p == points[1])
		self->delaunay[0] = e;
	else
		self->delaunay[1] = e;
}

void Tri_set_delunay_edge_cw(Tri self, Point p, bool e) {
	Point *points = self->points;
	if (p == points[0])
		self->delaunay[1] = e;
	else if (p == points[1])
		self->delaunay[2] = e;
	else
		self->delaunay[0] = e;
}

Tri Tri_neighbor_across(Tri self, Point opoint) {
	Point *points = self->points;
	if (opoint == points[0])
		return self->neighbors[0];
	else if (opoint == points[1])
		return self->neighbors[1];
	return self->neighbors[2];
}

void Tri_debug_print(Tri self) {
	Point *points = self->points;
	fprintf(stderr, "%.2f,%.2f %.2f,%.2f %.2f,%.2f\n", 
		points[0]->x, points[0]->y,
		points[1]->x, points[1]->y,
		points[2]->x, points[2]->y);
}
