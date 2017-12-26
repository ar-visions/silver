#include <obj/obj.h>
#include <obj/list.h>

implement(List);

void List_init(List self) {
    self->min_block_size = 32;
    llist(&self->list, 0, self->min_block_size);
}

List List_with_item_size(int item_size) {
    List self = auto(List);
    llist(&self->list, item_size, self->min_block_size);
}

List List_new_list_of(Class list_class, Class item_class) {
    List self = (List)new_obj((class_Base)list_class, 0);
    self->item_class = item_class;
    return self;
}

static void update_blocks(List self) {
    self->list.block_size = (self->list.count < self->min_block_size) ? 
        self->min_block_size : max(self->min_block_size, self->list.count / 4);
}

void List_free(List self) {
    llist_clear(&self->list, false);
}

int List_count(List self) {
    return self->list.count;
}

void *List_push(List self, Base obj) {
    void *bytes = NULL;
    if (obj || self->list.item_size) {
        bytes = llist_push(&self->list, obj);
        retain(obj);
        priv_call(update_blocks);
    }
    return self->list.item_size > 0 ? bytes : NULL;
}

Base List_pop(List self) {
    LItem *item = self->list.last;
    if (item) {
        Base obj = (Base)item->data;
        llist_remove(&self->list, item);
        if (self->list.item_size == 0)
            release(obj);
        priv_call(update_blocks);
        return obj;
    }
    return NULL;
}

bool List_remove(List self, Base obj) {
    if (obj && llist_remove_data(&self->list, obj)) {
        if (self->list.item_size == 0)
            release(obj);
        priv_call(update_blocks);
        return true;
    }
    return false;
}

Base List_first(List self) {
    LItem *item = self->list.first;
    return item ? (Base)item->data : NULL;
}

Base List_last(List self) {
    LItem *item = self->list.last;
    return item ? (Base)item->data : NULL;
}

int List_index_of(List self, Base obj) {
    return llist_index_of_data(&self->list, obj);
}

void List_clear(List self) {
    for (LItem *i = self->list.first; i; i = i->next) {
        Base obj = (Base)i->data;
        if (obj)
            release(obj);
    }
    self->list.block_size = self->min_block_size;
    llist_clear(&self->list, false);
}

int List_generic_sort(Base a, Base b) {
    if (a && b) {
        if (a->cl == b->cl)
            return call(a, compare, b);
        else
            return 0;
    } else if (a)
        return 1;
    else if (b)
        return -1;
    return 0;
}

void List_sort(List self, bool asc, SortMethod sortf) {
    llist_sort(&self->list, asc, sortf ? sortf : (SortMethod)List_generic_sort);
}
