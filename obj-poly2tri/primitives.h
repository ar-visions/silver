#ifndef _PRIMITIVES_
#define _PRIMITIVES_

#define _Point(D,T,C) _Base(spr,T,C)            \
    override(D,T,C,void,free,(C))               \
    method(D,T,C,C,with_xy,(float,float,bool))   \
    method(D,T,C,C,add,(const C a, const C b)   \
    method(D,T,C,C,sub,(const C a, const C b)   \
    method(D,T,C,C,scale,(float s, const C a)   \
    method(D,T,C,bool,equals,(const C a, const C b) \
    method(D,T,C,bool,not_equal,(const C a, const C b) \
    method(D,T,C,float,dot,(const C a, const C b) \
    method(D,T,C,float,cross,(const C a, const C b) \
    method(D,T,C,C,cross_scalar_sv,(const C a, float s) \
    method(D,T,C,C,cross_scalar_vs,(const float s, C a) \
    method(D,T,C,bool,cmp,(C, C))               \
    private_var(D,T,C,float,x)                  \
    private_var(D,T,C,float,y)                  \
    private_var(D,T,C,List,edge_list)           \
    private_var(D,T,C,struct _object_Edge *,edges[4])
declare(Point,Base)

#define point(x,y)              (class_call(Point, with_xy, x, y, false))
#define point_with_edges(x,y)   (class_call(Point, with_xy, x, y, true))

#define _Edge(D,T,C) _Base(spr,T,C)             \
    override(D,T,C,void,free,(C))               \
    method(D,T,C,C,with_points,(Point,Point,bool)) \
    private_var(D,T,C,Point,p)                  \
    private_var(D,T,C,Point,q)                  \
    private_var(D,T,C,int,user_data)
declare(Edge,Base)

#define _Tri(D,T,C) _Base(spr,T,C)                      \
    method(D,T,C,C,with_points,(Point,Point,Point))     \
    method(D,T,C,Point,get_point,(Tri self, const int index) \
    method(D,T,C,Tri,get_neighbor,(Tri self, const int index) \
    method(D,T,C,bool,contains,(Tri self, Point p) \
    method(D,T,C,bool,contains_edge,(Tri self, Edge e) \
    method(D,T,C,bool,contains_points,(Tri self, Point p, Point q) \
    method(D,T,C,void,mark_neighbor,(Tri self, Point p1, Point p2, Tri t) \
    method(D,T,C,void,mark_neighbor_tri,(Tri self, Tri t) \
    method(D,T,C,void,clear,(Tri self) \
    method(D,T,C,void,clear_neighbor,(Tri self, Tri t) \
    method(D,T,C,void,clear_points,(Tri self) \
    method(D,T,C,void,clear_neighbors,(Tri self) \
    method(D,T,C,void,clear_delunay,(Tri self) \
    method(D,T,C,Point,opposite_point,(Tri self, Tri t, Point p) \
    method(D,T,C,void,legalize_p0,(Tri self, Point point) \
    method(D,T,C,void,legalize_opoint,(Tri self, Point opoint, Point npoint) \
    method(D,T,C,int,index,(Tri self, Point p) \
    method(D,T,C,int,edge_index,(Tri self, Point p1, Point p2) \
    method(D,T,C,void,mark_constrained_edge_index,(Tri self, const int index) \
    method(D,T,C,void,mark_constrained_edge,(Tri self, Edge edge) \
    method(D,T,C,void,mark_constrained_edge_pq,(Tri self, Point p, Point q) \
    method(D,T,C,Point,point_cw,(Tri self, Point p) \
    method(D,T,C,Point,point_ccw,(Tri self, Point p) \
    method(D,T,C,Tri,neighbor_cw,(Tri self, Point p) \
    method(D,T,C,Tri,neighbor_ccw,(Tri self, Point p) \
    method(D,T,C,BOOL,get_constrained_edge_ccw,(Tri self, Point p) \
    method(D,T,C,BOOL,get_constrained_edge_cw,(Tri self, Point p) \
    method(D,T,C,void,set_constrained_edge_ccw,(Tri self, Point p, BOOL ce) \
    method(D,T,C,void,set_constrained_edge_cw,(Tri self, Point p, BOOL ce) \
    method(D,T,C,BOOL,get_delunay_edge_ccw,(Tri self, Point p) \
    method(D,T,C,BOOL,get_delunay_edge_cw,(Tri self, Point p) \
    method(D,T,C,void.set_delunay_edge_ccw,(Tri self, Point p, BOOL e) \
    method(D,T,C,void,set_delunay_edge_cw,(Tri self, Point p, BOOL e) \
    method(D,T,C,Tri,neighbor_across,(Tri self, Point opoint) \
    method(D,T,C,void,debug_print,(Tri self) \
    private_var(D,T,C,Point,points[3])                  \
    private_var(D,T,C,bool,delaunay[3])                 \
    private_var(D,T,C,bool,constrained[3])              \
    private_var(D,T,C,struct _object_Tri *,neighbors[3]) \
    private_var(D,T,C,bool,interior)
declare(Tri,Base)

#endif