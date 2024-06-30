#include <silver/a.h>
#include <ffi.h>
#include <string.h>
#include <assert.h>

#define FNV_PRIME    0x100000001b3
#define OFFSET_BASIS 0xcbf29ce484222325

static global_init_fn* call_after;
static num             call_after_alloc;
static num             call_after_count;

void A_lazy_init(global_init_fn fn) {
    if (call_after_count == call_after_alloc) {
        global_init_fn prev       = call_after;
        num            alloc_prev = call_after_alloc;
        call_after                = calloc(32 + (call_after_alloc << 1), sizeof(global_init_fn));
        if (prev) {
            memcpy(call_after, prev, sizeof(global_init_fn) * alloc_prev);
            free(prev);
        }
    }
    call_after[call_after_count++] = fn;
}

A_f** types;
num   types_alloc;
num   types_len;

void A_push_type(A_f* type) {
    if (types_alloc == types_len) {
        A_f** prev = types;
        num   alloc_prev = types_alloc;
        types = calloc(32 + (types_alloc << 1), sizeof(A_f*));
        if (prev) {
            memcpy(types, prev, sizeof(A_f*) * alloc_prev);
            free(prev);
        }
    }
    types[types_len++] = type;
}

A_f** A_types(num* length) {
    *length = types_len;
    return types;
}

/// exported method
A A_new(A_f* type, num count) {
    A a = calloc(1, (type == typeof(A) ? 0 : sizeof(struct A)) + type->size * count);
    a->type   = type;
    a->origin = a;
    a->object = a;
    a->data   = &a[1];
    A_f* a_type = &A_type;
    A_f* current = type;
    while (current) {
        if (current->init) /// init not being set on here somehow, even though it should be emitting string_type.init = &A_init
            current->init(a->data);
        if (current == a_type)
            break;
        current = current->parent;
    }
    return a->data; /// object(a) == this operation
}



void A_finish_types() {
    /// needs to create ffi
    /// must iterate through all types
    num         types_len;
    A_f**       types = A_types(&types_len);
    const num   max_args = 8;

    /// iterate through types
    for (num i = 0; i < types_len; i++) {
        A_f* type = types[i];

        /// for each member of type
        for (num m = 0; m < type->member_count; m++) {
            member_t* mem = &type->members[m];
            if (mem->member_type == A_TYPE_METHOD) {
                /// allocate method struct with ffi info stored
                mem->method = calloc(1, sizeof(method_t));
                mem->method->ffi_cif  = calloc(1,        sizeof(ffi_cif));
                mem->method->ffi_args = calloc(max_args, sizeof(ffi_type*));
                ffi_type **arg_types = (ffi_type**)mem->method->ffi_args;
                for (num i = 0; i < mem->args.count; i++) {
                    A_f* a_type   = ((A_f**)&mem->args.arg_0)[i];
                    bool is_prim  = a_type->traits & A_TRAIT_PRIMITIVE;
                    arg_types[i]  = is_prim ? a_type->arb : &ffi_type_pointer;
                }
                ffi_status status = ffi_prep_cif(
                    (ffi_cif*) mem->method->ffi_cif, FFI_DEFAULT_ABI, mem->args.count,
                    (ffi_type*)mem->type->arb, arg_types);
                assert(status == FFI_OK);
                memcpy(&mem->method->address, &((u8*)type)[mem->offset], sizeof(void*));
                assert(mem->method->address);
            }
        }
    }
}

member_t* A_find_member(A_f* type, enum A_TYPE member_type, char* name) {
    for (num i = 0; i < type->member_count; i++) {
        member_t* mem = &type->members[i];
        if (mem->member_type == member_type && strcmp(mem->name, name) == 0)
            return mem;
    }
    return 0;
}

A A_primitive(A_f* type, void* data) {
    assert(type->traits & A_TRAIT_PRIMITIVE);
    A copy = A_new(type, type->size);
    memcpy(copy, data, type->size);
    return copy;
}

A A_primitive_i8(i8 data)     { return A_primitive(&i8_type,  &data); }
A A_primitive_u8(u8 data)     { return A_primitive(&u8_type,  &data); }
A A_primitive_i16(i16 data)   { return A_primitive(&i16_type, &data); }
A A_primitive_u16(u16 data)   { return A_primitive(&u16_type, &data); }
A A_primitive_i32(i32 data)   { return A_primitive(&i32_type, &data); }
A A_primitive_u32(u32 data)   { return A_primitive(&u32_type, &data); }
A A_primitive_i64(i64 data)   { return A_primitive(&i64_type, &data); }
A A_primitive_u64(u64 data)   { return A_primitive(&u64_type, &data); }
A A_primitive_f32(f32 data)   { return A_primitive(&f32_type, &data); }
A A_primitive_f64(f64 data)   { return A_primitive(&f64_type, &data); }
A A_primitive_none()          { return A_primitive(&none_type, &data); }
A A_primitive_bool(bool data) { return A_primitive(&bool_type, &data); }

/// this calls type methods
A A_method(A_f* type, char* method_name, array args) {
    member_t* mem = A_find_member(type, A_TYPE_METHOD, method_name);
    assert(mem);
    const     num max_args = 8;
    void*     arg_values[max_args];

    assert(args->len <= max_args);
    assert(args->len == mem->args.count);
    for (num i = 0; i < args->len; i++) {
        A_f** method_args = &mem->args.arg_0;
        A_f*  arg_type    = method_args[i];
        arg_values[i] = (arg_type->traits & A_TRAIT_PRIMITIVE) ? 
            (void*)args->elements[i] : (void*)&args->elements[i];
    }

    void* result[8]; /// enough space to handle all primitive data
    ffi_call((ffi_cif*)mem->method->ffi_cif, mem->method->address, result, arg_values);
    if (mem->type->traits & A_TRAIT_PRIMITIVE)
        return A_primitive(mem->type, result);
    else
        return (A) result[0];
}

A A_data(A origin_instance) {
    A a = object(origin_instance);
    return a->data;
}

/// we can have a vector of 0 with A-type
A A_resize(A base_instance, num count) {
    A    a    = object(base_instance);
    A_f* type = a->type;
    u8*  data = base_instance;

    if (a->count) {
        if (a->count > count) {
            /// trim items
            for (num i = a->count - (a->count - count); i < a->count; i++) {
                A instance = &((u8*)a->data)[i * type->size];
                type->destructor(instance);
            }
        }
    }
    a->count = count;
}

/// methods are ops
/// A -------------------------
A  A_hold(A a) {
    ++a->refs;
    return a;
}

void A_init(A a) {
}

void A_destructor(A a) {
}

u64 A_hash(A a) { return (u64)(size_t)a; }

/// these pointers are invalid for A since they are in who-knows land, but the differences would be the same
static i32 A_compare(A a, A b) {
    return (i32)(size_t)(a - b);
}

static void     string_init(string a)               { printf("init string\n"); }
static void     string_destructor(string a)         { free(a->chars); }
static num      string_compare(string a, string b) { return strcmp(a->chars, b->chars); }
static array    string_split(string a, A sp) {
    return null;
}

u64 fnv1a_hash(const void* data, size_t length, u64 hash) {
    const u8* bytes = (const u8*)data;
    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];  // xor bottom with current byte
        hash *= FNV_PRIME; // multiply by FNV prime
    }
    return hash;
}

static u64 string_hash(string a) {
    if (a->h) return a->h;
    a->h = fnv1a_hash(a->chars, a->len, OFFSET_BASIS);
    return a->h;
}

string string_new_reserve(num sz) {
    string a = new(string);
    a->alloc = sz;
    a->chars = (char*)calloc(1, sz);
    return a;
}

/// collection -------------------------
static void collection_push(collection a, A b) {
    assert(false);
}

static A  collection_pop(collection a) {
    assert(false);
    return null;
}

static num collection_compare(array a, collection b) {
    assert(false);
    return 0;
}

A hold(A a) {
    (a - 1)->refs++;
    return a;
}

void drop(A a) {
    if (--(a - 1)->refs == -1) {
        A aa = (a - 1);
        A_f*  type = aa->type;
        void* prev = null;
        while (type) {
            if (prev != type->destructor) {
                type->destructor(a);
                prev = type->destructor;
            }
            if (type == &A_type)
                break;
            type = type->parent;
        }
        free(aa);
    }
}

A data(A instance) {
    return object(instance)->data;
}

/// never call object on A
A object(A instance) {
    return instance - 1;
}

/// list -------------------------
static void list_push(list a, A e) {
    item n = new(item);
    n->element = e;
    if (a->last) {
        a->last->next = n;
        n->prev       = a->last;
    } else {
        a->first      = n;
    }
    a->last = n;
    a->count++;
}

static num list_compare(list a, list b) {
    num diff  = a->count - b->count;
    if (diff != 0)
        return diff;
    for (item ai = a->first, bi = b->first; ai; ai = ai->next, bi = bi->next) {
        A_f* ai_t = *(A_f**)&((A)ai->element)[-1];
        num  cmp  = ai_t->compare(ai, bi);
        if (cmp != 0) return cmp;
    }
    return 0;
}

static A list_pop(list a) {
    item l = a->last;
    a->last = a->last->prev;
    if (!a->last)
        a->first = null;
    l->prev = null;
    a->count--;
    return l;
}

static A list_get(list a, num at_index) {
    num index = 0;
    for (item i = a->first; i; i = i->next) {
        if (at_index == index)
            return i->element;
        index++;
    }
    assert(false);
    return null;
}

static num list_count(list a) {
    return a->count;
}

/// array -------------------------
static void array_expand(array a) {
    num alloc = 32 + (a->alloc << 1);
    A* elements = (A*)calloc(alloc, sizeof(struct A*));
    memcpy(elements, a->elements, sizeof(struct A*) * a->len);
    free(a->elements);
    a->elements = elements;
    a->alloc = alloc;
}

static void array_push(array a, A b) {
    if (a->alloc == a->len) {
        array_expand(a);
    }
    a->elements[a->len++] = hold(b);
}

static A array_pop(array a) {
    assert(a->len > 0);
    return a->elements[a->len--];
}

static num array_compare(array a, array b) {
    num diff = a->len - b->len;
    if (diff != 0)
        return diff;
    for (num i = 0; i < a->len; i++) {
        num cmp = M(A, compare, a->elements[i], b->elements[i]);
        if (cmp != 0)
            return cmp;
    }
    return 0;
}

static A array_get(array a, num i) {
    return a->elements[i];
}

static num array_count(array a) {
    return a->len;
}

/// ordered init -------------------------
define_class(A)
define_class(string)
define_class(item)
define_proto(collection)
define_class(list)
define_class(array)

define_primitive( u8,  ffi_type_uint8)
define_primitive(u16,  ffi_type_uint16)
define_primitive(u32,  ffi_type_uint32)
define_primitive(u64,  ffi_type_uint64)
define_primitive( i8,  ffi_type_sint8)
define_primitive(i16,  ffi_type_sint16)
define_primitive(i32,  ffi_type_sint32)
define_primitive(i64,  ffi_type_sint64)
define_primitive(f32,  ffi_type_float)
define_primitive(f64,  ffi_type_double)
define_primitive(f128, ffi_type_longdouble)
define_primitive(bool, ffi_type_uint32)
define_primitive(num,  ffi_type_sint64)
define_primitive(none, ffi_type_void)

#define ident_meta(T, B, AR) \
    intern(T, B, AR, string,    value) \
    intern(T, B, AR, string,    fname) \
    intern(T, B, AR, int,       line_num) \
    intern(T, B, AR, u64,       h) \
    method(T, B, AR, array,     split, T, A) \
    override(T, B, AR, u64, hash)
declare_class(ident)

u64 ident_hash(ident a) {
    if (!a->h) {
        u64 h = OFFSET_BASIS;
            h *= FNV_PRIME;
            h ^= string_type.hash(a->value);
        a->h = h;
    }
    return a->h;
}

array ident_split(ident a, A obj) {
    return M(string, split, a->value, obj);
}

define_class(ident)

int main(int argc, char **argv) {
    A_finish_types();

    /// lets begin porting of silver.cpp

    string s = new(string);
    array  a = new(array);
    M(array, push, a, s);

    array args = new(array);
    M(array, push, args, a);
    M(array, push, args, s);
    array_f* type = typeid(a);

    A a_result = A_method(type, "push", args);

    assert(typeid(a_result) == typeof(none));

    /// json and high level apis will all be written in silver
    /// we only need parsing, exec and gen in here
    /// 
    drop(args);
    drop(a_result);
    drop(s); /// string is still in memory, since its held from push
    return 0;
}