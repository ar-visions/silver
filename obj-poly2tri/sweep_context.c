#include <obj-poly2tri/poly2tri.h>
#include <assert.h>

static const float kAlpha = 0.3;

implement(SweepContext)

SweepContext SweepContext_with_polyline(List polyline) {
	SweepContext self = auto(SweepContext);
	self->basin = new(Basin);
	self->edge_event = new(EdgeEvent);
    self->points = new(List);
    self->edge_list = new(List);
    self->triangles = new(List);
    self->map = new(List);
    self->points = retain(polyline);
	SweepContext_init_edges(self, polyline);
	return self;
}

void SweepContext_add_hole(SweepContext self, List polyline) {
  	SweepContext_init_edges(self, polyline);
    Point p;
	each(polyline, p) {
		push(self->points, p);
	}
}

void SweepContext_add_point(SweepContext self, Point point) {
	push(self->points, point);
}

void SweepContext_init_triangulation(SweepContext self) {
	Point f = (Point)call(self->points, first);
	float xmax = f->x, xmin = f->x;
	float ymax = f->y, ymin = f->y;

    Point p;
	each(self->points, p) {
		if (p->x > xmax)
			xmax = p->x;
		if (p->x < xmin)
			xmin = p->x;
		if (p->y > ymax)
			ymax = p->y;
		if (p->y < ymin)
			ymin = p->y;
	}

	float dx = kAlpha * (xmax - xmin);
	float dy = kAlpha * (ymax - ymin);
	self->head = point_with_edges(xmax + dx, ymin - dy); // [todo] all code to prot with point() must become point_with_edges()
	self->tail = point_with_edges(xmin - dx, ymin - dy);

	// Sort points along y-axis
	call(self->points, sort, true, (LSort)point_cmp); // [todo] this was flipped, probably change to false
}

void SweepContext_init_edges(SweepContext self, List polyline) {
	Point f = (Point)call(polyline, first);
    Point p = NULL;
    each(polyline, j) {
        if (p)
            push(self->edge_list, edge(p, j));
        p = j;
    }
    if (p)
        push(self->edge_list, edge(p, f));
}

AFNode SweepContext_locate_node(SweepContext self, Point point) {
  return call(self->front, locate_node, point->x);
}

void SweepContext_create_advancing_front(SweepContext self, List nodes) {
    Point pfirst = (Point)call(self->points, first);
    Tri tri = class_call(Tri, with_points, pfirst, self->tail, self->head);

    push(self->map, tri);

    self->af_head = class_call(AFNode, with_tri, tri->points[1], tri);
    self->af_middle = class_call(AFNode, with_tri, tri->points[0], tri);
    self->af_tail = class_call(AFNode, with_point, tri->points[2]);
    self->front = class_call(AdvancingFront, self->af_head, self->af_tail);

    self->af_head->next = self->af_middle;
    self->af_middle->next = self->af_tail;
    self->af_middle->prev = self->af_head;
    self->af_tail->prev = self->af_middle;
}

void SweepContext_remove_node(SweepContext self, AFNode node) {
	release(node);
}

void SweepContext_map_triangle_to_nodes(SweepContext self, Tri t) {
	for (int i = 0; i < 3; i++) {
		if (!t->neighbors[i]) {
			AFNode n = call(self->front, locate_point, tri_point_cw(t, t->points[i]));
			if (n)
				n->triangle = t;
		}
	}
}

void SweepContext_mesh_clean(SweepContext self, Tri tri) {
	List triangles = new(List);
	push(triangles, tri);

	while (triangles->count > 0) {
		Tri t = pop(triangles);
		if (t != NULL && !t->interior) {
			t->interior = true;
			push(self->triangles, t);
			for (int i = 0; i < 3; i++) {
				if (!t->constrained[i])
					push(triangles, t->neighbors[i]);
			}
		}
	}
    release(triangles);
}

void SweepContext_free(SweepContext self) {
	release(self->head);
	release(self->tail);
	release(self->front);
	release(self->af_head);
	release(self->af_middle);
	release(self->af_tail);
	release(self->edge_list);
	release(self->triangles);
	release(self->map);
    release(self->points);
	release(self->basin);
	release(self->edge_event);
}
