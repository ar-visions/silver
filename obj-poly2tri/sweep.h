#ifndef _SWEEP_
#define _SWEEP_

#define _Sweep(D,T,C) _Base(spr,T,C) \
    override(D,T,C,void,init,(C)) \
    override(D,T,C,void,free,(C)) \
    method(D,T,C,void,sweep_points,(C, SweepContext)) \
    method(D,T,C,void,triangulate,(C, SweepContext tcx) \
    method(D,T,C,void,finalization_polygon,(C, SweepContext tcx) \
    method(D,T,C,AFNode *sweep_point_event,(C, SweepContext tcx, Point point) \
    method(D,T,C,void,edge_event_en,(C, SweepContext tcx, Edge  edge, AFNode node) \
    method(D,T,C,void,edge_event_pptp,(C, SweepContext tcx, Point ep, Point eq, Tri* triangle, Point point) \
    method(D,T,C,BOOL,is_edge_side_of_triangle,(C, Tri triangle, Point ep, Point eq) \
    method(D,T,C,AFNode *sweep_new_front_triangle,(C, SweepContext tcx, Point point, AFNode *node) \
    method(D,T,C,void,fill,(C, SweepContext tcx, AFNode *node) \
    method(D,T,C,void,fill_advancing_front,(C, SweepContext tcx, AFNode *n) \
    method(D,T,C,BOOL,large_hole,(C, AFNode node) \
    method(D,T,C,BOOL,angle_exceeds_90,(C, Point origin, Point pa, Point pb) \
    method(D,T,C,BOOL,angle_invalid,(C, Point origin, Point pa, Point pb) \
    method(D,T,C,float,angle,(C, Point origin, Point pa, Point pb) \
    method(D,T,C,float,basin_angle,(C, AFNode *node) \
    method(D,T,C,float,hole_angle,(C, AFNode *node) \
    method(D,T,C,BOOL,legalize,(C, SweepContext tcx, Tri t) \
    method(D,T,C,BOOL,incircle,(C, Point pa, Point pb, Point pc, Point pd) \
    method(D,T,C,void,rotate_triangle_pair,(C, Tri t, Point p, Tri ot, Point op) \
    method(D,T,C,void,fill_basin,(C, SweepContext tcx, AFNode *node) \
    method(D,T,C,void,fill_basin_req,(C, SweepContext tcx, AFNode node) \
    method(D,T,C,BOOL,is_shallow,(C, SweepContext tcx, AFNode *node) \
    method(D,T,C,void,fill_edge_event,(C, SweepContext tcx, Edge  edge, AFNode node) \
    method(D,T,C,void,fill_right_above_edge_event,(C, SweepContext tcx, Edge  edge, AFNode node) \
    method(D,T,C,void,fill_right_below_edge_event,(C, SweepContext tcx, Edge  edge, AFNode *node) \
    method(D,T,C,void,fill_right_concave_edge_event,(C, SweepContext tcx, Edge  edge, AFNode *node) \
    method(D,T,C,void,fill_left_concave_edge_event,(C, SweepContext tcx, Edge  edge, AFNode *node) \
    method(D,T,C,void,fill_right_convex_edge_event,(C, SweepContext tcx, Edge  edge, AFNode *node) \
    method(D,T,C,void,fill_left_above_edge_event,(C, SweepContext tcx, Edge  edge, AFNode node) \
    method(D,T,C,void,fill_left_below_edge_event,(C, SweepContext tcx, Edge  edge, AFNode *node) \
    method(D,T,C,void,fill_left_convex_edge_event,(C, SweepContext tcx, Edge  edge, AFNode *node) \
    method(D,T,C,void,flip_edge_event,(C, SweepContext tcx, Point ep, Point eq, Tri* t, Point p) \
    method(D,T,C,Tri,next_flip_triangle,(C, SweepContext tcx, int o, Tri t, Tri ot, Point p, Point op) \
    method(D,T,C,Point,next_flip_point,(C, Point ep, Point eq, Tri ot, Point op) \
    method(D,T,C,void,flip_scan_edge_event,(C, SweepContext tcx, Point ep, Point eq, Tri flip_triangle, Tri t, Point p) \
    private_var(D,T,C,List,nodes)
declare(Sweep,Base)

#endif