#include <obj/obj.h>
#include <ctype.h>

implement(Pairs)

static const int base_count = 32;
static const int block_size = 32;
static const int ordered_block = 64;

void Pairs_init(Pairs self) {
    self->list_count = base_count;
    self->lists = (LList *)alloc_bytes(sizeof(LList) * self->list_count);
    for (int i = 0; i < (int)self->list_count; i++)
        llist(&self->lists[i], 0, block_size);
    llist(&self->ordered_list, 0, ordered_block);
}

void Pairs_clear(Pairs self) {
    for (int i = 0; i < (int)self->list_count; i++) {
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
    String skey = inherits(key, String);
    call(self, remove, key);
    KeyValue kv = new(KeyValue);
    kv->key = retain(key);
    kv->value = retain(value);
    ulong hash = call(key, hash) % self->list_count;
    kv->hashed = (LItem *)llist_push(&self->lists[hash], kv); // this is the hash item, not the ordered one
    kv->ordered = (LItem *)llist_push(&self->ordered_list, kv);
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
            llist_remove(list, kv->hashed);
            llist_remove(&self->ordered_list, kv->ordered);
            release(kv);
            ret = true;
        }
    }
    return ret;
}

Base Pairs_value(Pairs self, Base key) {
    String skey = (String)key;
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
    for (int i = 0; i < (int)self->list_count; i++) {
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
    release(self->user_data);
    free_ptr(self->lists);
}

typedef struct _StrRange {
    int from;
    int to;
} StrRange;

Pairs Pairs_from_cstring(const char *value) {
    if (!value)
        return NULL;
    Pairs self = auto(Pairs);
    StrRange key_range = { -1, -1 };
    StrRange val_range = { -1, -1 };
    bool sep = false, quote = false;
    char last = 0;
    for (const char *c = value; ; c++) {
        if (key_range.from == -1) {
            if (isalnum(*c))
                key_range.from = (int)(size_t)(c - value);
        } else if (key_range.to == -1) {
            if (isspace(*c) || *c == ':') {
                key_range.to = (int)(size_t)(c - value) - 1;
            }
        }
        if (key_range.to >= 0) {
            if (val_range.from == -1) {
                if (sep && !isspace(*c)) {
                    val_range.from = (int)(size_t)(c - value);
                    if (*c == '"') {
                        val_range.from++;
                        quote = true;
                    }
                } else if (*c == ':') {
                    sep = true;
                }
            } else if (val_range.to == -1) {
                if (!quote) {
                    if (*c == 0 || isspace(*c)) {
                        val_range.to = (int)(size_t)(c - value) - 1;
                    }
                } else {
                    if (*c == 0 || (*c == '"' && last != '\\')) {
                        val_range.to = (int)(size_t)(c - value) - 1;
                    }
                }
                if (val_range.to != -1) {
                    String key = class_call(String, from_bytes, (uint8 *)&value[key_range.from], key_range.to - key_range.from + 1);
                    String val = class_call(String, from_bytes, (uint8 *)&value[val_range.from], val_range.to - val_range.from + 1);
                    Base bval = call(val, infer_object);
                    call(self, add, base(key), bval);

                    key_range.from = -1;
                    key_range.to = -1;
                    val_range.from = -1;
                    val_range.to = -1;
                    quote = false;
                    sep = false;
                }
            }
        }
        last = *c;
        if (*c == 0)
            break;
    }
    return self;
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
