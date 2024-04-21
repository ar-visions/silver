/// base runtime for silver
#include <silver/silver.hpp>

u64 fnv1a_hash(const void* data, size_t length, u64 hash) {
    const u8* bytes = (const u8*)data;
    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];  // xor bottom with current byte
        hash *= FNV_PRIME; // multiply by FNV prime
    }
    return hash;
}

type** silver::types;

silver* silver::hold() {
    refs++;
    return this;
}
silver::silver() {
    refs = 1;
}

void silver::drop() {
    if (--refs == 0)
        delete this;
}

silver::silver(type* t) : silver() {
    info = t;
}


array::array():silver(typeof(silver, array)) {
    items = null;
    count = 0;
    alloc = 0;
    reserve(1);
}
array::array(int sz):silver(typeof(silver, array)) {
    assert(sz > 0);
    items = null;
    count = 0;
    alloc = 0;
    reserve(sz);
}
void array::reserve(int n) {
    silver **prev = items;
    items = new silver*[n];
    assert(n >= count);
    if (prev && count)
        memcpy(items, prev, count * sizeof(silver*));
    memset(&items[count], 0, (n - count) * sizeof(silver*));
    alloc = n;
}
void array::push(silver *o) {
    if (count == alloc)
        reserve(alloc << 1);
    items[count++] = o;
}
silver* array::pop() {
    assert(count > 0);
    silver *o = items[count - 1];
    items[count--] = null;
    return o;
}
silver* array::shift() {
    silver *o = items[0];
    for (int i = 1; i < count; i++)
        items[i - 1] = items[i];
    return o;
}


prop::prop():silver(typeof(silver, prop)) {
    member = null;
    info   = null;
    offset = 0;
    ptr    = null;
}
prop::prop(symbol member, void* ptr, type* info) : silver(typeof(silver, prop)), member(member), ptr(ptr), info(info) { }


array *silver::meta() {
    return new array(1);
}

u64 silver::hash_value() {
    assert(info->meta);
    assert(info->meta->count);
    u64 hash = OFFSET_BASIS;
    for (int i = 0; i < info->meta->count; i++) {
        prop* pr  = (prop*)info->meta->items[i];
        void* ptr = &((u8*)this)[pr->offset];
        if (pr->ob) {
            u64 h = ((silver*)ptr)->hash_value();
            for (int i = 0; i < 8; i++) {
                hash ^= ((u8*)&h)[i];
                hash *= FNV_PRIME;
            }
        } else {
            hash = fnv1a_hash(ptr, pr->info->sz, hash);
        }
    }
    return hash;
}



str::str():silver(typeof(silver, str)) {
    chars = null;
    alloc = 1;
    count = 0;
    reserve(64);
    chars[0] = 0;
}

u64 str::djb2(cstr str) {
    u64     h = 5381;
    u64     v;
    while ((v = *str++))
            h = h * 33 + v;
    return  h;
}

void str::reserve(int n) {
    char *prev = chars;
    chars = new char[n];
    assert(n >= count);
    if (prev && count)
        memcpy(chars, prev, count * sizeof(char));
    memset(&chars[count], 0, (n - count) * sizeof(char));
    alloc = n;
}

void str::append(char* data, int len) {
    if (count + len >= alloc)
        reserve((count + len) * 2 + 32);
    memcpy(&chars[count], data, len);
    count += len;
    chars[count] = 0;
}

u64 str::hash_value() {
    return djb2(chars);
}


field::field(silver* key, silver* val, u64 hash) : silver(), key(key), val(val), hash(hash) { }
field::field() : silver(), key(null), val(null), hash(0) { }


map::map(int sz):silver(typeof(silver, map)) {
    fields = new array(sz);
}

void map::set(silver* key, silver* val) {
    u64    h = key->hash_value();
    field* f = new field(key->hold(), val->hold(), h);
    fields->push(f);
}
silver *map::get(silver* key) {
    return null;
}


type::type(symbol name, int sz, silver *o) : silver() {
    if (!o) {
        meta     = new array(1);
        prop* pr = new prop();
        pr->info = this;
        meta->items[0] = pr; // leave offset as 0
    } else {
        meta = o->meta();
        for (int i = 0; i < meta->count; i++) {
            prop*   pr = (prop*)meta->items[i];
            pr->offset = (u8*)pr->ptr - (u8*)o;
        }
    }
}


test::test():silver(typeof(silver, test)) { }

array* test::meta() {
    array *res = new array(2);
    res->push(new prop("member1", &member1, typeof(silver, i32)));
    res->push(new prop("member2", &member2, typeof(silver, i32)));
    return res;
}

/// pattern: module::init 
void silver::init() {
    silver::types = (type**)calloc(64, sizeof(type*));
    silver::types[id::boolean] = new type { "bool", sizeof(bool) };
    silver::types[id::u8]      = new type { "u8",   sizeof(u8)   };
    silver::types[id::i8]      = new type { "i8",   sizeof(i8)   };
    silver::types[id::u16]     = new type { "u16",  sizeof(u16)  };
    silver::types[id::i16]     = new type { "i16",  sizeof(i16)  };
    silver::types[id::u32]     = new type { "u32",  sizeof(u32)  };
    silver::types[id::i32]     = new type { "i32",  sizeof(i32)  };
    silver::types[id::u64]     = new type { "u64",  sizeof(u64)  };
    silver::types[id::i64]     = new type { "i64",  sizeof(i64)  };
    
    silver::types[id::array]   = new type { "array", sizeof(array), new array() };
    silver::types[id::str]     = new type { "str",   sizeof(str),   new str()   };
    silver::types[id::prop]    = new type { "prop",  sizeof(prop),  new prop()  };
    silver::types[id::field]   = new type { "field", sizeof(field), new field() };
    silver::types[id::map]     = new type { "map",   sizeof(map),   new map()   };

    silver::types[id::test]    = new type { "test",  sizeof(test),  new test()  }; // one entry per user type
}

static silver mod;