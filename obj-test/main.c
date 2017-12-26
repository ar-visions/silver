#include <obj/obj.h>
#include <obj-math/math.h>
#include <obj-ui/ui.h>

int main() {
    class_init();
    Test t = new(Test);
    Vec2 v2 = vec2(1,2);
    t->prop1 = 10;
    t->prop2 = 20;
    v2->test = t;

    list_push(v2->list, t);

    String json = call(v2, to_json);
    printf("json = %s\n", json->buffer);

    Vec2 v2_f = from_json(Vec2, json);

    String json_2 = call(v2_f, to_json);
    printf("json_2 = %s\n", json_2->buffer);

    // take this json, and convert back into v2 object

    Window window = new(Window);
    window->title = new_string("Hello World");
    set(window, resizable, true);
    call(window, show);

    List points = new(List);
    list_push(points, point_with_edges(0, 0));
    list_push(points, point_with_edges(10, 0));
    list_push(points, point_with_edges(10, 10));
    list_push(points, point_with_edges(0, 10));

    Poly2Tri poly2tri = class_call(Poly2Tri, with_polyline, points);

    List hole = new(List);
    list_push(hole, point_with_edges(2, 2));
    list_push(hole, point_with_edges(8, 2));
    list_push(hole, point_with_edges(8, 8));
    list_push(hole, point_with_edges(2, 8));
    call(poly2tri, add_hole, hole);

    release(points);
    call(poly2tri, triangulate);
    List tris = call(poly2tri, get_triangles);

    call(app, loop);
    release(window);
    return 0;
}