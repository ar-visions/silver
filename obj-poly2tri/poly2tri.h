
#include <obj-math/math.h>
#include <obj-poly2tri/primitives.h>
#include <obj-poly2tri/advancing_front.h>
#include <obj-poly2tri/sweep_context.h>
#include <obj-poly2tri/sweep.h>

class Poly2Tri {
    override void free(C);
    C with_polyline(List);
    void add_hole(C, List);
    void add_point(C, Point);
    void triangulate(C);
    List get_triangles(C);
    List get_map(C);
    private SweepContext sweep_context;
    private Sweep sweep;
};

