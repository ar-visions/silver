#include <obj/obj.h>
#include <ctype.h>

implement(Pairs)

static const int base_count = 32;
static const int prealloc_size = 32;
static const int ordered_block = 64;

void Pairs_init(Pairs self) {
    self->list_count = base_count;
    self->lists = (List *)class_call(Pairs, alloc, sizeof(List) * self->list_count);
    for (int i = 0; i < (int)self->list_count; i++)
        self->lists[i] = class_call(List, new_prealloc, prealloc_size);
    self->ordered_list = new(List);
}

void Pairs_clear(Pairs self) {
    for (int i = 0; i < (int)self->list_count; i++) {
        list_clear(self->lists[i]);
    }
    list_clear(self->ordered_list);
}

void Pairs_add(Pairs self, Base key, Base value) {
    call(self, remove, key);
    KeyValue kv = new(KeyValue);
    self->str_test = (String)value;
    kv->key = retain(key);
    kv->value = retain(value);
    ulong hash = call(key, hash) % self->list_count;
    list_push(self->lists[hash], kv);
    list_push(self->ordered_list, kv);
    release(kv);
}

KeyValue Pairs_find(Pairs self, Base key) {
    ulong hash = call(key, hash) % self->list_count;
    List list = self->lists[hash];
    KeyValue kv;
    each(list, kv)
        if (call(kv->key, compare, key) == 0)
            return kv;
    return NULL;
}

bool Pairs_remove(Pairs self, Base key) {
    KeyValue kv = call(self, find, key);
    if (kv) {
        ulong hash = call(key, hash) % self->list_count;
        list_remove(self->lists[hash], kv);
        list_remove(self->ordered_list, kv);
        return true;
    }
    return false;
}

Base Pairs_value(Pairs self, Base key) {
    KeyValue kv = call(self, find, key);
    return kv ? kv->value : NULL;
}

Pairs Pairs_copy(Pairs self) {
    Pairs c = new(Pairs);
    for (int i = 0; i < (int)self->list_count; i++) {
        List list = self->lists[i];
        KeyValue kv;
        each(list, kv)
            call(c, add, kv->key, kv->value);
    }
    return c;
}

void Pairs_free(Pairs self) {
    if (self->testme) {
        int test = 0;
        test++;
    }
    self(clear);
    release(self->user_data);
    String *str = self->str_test;
    for (int i = 0; i < (int)self->list_count; i++)
        release(self->lists[i]);
    class_call(Pairs, deallocx, self->lists);
    release(self->ordered_list);
}

typedef struct _StrRange {
    int from;
    int to;
} StrRange;

Pairs Pairs_from_cstring(Class cl, const char *value) {
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
