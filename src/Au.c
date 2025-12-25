#include <import>

//#undef realloc
#include <ffi.h>
#undef bool
#include <ports.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#undef bool

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

Au_t_info        Au_t_i;

#ifndef line
#define line(...)       new(line, __VA_ARGS__)
#endif

i64 epoch_millis();


typedef struct _ffi_method_t {
    struct _array*  atypes;
    Au_t            rtype;
    void*           address;
    void*           ffi_cif;  /// ffi-calling info
    void*           ffi_args; /// ffi-data types for args
} ffi_method_t;

Au_t au_arg(Au a) {
    if (!isa(a) || a == isa(a)) return (Au_t)a;
    return cast(Au_t, a);
}

Au_t Au_cast_Au_t(Au a) {
    return isa(a);
}

bool Au_is_generic  (Au t) { return t && typeid(Au) == au_arg(t); }
bool Au_is_integral (Au t) { return t && au_arg(t)->is_integral; }
bool Au_is_void     (Au t) { return t && typeid(none) == au_arg(t); }
bool Au_is_double   (Au t) { return t && typeid(f64) == au_arg(t); }
bool Au_is_float    (Au t) { return t && typeid(f32) == au_arg(t); }
bool Au_is_realistic(Au t) { return t && au_arg(t)->is_realistic; }
bool Au_is_class    (Au t) {
    Au_t au = au_arg(t);
    return au && au != typeid(Au_t) && au->is_class;
}
bool Au_is_struct  (Au t) { return au_arg(t)->is_struct; }
bool Au_is_opaque  (Au t) {
    Au_t au = au_arg(t);
    if (au->is_struct && au->members.count == 0) return true;
    return false;
}
bool Au_is_func(Au t) {
    Au_t au = au_arg(t);
    return au && (au->member_type == AU_MEMBER_FUNC ||
                  au->member_type == AU_MEMBER_CAST ||
                  au->member_type == AU_MEMBER_CONSTRUCT) && (au->ident || au->alt);
}
bool Au_is_func_ptr(Au t) {
    Au_t au = au_arg(t);
    return au->member_type == AU_MEMBER_TYPE && au->is_funcptr;
}
bool Au_is_imethod (Au t) { return au_arg(t)->member_type == AU_MEMBER_FUNC && au_arg(t)->is_imethod; }
Au_t Au_is_rec     (Au t) {
    Au_t au = au_arg(t);
    if (au == typeid(ARef) || Au_is_func(t)) return null;
    if (au->src && au->src->is_class) return au->src;
    return (au->is_class || au->is_struct) ? au : null;
}
bool Au_is_prim    (Au t) { return au_arg(t)->is_primitive; }
bool Au_is_sign    (Au t) { return au_arg(t)->is_signed; }
bool Au_is_unsign  (Au t) { return au_arg(t)->is_unsigned; }
bool Au_is_ptr     (Au t) { return au_arg(t)->is_pointer; }
bool Au_is_enum    (Au t) { return au_arg(t)->is_enum; }
bool Au_is_bool    (Au t) { return typeid(bool) == au_arg(t); }
bool Au_is_type    (Au t) { return au_arg(t)->member_type == AU_MEMBER_TYPE; }


shape shape_with_array(shape a, array dims) {
    num count = len(dims);
    each (dims, Au, e) {
        i64* i = (i64*)instanceof(e, i64);
        a->data[a->count++] = *i;
    }
    return a;
}

i64 shape_total(shape a) {
    i64* data = a->data;
    i64 total = 1;
    for (int i = 0; i < a->count; i++)
        total *= data[i];
    return total;
}

i64 shape_compare(shape a, shape b) {
    if (a->count != b->count)
        return a->count - b->count;
    for (int i = 0; i < a->count; i++)
        if (a->data[i] != b->data[i])
            return a->data[i] - b->data[i];
    return 0;
}

shape shape_from(i64 count, ref_i64 values) {
    shape res = new(shape, count, count, data, values, is_global, false);
    res->count = count;
    if (values)
        memcpy(res->data, values, sizeof(i64) * count);
    return res;
}

shape shape_read(ARef ff) {
    FILE* f = (FILE*)ff;
    i32 n_dims;
    i64 data[256];
    verify(fread(&n_dims, sizeof(i32), 1, f) == 1, "n_dims");
    verify(n_dims < 256, "invalid");
    verify(fread(data, sizeof(i64), n_dims, f) == n_dims, "shape_read data");
    shape res = shape_from(n_dims, data);
    return res;
}

shape new_shape(i64 size, ...) {
    va_list args;
    va_start(args, size);
    i64 n_dims = 0;
    for (i64 arg = size; arg; arg = va_arg(args, i64))
        n_dims++;
    
    va_start(args, size);
    shape res = shape_from(n_dims, null);
    i64  index = 0;
    for (i64 arg = size; arg; arg = va_arg(args, i64))
        res->data[index++] = arg;
    return res;
}

array array_shift(array a) {
    int ln = len(a);
    array res = array(alloc, ln);
    bool skip = true;
    each(a, Au, i) {
        if (skip)
            skip = false;
        else
            push(res, i);
    }
    return res;
}

string array_join(array a, cstr str) {
    int ln = len(a);
    string res = string(alloc, ln * 32);
    each(a, Au, i) {
        string s = cast(string, i);
        concat(res, s);
    }
    return res;
}

bool array_cast_bool(array a) { return a && a->count > 0; }

none array_alloc_sz(array a, sz alloc) {
    Au* elements = (Au*)calloc(alloc, sizeof(struct _Au*));
    memcpy(elements, a->origin, sizeof(struct _Au*) * a->count);
    free(a->origin);
    a->origin = elements;
    a->alloc = alloc;
}

// for Au_t set on types, we use meta, which is a direct vector of the Au_t used
none set_meta_array(Au_t type, int count, ...) {
    type->meta.alloc  = count;
    type->meta.count  = count;
    type->meta.origin = calloc(count, sizeof(Au_t));

    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        type->meta.origin[i] = (Au)va_arg(args, Au_t);
    }
}

// for function model, we have an arg node with a name; this is to faciltate a separate named user object
none set_args_array(Au_t type, int count, ...) {
    type->meta.alloc  = count;
    type->meta.count  = count;
    type->meta.origin = calloc(count, sizeof(Au_t));

    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Au_t au     = va_arg(args, Au_t);
        Au_t au_arg = Au_register(type, null, AU_MEMBER_VAR, 0);
        au_arg->src = au;
        type->meta.origin[i] = (Au)au_arg;
    }
}

none set_meta_map(Au_t type, int count, ...) {
    verify(false, "todo... implement primitive allocations in map_init");
}

none array_init(array a) {
    if (a->alloc)
        array_alloc_sz(a, a->alloc);
}

none array_dealloc(array a) {
    clear(a);
    free(a->origin);
    a->origin = null;
}

none array_fill(array a, Au f) {
    for (int i = 0; i < a->alloc; i++)
        push(a, f);
}

string array_cast_string(array a) {
    string r = string(alloc, 64);
    for (int i = 0; i < a->count; i++) {
        Au e = (Au)a->origin[i];
        string s = cast(string, e);
        if (r->count)
            append(r, ", ");
        append(r, s ? s->chars : "null");
    }
    return r;
}

array array_reverse(array a) {
    array r = array((int)len(a));
    r->unmanaged = a->unmanaged;
    r->assorted  = a->assorted;
    for (int i = len(a) - 1; i >= 0; i--)
        push(r, a->origin[i]);
    return r;
}

none array_expand(array a) {
    num alloc = 512 + (a->alloc << 1);
    array_alloc_sz(a, alloc);
}

none array_push_weak(array a, Au b) {
    if (a->alloc == a->count) array_expand(a);
    a->origin[a->count++] = (Au)b;
}

array  array_copy(array a) {
    array  b = new(array, alloc, len(a));
    concat(b, a);
    return b;
}

array array_with_i32(array a, i32 alloc) {
    a->alloc = alloc;
    return a;
}

none array_push_vdata(array a, Au data, i64 count) {
    Au_t   t = isa(a)->meta.origin ? *(Au_t*)isa(a)->meta.origin : null;
    verify(t && t != typeid(Au),
        "method requires meta object with type signature");
    verify(a->unmanaged,
        "this method requires unmanaged primitives");
    u8*     cur = (u8*)data;
    i64     t_size = t->typesize;

    while (count < (a->alloc - a->count))
        array_expand(a);

    for (i64 i = 0; i < count; i++)
        a->origin[a->count + i] = (Au)(cur + (t_size * i));

    a->count += count;
    a->last_type = t;
}

Au_t Au_meta_index(Au a, int i) {
    Au_t t = isa(a) ? (Au_t)isa(a) : (Au_t)a;
    if  (t->meta.origin) {
        verify(i >= 0 && i < t->meta.count, "meta index out of type range for %s", t->ident);
        Au_t arg = (Au_t)t->meta.origin[i];
        if (arg->member_type == AU_MEMBER_VAR)
            return arg->src;
        else
            return arg;
    }
    return null;
}

none array_push(array a, Au b) {
    if (!a->origin || a->alloc == a->count) {
        array_expand(a);
    }
    Au_t t = isa(a);
    Au_t vtype = isa(b);
    Au info = (Au)head(a);
    verify(!a->last_type || a->last_type == vtype || a->assorted,
        "unassorted array received differing type: %s, previous: %s (%s:%i)",
        vtype->ident, a->last_type->ident, info->source, info->line);
    a->last_type = vtype;

    if (Au_is_meta((Au)a) && Au_meta_index((Au)a, 0) != typeid(Au))
        assert(Au_is_meta_compatible((Au)a, (Au)b), "not meta compatible");
    
    a->origin[a->count++] = (a->unmanaged ? b : Au_hold(b));
}

Au array_qpush(array a, Au b) {
    if (!a->origin || a->alloc == a->count)
        array_expand(a);
    a->origin[a->count++] = b;
    return b;
}

none array_clear(array a) {
    if (!a->unmanaged)
        for (num i = 0; i < a->count; i++) {
            Au_drop(a->origin[i]);
            a->origin[i] = null;
        }
    a->count = 0;
}

none array_concat(array a, array b) {
    each(b, Au, e) array_push(a, e);
}

Au array_index_num(array a, num i) {
    if (i < 0)
        i += a->count;
    if (i >= a->count)
        return 0;
    return a->origin[i];
}

none array_remove(array a, num b) {
    for (num i = b; i < a->count; i++) {
        Au prev = a->origin[b];
        a->origin[b] = a->origin[b + 1];
        if (!a->unmanaged)
            Au_drop(prev);
    }
    a->origin[--a->count] = null;
}

none array_remove_weak(array a, num b) {
    for (num i = b; i < a->count; i++) {
        Au prev = a->origin[b];
        a->origin[b] = a->origin[b + 1];
    }
    a->origin[--a->count] = null;
}

none array_operator__assign_add(array a, Au b) {
    array_push(a, b);
}

none array_operator__assign_sub(array a, num b) {
    array_remove(a, b);
}

Au array_first_element(array a) {
    assert(a->count, "no items");
    return a->origin[0];
}

Au array_last_element(array a) {
    assert(a->count, "no items");
    return a->origin[a->count - 1];
}

none array_push_symbols(array a, cstr symbol, ...) {
    va_list args;
    va_start(args, symbol);
    for (cstr value = symbol; value != null; value = va_arg(args, cstr)) {
        string s = new(string, chars, value);
        push(a, (Au)s);
    }
    va_end(args);
}

none array_push_objects(array a, Au f, ...) {
    va_list args;
    va_start(args, f);
    Au value;
    while ((value = va_arg(args, Au)) != null)
        push(a, value);
    va_end(args);
}

array array_of(Au first, ...) {
    array a = new(array, alloc, 32, assorted, true);
    va_list args;
    va_start(args, first);
    if (first) {
        push(a, first);
        for (;;) {
            Au arg = va_arg(args, Au);
            if (!arg)
                break;
            push(a, arg);
        }
    }
    return a;
}

array array_of_cstr(cstr first, ...) {
    array a = allocate(array, alloc, 256);
    va_list args;
    va_start(args, first);
    for (cstr arg = first; arg; arg = va_arg(args, cstr))
        push(a, (Au)string(arg));
    return a;
}

Au array_pop(array a) {
    assert(a->count > 0, "no items");
    if (!a->unmanaged) Au_drop(a->origin[a->count - 1]);
    return a->origin[--a->count];
}

num array_compare(array a, array b) {
    num diff = a->count - b->count;
    if (diff != 0)
        return diff;
    for (num i = 0; i < a->count; i++) {
        num cmp = compare((Au)a->origin[i], (Au)b->origin[i]);
        if (cmp != 0)
            return cmp;
    }
    return 0;
}

Au collective_peek(collective a, num i) {
    if (i < 0 || i >= a->count)
        return null;
    return a->origin[i];
}

array array_mix(array a, array b, f32 f) {
    int ln0 = len(a);
    int ln1 = len(b);
    if (ln0 != ln1) return b;

    Au_t fmix = null;
    Au_t expect = null;
    array res = array(ln0);
    for (int i = 0; i < ln0; i++) {
        Au aa = a->origin[i];
        Au bb = b->origin[i];

        Au_t at = isa(aa);
        Au_t bt = isa(bb);

        if (!expect) expect = at;
        verify(expect == at, "disperate types in array during mix");
        verify(at == bt, "types do not match");

        if (!fmix) fmix = find_member(at, AU_MEMBER_FUNC, "mix", false);
        verify(fmix, "implement mix method for type %s", at->ident);
        Au e = ((mix_fn)fmix->value)(aa, bb, f);
        push(res, e);
    }
    return res;
}

Au array_get(array a, num i) {
    if (i < 0 || i >= a->count)
        fault("out of bounds: %i, len = %i", i, a->count);
    return a->origin[i];
}

num array_count(array a) {
    return a->count;
}

num collective_len(collective a) {
    return a->count;
}

map array_cast_map(array a) {
    map m = map(hsize, 16);
    each (a, Au, i) {
        string k = cast(string, i);
        set(m, (Au)k, i);
    }
    return m;
}

num array_index_of(array a, Au b) {
    if (a->unmanaged) {
        for (num i = 0; i < a->count; i++) {
            if (a->origin[i] == b)
                return i;
        }
    } else {
        for (num i = 0; i < a->count; i++) {
            if (compare((Au)a->origin[i], b) == 0)
                return i;
        }
    }

    return -1;
}

__thread array     af_stack;
__thread   AF      af_top;

struct _init_dep {
    Au_t dep;
    global_init_fn call_after;
};

static struct _init_dep* call_after;
static num             call_after_alloc;
static num             call_after_count;
static map             log_funcs;

none lazy_init(global_init_fn fn, Au_t dependency) {
    if (call_after_count == call_after_alloc) {
        global_init_fn* prev = (void*)call_after;
        num alloc_prev = call_after_alloc;
        call_after_alloc = 32 + (call_after_alloc << 1);
        call_after = calloc(call_after_alloc, sizeof(struct _init_dep));
        if (prev) {
            memcpy(call_after, prev, sizeof(struct _init_dep) * alloc_prev);
            free(prev);
        }
    }
    call_after[call_after_count].call_after = fn;
    call_after[call_after_count].dep = dependency;
    call_after_count++;
}

static global_init_fn* call_last;
static num             call_last_alloc;
static num             call_last_count;

#pragma pack(push, 1)

typedef struct _Au_combine {
    struct _Au   info;
    struct _Au_t type;
} Au_combine;

typedef struct _array_combine {
    struct _Au    info;
    struct _array data;
} array_combine;
 
#pragma pack(pop)

static Au_combine* member_pool;
static int         n_members;

static Au_t   au_module;
static Au_t   module;
static array_combine modules;
static array  scope;
static bool   started = false;

Au_t Au_find_member(Au_t mdl, symbol f, int member_type, bool poly) {
    do {
        for (int i = 0; i < mdl->members.count; i++) {
            Au_t au = (Au_t)mdl->members.origin[i];
            if (!member_type || au->member_type == member_type)
                if (au->ident && strcmp(au->ident, f) == 0)
                    return au;
        }
        if (!poly || mdl->context == mdl) break;
        mdl = mdl->context;
    } while (mdl);
    return null;
}

Au_t Au_context(array lex, int member_type, int traits) {
    for (int i = len(lex) - 1; i >= 0; i--) {
        Au_t au = (Au_t)lex->origin[i];
        if (!member_type || au->member_type == member_type) {
            if (!traits || (au->traits & traits) == traits)
                return au;
        }
    }
    return null;
}

Au_t Au_lexical(array lex, symbol f) {
    for (int i = len(lex) - 1; i >= 0; i--) {
        Au_t au = (Au_t)lex->origin[i];
        while (au) {
            for (int ii = 0; ii < au->members.count; ii++) {
                Au_t m = (Au_t)au->members.origin[ii];
                if (m->ident && strcmp(m->ident, f) == 0)
                    return m;
            }
            Au_t au_isa = isa(au);
            if (!is_class((Au)au)) break;
            if (au->context == au) break;
            au = au->context;
        }
    }
    return null;
}

Au_t Au_register(Au_t type, symbol ident, u32 member_type, u32 traits) {
    if (n_members == 0) {
        n_members   = 2048;
        member_pool = calloc(n_members, sizeof(struct _Au_combine));
        memset(member_pool, 0, n_members * sizeof(struct _Au_combine));
    }
    struct _Au_combine* cur = &member_pool[--n_members];
    cur->info.refs = 1;

    Au_t au = &cur->type;
    au->ident = ident ? strdup(ident) : null;

    au->member_type = member_type;
    au->traits = traits;

    if (type) {
        Au_t new_member = (Au_t)array_qpush((array)&type->members, (Au)&cur->type);
        new_member->context = type;
        //printf("new_member on type %s = %p (%i)\n", type->ident, new_member, n_members);
        return new_member;
    }
    return (Au_t)&cur->type;
}

static none Au_dealloc_iter(Au_t type) {
    array_dealloc((array)&type->members);
    array_dealloc((array)&type->args);
    free(type->ident);
    Au_drop((Au)type);
}

// intelligently drops the inlay array elements and allocated member data
none Au_dealloc_type(Au_t type) {
    Au info = head(type);
    if (info->refs == 1) {
        Au_dealloc_iter(type);
    }
    Au_drop((Au)type);
}

Au_t Au_register_type(Au_t type, symbol ident, u32 traits) {
    return Au_register(type, ident, AU_MEMBER_TYPE, traits);
}

Au_t Au_register_class(Au_t type, symbol ident) {
    return Au_register(type, ident, AU_MEMBER_TYPE, AU_TRAIT_CLASS);
}

Au_t Au_register_struct(Au_t type, symbol ident) {
    return Au_register(type, ident, AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
}

Au_t Au_register_func_ptr(Au_t type, symbol ident) {
    return Au_register(type, ident, AU_MEMBER_TYPE, AU_TRAIT_FUNCPTR);
}

Au_t Au_register_pointer(Au_t context, Au_t ref, symbol ident) {
    if (!ref->ptr) {
        ref->ptr = Au_register(context, ident, AU_MEMBER_TYPE, AU_TRAIT_POINTER);
        ref->ptr->src = (Au_t)hold(ref);
    }
    return ref->ptr;
}

Au_t Au_register_enum(Au_t context, symbol ident, u32 traits) {
    return Au_register(context, ident, AU_MEMBER_TYPE, AU_TRAIT_ENUM);
}

Au_t Au_register_enum_value(Au_t context, symbol ident, Au value) {
    Au_t res = Au_register(context, ident, AU_MEMBER_ENUMV, 0);
    res->value = (object)value;
    return res;
}

Au_t Au_register_member(Au_t context, symbol ident, Au_t type_mem, u32 member_type, u32 traits) {
    Au_t au = Au_register(context, ident, member_type, traits);
    au->type = type_mem;
    return au;
}

cstr copy_cstr(cstr input) {
    sz len = strlen(input);
    cstr res = calloc(len + 1, 1);
    memcpy(res, input, len);
    return res;
}

Au Au_header(Au a) {
    return (((struct _Au*)a) - 1);
}

none Au_register_init(func f) {
    if (call_last_count == call_last_alloc) {
        global_init_fn* prev      = call_last;
        num            alloc_prev = call_last_alloc;
        call_last_alloc           = 32 + (call_last_alloc << 1);
        call_last                 = calloc(call_last_alloc, sizeof(global_init_fn));
        if (prev) {
            memcpy(call_last, prev, sizeof(global_init_fn) * alloc_prev);
            free(prev);
        }
    }
    call_last[call_last_count++] = f;
}

Au_t Au_module(symbol name) {
    for (int i = 0; i < modules.data.count; i++) {
        Au_t m = (Au_t)modules.data.origin[i];
        if (strcmp(m->ident, name) == 0)
            return m;
    }
    return Au_register_module(name);
}

Au_t Au_global() {
    return au_module;
}

Au_t Au_register_module(symbol next_module) {
    struct _Au_combine* combine = calloc(1, sizeof(struct _Au_combine));
    Au_t m = &combine->type;
    m->member_type = AU_MEMBER_MODULE;
    m->ident = strdup(next_module);
    if (!au_module) {
        au_module = m;
        module = m;
    }
    array_qpush((array)&modules.data, (Au)m);
    return m;
}

none collective_init(collective a) {
}

ffi_method_t* method_with_address(handle address, Au_t rtype, array atypes, Au_t method_owner);

static bool has_pushed;

none push_type(Au_t type) {

    // on first call, we register our basic type structures:
    if (type == typeid(Au_t)) {
        has_pushed = true;
        module = Au_module("Au");
        module->members.unmanaged = true;

        // the first ever type we really register is the collective_abi
        Au_t au_collective = Au_register(module, "collective_abi",
            AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
        Au_register_member(au_collective, "count",     typeid(i32), AU_MEMBER_VAR, 0);
        Au_register_member(au_collective, "alloc",     typeid(i32), AU_MEMBER_VAR, 0);
        Au_register_member(au_collective, "hsize",     typeid(i32), AU_MEMBER_VAR, 0);
        Au_register_member(au_collective, "origin",    typeid(ARef), AU_MEMBER_VAR, 0);
        Au_register_member(au_collective, "first",     typeid(ARef), AU_MEMBER_VAR, 0); // these are known as referenced types (classes)
        Au_register_member(au_collective, "last",      typeid(ARef), AU_MEMBER_VAR, 0); 
        Au_register_member(au_collective, "hlist",     typeid(ARef), AU_MEMBER_VAR, 0);
        Au_register_member(au_collective, "unmanaged", typeid(bool), AU_MEMBER_VAR, 0);
        Au_register_member(au_collective, "assorted",  typeid(bool), AU_MEMBER_VAR, 0); 
        Au_register_member(au_collective, "last_type", typeid(ARef), AU_MEMBER_VAR, 0);

        Au_t au_t = type; // pushed from the first global ctr call
        au_t->member_type = AU_MEMBER_TYPE;
        au_t->traits = AU_TRAIT_CLASS;

        Au_t ctx = Au_register_member(au_t, "context",       typeid(Au_t), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "src",           typeid(Au_t), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "user",          typeid(Au_t), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "module",        typeid(Au_t), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "ptr",           typeid(Au_t), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "ident",         typeid(cstr), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "index",         typeid(i64),  AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "value",         typeid(ARef), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "member_type",   typeid(u8),   AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "operator_type", typeid(u8),   AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "access_type",   typeid(u8),   AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "reserved",      typeid(u8),   AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "traits",        typeid(u32),  AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "global_count",  typeid(i32),  AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "offset",        typeid(i32),  AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "size",          typeid(i32),  AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "isize",         typeid(i32),  AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "ptr",           typeid(ARef), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "ffi",           typeid(ARef), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, "af",            typeid(ARef), AU_MEMBER_VAR, 0);
        
        Au_t minfo = Au_register_member(au_t, "members_info", typeid(Au), AU_MEMBER_VAR, AU_TRAIT_INLAY);
        Au_register_member(au_t, "members", au_collective, AU_MEMBER_VAR, AU_TRAIT_INLAY);
        
        Au_t metainfo = Au_register_member(au_t, "meta_info", typeid(Au), AU_MEMBER_VAR, AU_TRAIT_INLAY);
        Au_register_member(au_t, "meta",  au_collective, AU_MEMBER_VAR, AU_TRAIT_INLAY);
        Au_register_member(au_t, "shape", typeid(shape), AU_MEMBER_VAR, 0);

        Au_t required_bits  = Au_register_member(au_t, "required_bits",  typeid(u64), AU_MEMBER_VAR, 0);
        required_bits->elements = 2;
        Au_t ft             = Au_register(au_t, null,
            AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
        Au_register_member(ft, "_none_", typeid(ARef), AU_MEMBER_VAR, 0);
        Au_register_member(au_t, null, ft, AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
        // this process is replicated in schema creation / etype_init
    }

    array_qpush((array)&module->members, (Au)type);

    type->af = (au_core)calloc(1, sizeof(struct _au_core));
    type->af->re_alloc = 1024;
    type->af->re = (object*)(Au*)calloc(1024, sizeof(Au));
    type->af->arb = primitive_ffi_arb(type);

    if ((type->traits & AU_TRAIT_POINTER) != 0) {
        type->src = type->meta.origin ? *(Au_t*)type->meta.origin : null;
    }

    if ((type->traits & AU_TRAIT_ABSTRACT) == 0) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if ((mem->traits & AU_TRAIT_REQUIRED) != 0 && (mem->member_type == AU_MEMBER_VAR))
                AF_set(type->required_bits, mem->index);
        }
    }

    for (int i = 0; i < call_after_count; i++) {
        struct _init_dep* f = &call_after[i];
        if (f->dep == type && f->call_after())
            f->dep = null;
    }
}

Au_t Au_current_module() {
    return module;
}

Au_t Au_scope_lookup(array a, string f) {
    cstr s = f->chars;
    for (int i = len(a) - 1; i >= 0; i--) {
        Au_t t = (Au_t)a->origin[i];
        while (t) {
            if (t->ident && strcmp(t->ident, s) == 0) return t;
            if (t->context == t) break;
            t = t->context;
        }
    }
    return null;
}

ARef Au_types(ref_i64 length) {
    *length = module->members.count;
    return module->members.origin;
}

Au_t Au_find_type(symbol name, Au_t m) {
    Au_t mod = m ? m : module;

    for (int i = 0; i < mod->members.count; i++) {
        Au_t type = (Au_t)mod->members.origin[i];
        if (strcmp(name, type->ident) == 0)
            return type;
    }

    if (mod != module)
        for (int i = 0; i < module->members.count; i++) {
            Au_t type = (Au_t)module->members.origin[i];
            if (strcmp(name, type->ident) == 0)
                return type;
        }
   
    return null;
}

AF* Au_AF_bits(Au a) {
    Au_t type = isa(a);
    u64* fields = (u64*)((i8*)a + type->typesize - (sizeof(void*) * 2));
    return fields;
}

void Au_AF_set_id(Au a, int id) {
    //Au_t t = isa(a);
    //u64*   f = Au_AF_bits(a);
    //AF_set(f, id);
}

void Au_AF_set_name(Au a, cstr name) {
    Au_t t = isa(a);
    Au_t m = find_member(t, AU_MEMBER_VAR, name, true);
    u64*   f = Au_AF_bits(a);
    AF_set(f, m->index);
}

i32 Au_AF_query_name(Au a, cstr name) {
    Au_t t = isa(a);
    Au_t m = find_member(t, AU_MEMBER_VAR, name, true);
    u64* f = Au_AF_bits(a);
    return (i32)AF_get(f, m->index);
}

bool Au_validator(Au a) {
    Au_t type = isa(a);

    u64* f = Au_AF_bits(a);
    if (((type->required_bits[0] & f[0]) != type->required_bits[0]) ||
        ((type->required_bits[1] & f[1]) != type->required_bits[1])) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t m = (Au_t)type->members.origin[i];
            if ((m->traits & AU_TRAIT_REQUIRED) != 0 && AF_get(f, m->index) == 0) {
                u8* ptr = (u8*)a + m->offset;
                Au* ref = (Au*)ptr;
                fault("required arg [%s] not set for class %s",
                    m->ident, type->ident);
            }
        }
        exit(2);
    }
    return true;
}

i32* enum_default(Au_t type) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if (mem->member_type & AU_MEMBER_ENUMV)
            return (i32*)mem->value;
    }
    return null;
}

static Au enum_member_value(Au_t type, Au_t mem) {
    if (type->src == typeid(u8))  return _u8(*(u8*)mem->value);
    if (type->src == typeid(i8))  return _i8(*(i8*)mem->value);
    if (type->src == typeid(u16)) return _u16(*(u16*)mem->value);
    if (type->src == typeid(i16)) return _i16(*(i16*)mem->value);
    if (type->src == typeid(u32)) return _u32(*(u32*)mem->value);
    if (type->src == typeid(i32)) return _i32(*(i32*)mem->value);
    if (type->src == typeid(u64)) return _u64(*(u64*)mem->value);
    if (type->src == typeid(i64)) return _i64(*(i64*)mem->value);
    if (type->src == typeid(f32)) return _f32(*(f32*)mem->value);
    fault("implement enum conversion: %s", type->ident);
    return null;
}

i32 evalue(Au_t type, cstr cs) {
    int cur = 0;
    int default_val = INT_MIN;
    bool single = strlen(cs) == 1;
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((mem->member_type & AU_MEMBER_ENUMV) &&
            (strcasecmp(mem->ident, cs) == 0)) {
            return *(i32*)enum_member_value(type, mem);
        }
    }
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((mem->member_type & AU_MEMBER_ENUMV) &&
            (tolower(mem->ident[0]) == tolower(cs[0]))) {
            return *(i32*)enum_member_value(type, mem);
        }
    }
    fault("enum not found");
    return 0;
}

string estring(Au_t type, i32 value) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if (mem->member_type & AU_MEMBER_ENUMV) {
            if (memcmp((void*)mem->value, (i32*)&value, mem->src->typesize) == 0)
                return string(mem->ident); 
        }
    }
    fault ("invalid enum-value of %i for type %s", value, type->ident);
    return null;
}

none debug() {
    return;
}

static none init_recur(Au a, Au_t current, raw last_init) {
    if (current == (Au_t)&Au_i.type) return;
    none(*init)(Au) = ((Au_f*)current)->ft.init;
    init_recur(a, current->context, (raw)init);
    if (init && init != (none*)last_init) init(a); 
}

string numeric_cast_string(numeric a) {
    Au_t t = isa(a);
    if (t == typeid(i8))  return f(string, "%hhi", *(i8*) a);
    if (t == typeid(i16)) return f(string, "%hi",  *(i16*)a);
    if (t == typeid(i32)) return f(string, "%i",   *(i32*)a);
    if (t == typeid(i64)) return f(string, "%lli", *(i64*)a);

    if (t == typeid(u8))  return f(string, "%hhu", *(u8*) a);
    if (t == typeid(u16)) return f(string, "%hu",  *(u16*)a);
    if (t == typeid(u32)) return f(string, "%u",   *(u32*)a);
    if (t == typeid(u64)) return f(string, "%llu", *(u64*)a);

    if (t == typeid(f32)) return f(string, "%f",   *(f32*)a);
    if (t == typeid(f64)) return f(string, "%lf",  *(f64*)a);

    fault("numeric type not handled in string cast: %s", t->ident);
    return null;
}

numeric numeric_operator__add(numeric a, numeric b) {
    Au_t type_a = isa(a);
    Au_t type_b = isa(b);
    
    if (type_a == type_b) {
        if (type_a == typeid(i8))  return  _i8(* (i8*)a + * (i8*)b);
        if (type_a == typeid(i16)) return _i16(*(i16*)a + *(i16*)b);
        if (type_a == typeid(i32)) return _i32(*(i32*)a + *(i32*)b);
        if (type_a == typeid(i64)) return _i64(*(i64*)a + *(i64*)b);
        if (type_a == typeid(f32)) return _f32(*(f32*)a + *(f32*)b);
        if (type_a == typeid(f64)) return _f64(*(f64*)a + *(f64*)b);
    } else {
        fault("implement dislike add");
    }
    return null;
}

numeric numeric_operator__sub(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__mul(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__div(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__or(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__and(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__xor(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__mod(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__right(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__left(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__compare(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__equal(numeric a, numeric b) {
    return null;
}

numeric numeric_operator__not_equal(numeric a, numeric b) {
    return null;
}

none    numeric_operator__assign(numeric a, numeric b) {
    Au_t type_a = isa(a);
    Au_t type_b = isa(b);
    num sz = type_a->typesize < type_b->typesize ? type_a->typesize : type_b->typesize;
    memcpy(a, b, sz);
}

Au Au_initialize(Au a) {
    Au   f = header(a);
    if (f->type->traits & AU_TRAIT_USER_INIT) return a; 

    #ifndef NDEBUG
    Au_validator(a);
    #endif
    
    init_recur(a, f->type, null);
    hold_members(a);
    return a;
}

__thread __error_t* Au_error_top = NULL;

void halt(string msg) {
    if (!Au_error_top) {
#ifndef NDEBUG
        raise(SIGTRAP);
        exit(1);
#else
        abort();
#endif
    }
    Au_error_top->message = msg;
    longjmp(Au_error_top->env, 1);
}



pid_t _last_pid = 0;

i64 command_last_pid() {
    return (i64)_last_pid;
}

command command_with_cstr(command cmd, cstr buf) {
    int ln = strlen(buf);
    cmd->count = ln;
    cmd->alloc = ln + 1;
    cmd->chars = calloc(1, ln + 1);
    memcpy(cmd->chars, buf, ln + 1);
    return cmd;
}

string command_run(command cmd) {
    int pipe_in[2];   // for writing command input to sh -s
    int pipe_out[2];  // for reading stdout from sh -s

    pipe(pipe_in);
    pipe(pipe_out);

    pid_t pid = fork();

    if (pid == 0) {
        dup2(pipe_in[0], STDIN_FILENO);   // read command
        dup2(pipe_out[1], STDOUT_FILENO); // write output
        dup2(pipe_out[1], STDERR_FILENO); // optional: stderr too

        close(pipe_in[1]);
        close(pipe_out[0]);
        execlp("sh", "sh", "-s", NULL);
        _exit(127); // exec failed
    }

    close(pipe_in[0]);  // parent writes only
    close(pipe_out[1]); // parent reads only

    FILE *out = fdopen(pipe_in[1], "w");
    fprintf(out, "%s\n", cstring(cmd));
    fflush(out);
    fclose(out);  // send EOF to child

    char buffer[1024];
    string result = string(alloc, 1024);
    ssize_t bytes;
    while ((bytes = read(pipe_out[0], buffer, sizeof(buffer))) > 0) {
        append_count(result, buffer, bytes);
    }

    close(pipe_out[0]);

    int status;
    waitpid(pid, &status, 0);
    for (;result->count;) {
        char l = result->chars[result->count - 1];
        if (l == '\n' || l == '\r')
            ((cstr)result->chars)[--result->count] = 0;
        else
            break;
    }
    return result;
}

int command_exec(command cmd) {
    if (starts_with(cmd, "export ")) {
        string a = mid(cmd, 7, len(cmd) - 7);
        int i = index_of(a, "=");
        assert(i >= 1, "invalid syntax"); 
        string var   = trim(mid(a, 0, i));
        string value = trim(mid(a, i + 1, len(a) - i - 1));
        setenv((cstr)var->chars, (cstr)value->chars, 1);
        print("export: setting %o to %o", var, value);
        return 0;
    }

    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();

    if (pid == 0) {
        // child
        setpgid(0, 0);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); // close write
        execlp("sh", "sh", "-s", null); // or "bash"
        _exit(0);
    } else if (pid > 0) {
        _last_pid = pid;
        // parent
        close(pipefd[0]); // close read
        FILE *out = fdopen(pipefd[1], "w");
        cstr verbose = getenv("VERBOSE");
        if (verbose && strcmp(verbose, "0") != 0) {
            printf("----------------------\n");
            printf("%s\n", cstring(cmd));
        }
        fprintf(out, "%s\n", cstring(cmd));
        fflush(out);
        close(pipefd[1]);

        int status;
        int result;
        do {
            result = waitpid(pid, &status, 0);
        } while (result == -1 && errno == EINTR);
        _last_pid = 0;

        if (result == -1) {
            perror("waitpid");
            return -123456;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child terminated by signal: %s\n", strsignal(WTERMSIG(status)));
            return -128 - WTERMSIG(status);  // POSIX style
        }
        fprintf(stderr, "unknown termination\n");
        return -9999;
    } else {
        perror("fork");
        return -1234;
    }
}

static int all_type_alloc;

int alloc_count(Au_t type) {
    return type ? type->global_count : all_type_alloc;
}

Au alloc_instance(Au_t type, int n_bytes, int recycle_size) {
    Au a = null;
    au_core af = type->af;
    bool use_recycler = false; //af && n_bytes == recycle_size;

    if (use_recycler && af->re_count) {
        a = (Au)af->re[--af->re_count];
        memset(a, 0, n_bytes);
    } else {
        type->global_count++;
        all_type_alloc++;

        a = calloc(1, n_bytes);
        a->recycle = use_recycler;
    }
    a->refs = 1;
    if (use_recycler) {
        if ((af->af_count + 1) >= af->af_alloc) {
            i64 prev_size = af->af_alloc;
            i64 next_size = af->af_alloc << 1;
            if (next_size == 0) next_size = 1024;
            void* prev = af->af4;
            af->af4 = calloc(1, next_size * sizeof(Au*));
            memcpy(af->af4, prev, prev_size * sizeof(Au*));
            af->af4[0] = 0x00;
            af->af_alloc = next_size;
        }
        a->af_index = 1 + af->af_count;
        af->af4[a->af_index] = (object)a;
        af->af_count++;
    }
    return a;
}

Au alloc_dbg(Au_t type, num count, cstr source, int line) {
    sz map_sz = sizeof(map);
    sz _sz   = sizeof(struct _Au);
    Au a = alloc_instance(type,
        _sz + type->typesize * count, _sz + type->typesize);
    a->type       = type;
    a->data       = &a[1];
    a->count      = count;
    a->alloc      = count;
    a->source     = source;
    a->line       = line;
    return a->data; /// return fields (Au)
}


Au alloc(Au_t type, num count, Au_t* meta) {
    sz map_sz = sizeof(map);
    sz _sz   = sizeof(struct _Au);
    Au a = alloc_instance(type,
        _sz + type->typesize * count, _sz + type->typesize);
    a->type       = type;
    a->data       = &a[1];
    a->count      = count;
    a->alloc      = count;
    if (meta && type->meta.count > 0) {
        for (int i = 0; i < type->meta.count; i++) {
            Au_t m = meta[i];
            Au_t object_type = isa(m);
            Au_t ref = ((Au_t*)&type->meta)[i];
            if (object_type) {
                verify (inherits(object_type, ref), "expected object of compatible-type %s", m->ident);
            }
            a->meta[i] = (Au)m;
        }
    }
    return a->data;
}

Au alloc_new(Au_t type, num count, Au_t* meta) {
    return alloc(type, count, meta);
}

Au alloc2(Au_t type, Au_t scalar, shape s) {
    i64 _sz      = sizeof(struct _Au);
    i64 count     = shape_total(s);
    Au a      = alloc_instance(type,
        _sz + scalar->typesize * count, _sz + scalar->typesize);
    a->scalar     = scalar;
    a->type       = type;
    a->data       = &a[1];
    a->shape      = hold(s);
    a->count      = count;
    a->alloc      = count;
    return a->data;
}

ffi_method_t* method_with_address(handle address, Au_t rtype, array args, Au_t method_owner) {
    const num max_args = 16;
    ffi_method_t* method = calloc(1, sizeof(ffi_method_t));
    method->ffi_cif  = calloc(1,        sizeof(ffi_cif));
    method->ffi_args = calloc(max_args, sizeof(ffi_type*));
    method->atypes   = args;
    method->rtype    = rtype;
    method->address  = address;
    assert(args->count <= max_args, "adjust arg maxima");
    ffi_type **ffi_args = (ffi_type**)method->ffi_args;
    for (num i = 0; i < args->count; i++) {
        Au_t a_type   = (Au_t)args->origin[i];
        ffi_args[i]   = primitive_ffi_arb(a_type);
    }
    ffi_status status = ffi_prep_cif(
        (ffi_cif*) method->ffi_cif, FFI_DEFAULT_ABI, args->count,
        (ffi_type*)((rtype->traits & AU_TRAIT_ABSTRACT) ?
        primitive_ffi_arb(method_owner) : primitive_ffi_arb(rtype)), ffi_args);
    assert(status == FFI_OK, "status == %i", (i32)status);
    return method;
}

Au Au_method_call(Au_t m, array args) {
    if (!m->ffi) m->ffi = method_with_address(m->value, m->type, (array)&m->args, m->context);
    ffi_method_t* a = m->ffi;
    const num max_args = 8;
    none* arg_values[max_args];
    assert(args->count == a->atypes->count, "arg count mismatch");
    for (num i = 0; i < args->count; i++) {
        Au_t arg_type = (Au_t)a->atypes->origin[i];
        arg_values[i] = (arg_type->traits & (AU_TRAIT_PRIMITIVE | AU_TRAIT_ENUM)) ? 
            (none*)args->origin[i] : (none*)&args->origin[i];
    }
    none* result[8]; /// enough space to handle all primitive data
    ffi_call((ffi_cif*)a->ffi_cif, a->address, result, arg_values);
    if (a->rtype->traits & AU_TRAIT_PRIMITIVE)
        return primitive(a->rtype, result);
    else if (a->rtype->traits & AU_TRAIT_ENUM) {
        Au res = alloc(a->rtype, 1, null);
        verify(a->rtype->src == typeid(i32), "i32 enums supported");
        *((i32*)res) = *(i32*)result;
        return res;
    } else
        return (Au) result[0];
}

/// this calls type methods
Au method(Au_t type, cstr method_name, array args) {
    Au_t mem = find_member(type, AU_MEMBER_FUNC, method_name, false);
    assert(mem->ffi, "method not set");
    Au res = Au_method_call(mem, args);
    return res;
}

Au convert(Au_t type, Au input) {
    if (type == isa(input)) return input;
    return construct_with(type, input, null);
}

Au Au_method_vargs(Au a, Au_t mem, int n_args, ...) {
    assert(mem->ffi, "method not set");
    ffi_method_t* m = mem->ffi;
    va_list  vargs;
    va_start(vargs, n_args);
    array args = new(array, alloc, n_args + 1);
    push(args, a);
    for (int i = 0; i < n_args; i++) {
        Au arg = va_arg(vargs, Au);
        push(args, arg);
    }
    va_end(vargs);
    Au res = Au_method_call(mem, args);
    return res;
}


int fault_level;

static __attribute__((constructor)) bool Aglobal_AF();

string Au_cast_string(Au a);
string numeric_cast_string(numeric a);

none Au_member_override(Au_t type, Au_t type_mem, AFlag f) {
    Au_t base = type->context;
    if  (base) base = base->context;

    while (base) {
        for (num i = 0; i < base->members.count; i++) {
            Au_t m = (Au_t)base->members.origin[i];
            if (m->member_type == f && strcmp(m->ident, type_mem->ident) == 0) {
                type_mem->offset        = m->offset; // todo: better idea to use src on type_mem
                type_mem->args          = m->args;
                type_mem->src           = m->type;
                type_mem->member_type   = m->member_type;// | AU_MEMBER_OVERRIDE;
                type_mem->is_override   = 1;
                type_mem->index         = m->index;

                verify(m->index, "method %s.%s cannot be overridden\n", base->ident, m->ident);

                verify(m->value, "method pointer not set on source yet (%s.%s); cannot override\n",
                    base->ident, m->ident);
                 
                struct _string*(*base_method)(Au) = (void*)((ARef)&base->ft.__none__)[m->index];
                struct _string*(*ptr_method)(Au) = (void*)m->value;
                verify(base_method == m->value, "method not stored in base table correctly");
                return;
            }
        }
        if (base == typeid(Au)) break;
        base = base->context;
    }

    fprintf(stderr, "override could not find member %s from type %s\n", type_mem->ident, type->ident);

    base = type->context;
    while (base) {
        for (num i = 0; i < base->members.count; i++) {
            Au_t m = (Au_t)base->members.origin[i];
            printf("existing member on type %s = %p\n", base->ident, m);
            if (m->ident && strcmp(m->ident, type_mem->ident) == 0) {
                type_mem->offset = m->offset; // todo: better idea to use src on type_mem
                type_mem->args   = m->args;
                type_mem->src    = m->type;
                type_mem->member_type = m->member_type;// | AU_MEMBER_OVERRIDE;
                ARef ptr_find = (ARef)m->value;
                verify(m->index, "method %s.%s cannot be overridden\n", base->ident, m->ident);
                verify(m->value, "method pointer not set on source yet (%s.%s); cannot override\n",
                    base->ident, m->ident);
                ((ARef)&type->ft)[m->index] = (Au)m->value;
                return;
            }
        }
        if (base == typeid(Au)) break;
        base = base->context;
    }
}

none Au_engage(cstrs argv) {
    Au_t f32_type = typeid(f32);
    if (started) return;

    int argc    = 0;
    if (argv) while (argv[argc]) argc++;
    started     = true;
    fault_level = level_err;
    log_funcs   = hold(map(hsize, 32, unmanaged, true));

    /// initialize logging; * == all
    bool explicit_listen = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            string s = string(argv[i + 1]);
            array  a = split(s, ",");
            each(a, string, s) {
                set(log_funcs, (Au)(eq(s, "all") ? string("*") : s), _bool(true));
                explicit_listen = true;
            }
        }
    }
 
#ifndef NDEBUG
    if (!explicit_listen)
        set(log_funcs, (Au)string("*"), _bool(true));
#endif

    if (len(log_funcs)) {
        string topics = string(alloc, 32);
        pairs(log_funcs, i) {
            if (len(topics) > 0)
                append(topics, ", ");
            concat(topics, (string)i->key);
        }
        printf("listening-to: %s\n", topics->chars);
    }

    if (argv) {
        path sh = path_share_path();
        if (sh) cd(sh);
    }

    // call user-defined module initializers (after we have initialized)
    for (int i = 0; i < call_last_count; i++)
        call_last[i]();
    /*
    if (!app_schema) {
        string default_arg = null;
        if (item f = def->fifo->first; f; f = f->next) {
            default_arg = instanceof(f->key, string);
            if (default_arg)
                break;
        }
        return Au_arguments(argc, argv, def, default_arg);
    }
    */
}

map Au_args(cstrs argv, symbol default_arg, ...) {
    int argc = 0;
    while (argv[argc]) argc++;
    va_list  args;
    va_start(args, default_arg);
    symbol arg = default_arg;
    map    defaults = map(assorted, true);
    while (arg) {
        Au val = va_arg(args, Au);
        set(defaults, (Au)string(arg), hold(val));
        arg = va_arg(args, symbol);
    }
    va_end(args);
    return Au_arguments(argc, argv, defaults,
        (Au)(default_arg ? string(default_arg) : null));
}

none Au_tap(symbol f, hook sub) {
    string fname = string(f);
    set(log_funcs, (Au)fname, sub ? (Au)sub : (Au)_bool(true)); /// if subprocedure, then it may receive calls for the logging
}

none Au_untap(symbol f) {
    string fname = string(f);
    set(log_funcs, (Au)fname, _bool(false));
}

Au_t find_ctr(Au_t type, Au_t with, bool poly) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((mem->member_type & AU_MEMBER_CONSTRUCT) && meta_index((Au)mem, 0) == with)
            return mem;
    }
    if (poly && type->context && type->context != typeid(Au))
        return find_ctr(type->context, with, true);
    return 0;
}


Au_t find_member(Au_t type, AFlag memflags, symbol name, bool poly) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((memflags == 0) || (mem->member_type & memflags) && strcmp(mem->ident, name) == 0)
            return mem;
    }
    if (poly && type->context && type->context != typeid(Au))
        return find_member(type->context, memflags, name, true);
    return 0;
}

bool Au_is_inlay(Au_t m) {
    return (m->type->traits & AU_TRAIT_STRUCT    | 
            m->type->traits & AU_TRAIT_PRIMITIVE | 
            m->type->traits & AU_TRAIT_ENUM      | 
            m->type->traits & AU_TRAIT_INLAY) != 0;
}

none Au_hold_members(Au a) {
    Au_t type = isa(a);
    while (type != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            Au   *mdata = (Au*)((cstr)a + mem->offset);
            if (mem->member_type == AU_MEMBER_VAR)
                if (!Au_is_inlay(mem) && *mdata) {
                    if (mem->meta.origin && *(Au_t*)mem->meta.origin == typeid(weak))
                        continue;
                    Au member_value = *mdata;
                    Au head = header(member_value);
                    if (head->refs > 0) head->refs++;
                }
        }
        type = type->context;
    }
}

Au Au_set_property(Au a, symbol name, Au value) {
    Au_t type = isa(a);
    Au_t m = find_member(type, AU_MEMBER_VAR, (cstr)name, true);
    member_set(a, m, value);
    return value;
}


Au Au_get_property(Au a, symbol name) {
    Au_t type = isa(a);
    Au_t m = find_member(type, AU_MEMBER_VAR, (cstr)name, true);
    verify(m, "%s not found on Au %s", name, type->ident);
    Au *mdata = (Au*)((cstr)a + m->offset);
    Au  value = *mdata;
    return Au_is_inlay(m) ? primitive(m->type, mdata) : value;
}

map Au_arguments(int argc, cstrs argv, map default_values, Au default_key) {
    map result = new(map, hsize, 16, assorted, true);
    for (item ii = default_values->first; ii; ii = ii->next) {
        Au k = ii->key;
        Au v = ii->value;
        set(result, (Au)k, v);
    }
    int    i = 1;
    bool found_single = false;
    while (i < argc + 1) {
        symbol arg = argv[i];
        if (!arg) {
            i++;
            continue;
        }
        if (arg[0] == '-') {
            // -s or --silver
            bool doub  = arg[1] == '-';
            string s_key = new(string, chars, (cstr)&arg[doub + 1]);
            string s_val = new(string, chars, (cstr)argv[i + 1]);

            for (item f = default_values->first; f; f = f->next) {
                /// import Au types from runtime
                Au def_value = f->value;
                Au_t   def_type = def_value ? (Au_t)isa(def_value) : typeid(string);
                assert(f->key == f->key, "keys do not match"); /// make sure we copy it over from refs
                if ((!doub && strncmp(((string)f->key)->chars, s_key->chars, 1) == 0) ||
                    ( doub && compare(f->key, (Au)s_key) == 0)) {
                    /// inter-op with Au-based Au sells it.
                    /// its also a guide to use the same schema
                    Au value = formatter(def_type, null, (Au)false, "%o", s_val);
                    assert(isa(value) == def_type, "");
                    set(result, (Au)f->key, value);
                }
            }
        } else if (!found_single && default_key) {
            Au default_key_obj = header(default_key);
            string s_val     = new(string, chars, (cstr)arg);
            Au def_value = get(default_values, default_key);
            Au_t  def_type  = isa(def_value);
            Au value     = formatter(def_type, null, (Au)false, "%o", s_val);
            set(result, (Au)default_key, value);
            found_single = true;
        }
        i += 2;
    }
    return result;
}

Au primitive(Au_t type, none* data) {
    Au copy = alloc(type, 1, null);
    memcpy(copy, data, type->typesize);
    return copy;
}

Au _i8   (i8 data)   { return primitive(typeid(i8),   &data); }
Au _u8   (u8 data)   { return primitive(typeid(u8),   &data); }
Au _i16  (i16 data)  { return primitive(typeid(i16),  &data); }
Au _u16  (u16 data)  { return primitive(typeid(u16),  &data); }
Au _i32  (i32 data)  { return primitive(typeid(i32),  &data); }
Au _u32  (u32 data)  { return primitive(typeid(u32),  &data); }
Au _i64  (i64 data)  { return primitive(typeid(i64),  &data); }
Au i      (i64 data)  { return primitive(typeid(i64),  &data); }
Au _sz   (sz  data)  { return primitive(typeid(sz),   &data); }
Au _u64  (u64 data)  { return primitive(typeid(u64),  &data); }
Au _fp16 (fp16* data) { return primitive(typeid(fp16), data); }
Au _bf16 (bf16* data) { return primitive(typeid(bf16), data); }
Au _f32  (f32 data)  { return primitive(typeid(f32),  &data); }
Au _f64  (f64 data)  { return primitive(typeid(f64),  &data); }
Au float32(f32 data) { return primitive(typeid(f32),  &data); }
Au real64(f64 data)  { return primitive(typeid(f64),  &data); }
Au _cstr(cstr data) { return primitive(typeid(cstr), &data); }
Au _none()          { return primitive(typeid(none), NULL);  }
Au _bool(bool data) { return primitive(typeid(bool), &data); }

/// Au -------------------------
none Au_init(Au a) { }

none Au_drop_members(Au a) {
    Au   f = header((Au)a);
    Au_t type = f->type;
    while (type != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t m = (Au_t)type->members.origin[i];
            if ((m->member_type == AU_MEMBER_VAR) &&
                    !Au_is_inlay(m)) {
                if (m->args.origin && *(Au_t*)m->args.origin == typeid(weak))
                    continue;
                //printf("Au_dealloc: drop member %s.%s (%s)\n", type->ident, m->ident, m->type->ident);
                Au*  ref = (Au*)((u8*)a + m->offset);
                drop(*ref);
                *ref = null;
            }
        }
        type = type->context;
    }
}

none Au_dealloc(Au a) { 
    Au        f = header(a);
    Au_t type = f->type;
    if (!(type->traits & AU_TRAIT_USER_INIT)) // composer does this for an 'unmount' operation, and its non-mount results in no holds at all (which is why we must compensate here)
        drop_members(a);
    if ((Au)f->data != (Au)a) {
        drop(f->data);
        f->data = null;
    }
}
u64  Au_hash      (Au a) { return (u64)(size_t)a; }
bool Au_cast_bool (Au a) {
    Au info = header(a);
    bool has_count = info->count > 0;
    if (has_count && info->type == typeid(bool))
        return *(bool*)a;

    return has_count;
}

Au_t Au_member_type(Au_t type, AFlag mt, Au_t f, bool poly) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((mt == 0) || (mem->member_type & mt) && (mem->type == f))
            return mem;
    }
    if (poly && type->context && type->context != typeid(Au))
        return Au_member_type(type->context, mt, f, true);
    return 0;
}

static i64 read_integer(Au data) {
    Au_t data_type = isa(data);
    i64 v = 0;
         if (data_type == typeid(i8))   v = *(i8*)  data;
    else if (data_type == typeid(i16))  v = *(i16*) data;
    else if (data_type == typeid(i32))  v = *(i32*) data;
    else if (data_type == typeid(i64))  v = *(i64*) data;
    else if (data_type == typeid(u8))   v = *(u8*)  data;
    else if (data_type == typeid(u16))  v = *(u16*) data;
    else if (data_type == typeid(u32))  v = *(u32*) data;
    else if (data_type == typeid(u64))  v = *(u64*) data;
    else fault("unknown data");
    return v;
}

static cstr ws(cstr p) {
    cstr scan = p;
    bool retry = true;
    while (retry) {
        retry = false;
        while (*scan && isspace(*scan))
            scan++;
        if (*scan == '#') {
            scan++;
            while (*scan && *scan != '\n')
                scan++;
            retry = *scan != 0;
        } else if (strncmp(scan, "/*", 2) == 0) {
            scan += 2;
            scan = strstr(scan, "*/");
            if (scan) {
                scan = scan + 2;
                retry = true;
            }
        }
    }
    return scan;
}

string prep_cereal(cereal cs) {
    cstr   scan = (cstr)cs.value;
    string res;

    if (*scan == '\"') {
        scan++; // skip opening quote
        cstr start = scan;
        bool escaped = false;

        while (*scan) {
            if (escaped) {
                escaped = false;
            } else if (*scan == '\\') {
                escaped = true;
            } else if (*scan == '\"') {
                break;
            }
            scan++;
        }

        assert(*scan == '\"', "missing end-quote");

        i64 len = scan - start;
        res = string(alloc, len); // string handles +1 for null-terminator
        memcpy((cstr)res->chars, start, len);
    } else
        res = string((symbol)scan);

    return res;
}

Au Au_with_cstrs(Au a, cstrs argv) {
    Au_engage(argv);
    int argc = 0;
    while (argv[argc]) { // C standard puts a null char* on end, by law (see: Brannigans law)
        cstr arg = argv[argc];
        if (arg[0] == '-') {
            bool single = arg[1] != '-';
            Au_t mem    = null;
            Au_t type   = isa(a);
            while (type != typeid(Au)) {
                for (num i = 0; i < type->members.count; i++) {
                    Au_t m = (Au_t)type->members.origin[i];
                    if ((m->member_type == AU_MEMBER_VAR) && 
                        ( single &&        m->ident[0] == arg[1]) ||
                        (!single && strcmp(m->ident,     &arg[2]) == 0)) {
                        mem = m;
                        break;
                    }
                }
                type = type->context;
            }
            verify(mem, "member not found: %s", &arg[1 + !single]);
            cstr value = argv[++argc];
            verify(value, "expected value after %s", &arg[1 + !single]);
            
            Au conv = convert(mem->type, (Au)string(value));
            Au_set_property(a, mem->ident, conv);
        }
        argc++;
    }
    return a;
}

Au Au_with_cereal(Au a, cereal _cs) {
    cstr cs = _cs.value;
    sz len = strlen(cs);
    Au        f = header(a);
    Au_t type = f->type;
    if      (type == typeid(f64)) sscanf(cs, "%lf",  (f64*)a);
    else if (type == typeid(f32)) sscanf(cs, "%f",   (f32*)a);
    else if (type == typeid(i32)) sscanf(cs, "%i",   (i32*)a);
    else if (type == typeid(u32)) sscanf(cs, "%u",   (u32*)a);
    else if (type == typeid(i64)) sscanf(cs, "%lli", (i64*)a);
    else if (type == typeid(u64)) sscanf(cs, "%llu", (u64*)a);
    else if (type == typeid(bool)) {
        *(bool*)a = (cs[0] == 't' || cs[0] == 'T' || cs[0] == '1');
    }
    else if (type == typeid(string)) {
        string  res = (string)a;
        sz     a_ln = len > -1 ? len : strlen(cs);
        res->chars  = calloc(a_ln + 1, 1);
        res->count    = a_ln;
        memcpy((cstr)res->chars, cs, a_ln);
        return (Au)res;
    }
    else {
        bool can = constructs_with(f->type, typeid(string));
        if (can) {
            return construct_with(f->type, (Au)string(cs), null);
        } else if (constructs_with(f->type, typeid(cstr))) {
            return construct_with(f->type, (Au)string(cs), null);
        }
        constructs_with(f->type, typeid(cstr));
        printf("implement ctr cstr for %s\n", f->type->ident);
        exit(-1);
    }
    return a;
}

bool constructs_with(Au_t type, Au_t with_type) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((mem->member_type & AU_MEMBER_CONSTRUCT) != 0) {
            if (mem->type == with_type)
                return true;
        }
    }
    return false;
}

/// used by parse (from json) to construct objects from data
Au construct_with(Au_t type, Au data, ctx context) {
    if (type == typeid(map)) {
        verify(isa(data) == typeid(map), "expected map");
        return hold(data);
    }

    /// this will lookup ways to construct the type from the available data
    Au_t data_type = isa(data);
    Au result = null;
    map    mdata  = null;

    /// construct with map of fields
    if (!(type->traits & AU_TRAIT_PRIMITIVE) && data_type == typeid(map)) {
        map m = (map)data;
        result = alloc(type, 1, null);
        pairs(m, i) {
            verify(isa(i->key) == typeid(string),
                "expected string key when constructing Au from map");
            string s_key = (string)instanceof(i->key, string);
            Au_set_property(result, s_key->chars, i->value);
        }
        mdata = m;
    }
    /// check for identical constructor
    Au_t au = type;
    while (au != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            
            if (!result && mem->member_type == AU_MEMBER_CONSTRUCT) {
                none* addr = mem->value;
                /// no meaningful way to do this generically, we prefer to call these first
                if (mem->type == typeid(path) && data_type == typeid(string)) {
                    result = alloc(type, 1, null);
                    result = ((Au(*)(Au, path))addr)(result, path(((string)data)));
                    verify(Au_validator(result), "invalid Au");
                    break;
                }
                if ((mem->type == typeid(cstr) || mem->type == typeid(symbol)) && 
                        data_type == typeid(string)) {
                    result = alloc(type, 1, null);
                    result = ((Au(*)(Au, cstr))addr)(result, ((string)data)->chars);
                    verify(Au_validator(result), "invalid Au");
                    break;
                }
                if (mem->type == data_type) {
                    result = alloc(type, 1, null);
                    result = ((Au(*)(Au, Au))addr)(result, data);
                    verify(Au_validator(result), "invalid Au");
                    break;
                }
            } else if (context && result && mdata) {
                // lets set required properties from context
                string k = string(mem->ident);
                if ((mem->traits & AU_TRAIT_REQUIRED) != 0 && (mem->member_type == AU_MEMBER_VAR) && 
                    !contains(mdata, k))
                {
                    Au from_ctx = get(context, k);
                    verify(from_ctx,
                        "context requires property: %s (%s) in class %s",
                            mem->ident, mem->type->ident, au->ident);
                    member_set(result, mem, from_ctx);
                }
            }
        }
        au = au->context;
    }

    /// simple enum conversion, with a default handled in Au_enum_value and type-based match here
    if (!result)
    if (type->traits & AU_TRAIT_ENUM) {
        i64 v = 0;
        if (data_type->traits & AU_TRAIT_INTEGRAL)
            v = read_integer(data_type);
        else if (data_type == typeid(symbol) || data_type == typeid(cstr))
            v = evalue (type, (cstr)data);
        else if (data_type == typeid(string))
            v = evalue (type, (cstr)((string)data)->chars);
        else
            v = evalue (type, null);
        result = alloc(type, 1, null);
        *((i32*)result) = (i32)v;
    }

    /// check if we may use generic Au from string
    if (!result)
    if ((type->traits & AU_TRAIT_PRIMITIVE) && (data_type == typeid(string) ||
                                               data_type == typeid(cstr)   ||
                                               data_type == typeid(symbol))) {
        result = alloc(type, 1, null);
        if (data_type == typeid(string))
            Au_with_cereal(result, (cereal) { .value = (cstr)((string)data)->chars } );
        else
            Au_with_cereal(result, (cereal) { .value = data });
    }

    /// check for compatible constructor
    if (!result)
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if (!mem->value) continue;
        none* addr = mem->value;
        /// check for compatible constructors
        if (mem->member_type == AU_MEMBER_CONSTRUCT) {
            u64 combine = mem->type->traits & data_type->traits;
            if (combine & AU_TRAIT_INTEGRAL) {
                i64 v = read_integer(data);
                result = alloc(type, 1, null);
                     if (mem->type == typeid(i8))   ((none(*)(Au, i8))  addr)(result, (i8)  v);
                else if (mem->type == typeid(i16))  ((none(*)(Au, i16)) addr)(result, (i16) v);
                else if (mem->type == typeid(i32))  ((none(*)(Au, i32)) addr)(result, (i32) v);
                else if (mem->type == typeid(i64))  ((none(*)(Au, i64)) addr)(result, (i64) v);
            } else if (combine & AU_TRAIT_REALISTIC) {
                result = alloc(type, 1, null);
                if (mem->type == typeid(f64))
                    ((none(*)(Au, double))addr)(result, (double)*(float*)data);
                else
                    ((none(*)(Au, float)) addr)(result, (float)*(double*)data);
                break;
            } else if ((mem->type == typeid(symbol) || mem->type == typeid(cstr)) && 
                       (data_type == typeid(symbol) || data_type == typeid(cstr))) {
                result = alloc(type, 1, null);
                ((none(*)(Au, cstr))addr)(result, data);
                break;
            } else if ((mem->type == typeid(string)) && 
                       (data_type == typeid(symbol) || data_type == typeid(cstr))) {
                result = alloc(type, 1, null);
                ((none(*)(Au, string))addr)(result, string((symbol)data));
                break;
            } else if ((mem->type == typeid(symbol) || mem->type == typeid(cstr)) && 
                       (data_type == typeid(string))) {
                result = alloc(type, 1, null);
                ((none(*)(Au, cstr))addr)(result, (cstr)((string)data)->chars);
                break;
            }
        }
    }

    // set field bits here (removed case where json parser was doing this)
    if (result && data_type == typeid(map)) {
        map f = (map)data;
        pairs(f, i) {
            string name = i->key;
            Au_AF_set_name(result, (cstr)name->chars);
        }
    }
    
    if (!result && data) {
        // if constructor not found
        verify(data_type == typeid(string) || data_type == typeid(path),
            "failed to construct type %s with %s", type->ident, data_type->ident);

        // load from presumed .json as fallback
        path f = (data_type == typeid(string)) ? path((string)data) : (path)data;
        return load(f, type, null);
    }
    return result ? Au_initialize(result) : null;
}

none serialize(Au_t type, string res, Au a) {
    if (type->traits & AU_TRAIT_PRIMITIVE) {
        char buf[128];
        int len = 0;
        if      (type == typeid(bool)) len = sprintf(buf, "%s", *(bool*)a ? "true" : "false");
        else if (type == typeid(i64)) len = sprintf(buf, "%lld", *(i64*)a);
        else if (type == typeid(num)) len = sprintf(buf, "%lld", *(i64*)a);
        else if (type == typeid(i32)) len = sprintf(buf, "%d",   *(i32*)a);
        else if (type == typeid(i16)) len = sprintf(buf, "%hd",  *(i16*)a);
        else if (type == typeid(i8))  len = sprintf(buf, "%hhd", *(i8*) a);
        else if (type == typeid(u64)) len = sprintf(buf, "%llu", *(u64*)a);
        else if (type == typeid(u32)) len = sprintf(buf, "%u",   *(u32*)a);
        else if (type == typeid(u16)) len = sprintf(buf, "%hu",  *(u16*)a);
        else if (type == typeid(u8))  len = sprintf(buf, "%hhu", *(u8*) a);
        else if (type == typeid(f64)) len = sprintf(buf, "%f",   *(f64*)a);
        else if (type == typeid(f32)) len = sprintf(buf, "%f",   *(f32*)a);
        else if (type == typeid(cstr)) len = sprintf(buf, "%s",  *(cstr*)a);
        else if (type == typeid(symbol)) len = sprintf(buf, "%s",  *(cstr*)a);
        else if (type == typeid(hook)) len = sprintf(buf, "%p",  *(hook*)a);
        else {
            fault("implement primitive cast to str: %s", type->ident);
        }
        append(res, buf); // should allow for a -1 or len
    } else {
        string s = cast(string, a);
        if (s) {
            append(res, "\"");
            /// encode the characters
            concat(res, escape(s));
            append(res, "\"");
        } else
            append(res, "null");
    }
}

bool Au_member_set(Au a, Au_t m, Au value) {
    if (!(m->member_type == AU_MEMBER_VAR))
        return false;

    Au_t type         = isa(a);
    bool  is_primitive = (m->type->traits & AU_TRAIT_PRIMITIVE) != 0;
    bool  is_enum      = (m->type->traits & AU_TRAIT_ENUM)      != 0;
    bool  is_struct    = (m->type->traits & AU_TRAIT_STRUCT)    != 0;
    bool  is_inlay     = (m->type->traits & AU_TRAIT_INLAY)    != 0;
    ARef  member_ptr   = (cstr)a + m->offset;
    Au_t vtype        = isa(value);
    Au     vinfo        = head(value);

    if (is_struct) {
        verify(m->type->typesize == vtype->typesize * vinfo->count,
            "vector size mismatch for %s", m->ident);
        memcpy(member_ptr, value, m->type->typesize);
    } else if (is_enum || is_inlay || is_primitive) {
        Au_t ref = *m->type->meta.origin;
        verify(!is_struct || vtype == m->type ||
            vtype == ref,
            "%s: expected vmember_type (%s) to equal isa(value) (%s)",
            m->ident, ref->ident, vtype->ident);
        verify(!is_struct || vtype == m->type ||
            m->type->typesize == vtype->typesize * vinfo->count,
            "vector size mismatch for %s", m->ident);
        int sz = m->type->typesize < vtype->typesize ? m->type->typesize : vtype->typesize;
        memcpy(member_ptr, value, sz);
    } else if ((Au)*member_ptr != value) {
        drop(*member_ptr);
        *member_ptr = hold(value);
    }
    Au_AF_set_name(a, m->ident);
    return true;
}

// try to use this where possible
Au Au_member_object(Au a, Au_t m) {
    if (!(m->member_type == AU_MEMBER_VAR))
        return null; // we do this so much, that its useful as a filter in for statements

    bool is_primitive = (m->type->traits & AU_TRAIT_PRIMITIVE) != 0 || 
                        (m->type->traits & AU_TRAIT_STRUCT) != 0;
    bool is_inlay     = (m->type->traits & AU_TRAIT_INLAY) != 0;
    Au result;
    ARef   member_ptr = (cstr)a + m->offset;
    if (is_inlay || is_primitive) {
        result = alloc(m->type, 1, null);
        memcpy(result, member_ptr, m->type->typesize);
    } else {
        result = *member_ptr;
    }
    return result;
}

string Au_cast_string(Au a) {
    Au_t type = isa(a);
    
    // convenient feature of new object abi
    if (!type) {
        type = (Au_t)a;
        return string(type->ident);
    }

    Au a_header = header(a);
    bool  once = false; 
    if (instanceof(a, string)) return (string)a;
    string res = new(string, alloc, 1024);
    if (type->traits & AU_TRAIT_PRIMITIVE)
        serialize(type, res, a);
    else {
        //append(res, type->ident);
        append(res, "[");
        for (num i = 0; i < type->members.count; i++) {
            Au_t m = (Au_t)type->members.origin[i];
            // todo: intern members wont be registered
            if (m->member_type == AU_MEMBER_VAR) {
                if (once)
                    append(res, ", ");
                u8*    ptr = (u8*)a + m->offset;
                Au inst = null;
                bool is_primitive = (m->type->traits & AU_TRAIT_PRIMITIVE) != 0;
                if (is_primitive)
                    inst = (Au)ptr;
                else
                    inst = *(Au*)ptr;
                Au inst_h = header(inst);
                append(res, m->ident);
                append(res, ":");
                if (is_primitive)
                    serialize(m->type, res, inst);
                else {
                    string s_value = inst ? Au_cast_string(inst) : null; /// isa may be called on this, but not primitive data
                    if (!s_value)
                        append(res, "null");
                    else {
                        if (instanceof(inst, string)) {
                            s_value = escape(s_value);
                            append(res, "\"");
                            concat(res, s_value);
                            append(res, "\"");
                        } else {
                            concat(res, s_value);
                        }
                    }
                }
                once = true;
            }
        }
        append(res, "]");
    }
    return res;
}

#define set_v() \
    Au_t type = isa(a); \
    if (type == typeid(i8))  *(i8*) a = (i8) v; \
    if (type == typeid(i16)) *(i16*)a = (i16)v; \
    if (type == typeid(i32)) *(i32*)a = (i32)v; \
    if (type == typeid(i64)) *(i64*)a = (i64)v; \
    if (type == typeid(u8))  *(u8*) a = (u8) v; \
    if (type == typeid(u16)) *(u16*)a = (u16)v; \
    if (type == typeid(u32)) *(u32*)a = (u32)v; \
    if (type == typeid(u64)) *(u64*)a = (u64)v; \
    if (type == typeid(f32)) *(f32*)a = (f32)v; \
    if (type == typeid(f64)) *(f64*)a = (f64)v; \
    return a;

Au numeric_with_i8 (Au a, i8   v) { set_v(); }
Au numeric_with_i16(Au a, i16  v) { set_v(); }
Au numeric_with_i32(Au a, i32  v) { set_v(); }
Au numeric_with_i64(Au a, i64  v) {
    Au_t type = isa(a);
    if (type == typeid(i8))  *(i8*) a = (i8) v;
    if (type == typeid(i16)) *(i16*)a = (i16)v;
    if (type == typeid(i32)) *(i32*)a = (i32)v; \
    if (type == typeid(i64)) *(i64*)a = (i64)v; \
    if (type == typeid(u8))  *(u8*) a = (u8) v; \
    if (type == typeid(u16)) *(u16*)a = (u16)v; \
    if (type == typeid(u32)) *(u32*)a = (u32)v; \
    if (type == typeid(u64)) *(u64*)a = (u64)v; \
    if (type == typeid(f32)) *(f32*)a = (f32)v; \
    if (type == typeid(f64)) *(f64*)a = (f64)v; \
    return a;
}
Au numeric_with_u8 (Au a, u8   v) { set_v(); }
Au numeric_with_u16(Au a, u16  v) { set_v(); }
Au numeric_with_u32(Au a, u32  v) { set_v(); }
Au numeric_with_u64(Au a, u64  v) { set_v(); }
Au numeric_with_f32(Au a, f32  v) { set_v(); }
Au numeric_with_f64(Au a, f64  v) { set_v(); }
Au numeric_with_bool(Au a, bool v) { set_v(); }
Au numeric_with_num(Au a, num  v) { set_v(); }

Au Au_method(Au_t type, cstr method_name, array args);

sz Au_len(Au a) {
    if (!a) return 0;
    Au_t t = isa(a);
    if (t == typeid(string)) return ((string)a)->count;
    if (t == typeid(array))  return ((array) a)->count;
    if (t == typeid(map))    return ((map)a)->count;
    if (t == typeid(cstr) || t == typeid(symbol) || t == typeid(cereal))
        return strlen(a);
    Au aa = header(a);
    return aa->count;
}

i32 Au_compare(Au a, Au b) {
    Au_t au = isa(a);
    Au_t btype = isa(b);
    if (au != btype) return ((ssize_t)au - (ssize_t)btype) < 0 ? -1 : 1;
    return memcmp(a, b, au->typesize);
}

num parse_formatter(cstr start, cstr res, num sz) {
    cstr scan = start;
    num index = 0;
    if (*scan == '%') {
        if (index < sz - 1)
            res[index++] = *scan++;
        while (*scan) {
            if (strchr("diouxXeEfFgGaAcspn%", *scan)) {
                if (index < sz - 1) res[index++] = *scan++;
                break;  // end of the format specifier
            } else if (strchr("0123456789.-+lhjztL*", *scan)) {
                if (index < sz - 1) res[index++] = *scan++;
            } else
                break;
        }
    }
    res[index] = 0;
    return (num)(scan - start);
}

static int term_width() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col ? w.ws_col : 80;
}

Au formatter(Au_t type, handle ff, Au opt, symbol template, ...) {
    va_list args;
    FILE* f = (FILE*)ff;
    va_start(args, template);
    string  res  = new(string, alloc, 1024);
    cstr    scan = (cstr)template;
    bool write_ln = (i64)opt == true;
    bool is_input = (f == stdin);
    string  field = (!is_input && !write_ln && opt) ? instanceof(opt, string) : null;
    
    while (*scan) {
        /// format %o as Au's string cast
        char cmd[2] = { *scan, *(scan + 1) };
        int column_size = 0;
        int skip = 0;
        int f = cmd[1];
        /// column size formatting
        if (cmd[0] == '%' && (cmd[1] == '-' || isdigit(cmd[1]))) {
            /// register to fill this space
            for (int n = 1; n; n++) {
                if (!isdigit(cmd[1 + n])) {
                    int column_digits = 1 + n;
                    verify(column_digits < 32, "column size out of range");
                    char val[32];
                    memcpy(val, scan + 1, column_digits);
                    val[column_digits] = 0;
                    column_size = atoi(val);
                    skip = 1 + column_digits + 1;
                    cmd[1] = scan[skip - 1];
                    break;
                }
            }
        }
        if (cmd[0] == '%' && cmd[1] == 'o') {
            Au arg = va_arg(args, Au);
            string   a;
            Au_t isa_arg = isa(arg);
            if (isa_arg == null || isa_arg == arg) {
                if (!arg) {
                    a = string("null");
                } else {
                    Au_t au = arg;
                    a = string(au->ident);
                }
            } else
                a = arg ? cast(string, arg) : string((symbol)"null");
            num    len = a->count;
            reserve(res, len);
            if (column_size < 0) {
                for (int i = 0; i < -column_size - len; i++)
                    ((cstr)res->chars)[res->count++] = ' ';
            }
            memcpy((cstr)&res->chars[res->count], a->chars, len);
            res->count += len;
            if (column_size) {
                for (int i = 0; i < column_size - len; i++)
                    ((cstr)res->chars)[res->count++] = ' ';
            }
            scan     += skip ? skip : 2; // Skip over %o
        } else {
            /// format with vsnprintf
            const char* next_percent = strchr(scan, '%');
            num segment_len = next_percent ? (num)(next_percent - scan) : (num)strlen(scan);
            reserve(res, segment_len);
            memcpy((cstr)&res->chars[res->count], scan, segment_len);
            res->count += segment_len;
            scan     += segment_len;
            if (*scan == '%') {
                if (*(scan + 1) == 'o')
                    continue;
                char formatter[128];
                int symbol_len = parse_formatter(scan, formatter, 128);
                for (;;) {
                    num f_len = 0;
                    num avail = res->alloc - res->count;
                    cstr  end = (cstr)&res->chars[res->count];
                    if (strchr("fFgG", formatter[symbol_len - 1]))
                        f_len = snprintf(end, avail, formatter, va_arg(args, double));
                    else if (strchr("diouxX", formatter[symbol_len - 1]))
                        f_len = snprintf(end, avail, formatter, va_arg(args, int));
                    else if (strchr("c", formatter[symbol_len - 1]))
                        f_len = snprintf(end, avail, formatter, va_arg(args, int));
                    else
                        f_len = snprintf(
                            end, avail, formatter, va_arg(args, none*));
                    if (f_len > avail) {
                        reserve(res, res->alloc << 1);
                        continue;
                    }
                    res->count += f_len;
                    break;
                }
                scan += symbol_len;
            }
        }
    }
    va_end(args);
    bool symbolic_logging = false;
    
    // handle generic logging with type and function name labels, ability to filter based on log_funcs global map
    // map is setup with *:true on debug builds, unless we explicitly listen
    if (f && field) {
        char info[256];
        symbolic_logging = true;
        Au fvalue = get(log_funcs, field); // make get be harmless to map; null is absolutely fine identity wise to understand that
        int    l      = 0;
        string tname  = null;
        string fname  = field;
        static string  asterick = null;
        if (!asterick) asterick = hold(string("*"));
        bool   listen = fvalue ? cast(bool, fvalue) : false;
        if ((l = index_of(fname, "_")) > 1) {
            tname = mid(fname, 0, l);
            fname = mid(fname, l + 1, len(fname) - (l + 1));
            if (!listen && (contains(log_funcs, tname) || contains(log_funcs, fname)))
                listen = true;
        }
        if (!listen && !contains(log_funcs, asterick)) return null;
        // write type / function
        if (tname)
            sprintf(info, "\x1b[34m%s::%s:\x1b[21G \x1b[0m", tname->chars, fname->chars);
        else
            sprintf(info, "\x1b[34m%s:\x1b[21G \x1b[0m", fname->chars);

        // based on the number of columns left, we need to isue multiple prints starting at 30
        fwrite(info, strlen(info), 1, f);
    }

    if (f == stderr)
        fwrite("\033[1;33m", 7, 1, f);

    if (f) {
        // based on the number of columns left, we need to isue multiple prints starting at 30
        // write message
        int n = len(res);
        int tw = max(32, term_width() - 22);
        float lc = (float)n / tw;
        if (lc <= 1 || !field) {
            string_writef(res, f, false);
            if (symbolic_logging || write_ln) {
                fwrite("\n", 1, 1, f);
                fflush(f);
            }
        } else {
            for (int i = 0, to = floorf(lc); i <= to; i++) {
                string l  = mid(res, i * tw, tw);
                string ff = f(string, "\x1b[22G%o", l);
                string_writef(ff, f, false);
                if (symbolic_logging || write_ln) {
                    if (i == to)
                        fwrite("\n", 1, 1, f);
                    else
                        fwrite("\n\x1b[22G", 1, 7, f);
                }
            }
            fflush(f);
        }

    }
    
    if (f == stderr) {
        fwrite("\033[0m", 4, 1, f); // ANSI reset
        fflush(f);
    }

    if (type && (type->traits & AU_TRAIT_ENUM)) {
        // convert res to instance of this enum
        i32 v = evalue(type, (cstr)res->chars);
        return primitive(typeid(i32), &v);
    }
    return type ? (Au)
        ((Au_f*)type)->ft.with_cereal(alloc(type, 1, null), (cereal) { .value = (cstr)res->chars }) :
        (Au)res;
}

u64 fnv1a_hash(const none* data, size_t length, u64 hash) {
    const u8* bytes = (const u8*)data;
    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];  // xor bottom with current byte
        hash *= FNV_PRIME; // multiply by FNV prime
    }
    return hash;
}

list   list_copy(list a) {
    list  b = new(list);
    for (item i = a->first; i; i = i->next)
        push(b, i->value);
    return b;
}

u64 item_hash(item f) {
    return hash(f->key ? f->key : f->value);
}

none item_init(item a) {
}

num clamp(num i, num mn, num mx) {
    if (i < mn) return mn;
    if (i > mx) return mx;
    return i;
}

real clampf(real i, real mn, real mx) {
    if (i < mn) return mn;
    if (i > mx) return mx;
    return i;
}

none vector_init(vector a);

vector vector_with_i32(vector a, i32 count) {
    Au_vrealloc(a, count);
    return a;
}

sz vector_len(vector a) {
    return header(a)->count;
}



none map_init(map m) {
    if (m->hsize <= 0) m->hsize = 8;
    if (m->hsize) m->hlist = (item*)calloc(m->hsize, sizeof(item));
}

map map_copy(map m) {
    map a = map(hsize, 16, assorted, m->assorted, unmanaged, m->unmanaged);
    pairs(m, i) {
        set(a, i->key, i->value);
    }
    return a;
}

none map_dealloc(map m) {
    Au   info  = head(m);
    item* hlist = m->hlist;
    if (hlist) {
        for (int b = 0; b < m->hsize; b++)
            while (hlist[b]) {
                item i = hlist[b];
                item n = i->next;
                drop(i->key);
                drop(i->value);
                drop(i);
                //drop(i->ref);
                hlist[b] = n;
            }
        drop((Au)hlist);
        m->hlist = null;
    }
}

item map_lookup(map m, Au k) {
    if (!m->hlist) {
        u64 h = hash(k);
        for (item i = m->first; i; i = i->next)
            if (i->h == h && compare(i->key, k) == 0)
                return i;
    }
    item* hlist = m->hlist;
    u64 h = hash(k);
    i64 b = h % m->hsize;
    for (item i = hlist[b]; i; i = i->next) {
        if (i->h == h && compare(i->key, k) == 0)
            return i;
    }
    return null;
}

bool map_contains(map m, Au k) {
    return map_lookup(m, k) != null;
}

Au map_get(map m, Au k) {
    item i = map_lookup(m, k);
    return i ? i->value : null;
}

item map_fetch(map m, Au k) {
    item i = map_lookup(m, k);
    if (!i) {
        u64 h = hash(k);
        i64 b = h % m->hsize;

        ((item*)m->hlist)[b] = i = item(next, ((item*)m->hlist)[b], key, hold(k), h, h);
        m->count++;
    }
    return i;
}

Au map_value_by_index(map m, num idx) {
    int i = 0;
    pairs(m, ii) {
        if (i++ == idx)
            return ii->value;
    }
    return null;
}

none map_set(map m, Au k, Au v) {
    if (!m->hlist) m->hlist = (item*)calloc(m->hsize, sizeof(item));
    item i = map_fetch(m, k);
    Au_t vtype = isa(v);
    Au info = head(m);

    bool allowed = m->unmanaged || !m->last_type || m->last_type == vtype || m->assorted;
    verify(allowed,
        "unassorted map set to differing type: %s, previous: %s (%s:%i)",
        vtype->ident, m->last_type->ident, info->source, info->line);
    m->last_type = vtype;
    
    if (i->value) {
        if (i->value != v) {
            drop(i->value);
            i->value = m->unmanaged ? v : hold(v);
        } else {
            return;
        }
    } else {
        i->value = m->unmanaged ? v : hold(v);
    }

    bool in_fifo = i->ref != null;
    if (!in_fifo) {
        item ref = list_push(m, m->unmanaged ? v : hold(v));
        ref->key = hold(k);
        ref->ref = i; // these reference each other
        i->ref = ref;
    }
}

none map_rm_item(map m, item i) {
    drop(i->key);
    drop(i->value);
    list_remove_item(m, i->ref);
    drop(i);
}

none map_rm(map m, Au k) {
    u64  h    = hash(k);
    i64  b    = h % m->hsize;
    item prev = null;
    if (m->hlist)
        for (item i = ((item*)m->hlist)[b]; i; i = i->next) {
            if (i->h == h && compare(i->key, k) == 0) {
                if (prev) {
                    prev->next = i->next;
                } else {
                    ((item*)m->hlist)[b] = i->next;
                }
                map_rm_item(m, i);
                return;
            }
            prev = i;
        }
}

none map_clear(map m) {
    for (int b = 0; b < m->hsize; b++) {
        item prev = null;
        item cur  = ((item*)m->hlist)[b];
        item next = null;
        while (cur) {
            next = cur->next;
            map_rm_item(m, cur);
            cur = next;
        }
    }
}

sz map_len(map a) {
    return a->count;
}

Au map_index_sz(map a, sz index) {
    assert(index >= 0 && index < a->count, "index out of range");
    item i = (item)list_get(a, (Au)_sz(index));
    return i ? i->value : null;
}

Au map_index_Au(map a, Au key) {
    return map_get(a, key);
}

map map_with_i32(map a, i32 size) {
    a->hsize = size;
    return a;
}

string map_cast_string(map a) {
    string res  = string(alloc, 1024);
    bool   once = false;
    for (item i = a->first; i; i = i->next) {
        string key   = cast(string, i->key);
        string value = cast(string, i->value);
        if (once) append(res, " ");
        append(res, key->chars);
        append(res, ":");
        append(res, value->chars);
        once = true;
    }
    return res;
}

none map_concat(map a, map b) {
    pairs(b, e) set(a, e->key, e->value);
}

bool map_cast_bool(map a) {
    return a->count > 0;
}

map map_of(symbol first_key, ...) {
    map a = map(hsize, 16, assorted, true);
    va_list args;
    va_start(args, first_key);
    symbol key = first_key;
    if (!key) return a;
    for (;;) {
        Au arg = va_arg(args, Au);
        set(a, (Au)string(key), arg);
        key = va_arg(args, cstr);
        if (key == null)
            break;
    }
    return a;
}

srcfile srcfile_with_Au(srcfile a, Au obj) {
    a->obj = obj;
    return a;
}

string srcfile_cast_string(srcfile a) {
    Au i = head(a->obj);
    return f(string, "%s:%i", i->source, i->line);
}

define_class(srcfile, Au);



bool string_is_numeric(string a) {
    return a->chars[0] == '-' ||
          (a->chars[0] >= '0' && a->chars[0] <= '9');
}

i32 string_first(string a) {
    return a->count ? a->chars[0] : 0;
}

i32 string_last(string a) {
    return a->count ? a->chars[a->count - 1] : 0;
}

f64 string_real_value(string a) {
    double v = 0.0;
    sscanf(a->chars, "%lf", &v);
    return v;
}


string string_ucase(string a) {
    string res = string(a->chars);
    for (cstr s = (cstr)res->chars; *s; ++s) *s = toupper((unsigned char)*s);
    return res;
}

string string_lcase(string a) {
    string res = string(a->chars);
    for (cstr s = (cstr)res->chars; *s; ++s) *s = tolower((unsigned char)*s);
    return res;
}

string string_escape(string input) {
    struct {
        char   ascii;
        symbol escape;
    } escape_map[] = {
        {'\n', "\\n"},
        {'\t', "\\t"},
        {'\"', "\\\""},
        {'\\', "\\\\"},
        {'\r', "\\r"}
    };
    int escape_count = sizeof(escape_map) / sizeof(escape_map[0]);
    int input_len    = len(input);
    int extra_space  = 0;
    for (int i = 0; i < input_len; i++)
        for (int j = 0; j < escape_count; j++)
            if (input->chars[i] == escape_map[j].ascii) {
                extra_space += strlen(escape_map[j].escape) - 1;
                break;
            }

    // allocate memory for escaped string with applied space
    cstr escaped = calloc(input_len + extra_space + 1, 1);
    if (!escaped) return NULL;

    // fill escaped string
    int pos = 0;
    for (int i = 0; i < input_len; i++) {
        bool found = false;
        for (int j = 0; j < escape_count; j++) {
            if (input->chars[i] == escape_map[j].ascii) {
                const char *escape_seq = escape_map[j].escape;
                int len = strlen(escape_seq);
                strncpy(escaped + pos, escape_seq, len);
                pos += len;
                found = true;
                break;
            }
        }
        if (!found) escaped[pos++] = input->chars[i];
    }
    escaped[pos] = '\0';
    string res = string((symbol)escaped); /// with cstr constructor, it does not 'copy' but takes over life cycle
    free(escaped);
    return res;
}

none  string_dealloc(string a) {
    printf("string_dealloc: %s", a->chars);
    free((cstr)a->chars);
}
num   string_compare(string a, string b) { return strcmp(a->chars, b->chars); }
num   string_cmp    (string a, symbol b) { return strcmp(a->chars, b); }
bool  string_eq     (string a, symbol b) { return strcmp(a->chars, b) == 0; }

string string_copy(string a) {
    return string((symbol)a->chars);
}

bool inherits(Au_t src, Au_t check) {
    if (!src) return false;
    while (src != typeid(Au)) {
        if (src == check) return true;
        src = src->context;
    }
    if ((src   == typeid(Au) || src   == typeid(Au)) &&
        (check == typeid(Au) || check == typeid(Au))) {
        return true;
    }
    return src == check; // true for Au against Au
}

static inline char just_a_dash(char a) {
    return a == '-' ? '_' : a;
}

array string_split_parts(string a) {
    array res = array(alloc, 32);
    cstr s = (cstr)a->chars;
    cstr prev = null;

    while (*s) {
        if (*s == '{' || s[1] == 0) {
            if (s[1] == '{') {
                // escaped {{
                if (!prev) prev = s;
                s += 2;
                continue;
            }

            // flush any literal before the expression
            if (prev) {
                string lit = const_string(chars, prev, ref_length, (sz)(s - prev));
                ipart p = ipart(is_expr, false, content, lit);
                push(res, (Au)p);
                prev = null;
            }

            if (s[1] == 0)
                break;

            // parse expression content
            s++; // skip '{'
            cstr start = s;
            while (*s && *s != '}')
                s++;

            verify(*s == '}', "unterminated interpolation", 1);

            string expr = string(chars, start, ref_length, (sz)(s - start));
            ipart p = ipart(is_expr, true, content, expr);
            push(res, (Au)p);

            s++; // skip '}'
        } else if (*s == '}') {
            // escaped }}
            verify(s[1] == '}', "single unmatched }", 2);
            if (!prev) prev = s;
            s += 2;
        } else {
            if (!prev) prev = s;
            s++;
        }
    }

    return res;
}

string string_interpolate(string a, Au ff) {
    cstr   s    = (cstr)a->chars;
    cstr   prev = null;
    string res  = string(alloc, 256);
    map    f    = instanceof(ff, map);

    verify( f || ff,       "no object given for interpolation");
    verify(!f || f->hsize, "no hashmap on map, set hsize > 0");

    while (*s) {
        if (*s == '{') {
            if (s[1] == '{') {
                s += 2;
                continue;
            } else {
                if (prev) {
                    append_count(res, prev, (sz)(s - prev));
                    prev = null;
                }
                cstr kstart = ++s;
                string v = null;
                if (f) {
                    u64  hash   = OFFSET_BASIS;
                    while (*s != '}') {
                        verify (*s, "unexpected end of string", 1);
                        hash ^= just_a_dash((u8)*(s++));
                        hash *= FNV_PRIME;
                    }
                    item b = ((item*)f->hlist)[hash % f->hsize]; // todo: change schema of hashmap to mirror map
                    item i = null;
                    for (i = b; i; i = i->next)
                        if (i->h == hash)
                            break;
                    verify(i, "key not found in map");
                    v = cast(string, i->value);
                } else {
                    cstr   s_ind = strchr(kstart, '}');
                    verify(s_ind, "unexpected end of string");
                    int    ind   = (size_t)s_ind - (size_t)kstart;
                    string k     = string(chars, kstart, ref_length, ind);
                    Au      vv    = Au_get_property(ff, k->chars);
                    verify(vv, "property %o does not exist on object", k);
                    v            = cast(string, vv);
                    s           += ind;
                }
                if (!v)
                    append(res, "null");
                else
                    concat(res, v);
                
                s++; // skip the }
            }
        } else if (*s == '}') {
            verify(s[1] == '}', "missing extra '}'", 2);
            s += 2;
        } else if (!prev) {
            prev = s++;
        } else
            s++;
    }
    if (prev) {
        append_count(res, prev, (sz)(s - prev));
        prev = null;
    }
    return res;
}

i32   string_index_num(string a, num index) {
    if (index < 0)
        index += a->count;
    if (index >= a->count)
        return 0;
    return (i32)a->chars[index];
}

array string_split(string a, symbol sp) {
    cstr next = (cstr)a->chars;
    sz   slen = strlen(sp);
    array result = array(32);
    while (next) {
        cstr   n = strstr(&next[1], sp);
        string v = string(chars, next, ref_length, n ? (sz)(n - next) : 0);
        next = n ? n + slen : null;
        push(result, (Au)v);
        if (!next || !next[0])
            break;
    }
    return result;
}

none string_alloc_sz(string a, sz alloc) {
    char* chars = calloc(1 + alloc, sizeof(char));
    memcpy(chars, a->chars, sizeof(char) * a->count);
    chars[a->count] = 0;
    //free(a->chars);
    a->chars = chars;
    a->alloc = alloc;
}

string string_mid(string a, num start, num len) {
    if (start < 0)
        start = a->count + start;
    if (start < 0)
        start = 0;
    if (start + len > a->count)
        len = a->count - start;
    return new(string, chars, &a->chars[start], ref_length, len);
}

none  string_reserve(string a, num extra) {
    if (a->alloc - a->count >= extra)
        return;
    string_alloc_sz(a, a->alloc + extra);
}

none  string_append(string a, symbol b) {
    sz blen = strlen(b);
    if (blen + a->count >= a->alloc)
        string_alloc_sz(a, (a->alloc << 1) + blen);
    memcpy((cstr)&a->chars[a->count], b, blen);
    a->count += blen;
    a->h = 0; /// mutable operations must clear the hash value
    ((cstr)a->chars)[a->count] = 0;
}

none  string_append_count(string a, symbol b, i32 blen) {
    if (blen + a->count >= a->alloc)
        string_alloc_sz(a, (a->alloc << 1) + blen);
    memcpy((cstr)&a->chars[a->count], b, blen);
    a->count += blen;
    a->h = 0; /// mutable operations must clear the hash value
    ((cstr)a->chars)[a->count] = 0;
}

string string_trim(string a) {
    cstr s = cstring(a);
    int count = len(a);
    while (*s == ' ') {
        s++;
        count--;
    }
    while (count && s[count - 1] == ' ')
        count--;
    
    return string(chars, s, ref_length, count);
}

string string_ltrim(string a) {
    cstr s = cstring(a);
    int count = len(a);
    while (*s == ' ') {
        s++;
        count--;
    }
    return string(chars, s, ref_length, count);
}

string string_rtrim(string a) {
    cstr s = cstring(a);
    int count = len(a);
    while (s[count - 1] == ' ')
        count--;
    return string(chars, s, ref_length, count);
}

none string_operator__assign_add(string a, string b) {
    concat(a, b);
}

string string_operator__add(string a, string b) {
    concat(a, b);
    return a;
}

none  string_push(string a, u32 b) {
    sz blen = 1;
    if (blen + a->count >= a->alloc)
        string_alloc_sz(a, (a->alloc << 1) + blen);
    memcpy((cstr)&a->chars[a->count], &b, 1);
    a->count += blen;
    a->h = 0; /// mutable operations must clear the hash value
    ((cstr)a->chars)[a->count] = 0;
}

none  string_concat(string a, string b) {
    string_append(a, b->chars);
}

sz string_len(string a) { return a->count; }

num   string_index_of(string a, symbol cs) {
    cstr f = strstr(a->chars, cs);
    return f ? (num)(f - a->chars) : (num)-1;
}

num   string_rindex_of(string a, symbol cs) {
    cstr   haystack = a->chars;
    cstr   last     = NULL;
    size_t len      = strlen(haystack);
    size_t cs_len   = strlen(cs);
    for (size_t i = 0; i + cs_len <= len; i++) {
        if (memcmp(haystack + i, cs, cs_len) == 0)
            last = haystack + i;
    }
    return last ? (num)(last - haystack) : (num)-1;
}

bool string_cast_bool(string a) {
    return a->count > 0;
}

sz string_cast_sz(string a) {
    return a->count;
}

cstr string_cast_cstr(string a) {
    return (cstr)a->chars;
}

none string_writef(string a, handle f, bool new_line) {
    FILE* output = f ? f : stdout;
    fwrite(a->chars, a->count, 1, output);
    if (new_line) fwrite("\n", 1, 1, output);
    fflush(output);
}

path string_cast_path(string a) {
    return new(path, chars, a->chars);
}



u64 string_hash(string a) {
    if (a->h) return a->h;
    a->h = fnv1a_hash(a->chars, a->count, OFFSET_BASIS);
    return a->h;
}

none msg_init(msg a) {
    a->role = strdup(a->role);
    a->content = strdup(a->content);
}

none msg_dealloc(msg a) {
    free(a->role);
    free(a->content);
}

none string_init(string a) {
    cstr value = (cstr)a->chars;
    if (a->alloc)
        a->chars = (char*)calloc(1, 1 + a->alloc);
    if (value) {
        sz len = a->ref_length ? a->ref_length : strlen(value);
        if (!a->alloc)
            a->alloc = len;
        if (a->chars == value)
            a->chars = (char*)calloc(1, len + 1);
        memcpy((cstr)a->chars, value, len);
        ((cstr)a->chars)[len] = 0;
        a->count = len;
    }
}

string string_with_i32(string a, i32 value) {
    // Check if the value is within the BMP (U+0000 - U+FFFF)
    if (value <= 0xFFFF) {
        a->count = 1;
        a->chars = calloc(8, 1);
        ((cstr)a->chars)[0] = (char)value;
    } else {
        // Encode Unicode code point as UTF-8
        a->count = 0;
        char buf[4];
        int len = 0;
        if (value <= 0x7F) {
            buf[len++] = (char)value;
        } else if (value <= 0x7FF) {
            buf[len++] = 0xC0 | ((value >> 6) & 0x1F);
            buf[len++] = 0x80 | (value & 0x3F);
        } else if (value <= 0xFFFF) {
            buf[len++] = 0xE0 | ((value >> 12) & 0x0F);
            buf[len++] = 0x80 | ((value >> 6) & 0x3F);
            buf[len++] = 0x80 | (value & 0x3F);
        } else if (value <= 0x10FFFF) {
            buf[len++] = 0xF0 | ((value >> 18) & 0x07);
            buf[len++] = 0x80 | ((value >> 12) & 0x3F);
            buf[len++] = 0x80 | ((value >> 6) & 0x3F);
            buf[len++] = 0x80 | (value & 0x3F);
        } else {
            // Invalid Unicode code point
            a->count = 0;
            a->chars = NULL;
            return a;
        }
        a->count = len;
        a->chars = calloc(len + 1, 1);
        memcpy((cstr)a->chars, buf, len);
    }
    return a;
}

string string_with_cstr(string a, cstr value) {
    a->count   = value ? strlen(value) : 0;
    a->chars = calloc(a->count + 1, 1);
    memcpy((cstr)a->chars, value, a->count);
    return a;
}


string string_with_symbol(string a, symbol value) {
    return string_with_cstr(a, (cstr)value);
}


bool string_starts_with(string a, symbol value) {
    sz ln = strlen(value);
    if (!ln || ln > a->count) return false;
    return strncmp(&a->chars[0], value, ln) == 0;
}

bool string_ends_with(string a, symbol value) {
    sz ln = strlen(value);
    if (!ln || ln > a->count) return false;
    return strcmp(&a->chars[a->count - ln], value) == 0;
}

item list_push(list a, Au e);


none list_quicksort(list a, i32(*sfn)(Au, Au)) {
    item f = a->first;
    int  n = a->count;
    for (int i = 0; i < n - 1; i++) {
        int jc = 0;
        for (item j = f; jc < n - i - 1; j = j->next, jc++) {
            item j1 = j->next;
            if (sfn(j->value, j1->value) > 0) {
                Au t = j->value;
                j ->value = j1->value;
                j1->value = t;
            }
        }
    }
}

none list_sort(list a, ARef fn) {
    list_quicksort(a, (i32(*)(Au, Au))fn);
}

Au list_get(list a, Au at_index);

Au Au_copy(Au a) {
    Au f = header(a);
    assert(f->count > 0, "invalid count");
    Au_t type = isa(a);
    Au b = alloc(type, f->count, null);
    memcpy(b, a, f->type->typesize * f->count);
    Au_hold_members(b);
    return b;
}

Au Au_hold(Au a) {
    if (a) {
        Au f = header(a);
        if (f->refs == 0) return a; // refs of 0 is unmanaged memory (user managed)
        f->refs++;

        if (f->type) {
            au_core af = f->type->af;
            if (f->af_index > 0) {
                af->af4[f->af_index] = null;
                f->af_index = 0;
            }
        }
    }
    return a;
}

none Au_free(Au a) {
    Au       aa = header(a);
    Au_f*  type = (Au_f*)aa->type;
    none* prev = null;
    Au_f*   cur = type;
    while (cur) {
        if (prev != cur->ft.dealloc) {
            cur->ft.dealloc(a);
            prev = cur->ft.dealloc;
        }
        if (cur == &Au_i.type)
            break;
        cur = (Au_f*)cur->context;
    }
    
    au_core af = type->af;    
    if (true || !af || af->re_alloc == af->re_count) {
        memset(aa, 0xff, type->typesize);
        free(aa);
        if (--all_type_alloc < 0) {
            printf("all_type_alloc < 0\n");
        }
        if (--type->global_count < 0) {
            printf("global_count < 0 for type %s\n", type->ident);
        }
    } else if (af) {
        aa->af_index = 0;
        af->re[af->re_count++] = (object)aa;
    }
}

none Au_recycle() {
    i64   types_len;
    Au_t* atypes = (Au_t*)Au_types(&types_len);

    /// iterate through types
    for (num i = 0; i < types_len; i++) {
        Au_t type = atypes[i];
        au_core af = type->af;
        if (af && af->af_count) {
            for (int i = 0; i <= af->af_count; i++) {
                Au a = (Au)af->af4[i];
                if (a && a->refs > 0 && --a->refs == 0) {
                    Au_free(&a[1]);
                }
            }
            af->af_count = 0;
        }
    }
}

none Au_drop(Au a) {
    if (!a) return;
    Au info = header(a);

    if (info->refs > 0 && --info->refs == 0) {
        if (info->af_index > 0) {
            info->type->af->af4[info->af_index] = null;
            info->af_index = 0;
        }
        Au_free(a);
    }
}

/// binding works with delegate callback registration efficiently to 
/// avoid namespace collisions, and allow enumeration interfaces without override and base Au boilerplate
callback Au_binding(Au a, Au target, bool required, Au_t rtype, Au_t arg_type, symbol id, symbol name) {
    Au_t self_type   = isa(a);
    Au_t target_type = isa(target);
    bool inherits     = instance_of(target, self_type) != null;
    string method     = f(string, "%s%s%s", id ? id : 
        (!inherits ? self_type->ident : ""), (id || !inherits) ? "_" : "", name);
    Au_t m  = find_member(target_type, AU_MEMBER_FUNC, method->chars, true);
    verify(!required || m, "bind: required method not found: %o", method);
    if (!m) return null;
    callback f       = (callback)m->value;
    verify(f, "expected method address");
    verify(m->args.count  == 2, "%s: expected method address with instance, and arg*", name);
    verify(!arg_type || m->args.origin[1] == arg_type, "%s: expected arg type: %s", name, arg_type->ident);
    verify(!rtype    || m->type == rtype, "%s: expected return type: %s", name, rtype->ident);
    return f;
}

Au Au_vdata(Au a) {
    Au obj = header(a);
    return obj->data;
}

i64 Au_vdata_stride(Au a) {
    Au_t t = vdata_type(a);
    return t->typesize - (t->traits & AU_TRAIT_PRIMITIVE ? 0 : sizeof(none*));
}

Au_t Au_vdata_type(Au a) {
    Au f = header(a);
    return f->scalar ? f->scalar : f->type;
}

Au Au_instance_of(Au inst, Au_t type) {
    if (!inst) return null;

    verify(inst, "instanceof given a null value");
    Au_t t  = type;
    Au_t it = isa(inst); 
    Au_t it_copy = it;
    while (it) {
        if (it == t)
            return inst;
        else if (it == typeid(Au))
            break;
        it = it->context; 
    }
    return null;
}

/// list -------------------------
none list_push_item(list a, item i) {
    if (a->last) {
        a->last->next = i;
        i->prev = a->last;
        i->next = null;
        a->last = i;
    } else {
        a->first = a->last = i;
        i->next = null;
        i->prev = null;
    }
    a->count++;
}

item list_push(list a, Au e) {
    item n = item();
    n->value = a->unmanaged ? e : hold(e);
    if (a->last) {
        a->last->next = n;
        n->prev       = a->last;
    } else {
        a->first      = n;
    }
    a->last = n;
    a->count++;
    return n;
}


none list_dealloc(list a) {
    while (pop(a)) { }
}

item list_insert_after(list a, Au e, i32 after) {
    num index = 0;
    item found = null;
    if (after >= 0)
        for (item ai = a->first; ai; ai = ai->next) {
            if (after <= index) {
                found = ai;
                break;
            }
            index++;
        }
    item n = hold(item(value, e));
    if (!found) {
        n->next = a->first;
        if (a->first)
            a->first->prev = n;
        a->first = n;
        if (!a->last)
             a->last = n;
    } else {
        n->next = null;
        n->prev = found;
        found->next = n;
        if (found->prev)
            found->prev->next = n;
        if (a->last == found)
            a->last = n;
    }
    a->count++;
    return n;
}

num list_index_of(list a, Au value) {
    num index = 0;
    for (item ai = a->first; ai; ai = ai->next) {
        if (ai->value == value)
            return index;
        index++;
    }
    return -1;
}

item list_item_of(list a, Au value) {
    num index = 0;
    for (item ai = a->first; ai; ai = ai->next) {
        if (ai->value == value) {
            ai->key = _i64(index);
            return ai;
        }
        index++;
    }
    return null;
}

none list_remove(list a, num index) {
    num i = 0;
    item res = null;
    for (item ai = a->first; ai; ai = ai->next) {
        if (i++ == index) {
            res = ai;
            if (ai == a->first) a->first = ai->next;
            if (ai == a->last)  a->last  = ai->prev;
            if (ai->prev)       ai->prev->next = ai->next;
            if (ai->next)       ai->next->prev = ai->prev;
            a->count--;
            res->prev = null;
            res->next = null;
        }
    }
}

none list_remove_item(list a, item ai) {
    num i = 0;
    if (ai) {
        if (ai == a->first) a->first = ai->next;
        if (ai == a->last)  a->last  = ai->prev;
        if (ai->prev)       ai->prev->next = ai->next;
        if (ai->next)       ai->next->prev = ai->prev;
        a->count--;
    }
}

num list_compare(list a, list b) {
    num diff  = a->count - b->count;
    if (diff != 0)
        return diff;
    Au_t ai_t = a->first ? isa(a->first->value) : null;
    if (ai_t) {
        Au_t m = find_member(ai_t, AU_MEMBER_FUNC, "compare", true);
        for (item ai = a->first, bi = b->first; ai; ai = ai->next, bi = bi->next) {
            num   v  = ((num(*)(Au,Au))(m->value))(ai, bi);
            if (v != 0) return v;
        }
    }
    return 0;
}

Au list_pop(list a) {
    item l = a->last;
    if (!l)
        return null;
    Au info = head(a);
    a->last = a->last->prev;
    if (!a->last)
        a->first = null;
    l->prev = null;
    if (!a->unmanaged)
        drop(l->value);
    drop(l->key);
    a->count--;
    drop(l);
    return l;
}

Au list_get(list a, Au at_index) {
    sz index = 0;
    Au_t itype = isa(at_index);
    sz at = 0;
    if (itype == typeid(sz)) {
        at = *(sz*)at_index;
    } else {
        assert(itype == typeid(i32), "invalid indexing type");
        at = (sz)*(i32*)at_index;
    }
    for (item i = a->first; i; i = i->next) {
        if (at == index)
            return i->value;
        index++;
    }
    assert(false, "could not fetch item at index %i", at);
    return null;
}

num list_count(list a) {
    return a->count;
}


bool Au_is_meta(Au a) {
    Au_t t = isa(a);
    return t->meta.count > 0;
}

bool Au_is_meta_compatible(Au a, Au b) {
    Au_t t = isa(a);
    if (is_meta(a)) {
        Au_t bt = isa(b);
        num found = 0;
        for (num i = 0; i < t->meta.count; i++) {
            Au_t mt = meta_index(a, i);
            if (inherits(bt, mt))
                found++;
        }
        return found > 0;
    }
    return false;
}

Au Au_vrealloc(Au a, sz alloc) {
    Au   i = header(a);
    if (alloc > i->alloc) {
        Au_t type = vdata_type(a);
        sz  size  = vdata_stride(a);
        u8* data  = calloc(alloc, size);
        u8* prev  = i->data;
        memcpy(data, prev, i->count * size);
        i->data  = data;
        i->alloc = alloc;
        if ((Au)prev != a) free(prev);
    }
    return i->data;
}

none vector_init(vector a) {
    Au f = header(a);
    f->count = 0;
    f->scalar = (f->type && f->type->meta.origin) ? meta_index(a, 0) : a->type ? a->type : typeid(i8);
    f->shape  = hold(a->shape);
    verify(f->scalar, "scalar not set");
    if (f->shape)
        a->alloc = shape_total(f->shape);
    Au_vrealloc(a, a->alloc);
}

vector vector_with_path(vector a, path file_path) {
    Au f = header(a);
    f->scalar = typeid(i8);
    
    verify(exists(file_path), "file %o does not exist", file_path);
    FILE* ff = fopen(cstring(file_path), "rb");
    fseek(ff, 0, SEEK_END);
    sz flen = ftell(ff);
    fseek(ff, 0, SEEK_SET);

    Au_vrealloc(a, flen);
    f->count = flen;
    size_t n = fread(f->data, 1, flen, ff);
    verify(n == flen, "could not read file: %o", f);
    fclose(ff);
    return a;
}

ARef vector_get(vector a, num index) {
    num location = index * a->type->typesize;
    i8* arb = vdata(a);
    return &arb[location];
}

none vector_set(vector a, num index, ARef element) {
    num location = index * a->type->typesize;
    i8* arb = vdata(a);
    memcpy(&arb[location], element, a->type->typesize); 
}

Au vector_resize(vector a, sz size) {
    Au_vrealloc(a, size);
    Au f = header(a);
    f->count = size;
    return f->data;
}

Au vector_reallocate(vector a, sz size) {
    Au_vrealloc(a, size);
    Au f = header(a);
    return f->data;
}

none vector_concat(vector a, ARef any, num count) {
    if (count <= 0) return;
    Au_t type = vdata_type(a);
    Au f = header(a);
    if (f->alloc < f->count + count)
        vrealloc(a, (f->alloc << 1) + 32 + count);
    
    u8* ptr  = (u8*)vdata(a);
    i64 size = vdata_stride(a);
    memcpy(&ptr[f->count * size], any, size * count);
    f->count += count;
    if (f->shape)
        f->shape->data[f->shape->count - 1] = f->count;
}

none vector_push(vector a, Au any) {
    vector_concat(a, (ARef)any, 1);
}

num abso(num i) { 
    return (i < 0) ? -i : i;
}

vector vector_slice(vector a, num from, num to) {
    Au      f   = header(a);
    num count = (1 + abso(from - to)); // + 31 & ~31;
    Au res = alloc(f->type, 1, null);
    Au res_f = header(res);
    u8* src   = f->data;
    u8* dst   = null;
    fault("implement vector_slice allocator");
    i64 stride = vdata_stride(a);
    if (from <  to)
        memcpy(dst, &src[from * stride], count * stride);
    else
        for (int i = from; i > to; i--, dst++)
            memcpy(dst, &src[i * stride], count * stride);
    res_f->data = dst;
    Au_initialize(res);
    return res;
}

sz vector_count(vector a) {
    Au f = header(a);
    return f->count;
}

define_class(vector, collective);


Au subprocedure_invoke(subprocedure a, Au arg) {
    Au(*addr)(Au, Au, Au) = a->addr;
    return addr(a->target, arg, a->ctx);
}

/*
none file_init(file f) {
    verify(!(f->read && f->write), "cannot open for both read and write");
    cstr src = (cstr)(f->src ? f->src->chars : null);
    if (!f->id && (f->read || f->write)) {
        verify (src || f->write, "can only create temporary files for write");

        if (!src) {
            i64    h      = 0;
            bool   exists = false;
            string r      = null;
            path   p      = null;
            do {
                h   = (i64)rand() << 32 | (i64)rand();
                r   = formatter("/tmp/f%p", (none*)h);
                src = (cstr)r->chars;
                p   = new(path, chars, r);
            } while (exists(p));
        }
        f->id = fopen(src, f->read ? "rb" : "wb");
        if (!f->src)
             f->src = new(path, chars, src);
    }
}

bool file_cast_bool(file f) {
    return f->id != null;
}

string file_gets(file f) {
    char buf[2048];
    if (fgets(buf, 2048, f->id) > 0)
        return string(buf);
    return null;
}

bool file_file_write(file f, Au o) {
    Au_t type = isa(o);
    if (type == typeid(string)) {
        u16 nbytes    = ((string)o)->count;
        u16 le_nbytes = htole16(nbytes);
        fwrite(&le_nbytes, 2, 1, f->id);
        f->size += (num)nbytes;
        return fwrite(((string)o)->chars, 1, nbytes, f->id) == nbytes;
    }
    sz size = isa(o)->size;
    f->size += (num)size;
    verify(type->traits & AU_TRAIT_PRIMITIVE, "not a primitive type");
    return fwrite(o, size, 1, f->id) == 1;
}



Au file_file_read(file f, Au_t type) {
    if (type == typeid(string)) {
        char bytes[65536];
        u16  nbytes;
        if (f->text_mode) {
            verify(fgets(bytes, sizeof(bytes), f->id), "could not read text");
            return string(bytes); 
        }
        verify(fread(&nbytes, 2, 1, f->id) == 1, "failed to read byte count");
        nbytes = le16toh(nbytes);
        f->location += nbytes;
        verify(nbytes < 1024, "overflow");
        verify(fread(bytes, 1, nbytes, f->id) == nbytes, "read fail");
        bytes[nbytes] = 0;
        return string(bytes); 
    }
    Au o = alloc(type, 1, null);
    sz size = isa(o)->size;
    f->location += size;
    verify(type->traits & AU_TRAIT_PRIMITIVE, "not a primitive type");
    bool success = fread(o, size, 1, f->id) == 1;
    return success ? o : null;
}

none file_file_close(file f) {
    if (f->id) {
        fclose(f->id);
        f->id = null;
    }
}

none file_dealloc(file f) {
    file_file_close(f);
}
*/

none path_init(path a) {
    cstr arg = (cstr)a->chars;
    num  len = arg ? strlen(arg) : 0;
    a->chars = calloc(len + 1, 1);
    if (arg) {
        memcpy((cstr)a->chars, arg, len + 1);
        a->count = len;
    }
}

string path_mime(path a) {
    string e = ext(a);
    cstr res = null;
    if      (eq(e, "png"))  res = "image/png";
    else if (eq(e, "jpg"))  res = "image/jpeg";
    else if (eq(e, "jpeg")) res = "image/jpeg";
    else if (eq(e, "gif"))  res = "image/gif";
    else if (eq(e, "txt"))  res = "text/plain";
    else if (eq(e, "json")) res = "application/json";
    else if (eq(e, "bmp"))  res = "image/bmp";
    else if (eq(e, "jpg"))  res = "image/jpeg";
    verify(res, "unsupported extension: %o", e);
    return string(res);
}

none path_cd(path a) {
    chdir(a->chars);
}

string path_base64(path a) {
    FILE*    f = fopen(a->chars, "rb");
    if (!f) return null;

    fseek(f, 0, SEEK_END);
    sz    flen = ftell(f);
    fseek(f, 0, SEEK_SET);

    int    rem = flen % 3;
    int    pad = (3 - rem) % 3;
    int    thr = ceil(flen / 3.0);
    int    cnt = thr  * 4;

    u8*   data = calloc(1, flen + 3);
    fread(data, flen, 1, f);
    
    string res = string(alloc, cnt);

    static const char b64[64] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    for (int i = 0; i < flen / 3; i++) {
        int istart = i * 3;
        u32 chrs   = data[istart] << 16 | data[istart+1] << 8 | data[istart+2] << 0;
        u8  i0     = (chrs >> 18) & 0x3f;
        u8  i1     = (chrs >> 12) & 0x3f;
        u8  i2     = (chrs >> 6)  & 0x3f;
        u8  i3     = (chrs >> 0)  & 0x3f;
        char b[5]  = { b64[i0], b64[i1], b64[i2], b64[i3], 0 };
        append(res, b);
    }
    if (pad != 0) {
        int istart = flen / 3 * 3;
        u32 chrs = data[istart] << 16 | data[istart+1] << 8 | data[istart+2] << 0;
        u8 i0 = (chrs >> 18) & 0x3f;
        u8 i1 = (chrs >> 12) & 0x3f;
        u8 i2 = (chrs >> 6)  & 0x3f;
        u8 i3 = (chrs >> 0)  & 0x3f;
        char b[5] = { b64[i0], pad >= 3 ? 0 : b64[i1], pad >= 2 ? 0 : b64[i2], pad >= 1 ? 0 : b64[i3], 0 };
        append(res, b);
    }

    free(data);

    for (int i = 0; i < pad; i++)
        append(res, "=");

    return res;
}

bool path_touch(path a) {
    FILE* f = fopen(a->chars, "wx");
    if (f)
        fclose(f);
    return f != null;
}

bool path_remove_dir(path a) {
    DIR* d = opendir(a->chars);
    if (!d) return true;
    struct dirent* entry;
    char full[PATH_MAX];
    while ((entry = readdir(d))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        snprintf(full, sizeof(full), "%s/%s", a->chars, entry->d_name);
        if (entry->d_type == DT_DIR)
            remove_dir(path(full));
        else
            unlink(full);
    }
    closedir(d);
    return rmdir(a->chars) == 0;
}

path path_tempfile(symbol tmpl) {
    path p = null;
    do {
        i64    h = (i64)rand() << 32 | (i64)rand();
        string r = f(string, "/tmp/%p.%s", (none*)h, tmpl);
        p        = path(chars, r->chars);
    } while (exists(p));
    return p;
}

path path_with_string(path a, string s) {
    a->chars = copy_cstr((cstr)s->chars);
    a->count   = strlen(a->chars);
    return a;
}

path path_with_tokens(path a, tokens s) {
    return path_with_string(a, s);
}

bool path_create_symlink(path target, path link) {
    bool is_err = symlink(target->chars, link->chars) == -1;
    return !is_err;
}

num path_len(path a) {
    return strlen(cstring(a));
}

bool path_is_ext(path a, symbol e) {
    string ex = ext(a);
    if (ex && cmp(ex, e) == 0)
        return true;
    return false;
}

bool path_cast_bool(path a) {
    return a->chars && strlen(a->chars) > 0;
}

sz path_cast_sz(path a) {
    return strlen(a->chars);
}

cstr path_cast_cstr(path a) {
    return (cstr)a->chars;
}

string path_cast_string(path a) {
    return new(string, chars, a->chars);
}

path path_with_cstr(path a, cstr cs) {
    a->chars = copy_cstr((cstr)cs);
    a->count   = strlen(a->chars);
    return a;
}

path path_with_symbol(path a, symbol cs) {
    a->chars = copy_cstr((cstr)cs);
    a->count   = strlen(a->chars);
    return a;
}

bool path_move(path a, path b) {
    return rename(a->chars, b->chars) == 0;
}

bool path_make_dir(path a) {
    cstr cs  = (cstr)a->chars;
    sz   len = strlen(cs);
    for (num i = 1; i < len; i++) {
        if (cs[i] == '/' || i == len - 1) {
            bool set = false;
            if (cs[i] == '/') { cs[i] = 0; set = true; }
            mkdir(cs, 0755);
            if (set) cs[i] = '/';
        }
    }
    struct stat st = {0};
    return stat(cs, &st) == 0 && S_ISDIR(st.st_mode);
}

i64 get_stat_millis(struct stat* st) {
#if defined(__APPLE__)
    return (i64)(st->st_mtimespec.tv_sec) * 1000 + st->st_mtimespec.tv_nsec / 1000000;
#elif defined(_WIN32)
    return (i64)(st->st_mtime) * 1000;  // Windows: only seconds resolution
#else
    return (i64)(st->st_mtim.tv_sec) * 1000 + st->st_mtim.tv_nsec / 1000000;
#endif
}

static path _path_latest_modified(path a, ARef mvalue, map visit) {
    cstr base_dir = (cstr)a->chars;
    if (!is_dir(a))
        return 0;

    cstr canonical = realpath(base_dir, null);
    if (!canonical) return null;
    string k = string(canonical);
    if (contains(visit, k)) return null;;
    set(visit, (Au)k, _bool(true));
    
    DIR *dir = opendir(base_dir);
    char abs[4096];
    struct dirent *entry;
    struct stat statbuf;
    i64  latest   = 0;
    path latest_f = null;

    verify(dir, "opendir");
    while ((entry = readdir(dir)) != null) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(abs, sizeof(abs), "%s/%s", base_dir, entry->d_name);

        if (stat(abs, &statbuf) == 0) {
            if (S_ISREG(statbuf.st_mode)) {
                i64 stat_millis = get_stat_millis(&statbuf);
                if (stat_millis > latest) {
                    latest = stat_millis;
                    *((i64*)mvalue) = latest;
                    latest_f = path(abs);
                }
            } else if (S_ISDIR(statbuf.st_mode)) {
                path subdir = new(path, chars, abs);
                i64 sub_latest = 0;
                path lf = _path_latest_modified(subdir, &sub_latest, visit);
                if (lf && sub_latest > latest) {
                    latest = sub_latest;
                    *((i64*)mvalue) = latest;
                    latest_f = lf;
                }
            }
        }
    }
    closedir(dir);
    return latest_f;
}

#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

i64 path_wait_for_change(path a, i64 last_mod, i64 millis) {
    struct stat st;

    int fd = open(a->chars, O_EVTONLY);
    if (fd < 0) return last_mod;

    int kq = kqueue();
    if (kq < 0) {
        close(fd);
        return last_mod;
    }

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_VNODE,
           EV_ADD | EV_CLEAR,
           NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_DELETE,
           0, NULL);

    // poll loop similar to your Linux version
    while (1) {
        i64 m = modified_time(a);
        if (m != last_mod && m != 0) {
            last_mod = m;
            break;
        }

        // wait for new events (block up to 'millis')
        struct timespec ts;
        ts.tv_sec  = millis / 1000;
        ts.tv_nsec = (millis % 1000) * 1000000;

        struct kevent out;
        int n = kevent(kq, &ev, 1, &out, 1, &ts);

        if (n > 0) {
            // something happened — check again
            continue;
        }

        // timeout → loop again
    }

    close(kq);
    close(fd);
    return last_mod;
}

#else

i64 path_wait_for_change(path a, i64 last_mod, i64 millis) {
    int    fd = inotify_init1(IN_NONBLOCK);
    int    wd = inotify_add_watch(fd, a->chars, IN_MODIFY | IN_CLOSE_WRITE);
    char   buf[4096];
    struct stat st;

    while (1) {
        i64 m = modified_time(a);
        if (m != last_mod && m != 0) {
            last_mod = m;
            break;
        }
        // drain any pending events (old ones)
        read(fd, buf, sizeof(buf));

        // block until something *new* arrives
        int ln = read(fd, buf, sizeof(buf));
        if (ln > 0) continue;
        usleep(100000); // 100 ms safety
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    return last_mod;
}
#endif

path path_latest_modified(path a, ARef mvalue) {
    return _path_latest_modified(a, mvalue, map(hsize, 64));
}
 
i64 path_modified_time(path a) {
    struct stat st;
    if (stat((cstr)a->chars, &st) != 0) return 0;

    if (is_dir(a)) {
        i64  mtime  = 0;
        path latest = latest_modified(a, &mtime);
        return mtime;
    } else {

#if defined(__APPLE__)
    return (i64)(st.st_mtimespec.tv_sec) * 1000 + st.st_mtimespec.tv_nsec / 1000000;
#elif defined(_WIN32)
    return (i64)(st.st_mtime) * 1000;  // Windows: only seconds resolution
#else
    return (i64)(st.st_mtim.tv_sec) * 1000 + st.st_mtim.tv_nsec / 1000000;
#endif

    }
}

bool path_is_dir(path a) {
    DIR   *dir = opendir(a->chars);
    if (dir == NULL)
        return false;
    closedir(dir);
    return true;
}

bool path_is_empty(path a) {
    int    n = 0;
    struct dirent *d;
    DIR   *dir = opendir(a->chars);

    if (dir == NULL)  // Not a directory or doesn't exist
        return false;

    while ((d = readdir(dir)) != NULL) {
        if (++n > 2)
            break;
    }
    closedir(dir);
    return n <= 2;  // Returns true if the directory is empty (only '.' and '..' are present)
}

string path_ext(path a) {
    for (int i = strlen(a->chars) - 1; i >= 0; i--)
        if (a->chars[i] == '.')
            return string(&a->chars[i + 1]);
    return string((cstr)null);
}

string path_stem(path a) {
    cstr cs  = (cstr)a->chars; /// this can be a bunch of folders we need to make in a row
    sz   len = strlen(cs);
    string res = new(string, alloc, 256);
    sz     dot = 0;
    for (num i = len - 1; i >= 0; i--) {
        if (cs[i] == '.')
            dot = i;
        if (cs[i] == '/' || i == 0) {
            int offset = cs[i] == '/';
            cstr start = &cs[i + offset];
            int n_bytes = (dot > 0 ? dot : len) - (i + offset);
            memcpy((cstr)res->chars, start, n_bytes);
            res->count = n_bytes;
            break;
        }
    }
    return res;
}

string path_filename(path a) {
    cstr cs  = (cstr)a->chars;
    sz   len = strlen(cs);
    string res = new(string, alloc, 256);
    for (num i = len - 1; i >= 0; i--) {
        if (cs[i] == '/' || i == 0) {
            cstr start = &cs[i + (cs[i] == '/')];
            int n_bytes = len - i - 1;
            memcpy((cstr)res->chars, start, n_bytes);
            res->count = n_bytes;
            break;
        }
    }
    return res;
}

path path_absolute(path a) {
    path  result   = new(path);
    cstr  rpath    = realpath(a->chars, null);
    result->chars  = rpath ? strdup(rpath) : copy_cstr("");
    result->count    = strlen(result->chars);
    return result;
}

path path_directory(path a) {
    path  result  = new(path);
    char* cp      = strdup(a->chars);
    char* temp    = dirname(cp);
    result->chars = strdup(temp);
    result->count   = strlen(result->chars);
    free(cp);
    return result;
}

path path_parent_dir(path a) {
    int len = strlen(a->chars);
    for (int i = len - 2; i >= 0; i--) { // -2 because we dont mind the first
        char ch = a->chars[i];
        if  (ch == '/') {
            string trim = new(string, chars, a->chars, ref_length, i);
            return new(path, chars, trim->chars);
        }
    }
    char *cp = calloc(len + 4, 1);
    memcpy(cp, a->chars, len);
    if (a->chars[len - 1] == '\\' || a->chars[len - 1] == '/')
        memcpy(&cp[len], "..", 3);
    else
        memcpy(&cp[len], "/..", 4);
    char *dir_name = dirname(cp);
    path  result   = new(path);
    result->chars  = strdup(dir_name);
    free(cp);
    return result;
}

path path_change_ext(path a, cstr ext) {
    int   e_len = strlen(ext);
    int     len = strlen(a->chars);
    int ext_pos = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (a->chars[i] == '/')
            break;
        if (a->chars[i] == '.') {
            ext_pos = i;
            break;
        }
    }
    path res = new(path);
    res->chars = calloc(32 + len + e_len, 1);
    if (ext_pos >= 0) {
        memcpy( (cstr)res->chars, a->chars, ext_pos + 1);
        if (e_len)
            memcpy((cstr)&res->chars[ext_pos + 1], ext, e_len);
        else
            ((cstr)res->chars)[ext_pos] = 0;
    } else {
        memcpy( (cstr)res->chars, a->chars, len);
        if (e_len) {
            memcpy((cstr)&res->chars[len], ".", 1);
            memcpy((cstr)&res->chars[len + 1], ext, e_len);
        }
    }
    return res;
}

path path_self() {
    char exe[4096];
#if defined(__APPLE__)
    uint32_t size = sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) != 0) return path(""); // fail safe
#else
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len == -1) return path(""); // fail safe
    exe[len] = '\0';
#endif
    return path(exe);
}

path path_share_path() {
    path exe    = path_self();
    path parent = path_parent_dir(exe); // verify this folder is bin?
    string n = stem(exe);
    path res    = form(path, "%o/../share/%o/", parent, n);
    if (dir_exists("%o", res))
        return res;
    return null;
}

bool path_is_symlink(path p) {
    struct stat st;
    return lstat(p->chars, &st) == 0 && S_ISLNK(st.st_mode);
}

path path_resolve(path p) {
    char buf[4096];
    ssize_t len = readlink(p->chars, buf, sizeof(buf) - 1);
    if (len == -1) return hold(p);
    buf[len] = '\0';
    return path(buf);
}
 
bool path_eq(path a, path b) {
    struct stat sa, sb;
    int ia = stat(a->chars, &sa);
    int ib = stat(b->chars, &sb);
    return ia == 0 &&
           ib == 0 &&
           sa.st_ino == sb.st_ino &&
           sa.st_dev == sb.st_dev;
}

#define MAX_PATH_LEN 4096

/// public statics are not 'static'
path path_cwd() {
    sz size = MAX_PATH_LEN;
    path a = new(path);
    a->chars = calloc(size, 1);
    char* res = getcwd((cstr)a->chars, size);
    a->count   = strlen(a->chars);
    assert(res != null, "getcwd failure");
    return a;
}

Exists Au_exists(Au o) {
    Au_t type = isa(o);
    path  f = null;
    if (type == typeid(string))
        f = cast(path, (string)o);
    else if (type == typeid(path))
        f = o;
    assert(f, "type not supported");
    bool is_dir = is_dir(f);
    bool r = exists(f);
    if (is_dir)
        return r ? Exists_dir  : Exists_no;
    else
        return r ? Exists_file : Exists_no;
}

bool path_exists(path a) {
    struct stat st;
    return stat(a->chars, &st) == 0;
}

u64 path_hash(path a) {
    return fnv1a_hash(a->chars, strlen(a->chars), OFFSET_BASIS);
}

static cstr ws_inline(cstr p) {
    cstr scan = p;
    while (*scan && isspace(*scan) && *scan != '\n')
        scan++;
    return scan;
}

static cstr read_word(cstr i, string* result) {
    i = ws_inline(i);
    *result = null;
    if (*i == '\n' || !*i)
        return i;

    int depth = 0;

    while (*i) {
        // stop only if: outside nesting AND hit whitespace
        if (depth == 0 && isspace(*i)) break;

        // detect start of $(...) anywhere
        if (*i == '$' && *(i + 1) == '(') {
            if (!*result) *result = string(alloc, 64);
            append_count(*result, i, 2);
            i += 2;
            depth++;
            continue;
        }

        // track end of $(...)
        if (*i == ')' && depth > 0) {
            append_count(*result, i, 1);
            i++;
            depth--;
            continue;
        }

        // quoted strings inside word (optional but safe)
        if (*i == '"' || *i == '\'') {
            char quote = *i;
            if (!*result) *result = string(alloc, 64);
            append_count(*result, i, 1);
            i++;
            while (*i && *i != quote) {
                if (*i == '\\' && *(i + 1)) {
                    append_count(*result, i, 1);
                    i++;
                }
                append_count(*result, i, 1);
                i++;
            }
            if (*i) {
                append_count(*result, i, 1);
                i++;
            }
            continue;
        }

        if (!*result) *result = string(alloc, 64);
        append_count(*result, i, 1);
        i++;
    }

    return i;
}

static cstr read_indent(cstr i, i32* result) {
    int t = 0;
    int s = 0;
    while (*i == ' ' || *i == '\t') {
        if (*i == ' ')
            s++;
        else if (*i == '\t')
            t++;
        i++;
    }
    *result = t + s / 4;
    return i;
}

static array read_lines(path f) {
    array  lines   = array(256);
    string content = (string)load(f, typeid(string), null);
    cstr   scan    = cstring(content);

    while (*scan) {
        i32 indent = 0;
        scan = read_indent(scan, &indent);
        array words = array(32);
        for (;;) {
            string w = null;
            scan = read_word(scan, &w);
            if (!w) break;
            push(words, (Au)w);
        }
        if (len(words)) {
            line l = line(indent, indent, text, words);
            push(lines, (Au)l);
        }
        if (*scan == '\n') scan++;
    }

    return lines;
}

bool path_save(path a, Au content, ctx context) {
    if (is_dir(a)) return false;
    string s = cast(string, content);
    FILE* f = fopen(a->chars, "w");
    if  (!f) return false;
    bool success = fwrite(s->chars, s->count, 1, f) == 1;
    fclose(f);
    return success;
}

Au path_load(path a, Au_t type, ctx context) {
    if (is_dir(a)) return null;
    if (type == typeid(array))
        return (Au)read_lines(a);
    FILE* f = fopen(a->chars, "rb");
    if (!f) return null;
    bool is_obj = type && !(type->traits & AU_TRAIT_PRIMITIVE);
    fseek(f, 0, SEEK_END);
    sz flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    string str = new(string, alloc, flen + 1);
    size_t n = fread((cstr)str->chars, 1, flen, f);
    fclose(f);
    assert(n == flen, "could not read enough bytes");
    str->count   = flen;
    if (type == typeid(string))
        return str;
    if (is_obj) {
        Au obj = parse(type, (cstr)str->chars, context);
        return obj;
    }
    assert(false, "not implemented");
    return null;
}

none* primitive_ffi_arb(Au_t ptype) {
    //type_ref->arb      = primitive_ffi_arb(typeid(i32));
    if ((ptype->traits & AU_TRAIT_ENUM)) return primitive_ffi_arb(ptype->src);
    if (ptype == typeid(u8))        return &ffi_type_uint8;
    if (ptype == typeid(i8))        return &ffi_type_sint8;
    if (ptype == typeid(u16))       return &ffi_type_uint16;
    if (ptype == typeid(i16))       return &ffi_type_sint16;
    if (ptype == typeid(u32))       return &ffi_type_uint32;
    if (ptype == typeid(i32))       return &ffi_type_sint32;
    if (ptype == typeid(u64))       return &ffi_type_uint64;
    if (ptype == typeid(i64))       return &ffi_type_sint64;
    if (ptype == typeid(f32))       return &ffi_type_float;
    if (ptype == typeid(f64))       return &ffi_type_double;
    if (ptype == typeid(AFlag))     return &ffi_type_sint32;
    if (ptype == typeid(bool))      return &ffi_type_uint32;
    if (ptype == typeid(num))       return &ffi_type_sint64;
    if (ptype == typeid(sz))        return &ffi_type_sint64;
    if (ptype == typeid(none))      return &ffi_type_void;
    return &ffi_type_pointer;
}


static none copy_file(path from, path to) {
    path f_to = is_dir(to) ? f(path, "%o/%o", to, filename(from)) : (path)hold(to);
    FILE *src = fopen(cstring(from), "rb");
    FILE *dst = fopen(cstring(f_to), "wb");
    drop(f_to);
    verify(src && dst, "copy_file: cannot open file");

    char buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, n, dst);

    fclose(src);
    fclose(dst);
}

none path_cp(path from, path to, bool recur, bool if_newer) {
    if (dir_exists("%o", from) && file_exists("%o", to))
        fault("attempting to copy from directory to a file");
    
    bool same_kind = is_dir(from) == is_dir(to);
    if (file_exists("%o", from)) {
        if (!if_newer || (!same_kind || modified_time(from) > modified_time(to)))
            copy_file(from, to);
    } else {
        verify(is_dir(from), "source must be directory");
        make_dir(to);
    
        DIR *dir = opendir(cstring(from));
        struct dirent *entry;
        verify(dir, "opendir");
    
        while ((entry = readdir(dir)) != null) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            path src = form(path, "%o/%s", from, entry->d_name);
            path dst = form(path, "%o/%s", to,   entry->d_name);
            cp(src, dst, recur, if_newer);
        }
        closedir(dir);
    }
}

array path_ls(path a, string pattern, bool recur) {
    cstr base_dir = (cstr)a->chars;
    array list = new(array, alloc, 32); // Initialize array for storing paths
    if (!is_dir(a))
        return list;
    DIR *dir = opendir(base_dir);
    char abs[MAX_PATH_LEN];
    struct dirent *entry;
    struct stat statbuf;

    assert (dir, "opendir");
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(abs, sizeof(abs), "%s/%s", base_dir, entry->d_name);
        string s_abs = string(abs);
        if (stat(abs, &statbuf) == 0) {
            if (S_ISREG(statbuf.st_mode)) {
                if (!pattern || !pattern->count || ends_with(s_abs, pattern->chars))
                    push(list, (Au)new(path, chars, abs));
                
            } else if (S_ISDIR(statbuf.st_mode)) {
                if (recur) {
                    path subdir = new(path, chars, abs);
                    array sublist = ls(subdir, pattern, recur);
                    concat(list, sublist);
                } else if (!pattern)
                    push(list, (Au)new(path, chars, abs));
            }
        }
    }
    closedir(dir);
    return list;
}


struct mutex_t {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
};

none mutex_init(mutex m) {
    m->mtx = calloc(sizeof(struct mutex_t), 1);
    pthread_mutex_init(&m->mtx->lock, null);
    if (m->cond) pthread_cond_init(&m->mtx->cond, null);
}

none mutex_dealloc(mutex m) {
    pthread_mutex_destroy(&m->mtx->lock);
    if (m->cond) pthread_cond_destroy(&m->mtx->cond);
    free(m->mtx);
}

none mutex_lock(mutex m) {
    pthread_mutex_lock(&m->mtx->lock);
}

none mutex_unlock(mutex m) {
    pthread_mutex_unlock(&m->mtx->lock);
}

none mutex_cond_broadcast(mutex m) {
    pthread_cond_broadcast(&m->mtx->lock);
}

none mutex_cond_signal(mutex m) {
    pthread_cond_signal(&m->mtx->lock);
}

none mutex_cond_wait(mutex m) {
    pthread_cond_wait(&m->mtx->cond, &m->mtx->lock);
}

define_class(mutex, Au)

// idea:
// silver should: swap = and : ... so : is const, and = is mutable-assign
// we serialize our data with : and we do not think of this as a changeable form, its our data and we want it intact, lol
// serialize Au into json
string json(Au a) {
    Au_t  type  = isa(a);
    string res   = string(alloc, 1024);
    /// start at 1024 pre-alloc
    if (!a) {
        append(res, "null");
    } else if (instanceof(a, string)) {
        push(res, '"');
        concat(res, escape((string)a));
        push(res, '"');
    } else if (instanceof(a, array)) {
        // array with items
        push(res, '[');
        bool first = true;
        each ((collective)a, Au, i) {
            if (!first) push(res, ',');
            concat(res, json(i));
            first = false;
        }
        push(res, ']');
    } else if (instanceof(a, map)) {
        push(res, '{');
        map ma = a;
        bool first = true;
        pairs (ma, i) {
            if (!first) push(res, ',');
            string k = json(i->key);
            string v = json(i->value);
            concat(res, k);
            push(res, ':');
            concat(res, v);
            first = false;
        }
        push(res, '}');
    } else if (!(type->traits & AU_TRAIT_PRIMITIVE)) {
        // Au with fields
        push(res, '{');
        bool one = false;
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if (one) push(res, ',');
            if (!(mem->member_type == AU_MEMBER_VAR)) continue;
            concat(res, json(string(mem->ident)));
            push  (res, ':');
            Au value = Au_get_property(a, mem->ident);
            concat(res, json(value));
            one = true;
        }
        push(res, '}');
    } else {
        serialize(type, res, a);
    }
    return res;
}

static string parse_symbol(cstr input, cstr* remainder, ctx context) {
    cstr start = null;
    bool second = false;
    while ((!second && isalpha(*input)) || 
           ( second && isalnum(*input)) ||
           ((*input == '-' || *input == '_')  && second))
    {
        if (!start) start = input;
        input++;
        second = true;
    }
    *remainder = ws(input);

    if (start) {
        string r = string(chars, start, ref_length, (size_t)input - (size_t)start);
        for (int i = 0; i < r->count; i++)
            if (r->chars[i] == '-') ((cstr)r->chars)[i] = '_';
        return r;
    }
    
    return null;
}

static string parse_json_string(cstr origin, cstr* remainder, ctx context) {
    char delim = *origin;
    if (delim != '\"' && delim != '\'')
        return null;
    
    string res = string(alloc, 64);
    cstr scan;
    for (scan = &origin[1]; *scan;) {
        if (*scan == delim) {
            scan++;
            break;
        }
        if (*scan == '\\') {
            scan++;
            if (*scan == 'n') push(res, 10);
            else if (*scan == 'r') push(res, 13);
            else if (*scan == 't') push(res,  9);
            else if (*scan == 'b') push(res,  8);
            else if (*scan == '/') push(res, '/');
            else if (*scan == 'u') {
                // Read the next 4 hexadecimal digits and compute the Unicode codepoint
                uint32_t code = 0;
                for (int i = 0; i < 4; i++) {
                    scan++;
                    char c = *scan;
                    if      (c >= '0' && c <= '9') code = (code << 4) | (c - '0');
                    else if (c >= 'a' && c <= 'f') code = (code << 4) | (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') code = (code << 4) | (c - 'A' + 10);
                    else
                        fault("Invalid Unicode escape sequence");
                }
                // Convert the codepoint to UTF-8 encoded bytes
                if (code <= 0x7F) {
                    push(res, (i8)code);
                } else if (code <= 0x7FF) {
                    push(res, (i8)(0xC0 | (code >> 6)));
                    push(res, (i8)(0x80 | (code & 0x3F)));
                } else if (code <= 0xFFFF) {
                    push(res, (i8)(0xE0 | ( code >> 12)));
                    push(res, (i8)(0x80 | ((code >> 6) & 0x3F)));
                    push(res, (i8)(0x80 | ( code       & 0x3F)));
                } else if (code <= 0x10FFFF) {
                    push(res, (i8)(0xF0 | ( code >> 18)));
                    push(res, (i8)(0x80 | ((code >> 12) & 0x3F)));
                    push(res, (i8)(0x80 | ((code >> 6)  & 0x3F)));
                    push(res, (i8)(0x80 | ( code        & 0x3F)));
                } else {
                    fault("Unicode code point out of range");
                }
            }
        } else
            push(res, *scan);
        scan++;
    }
    *remainder = scan;
    return (context && delim == '\'') ? interpolate(res, context) : res;
}

static Au parse_array(cstr s, Au_t schema, Au_t meta, cstr* remainder, ctx context);

Au_t member_first(Au_t type, Au_t find, bool poly) {
    if (poly && type->context != typeid(Au)) {
        Au_t m = member_first(type->context, find, poly);
        if (m) return m;
    }
    for (num i = 0; i < type->members.count; i++) {
        Au_t m = (Au_t)type->members.origin[i];
        if (!(m->member_type == AU_MEMBER_VAR)) continue;
        if (m->type == type) return m;
    }
    return null;
}

static Au parse_object(cstr input, Au_t schema, Au_t meta_type, cstr* remainder, ctx context) {
    cstr   scan   = ws(input);
    cstr   origin = null;
    Au res    = null;
    char  *endptr;

    if (remainder)
       *remainder = null;

    string sym     = parse_symbol(scan, &scan, context);
    bool   set_ctx = sym && context && !remainder && context->establishing && eq(sym, "ctx");
    bool   is_true = false;

    if (sym && (eq(sym, "null"))) {
        res = null;
    }
    else if (sym && ((is_true = eq(sym, "true")) || eq(sym, "false"))) {
        verify(!schema || schema == typeid(bool), "type mismatch");
        res = _bool(is_true); 
    }
    else if (*scan == '[') {
        if (sym) {
            // can be an enum, can be a struct, or class
            // for these special syntax, we cannot take schema & meta_type into account
            // this is effectively 'ason' syntax
            scan = ws(scan + 1);
            Au_t type = Au_find_type(sym->chars, null);
            verify(type, "type not found: %o", sym);

            if (type->traits & AU_TRAIT_ENUM) {
                // its possible we could reference a context variable within this enum [ value-area ]
                // in which case, we could effectively look it up
                Au evalue = alloc(type, 1, null);
                string enum_symbol = parse_symbol(scan, &scan, context);
                verify(enum_symbol, "enum symbol expected");
                verify(*scan == ']', "expected ']' after enum symbol");
                scan++;
                Au_t e = find_member(
                    type, AU_MEMBER_ENUMV, enum_symbol->chars, false);
                verify(e, "enum symbol %o not found in type %s", enum_symbol, type->ident);
                memcpy(evalue, e->value, e->type->typesize);

                res = evalue;
            } else if (type->traits & AU_TRAIT_STRUCT) {
                Au svalue = alloc(type, 1, null);
                for (num i = 0; i < type->members.count; i++) {
                    Au_t m = (Au_t)type->members.origin[i];
                    if (!(m->member_type == AU_MEMBER_VAR)) continue;
                    Au f = (Au)((cstr)svalue + m->offset);
                    Au r = parse_object(scan, null, null, &scan, context);
                    verify(r && isa(r) == m->type, "type mismatch while parsing struct %s:%s (read: %s, expect: %s)",
                        type->ident, m->ident, !r ? "null" : isa(r)->ident, m->type->ident);
                    if (*scan == ',') scan = ws(scan + 1); // these dumb things are optional
                }
                scan++;
                res = svalue;
            } else {
                // this is parsing a 'constructor call', so we must effectively call this constructor with the arg given
                // only a singular arg is allowed for these
                Au r = parse_object(scan, null, null, &scan, context);
                verify(r, "expected value to give to type %s", type->ident);
                verify(*scan == ']', "expected ']' after construction of %s", type->ident);
                scan++;

                res = construct_with(type, r, null);
            }
        } else {
            res = parse_array (scan, schema, meta_type, remainder, context);
            scan = *remainder;
        }
    }
    else if ((*scan >= '0' && *scan <= '9') || *scan == '-') {
        verify(!sym, "unexpected numeric after symbol %o", sym);
        origin = scan;
        int has_dot = 0;
        while (*++scan) {
            has_dot += *scan == '.';
            if (*scan == 'f')
                break;
            if (*scan != '.' && !(*scan >= '0' && *scan <= '9'))
                break;
        }
        if (has_dot || (schema && schema->traits & AU_TRAIT_REALISTIC)) {
            bool force_f32 = false;
            if (has_dot) {
                force_f32 = *scan == 'f';
                if (force_f32) scan++;
            }
            if (schema == typeid(i64)) {
                double v = strtod(origin, &scan);
                res = _i64((i64)floor(v));
            }
            else if (force_f32 || schema == typeid(f32)) {
                res = _f32(strtof(origin, &scan));
                if (force_f32)
                    scan++; // f++
            }
            else
                res = _f64(strtod(origin, &scan));
        } else
            res = _i64(strtoll(origin, &scan, 10));
    }
    else if (*scan == '"' || *scan == '\'') {
        verify(!sym, "unexpected string after symbol %o", sym);
        origin = scan;
        string js = parse_json_string(origin, &scan, context); // todo: use context for string interpolation
        res = construct_with((schema && schema != typeid(Au)) 
            ? schema : typeid(string), js, context);
    }
    else if (*scan == '{') { /// Type will make us convert to the Au, from map, and set is_map back to false; what could possibly get crossed up with this one
        if (sym) {
            verify(!schema || schema == typeid(Au) || eq(sym, schema->ident),
                "expected type: %s, found %o", schema->ident, sym);

            if (!schema) {
                schema = Au_find_type(sym->chars, null);
                verify(schema, "type not found: %o", sym);
            }

            if (schema == typeid(Au)) {
                schema = Au_find_type(sym->chars, null);
                verify(schema, "%s not found", sym->chars);
            }
        }
        Au_t use_schema = schema ? schema : typeid(map);
        bool  is_map     = use_schema == typeid(map);
        scan = ws(&scan[1]);
        map props = map(hsize, 16, assorted, true);

        for (;;) {
            scan = ws(&scan[0]);
            if (*scan == '}') {
                scan++;
                break;
            }

            if (!context && *scan != '\"')
                return null;

            origin        = scan;
            string name   = null;
            Au_t mem    = null;
            bool   quick_map = false;
            bool   json_type = false;

            // if Au, then we find the first Au
            if (*scan == '{' && use_schema) {
                /// this goes to the bottom class above Au first, then proceeds to look
                /// if you throw a map onto a subclass of element, its going to look through element first
                mem = member_first(use_schema, typeid(map), true);
                verify(mem, "map not found in Au %s (shorthand {syntax})",
                    use_schema->ident);
                name = string(mem->ident);
                quick_map = true;
            } else
                name = (*scan != '\"' && *scan != '\'') ?
                    (string)parse_symbol(origin, &scan, context) : 
                    (string)parse_json_string(origin, &scan, context);
            
            if (!mem) {
                json_type = cmp(name, "Type") == 0;
                mem  = (is_map) ? null : 
                    find_member(use_schema, AU_MEMBER_VAR, name->chars, true);
            }

            if (!json_type && !mem && !is_map && !context) {
                print("property '%o' not found in type: %s", name, use_schema->ident);
                return null;
            }
            
            if (!quick_map) {
                bool short_hand_key = false;
                if (context && *scan == '{') {
                    scan = ws(origin); // nice trick here for this mode, effectively reads symbol again in parse_object
                    short_hand_key = true;
                } else if (*scan != ':')
                    return null;
                else
                    scan = ws(&scan[1]);
            }
            Au value = parse_object(scan, (mem ? mem->type : null),
                (mem->args.origin ? *mem->args.origin : null), &scan, context);
            
            //if (!value)
            //    return null;

            if (set_ctx && value)
                set(context, (Au)name, value);

            if (json_type) {
                string type_name = value;
                use_schema = Au_find_type(type_name->chars, null);
                verify(use_schema, "type not found: %o", type_name);
            } else
                set(props, (Au)name, value);

            if (*scan == ',') {
                scan++;
                continue;
            } else if (!context && *scan != '}')
                return null;
        }

        if (set_ctx && use_schema == typeid(ctx)) // nothing complex; we have a ctx already and will merge these props in
            use_schema = typeid(map);
        
        res = construct_with(use_schema, props, context); // makes a bit more sense to implement required here
        if (use_schema != typeid(map))
            drop(props);
        else
            hold(props);
    } else {
        res = (context && sym) ? get(context, sym) : null;
        if (res) {
            verify(!schema || inherits(isa(res), schema),
                "variable type mismatch: %s does not match expected %s",
                isa(res)->ident, schema->ident);
        } else
            verify(!sym, "cannot resolve symbol: %o", sym);
    }
    if (remainder) *remainder = ws(scan);
    return res;
}

static array parse_array_objects(cstr* s, Au_t element_type, ctx context) {
    cstr scan = *s;
    array res = array(64);
    static int seq = 0;
    seq++;

    for (;;) {
        if (scan[0] == ']') {
            scan = ws(&scan[1]);
            break;
        }
        Au a = parse_object(scan, element_type, null, &scan, context);
        push(res, a);
        scan = ws(scan);
        if (scan && scan[0] == ',') {
            scan = ws(&scan[1]);
            continue;
        }
    }
    *s = scan;
    return res;
}

static Au parse_array(cstr s, Au_t schema, Au_t meta_type, cstr* remainder, ctx context) {
    cstr scan = ws(s);
    verify(*scan == '[', "expected array '['");
    scan = ws(&scan[1]);
    Au res = null;
    if (!schema || (schema == typeid(array) || schema->src == typeid(array))) {
        Au_t element_type = meta_type ? meta_type : (schema ? *(Au_t*)schema->meta.origin : typeid(map));
        res = parse_array_objects(&scan, element_type, context);
    } else if (schema->meta.count && *(Au_t*)schema->meta.origin == typeid(i64)) { // should support all vector types of i64 (needs type bounds check with vmember_count)
        array arb = parse_array_objects(&scan, typeid(i64), context);
        int vcount = len(arb);
        res = alloc2(schema, typeid(i64), new_shape(vcount, 0));
        int n = 0;
        each(arb, Au, a) {
            verify(isa(a) == typeid(i64), "expected i64");
            ((i64*)res)[n++] = *(i64*)a;
        }
    } else if (schema->meta.count && *(Au_t*)schema->meta.origin == typeid(f32)) { // should support all vector types of f32 (needs type bounds check with vmember_count)
        array arb = parse_array_objects(&scan, typeid(f32), context);
        int vcount = len(arb);
        res = alloc(typeid(f32), vcount, null);
        int n = 0;
        each(arb, Au, a) {
            Au_t a_type = isa(a);
            if (a_type == typeid(i64))      ((f32*)res)[n++] =  (float)*(i64*)a;
            else if (a_type == typeid(f32)) ((f32*)res)[n++] = *(float*)a;
            else if (a_type == typeid(f64)) ((f32*)res)[n++] =  (float)*(double*)a;
            else fault("unexpected type");
        }
    } else if (constructs_with(schema, typeid(array))) {
        // i forget where we use this!
        array arb = parse_array_objects(&scan, typeid(i64), context);
        res = construct_with(schema, arb, null);
    } else if (schema->src == typeid(vector)) {
        Au_t scalar_type = *(Au_t*)schema->meta.origin;
        verify(scalar_type, "scalar type required when using vector (define a meta-type of vector with type)");
        
        array prelim = parse_array_objects(&scan, null, context);
        int count = len(prelim);
        // this should contain multiple arrays of scalar values; we want to convert each array to our 'scalar_type'
        // for instance, we may parse [[1,2,3,4,5...16],...] mat4x4's; we merely need to validate vmember_count and vmember_type and convert
        // if we have a vmember_count of 0 then we are dealing with a single primitive type
        vector vres = alloc(schema, 1, null);
        vres->shape = new_shape(count, 0);
        Au_initialize(vres);
        i8* data = vdata(vres);
        int index = 0;
        each (prelim, Au, o) {
            Au_t itype = isa(o);
            if (itype->traits & AU_TRAIT_PRIMITIVE) {
                /// parse Au here (which may require additional
                memcpy(&data[index], o, scalar_type->typesize);
                index += scalar_type->typesize;
            } else {
                fault("implement struct parsing");
            }
        }
        res = vres;
    } else {
        fault("unhandled vector type: %s", schema ? schema->ident : null);
    }
    if (remainder) *remainder = scan;
    return res;
}

static map ctx_checksums; // where context goes to live, lol.

static string extract_context(cstr src, cstr *endptr) {
    src = ws(src);
    const char *start = strstr(src, "ctx");
    if (!start) return NULL;

    const char *scan = start;
    while (*scan && *scan != '{') scan++;  // find first {
    if (*scan != '{') return NULL;
    scan++;  // past initial {

    int depth = 1;
    bool in_string = false;
    char string_delim = 0;

    while (*scan && depth > 0) {
        if (in_string) {
            if (*scan == '\\' && scan[1]) {
                scan += 2;
                continue;
            } else if (*scan == string_delim) {
                in_string = false;
                scan++;
                continue;
            }
        } else {
            if (*scan == '\'' || *scan == '"') {
                in_string = true;
                string_delim = *scan++;
                continue;
            } else if (*scan == '#' || strncmp(scan, "/*", 2) == 0) {
                scan = ws((cstr)scan);
                continue;
            } else if (*scan == '{') {
                depth++;
            } else if (*scan == '}') {
                depth--;
            }
        }
        scan++;
    }

    size_t len = scan - start;
    if (endptr) *endptr = (cstr)scan;
    return string(chars, start, ref_length, len);
}

Au parse(Au_t schema, cstr s, ctx context) {
    printf("parse: %s", s);
    if (context) {
        if (!ctx_checksums) ctx_checksums = hold(map(hsize, 32));
        string key = f(string, "%p", context);
        u64*   chk = get(ctx_checksums, key);
        string ctx = extract_context(s, &s);
        u64 h = hash(ctx);
        if (!chk || *chk != h) {
            set(ctx_checksums, (Au)key, _u64(h));
            context->establishing = true;
            map ctx_update = parse_object((cstr)ctx->chars, null, null, null, context);
            context->establishing = false;
        }
    }
    return parse_object(s, schema, null, null, context);
}

typedef struct thread_t {
    pthread_t       obj;
    mutex           lock;
    i32             index;
    bool            done;
    Au               w;
    Au               next;
    async           t;
    i32             jobs;
} thread_t;

static none async_runner(thread_t* thread) {
    async t = thread->t;

    for (; thread->next; unlock(thread->lock)) {
        t->work_fn(thread->w);
        lock(thread->lock);
        thread->done = true;
        cond_signal(thread->lock);
        cond_broadcast(t->global);
        while (thread->next == thread->w)
            cond_wait(thread->lock);
        if (!thread->next)
            break;
        thread->done = false;
        drop(thread->w);
        thread->w    = thread->next;
        thread->jobs++; // set something so sync can know
    }
    unlock(thread->lock);
}

none async_init(async t) {
    i32    n = len(t->work);
    verify(n > 0, "no work given, no threads needed");
    // we can then have a worker modulo restriction
    // (1 by default to grab the next available work; 
    //  or say modulo of work length to use a pool)
    t->threads = (thread_t*)calloc(sizeof(thread_t), n);
    t->global = mutex(cond, true);
    for (int i = 0; i < n; i++) {
        thread_t* thread = &t->threads[i];
        thread->index = i;
        thread->w     = get(t->work, i);
        thread->next  = thread->w;
        thread->t     = t;
        thread->lock  = mutex(cond, true);
        lock(thread->lock);
        pthread_create(&thread->obj, null, async_runner, thread);
    }
    for (int i = 0; i < n; i++) {
        thread_t* thread = &t->threads[i];
        unlock(thread->lock);
    }
}

none async_dealloc(async t) {
    sync(t, null);
    for (int i = 0, n = len(t->work); i < n; i++) {
        thread_t* thread = &t->threads[i];
        drop(thread->lock);
    }
}

Au async_sync(async t, Au w) {
    int n = len(t->work);
    Au result = null;

    if (w) {
        /// in this mode, wait for one to finish
        /// can we set a condition here or purely wait real fast?
        for (;;) {
            bool found = false;
            lock(t->global);
            for (int i = 0; i < n; i++) {
                thread_t* thread = &t->threads[i];
                lock(thread->lock);
                if (thread->done) {
                    result = copy(thread->w);
                    thread->next = w;
                    found = true;
                    cond_signal(thread->lock);
                    unlock(thread->lock);
                    break;
                }
                unlock(thread->lock);
            }
            if (found) {
                unlock(t->global);
                if (found)
                    break;
            }
            cond_wait(t->global);
        }
    } else {
        for (int i = 0; i < n; i++) {
            thread_t* thread = &t->threads[i];
            lock(thread->lock);
            thread->next = null;
            unlock(thread->lock);
            pthread_join(thread->obj, null);
        }
        // user can get thread->work for simple cases of sync
        // why return either array or individual work based on argument?
    }
    return result;
}

/*
struct inotify_event {
    int wd;           // THIS is the ID of the watch
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char name[];      // Optional file name (if watching a dir)
};
*/

#undef remove
#ifdef _WIN32
#include <io.h>
#endif

none watch_init(watch a) {
#ifndef __APPLE__
    if (!a->res) return;
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        perror("inotify_init1");
        exit(1);
    }

    int wd = inotify_add_watch(fd, a->res->chars, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd == -1) {
        perror("inotify_add_watch");
        exit(1);
    }

    char buf[4096]
        __attribute__((aligned(__alignof__(struct inotify_event))));
    
    while (1) {
        int len = read(fd, buf, sizeof(buf));
        if (len <= 0) continue;

        for (char *ptr = buf; ptr < buf + len; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;
            printf("Event on %s: ", event->len ? event->name : "file");
            if (event->mask & IN_CREATE) puts("Created");
            if (event->mask & IN_DELETE) puts("Deleted");
            if (event->mask & IN_MODIFY) puts("Modified");
            ptr += sizeof(struct inotify_event) + event->len;
        }
        usleep(100000); // throttle
    }

    close(fd);
#endif
}

none watch_dealloc(watch a) {
    pause(a);
}

none watch_pause(watch a) {
}

none watch_start(watch a) {
}

bool is_alphabetic(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
        return true;
    return false;
}


Au subs_invoke(subs a, Au arg) {
    each (a->entries, subscriber, sub) {
        sub->method(sub->target, arg);
    }
    return null;
}

none subs_add(subs a, Au target, callback fn) {
    subscriber sub = subscriber(target, target, method, fn);
    push(a->entries, (Au)sub);
}

#undef cast
// generic casting function
Au typecast(Au_t type, Au a) {
    if (Au_instance_of(a, type)) return (Au)a;
    Au_t au = isa(a);
    Au_t m = Au_member_type(au, AU_MEMBER_CAST, type, true);
    if (m) {
        Au(*fcast)(Au) = (void*)m->value;
        return fcast(a);
    }
    return null;
}

// ----------------------------------------
none shape_dealloc(shape a) {
    if (!a->is_global)
        free(a->data);
}

none shape_push(shape a, i64 i) {
    i64* prev = a->data;
    a->data = calloc(sizeof(i64), (a->count + 2));
    a->data[a->count++] = i;
    a->data[a->count]   = 0;
    if (!a->is_global)
        free(prev);
    a->is_global = false;
}

none shape_init(shape a) {
    if (!a->is_global) {
        i64 sz = a->count ? a->count : 16;
        i64* cp = calloc(sizeof(i64), (sz + 1));
        if (a->data)
            memcpy(cp, a->data, sizeof(i64) * a->count);
        else
            memset(cp, 0, sizeof(i64) * sz);
        cp[a->count] = 0;
        a->data = cp;
    }
}


// ----------------------------------------
token token_with_cstr(token a, cstr s) {
    a->chars = s;
    a->count   = strlen(s);
    return a;
}

token token_copy(token a) {
    return token(chars, a->chars, line, a->line, column, a->column);
}


string read_string(cstr cs, bool is_const) {
    int ln = strlen(cs);
    string res = is_const ? (string)const_string(alloc, ln) : string(alloc, ln);
    char*   cur = cs;
    while (*cur) {
        int inc = 1;
        if (cur[0] == '\\') {
            symbol app = null;
            switch (cur[1]) {
                case 'n':  app = "\n"; break;
                case 't':  app = "\t"; break;
                case 'r':  app = "\r"; break;
                case '\\': app = "\\"; break;
                case '\'': app = "\'"; break;
                case '\"': app = "\""; break;
                case '\f': app = "\f"; break;
                case '\v': app = "\v"; break;
                case '\a': app = "\a"; break;
                default:   app = "?";  break;
            }
            inc = 2;
            append(res, (cstr)app);
        } else {
            char app[2] = { cur[0], 0 };
            append(res, (cstr)app);
        }
        cur += inc;
    }
    return res;
}


Au read_numeric(token a) {
    cstr cs = (cstr)a->chars;
    if (strcmp(cs, "true") == 0) {
        bool v = true;
        return primitive(typeid(bool), &v);
    }
    if (strcmp(cs, "false") == 0) {
        bool v = false;
        return primitive(typeid(bool), &v);
    }
    bool is_base16 = cs[0] == '0' && cs[1] == 'x'; //0xff_ff_ff_ff_ff_ff_ff_ffull
    i32  is_unsigned = 0;
    i32  is_long     = 0;
    i64  ln          = strlen(cs);
    i32  suffix      = 0;

    for (int i = 0; i < 2; i++) {
        int ii = ln - 1 - suffix;
        if (ii >= 0 && tolower(cs[ii]) == 'l') {
            is_long++;
            suffix++;
        }
    }

    int ii = ln - 1 - suffix;
    if (ii >= 0 && tolower(cs[ii]) == 'u') {
        is_unsigned++;
        suffix++;
    }

    if (is_long == 0)
        for (int i = 0; i < 2; i++) {
            int ii = ln - 1 - suffix;
            if (ii >= 0 && tolower(cs[ii]) == 'l') {
                is_long++;
                suffix++;
            }
        }
    
    bool has_dot = strstr(cs, ".") != 0;
    i64  val = 0;
    i32 digits = 0;
    bool is_base10 = false;

    if (is_base16) {
        // not supporting the float format at the moment, like rust
        if (ln <= 2 || ln > 28) return null;

        i32 nlen = 0;
        
        for (int i = 0; i < ln - suffix; i++) {
            i32 v = tolower(cs[i]);
            if (v == '_') continue;
            if (!((v >= 'a' && v <= 'f') || (v >= '0' && v <= '9')))
                return null;
            i32 n = (v >= 'a' && v <= 'f') ? (v - 'a' + 10) : (v - '0');
            val = (val << 4) | (n & 0xF);
            digits++;
        }
    } else if (!has_dot) {
        is_base10 = true;
        bool is_digit = cs[0] >= '0' && cs[0] <= '9';
        if (!is_digit && !has_dot)
            return null;
        char* e = null;
        val = is_unsigned ? strtoull(cs, &e, 10) : strtoll(cs, &e, 10);
        digits = (sz)e - (sz)cs - (sz)(cs[0] == '-');
    } else {
        char* e = null;
        f64 v = strtod(cs, &e);
        return primitive(typeid(f64), &v);
    }

    if (is_unsigned) {
        if (is_long == 0) {
            return (!a->cmode || (!is_base10 && digits > 8) || (val > 0xffffffff)) ?
                _u64((i64)val) : _u32((i32)val);
        } else if (is_long == 1) {
            return _u32((u32)val);
        } else if (is_long == 2) {
            return _u64((u64)val);
        }
    } else {
        if (is_long == 0) {
            return (!a->cmode || (!is_base10 && digits > 8) || (val > 0xffffffff)) ?
                _i64((i64)val) : _i32((i32)val);
        } else if (is_long == 1) {
            return _i32((i32)val);
        } else if (is_long == 2) {
            return _i64((i64)val);
        }
    }
    return null;
}

static Au             parser_target;
static callback_extra parser_ident;

void tokens_init(tokens a) {
    // lets not allow switching of parser functions for the time being
    verify (!parser_ident || a->parser == parser_ident,
        "invalid parser state");
    
    if (!parser_ident) {
         parser_ident = a->parser;
    }
    // the target will change in order, 
    // however its registration will be updated with an init
    // before ctr is ever called; with those, they are within the scope of the user
    parser_target = a->target;
    parser_ident(a->target, a->input, a);
}

// constructors have ability to return whatever data they want, and
// when doing so, the buffer is kept around for the next user (max of +1)
tokens tokens_with_cstr(tokens a, cstr cs) {
    a->parser = parser_ident;
    a->target = parser_target;
    a->input  = (Au)string(cs);
    return a;
}

void token_init(token a) {
    cstr prev = a->chars;
    sz length = a->count ? a->count : strlen(prev);
    a->chars  = (cstr)calloc((a->alloc ? a->alloc : length) + 1, 1);
    a->count    = length;

    memcpy(a->chars, prev, length);

    if (a->chars[0] == '\"' || a->chars[0] == '\'') {
        string crop = string(chars, &a->chars[1], ref_length, length - 2);
        a->literal = (Au)read_string(crop->chars, a->chars[0] == '\"');
    } else
        a->literal = read_numeric(a);
}

string token_location(token a) {
    string f = form(string, "%o:%i:%i", a->source, a->line, a->column);
    return f;
}

Au_t token_get_type(token a) {
    return a->literal ? isa(a->literal) : null;
}

Au_t token_is_bool(token a) {
    string t = (string)a;
    return (cmp(t, "true") || cmp(t, "false")) ?
        (Au_t)typeid(bool) : null;
}

array read_arg(array tokens, int start, int* next_read) {
    int   level = 0;
    int   ln    = len(tokens);
    bool  count = ln - start;
    array res   = array(alloc, 32);

    for (int i = start; i < ln; i++) {
        token t = (token)get(tokens, i);

        if (eq(t, "("))
            level++;
        else if (eq(t, ")") && level > 0)
            level--;

        if ((eq(t, ",") || eq(t, ")")) && level == 0) {
            *next_read = i + (int)eq(t, ",");
            return res;
        }

        push(res, (Au)t);
    }
    return count > 0 ? null : res;
}

// this signals an application entry
define_class(subscriber, Au)
define_class(subs, Au)

define_any(Au, Au, sizeof(struct _Au), AU_TRAIT_CLASS);

define_class(watch,   Au)
define_class(msg,     Au)
define_class(async,   Au)


define_abstract(numeric,        0, Au)
define_abstract(string_like,    0, Au)
define_abstract(nil,            0, Au)
define_abstract(raw,            0, Au)
define_abstract(ref,            0, Au)
define_abstract(imported,       0, Au)
define_abstract(weak,           0, Au)
define_abstract(functional,     0, Au)
 


define_primitive(ref_u8, numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED, u8)
define_primitive(ref_u16,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED,  u16)
define_primitive(ref_u32,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED,  u32)
define_primitive(ref_u64,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED,  u64)
define_primitive(ref_i8,     numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED,    i8)
define_primitive(ref_i16,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED,    i16)
define_primitive(ref_i32,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED,    i32)
define_primitive(ref_i64,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED,    i64)
define_primitive(ref_bool,   numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED,  bool)
//define_primitive(ref_num,    numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED,  num)
//define_primitive(ref_sz,     numeric, AU_TRAIT_POINTER | AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED,  sz)
define_primitive(ref_f32,    numeric, AU_TRAIT_POINTER | AU_TRAIT_REALISTIC, f32)
define_primitive(ref_f64,    numeric, AU_TRAIT_POINTER | AU_TRAIT_REALISTIC, f64)


define_primitive( u8,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive(u16,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive(u32,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive(u64,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive( i8,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED)
define_primitive(i16,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED)
define_primitive(i32,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED)
define_primitive(i64,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED)
define_primitive(bool,   numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive(num,    numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED)
define_primitive(sz,     numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_SIGNED)
define_primitive(bf16,   numeric, AU_TRAIT_REALISTIC)
define_primitive(fp16,   numeric, AU_TRAIT_REALISTIC)
define_primitive(f32,    numeric, AU_TRAIT_REALISTIC)
define_primitive(f64,    numeric, AU_TRAIT_REALISTIC)
define_primitive(AFlag,  numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive(cstr,   string_like, AU_TRAIT_POINTER, i8)
define_primitive(symbol, string_like, AU_TRAIT_CONST | AU_TRAIT_POINTER, i8)
define_primitive(cereal, raw, 0)
define_primitive(none,   nil, AU_TRAIT_VOID)
//define_primitive(Au_t,  raw, 0)
define_primitive(handle, raw, AU_TRAIT_POINTER, u8)
define_primitive(ARef,   ref, AU_TRAIT_POINTER, Au)
define_primitive(Au_ts, ref, AU_TRAIT_POINTER, Au_t)
define_primitive(floats, raw, AU_TRAIT_POINTER, f32)

define_primitive(func,     raw, AU_TRAIT_FUNCPTR)
define_primitive(hook,     raw, AU_TRAIT_FUNCPTR)
define_primitive(callback, raw, AU_TRAIT_FUNCPTR)
define_primitive(callback_extra, raw, AU_TRAIT_FUNCPTR)
define_primitive(cstrs, raw, AU_TRAIT_POINTER, cstr)

define_class(line, Au)

define_enum(OPType)
define_enum(Exists)
define_enum(level)

define_class(path, string)
//define_class(file)
define_class(string,  Au)
define_class(const_string, string)
define_class(ipart,   Au)
define_class(command, string)

define_class(shape, Au)

define_class(token, string)

// this is defining tokens as array <token> .. the shape, is something the user gives (hard coded in Au)
define_class(tokens, array, token)

define_class(item, Au)

define_class(collective, Au)
define_class(list,    collective, Au)
define_class(array,   collective, Au, shape)
define_class(map,     collective, Au, Au)
define_class(ctx,            map)
define_class(subprocedure,    Au)

//define_class(Au_ts,           array, Au_t)
//define_class(array_map,        array, map)
//define_class(array_string,     array, string)

define_enum  (interface)
define_enum  (comparison)

#undef bind
