#include <obj.h>
#include <list.h>

static const int block_size = 32;

implement(List);

static void update_blocks(List this) {
    this->list.block_size = (this->list.count < block_size) ? max(this->list.count, block_size) : max(block_size, this->list.count / 4);
}

void List_init(List this) {
    llist(&this->list, 0, block_size);
}

void List_free(List this) {
    ll_clear(&this->list, false);
}

void List_push(List this, Base obj) {
    if (obj) {
        ll_push(&this->list, obj);
        retain(obj);
        priv_call(update_blocks);
    }
}

Base List_pop(List this) {
    LItem *item = this->list.last;
    if (item) {
        Base obj = item->data;
        llist_remove(&this->list, item);
        release(obj);
        priv_call(update_blocks);
        return obj;
    }
    return NULL;
}

bool List_remove(List this, Base obj) {
    if (obj && llist_remove_data(&this->list, obj)) {
        release(obj);
        priv_call(update_blocks);
        return true;
    }
    return false;
}

int List_index_of(List this, Base obj) {
    return llist_index_of_data(&this->list, obj);
}

void List_clear(List this) {
    for (LItem *i = this->list.first; i; i = i->next) {
        Base obj = (Base)i->data;
        if (obj)
            release(obj);
    }
    this->list.block_size = block_size;
    ll_clear(&this->list, false);
}

implement(List2);

void List2_test1(List2 this) {
    printf("test1!\n");
}