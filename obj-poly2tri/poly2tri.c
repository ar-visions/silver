#include <obj-poly2tri/poly2tri.h>

implement(Poly2Tri)

Poly2Tri Poly2Tri_with_polyline(List polyline) {
	Poly2Tri self = auto(Poly2Tri);
	self->sweep_context = retain(class_call(SweepContext, with_polyline, polyline));
	self->sweep = new(Sweep);
	return self;
}

void Poly2Tri_free(Poly2Tri self) {
    release(self->sweep_context);
    release(self->sweep);
}

void Poly2Tri_add_hole(Poly2Tri self, List polyline) {
    call(self->sweep_context, add_hole, polyline);
}

void Poly2Tri_add_point(Poly2Tri self, Point point) {
    call(self->sweep_context, add_point, point);
}

void Poly2Tri_triangulate(Poly2Tri self) {
    call(self->sweep, triangulate, self->sweep_context);
}

List Poly2Tri_get_triangles(Poly2Tri self) {
	return self->sweep_context->triangles;
}

List Poly2Tri_get_map(Poly2Tri self) {
	return self->sweep_context->map;
}