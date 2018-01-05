#include <obj/obj.h>
#include <obj-poly2tri/poly2tri.h>

forward Gfx;

class Outline {
    private List outline;
    private List holes;
    private bool is_init;
    private Poly2Tri poly2tri;
    private List tris;
    private bool no_feather;
}

class Shape {
    override void init(C);
    override void free(C);
    C from_path(Gfx *, List);
    bool poly_contains_point(List, float2);
    private List outlines;
    private List edges;
}