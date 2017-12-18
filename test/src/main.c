#include <libobj/obj.h>
#include <libobj/autorelease.h>

int main() {
    class_init();
    AutoRelease ar = class_call(AutoRelease, current);
    List list = new(List);

    KeyValue p;
    each_pair(enums(Type), p) {
        Type t = inherits(p->value, Type);
        if (t)
            printf("Type:%s = %d\n", cstring(t->symbol), t->ordinal);
    }

    Vec3 v = vec3(0,1,2);
    Vec3 z = vec3(0,2,3);
    Vec3 r = vadd(v, z);

    set_prop(r, "z", int32_object(10));

    push(list, int32_object(10));
    push(list, int32_object(30));
    push(list, int32_object(5));

    print(r, "r = %p", v);
    call(ar, drain);
    return 0;
}