#include <obj/obj.h>
#include <obj-ui/ui.h>

int main() {
    class_init();
    Window window = new(Window);
    window->title = new_string("Hello World");
    set(window, resizable, true);
    call(window, show);

    List points = new(List);
    list_push(points, point_with_edges(0, 0));
    list_push(points, point_with_edges(10, 0));
    list_push(points, point_with_edges(10, 10));
    list_push(points, point_with_edges(0, 10));

    CDT cdt = class_call(CDT, with_polyline, points);

    List hole = new(List);
    list_push(hole, point_with_edges(2, 2));
    list_push(hole, point_with_edges(8, 2));
    list_push(hole, point_with_edges(8, 8));
    list_push(hole, point_with_edges(2, 8));
    call(cdt, add_hole, hole);

    release(points);
    call(cdt, triangulate);
    List tris = call(cdt, get_triangles);

    call(app, loop);
    release(window);
    return 0;
}