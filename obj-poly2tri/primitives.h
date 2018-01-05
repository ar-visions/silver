#ifndef _POLY2TRI_PRIMITIVES_
#define _POLY2TRI_PRIMITIVES_

forward Edge;

class Point {
    override void free(C);
    C with_xy(float,float,bool);
    C add(const C a, const C b);
    C sub(const C a, const C b);
    C scale(float s, const C a);
    bool equals(const C a, const C b);
    bool not_equal(const C a, const C b);
    float dot(const C a, const C b);
    float cross(const C a, const C b);
    C cross_scalar_sv(const C a, float s;
    C cross_scalar_vs(const float s, C a);
    bool cmp(C, C);
    private float x;
    private float y;
    private List edge_list;
    private Edge edges[4];
};

class Edge {
    override void free(C);
    C with_points(Point,Point,bool);
    private Point p;
    private Point q;
    private int user_data;
};

class Tri {
    C with_points(Point,Point,Point);
    Point get_point(C self, const int index);
    C get_neighbor(C self, const int index);
    bool contains(C self, Point p);
    bool contains_edge(C self, Edge e);
    bool contains_points(C self, Point p, Point q);
    void mark_neighbor(C self, Point p1, Point p2, Tri t);
    void mark_neighbor_tri(C self, Tri t);
    void clear(C self);
    void clear_neighbor(C self, Tri t);
    void clear_points(C self);
    void clear_neighbors(C self);
    void clear_delunay(C self);
    Point opposite_point(C self, Tri t, Point p);
    void legalize_p0(C self, Point point);
    void legalize_opoint(C self, Point opoint, Point npoint);
    int index(C self, Point p);
    int edge_index(C self, Point p1, Point p2);
    void mark_constrained_edge_index(C self, const int index);
    void mark_constrained_edge(C self, Edge edge);
    void mark_constrained_edge_pq(C self, Point p, Point q);
    Point point_cw(C self, Point p);
    Point point_ccw(C self, Point p);
    C neighbor_cw(C self, Point p);
    C neighbor_ccw(C self, Point p);
    bool get_constrained_edge_ccw(C self, Point p);
    bool get_constrained_edge_cw(C self, Point p);
    void set_constrained_edge_ccw(C self, Point p, bool ce);
    void set_constrained_edge_cw(C self, Point p, bool ce);
    bool get_delunay_edge_ccw(C self, Point p);
    bool get_delunay_edge_cw(C self, Point p);
    void set_delunay_edge_ccw(C self, Point p, bool e);
    void set_delunay_edge_cw(C self, Point p, bool e);
    C neighbor_across(C self, Point opoint);
    void debug_print(C self);
    private Point points[3];
    private bool delaunay[3];
    private bool constrained[3];
    private struct _object_Tri * neighbors[3];
    private bool interior;
};

#define point(x,y)              (class_call(Point, with_xy, x, y, false))
#define point_with_edges(x,y)   (class_call(Point, with_xy, x, y, true))
#define edge(p1,p2)             (class_call(Edge, with_points, (p1), (p2), true))
#define edge_simple(p1,p2)      (class_call(Edge, with_points, (p1), (p2), false))

#endif