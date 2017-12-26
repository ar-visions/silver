#ifndef _GFX_SHAPE_H_
#define _GFX_SHAPE_H_

#include <obj/obj.h>
#include <obj-poly2tri/poly2tri.h>

#define _Outline(D,T,C) _Base(spr,T,C)  \
    private_var(D,T,C,List,outline)     \
    private_var(D,T,C,List,holes)       \
    private_var(D,T,C,bool,is_init)     \
    private_var(D,T,C,Poly2Tri,poly2tri)          \
    private_var(D,T,C,List,tris)        \
    private_var(D,T,C,bool,no_feather)
declare(Outline,Base)

#define _Shape(D,T,C) _Base(spr,T,C)    \
    override(D,T,C,void,init,(C))       \
    override(D,T,C,void,free,(C))       \
    method(D,T,C,C,from_path,(Gfx, List)) \
    method(D,T,C,bool,poly_contains_point,(List, float2)) \
    private_var(D,T,C,List,outlines)    \
    private_var(D,T,C,List,edges)
declare(Shape,Base)

#endif