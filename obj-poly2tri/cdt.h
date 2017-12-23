#ifndef _CDT_
#define _CDT_

#define _CDT(D,T,C) _Base(spr,T,C)          \
    override(D,T,C,void,free,(C))           \
    method(D,T,C,C,with_polyline,(List))    \
    method(D,T,C,void,add_hole,(C, List))   \
    method(D,T,C,void,add_point,(C, Point)) \
    method(D,T,C,void,triangulate,(C))      \
    method(D,T,C,List,get_triangles,(C))    \
    method(D,T,C,List,get_map,(C))          \
    private_var(D,T,C,SweepContext,sweep_context) \
    private_var(D,T,C,Sweep,sweep)
declare(CDT,Base)

#endif
