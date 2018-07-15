#include <obj/obj.h>

implement(List)

Base List_placeholder;

void List_class_init() {
}

void List_free(List self) {
    call(self, clear);
    class_call(List, deallocx, self->buffer);
}

void *List_alloc(Class cl, size_t count) {
    return calloc(1, count);
}

void List_deallocx(Class cl, void *ptr) {
    free(ptr);
}

List List_new_prealloc(Class cl, int count) {
    List self = new(List);
    self->buffer = class_call(List, alloc, count * sizeof(List));
    self->remaining = count;
    return self;
}

List List_new_list_of_objects(Class list_class, Class item_class, ...) {
    List self = (List)new_obj((class_Base)list_class, 0);
    self->item_class = item_class;
    va_list args;
    va_start(args, item_class);
    for (;;) {
        Base o = va_arg(args, Base);
        if (!o)
            break;
        if (object_inherits(o, self->item_class))
            list_push(self, o);
        else {
            String str = instance(String, o);
            if (!str)
                str = call(o, to_string);
            if (str) {
                Base item = ((class_Base)self->item_class)->from_string((Class)self->item_class, str);
                if (item)
                    list_push(self, item);
            }
        }
    }
    va_end(args);
    return self;
}

int List_count(List self) {
    return self->count;
}

void List_push(List self, Base obj) {
    if (self->remaining <= 0) {
        size_t size = ((self->count & ~15) | 16) << 1;
        Base *buffer = class_call(List, alloc, sizeof(Base) * size);
        memcpy(buffer, self->buffer, sizeof(Base) * self->count);
        self->remaining = size - self->count;
        class_call(List, deallocx, self->buffer);
        self->buffer = buffer;
    }
    self->buffer[self->count++] = self->weak_refs ? obj : retain(obj);
    self->remaining--;
}

Base List_pop(List self) {
    if (self->count == 0)
        return NULL;
    self->remaining++;
    return self->buffer[--self->count];
}

bool List_remove(List self, Base obj) {
    int index = call(self, index_of, obj);
    if (index >= 0) {
        Base o = self->buffer[index];
        if (!self->weak_refs)
            release(o);
        for (int i = index + 1; i < self->count; i++)
            self->buffer[i - 1] = self->buffer[i];
        self->remaining++;
        self->count--;
        return true;
    }
    return false;
}

Base List_first(List self) {
    if (self->count == 0)
        return NULL;
    return self->buffer[0];
}

Base List_last(List self) {
    if (self->count == 0)
        return NULL;
    return self->buffer[self->count - 1];
}

int List_index_of(List self, Base obj) {
    for (int i = 0; i < self->count; i++) {
        if (self->buffer[i] == obj)
            return i;
    }
    return -1;
}

void List_clear(List self) {
    if (!self->weak_refs)
        for (int i = 0; i < self->count; i++) {
            Base obj = self->buffer[i];
            release(obj);
            self->remaining++;
        }
    self->count = 0;
}

Base List_object_at(List self, int index) {
    if (index < 0 || index >= self->count)
        return NULL;
    return self->buffer[index];
}

String List_to_string(List self) {
    String result = auto(String);
    Base o;
    bool multi = false;
    each(self, o) {
        if (multi)
            call(result, concat_char, ',');
        String s = call(o, to_string);
        call(result, concat_string, s);
        multi = true;
    }
    return result;
}
