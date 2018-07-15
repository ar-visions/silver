#include <obj/obj.h>

static _thread_local_ List ars;

implement(AutoRelease)

void AutoRelease_class_init(Class c) {
    new(AutoRelease);
}

AutoRelease AutoRelease_current(Class c) {
    return (AutoRelease)call(ars, last);
}

void AutoRelease_init(AutoRelease self) {
    if (!ars) {
        ars = new(List);
        ars->weak_refs = true;
    }
    list_push(ars, self);
}

void AutoRelease_free(AutoRelease self) {
    list_remove(ars, self);
}
