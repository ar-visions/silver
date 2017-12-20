#include <obj.h>
#include <list.h>

static const int block_size = 32;

implement(List);

static void update_blocks(List self) {
    self->list.block_size = (self->list.count < block_size) ? max(self->list.count, block_size) : max(block_size, self->list.count / 4);
}

void List_init(List self) {
    llist(&self->list, 0, block_size);
}

void List_free(List self) {
    llist_clear(&self->list, false);
}

int List_count(List self) {
    return self->list.count;
}

void List_push(List self, Base obj) {
    if (obj) {
        llist_push(&self->list, obj);
        retain(obj);
        priv_call(update_blocks);
    }
}

Base List_pop(List self) {
    LItem *item = self->list.last;
    if (item) {
        Base obj = (Base)item->data;
        llist_remove(&self->list, item);
        release(obj);
        priv_call(update_blocks);
        return obj;
    }
    return NULL;
}

bool List_remove(List self, Base obj) {
    if (obj && llist_remove_data(&self->list, obj)) {
        release(obj);
        priv_call(update_blocks);
        return true;
    }
    return false;
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
    self->list.block_size = block_size;
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
