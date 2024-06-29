#include <silver/a.h>
#include <ffi.h>
#include <string.h>
#include <assert.h>

static global_init_fn* call_after;
static sz_t            call_after_alloc;
static int             call_after_count;

void A_lazy_init(global_init_fn fn) {
    if (call_after_count == call_after_alloc) {
        global_init_fn prev       = call_after;
        int            alloc_prev = call_after_alloc;
        call_after                = calloc(32 + (call_after_alloc << 1), sizeof(global_init_fn));
        if (prev) {
            memcpy(call_after, prev, sizeof(global_init_fn) * alloc_prev);
            free(prev);
        }
    }
    call_after[call_after_count++] = fn;
}

/*
/// this calls type methods
A_alloc_t method(A_alloc_t inst, void* method_fn, array* args) {
    const sz_t max_args = 8;
    int       args_count = args->len;
    ffi_type* arg_types [max_args];
    void*     arg_values[max_args];

    assert(args_count <= max_args);
    for (int i = 0; i < arg_count; i++) {
        A*   object = args->get(i);
        A_ftable* type     = ftable(object);
        bool is_prim  = type->flags & A_TRAIT_PRIMITIVE;
        arg_types [i] = is_prim ? type->arb : &ffi_type_pointer;
        arg_values[i] = object;
    }

    /// meta can use the tokens formed from T ## _t
    /// member { string* name, int offset, A_ftable** types, int type count }
    /// the global constructor needs to write this out; allocation perhaps
    //ffi_type* r_type = 

    ffi_cif cif;
    ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arg_count, &ffi_type_void, arg_types);
}
*/

/// exported method
A A_new(A_ftable* type, sz_t count) {
    A a = calloc(1, (type == A_typeof(A) ? 0 : sizeof(struct A)) + type->size * count);
    a->type   = type;
    a->origin = a;
    a->object = a;
    a->data   = &a[1];
    A_ftable* a_type = &A_type;
    A_ftable* current = type;
    while (current) {
        if (current->init) /// init not being set on here somehow, even though it should be emitting string_type.init = &A_init
            current->init(a->data);
        if (current == a_type)
            break;
        current = current->parent;
    }
    return a->data; /// object(a) == this operation
}

void A_startup() {
    ///
}

A A_data(A origin_instance) {
    A a = object(origin_instance);
    return a->data;
}

/// we can have a vector of 0 with A-type
A A_resize(A base_instance, sz_t count) {
    A  a      = object(base_instance);
    A_ftable* type = a->type;
    uint8_t* data = base_instance;

    if (a->count) {
        if (a->count > count) {
            /// trim items
            for (int i = a->count - (a->count - count); i < a->count; i++) {
                A instance = &((u8*)a->data)[i * type->size];
                type->destructor(instance);
            }
        }
    }
    a->count = count;
}

/// methods are ops
/// if all args were object this is fine. no primitives in C -- why would we want that
/// A -------------------------
A  A_hold(A a) {
    ++a->refs;
    return a;
}

void A_init(A a) {
}

void A_destructor(A a) {
}

/// these pointers are invalid for A since they are in who-knows land, but the differences would be the same
static i32 A_compare(A a, A b) {
    return (int)(size_t)(a - b);
}

static void     string_init(string a)               { printf("init string\n"); }
static void     string_destructor(string a)         { free(a->chars); }
static int      string_compare(string a, string b) { return strcmp(a->chars, b->chars); }

string string_new_reserve(int sz) {
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

static int collection_compare(array a, collection b) {
    assert(false);
    return 0;
}

A hold(A a) {
    (a - 1)->refs++;
    return a;
}

void drop(A a) {
    if (--(a - 1)->refs == -1) {
        A_ftable*  type = a->type;
        void* prev = null;
        while (type) {
            if (prev != type->destructor) {
                type->destructor(a);
                prev = type->destructor;
            }
            type = type->parent;
        }
        free(a);
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

/// return types can be anything, including primitives
/// the args, though, its sensible to make objects
static int list_compare(list a, list b) {
    int diff  = a->count - b->count;
    if (diff != 0)
        return diff;
    for (item ai = a->first, bi = b->first; ai; ai = ai->next, bi = bi->next) {
        A_ftable* ai_t = *(A_ftable**)&((A)ai->element)[-1];
        int  cmp  = ai_t->compare(ai, bi);
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

/// 
static A list_get(list a, sz_t at_index) {
    int index = 0;
    for (item i = a->first; i; i = i->next) {
        if (at_index == index)
            return i->element;
        index++;
    }
    assert(false);
    return null;
}

static sz_t list_count(list a) {
    return a->count;
}

/// array -------------------------
static void array_expand(array a) {
    int alloc = 32 + (a->alloc << 1);
    A* elements = (A*)calloc(alloc, sizeof(struct A*));
    memcpy(elements, a->elements, sizeof(struct A*) * a->len);
    free(a->elements);
    a->elements = elements;
}

static void array_push(array a, A b) {
    if (a->alloc == a->len) {
        array_expand(a);
    }
    a->elements[a->len] = hold(b);
}

static A array_pop(array a) {
    assert(a->len > 0);
    return a->elements[a->len--];
}

static int array_compare(array a, array b) {
    int diff = a->len - b->len;
    if (diff != 0)
        return diff;
    for (int i = 0; i < a->len; i++) {
        int cmp = M(A, compare, a->elements[i], b->elements[i]);
        if (cmp != 0)
            return cmp;
    }
    return 0;
}

static A array_get(array a, sz_t i) {
    return a->elements[i];
}

static sz_t array_count(array a) {
    return a->len;
}

void set_primitive_arb(A_ftable* type) {
         if (type == A_typeof(boolean)) type->arb = &ffi_type_uint32;
    else if (type == A_typeof(u8))      type->arb = &ffi_type_uint8;
    else if (type == A_typeof(u16))     type->arb = &ffi_type_uint16;
    else if (type == A_typeof(u32))     type->arb = &ffi_type_uint32;
    else if (type == A_typeof(u64))     type->arb = &ffi_type_uint64;
    else if (type == A_typeof(i8))      type->arb = &ffi_type_sint8;
    else if (type == A_typeof(i16))     type->arb = &ffi_type_sint16;
    else if (type == A_typeof(i32))     type->arb = &ffi_type_sint32;
    else if (type == A_typeof(i64))     type->arb = &ffi_type_sint64;
    else if (type == A_typeof(f32))     type->arb = &ffi_type_float;
    else if (type == A_typeof(f64))     type->arb = &ffi_type_double;
    else if (type == A_typeof(f128))    type->arb = &ffi_type_longdouble;
    else {
        printf("implement ffi mapping for primitive: %s\n", type->name);
        assert(false);
    }
}

/// ordered init -------------------------
define_class(A)
define_class(string)
define_primitive( u8)
define_primitive(u16)
define_primitive(u32)
define_primitive(u64)
define_primitive( i8)
define_primitive(i16)
define_primitive(i32)
define_primitive(i64)
define_primitive(f32)
define_primitive(f64)
define_primitive(f128)
define_primitive(boolean)
define_primitive(sz_t)
define_primitive(none)

define_class(item)
define_proto(collection)
define_class(list)
define_class(array) // implements collection

/// no auto-release, drop objects after new and not using
int main(int argc, char **argv) {
    A_startup();

    string s = new(string);
    array  a = new(array);

    M(array, push, a, s);

    drop(s); /// string is still in memory, since its held from push

    /// lets make an array with this
    /// elements are pointers to A*
    return 0;
}