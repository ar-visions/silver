#include <obj-ui/ui.h>

implement(Font)
implement(Rect)
implement(KeyEvent)
implement(TouchEvent)
implement(Element)

void Element_init(Element self) {
    self->opacity = 1.0;
    self->relayout = new(Pairs);
    pairs_add(self->relayout, string("top"), bool_object(true));
    pairs_add(self->relayout, string("right"), bool_object(true));
    pairs_add(self->relayout, string("bottom"), bool_object(true));
    pairs_add(self->relayout, string("left"), bool_object(true));
}

void Element_free(Element self) {
}

void Element_push(Element self, Base o) {
    Element child = inherits(o, Element);
    if (!child)
        return;
    child->parent = retain(self);
    super(push, o);
}

Element Element_root(Element self) {
    for (Element e = self; e; e = e->parent) {
        if (!e->parent)
            return e;
    }
    return NULL;
}

bool Element_remove(Element self, Base o) {
    Element child = inherits(o, Element);
    if (!child)
        return false;
    release(child->parent);
    child->parent = NULL;
    super(remove, o);
}

void Element_touch(Element self, TouchEvent e) { }
void Element_key(Element self, KeyEvent e) { }

void layout(Element root, Element self) {
    Element child;
    // set rect on element relative to its parent rect
    each(self, child) {
        layout(root, child);
    }
}

void Element_layout(Element self) {
    Element root = call(self, root);
    if (root && root != self)
        return;
}

void render(Element root, Element self) {
}

void Element_render(Element self) {
    Element root = call(self, root);
    if (root && root != self)
        return;
    render(root, self);
}
