#ifndef _POLY2TRI_
#define _POLY2TRI_

#include <obj-math/math.h>
#include <obj-poly2tri/primitives.h>
#include <obj-poly2tri/advancing_front.h>
#include <obj-poly2tri/sweep_context.h>
#include <obj-poly2tri/sweep.h>

#define _Poly2Tri(D,T,C) _Base(spr,T,C)     \
    override(D,T,C,void,free,(C))           \
    method(D,T,C,C,with_polyline,(List))    \
    method(D,T,C,void,add_hole,(C, List))   \
    method(D,T,C,void,add_point,(C, Point)) \
    method(D,T,C,void,triangulate,(C))      \
    method(D,T,C,List,get_triangles,(C))    \
    method(D,T,C,List,get_map,(C))          \
    private_var(D,T,C,SweepContext,sweep_context) \
    private_var(D,T,C,Sweep,sweep)
declare(Poly2Tri,Base)

#endif
