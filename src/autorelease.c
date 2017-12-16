#include <obj.h>
#include <autorelease.h>

static const int block_size = 4096;

static __thread LList ars;

implement(AutoRelease)

void AutoRelease_class_init(class c) {
    llist(&ars, 0, 64);
    AutoRelease this = new(AutoRelease);
    llist_push(&ars, this);
}

AutoRelease AutoRelease_current() {
    return (AutoRelease)llist_last(&ars);
}

void AutoRelease_init(AutoRelease this) {
    llist(&this->list, 0, block_size);
}

void AutoRelease_free(AutoRelease this) {
    call(this, drain);
    ll_remove(&ars, this);
}

Base AutoRelease_add(AutoRelease this, Base obj) {
    if (obj->refs != 0) {
        obj->refs = 0;
        obj->ar_node = (LItem *)ll_push(&this->list, obj);
    }
    return obj;
}

void AutoRelease_remove(AutoRelease this, Base obj) {
    if (obj->ar_node) {
        llist_remove(&this->list, obj->ar_node);
        obj->ar_node = NULL;
    }
}

void AutoRelease_drain(AutoRelease this) {
    for (LItem *i = this->list.first; i; i = i->next) {
        Base obj = (Base)i->data;
        free_obj(obj);
    }
    ll_clear(&this->list, false);
}
