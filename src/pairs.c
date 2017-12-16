#include <obj.h>

implement(Pairs)

static const int base_count = 32;
static const int block_size = 32;
static const int ordered_block = block_size * 4;

void Pairs_init(Pairs this) {
    this->list_count = base_count;
    this->lists = (LList *)alloc_bytes(sizeof(LList) * this->list_count);
    for (int i = 0; i < this->list_count; i++)
        llist(&this->lists[i], 0, block_size);
    llist(&this->ordered_list, 0, ordered_block);
}

void Pairs_clear(Pairs this) {
    for (int i = 0; i < this->list_count; i++) {
        LList *list = &this->lists[i];
        for (LItem *item = list->first; item; item = item->next) {
            KeyValue kv = item->data;
            release(kv);
        }
        ll_clear(&this->lists[i], false);
        ll_clear(&this->ordered_list, false);
    }
}

void Pairs_add(Pairs this, Base key, Base value) {
    KeyValue kv = new(KeyValue);
    kv->key = retain(key);
    kv->value = retain(value);
    ulong hash = call(key, hash) % this->list_count;
    kv->ordered = ll_push(&this->lists[hash], kv);
    ll_push(&this->ordered_list, kv);
}

bool Pairs_remove(Pairs this, Base key) {
    ulong hash = call(key, hash) % this->list_count;
    LList *list = &this->lists[hash];
    LItem *next = NULL;
    bool ret = false;
    for (LItem *item = list->first; item; item = next) {
        next = item->next;
        KeyValue kv = (KeyValue)item->data;
        if (call(kv->key, compare, key) == 0) {
            llist_remove(list, item);
            llist_remove(&this->ordered_list, kv->ordered);
            release(kv);
            ret = true;
        }
    }
    return ret;
}

Base Pairs_value(Pairs this, Base key) {
    ulong hash = call(key, hash) % this->list_count;
    LList *list = &this->lists[hash];
    for (LItem *item = list->first; item; item = item->next) {
        KeyValue kv = (KeyValue)item->data;
        if (call(kv->key, compare, key) == 0)
            return kv->value;
    }
    return NULL;
}

Pairs Pairs_copy(Pairs this) {
    Pairs c = new(Pairs);
    for (int i = 0; i < this->list_count; i++) {
        LList *list = &this->lists[i];
        for (LItem *item = list->first; item; item = item->next) {
            KeyValue kv = item->data;
            call(c, add, kv->key, kv->value);
        }
    }
    return c;
}

void Pairs_free(Pairs this) {
    self(clear);
    free_ptr(this->lists);
}

Pairs Pairs_from_string(String value) {
    return NULL;
}

String Pairs_to_string(Pairs this) {
    return string("");
}

implement(KeyValue);

ulong KeyValue_hash(KeyValue this) {
    return call(this->key, hash);
}

void KeyValue_free(KeyValue this) {
    release(this->key);
    release(this->value);
}
