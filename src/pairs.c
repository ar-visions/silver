#include <obj.h>

implement(Pairs)

static const int base_count = 32;
static const int block_size = 32;
static const int ordered_block = block_size * 4;

void Pairs_init(Pairs self) {
    self->list_count = base_count;
    self->lists = (LList *)alloc_bytes(sizeof(LList) * self->list_count);
    for (int i = 0; i < self->list_count; i++)
        llist(&self->lists[i], 0, block_size);
    llist(&self->ordered_list, 0, ordered_block);
}

void Pairs_clear(Pairs self) {
    for (int i = 0; i < self->list_count; i++) {
        LList *list = &self->lists[i];
        for (LItem *item = list->first; item; item = item->next) {
            KeyValue kv = (KeyValue)item->data;
            release(kv);
        }
        llist_clear(&self->lists[i], false);
        llist_clear(&self->ordered_list, false);
    }
}

void Pairs_add(Pairs self, Base key, Base value) {
    KeyValue kv = new(KeyValue);
    kv->key = retain(key);
    kv->value = retain(value);
    ulong hash = call(key, hash) % self->list_count;
    kv->ordered = (LItem *)llist_push(&self->lists[hash], kv);
    llist_push(&self->ordered_list, kv);
}

bool Pairs_remove(Pairs self, Base key) {
    ulong hash = call(key, hash) % self->list_count;
    LList *list = &self->lists[hash];
    LItem *next = NULL;
    bool ret = false;
    for (LItem *item = list->first; item; item = next) {
        next = item->next;
        KeyValue kv = (KeyValue)item->data;
        if (call(kv->key, compare, key) == 0) {
            llist_remove(list, item);
            llist_remove(&self->ordered_list, kv->ordered);
            release(kv);
            ret = true;
        }
    }
    return ret;
}

Base Pairs_value(Pairs self, Base key) {
    ulong hash = call(key, hash) % self->list_count;
    LList *list = &self->lists[hash];
    for (LItem *item = list->first; item; item = item->next) {
        KeyValue kv = (KeyValue)item->data;
        if (call(kv->key, compare, key) == 0)
            return kv->value;
    }
    return NULL;
}

Pairs Pairs_copy(Pairs self) {
    Pairs c = new(Pairs);
    for (int i = 0; i < self->list_count; i++) {
        LList *list = &self->lists[i];
        for (LItem *item = list->first; item; item = item->next) {
            KeyValue kv = (KeyValue)item->data;
            call(c, add, kv->key, kv->value);
        }
    }
    return c;
}

void Pairs_free(Pairs self) {
    self(clear);
    free_ptr(self->lists);
}

Pairs Pairs_from_string(String value) {
    return NULL;
}

String Pairs_to_string(Pairs self) {
    return string("");
}

implement(KeyValue);

ulong KeyValue_hash(KeyValue self) {
    return call(self->key, hash);
}

void KeyValue_free(KeyValue self) {
    release(self->key);
    release(self->value);
}
