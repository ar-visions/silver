#ifndef _SWEEP_CONTEXT_
#define _SWEEP_CONTEXT_

class Basin {
    private AFNode left_node;
    private AFNode bottom_node;
    private AFNode right_node;
    private float width;
    private bool left_highest;
};

class EdgeEvent {
    private Edge constrained_edge;
    private bool right;
};

class SweepContext {
    override void free(C);
    C with_polyline,(List)
    void add_hole,(C, List)
    void add_point,(C, Point)
    void init_triangulation,(SweepContext)
    void init_edges,(SweepContext, List)
    AFNode locate_node,(SweepContext, Point)
    void create_advancing_front,(SweepContext, List)
    void remove_node,(SweepContext, AFNode)
    void map_triangle_to_nodes,(SweepContext, Tri)
    void mesh_clean,(SweepContext, Tri)
    private List,edge_list
    private List,triangles
    private List,map
    private List,points
    private AdvancingFront,front
    private Point,head
    private Point,tail
    private AFNode,af_head
    private AFNode,af_middle
    private AFNode,af_tail
    private Basin,basin
    private EdgeEvent,edge_event
};
