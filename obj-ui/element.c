#include <obj.h>

implement(Element)

void Element_init(Element self) {
    self->opacity = 1.0;
    self->relayout = new(Pairs);
    pairs_add(self->relayout, string("top"), bool_object(true));
    pairs_add(self->relayout, string("right"), bool_object(true));
    pairs_add(self->relayout, string("bottom"), bool_object(true));
    pairs_add(self->relayout, string("left"), bool_object(true));
}

void Element_push(Element self, Base o) {
    Element child = inherits(o, Element);
    if (!child)
        return;
    child->parent = retain(self);
    super(push, o);
}

void Element_remove(Element self, Base o) {
    Element child = inherits(o, Element);
    if (!child)
        return;
    release(child->parent);
    child->parent = NULL;
    super(remove, o);
}

void Element_touch(Element self, TouchEvent e) { }
void Element_key(Element self, KeyEvent e) { }

void Element_layout(Element self) {
}

void Element_render(Element self) {
}
