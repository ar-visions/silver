#include <obj-poly2tri/poly2tri.h>

CDT CDT_with_polyline(List polyline) {
	CDT self = auto(CDT);
	self->sweep_context = retain(class_call(SweepContext, with_polyline, polyline));
	self->sweep = new(Sweep);
	return self;
}

void CDT_free(CDT self) {
    release(self->sweep_context);
    release(self->sweep);
}

void CDT_add_hole(CDT self, List polyline) {
    call(self->sweep_context, add_hole, polyline);
}

void CDT_add_point(CDT self, Point point) {
    call(self->sweep_context, add_point, point);
}

void CDT_triangulate(CDT self) {
    call(self->sweep, triangulate, self->sweep_context);
}

List CDT_get_triangles(CDT self) {
	return self->sweep_context->triangles;
}

List CDT_get_map(CDT self) {
	return self->sweep_context->map;
}