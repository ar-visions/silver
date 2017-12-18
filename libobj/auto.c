#include <obj.h>
#include <auto.h>

static const int block_size = 4096;

static __thread LList ars;

implement(AutoRelease)

void AutoRelease_class_init(Class c) {
    llist(&ars, 0, 64);
    AutoRelease self = new(AutoRelease);
    llist_push(&ars, self);
}

AutoRelease AutoRelease_current() {
    return (AutoRelease)llist_last(&ars);
}

void AutoRelease_init(AutoRelease self) {
    llist(&self->list, 0, block_size);
}

void AutoRelease_free(AutoRelease self) {
    call(self, drain);
    llist_remove_data(&ars, self);
}

Base AutoRelease_add(AutoRelease self, Base obj) {
    if (obj->refs != 0) {
        obj->refs = 0;
        obj->ar_node = (LItem *)llist_push(&self->list, obj);
    }
    return obj;
}

void AutoRelease_remove(AutoRelease self, Base obj) {
    if (obj->ar_node) {
        llist_remove(&self->list, obj->ar_node);
        obj->ar_node = NULL;
    }
}

void AutoRelease_drain(AutoRelease self) {
    for (LItem *i = self->list.first; i; i = i->next) {
        Base obj = (Base)i->data;
        free_obj(obj);
    }
    llist_clear(&self->list, false);
}
