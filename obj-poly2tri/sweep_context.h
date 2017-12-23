#ifndef _SWEEP_CONTEXT_
#define _SWEEP_CONTEXT_

#define _Basin(D,T,C) _Base(spr,T,C) \
    private_var(D,T,C,AFNode,left_node) \
    private_var(D,T,C,AFNode,bottom_node) \
    private_var(D,T,C,AFNode,right_node) \
    private_var(D,T,C,float,width) \
    private_var(D,T,C,bool,left_highest)
declare(Basin,Base)

#define _EdgeEvent(D,T,C) _Base(spr,T,C) \
    private_var(D,T,C,Edge,constrained_edge) \
    private_var(D,T,C,bool,right)
declare(EdgeEvent,Base)

#define _SweepContext(D,T,C) _Base(spr,T,C) \
    override(D,T,C,void,free,(C)) \
    method(D,T,C,C,with_polyline,(List)) \
    method(D,T,C,void,add_hole,(C, List)) \
    method(D,T,C,void,add_point,(C, Point)) \
    method(D,T,C,void,init_triangulation,(SweepContext)) \
    method(D,T,C,void,init_edges,(SweepContext, List)) \
    method(D,T,C,AFNode,locate_node,(SweepContext, Point)) \
    method(D,T,C,void,create_advancing_front,(SweepContext, List)) \
    method(D,T,C,void,remove_node,(SweepContext, AFNode)) \
    method(D,T,C,void,map_triangle_to_nodes,(SweepContext, Tri)) \
    method(D,T,C,void,mesh_clean,(SweepContext, Tri)) \
    private_var(D,T,C,List,edge_list) \
    private_var(D,T,C,List,triangles) \
    private_var(D,T,C,List,map) \
    private_var(D,T,C,List,points) \
    private_var(D,T,C,AdvancingFront,front) \
    private_var(D,T,C,Point,head) \
    private_var(D,T,C,Point,tail) \
    private_var(D,T,C,AFNode,af_head) \
    private_var(D,T,C,AFNode,af_middle) \
    private_var(D,T,C,AFNode,af_tail) \
    private_var(D,T,C,Basin,basin) \
    private_var(D,T,C,EdgeEvent,edge_event)
declare(SweepContext,Base)

#endif
