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

    set_prop(list, "test_prop", string("1"));
    Int32 prop = get_prop(list, "test_prop", Int32);
    printf("list->test_prop = %d\n", prop->value);

    Vec3 v = vec3(0,1,2);
    Vec3 z = vec3(0,2,3);
    Vec3 r = vadd(v, z);

    //Vec3 n = object_new(v);
    set_prop(r, "z", int32_object(10));

    printf("%s + %s = %s\n", cstring(v), cstring(z), cstring(r));
    call(ar, drain);
    return 0;
}