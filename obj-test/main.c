#include <obj/obj.h>
#include <obj-math/math.h>
//#include <obj-ui/ui.h>

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
    return 0;
}