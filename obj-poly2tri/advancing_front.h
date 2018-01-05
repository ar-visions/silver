
class AFNode {
    C with_point(Point);
    C with_tri(Point, Tri);
    private Point point;
    private Tri triangle;
    private AFNode next;
    private AFNode prev;
    private float value;
};

class AdvancingFront {
    C with_nodes(AFNode, AFNode);
    AFNode locate_node(C, const float);
    AFNode locate_point(C, const Point);
    private AFNode head;
    private AFNode tail;
    private AFNode search_node;
};
