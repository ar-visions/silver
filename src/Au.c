#include <import>
#include <execinfo.h>

int seq;

const char* silver_listen;

#ifndef NDEBUG
// AU_SKIP_DROP=TypeName or AU_SKIP_DROP=TypeName.member_name
// comma-separated, parsed once at first use
#define AU_SKIP_DROP_MAX 64
static struct { char type[64]; char member[64]; } au_skip_drop[AU_SKIP_DROP_MAX];
static int  au_skip_drop_count = -1; // -1 = not yet parsed

static void au_skip_drop_init() {
    au_skip_drop_count = 0;
    const char *env = getenv("AU_SKIP_DROP");
    if (!env) return;
    char buf[1024];
    strncpy(buf, env, sizeof(buf) - 1);
    char *tok = strtok(buf, ",");
    while (tok && au_skip_drop_count < AU_SKIP_DROP_MAX) {
        char *dot = strchr(tok, '.');
        if (dot) {
            int tlen = dot - tok;
            strncpy(au_skip_drop[au_skip_drop_count].type,   tok,   tlen < 63 ? tlen : 63);
            au_skip_drop[au_skip_drop_count].type[tlen < 63 ? tlen : 63] = '\0';
            strncpy(au_skip_drop[au_skip_drop_count].member, dot+1, 63);
        } else {
            strncpy(au_skip_drop[au_skip_drop_count].type,   tok, 63);
            au_skip_drop[au_skip_drop_count].member[0] = '\0';
        }
        au_skip_drop_count++;
        tok = strtok(NULL, ",");
    }
}

static int au_skip_drop_check(const char *type, const char *member) {
    if (au_skip_drop_count < 0) au_skip_drop_init();
    for (int i = 0; i < au_skip_drop_count; i++) {
        if (strcmp(au_skip_drop[i].type, type) != 0) continue;
        if (au_skip_drop[i].member[0] == '\0') return 1; // whole type
        if (member && strcmp(au_skip_drop[i].member, member) == 0) return 1;
    }
    return 0;
}
#endif

//#undef realloc
#include <ffi.h>
#undef bool
#include <ports.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#undef bool

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define Au_t_module_ Au
Au_t_info        Au_Au_t_i;

#ifndef line
#define line(...)       new(line, __VA_ARGS__)
#endif

i64 epoch_millis();


int seq;

#undef hold
#undef drop

#ifndef NDEBUG
// ---- ref provenance (debug) -------------------------------------------------
// track which source:line holds each live ref, for ONE type at a time. off
// until au_track(name) is called. only the tracked type pays anything; every
// other object and all call sites are untouched (no signature/header change).
static Au_t au_track_type = NULL;
typedef struct { Au obj; void* bt[6]; int n; } au_prov_t;
static au_prov_t* au_prov = NULL;
static int au_prov_count = 0, au_prov_alloc = 0;

none au_track(Au_t type) { au_track_type = type; }

static bool au_prov_match(Au a) {
    return au_track_type && header(a)->au == (Au_f*)au_track_type;
}
static void au_prov_push(Au a) {
    if (au_prov_count == au_prov_alloc) {
        au_prov_alloc = au_prov_alloc ? au_prov_alloc * 2 : 256;
        au_prov = (au_prov_t*)realloc(au_prov, au_prov_alloc * sizeof(au_prov_t));
    }
    au_prov_t* e = &au_prov[au_prov_count++];
    e->obj = a;
    e->n   = backtrace(e->bt, 6);
}
static void au_prov_pop(Au a) {
    for (int i = au_prov_count - 1; i >= 0; i--)
        if (au_prov[i].obj == a) { au_prov[i] = au_prov[--au_prov_count]; return; }
}
// dump the live holders of `a` (their source:line via symbolized stacks).
none au_prov_dump(Au a) {
    static time_t last = 0;
    time_t now = time(NULL);
    if (now == last) return;   // at most one dump/sec — reports live while you drag, no wall
    last = now;
    Au f = header(a);
    printf("=== %i live holders of %s %p ===\n",
        (i32)f->refs, (f->au && f->au->ident) ? f->au->ident : "?", (void*)a);
    for (int i = 0; i < au_prov_count; i++) {
        if (au_prov[i].obj != a) continue;
        char** s = backtrace_symbols(au_prov[i].bt, au_prov[i].n);
        printf("  held by:\n");
        for (int j = 2; j < au_prov[i].n && j < 5; j++) printf("    %s\n", s[j]);
        free(s);
    }
}
#endif

Au Au_hold(Au a) {
    if (a) {
        Au f = header(a);
        if (f->managed == 0) return a; // refs of 0 is unmanaged memory (user managed)
        __atomic_fetch_add((i32*)__builtin_assume_aligned(&f->refs, 4), 1, __ATOMIC_SEQ_CST);
#ifndef NDEBUG
        if (au_prov_match(a)) au_prov_push(a);
#endif
    }
    return a;
}

none Au_drop(Au a);

Au   hold(Au a) { return Au_hold(a); }
none drop(Au a) { Au_drop(a); }

#define hold(a) (__typeof__(a))hold((Au)(a))
#define drop(a)                drop((Au)(a))

typedef struct _ffi_method_t {
    micro*          atypes;
    Au_t            rtype;
    void*           address;
    void*           ffi_cif;  /// ffi-calling info
    void*           ffi_args; /// ffi-data types for args
} ffi_method_t;

// with all this macro stuff, we can still safely see the type of our type, is here.  'that' is not to be in a macro, but it must exist.
// if the type is actually a Au_t, it will be this
_Pragma("pack(push, 1)")
#define Au_t_f_module_ Au
Au_t_f_info Au_Au_t_f_i;
_Pragma("pack(pop)")

Au_t au_arg(Au a) {
    verify(!a || isa(a), "unexpected isa result for Au object");
    if (isa(a) == typeid(Au_t_f) || a == (Au)isa(a)) return (Au_t)a;
    return ((Au_f*)a->au)->ft.cast_Au_t(a);
}


static void Au_module_initializer() {
    seq = 0;
    // this tells us that we 'need' a sequencer defined; 
    // this is because we want to log seq all the time -- extremely useful debug information to grab when initializing objects
    // if its defined as a static in a function, then it grabs the actual one.
    // this allows us to make all code compilable without having a seq overtly in each function.
}

cstr cstr_copy(cstr f) {
    if (!f) return null;
    int l = strlen(f);
    cstr res = (cstr)calloc(1, l + 1);
    memcpy(res, f, l);
    res[l] = 0;
    return res;
}

bool check(bool ch, string log) {
    if (!ch) {
        printf("%s\n", log->chars);
        return false;
    }
    return true;
}

Au_t au_arg_type(Au a) {
    if (!a) return null;
    Au_t au = au_arg(a);
    au = au->member_type == AU_MEMBER_VAR ? au->src : au;
    while (au && au->is_alias && !au->is_pointer && !au->is_funcptr)
        au = au->src;
    return au;
}

Au_t typeid_string(Au_t check) {
    Au_t expected = typeid(string);
    if (check != expected) {
        fprintf(stderr, "typeid_string mismatch: got %p (%s) expected %p (%s)\n",
            (void*)check, check ? check->ident : "(null)",
            (void*)expected, expected->ident);
    }
    verify(check == expected, "typeid_string: mismatch");
    return expected;
}

none validate_meta(Au_t actual, Au_t expected) {
    if (actual != expected) {
        fprintf(stderr, "meta mismatch: got %p (%s) expected %p (%s)\n",
            (void*)actual, actual ? actual->ident : "(null)",
            (void*)expected, expected ? expected->ident : "(null)");
        verify(false, "validate_meta: mismatch");
    }
}

Au_t Au_cast_Au_t(Au a) {
    return isa(a);
}

bool Au_is_au_type(Au a)       { return is_au_type(a); }
bool Au_is_imported_type(Au a) { return is_imported_type(a); }
bool Au_is_module(Au t)        { return is_module(t); }

//extern Au_info Au_Au_i;

//extern Au_info Au_Au_i;


#undef is_generic
#undef is_au_type
#undef is_imported_type
#undef is_integral
#undef is_void
#undef is_double
#undef is_float
#undef is_realistic
#undef is_class
#undef is_struct
#undef is_opaque
#undef is_system
#undef is_func
#undef is_var
#undef is_lambda
#undef is_func_ptr
#undef is_imethod
#undef is_rec
#undef is_module
#undef is_prim
#undef is_sign
#undef is_unsign
#undef is_ptr
#undef is_enum
#undef is_bool
#undef is_type

bool is_generic  (Au au) { return au && typeid(Au) == au_arg_type(au); }
bool is_integral (Au au) { return au && au_arg_type(au)->is_integral; }
bool is_void     (Au au) { return au && typeid(none) == au_arg_type(au); }
bool is_double   (Au au) { return au && typeid(f64) == au_arg_type(au); }
bool is_float    (Au au) { return au && typeid(f32) == au_arg_type(au); }
bool is_realistic(Au au) { return au && au_arg_type(au)->is_realistic; }
bool is_class(Au au) {
    Au_t t = au_arg_type(au);
    return t && t != typeid(Au_t) && t->is_class;
}
bool is_struct(Au au) {
    Au_t t = au_arg_type(au);
    if (!t || t->is_pointer) return false;
    return t->is_struct;
}
bool is_opaque(Au au) {
    Au_t t = au_arg(au);
    return t && t->is_struct && t->members.count == 0;
}
bool is_system(Au au) {
    Au_t t = au_arg(au);
    if (!t) return false;
    if (t->is_system) return true;
    t = au_arg_type((Au)t);
    return t && t->is_system;
}
bool is_func(Au au) {
    Au_t t = au_arg_type(au);
    return t && (t->member_type == AU_MEMBER_FUNC     ||
                 t->member_type == AU_MEMBER_CAST      ||
                 t->member_type == AU_MEMBER_GETTER    ||
                 t->member_type == AU_MEMBER_SETTER    ||
                 t->member_type == AU_MEMBER_OPERATOR  ||
                 t->member_type == AU_MEMBER_CONSTRUCT) && (t->ident || t->alt);
}
bool is_var(Au au) {
    Au_t t = au_arg(au);
    return t && (t->member_type == AU_MEMBER_VAR) && (t->ident || t->alt);
}
bool is_lambda(Au au) {
    Au_t t = au_arg_type(au);
    return t && is_func(au) && t->is_lambda;
}
bool is_func_ptr(Au au) {
    Au_t t = au_arg_type(au);
    return t && t->member_type == AU_MEMBER_TYPE && t->is_funcptr;
}
bool is_imethod(Au au) {
    Au_t t = au_arg(au);
    return t && t->member_type == AU_MEMBER_FUNC && t->is_imethod;
}
Au_t is_rec(Au au) {
    if (!au) return null;
    Au_t t = au_arg_type(au);
    if (!t || t == typeid(ARef) || is_func(au)) return null;
    if (t->src && t->src->is_class) return t->src;
    return (t->is_class || t->is_struct) ? t : null;
}
bool is_prim   (Au au) { Au_t t = au_arg_type(au); return t && t->is_primitive; }
bool is_sign   (Au au) { Au_t t = au_arg_type(au); return t && t->is_signed; }
bool is_unsign (Au au) { Au_t t = au_arg_type(au); return t && t->is_unsigned; }
bool is_ptr(Au au) {
    Au_t a = au_arg(au);
    if (!a) return false;
    if (a->is_explicit_ref) return true;
    Au_t walk = a;
    while (walk) {
        if (walk->is_pointer || walk->is_class) return true;
        if (!walk->src || walk == walk->src) break;
        walk = walk->src;
    }
    return false;
}
bool is_enum(Au au) { Au_t t = au_arg_type(au); return t && t->is_enum; }
bool is_bool(Au au) { return typeid(bool) == au_arg_type(au); }
bool is_type(Au au) { Au_t t = au_arg_type(au); return t && t->member_type == AU_MEMBER_TYPE; }
bool is_module(Au au) {
    Au_t a = au_arg(au);
    return a && a->member_type == AU_MEMBER_MODULE;
}
bool is_au_type(Au au) {
    Au_t a = au_arg(au);
    if (a->ident && strlen(a->ident) && a->member_type != AU_MEMBER_TYPE)
        return false;
    return a->module && a->module->is_au;
}
bool is_imported_type(Au au) {
    Au_t a = au_arg(au);
    return a->module->is_imported;
}

bool Au_is_generic  (Au t) { return is_generic  (t); }
bool Au_is_integral (Au t) { return is_integral (t); }
bool Au_is_void     (Au t) { return is_void     (t); }
bool Au_is_double   (Au t) { return is_double   (t); }
bool Au_is_float    (Au t) { return is_float    (t); }
bool Au_is_realistic(Au t) { return is_realistic(t); }
bool Au_is_class    (Au t) { return is_class    (t); }
bool Au_is_struct   (Au t) { return is_struct   (t); }
bool Au_is_opaque   (Au t) { return is_opaque   (t); }
bool Au_is_system   (Au t) { return is_system   (t); }
bool Au_is_func     (Au t) { return is_func     (t); }
bool Au_is_var      (Au t) { return is_var      (t); }
bool Au_is_lambda   (Au t) { return is_lambda   (t); }
bool Au_is_func_ptr (Au t) { return is_func_ptr (t); }
bool Au_is_imethod  (Au t) { return is_imethod  (t); }
Au_t Au_is_rec      (Au t) { return is_rec      (t); }
bool Au_is_prim     (Au t) { return is_prim     (t); }
bool Au_is_sign     (Au t) { return is_sign     (t); }
bool Au_is_unsign   (Au t) { return is_unsign   (t); }
bool Au_is_ptr      (Au t) { return is_ptr      (t); }
bool Au_is_enum     (Au t) { return is_enum     (t); }
bool Au_is_bool     (Au t) { return is_bool     (t); }
bool Au_is_type     (Au t) { return is_type     (t); }


shape shape_with_array(shape a, array dims) {
    num count = len(dims);
    a->data = (i64*)calloc(sizeof(i64), len(dims) + 1);
    each (dims, Au, e) {
        i64* i = (i64*)Au_instance_of(e, typeid(i64));
        a->data[a->count++] = *i;
    }
    return a;
}

shape shape_operator__mul(shape a, i64 n) {
    shape res = shape_from(a->count, a->data);
    res->data[a->count - 1] *= n;
    return res;
}

shape shape_operator__lmul(shape a, i64 n) {
    shape res = shape_from(a->count, a->data);
    res->data[0] *= n;
    return res;
}

shape shape_operator__div(shape a, i64 n) {
    i64 reduce = a->data[a->count - 1];
    verify(reduce % n == 0, "shape not right-divisible by %i", n);
    shape res = shape_from(a->count, a->data);
    res->data[a->count - 1] = reduce / n;
    return res;
}

shape shape_operator__ldiv(shape a, i64 n) {
    i64 reduce = a->data[0];
    verify(reduce % n == 0, "shape not left-divisible by %i", n);
    shape res = shape_from(a->count, a->data);
    res->data[0] = reduce / n;
    return res;
}

shape shape_operator__left(shape a, i64 n) {
    shape res = shape_from(a->count + n, null);
    memcpy(res->data, a->data, sizeof(i64) * a->count);
    for (int i = 0; i < n; i++)
        res->data[a->count + i] = 1;
    return res;
}

shape shape_operator__right(shape a, i64 n) {
    verify((a->count - n) >= 1, "cannot reduce shape");
    return new(shape, count, a->count - n, data, a->data, is_global, false);
}

shape shape_operator__lright(shape a, i64 n) {
    shape res = shape_from(a->count + n, null);
    for (int i = 0; i < n; i++)
        res->data[i] = 1;
    memcpy(res->data + n, a->data, sizeof(i64) * a->count);
    return res;
}

shape shape_operator__lleft(shape a, i64 n) {
    verify((a->count - n) >= 1, "cannot reduce shape from left");
    return new(shape, count, a->count - n, data, a->data + n, is_global, false);
}

string shape_cast_string(shape a) {
    string r = string(alloc, 32);
    for (int i = 0; i < a->count; i++) {
        if (r->count)
            append(r, "x");
        concat(r, f(string, "%lli", a->data[i]));
    }
    return r;
}


i64 shape_total(shape a) {
    i64* data = a->data;
    i64 total = 1;
    for (int i = 0; i < a->count; i++)
        total *= data[i];
    return total;
}

i64 shape_getter_i64(shape a, i64 i) {
    return a->data[i];
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

// set meta type pair on Au_t: a = primary type (element/key), b = optional (value type, shape, etc.)
none set_meta(Au_t type, Au_t a, Au b) {
    type->meta.a = a;
    type->meta.b = b;
}

none set_meta_array(Au_t type, int count, ...) {
    va_list args;
    va_start(args, count);
    type->meta.a = (count > 0) ? va_arg(args, Au_t) : null;
    type->meta.b = (count > 1) ? (Au)va_arg(args, Au_t) : null;
    va_end(args);
}

// for function model, we have an arg node with a name
none set_args_array(Au_t type, int count, ...) {
    type->args.alloc  = count;
    type->args.count  = count;
    type->args.origin = calloc(count, sizeof(Au));

    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Au_t au     = va_arg(args, Au_t);
        Au_t au_arg = def(type, null, AU_MEMBER_VAR, 0);
        au_arg->src = au;
        type->args.origin[i] = (Au)au_arg;
    }
}

none array_init(array a) {
    if (a->alloc)
        array_alloc_sz(a, a->alloc);
    a->assorted = true;
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

none array_push_vdata(array a, Au data, i64 count, Au_t data_type) {
    Au_t   t = data_type ? data_type : isa(a)->meta.a;
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
    if (i == 0) return t->meta.a;
    if (i == 1) return (Au_t)t->meta.b;
    return null;
}

Au collective_push(collective a, Au b) {
    fault("implement push method on %o", isa(a));
    return null;
}

Au array_push(array a, Au b) {
    if (!a->origin || a->alloc == a->count) {
        array_expand(a);
    }
    // unmanaged arrays hold raw non-Au pointers (cstr from strdup, OS handles,
    // etc). Their elements have no Au header — skip isa(b)/header reads and
    // the hold; just store the pointer.
    if (a->unmanaged) {
        a->origin[a->count++] = b;
        return b;
    }
    Au_t t = isa(a);
    Au_t vtype = isa(b);
    Au info = (Au)head(a);
    verify(!a->last_type || a->last_type == vtype || a->assorted,
        "unassorted array received differing type: %s, previous: %s (%s:%i)",
        vtype->ident, a->last_type->ident, info->source, info->line);
    a->last_type = vtype;

    if (Au_is_meta((Au)a) && Au_meta_index((Au)a, 0) != typeid(Au) &&
        !Au_is_meta_compatible((Au)a, (Au)b)) {
        Au_t ma = Au_meta_index((Au)a, 0);
        fault("not meta compatible: pushing %s into %s of %s (%s:%i)",
            isa(b)->ident, t->ident, ma ? ma->ident : "(null)",
            info->source, info->line);
    }

    a->origin[a->count++] = Au_hold(b);
    return b;
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
            Au info = head(a);
            Au_drop(a->origin[i]);
            a->origin[i] = null;
        }
    a->count = 0;
}

none array_concat(array a, array b) {
    each(b, Au, e) array_push(a, e);
}

Au array_getter_num(array a, num i) {
    if (i < 0 || i >= a->count)
        return null;
    Au r = a->origin[i];
    Au_t type = head(a)->meta_a;
    //if (type == typeid(i32))
    //    printf("array_getter_num[i32]: i=%lld result=%p\n", (long long)i, (void*)r);
    return r;
}

static Au* array_indexer(array a, Au ai) {
    num offset = 0;
    i32* n32 = null;
    num*    n = (num*)instance_of(ai, typeid(num));
    if (!n) n = (i64*)instance_of(ai, typeid(i64));
    if (!n) n32 = (i32*)instance_of(ai, typeid(i32));
    if  (n) {
        offset = *n;
    } else if (n32) {
        offset = *n32;
    } else {
        Au info = header(ai);
        shape i = instanceof(ai, shape);
        //verify(i, "expected shape");

        verify(!i || a->data_shape, "array has no shape");
        verify(!i || (a->data_shape && i->count <= a->data_shape->count),
            "shape index rank exceeds array rank");

        u64 stride = 1;

        // compute strides from the back (row-major)
        for (i32 d = a->data_shape->count - 1; d >= 0; d--) {
            u64 dim_size = a->data_shape->data[d];
            u64 idx = (d < i->count) ? i->data[d] : 0;

            verify(idx < dim_size,
                "index %llu out of bounds for dimension %i (size %llu)",
                idx, d, dim_size);

            offset += idx * stride;
            stride *= dim_size;

            if (d == 0) break; // avoid underflow
        }
    }
    if (offset < 0 || offset >= a->count)
        return null;

    return &a->origin[offset];
}

Au array_getter_Au(array a, Au ai) {
    Au* n = array_indexer(a, ai);
    return n ? *n : null;
}


/// assign to an Au* slot with reference management and compound ops
/// for compound ops, the slot value is treated as numeric (i64/f64)
static void Au_assign(Au* slot, Au value, i32 op, bool unmanaged) {
    if (op == OPType__assign) {
        if (*slot != value) {
            if (!unmanaged) drop(*slot);
            if (!unmanaged) hold(value);
            *slot = value;
        }
    } else {
        fault("not implemented");
    }
}

none array_setter(array a, Au key, Au value, i32 op) {
    Au* val = array_indexer(a, key);
    verify(val, "array setter: index out of bounds");
    Au_t type = head(a)->meta_a;
    Au_assign(val, value, op, a->unmanaged);
}

none array_remove(array a, num b) {
    if (b < 0 || b >= a->count) return;
    Au prev = a->origin[b];
    // shift the tail down by one (use i, not b)
    for (num i = b; i < a->count - 1; i++)
        a->origin[i] = a->origin[i + 1];
    a->origin[--a->count] = null;
    // drop the removed element exactly once
    if (!a->unmanaged)
        Au_drop(prev);
}

none array_remove_weak(array a, num b) {
    if (b < 0 || b >= a->count) return;
    for (num i = b; i < a->count - 1; i++)
        a->origin[i] = a->origin[i + 1];
    a->origin[--a->count] = null;
}

none array_operator__assign_add(array a, Au b) {
    array_push(a, b);
}

none array_operator__assign_sub(array a, num b) {
    array_remove(a, b);
}

Au array_first_element(array a) {
    return a && a->count ? a->origin[0] : null;
}

Au array_last_element(array a) {
    return a && a->count ? a->origin[a->count - 1] : null;
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
    array a = Au_allocate(array, alloc, 256);
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

        if (!fmix) fmix = find_member(at, "mix", AU_MEMBER_FUNC, 0, false);
        verify(fmix, "implement mix method for type %s", at->ident);
        Au e = ((mix_fn)fmix->value)(aa, bb, f);
        push(res, e);
    }
    return res;
}

Au array_get(array a, num i) {
    if (!a || i < 0 || i >= a->count)
        return null;
    if (i < 0 || i >= a->count)
        fault("out of bounds: %i, len = %i", i, a->count);
    return a->origin[i];
}

num array_count(array a) {
    return a->count;
}

sz string_len(string a) {
    return a->count;
}

i64 string_integer_value(string a) {
    return strtoll(a->chars, null, 10);
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
    // Au_t (type descriptors) have no Au header — isa(au_t) is null.
    // Compare by pointer for those; use content compare for real Au objects.
    bool is_au_t = b && isa(b) == typeid(Au_t_f);
    if (a->unmanaged || is_au_t) {
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
 
#pragma pack(pop)

static Au_combine* member_pool;
static int         n_members;

static Au_t   au_module;
static Au_t   module;
static micro  modules;
static array  scope;
static bool   started = false;
static pthread_rwlock_t modules_lock = PTHREAD_RWLOCK_INITIALIZER;

typedef struct _AuSpace {
    micro            modules;
    pthread_rwlock_t lock;
    struct _AuSpace* parent;
    void*            parent_owner;
} AuSpace_t, *AuSpace;

__thread AuSpace au_current_space = null;
__thread void*   au_space_owner   = null;

u64 au_hash_ident(symbol s) {
    // 3 more lines to not have a string length prior is best
    //return fnv1a_hash(a->chars, a->count, OFFSET_BASIS);
    if (!s) return 0;
    u64 h = 5381;
    for (const char* p = s; *p; p++)
        h = ((h << 5) + h) ^ *p;
    return h;
}

Au_t find_member(Au_t mdl, symbol f, int member_type, u64 traits, bool poly) {
    if (!mdl) return null;
    u64 fhash = f ? au_hash_ident(f) : 0;
    do {
        // fast path: hash map lookup, then verify filters
        if (f && fhash && mdl->member_map) {
            store s = (store)mdl->member_map;
            size_t idx = ((size_t)fhash >> 3) % s->hsize;
            for (item it = s->hlist[idx]; it; it = it->next) {
                if (it->key == (Au)(uintptr_t)fhash) {
                    Au_t au = (Au_t)it->value;
                    if (au && au->ident && !au->is_expanding && strcmp(au->ident, f) == 0) {
                        if ((!member_type || au->member_type == member_type) &&
                            (!traits || (au->traits & traits) == traits))
                            return au;
                    }
                }
            }
        }
        {
            // slow path: linear scan with filters
            for (int i = 0; i < mdl->members.count; i++) {
                Au_t au = (Au_t)mdl->members.origin[i];
                if (au->is_expanding) continue;
                if (!member_type || au->member_type == member_type) {
                    if (!traits || (au->traits & traits) == traits) {
                        if (!f || (au->ident && strcmp(au->ident, f) == 0))
                            return au;
                    }
                }
            }
        }
        if (!poly || mdl->context == mdl) break;
        mdl = mdl->context;
    } while (mdl);
    return null;
}

Au_t find_context(array lex, int member_type, int traits) {
    for (int i = len(lex) - 1; i >= 0; i--) {
        Au_t au = (Au_t)lex->origin[i];
        if (!member_type || au->member_type == member_type) {
            if (!traits || (au->traits & traits) == traits)
                return au;
        }
    }
    return null;
}

Au_t lexical_traits(array lex, symbol f, u64 traits, int member_type);

Au_t lexical(array lex, symbol f) {
    return lexical_traits(lex, f, 0, 0);
}

Au_t lexical_traits(array lex, symbol f, u64 traits, int member_type) {

    bool top_set = false;
    bool top_Au  = false;
    for (int i = len(lex) - 1; i >= 0; i--) {
        Au_t au = (Au_t)lex->origin[i];
        if (!top_set) {
            top_set = true;
            top_Au  = au == typeid(Au);
        }
        while (au) {
            if (((au != typeid(Au) || top_Au) && au->member_type == AU_MEMBER_TYPE) || is_func((Au)au))
                for (int ii = 0; ii < au->args.count; ii++) {
                    Au_t m = (Au_t)au->args.origin[ii];
                    if (m->ident && strcmp(m->ident, f) == 0 && (!traits || (m->traits & traits)) && (!member_type || m->member_type == member_type))
                        return m;
                }
            for (int ii = 0; ii < au->members.count; ii++) {
                Au_t m = (Au_t)au->members.origin[ii];
                if (au->is_struct || au->is_class) {
                    if (((au != typeid(Au) || top_Au) && au->member_type == AU_MEMBER_TYPE) || is_func((Au)m)) {
                        if (m->ident && strcmp(m->ident, f) == 0 && (!traits || (m->traits & traits)) && (!member_type || m->member_type == member_type))
                            return m;
                    }
                } else {
                    if (m->ident && !m->is_expanding && strcmp(m->ident, f) == 0 && (!traits || (m->traits & traits)) && (!member_type || m->member_type == member_type)) {
                        // prefer functions over types when both exist with same name
                        if (m->member_type == AU_MEMBER_TYPE && !traits) {
                            Au_t func_match = null;
                            for (int jj = ii + 1; jj < au->members.count; jj++) {
                                Au_t mj = (Au_t)au->members.origin[jj];
                                if (mj->ident && strcmp(mj->ident, f) == 0 && is_func((Au)mj)) {
                                    func_match = mj;
                                    break;
                                }
                            }
                            if (func_match) return func_match;
                        }
                        return m;
                    }
                }
            }
            Au_t au_isa = isa(au);
            if (!is_class((Au)au)) break;
            if (au->context == au) break;
            au = au->context;
        }
    }

    return null;
}

static Au_t _push_arg(Au_t type, bool add_arg) {
    struct _Au_combine* cur = calloc(1, sizeof(struct _Au_combine));
    cur->info.refs = 0;
    cur->info.managed = 0;
    cur->info.au = (Au_f*)&Au_Au_t_f_i.type;
    
    Au_t au = &cur->type;
    au->member_type = AU_MEMBER_VAR;
    au->traits = AU_TRAIT_ALLOCATED;
    au->context = type;

    if (add_arg && type)
        micro_push(&type->args, (Au)au);
    
    return au;
}

Au_t def_prop(Au_t context, symbol ident, Au_t type, u64 traits, u32 offset, u32 abi_size, ARef value, Au_t meta_a, Au meta_b, i32 index) {
    Au_t prop = def(context, ident, AU_MEMBER_VAR, traits);
    verify(type, "def_prop: src/type is null for %s.%s", context->ident, ident);
    prop->type      = type;
    prop->offset    = offset;
    //prop->abi_size  = abi_size;
    prop->value     = (object)value;
    prop->meta.a    = meta_a;
    prop->meta.b    = meta_b && !instanceof(meta_b, Au_t) ? hold(meta_b) : meta_b;
    prop->af_index     = index; // AF-bit slot position (computed by the codegen)
    if (context && context->ident && ident &&
        strcmp(context->ident, "Option") == 0 && strcmp(ident, "selected") == 0)
        printf("[def_prop] Option.selected index=%d offset=%u\n", index, offset);
    return prop;
}

Au_t alloc_arg(Au_t context, symbol ident, Au_t arg) {
    Au_t var = _push_arg(context, false);
    var->src = arg;
    var->ident = cstr_copy((cstr)ident);
    return var;
}

Au_t def_test(Au_t context, ARef arg) {
    return null;
}

Au_t def_arg(Au_t context, symbol ident, Au_t arg, u64 traits) {
    Au_t var = _push_arg(context, true);
    var->src = arg;
    var->ident = cstr_copy((cstr)ident);
    var->ident_hash = au_hash_ident(ident);
    var->traits = traits;
    if (traits & AU_TRAIT_EXPLICIT_REF) var->is_explicit_ref = true;
    return var;
}

Au_t def_meta(Au_t context, symbol ident, Au_t arg) {
    Au_t var = _push_arg(context, true);
    var->src = arg;
    var->ident = cstr_copy((cstr)ident);
    var->ident_hash = au_hash_ident(ident);
    return var;
}

Au_t def_func(Au_t type, symbol ident, Au_t rtype, u32 member_type,
        u32 access_type, u32 operator_type, u64 traits, ARef value, symbol alt, i32 index,
        Au_t meta_a, Au meta_b) {
    Au_t func = def(type, ident, AU_MEMBER_TYPE, traits);
    func->rtype         = rtype;
    func->member_type   = member_type;
    func->operator_type = operator_type;
    func->access_type   = interface_public;
    func->value         = (object)value;
    func->alt           = alt ? cstr_copy((cstr)alt) : null;
    func->member_index         = index;
    func->meta.a        = meta_a;
    func->meta.b        = meta_b;
    return func;
}

Au_t def(Au_t type, symbol ident, u32 member_type, u64 traits) {
    static int seq; seq++;

    struct _Au_combine* cur = calloc(1, sizeof(struct _Au_combine));
    cur->info.refs = 0;
    cur->info.managed = 0;
    cur->info.au = (Au_f*)&Au_Au_t_f_i.type;

    Au_t au = &cur->type;

    au->ident = ident ? (cstr)cstr_copy((cstr)ident) : (cstr)null;
    au->ident_hash = au_hash_ident(ident);
    au->traits = traits | AU_TRAIT_ALLOCATED; // erasing modules can consist of freeing only pool origined data (or we check performance against a simple approach and fallback to non complicated)
    au->member_type = member_type;

    if (type && type->member_type == AU_MEMBER_MODULE) {
        if (au->module && au->module != type) {
            fprintf(stderr, "MODULE OVERWRITE [def_member]: %s (%p) module %p -> %p\n", au->ident, au, au->module, type);
            exit(1);
        }
        au->module = type;
    }

    if (type) {
        Au_t new_member = (Au_t)micro_push(&type->members, (Au)&cur->type);
        new_member->context = type;

        // create member_map on first member addition (raw alloc — Au may not be ready)
        if (!type->member_map) {
            int hsz = 0;
            if (type->member_type == AU_MEMBER_MODULE || type->member_type == AU_MEMBER_NAMESPACE)
                hsz = 1024;
            else if (type->is_class || type->is_struct)
                hsz = 256;
            if (hsz) {
                store s = calloc(1, sizeof(struct _store) + sizeof(struct _Au));
                s->hsize = hsz;
                s->hlist = (item*)calloc(hsz, sizeof(item));
                type->member_map = (void*)s;
            }
        }

        // insert into member_map keyed by ident_hash (raw — no Au object model)
        if (type->member_map && ident) {
            store s = (store)type->member_map;
            size_t idx = ((size_t)new_member->ident_hash >> 3) % s->hsize;
            // check for existing
            for (item i = s->hlist[idx]; i; i = i->next) {
                if (i->key == (Au)(uintptr_t)new_member->ident_hash) {
                    i->value = (Au)new_member;
                    goto member_map_done;
                }
            }
            // allocate raw item
            item ni = calloc(1, sizeof(struct _Au) + sizeof(struct _item));
            ni = (item)(((struct _Au*)ni) + 1);
            ni->key   = (Au)(uintptr_t)new_member->ident_hash;
            ni->value = (Au)new_member;
            ni->next  = s->hlist[idx];
            if (s->hlist[idx]) s->hlist[idx]->prev = ni;
            s->hlist[idx] = ni;
            s->count++;
            member_map_done:;
        }

        return new_member;
    }
    return (Au_t)&cur->type;
}

static none dealloc_iter(Au_t type) {
    micro_clear(&type->members);
    micro_clear(&type->args);
    free(type->ident);
    Au_drop((Au)type);
}

// intelligently drops the inlay array elements and allocated member data
none dealloc_type(Au_t type) {
    Au info = head(type);
    if (info->managed && info->refs == 1) {
        dealloc_iter(type);
    }
    Au_drop((Au)type);
}

Au lambda_call(lambda a, Au args) {
    return a->vfn(args, a->context);
}

bool lambda_cast_bool(lambda a) {
    return a != null;
}

lambda lambda_instance(Au_t au, callback fn, Au target, Au context) {
    lambda a = (lambda)alloc_new(typeid(lambda), 0, null, null, null, __FILE__, __LINE__, 0);
    a->au_t    = au;
    a->vfn     = fn;
    a->target  = target;
    a->context = hold(context);
    return a;
}

Au_t emplace_type(Au_t type, Au_t context, Au_t src, Au_t module, symbol ident, i32 member_type, u64 traits, u64 typesize, u64 isize, i32 icount) {
    type->member_type       = member_type;
    memset(&type->members, 0, sizeof(micro));
    memset(&type->args,    0, sizeof(micro));
    type->context           = context;
    type->src               = src;
    if (type->module && type->module != module) {
        fprintf(stderr, "MODULE OVERWRITE [def]: %s (%p) module %p -> %p\n", type->ident, type, type->module, module);
        exit(1);
    }
    type->module            = module;
    type->ident             = cstr_copy((cstr)ident);
    type->traits            = traits;
    if (!type->typesize)
        type->typesize      = typesize;
    type->table_size        = context ? context->table_size : 0; // [in module-init] increment with additional functions that do not overload
    type->isize             = isize;
    type->icount            = icount;
    if (context)
        memcpy(&type->ft, &context->ft, context->table_size);

    head(type)->au = (Au_f*)typeid(Au_t_f);
    
    if (member_type == AU_MEMBER_MODULE) {
        micro_push((micro_*)&modules, (Au)type); // we should error if we ever find a duplicate here
    }
    return type;
}

Au_t def_type(Au_t type, symbol ident, u64 traits) {
    return def(type, ident, AU_MEMBER_TYPE, traits);
}

Au_t def_class(Au_t type, symbol ident) {
    return def(type, ident, AU_MEMBER_TYPE, AU_TRAIT_CLASS);
}

Au_t def_struct(Au_t type, symbol ident) {
    return def(type, ident, AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
}

Au_t def_func_ptr(Au_t type, symbol ident) {
    return def(type, ident, AU_MEMBER_TYPE, AU_TRAIT_FUNCPTR);
}

Au_t def_pointer(Au_t context, Au_t ref, symbol ident) {
    if (!ref->ptr) {
        ref->ptr = def(context, ident, AU_MEMBER_TYPE, AU_TRAIT_POINTER);
        ref->ptr->src = (Au_t)ref;
    }
    return ref->ptr;
}

Au_t def_enum(Au_t context, symbol ident, u64 traits) {
    return def(context, ident, AU_MEMBER_TYPE, AU_TRAIT_ENUM);
}

Au_t def_enum_value(Au_t context, symbol ident, Au value) {
    Au_t res = def(context, ident, AU_MEMBER_ENUMV, 0);
    res->src   = context;
    res->value = (object)value;
    return res;
}

Au_t def_member(Au_t context, symbol ident, Au_t type_mem, u32 member_type, u64 traits) {
    Au_t au = def(context, ident, member_type, traits);
    au->type = type_mem;
    return au;
}

cstr copy_cstr(cstr input) {
    sz len = strlen(input);
    cstr res = calloc(len + 1, 1);
    memcpy(res, input, len);
    return res;
}

Au header(Au a) {
    return (((struct _Au*)a) - 1);
}

// attach a companion object onto a's header (meta_b), owning it. this is the
// per-object side-channel transitions ride on: the animated element carries
// its transition state here, freed with the element in Au_dealloc.
none Au_attach(Au a, Au other) {
    if (!a) return;
    Au hd = header(a);
    if (hd->meta_b == other) return;
    if (other) hold(other);
    if (hd->meta_b) drop(hd->meta_b);
    hd->meta_b = other;
}

Au Au_attached(Au a) {
    return a ? header(a)->meta_b : null;
}

none def_init(func f) {
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
    call_last[call_last_count++] = (__typeof__(call_last[0]))f;
}

Au_t module_lookup(symbol name) {
    pthread_rwlock_rdlock(&modules_lock);
    for (int i = 0; i < modules.count; i++) {
        Au_t m = (Au_t)modules.origin[i];
        if (m && strcmp(m->ident, name) == 0) {
            pthread_rwlock_unlock(&modules_lock);
            return m;
        }
    }
    pthread_rwlock_unlock(&modules_lock);
    return def_module(name);
}

static void* au_live_window   = null;
static void* au_live_vk       = null;
static void* au_live_surface  = null;
static void* au_live_swapchain = null;

static Au au_compiler = null;
Au method_vargs(Au a, Au_t mem, int n_args, ...);

void au_set_compiler(Au inst)       { au_compiler = inst; }
bool au_compile_ready()             { return au_compiler != null; }
void au_compile_invoke(const char* name) {
    if (!au_compiler) { printf("au_compile_invoke: no compiler set\n"); return; }
    Au_t type = isa(au_compiler);
    Au_t m = find_member(type, "compile", AU_MEMBER_FUNC, 0, true);
    printf("au_compile_invoke: type=%s compile=%p\n", type ? type->ident : "?", m ? m->value : null);
    if (!m || !m->value) { printf("au_compile_invoke: compile method not found\n"); return; }
    typedef void (*compile_fn)(Au, Au);
    ((compile_fn)m->value)(au_compiler, (Au)string(name));
}

// live-reload DEFER mode: instead of auto-recompiling on a source change, the host stages
// the change, SIGNALS the app a reload is pending, and waits for the app to request it —
// so the app (orbiter) can surface a "reload ready" affordance and apply on the user's
// terms. host calls *_set_pending / *_take_apply; app calls *_get_pending / *_request_apply.
static int au_live_defer_flag   = 0;   // app -> host: stage changes, don't auto-reload
static int au_live_pending_flag = 0;   // host -> app: a rebuilt module is staged & ready
static int au_live_apply_flag   = 0;   // app -> host: please recompile + hot-swap now
static int au_live_reload_flag  = 1;   // app -> host: watch + reload at all (orbiter: off)
void au_live_set_defer(int v)   { au_live_defer_flag = v ? 1 : 0; }   // the APP turns defer on
int  au_live_get_defer()        { return au_live_defer_flag; }        // the host polls it
void au_live_set_reload(int v)  { au_live_reload_flag = v ? 1 : 0; }  // the APP turns reload off
int  au_live_get_reload()       { return au_live_reload_flag; }       // the host polls it
void au_live_set_pending(int v) { au_live_pending_flag = v ? 1 : 0; }
int  au_live_get_pending()      { return au_live_pending_flag; }
void au_live_request_apply()    { au_live_apply_flag = 1; }
int  au_live_take_apply()       { int v = au_live_apply_flag; au_live_apply_flag = 0; return v; }

handle live_window_get() { return au_live_window; }
void   live_window_set(handle w) { au_live_window = w; }

// own-stdout tee: fd 1 becomes a pipe drained each frame
static int au_tee_read = -1;
static int au_tee_real = -1;

// exit can outrun the frame drain: push whatever remains through
static void au_tee_flush() {
    if (au_tee_read < 0 || au_tee_real < 0) return;
    fflush(stdout);
    char buf[4096];
    int  n;
    while ((n = (int)read(au_tee_read, buf, sizeof(buf))) > 0) {
        int off = 0;
        while (off < n) {
            int w = (int)write(au_tee_real, buf + off, (size_t)(n - off));
            if (w <= 0) break;
            off += w;
        }
    }
}

i32 stdout_tee() {
    if (au_tee_read >= 0) return au_tee_read;
    int p[2];
    if (pipe(p)) return -1;
    au_tee_real = dup(1);
    dup2(p[1], 1);
    close(p[1]);
    au_tee_read = p[0];
    fcntl(au_tee_read, F_SETFL, O_NONBLOCK);
    // pipes are fully buffered by default; keep lines timely
    setvbuf(stdout, NULL, _IOLBF, 0);
    atexit(au_tee_flush);
    return au_tee_read;
}

// drain the tee and return the next complete line (null = none);
// every byte read is passed through to the real terminal fd
string stdout_line() {
    static char acc[8192];
    static int  acc_n = 0;
    if (au_tee_read < 0) return null;
    char buf[4096];
    int  n;
    while ((n = (int)read(au_tee_read, buf, sizeof(buf))) > 0) {
        if (au_tee_real >= 0) {
            int off = 0;
            while (off < n) {
                int w = (int)write(au_tee_real, buf + off, (size_t)(n - off));
                if (w <= 0) break;
                off += w;
            }
        }
        int keep = n;
        if (acc_n + keep > (int)sizeof(acc)) keep = (int)sizeof(acc) - acc_n;
        if (keep > 0) {
            memcpy(acc + acc_n, buf, keep);
            acc_n += keep;
        }
    }
    for (int i = 0; i < acc_n; i++) {
        if (acc[i] == '\n') {
            acc[i] = 0;
            string s = string(acc);
            memmove(acc, acc + i + 1, acc_n - i - 1);
            acc_n -= i + 1;
            return s;
        }
    }
    return null;
}

handle live_vk_get() { return au_live_vk; }
void   live_vk_set(handle vk) {
    if (au_live_vk) drop(au_live_vk);
    au_live_vk = vk ? hold(vk) : null;
}

handle live_surface_get()  { return au_live_surface; }
void   live_surface_set(handle s) { au_live_surface = s; }

handle live_swapchain_get() { return au_live_swapchain; }
void   live_swapchain_set(handle s) { au_live_swapchain = s; }

void module_erase(Au_t module, symbol name) {
    if (!module && !name) return;
    micro* mods = au_current_space ? &au_current_space->modules : &modules;
    pthread_rwlock_t* lk = au_current_space ? &au_current_space->lock : &modules_lock;
    pthread_rwlock_wrlock(lk);
    for (int i = 0; i < mods->count; i++) {
        Au_t m = (Au_t)mods->origin[i];
        if (!m || (m && !m->ident)) continue;

        if (m && module == m || (name && m->ident && strcmp(m->ident, name) == 0)) {
            mods->origin[i] = null;
            m->members.count = 0;
            m->args.count = 0;
        }
    }
    pthread_rwlock_unlock(lk);
}

// erase all Silver-defined modules (not C-native ones) so module_init
// in the reloaded .so starts with a clean registry
void module_erase_silver(void) {
    pthread_rwlock_wrlock(&modules_lock);
    for (int i = 0; i < modules.count; i++) {
        Au_t m = (Au_t)modules.origin[i];
        if (!m || !m->ident) continue;
        if (m->is_au_native) continue;   // keep libAu.so system types
        modules.origin[i] = null;
        m->members.count = 0;
        m->args.count = 0;
    }
    pthread_rwlock_unlock(&modules_lock);
}

void au_space_begin(void* owner) {
    if (au_current_space && au_space_owner == owner) {
        // same owner re-entering (watch mode): reset the existing space
        free(au_current_space->modules.origin);
        memset(&au_current_space->modules, 0, sizeof(micro));
        return;
    }
    // new owner: push a new space, saving the current one
    AuSpace s = (AuSpace)calloc(1, sizeof(AuSpace_t));
    pthread_rwlock_init(&s->lock, null);
    s->parent       = au_current_space;
    s->parent_owner = au_space_owner;
    au_current_space = s;
    au_space_owner   = owner;
}

void au_space_end(void* owner) {
    if (au_space_owner == owner) {
        AuSpace s = au_current_space;
        au_current_space = s->parent;
        au_space_owner   = s->parent_owner;
        free(s->modules.origin);
        pthread_rwlock_destroy(&s->lock);
        free(s);
    }
}

void au_space_clear(void) {
    while (au_current_space) {
        AuSpace s = au_current_space;
        au_current_space = s->parent;
        free(s->modules.origin);
        pthread_rwlock_destroy(&s->lock);
        free(s);
    }
    au_space_owner = null;
}

// resident set size as a fraction of total physical RAM. reads /proc/self/statm
// (field 2 = resident pages) and sysconf for page size + physical page count.
// returns 0 when it can't be determined so callers treat that as "no guard".
float au_mem_fraction(void) {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0.0f;
    long size_pages = 0, rss_pages = 0;
    int n = fscanf(f, "%ld %ld", &size_pages, &rss_pages);
    fclose(f);
    if (n < 2 || rss_pages <= 0) return 0.0f;
    long page = sysconf(_SC_PAGESIZE);
    long phys = sysconf(_SC_PHYS_PAGES);
    if (page <= 0 || phys <= 0) return 0.0f;
    double rss_bytes  = (double)rss_pages * (double)page;
    double phys_bytes = (double)phys      * (double)page;
    return (float)(rss_bytes / phys_bytes);
}

// redirect this process's stdout (and stderr) into a pipe so the app can read
// its own printf/puts output. returns the READ end (non-blocking) for draining.
// au_stdout_orig() hands back a dup of the original stdout so callers can tee
// output back to the real terminal. key quirk handled: once fd 1 is a pipe (not
// a tty), glibc full-buffers stdout, so we force it unbuffered or nothing ever
// shows. write end is non-blocking too, so a flood drops bytes instead of
// deadlocking the single-threaded drain loop. returns -1 on failure.
//
// INSTRUMENTED: logs to /tmp/orbiter_cap.log and runs an in-process round-trip
// self-test so we can see whether the redirect actually took effect (the log fd
// is its own descriptor, unaffected by the dup2 of 1/2).
static int g_stdout_orig = -1;

int au_capture_stdout(void) {
    FILE* dbg = fopen("/tmp/orbiter_cap.log", "a");
    int fds[2];
    if (pipe(fds) != 0) {
        if (dbg) { fprintf(dbg, "au_capture_stdout: pipe() failed errno=%d\n", errno); fclose(dbg); }
        return -1;
    }
    int orig = dup(STDOUT_FILENO);
    int r1   = dup2(fds[1], STDOUT_FILENO);
    int r2   = dup2(fds[1], STDERR_FILENO);
    close(fds[1]);                                   // fd 1/2 now hold the write end
    fcntl(fds[0],        F_SETFL, O_NONBLOCK);       // drain never blocks when empty
    fcntl(STDOUT_FILENO, F_SETFL, O_NONBLOCK);       // printf drops instead of hanging when full
    setvbuf(stdout, NULL, _IONBF, 0);                // unbuffered: output reaches the pipe immediately
    setvbuf(stderr, NULL, _IONBF, 0);
    g_stdout_orig = orig;

    // round-trip self-test: write straight to the (now redirected) fd 1 and read
    // it back off the pipe. if this lands, the redirect works in-process.
    static const char msg[] = "CAPTURE_SELFTEST\n";
    ssize_t w = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    usleep(2000);
    char tb[64];
    ssize_t rd = read(fds[0], tb, sizeof(tb) - 1);
    if (rd > 0) tb[rd] = 0; else tb[0] = 0;
    if (dbg) {
        fprintf(dbg, "au_capture_stdout: orig=%d rfd=%d dup2(out)=%d dup2(err)=%d selftest_write=%zd read=%zd got='%s'\n",
            orig, fds[0], r1, r2, w, rd, tb);
        fclose(dbg);
    }
    return fds[0];
}

int au_stdout_orig(void) { return g_stdout_orig; }

// silver-host stashes the process argv here (dlsym'd from libAu before it calls
// silver_live_init); au_apply_args then parses it into the freshly-created app
// instance, so an app receives its own command-line flags through its schema.
static cstrs g_main_argv = NULL;
static int   g_argv_stop = 0;
void au_main_args(int argc, cstrs argv) { (void)argc; g_main_argv = argv; }
void au_apply_args(Au a) { if (a && g_main_argv) Au_with_cstrs((Au)a, g_main_argv); }
int   au_argv_stop(void) { return g_argv_stop; }
cstrs au_argv(void)      { return g_main_argv; }

// live-reload rebuild flag, lives in libAu so the host and the dlopen'd app both
// see it. silver-host sets it true around a blocking recompile (pumping one app
// frame so the loading overlay paints, then it stays frozen during the compile);
// trinity reads it each on_render to append the full-screen Avatar overlay.
static bool g_rebuilding = false;
void au_set_rebuilding(bool b) { g_rebuilding = b; }
bool au_rebuilding(void) { return g_rebuilding; }

Au_t global() {
    Au_t au_module_t = isa(au_module);
    return au_module;
}

Au_t def_module(symbol next_module) {
    struct _Au_combine* combine = calloc(1, sizeof(struct _Au_combine));
    Au_t m = &combine->type;
    m->member_type = AU_MEMBER_MODULE;
    m->traits      = AU_TRAIT_ALLOCATED | AU_TRAIT_IS_AU;
    m->ident       = cstr_copy((cstr)next_module);
    combine->info.au  = (Au_f*)typeid(Au_t_f);

    // first global module is registered as main (never overridden by space modules)
    if (!au_module) {
        au_module = m;
        module    = m;
    }

    if (au_current_space) {
        pthread_rwlock_wrlock(&au_current_space->lock);
        for (int i = 0; i < au_current_space->modules.count; i++)
            if (!au_current_space->modules.origin[i]) {
                au_current_space->modules.origin[i] = (Au_t)m;
                pthread_rwlock_unlock(&au_current_space->lock);
                return m;
            }
        micro_push((micro_*)&au_current_space->modules, (Au)m);
        pthread_rwlock_unlock(&au_current_space->lock);
        return m;
    }

    pthread_rwlock_wrlock(&modules_lock);
    for (int i = 0; i < modules.count; i++)
        if (!modules.origin[i]) {
            modules.origin[i] = (Au_t)m;
            pthread_rwlock_unlock(&modules_lock);
            return m;
        }
    micro_push((micro_*)&modules, (Au)m);
    pthread_rwlock_unlock(&modules_lock);
    return m;
}

none collective_init(collective a) {
}

ffi_method_t* method_with_address(handle address, Au_t rtype, micro* atypes, Au_t method_owner);

#define members(MDL, VAR) \
    for (int __i = 0; __i < (MDL)->members.count; __i++) \
        for (Au_t VAR = (Au_t)(MDL)->members.origin[__i]; VAR; VAR = NULL)

none push_type(Au_t type, Au_t to_mod) {
    // ensure ident_hash is set for all types (static types from declare_class etc.)
    if (type->ident && !type->ident_hash)
        type->ident_hash = au_hash_ident(type->ident);

    if (!Au_Au_t_i.type.ident) {
        module = module_lookup("Au");
        Au_Au_t_i.type.ident  = "Au_t";
        Au_Au_t_i.type.ident_hash = au_hash_ident("Au_t");
        Au_Au_t_i.type.src    = typeid(Au);
        Au_Au_t_i.type.traits = AU_TRAIT_IS_AU;
        Au_Au_t_i.type.module = module;
        push_type((Au_t)&Au_Au_t_i.type, null);
    }

    if (type == typeid(Au)) {
        Au_Au_t_f_i.info.au = (Au_f*)&Au_Au_t_f_i.type;
        Au_Au_t_f_i.type.ident = "Au_t";
        Au_Au_t_f_i.type.module = module;
        Au_Au_t_f_i.type.traits = AU_TRAIT_IS_AU | AU_TRAIT_SCHEMA;
        int Au_ft_size   = sizeof(((Au_f*)typeid(Au))->ft);
        int Au_t_ft_size = sizeof(((Au_t_f*)typeid(Au_t_f))->ft);
        verify(Au_ft_size == Au_t_ft_size, "Au_f->ft not identical to Au_t_f->ft");
        memcpy(&Au_Au_t_f_i.type.ft, &typeid(Au)->ft, Au_ft_size);
        typeid(Au)->table_size = Au_ft_size;
        head(typeid(Au_t))->au = Au_Au_t_f_i.info.au;

        // expose the vtable pointer ('au') as an accessible Silver field on Au instances
        // index=0 matches the has_fn_only vtable slot; is_elaborate=1 prevents etype_implement
        // from adding a duplicate LLVM struct member (has_fn_only already emits it at slot 0)
        Au_t au_field = def(typeid(Au), "au", AU_MEMBER_VAR, 0);
        au_field->access_type = interface_public;
        au_field->offset      = 0;
        au_field->type        = typeid(Au_t);
        au_field->member_type = AU_MEMBER_VAR;
        au_field->member_index       = 0;
        au_field->is_elaborate = 1;
    }
    
    // on first call, we register our basic type structures:
    if (type == typeid(Au_t)) {
        module->traits |= AU_TRAIT_IS_AU;
        // the first ever type we really register is the collective_abi
        Au_t au_collective = def(module, "collective_abi",
            AU_MEMBER_TYPE, AU_TRAIT_STRUCT | AU_TRAIT_SYSTEM);
        def_member(au_collective, "count",     typeid(i32), AU_MEMBER_VAR, 0);
        def_member(au_collective, "alloc",     typeid(i32), AU_MEMBER_VAR, 0);
        def_member(au_collective, "hsize",     typeid(i32), AU_MEMBER_VAR, 0);
        def_member(au_collective, "origin",    typeid(ARef), AU_MEMBER_VAR, 0);
        def_member(au_collective, "first",     typeid(ARef), AU_MEMBER_VAR, 0);
        def_member(au_collective, "last",      typeid(ARef), AU_MEMBER_VAR, 0); 
        def_member(au_collective, "hlist",     typeid(ARef), AU_MEMBER_VAR, 0);
        def_member(au_collective, "unmanaged", typeid(bool), AU_MEMBER_VAR, 0);
        def_member(au_collective, "assorted",  typeid(bool), AU_MEMBER_VAR, 0);
        def_member(au_collective, "hash_only", typeid(bool), AU_MEMBER_VAR, 0); 
        def_member(au_collective, "last_type", typeid(ARef), AU_MEMBER_VAR, 0);
        //def_member(au_collective, "type",      typeid(ARef), AU_MEMBER_VAR, 0);
        //def_member(au_collective, "shape",     typeid(ARef), AU_MEMBER_VAR, 0);

        Au_t au_t = type; // pushed from the first global ctr call
        au_t->member_type = AU_MEMBER_TYPE;
        au_t->traits = AU_TRAIT_CLASS;

        def_member(au_t, "context",       typeid(Au_t), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, context);
        def_member(au_t, "src",           typeid(Au_t), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, src);
        def_member(au_t, "schema",        typeid(Au_t), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, schema);
        def_member(au_t, "module",        typeid(Au_t), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, module);
        def_member(au_t, "ptr",           typeid(Au_t), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, ptr);
        def_member(au_t, "ident",         typeid(cstr), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, ident);
        def_member(au_t, "alt",           typeid(cstr), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, alt);
        def_member(au_t, "table_size",    typeid(u32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, table_size);
        def_member(au_t, "abi_size",      typeid(u32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, abi_size);
        def_member(au_t, "align_bits",    typeid(u32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, align_bits);
        def_member(au_t, "record_alignment", typeid(u32), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, record_alignment);
        def_member(au_t, "member_index",  typeid(i64),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, member_index);
        def_member(au_t, "af_index",      typeid(i64),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, af_index);
        def_member(au_t, "value",         typeid(ARef), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, value);
        def_member(au_t, "member_type",   typeid(u8),   AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, member_type);
        def_member(au_t, "operator_type", typeid(u8),   AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, operator_type);
        def_member(au_t, "access_type",   typeid(u8),   AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, access_type);
        def_member(au_t, "reserved",      typeid(u8),   AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, reserved);
        def_member(au_t, "traits",        typeid(u64),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, traits);
        def_member(au_t, "global_count",  typeid(i32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, global_count);
        def_member(au_t, "offset",        typeid(i32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, offset);
        def_member(au_t, "elements",      typeid(i32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, elements);
        def_member(au_t, "typesize",      typeid(i32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, typesize);
        def_member(au_t, "isize",         typeid(i32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, isize);
        def_member(au_t, "icount",       typeid(i32),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, icount);
        def_member(au_t, "ident_hash",   typeid(u64),  AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, ident_hash);
        def_member(au_t, "fn",            typeid(ARef), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, fn);
        def_member(au_t, "ffi",           typeid(ARef), AU_MEMBER_VAR, 0)->offset = offsetof(struct _Au_f, ffi);
                
        
        def_member(au_t, "members", typeid(micro), AU_MEMBER_VAR, AU_TRAIT_INLAY)
            ->offset = offsetof(struct _Au_f, members);

        def_member(au_t, "member_map", typeid(ARef), AU_MEMBER_VAR, 0)
            ->offset = offsetof(struct _Au_f, member_map);

        def_member(au_t, "args", typeid(micro), AU_MEMBER_VAR, AU_TRAIT_INLAY)
            ->offset = offsetof(struct _Au_f, args);

        def_member(au_t, "meta", typeid(meta_t), AU_MEMBER_VAR, AU_TRAIT_INLAY)
            ->offset = offsetof(struct _Au_f, meta);

        // register meta_t members
        Au_t mt = typeid(meta_t);
        mt->is_struct  = true;
        mt->typesize   = sizeof(meta_t);
        def_member(mt, "a", typeid(Au_t), AU_MEMBER_VAR, 0)->offset = offsetof(meta_t, a);
        def_member(mt, "b", typeid(Au),   AU_MEMBER_VAR, 0)->offset = offsetof(meta_t, b);
 
        Au_t required_bits = def_member(au_t, "required_bits",  typeid(u64), AU_MEMBER_VAR, 0);
        required_bits->elements = 4;

        // we are having trouble creating the space inside an inlay struct at the tail of Au_t; this is to compensate
        def_member(au_t, "ft_space", typeid(ARef), AU_MEMBER_VAR, 0)
            ->offset = offsetof(struct _Au_f, ft);
    
        Au_t ft = def(au_t, null, AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
        def_member(ft, "_none_", typeid(ARef), AU_MEMBER_VAR, 0);
        def_member(au_t, "ft", ft, AU_MEMBER_TYPE, AU_TRAIT_STRUCT)->offset = offsetof(struct _Au_f, ft);
        // this process is replicated in schema creation / etype_init
    }

    members(type, m) {
        if (m->module && m->module != type->module) {
            fprintf(stderr, "MODULE OVERWRITE [etype_define]: %s (%p) module %p -> %p\n", m->ident, m, m->module, type->module);
            exit(1);
        }
        m->module = type->module;
    }

    micro_push(&type->module->members, (Au)type);

    //type->af->re_alloc = 1024;
    //type->af->re = (object*)(Au*)calloc(1024, sizeof(Au));

    if ((type->traits & AU_TRAIT_POINTER) != 0) {
        type->src = type->meta.a ? type->meta.a : null;
    }

    if ((type->traits & AU_TRAIT_ABSTRACT) == 0) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if ((mem->traits & AU_TRAIT_REQUIRED) != 0 && (mem->member_type == AU_MEMBER_VAR) && mem->af_index)
                AF_set(type->required_bits, mem->af_index - 1);
        }
    }

    for (int i = 0; i < call_after_count; i++) {
        struct _init_dep* f = &call_after[i];
        if (f->dep == type && f->call_after())
            f->dep = null;
    }
}

Au_t current_module() {
    return module;
}

i64 current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (i64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

Au_t scope_lookup(array a, string f) {
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

ARef types(ref_i64 length) {
    *length = module->members.count;
    return module->members.origin;
}

// all DIRECT subclasses of `base` across EVERY module (base->src match). returns a
// flat Au_t[] (reused static buffer) + count via `length`. used for class-driven
// registries (e.g. orbiter's ResourceView views) without a per-module walk in silver.
static Au_t* subclass_buf = NULL;
static int   subclass_cap = 0;
ARef subclasses(Au_t base, ref_i64 length) {
    micro* mods = au_current_space ? &au_current_space->modules : &modules;
    pthread_rwlock_t* lk = au_current_space ? &au_current_space->lock : &modules_lock;
    pthread_rwlock_rdlock(lk);
    int count = 0, total_members = 0;
    for (int pass = 0; pass < 2; pass++) {
        int k = 0;
        for (int i = 0; i < mods->count; i++) {
            Au_t mod = (Au_t)mods->origin[i];
            if (!mod) continue;
            for (int j = 0; j < mod->members.count; j++) {
                Au_t t = (Au_t)mod->members.origin[j];
                if (pass == 0) total_members++;
                if (!t || t == base || !inherits(t, base)) continue;
                if (pass == 0) count++;
                else subclass_buf[k++] = t;
            }
        }
        if (pass == 0 && count > subclass_cap) {
            subclass_cap = count;
            subclass_buf = (Au_t*)realloc(subclass_buf, sizeof(Au_t) * (subclass_cap ? subclass_cap : 1));
        }
    }
    pthread_rwlock_unlock(lk);
    *length = count;
    return (ARef)subclass_buf;
}

Au_t find_module(symbol name) {
    if (au_current_space) {
        // search space first — modules created during this compile take priority
        // but skip empty space modules (cached imports): fall through to global
        pthread_rwlock_rdlock(&au_current_space->lock);
        Au_t space_mod = null;
        for (int i = 0; i < au_current_space->modules.count; i++) {
            Au_t mod = (Au_t)au_current_space->modules.origin[i];
            if (mod && !mod->is_hidden && mod->ident && strcmp(mod->ident, name) == 0) {
                space_mod = mod;
                break;
            }
        }
        pthread_rwlock_unlock(&au_current_space->lock);
        if (space_mod && space_mod->members.count > 0)
            return space_mod;
        // fall back to global for already-loaded runtime modules (read-only reference)
        pthread_rwlock_rdlock(&modules_lock);
        for (int i = 0; i < modules.count; i++) {
            Au_t mod = (Au_t)modules.origin[i];
            if (mod && !mod->is_hidden && mod->ident && strcmp(mod->ident, name) == 0) {
                pthread_rwlock_unlock(&modules_lock);
                return mod;
            }
        }
        pthread_rwlock_unlock(&modules_lock);
        return null;
    }
    pthread_rwlock_rdlock(&modules_lock);
    for (int i = 0; i < modules.count; i++) {
        Au_t mod = (Au_t)modules.origin[i];
        if (mod && !mod->is_hidden && mod->ident && strcmp(mod->ident, name) == 0) {
            pthread_rwlock_unlock(&modules_lock);
            return mod;
        }
    }
    pthread_rwlock_unlock(&modules_lock);
    return null;
}

Au_t find_type(symbol name, Au_t m) {
    if (!name)
        return null;
    if (au_current_space) {
        // search space modules first
        pthread_rwlock_rdlock(&au_current_space->lock);
        for (int i = 0; i < au_current_space->modules.count; i++) {
            Au_t mod = (Au_t)au_current_space->modules.origin[i];
            if (mod && (!m || m == mod)) {
                for (int j = 0; j < mod->members.count; j++) {
                    Au_t type = (Au_t)mod->members.origin[j];
                    if (type->ident && strcmp(name, type->ident) == 0) {
                        pthread_rwlock_unlock(&au_current_space->lock);
                        return type;
                    }
                }
            }
        }
        pthread_rwlock_unlock(&au_current_space->lock);
        // fall back to global modules (built-ins + already-loaded runtime modules)
        pthread_rwlock_rdlock(&modules_lock);
        for (int i = 0; i < modules.count; i++) {
            Au_t mod = (Au_t)modules.origin[i];
            if (mod && (!m || m == mod)) {
                for (int j = 0; j < mod->members.count; j++) {
                    Au_t type = (Au_t)mod->members.origin[j];
                    if (type->ident && strcmp(name, type->ident) == 0) {
                        pthread_rwlock_unlock(&modules_lock);
                        return type;
                    }
                }
            }
        }
        pthread_rwlock_unlock(&modules_lock);
        return null;
    }
    pthread_rwlock_rdlock(&modules_lock);
    for (int i = 0; i < modules.count; i++) {
        Au_t mod = (Au_t)modules.origin[i];
        if (mod && (!m || m == mod)) {
            if (m && m != mod)
                continue;
            for (int j = 0; j < mod->members.count; j++) {
                Au_t type = (Au_t)mod->members.origin[j];
                if (type->ident && strcmp(name, type->ident) == 0) {
                    pthread_rwlock_unlock(&modules_lock);
                    return type;
                }
            }
        }
    }
    pthread_rwlock_unlock(&modules_lock);
    return null;
}

static inline u64* af_bits_ptr(Au a) {
    return (u64*)((u8*)a + sizeof(void*));
}

AF* Au_AF_bits(Au a) {
    return af_bits_ptr(a);
}

void Au_AF_set_id(Au a, int id) {
    AF_set(af_bits_ptr(a), id);
}

// member index is 1-based (0 == not an af-bit slot); the bit position is index-1.
void Au_AF_set_name(Au a, cstr name) {
    Au_t t = isa(a);
    Au_t m = find_member(t, name, AU_MEMBER_VAR, 0, true);
    if (m && m->af_index) AF_set(af_bits_ptr(a), m->af_index - 1);
}

i32 Au_AF_query_name(Au a, cstr name) {
    Au_t t = isa(a);
    Au_t m = find_member(t, name, AU_MEMBER_VAR, 0, true);
    return (m && m->af_index) ? (i32)AF_get(af_bits_ptr(a), m->af_index - 1) : 0;
}

bool Au_AF_get_member(Au a, Au_t mem) {
    return mem->af_index ? AF_get(af_bits_ptr(a), mem->af_index - 1) : false;
}

none Au_AF_set_member(Au a, Au_t mem) {
    if (mem->af_index) AF_set(af_bits_ptr(a), mem->af_index - 1);
}

bool Au_validator(Au a) {
    Au_t type = isa(a);

    u64* f = af_bits_ptr(a);
    if (((type->required_bits[0] & f[0]) != type->required_bits[0]) ||
        ((type->required_bits[1] & f[1]) != type->required_bits[1]) ||
        ((type->required_bits[2] & f[2]) != type->required_bits[2]) ||
        ((type->required_bits[3] & f[3]) != type->required_bits[3])) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t m = (Au_t)type->members.origin[i];
            if ((m->traits & AU_TRAIT_REQUIRED) != 0 && m->af_index && AF_get(f, m->af_index - 1) == 0) {
                u8* ptr = (u8*)a + m->offset;
                Au* ref = (Au*)ptr;
                fault("expected arg [%s] not set for class %s",
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
    if (single) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if ((mem->member_type & AU_MEMBER_ENUMV) &&
                (tolower(mem->ident[0]) == tolower(cs[0]))) {
                return *(i32*)enum_member_value(type, mem);
            }
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
    Au_t map_type = typeid(map);
    if (!current) return;
    if (current == (Au_t)&Au_Au_i.type) return;
    Au_f* au_f = ((Au_f*)current);
    none(*init)(Au) = au_f->ft.init;
    init_recur(a, current->context, (raw)init);
    if (init && init != (none*)last_init) {
        init(a);
    }
}

/*
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
*/

string f32_cast_string(f32* a) { return f(string, "%f",  *(f32*)a); }
string f64_cast_string(f64* a) { return f(string, "%lf", *(f64*)a); }

// knows pi to a thousand places; got a collection of gigantic maces; 
// even made a function table for my dog
f32 f32_round(f32* a, i32 places) {
    f32 scale   = powf(10.0f, (f32)places);
    f32 scaled  = *a * scale;
    f32 rounded = nearbyintf(scaled);
    return rounded / scale;
}

bool f32_is_nan(f32* a)     { return isnan(*a); }
bool f32_is_inf(f32* a)     { return isinf(*a); }
bool f32_is_finite(f32* a)  { return isfinite(*a); }
bool f32_is_zero(f32* a)    { return *a == 0.0f; }

f64 f64_round(f64* a, i32 places) {
    f64 scale   = pow(10.0, (f64)places);
    f64 scaled  = *a * scale;
    f64 rounded = nearbyint(scaled);
    return rounded / scale;
}

bool f64_is_nan(f64* a)     { return isnan(*a); }
bool f64_is_inf(f64* a)     { return isinf(*a); }
bool f64_is_finite(f64* a)  { return isfinite(*a); }
bool f64_is_zero(f64* a)    { return *a == 0.0; }

Au Au_initialize(Au a) {
    Au   f = header(a);
    // user-init is now implemented at design-time
    //if (f->type->traits & AU_TRAIT_USER_INIT) return a;
    // primitives, enums, and other non-class types have no init chain
    // or hold_members vtable; skip them
    if (f->au->traits & (AU_TRAIT_STRUCT | AU_TRAIT_PRIMITIVE | AU_TRAIT_ENUM)) return a; // isolate these cases and remove this code

    #ifndef NDEBUG
    //Au_validator(a);
    #endif

    init_recur(a, (Au_t)f->au, null);
    Au_t_f* ptr = (Au_t_f*)isa(a);
    hold_members(a);
    return a;
}

__thread __error_t* Au_error_top = NULL;

void halt(string msg, token tok) {
    if (!Au_error_top) {
#ifndef NDEBUG
        raise(SIGTRAP);
        exit(1);
#else
        abort();
#endif
    }
    Au_error_top->message = msg;
    Au_error_top->tok     = tok;
    longjmp(Au_error_top->env, 1);
}



pid_t _last_pid = 0;

i64 last_pid() {
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

string command_run(command cmd, bool verbose) {
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

int command_exec(command cmd, bool verbose) {
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
        cstr verb = getenv("VERBOSE");
        if ((verb && strcmp(verb, "0") != 0) || verbose) {
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

__thread ARef af       = null;
__thread int  af_count = 2; // managed == 1 means its not in the af vector, managed == 0 means we are not managed memory; hold and drop are null ops
__thread int  af_size  = 0;

none Au_free(Au);

none Au_drop(Au a) {
    if (!a) return;
    Au info = header(a);
    if (!info->managed) return;
    i32 n = __atomic_sub_fetch((i32*)__builtin_assume_aligned(&info->refs, 4), 1, __ATOMIC_SEQ_CST);
#ifndef NDEBUG
    if (au_prov_match(a)) au_prov_pop(a);
#endif
    if (n <= 0) {
        af[info->managed] = null;
        Au_free(a);
    }
}

static int total_objects;

int alloc_count(Au_t type) {
    return type ? type->global_count : total_objects;
}

Au alloc_instance(Au_t type, int n_bytes, bool managed) {
    Au a = null;
    //af && n_bytes == recycle_size;

    #ifndef NDEBUG
    type->global_count++;
    total_objects++;
    #endif

    a = calloc(1, n_bytes);
    a->refs = 0;
    a->managed = managed ? af_count : 0;
    if (managed && af_count >= af_size) {
        ARef af_prev = af;
        int new_size = (af_size + 16) << 2;
        af = (ARef)calloc(sizeof(ARef), new_size);
        if (af_prev)
            memcpy(af, af_prev, af_size * sizeof(ARef));
        af_size = new_size;
    }
    af[af_count++] = a;
    
    return a;
}

none Au_free(Au a);

none auto_free(bool reset_only) {
    // only managed objects go into af
    for (num i = 2; i < af_count; i++) {
        Au a = af[i];

        if (a && a->refs == 0) {
            //print("auto freeing data from %s:%i", a->source, a->line);
            if (!reset_only) Au_free(&a[1]);
        } else if (a)
            a->managed = 1; // says i am not in the list, but managed
    }
    af_count = 2;
}

Au alloc_dbg(Au_t type, num count, symbol source, i32 line, i32 sequence) {
    sz map_sz = sizeof(map);
    sz _sz   = sizeof(struct _Au);
    Au a = alloc_instance(type, _sz + (type->typesize << 1) * count, true);
    a->au       = (Au_f*)type;
    a->data       = &a[1];
    if (type->is_class) *(void**)a->data = (void*)type;
    a->count      = count;
    a->alloc      = count;
    a->source     = (cstr)source;
    a->line       = line;
    a->sequence   = sequence;
    if (!type->is_au_native && !source) {
        printf("warning: no source binding for allocation %s:%i\n", a->source, a->line);
        exit(0);
    }
    return a->data; /// return fields (Au)
}


static ARef  tracing = null;
static int   tracing_count = 0;



void alloc_trace() {
    tracing = calloc(1024 * 100, sizeof(ARef));
    tracing_count = 0;
}


int get_total_objects(Au_t type) {
    if (type) {
        return type->global_count;
    }
    return total_objects;
}

void alloc_validate() {
    for (int i = 0; i < tracing_count; i++) {
        Au obj  = tracing[i];
        if (!obj) continue;
        Au info = head(obj);

        printf("target-remains:%llx, refs:%i, source:%s:%i managed:%i\n",
            (unsigned long long)(uintptr_t)obj, (int)info->refs, info->source, (int)info->line, info->managed);
    }
}

Au alloc(Au_t type, num count, shape shape_data, Au_t meta_a, Au meta_b, symbol source, i32 line, i32 seq) {
    sz map_sz = sizeof(map);
    sz _sz   = sizeof(struct _Au);

    sz alloc_count = shape_data ? shape_total(shape_data) : (count ? count : 1);
    sz n_bytes = _sz + type->typesize * alloc_count;
    if (n_bytes == _sz)
        printf("alloc: WARNING typesize=0 type=%s _sz=%lld\n", type->ident, (long long)_sz);
    Au a = alloc_instance(type, n_bytes, true);

    a->au       = (Au_f*)type;
    a->data       = &a[1];
    if (type->is_class) *(void**)a->data = (void*)type;
    a->count      = alloc_count;
    a->alloc      = alloc_count;
    a->data_shape = hold(shape_data);
    a->source     = (cstr)source;
    a->line       = line;
    a->sequence   = seq;

    if (!type->is_au_native && !source) {
        printf("warning: no source binding for allocation of type %s\n", type->ident);
        exit(0);
    }

    if (meta_a) a->meta_a = meta_a;
    if (meta_b) a->meta_b = meta_b;
    if (tracing)
        tracing[tracing_count++] = a->data;

    return a->data;
}

Au alloc_new(Au_t type, num count, shape shape_data, Au_t meta_a, Au meta_b,
             symbol source, i32 line, i32 seq) {
    return alloc(type, count, shape_data, meta_a, meta_b, source, line, seq);
}

// N-slot reference holder — the held type's dealloc chain does NOT apply to
// the holder's raw buffer; slots are user-managed refs.
#define AU_IF_HOLDER 0x02

Au alloc_vector(Au_t type, num count, shape shape_data, Au_t meta_a, Au meta_b,
                symbol source, i32 line, i32 seq) {
    Au a  = alloc(type, count, shape_data, meta_a, meta_b, source, line, seq);
    Au hd = header(a);
    hd->iflags  |= AU_IF_HOLDER;
    return a;
}

Au alloc2(Au_t type, Au_t scalar, shape s, symbol source, i32 line, i32 seq) {
    i64 _sz      = sizeof(struct _Au);
    i64 count     = shape_total(s);
    Au a      = alloc_instance(type,
        _sz + scalar->typesize * count, true);
    a->scalar     = scalar;
    a->au       = (Au_f*)type;
    a->data       = &a[1];
    a->data_shape = hold(s);
    a->count      = count;
    a->alloc      = count;
    a->source     = (cstr)source;
    a->line       = line;
    a->sequence   = seq;
    return a->data;
}

Au new_object(Au_t type, Au_t meta_a, Au meta_b, bool call_init, symbol source, i32 line, i32 seq) {
    Au a = alloc_new(type, 1, null, meta_a, meta_b, source, line, seq);
    if (call_init)
        Au_initialize(a);
    return a;
}

ffi_method_t* method_with_address(handle address, Au_t rtype, micro* args, Au_t method_owner) {
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

__attribute__((no_sanitize("address"))) 
Au method_call(Au_t m, array args) {
    if (!m->ffi) m->ffi = method_with_address(m->value, m->type, (micro*)&m->args, m->context);
    ffi_method_t* a = m->ffi;
    const num max_args = 64;
    none* arg_values[max_args];
    Au    arg_data[max_args];

    memset(arg_values, 0, sizeof(arg_values));
    memset(arg_data,   0, sizeof(arg_data));
    
    // populate provided args, create default objects for any missing
    for (num i = 0; i < a->atypes->count; i++) {
        Au_t arg_type = au_arg_type((Au)a->atypes->origin[i]);
        assert(arg_type->typesize > 0 || (arg_type->traits & (AU_TRAIT_PRIMITIVE | AU_TRAIT_ENUM | AU_TRAIT_ABSTRACT | AU_TRAIT_CLASS)), "arg type %s has zero typesize", arg_type->ident);
        if (args && i < args->count) {
            arg_values[i] = (arg_type->traits & (AU_TRAIT_PRIMITIVE | AU_TRAIT_ENUM)) ?
                (none*)args->origin[i] : (none*)&args->origin[i];
        } else {
            arg_data[i] = alloc(arg_type, 1, null, null, null, __FILE__, __LINE__, 0);
            arg_values[i] = (arg_type->traits & (AU_TRAIT_PRIMITIVE | AU_TRAIT_ENUM)) ?
                (none*)arg_data[i] : (none*)&arg_data[i];
        }
    }
    none* result[64]; /// enough space to handle all primitive data
    ffi_call((ffi_cif*)a->ffi_cif, a->address, result, arg_values);
    if (a->rtype->traits & AU_TRAIT_PRIMITIVE)
        return primitive(a->rtype, result);
    else if (a->rtype->traits & AU_TRAIT_ENUM) {
        Au res = alloc(a->rtype, 1, null, null, null, __FILE__, __LINE__, 0);
        verify(a->rtype->src == typeid(i32), "i32 enums supported");
        *((i32*)res) = *(i32*)result;
        return res;
    } else
        return (Au) result[0];
}

/// this calls type methods
Au method(Au_t type, cstr method_name, array args) {
    Au_t mem = find_member(type, method_name, AU_MEMBER_FUNC, 0, false);
    assert(mem->ffi, "method not set");
    Au res = method_call(mem, args);
    return res;
}

Au convert(Au_t type, Au input) {
    Au info = head(input);
    if (type == isa(input))
        return input;
    return construct_with(type, input, null);
}

Au method_vargs(Au a, Au_t mem, int n_args, ...) {
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
    Au res = method_call(mem, args);
    return res;
}


int fault_level;

static __attribute__((constructor)) bool Aglobal_AF();

string Au_cast_string(Au a);
string numeric_cast_string(numeric a);

none member_override(Au_t type, Au_t type_mem, AFlag f) {
    Au_t base = type->context;

    while (base) {
        for (num i = 0; i < base->members.count; i++) {
            Au_t m = (Au_t)base->members.origin[i];
            if (m->member_type == f && strcmp(m->ident, type_mem->ident) == 0) {
                type_mem->offset        = m->offset; // todo: better idea to use src on type_mem
                for (int ar = 0; ar < m->args.count; ar++) {
                    Au_t arg = (Au_t)m->args.origin[ar];
                    verify(arg->member_type == AU_MEMBER_VAR, "unexpected member_type on arg");
                    def_arg(type_mem, arg->ident, arg->src, 0);
                }
                //type_mem->args          = m->args;
                type_mem->src           = m->type;
                type_mem->member_type   = m->member_type;// | AU_MEMBER_OVERRIDE;
                type_mem->is_override   = 1;
                type_mem->member_index         = m->member_index;

                verify(m->member_index, "method %s.%s cannot be overridden\n", base->ident, m->ident);

                verify(m->value, "method pointer not set on source yet (%s.%s); cannot override\n",
                    base->ident, m->ident);
                 
                struct _string*(*base_method)(Au) = (void*)((ARef)&base->ft.__none__)[m->member_index];
                struct _string*(*ptr_method)(Au) = (void*)m->value;
                verify((void*)base_method == (void*)m->value, "method not stored in base table correctly");
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
            //printf("existing member on type %s = %p\n", base->ident, m);
            if (m->ident && strcmp(m->ident, type_mem->ident) == 0) {
                type_mem->offset = m->offset; // todo: better idea to use src on type_mem
                type_mem->args   = m->args;
                type_mem->src    = m->type;
                type_mem->member_type = m->member_type;// | AU_MEMBER_OVERRIDE;
                ARef ptr_find = (ARef)m->value;
                verify(m->member_index, "method %s.%s cannot be overridden\n", base->ident, m->ident);
                verify(m->value, "method pointer not set on source yet (%s.%s); cannot override\n",
                    base->ident, m->ident);
                ((ARef)&type->ft)[m->member_index] = (Au)m->value;
                return;
            }
        }
        if (base == typeid(Au)) break;
        base = base->context;
    }
}

path path_share_path();
path path_cwd();
static path startup_cwd_ = null;   // the cwd we launched in, before cd to share
none engage(cstrs argv) {
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
    if (!explicit_listen) set(log_funcs, string("*"), _bool(true));
#endif

    if (len(log_funcs)) {
        string topics = string(alloc, 32);
        pairs(log_funcs, i) {
            if (len(topics) > 0)
                append(topics, ", ");
            concat(topics, (string)i->key);
        }
        if (len(topics) && !eq(topics, "*"))
            printf("listening-to: %s\n", topics->chars);
    }

    if (argv) {
        // capture the launch cwd before we change it to the app's share dir
        if (!startup_cwd_) startup_cwd_ = hold(path_cwd());
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

map args(cstrs argv, symbol default_arg, ...) {
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
    return arguments(argc, argv, defaults,
        (Au)(default_arg ? string(default_arg) : null));
}

none tap(symbol f, hook sub) {
    string fname = string(f);
    set(log_funcs, (Au)fname, sub ? (Au)sub : (Au)_bool(true)); /// if subprocedure, then it may receive calls for the logging
}

none untap(symbol f) {
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

bool is_inlay(Au_t m) {
    return (m->type->traits & AU_TRAIT_STRUCT    |
            m->type->traits & AU_TRAIT_PRIMITIVE |
            m->type->traits & AU_TRAIT_ENUM      |
            m->type->traits & AU_TRAIT_INLAY     |
            m->type->traits & AU_TRAIT_POINTER) != 0;
}

none Au_hold_members(Au a) {
    Au_t type = isa(a);
    Au head = header(a);
    if (head->iflags & 0x01) return;
    head->iflags |= 0x01;
    while (type != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if (mem->member_type != AU_MEMBER_VAR) continue;
            if (mem->is_unmanaged || mem->is_static || mem->type == typeid(ARef)) continue;
            bool hold = (!is_inlay(mem) && mem->type->is_class) || mem->type->is_shaped;
            if (!hold) continue;
            if (mem->meta.a == typeid(weak) || mem->is_context) continue;
            Au *mdata = (Au*)((cstr)a + mem->offset);
            Au member_value = *mdata;
            if (!member_value) continue;
            Au hd = header(member_value);
            if (hd->managed) {
                __atomic_fetch_add((i32*)__builtin_assume_aligned(&hd->refs, 4), 1, __ATOMIC_SEQ_CST);
                if (hd->refs <= 0 && strcmp(hd->au->ident, "Canvas") == 0) {
                    printf("(hold members) holding FREED Canvas (%p) with refs now set to %i\n", *mdata, hd->refs);
                }
            }
        }
        type = type->context;
    }
}

Au Au_set_property(Au a, symbol name, Au value) {
    Au_t type = isa(a);
    Au_t m = find_member(type, (cstr)name, AU_MEMBER_VAR, 0, true);
    /// a persisted .agi outlives the schema that wrote it; skip dropped fields
    if (!m) {
        print("set_property: no member %s on %s (skipped)", name, type->ident);
        return value;
    }
    member_set(a, m, value);
    return value;
}


Au Au_get_property_by_type(Au a, Au_t find_type) {
    Au_t type = isa(a);
    while (type && type != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if (mem->member_type != AU_MEMBER_VAR) continue;
            if (mem->type == find_type || (Au_t)au_arg_type((Au)mem->type) == find_type) {
                Au *mdata = (Au*)((cstr)a + mem->offset);
                return is_inlay(mem) ? primitive(mem->type, mdata) : *mdata;
            }
        }
        type = type->context;
    }
    return null;
}

none Au_set_context_from(Au target, Au source) {
    Au_t type     = isa(target);
    Au_t src_type = isa(source);
    while (type && type != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t mem = (Au_t)type->members.origin[i];
            if (mem->member_type != AU_MEMBER_VAR || !mem->is_context) continue;
            Au val = inherits(src_type, mem->src) ? source : Au_get_property_by_type(source, mem->type);
            Au_t_f* fn = (Au_t_f*)isa(target);
            if (val) member_set(target, mem, val);
        }
        type = type->context;
    }
}

Au Au_get_property(Au a, symbol name) {
    Au_t type = isa(a);
    Au_t m = find_member(type, (cstr)name, AU_MEMBER_VAR, 0, true);
    if (!m) return null;
    Au *mdata = (Au*)((cstr)a + m->offset);
    if (is_inlay(m)) return primitive(m->type, mdata);
    return *mdata;
}

map arguments(int argc, cstrs argv, map default_values, Au default_key) {
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
                    Au value = formatter(def_type, false, null, (Au)false, seq, "%o", s_val);
                    assert(isa(value) == def_type, "");
                    set(result, (Au)f->key, value);
                }
            }
        } else if (!found_single && default_key) {
            Au default_key_obj = header(default_key);
            string s_val     = new(string, chars, (cstr)arg);
            Au def_value = get(default_values, default_key);
            Au_t  def_type  = isa(def_value);
            Au value     = formatter(def_type, false, null, (Au)false, seq, "%o", s_val);
            set(result, (Au)default_key, value);
            found_single = true;
        }
        i += 2;
    }
    return result;
}

Au primitive(Au_t type, none* data) {
    Au copy = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
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
    Au_t type = (Au_t)f->au;
    #ifndef NDEBUG
    const char *type_ident = (type && type->ident) ? type->ident : "";
    if (au_skip_drop_check(type_ident, NULL)) return;
    #endif
    while (type != typeid(Au)) {
        for (num i = 0; i < type->members.count; i++) {
            Au_t m = (Au_t)type->members.origin[i];
            if ((m->member_type == AU_MEMBER_VAR) &&
                    !is_inlay(m) && m->type->is_class) {
                if (m->meta.a == typeid(weak))
                    continue;
                if (m->is_context)
                    continue;
                if (m->traits & AU_TRAIT_UNMANAGED)
                    continue;
                #ifndef NDEBUG
                if (au_skip_drop_check(type_ident, m->ident)) continue;
                #endif
                //printf("Au_dealloc: drop member %s.%s (%s)\n", type->ident, m->ident, m->type->ident);
                Au*  ref = (Au*)((u8*)a + m->offset);
                Au info = head(*ref);
                Au_drop(*ref);
                *ref = null;
            }
        }
        type = type->context;
    }
}

none Au_dealloc(Au a) {
    Au   f    = header(a);
    Au_t type = (Au_t)f->au;

    drop_members(a);
    // NOTE: do NOT drop f->meta_b here. collections store their value/element
    // TYPE in the header meta_b (a shared singleton, unheld) — dropping it
    // underflows the type's refcount and corrupts the heap. Au_attach's
    // companion is intentionally not auto-freed until a safe owner exists.

    if ((Au)f->data != (Au)a) {
        drop(f->data);
        f->data = null;
    }
}

u64 fnv1a_hash(const none* data, size_t length, u64 hash);

// auto-wash
u64  Au_hash(Au a) {
    Au_t info = isa(a);
    if (info == typeid(Au_t)) return (u64)(size_t)a;

    u64 hash = 0;
    Au_f* fn = (Au_f*)info;
    if (fn->ft.hash && fn->ft.hash != ((Au_f*)typeid(Au))->ft.hash) {
        hash = hash(a);
    } else if (info->is_enum || (info->traits & AU_TRAIT_PRIMITIVE)) {
        // silver-built enums/primitives carry no ftable — hash the value bytes
        hash = (u64)fnv1a_hash(a, info->typesize, OFFSET_BASIS);
    } else {
        string s = cast(string, a);
        verify(s, "%o cast string required");
        hash = (u64)fnv1a_hash(s->chars, s->count, OFFSET_BASIS);
    }
    return hash;
}

// map-key compare that survives types with no ftable (silver enums): dispatch
// the override when present, else raw byte compare
static i32 au_key_compare(Au a, Au b) {
    Au_t t = isa(a);
    if (((Au_f*)t)->ft.compare && ((Au_f*)t)->ft.compare != ((Au_f*)typeid(Au))->ft.compare)
        return compare(a, b);
    return Au_compare(a, b);
}

bool Au_cast_bool (Au a) {
    Au info = header(a);
    bool has_count = info->count > 0;
    if (has_count && info->au == typeid(bool))
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

// print `usage: <prog> <default-arg> [options]` (from argv[0] + the type's
// default/positional member) and exit cleanly — used for missing required args
// and unknown flags (e.g. --help) instead of trapping.
static void au_arg_usage(Au a, cstrs argv) {
    symbol defname = null;
    for (Au_t rt = isa(a); rt && rt != typeid(Au); ) {
        for (num i = 0; i < rt->members.count; i++) {
            Au_t d = (Au_t)rt->members.origin[i];
            if (d->is_default && d->ident) { defname = d->ident; break; }
        }
        if (defname || rt->context == rt) break;
        rt = rt->context;
    }
    if (!defname) defname = "module";
    cstr prog = (argv && argv[0]) ? argv[0] : "program";
    cstr base = strrchr(prog, '/'); base = base ? base + 1 : prog;
    fprintf(stderr, "usage: %s <%s> [options]\n", base, defname);
    exit(0);
}

// a path-typed arg is built into a real absolute path, never stored as a
// string. cwd is the app's share bundle (cd_share), so a RELATIVE user arg
// resolves against SILVER_STARTUP (the launch dir); an absolute one passes
// through. this is why apps declare file inputs as `path`, not `string`.
static Au au_arg_path(cstr value) {
    if (!value) return null;
    if (value[0] == '/') return (Au)path(value);
    const char* sp = getenv("SILVER_STARTUP");
    if (!sp || !*sp) return (Au)path(value);
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/%s", sp, value);
    return (Au)path(buf);
}

Au Au_with_cstrs(Au a, cstrs argv) {
    engage(argv);
    if (!g_main_argv) g_main_argv = argv;
    int argc = argv[0] ? 1 : 0; // skip executable
    Au_t rtype = isa(a);
    while (argv[argc]) { // C standard puts a null char* on end, by law (see: Brannigans law)
        cstr arg = argv[argc];
        if (arg[0] == '-') {
            bool single = arg[1] != '-';
            Au_t mem    = null;
            Au_t type   = isa(a);
            while (type != typeid(Au)) {
                for (num i = 0; i < type->members.count; i++) {
                    Au_t m = (Au_t)type->members.origin[i];
                    if (m->access_type == interface_intern) continue;
                    // && binds tighter than || — parenthesized so BOTH forms
                    // require a VAR member (a method must never match a flag)
                    if ((m->member_type == AU_MEMBER_VAR) &&
                        (( single &&        m->ident[0] == arg[1]) ||
                         (!single && strcmp(m->ident,     &arg[2]) == 0))) {
                        mem = m;
                        break;
                    }
                }
                if (mem) break; // most-derived match wins; don't let a base class clobber it
                type = type->context;
            }
            if (!mem) {
                if (getenv("AU_ARG_DEBUG")) {
                    Au_t t2 = isa(a);
                    while (t2 && t2 != typeid(Au)) {
                        fprintf(stderr, "AU_ARG walk %s:\n", t2->ident);
                        for (num i = 0; i < t2->members.count; i++) {
                            Au_t m2 = (Au_t)t2->members.origin[i];
                            fprintf(stderr, "  member %s mt=%d at=%d\n",
                                m2->ident, (int)m2->member_type, (int)m2->access_type);
                        }
                        if (t2->context == t2) break;
                        t2 = t2->context;
                    }
                }
                au_arg_usage(a, argv);   // unknown flag (e.g. --help)
            }
            bool is_bool = mem->src == typeid(bool);
            cstr value = null;
            if (is_bool && argv[argc + 1] && (
                strcmp(argv[argc + 1], "1") == 0 ||
                strcmp(argv[argc + 1], "0") == 0 ||
                strcmp(argv[argc + 1], "true") == 0 ||
                strcmp(argv[argc + 1], "false") == 0)) {
                value = argv[++argc];
            }
            else if (is_bool || !argv[argc + 1] ||
                    (argv[argc + 1][0] == '-' &&
                     !(argv[argc + 1][1] >= '0' && argv[argc + 1][1] <= '9') &&
                       argv[argc + 1][1] != '.'))
                value = null;
            else
                value = argv[++argc];

            bool is_array = mem->src == typeid(array);
            verify(value || is_bool || is_array,
                "expected value after %s", &arg[1 + !single]);
            
            // arrays consume the remaining set; construct from the meta type
            if (is_array) {
                array arr = array();
                argc++;
                Au_t element = is_meta(a) ? meta_index(a, 0) : null;
                while (argv[argc]) {
                    string ar = string(argv[argc]);
                    if (element && element != typeid(none) && element != typeid(Au))
                        push(arr, (Au)construct_with(element, (Au)ar, null));
                    else
                        push(arr, (Au)ar);
                    argc++;
                }
                Au_set_property(a, mem->ident, (Au)arr);
                break;
            }

            Au conv;
            if (mem->src == typeid(path))
                conv = value ? au_arg_path(value) : null;
            else
                conv = value ? convert(mem->type, (Au)string(value)) : _bool(true);
            if (getenv("AU_ARG_DEBUG"))
                fprintf(stderr, "AU_ARG %s = %s (af_index %lld)\n",
                    mem->ident, value ? value : "(true)", (long long)mem->af_index);
            Au_set_property(a, mem->ident, conv);
        } else {
            Au_t def  = find_member(rtype, null, AU_MEMBER_VAR, AU_TRAIT_IS_DEFAULT, true);
            Au   conv = null;
            if (def && def->src == typeid(path))
                conv = au_arg_path(arg);
            else if (def)
                conv = convert(def->type, (Au)string(arg));
            if  (conv) Au_set_property(a, def->ident, (Au)conv);
            // the default value is the separator: everything after it
            // belongs to the launched program (see au_argv_stop)
            g_argv_stop = argc + 1;
            break;
        }
        argc++;
    }

    // verify all required properties were given. if one is missing, Au already
    // knows the required + default (positional) args — print a usage line and
    // exit cleanly rather than trapping with a raw "expected <member>".
    while (rtype && rtype != typeid(Au)) {
        for (num i = 0; i < rtype->members.count; i++) {
            Au_t m = (Au_t)rtype->members.origin[i];
            if (m->member_type == AU_MEMBER_VAR && m->is_required && !m->is_context && m->offset) {
                Au val = *(Au*)((cstr)a + m->offset);
                if (!val) au_arg_usage(a, argv);
            }
        }
        if (rtype->context == rtype) break;
        rtype = rtype->context;
    }
    return a;
}

Au Au_with_cereal(Au a, cereal _cs) {
    cstr cs = _cs.value;
    sz len = strlen(cs);
    Au        f = header(a);
    Au_t type = (Au_t)f->au;
    if      (type == typeid(f64)) sscanf(cs, "%lf",  (f64*)a);
    else if (type == typeid(f32)) sscanf(cs, "%f",   (f32*)a);
    else if (type == typeid(i32)) sscanf(cs, "%i",   (i32*)a);
    else if (type == typeid(u32)) sscanf(cs, "%u",   (u32*)a);
    else if (type == typeid(i64)) sscanf(cs, "%lli", (i64*)a);
    else if (type == typeid(u64)) sscanf(cs, "%llu", (u64*)a);
    else if (type == typeid(bool)) {
        *(bool*)a = (cs[0] == 't' || cs[0] == 'T' || cs[0] == '1');
    }
    else if (type == typeid(unichar)) {
        // first UTF-8 codepoint of the text
        u8 *p = (u8*)cs;
        u32 c = p[0];
        if      ((c & 0xE0) == 0xC0) c = ((c & 0x1F) << 6)  |  (p[1] & 0x3F);
        else if ((c & 0xF0) == 0xE0) c = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6)  |  (p[2] & 0x3F);
        else if ((c & 0xF8) == 0xF0) c = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        *(unichar*)a = (unichar)c;
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
        bool can = constructs_with((Au_t)f->au, typeid(string));
        if (can) {
            return construct_with((Au_t)f->au, (Au)string(cs), null);
        } else if (constructs_with((Au_t)f->au, typeid(cstr))) {
            return construct_with((Au_t)f->au, (Au)string(cs), null);
        }
        constructs_with((Au_t)f->au, typeid(cstr));
        printf("implement ctr cstr for %s\n", f->au->ident);
        exit(-1);
    }
    return a;
}

bool constructs_with(Au_t type, Au_t with_type) {
    for (num i = 0; i < type->members.count; i++) {
        Au_t mem = (Au_t)type->members.origin[i];
        if ((mem->member_type & AU_MEMBER_CONSTRUCT) != 0) {
            if (mem->args.count < 2) continue;
            Au_t arg = au_arg_type(micro_get(&mem->args, 1));
            if (arg == with_type)
                return true;
        }
    }
    return false;
}

/// used by parse (from json) to construct objects from data
Au construct_with(Au_t type, Au data, ctx context) { sequencer
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
        result = alloc(type, 1, null, null, null, __FILE__, __LINE__, seq);
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
    while (au && au != typeid(Au)) {
        for (num i = 0; i < au->members.count; i++) {
            Au_t mem = (Au_t)au->members.origin[i];
            
            if (!result && mem->member_type == AU_MEMBER_CONSTRUCT) {
                none* addr = mem->value;
                Au_t arg = au_arg_type(micro_get(&mem->args, 1));
                /// no meaningful way to do this generically, we prefer to call these first
                if (arg == typeid(path) && data_type == typeid(string)) {
                    result = alloc(type, 1, null, null, null, __FILE__, __LINE__, seq);
                    ((none(*)(Au, path))addr)(result, path(((string)data)));
                    verify(is_struct((Au)type) || Au_validator(result), "invalid Au");
                    break;
                }
                if ((arg == typeid(cstr) || arg == typeid(symbol)) &&
                        data_type == typeid(string)) {
                    result = alloc(type, 1, null, null, null, __FILE__, __LINE__, seq);
                    ((none(*)(Au, cstr))addr)(result, ((string)data)->chars);
                    verify(is_struct((Au)type) || Au_validator(result), "invalid Au");
                    break;
                }
                if (arg == data_type) {
                    result = alloc(type, 1, null, null, null, __FILE__, __LINE__, seq);
                    ((none(*)(Au, Au))addr)(result, data);
                    verify(is_struct((Au)type) || Au_validator(result), "invalid Au");
                    break;
                }
            } else if (context && result && mdata) {
                // lets set required properties from context
                string k = string(mem->ident);
                if ((mem->traits & AU_TRAIT_REQUIRED) != 0 && (mem->member_type == AU_MEMBER_VAR) && 
                    !contains(mdata, (Au)k))
                {
                    Au from_ctx = get(context, (Au)k);
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
            v = read_integer((Au)data_type);
        else if (data_type == typeid(symbol) || data_type == typeid(cstr))
            v = evalue (type, (cstr)data);
        else if (data_type == typeid(string))
            v = evalue (type, (cstr)((string)data)->chars);
        else
            v = evalue (type, null);
        result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
        *((i32*)result) = (i32)v;
    }

    /// check if we may use generic Au from string
    if (!result)
    if ((type->traits & AU_TRAIT_PRIMITIVE) && (data_type == typeid(string) ||
                                               data_type == typeid(cstr)   ||
                                               data_type == typeid(symbol))) {
        result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
        if (data_type == typeid(string))
            Au_with_cereal(result, (cereal) { .value = (cstr)((string)data)->chars } );
        else
            Au_with_cereal(result, (cereal) { .value = (cstr)data });
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
                result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                     if (mem->type == typeid(i8))   ((none(*)(Au, i8))  addr)(result, (i8)  v);
                else if (mem->type == typeid(i16))  ((none(*)(Au, i16)) addr)(result, (i16) v);
                else if (mem->type == typeid(i32))  ((none(*)(Au, i32)) addr)(result, (i32) v);
                else if (mem->type == typeid(i64))  ((none(*)(Au, i64)) addr)(result, (i64) v);
            } else if (combine & AU_TRAIT_REALISTIC) {
                result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                if (mem->type == typeid(f64))
                    ((none(*)(Au, double))addr)(result, (double)*(float*)data);
                else
                    ((none(*)(Au, float)) addr)(result, (float)*(double*)data);
                break;
            } else if ((mem->type == typeid(symbol) || mem->type == typeid(cstr)) && 
                       (data_type == typeid(symbol) || data_type == typeid(cstr))) {
                result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                ((none(*)(Au, cstr))addr)(result, (cstr)data);
                break;
            } else if (mem->type == typeid(string) && data_type == typeid(string)) {
                result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                ((none(*)(Au, string))addr)(result, (string)data);
                break;
            } else if ((mem->type == typeid(string)) &&
                       (data_type == typeid(symbol) || data_type == typeid(cstr))) {
                result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                ((none(*)(Au, string))addr)(result, string((symbol)data));
                break;
            } else if ((mem->type == typeid(symbol) || mem->type == typeid(cstr)) &&
                       (data_type == typeid(string))) {
                result = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                ((none(*)(Au, cstr))addr)(result, (cstr)((string)data)->chars);
                break;
            }
        }
    }

    // set field bits here (removed case where json parser was doing this)
    if (result && data_type == typeid(map)) {
        map f = (map)data;
        pairs(f, i) {
            string name = (string)i->key;
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

// call matching constructor on an already-allocated object (context props set beforehand)
none Au_call_construct(Au obj, Au data) {
    Au_t type      = isa(obj);
    Au_t data_type = isa(data);
    Au_t au = type;
    while (au && au != typeid(Au)) {
        for (num i = 0; i < au->members.count; i++) {
            Au_t mem = (Au_t)au->members.origin[i];
            if (mem->member_type != AU_MEMBER_CONSTRUCT) continue;
            none* addr = mem->value;
            Au_t  arg  = au_arg_type(micro_get(&mem->args, 1));
            if (arg == data_type) {
                ((none(*)(Au, Au))addr)(obj, data);
                return;
            }
            if (arg == typeid(string) && data_type == typeid(string)) {
                ((none(*)(Au, string))addr)(obj, (string)data);
                return;
            }
            if ((arg == typeid(cstr) || arg == typeid(symbol)) && data_type == typeid(string)) {
                ((none(*)(Au, cstr))addr)(obj, ((string)data)->chars);
                return;
            }
        }
        au = au->context;
    }
    Au_initialize(obj);
}

Au_t __typeid(Au input) {
    Au_t i = isa(input);
    return i;
}

Au __convert(Au_t type, Au value) {
    Au_t vtype = isa(value);
    if (vtype == type)
        return value;
    return formatter(type, false, null, (Au)false, 0, "%o", value);
}

Au Au_mix(Au a, Au target, f64 amount) {
    Au t_val = _f64(amount);
    Au diff  = __op(OPType__sub, target, a);
    Au scale = __op(OPType__mul, diff, t_val);
    Au result = __op(OPType__add, a, scale);
    drop(t_val);
    return result;
}

Au Au_member_ref(Au container, Au_t m) {
    if (m->member_type != AU_MEMBER_VAR) return null;
    return (Au)((char*)container + m->offset);
}

none mix_structs(Au_t type, Au dst, Au a, Au b, f64 t) {
    f64 inv = 1.0 - t;
    for (int i = 0; i < type->members.count; i++) {
        Au_t m = (Au_t)type->members.origin[i];
        if (m->member_type != AU_MEMBER_VAR) continue;
        Au_t  st = m->type;
        u32   o  = m->offset;
        char* pa = (char*)a   + o;
        char* pb = (char*)b   + o;
        char* po = (char*)dst + o;
             if (st == typeid(f32)) *(f32*)po = (f32)((f64)*(f32*)pa * inv + (f64)*(f32*)pb * t);
        else if (st == typeid(f64)) *(f64*)po = *(f64*)pa * inv + *(f64*)pb * t;
        else if (st == typeid(i32)) *(i32*)po = (i32)((f64)*(i32*)pa * inv + (f64)*(i32*)pb * t);
        else if (st == typeid(u32)) *(u32*)po = (u32)((f64)*(u32*)pa * inv + (f64)*(u32*)pb * t);
        else if (st == typeid(i16)) *(i16*)po = (i16)((f64)*(i16*)pa * inv + (f64)*(i16*)pb * t);
        else if (st == typeid(u16)) *(u16*)po = (u16)((f64)*(u16*)pa * inv + (f64)*(u16*)pb * t);
        else if (st == typeid(i8))  *(i8*) po = (i8) ((f64)*(i8*) pa * inv + (f64)*(i8*) pb * t);
        else if (st == typeid(u8))  *(u8*) po = (u8) ((f64)*(u8*) pa * inv + (f64)*(u8*) pb * t);
        else if (st == typeid(i64)) *(i64*)po = (i64)((f64)*(i64*)pa * inv + (f64)*(i64*)pb * t);
        else if (st == typeid(u64)) *(u64*)po = (u64)((f64)*(u64*)pa * inv + (f64)*(u64*)pb * t);
        else memcpy(po, (t < 0.5) ? (void*)pa : (void*)pb, st->typesize);
    }
}

Au __op(i32 optype, Au L, Au R) {
    Au_t type = isa(L);
    Au_t op_mem = null;
    Au_t search = type;
    while (search && !op_mem) {
        for (int i = 0; i < search->members.count; i++) {
            Au_t mem = (Au_t)search->members.origin[i];
            if (mem->operator_type == optype) {
                op_mem = mem;
                break;
            }
        }
        if (search->context == search) break;
        search = search->context;
    }
    static symbol op_names[] = { "?", "lmul", "ldiv", "lright", "lleft",
        "add", "sub", "mul", "div", "or", "and", "bitwise_or", "bitwise_and",
        "xor", "mod" };
    symbol op_name = (optype > 0 && optype <= 14) ? op_names[optype] : "?";
    assert(op_mem, "operator %s (%d) not found on type %s — generic Au operand; convert to a concrete type at the call site",
        op_name, optype, type->ident ? type->ident : "?");
    char*  type_bytes = (char*)type;
    void** ft = (void**)(type_bytes + offsetof(struct _Au_f, ft));
    typedef Au (*op_fn)(Au, Au);
    op_fn fn = (op_fn)ft[op_mem->member_index];
    return fn(L, R);
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
        else if (type == typeid(unichar)) len = sprintf(buf, "%c",  *(unichar*)a);
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
    ARef member_ptr   = (ARef)((cstr)a + m->offset);
    Au_t vtype        = isa(value);
    Au   vinfo        = head(value);

    if (m->type->is_struct) {
        // partial vectors are valid (e.g. gltf accessor min/max carries 1..4
        // elements depending on the accessor type, into a vec3f field) —
        // copy what fits and leave the remainder at its default.
        i64 vbytes = (i64)vtype->typesize * vinfo->count;
        i64 sz     = vbytes < (i64)m->type->typesize ? vbytes : (i64)m->type->typesize;
        memcpy(member_ptr, value, sz);
    } else if (m->type->is_enum || m->type->is_inlay || m->type->is_primitive) {
        Au_t ref = m->type->meta.a;
        verify(!m->type->is_struct || vtype == m->type ||
            vtype == ref,
            "%s: expected vmember_type (%s) to equal isa(value) (%s)",
            m->ident, ref->ident, vtype->ident);
        verify(!m->type->is_struct || vtype == m->type ||
            m->type->typesize == vtype->typesize * vinfo->count,
            "vector size mismatch for %s", m->ident);
        int sz = m->type->typesize < vtype->typesize ? 
            m->type->typesize : vtype->typesize;
        memcpy(member_ptr, value, sz);
    } else if ((Au)*member_ptr != value) {
        if (m->is_context) {
            *member_ptr = value;
        } else {
            Au old = *member_ptr;
            *member_ptr = Au_hold(value);
            Au_drop(old);
        }
    }
    Au_AF_set_member(a, m);
    return true;
}

// try to use this where possible
Au Au_member_object(Au a, Au_t m) {
    if (!(m->member_type == AU_MEMBER_VAR))
        return null; // we do this so much, that its useful as a filter in for statements

    bool is_primitive = (m->type->traits & AU_TRAIT_PRIMITIVE) != 0 ||
                        (m->type->traits & AU_TRAIT_STRUCT) != 0 ||
                        (m->type->traits & AU_TRAIT_ENUM) != 0;
    bool is_inlay     = (m->type->traits & AU_TRAIT_INLAY) != 0;
    Au result;
    ARef   member_ptr = (ARef)((cstr)a + m->offset);
    if (is_inlay || is_primitive) {
        result = alloc(m->type, 1, null, null, null, __FILE__, __LINE__, 0);
        memcpy(result, member_ptr, m->type->typesize);
        // issue with having a separate state with object header; it doesnt make enough sense, too
        //result = (Au)member_ptr;
    } else {
        result = *member_ptr;
    }
    return result;
}

string Au_cast_string(Au a) {
    Au_t type = isa(a);
    
    // convenient feature of new object abi
    if (type == typeid(Au_t_f)) {
        type = (Au_t)a;
        //return f(string, "wtf");
        if (type->is_pointer && !type->ident) {
            return f(string, "ref %s", type->src->ident);
        }
        return string(type->ident);
    } else if (!type) {
        verify(!a, "invalid type");
        return null;
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
    if (instanceof(t, string))     return ((string)a)->count;
    if (instanceof(t, collective)) return ((array) a)->count;
    if (t == typeid(cstr) || t == typeid(symbol) || t == typeid(cereal))
        return strlen((cstr)a);
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

Au formatter(Au_t type, bool print_info, handle ff, Au opt, int seq, symbol template, ...) {
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
        char cmd[8] = { *scan, *(scan + 1), 0 };
        int column_size = 0;
        int skip = 0;
        int f = cmd[1];
        /// column size formatting
        if (cmd[0] == '%' && (cmd[1] == '-' || isdigit(cmd[1]))) {
            /// register to fill this space
            for (int n = 1; n; n++) {
                if (cmd[n] && !isdigit(cmd[1 + n])) {
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
            bool success = !arg || isa_arg;
            verify(!arg || isa_arg, "unexpected null isa on object");
            if (isa_arg == typeid(Au_t_f) || isa_arg == (Au_t)arg) {
                if (!arg) {
                    a = string("null");
                } else {
                    Au_t au = (Au_t)arg;
                    if (au->is_pointer && !au->ident && au->src)
                        a = f(string, "ref %s", au->src->ident ? au->src->ident : "?");
                    else
                        a = string(au->ident ? au->ident : "(anon)");
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
    if (f && field && print_info) {
        char info[256];
        symbolic_logging = true;
        Au fvalue = get(log_funcs, (Au)field); // make get be harmless to map; null is absolutely fine identity wise to understand that
        int    l      = 0;
        string tname  = null;
        string fname  = field;
        static string  asterick = null;
        if (!asterick) asterick = hold(string("*"));
        bool   listen = fvalue ? cast(bool, fvalue) : false;
        if ((l = index_of(fname, "_")) > 1) {
            tname = mid(fname, 0, l);
            fname = mid(fname, l + 1, len(fname) - (l + 1));
            if (!listen && (contains(log_funcs, (Au)tname) || contains(log_funcs, (Au)fname)))
                listen = true;
        }
        if (!listen && !contains(log_funcs, (Au)asterick)) return null;
        // write type / function
        if (tname)
            sprintf(info, "\x1b[34m%s::%s [%i]\x1b[21G \x1b[0m", tname->chars, fname->chars, seq);
        else
            sprintf(info, "\x1b[34m%s [%i]\x1b[21G \x1b[0m", fname->chars, seq);

        // based on the number of columns left, we need to isue multiple prints starting at 30
        fwrite(info, strlen(info), 1, f);
    } else if (f && field) {
        write_ln = true;
    }

    if (f == stderr)
        fwrite("\033[1;33m", 7, 1, f);

    if (f) {
        // based on the number of columns left, we need to isue multiple prints starting at 30
        // write message
        /*
        string prepend = string(alloc, 32);

        char label[64];
        snprintf(label, sizeof(label), "[ %i ]", seq);
        append(prepend, label);
        concat(prepend, res);
        res = prepend;
        */

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
    if (type && is_struct((Au)type))
        return construct_with(type, (Au)res, null);

    return type ? (Au)
        ((Au_f*)type)->ft.with_cereal(alloc(type, 1, null, null, null, __FILE__, __LINE__, 0), (cereal) { .value = (cstr)res->chars }) :
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
    return Au_hash(f->key ? f->key : f->value);
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

// grow the vector's user-space `origin` buffer to hold at least `alloc` elements.
// the buffer is sized in `vdata_stride` units (scalar size for primitive vectors,
// pointer size for object vectors). updates a->alloc; leaves a->count alone.
static void vector_grow(vector a, sz alloc) {
    if (alloc <= a->alloc) return;
    sz   stride = vdata_stride((Au)a);
    u8*  prev   = (u8*)a->origin;
    u8*  data   = calloc(alloc, stride);
    if (prev && a->count > 0)
        memcpy(data, prev, a->count * stride);
    if (prev) free(prev);
    a->origin = (Au*)data;
    a->alloc  = alloc;
}

vector vector_with_i32(vector a, i32 count) {
    vector_grow(a, count);
    return a;
}

sz vector_len(vector a) {
    return a->count;
}

// we want hashmap to do less memory refs than map; also no ordering needed
none store_init(store a) {
    Au_t au = isa(a);
    int sz = sizeof(struct _store);
    if (a->hsize <= 0) a->hsize = 4096;
    if (a->hsize) a->hlist = (item*)calloc(a->hsize, sizeof(item));
}

none store_dealloc(store a) {
    for (int h = 0; h < a->hsize; h++) {
        item n = null;
        for (item i = a->hlist[h]; i; i = n) {
            n = i->next;
            drop(i);
        }
    }
    a->count = 0;
}

Au store_get(store a, Au key) {
    item f = a->hlist[((size_t)(uintptr_t)key >> 3) % a->hsize];
    for (item i = f; i; i = i->next) {
        if (i->key == key)
            return i->value;
    }
    return null;
}

none store_set(store a, Au key, Au val) {
    item *loc = &a->hlist[((size_t)(uintptr_t)key >> 3) % a->hsize];
    item f = *loc;
    for (item i = f; i; i = i->next) {
        if (i->key == key) {
            i->value = val;
            return;
        }
    }
    item i = item(key, key, value, val);
    if (f) {
        i->next = *loc;
        (*loc)->prev = i;
    }
    *loc = i;
    a->count++;
}

none store_rm(store a, Au key) {
    item *loc = &a->hlist[((size_t)(uintptr_t)key >> 3) % a->hsize];
    item  f   = *loc;
    for (item i = f; i; i = i->next) {
        if (i->key == key) {
            if (i->prev) {
                i->prev->next = i->next;
            } else
                *loc = i->next;
            
            if (i->next) {
                i->next->prev = i->prev;
            }
            
            a->count--;
            drop(i);
            return;
        }
    }
}


none map_init(map m) {
    if (m->hsize <= 0) m->hsize = 8;
    if (m->hsize) m->hlist = (item*)calloc(m->hsize, sizeof(item));
    m->assorted = true;
}

map map_copy(map m) {
    map a = map(hsize, 16, assorted, m->assorted, unmanaged, m->unmanaged);
    pairs(m, i) {
        set(a, i->key, i->value);
    }
    return a;
}

none map_dealloc(map m) {
    clear(m);
    free(m->hlist);
}

item map_lookup(map m, Au k) {
    if (!m->hlist) {
        u64 h = Au_hash(k);
        for (item i = m->first; i; i = i->next)
            if (i->h == h && (m->hash_only || au_key_compare(i->key, k) == 0))
                return i;
        return null;
    }
    item* hlist = m->hlist;
    Au_t k_type = isa(k);
    u64 h = Au_hash(k);
    i64 b = h % m->hsize;
    for (item i = hlist[b]; i; i = i->next) {
        if (i->h == h && (m->hash_only || au_key_compare(i->key, k) == 0))
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
        u64 h = Au_hash(k);
        i64 b = h % m->hsize;

        ((item*)m->hlist)[b] = i = hold(
            item(next, ((item*)m->hlist)[b],
                key, m->unmanaged ? k : hold(k), h, h));
        if (m->hsize == 0)
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

Au list_push(list a, Au e);

none map_set(map m, Au k, Au v) {
    if (!m->hlist) m->hlist = (item*)calloc(m->hsize, sizeof(item));
    item i = map_fetch(m, k);
    Au_t vtype = m->unmanaged ? null : isa(v);
    Au info = head(m);

    bool allowed = m->unmanaged || !m->last_type || m->last_type == vtype || m->assorted;
    verify(allowed,
        "unassorted map set to differing type: %s, previous: %s (%s:%i)",
        vtype->ident, m->last_type->ident, info->source, info->line);
    m->last_type = vtype;
    
    Au info2 = header(v);
    if (i->value) {
        if (i->value != v) {
            if (!m->unmanaged) drop(i->value);
            i->value = m->unmanaged ? v : Au_hold(v);
            item ref = (item)i->ref; // keep the FIFO value in sync on overwrite
            if (ref) {
                if (!m->unmanaged) drop(ref->value);
                ref->value = m->unmanaged ? v : Au_hold(v);
            }
        } else {
            return;
        }
    } else {
        i->value = m->unmanaged ? v : Au_hold(v);
    }

    bool in_fifo = i->ref != null;
    if (!in_fifo) {
        item ref = (item)list_push((list)m, v);
        ref->key = m->unmanaged ? k : Au_hold(k);
        ref->ref = (Au)i; // these reference each other
        i->ref = (Au)ref;
    }
}

none map_rm_item(map m, item i) {
    item ref = (item)i->ref;
    if (!m->unmanaged) {
        if (i->key)   drop(i->key);
        if (i->value) drop(i->value);
        if (ref) {
            if (ref->key)   drop(ref->key);
            if (ref->value) drop(ref->value);
        }
    }
    if (ref) {
        list_remove_item((list)m, ref);
        drop(ref);
    }
    drop(i);
}

none map_rm(map m, Au k) {
    u64  h    = Au_hash(k);
    i64  b    = h % m->hsize;
    item prev = null;
    if (m->hlist)
        for (item i = ((item*)m->hlist)[b]; i; i = i->next) {
            if (i->h == h && (m->hash_only || au_key_compare(i->key, k) == 0)) {
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
    if (m->hlist) {
        for (int b = 0; b < m->hsize; b++) {
            item cur = ((item*)m->hlist)[b];
            while (cur) {
                item next = cur->next;
                map_rm_item(m, cur);
                cur = next;
            }
        }
    }
}

sz map_len(map a) {
    return a->count;
}

// find out how these two get mixed up on import
Au map_getter_sz(map a, sz index) {
    assert(index >= 0 && index < a->count, "index out of range");
    item i = (item)list_value_by_index((list)a, (Au)_sz(index));
    return i ? i->value : null;
}

Au map_getter_Au(map a, Au key) {
    return map_get(a, key);
}

none map_setter(map a, Au key, Au value, i32 op) {
    if (op == OPType__assign) {
        map_set(a, key, value);
    } else {
        Au existing = map_get(a, key);
        verify(existing, "setter: key not found for compound assignment");
        map_set(a, key, value);
    }
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

string string_unescape(string input) {
    int input_len = len(input);
    cstr buf = calloc(input_len + 1, 1);
    int pos = 0;
    for (int i = 0; i < input_len; i++) {
        if (input->chars[i] == '\\' && i + 1 < input_len) {
            switch (input->chars[i + 1]) {
                case 'n':  buf[pos++] = '\n'; i++; break;
                case 'r':  buf[pos++] = '\r'; i++; break;
                case 't':  buf[pos++] = '\t'; i++; break;
                // octal escape: \ followed by 1-3 octal digits (e.g. \033 = ESC, \0 = nul)
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7': {
                    int oct = 0, d = 0;
                    while (d < 3 && i + 1 + d < input_len &&
                           input->chars[i + 1 + d] >= '0' &&
                           input->chars[i + 1 + d] <= '7') {
                        oct = oct * 8 + (input->chars[i + 1 + d] - '0');
                        d++;
                    }
                    buf[pos++] = (char)oct;
                    i += d;
                    break;
                }
                case '\\': buf[pos++] = '\\'; i++; break;
                case '\'': buf[pos++] = '\''; i++; break;
                case '"':  buf[pos++] = '"';  i++; break;
                case 'x': {
                    int h = 0;
                    char hex[3] = {0};
                    if (i + 2 < input_len && isxdigit(input->chars[i + 2]))
                        hex[h++] = input->chars[i + 2];
                    if (i + 3 < input_len && isxdigit(input->chars[i + 3]))
                        hex[h++] = input->chars[i + 3];
                    if (h > 0) {
                        buf[pos++] = (char)strtol(hex, NULL, 16);
                        i += 1 + h;
                    } else {
                        buf[pos++] = input->chars[i];
                    }
                    break;
                }
                default:   buf[pos++] = input->chars[i]; break;
            }
        } else {
            buf[pos++] = input->chars[i];
        }
    }
    buf[pos] = '\0';
    string res = string(chars, buf, ref_length, pos);
    free(buf);
    return res;
}

none  string_dealloc(string a) {
    free((cstr)a->chars);
}
num   string_compare(string a, string b) { if (a == b) return 0; if (!a || !b) return a ? 1 : -1; return strcmp(a->chars, b->chars); }
num   string_cmp    (string a, symbol b) { return strcmp(a->chars, b); }
bool  string_eq     (string a, symbol b) { return strcmp(a->chars, b) == 0; }

string string_copy(string a) {
    return string((symbol)a->chars);
}

bool inherits(Au_t src, Au_t check) {
    if (!src) return false;
    if (src->member_type == AU_MEMBER_VAR)
        src = src->src;
    while (src && src->is_alias && src->src && src->src != src)
        src = src->src;
    while (check && check->is_alias && check->src && check->src != check)
        check = check->src;
    while (src && src != typeid(Au)) {
        if (src == check) return true;
        if (src->context == src) break;
        src = src->context;
    }
    if (src == typeid(Au) && check == typeid(Au))
        return true;
    return src == check;
}

static inline char just_a_dash(char a) {
    return a == '-' ? '_' : a;
}

array string_split_parts(string a) {
    array  res = array(alloc, 32);
    cstr   s   = (cstr)a->chars;
    string buf = string(alloc, 256);

    while (*s) {
        if (*s == '{' && s[1] == '{') {
            append(buf, "{");
            s += 2;
        } else if (*s == '}' && s[1] == '}') {
            append(buf, "}");
            s += 2;
        } else if (*s == '{') {
            // flush literal
            if (len(buf) > 0) {
                push(res, (Au)ipart(is_expr, false, content, buf));
                buf = string(alloc, 256);
            }
            // scan expression
            s++;
            int  depth = 1;
            cstr expr_start = s;
            while (*s && depth > 0) {
                if      (*s == '{' && s[1] == '{') { s += 2; continue; }
                else if (*s == '{') depth++;
                else if (*s == '}') { depth--; if (depth == 0) break; }
                else if (*s == '\'' || *s == '"') {
                    char q = *s++;
                    while (*s && *s != q) {
                        if (*s == '\\' && s[1] == q) s += 2;
                        else s++;
                    }
                    if (*s) s++;
                    continue;
                }
                s++;
            }
            // unterminated: bail with null so the caller (aether) can report it
            // with the original .ag source location instead of halting here blind.
            if (depth != 0) return null;
            cstr expr_end = s;
            while (expr_end > expr_start && isspace(*(expr_end - 1))) expr_end--;
            string expr = string(chars, expr_start, ref_length, (sz)(expr_end - expr_start));
            push(res, (Au)ipart(is_expr, true, content, expr));
            s++;
        } else {
            char c[2] = { *s, 0 };
            append(buf, c);
            s++;
        }
    }
    if (len(buf) > 0)
        push(res, (Au)ipart(is_expr, false, content, buf));

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
                if (prev) {
                    append_count(res, prev, (sz)(s - prev));
                    prev = null;
                }
                append(res, "{");
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
            if (prev) {
                append_count(res, prev, (sz)(s - prev));
                prev = null;
            }
            append(res, "}");
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

i8    string_getter_i64(string a, i64 index) {
    if (index < 0)
        index += a->count;
    if (index >= a->count)
        return 0;
    return (i8)a->chars[index];
}

array string_split(string a, symbol sp) {
    cstr next = (cstr)a->chars;
    sz   slen = strlen(sp);
    array result = array(32);
    while (next) {
        cstr   n = strstr(next, sp);
        sz     vlen = n ? (sz)(n - next) : strlen(next);
        string v    = vlen ? string(chars, next, ref_length, vlen) : new(string, alloc, 1);
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
    free((char*)a->chars);
    a->chars = chars;
    a->alloc = alloc;
}

string string_mid(string a, num start, num len) {
    if (start < 0)
        start = a->count + start;
    if (start < 0)
        start = 0;
    if (len <= 0)
        return new(string, alloc, 1);
    if (start + len > a->count)
        len = a->count - start;
    return new(string, chars, &a->chars[start], ref_length, len);
}

none  string_reserve(string a, num extra) {
    if (a->alloc - a->count >= extra)
        return;
    string_alloc_sz(a, a->alloc + extra);
}

none  string_alloc_ahead(string a, i64 extra_space) {
    if (extra_space + a->count >= a->alloc)
        string_alloc_sz(a, (a->alloc << 1) + extra_space);
}

none  string_append(string a, symbol b) {
    if (!b) return;
    sz blen = strlen(b);
    alloc_ahead(a, blen);
    memcpy((cstr)&a->chars[a->count], b, blen);
    a->count += blen;
    a->h = 0; /// mutable operations must clear the hash value
    ((cstr)a->chars)[a->count] = 0;
}

none  string_append_count(string a, symbol b, i32 blen) {
    alloc_ahead(a, blen);
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
    string res = string(alloc, (a ? a->count : 0) + (b ? b->count : 0) + 1);
    concat(res, a);
    concat(res, b);
    return res;
}

none  string_push(string a, u32 b) {
    sz blen = 1;
    alloc_ahead(a, blen);
    memcpy((cstr)&a->chars[a->count], &b, 1);
    a->count += blen;
    a->h = 0; /// mutable operations must clear the hash value
    ((cstr)a->chars)[a->count] = 0;
}

none  string_concat(string a, string b) {
    if (b)
        string_append(a, b->chars);
}

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
    return a && a->count > 0;
}

sz string_cast_sz(string a) {
    return a->count;
}

cstr string_cast_cstr(string a) {
    return (cstr)a ? a->chars : null;
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
    a->role    = cstr_copy(a->role);
    a->content = cstr_copy(a->content);
}

none msg_dealloc(msg a) {
    free(a->role);
    free(a->content);
}

none string_init(string a) {
    cstr value = (cstr)a->chars;
    if (a->chars && a->alloc) return; // we're allocated already

    if (a->alloc)
        a->chars = (char*)calloc(1, 1 + a->alloc);
    if (value) {
        sz len = a->ref_length ? a->ref_length : strlen(value);
        if (!a->alloc)
            a->alloc = len;
        if (a->chars == value)
            a->chars = (char*)calloc(1, len + 1);
        //char buf[256];
        //snprintf(buf, 256, "string len = %i, chars buf was %p (%s)\n", (int)len, value, value);
        //puts(buf);
        memcpy((cstr)a->chars, value, len);
        ((cstr)a->chars)[len] = 0;
        a->count = len;
    }
}


string string_with_unichar(string a, unichar value) {
    char buf[4];
    int  n = 0;
    if      (value < 0x80)     { buf[n++] = value; }
    else if (value < 0x800)    { buf[n++] = 0xC0 | (value >> 6);
                                 buf[n++] = 0x80 | (value & 0x3F); }
    else if (value < 0x10000)  { buf[n++] = 0xE0 | (value >> 12);
                                 buf[n++] = 0x80 | ((value >> 6) & 0x3F);
                                 buf[n++] = 0x80 | (value & 0x3F); }
    else if (value <= 0x10FFFF){ buf[n++] = 0xF0 | (value >> 18);
                                 buf[n++] = 0x80 | ((value >> 12) & 0x3F);
                                 buf[n++] = 0x80 | ((value >> 6) & 0x3F);
                                 buf[n++] = 0x80 | (value & 0x3F); }
    else                       { a->count = 0; a->chars = NULL; return a; }
    a->count = n;
    a->chars = calloc(n + 1, 1);
    memcpy((cstr)a->chars, buf, n);
    return a;
}

string unicode_char(i32 value) {
    string s = string();
    string_with_unichar(s, value);
    return s;
}

string string_with_i64(string a, i64 value) {
    a->alloc = 64;
    a->chars = calloc(a->alloc, 1);
    a->count = snprintf(a->chars, 64, "%lli", value);
    return a;
}

string string_with_f64(string a, f64 value) {
    a->alloc = 64;
    a->chars = calloc(a->alloc, 1);
    a->count = snprintf(a->chars, 64, "%g", value);
    return a;
}

string string_with_cstr(string a, cstr value) {
    a->count = value ? strlen(value) : 0;
    a->alloc = a->count + 1;
    a->chars = calloc(a->alloc, 1);
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
    Au b = alloc(type, f->count, null, null, null, __FILE__, __LINE__, 0);
    memcpy(b, a, f->au->typesize * f->count);
    Au_hold_members(b);
    return b;
}

none Au_free(Au a) {
    Au       aa = header(a);
    //char ch = aa->au->ident[0];

    /*
    if (ch == 'V') {
        if (aa->source)
            printf("freeing %s:%i (%s)\n", aa->source, aa->line, aa->au->ident);
        //return;
    }
    */

    // C-imported types are flat memory — no init/dealloc/hold/drop vtable.
    // just free the allocation and skip the chain walk entirely.
    bool     is_c = aa->au && ((Au_t)aa->au)->is_c;
    // reference holder (`new Type[N]`): the held type's dealloc chain does
    // NOT apply to the holder's raw buffer; slots are user-managed refs.
    bool     is_holder = (aa->iflags & AU_IF_HOLDER) != 0;
    Au_f*  type = (Au_f*)aa->au;
    none* prev = null;
    Au_f*   cur = (is_c || is_holder || ((Au_t)aa->au)->is_struct) ? null : type;
#ifndef NDEBUG
    //if (aa->line == 894)
    //    return;
    //if (aa->source)
    //    printf("Au_free type=%s source=%s:%i seq=%i\n", aa->au->ident, aa->source, aa->line, aa->sequence);
#endif

    while (cur) {
        if (prev != cur->ft.dealloc) {
            cur->ft.dealloc(a);
            prev = cur->ft.dealloc;
        }
        if (cur == &Au_Au_i.type)
            break;
        cur = (Au_f*)cur->context;
    }
    
#ifndef NDEBUG
    if (tracing)
        for (int i = 0; i < tracing_count; i++)
            if (tracing[i] == a)
                tracing[i] = null;

    if (--total_objects < 0)
        printf("total_objects < 0\n");
    
    if (--type->global_count < 0)
        printf("global_count < 0 for type %s\n", type->ident);
    
    aa->refs = -8888;
    //printf("freeing %s %s:%i\n", type->ident, aa->source, aa->line);

    free(aa);
#else
    free(aa);
#endif

}



/// binding works with delegate callback registration efficiently to 
/// avoid namespace collisions, and allow enumeration interfaces without override and base Au boilerplate
callback Au_binding(Au a, Au target, bool required, Au_t rtype, Au_t arg_type, symbol id, symbol name) {
    Au_t self_type   = isa(a);
    Au_t target_type = isa(target);
    bool inherits     = instance_of(target, self_type) != null;
    string method     = f(string, "%s%s%s", id ? id : 
        (!inherits ? self_type->ident : ""), (id || !inherits) ? "_" : "", name);
    Au_t m  = find_member(target_type, method->chars, AU_MEMBER_FUNC, 0, true);
    verify(!required || m, "bind: required method not found: %o", method);
    if (!m) return null;
    callback f       = (callback)m->value;
    verify(f, "expected method address");
    verify(m->args.count  == 2, "%s: expected method address with instance, and arg*", name);
    Au_t first_arg_node = ((Au_t)m->args.origin[1]);
    Au_t first_arg_type = first_arg_node->type;
    //verify(!arg_type || first_arg_type == (Au)arg_type, "%s: expected arg type: %s", name, arg_type->ident);
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
    return f->scalar ? f->scalar : (Au_t)f->au;
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

Au list_push(list a, Au e) {
    item n = (item)Au_hold((Au)item());
    n->value = a->unmanaged ? e : Au_hold(e);
    if (a->last) {
        a->last->next = n;
        n->prev       = a->last;
    } else {
        a->first      = n;
    }
    a->last = n;
    a->count++;
    return (Au)n;
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
    // `item.value` is a WEAK prop — constructing with item(value, e) does NOT retain e,
    // so the inserted element would be freed the moment the caller's local drops (e.g.
    // the next keystroke then slices a dangling string buffer). hold it explicitly, the
    // same way list_push does.
    item n = (item)Au_hold((Au)item());
    n->value = a->unmanaged ? e : Au_hold(e);
    if (!found) {
        n->next = a->first;
        if (a->first)
            a->first->prev = n;
        a->first = n;
        if (!a->last)
             a->last = n;
    } else {
        // splice n in AFTER found: n takes found->next; only found and the old
        // successor are relinked. (the previous code zeroed n->next — truncating
        // everything past found — and rewrote found->prev->next, corrupting the
        // node BEFORE found. that desynced the list and crashed the next read,
        // e.g. typing on any line but the first.)
        n->prev = found;
        n->next = found->next;
        if (found->next)
            found->next->prev = n;
        found->next = n;
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
        Au_t m = find_member(ai_t, "compare", AU_MEMBER_FUNC, 0, true);
        for (item ai = a->first, bi = b->first; ai; ai = ai->next, bi = bi->next) {
            num   v  = ((num(*)(Au,Au))(m->value))((Au)ai, (Au)bi);
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
    return (Au)l;
}

Au list_value_by_index(list a, Au at_index) {
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


bool Au_is_meta(Au a) {
    Au_t t = isa(a);
    return t->meta.a != null;
}

bool Au_is_meta_compatible(Au a, Au b) {
    Au_t t = isa(a);
    if (t->meta.a) {
        Au_t bt = isa(b);
        if (inherits(bt, t->meta.a))
            return true;
        if (t->meta.b && inherits(bt, (Au_t)t->meta.b))
            return true;
    }
    return false;
}

Au Au_vrealloc(Au a, sz alloc) {
    Au   i = header(a);
    if (alloc > i->alloc) {
        Au_t type = vdata_type(a);
        sz  size  = vdata_stride(a);
        u8* data  = calloc(alloc, size);
        u8* prev  = (u8*)i->data;
        memcpy(data, prev, i->count * size);
        i->data  = (Au)data;
        i->alloc = alloc;
        if ((Au)prev != a) free(prev);
    }
    return i->data;
}


none vector_init(vector a) {
    Au f = head(a);
    a->count   = 0;
    f->scalar  = (f->au && f->au->meta.a) ? meta_index((Au)a, 0)
                : a->au ? (Au_t)a->au : typeid(i8);
    f->data_shape = hold(a->data_shape);
    verify(f->scalar, "scalar not set");
    if (f->data_shape)
        a->alloc = shape_total(f->data_shape);
    if (a->alloc > 0)
        vector_grow(a, a->alloc);
}

vector vector_with_path(vector a, path file_path) {
    Au f = head(a);
    f->scalar = typeid(i8);
    verify(exists(file_path), "file %o does not exist", file_path);
    FILE* ff = fopen(cstring(file_path), "rb");
    fseek(ff, 0, SEEK_END);
    sz flen = ftell(ff);
    fseek(ff, 0, SEEK_SET);

    vector_grow(a, flen);
    a->count = flen;
    size_t n = fread(a->origin, 1, flen, ff);
    verify(n == flen, "could not read file: %o", a);
    fclose(ff);
    return a;
}

ARef vector_vget(vector a, num index) {
    num location = index * a->au->typesize;
    i8* arb = (i8*)a->origin;
    return (ARef)&arb[location];
}

none vector_vset(vector a, num index, ARef element) {
    num location = index * a->au->typesize;
    i8* arb = (i8*)a->origin;
    memcpy(&arb[location], element, a->au->typesize);
}

Au vector_resize(vector a, sz size) {
    vector_grow(a, size);
    a->count = size;
    return (Au)a->origin;
}

Au vector_reallocate(vector a, sz size) {
    vector_grow(a, size);
    return (Au)a->origin;
}

none vector_vconcat(vector a, ARef any, num count) {
    if (count <= 0) return;
    Au_t type = vdata_type((Au)a);
    if (a->alloc < a->count + count)
        vector_grow(a, (a->alloc << 1) + 32 + count);

    u8* ptr  = (u8*)a->origin;
    i64 size = vdata_stride((Au)a);
    memcpy(&ptr[a->count * size], any, size * count);
    a->count += count;
    Au f = head(a);
    if (f->data_shape)
        f->data_shape->data[f->data_shape->count - 1] = a->count;
}

none vector_vpush(vector a, Au any) {
    vector_vconcat(a, (ARef)any, 1);
}

num abso(num i) {
    return (i < 0) ? -i : i;
}

vector vector_vslice(vector a, num from, num to) {
    Au   f      = head(a);
    num  count  = (1 + abso(from - to));
    Au   res    = alloc((Au_t)f->au, 1, null, null, null, __FILE__, __LINE__, 0);
    vector vres = (vector)res;
    Au_initialize(res);
    vector_grow(vres, count);
    u8* src    = (u8*)a->origin;
    u8* dst    = (u8*)vres->origin;
    i64 stride = vdata_stride((Au)a);
    if (from < to)
        memcpy(dst, &src[from * stride], count * stride);
    else
        for (int i = from; i > to; i--, dst += stride)
            memcpy(dst, &src[i * stride], stride);
    vres->count = count;
    return vres;
}

sz vector_count(vector a) {
    return a->count;
}

define_class(vector, collective);


Au subprocedure_invoke(subprocedure a, Au arg) {
    Au(*addr)(Au, Au, Au) = a->addr;
    return addr(a->target, arg, a->ctx);
}


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

// backslash-escape shell metacharacters so the path survives as a single argument
// (matches what a terminal inserts when you drag a file onto it)
string path_shell_escape(path a) {
    cstr s   = (cstr)a->chars;
    num  len = a->count;
    if (!s) return string("");
    cstr out = calloc(len * 2 + 1, 1);
    num  pos = 0;
    for (num i = 0; i < len; i++) {
        char c = s[i];
        if (c == ' '  || c == '\t' || c == '\\' || c == '\'' || c == '"' ||
            c == '('  || c == ')'  || c == '&'  || c == ';'  || c == '|'  ||
            c == '<'  || c == '>'  || c == '$'  || c == '`'  || c == '*'  ||
            c == '?'  || c == '['  || c == ']'  || c == '#'  || c == '~'  ||
            c == '!'  || c == '{'  || c == '}'  || c == '^')
            out[pos++] = '\\';
        out[pos++] = c;
    }
    out[pos] = '\0';
    string res = string((symbol)out);
    free(out);
    return res;
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

bool path_remove(path a) {
    return unlink(a->chars) == 0;
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
    a->chars = copy_cstr((s && s->chars) ? (cstr)s->chars : "");
    a->count   = strlen(a->chars);
    return a;
}

path path_with_tokens(path a, tokens s) {
    return path_with_string(a, (string)s);
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
    if (contains(visit, (Au)k)) return null;;
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
                path lf = _path_latest_modified(subdir, (ARef)&sub_latest, visit);
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

    if (path_is_dir(a)) {
        i64  mtime  = 0;
        path latest = latest_modified(a, (ARef)&mtime);
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

// the SINGLE shared loader for a silver --format (.f) map. returns an array of fmt_file
// (one per source file), each carrying its canonical path, source mtime, and Syntax
// tokens. used by BOTH silver (incremental: skip files whose mtime is unchanged) and
// orbiter (syntax coloring) — there is no second copy of this parser anywhere.
// layout (LE): u32 magic('SFMT') u32 ver(=2);  section: u32 0xC0DEFACE u32 path_len,
//   path bytes, i64 mtime, u32 token_count, token_count*{u32 line,col,len,syntax};
//   end: u32 0.
array path_read_format(path a) {
    array out = array(alloc, 16);
    FILE* f = fopen((cstr)a->chars, "rb");
    if (!f) return out;
    u32 magic = 0, ver = 0;
    if (fread(&magic, 4, 1, f) != 1 || fread(&ver, 4, 1, f) != 1 ||
        magic != 0x53464D54u || ver != 2u) { fclose(f); return out; }
    for (;;) {
        u32 tag = 0;
        if (fread(&tag, 4, 1, f) != 1 || tag != 0xC0DEFACEu) break;  // 0 = clean end
        u32 plen = 0;
        if (fread(&plen, 4, 1, f) != 1) break;
        char* p = malloc((size_t)plen + 1);
        if (fread(p, 1, plen, f) != plen) { free(p); break; }
        p[plen] = 0;
        i64 mt = 0;  u32 ntok = 0;
        if (fread(&mt, 8, 1, f) != 1 || fread(&ntok, 4, 1, f) != 1) { free(p); break; }
        fmt_file ff = fmt_file(
            source, string(p), mtime, mt, tokens, array(alloc, ntok ? ntok : 1));
        free(p);
        bool ok = true;
        for (u32 t = 0; t < ntok; t++) {
            u32 rec[4];
            if (fread(rec, 4, 4, f) != 4) { ok = false; break; }
            push(ff->tokens, (Au)fmt_token(
                line,   (num)rec[0], column, (num)rec[1],
                length, (num)rec[2], syntax, (Syntax)rec[3]));
        }
        push(out, (Au)ff);
        if (!ok) break;
    }
    fclose(f);
    return out;
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
    result->chars  = rpath ? cstr_copy(rpath) : copy_cstr("");
    result->count    = strlen(result->chars);
    return result;
}

path path_directory(path a) {
    path  result  = new(path);
    char* cp      = cstr_copy(a->chars);
    char* temp    = dirname(cp);
    result->chars = cstr_copy(temp);
    result->count   = strlen(result->chars);
    free(cp);
    return result;
}

path path_parent_dir(path a) {
    int len = strlen(a->chars);
    for (int i = len - 2; i >= 0; i--) { // -2 because we dont mind the first
        char ch = a->chars[i];
        if  (ch == '/') {
            // slash at 0: ref_length 0 means strlen — would copy a whole
            if (i == 0)
                return new(path, chars, "/");
            string trim = new(string, chars, a->chars, ref_length, i);
            return new(path, chars, trim->chars);
        }
    }
    return null;
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

Exists resource_exists(Au o);

path path_share_path() {
    path exe    = path_self();
    path parent = path_parent_dir(exe); // verify this folder is bin?
    string n = stem(exe);
    path res    = form(path, "%o/../share/%o/", parent, n);
    if (dir_exists("%o", res))
        return res;
    return null;
}

// disposable per-app cache, scoped by name: safe to delete anytime.
// linux ~/.cache/<app> (XDG), mac ~/Library/Caches, win LOCALAPPDATA.
path path_cache(cstr app) {
    string dir = null;
#ifdef _WIN32
    cstr la = getenv("LOCALAPPDATA");
    if (la && strlen(la)) dir = f(string, "%s/%s/cache", la, app);
#elif defined(__APPLE__)
    cstr home = getenv("HOME");
    if (home && strlen(home))
        dir = f(string, "%s/Library/Caches/%s", home, app);
#else
    cstr xdg = getenv("XDG_CACHE_HOME");
    if (xdg && strlen(xdg))
        dir = f(string, "%s/%s", xdg, app);
    else {
        cstr home = getenv("HOME");
        if (home && strlen(home))
            dir = f(string, "%s/.cache/%s", home, app);
    }
#endif
    if (!dir) return null;
    path p = f(path, "%o", dir);
    make_dir(p);
    return p;
}

// app-private mutable state, scoped by name (usually the module).
// share is static build resources; this is the runtime home:
// linux ~/.local/state/<app> (XDG), mac Application Support, win
// LOCALAPPDATA. created on first use.
path path_storage(cstr app) {
    string dir = null;
#ifdef _WIN32
    cstr la = getenv("LOCALAPPDATA");
    if (la && strlen(la)) dir = f(string, "%s/%s", la, app);
#elif defined(__APPLE__)
    cstr home = getenv("HOME");
    if (home && strlen(home))
        dir = f(string, "%s/Library/Application Support/%s", home, app);
#else
    cstr xdg = getenv("XDG_STATE_HOME");
    if (xdg && strlen(xdg))
        dir = f(string, "%s/%s", xdg, app);
    else {
        cstr home = getenv("HOME");
        if (home && strlen(home))
            dir = f(string, "%s/.local/state/%s", home, app);
    }
#endif
    if (!dir) return null;
    path p = f(path, "%o", dir);
    make_dir(p);
    return p;
}

bool path_is_symlink(path p) {
    struct stat st;
    return lstat(p->chars, &st) == 0 && S_ISLNK(st.st_mode);
}

path path_normalize(path p) {
    const char *s = p->chars;
    const int is_abs = s[0] == '/';

    const char *parts[256];
    int depth = 0;

    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;

        const char *start = s;
        while (*s && *s != '/') s++;

        size_t len = s - start;

        if (len == 1 && start[0] == '.') {
            continue;
        }
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) depth--;
            continue;
        }

        parts[depth++] = strndup(start, len);
    }

    // rebuild
    char buf[4096];
    char *out = buf;

    if (is_abs) *out++ = '/';

    for (int i = 0; i < depth; i++) {
        size_t len = strlen(parts[i]);
        memcpy(out, parts[i], len);
        out += len;
        if (i + 1 < depth) *out++ = '/';
        free((void *)parts[i]);
    }

    *out = 0;
    return path(buf);
}

path path_resolve(path p) {
    char buf[4096];
    ssize_t len = readlink(p->chars, buf, sizeof(buf) - 1);
    if (len == -1) return hold(p);
    buf[len] = '\0';
    return path(buf);
}
 
bool path_eq(path a, symbol b) {
    struct stat sa, sb;
    int ia = stat(a->chars, &sa);
    int ib = stat(b,        &sb);
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

// the cwd the process launched in. silver-host records it in SILVER_STARTUP before
// it cd's to the share dir (host is plain C, can't reach this global); a direct run
// has it captured in engage() before its own cd. falls back to the live cwd.
path path_startup() {
    symbol e = getenv("SILVER_STARTUP");
    if (e && *e) return path(e);
    return startup_cwd_ ? startup_cwd_ : path_cwd();
}

Exists resource_exists(Au o) {
    Au_t type = isa(o);
    path  f = null;
    if (type == typeid(string))
        f = cast(path, (string)o);
    else if (type == typeid(path))
        f = (path)o;
    assert(f, "type not supported");
    bool is_dir = is_dir(f);
    bool r = path_exists(f);
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

Au parse(Au_t schema, cstr s, ctx context);

// ---- .agi : tab-indented JSON (no braces) ----------------------------------
// dedicated parser (parse_agi, defined below after parse_object). a `key: Type`
// line whose next non-blank line is deeper opens that Type's indented body; a
// dedent closes it. leaf `key: value` lines parse their value via parse_object.
// tabs only for indent; a blank line does NOT close a block.
// `target` is optional: when non-null its fields are SET (member_set), recursing
// into existing sub-objects; when null a fresh object is constructed from schema.
Au parse_agi(Au_t schema, cstr s, Au target);

static int agi_peek_indent(cstr scan) {
    while (*scan) {
        cstr ls = scan; int ind = 0;
        while (*ls == '\t') { ind++; ls++; }
        while (*ls == ' ')  ls++;
        if (*ls == '\n') { scan = ls + 1; continue; }   // blank line — skip
        if (*ls == '#') {                               // comment line — skip
            while (*ls && *ls != '\n') ls++;
            scan = (*ls == '\n') ? ls + 1 : ls;
            continue;
        }
        if (*ls == 0)     return -1;
        return ind;
    }
    return -1;
}

Au path_load(path a, Au_t type, ctx context) {
    if (path_is_dir(a)) return null;
    if (type == typeid(array))
        return (Au)read_lines(a);
    FILE* f = fopen(a->chars, "rb");
    if (!f) return null;
    bool is_obj = type && !(type->traits & AU_TRAIT_PRIMITIVE);
    fseek(f, 0, SEEK_END);
    sz flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    string str = string(alloc, flen + 1);
    size_t n = fread((cstr)str->chars, 1, flen, f);
    fclose(f);
    assert(n == flen, "could not read enough bytes");
    str->count   = flen;
    if (type == typeid(string))
        return (Au)str;
    if (is_obj) {
        Au obj = is_ext(a, "agi")
            ? parse_agi((Au_t)type, (cstr)str->chars, null)      // tab-indented (fresh)
            : parse    ((Au_t)type, (cstr)str->chars, context);  // braced json
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
    bool own = is_dir(to);
    path f_to = own ? f(path, "%o/%o", to, filename(from)) : to;
    FILE *src = fopen(cstring(from), "rb");
    FILE *dst = fopen(cstring(f_to), "wb");
    if (own) drop(f_to);
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
            // sockets are listable files too (app-bus presence lives on them)
            if (S_ISREG(statbuf.st_mode) || S_ISSOCK(statbuf.st_mode)) {
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
    pthread_cond_broadcast(&m->mtx->cond);
}

none mutex_cond_signal(mutex m) {
    pthread_cond_signal(&m->mtx->cond);
}

none mutex_cond_wait(mutex m) {
    pthread_cond_wait(&m->mtx->cond, &m->mtx->lock);
}

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
        map ma = (map)a;
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
            concat(res, json((Au)string(mem->ident)));
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
            else if (*scan == '"')  push(res, '"');
            else if (*scan == '\'') push(res, '\'');
            else if (*scan == '\\') push(res, '\\');
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
    return (context && delim == '\'') ? interpolate(res, (Au)context) : res;
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
            Au_t type = find_type(sym->chars, null);
            verify(type, "type not found: %o", sym);

            if (type->traits & AU_TRAIT_ENUM) {
                // its possible we could reference a context variable within this enum [ value-area ]
                // in which case, we could effectively look it up
                Au evalue = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                string enum_symbol = parse_symbol(scan, &scan, context);
                verify(enum_symbol, "enum symbol expected");
                verify(*scan == ']', "expected ']' after enum symbol");
                scan++;
                Au_t e = find_member(
                    type, enum_symbol->chars, AU_MEMBER_ENUMV, 0, false);
                verify(e, "enum symbol %o not found in type %s", enum_symbol, type->ident);
                memcpy(evalue, e->value, e->type->typesize);

                res = evalue;
            } else if (type->traits & AU_TRAIT_STRUCT) {
                Au svalue = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
                for (num i = 0; i < type->members.count; i++) {
                    Au_t m = (Au_t)type->members.origin[i];
                    if (!(m->member_type == AU_MEMBER_VAR)) continue;
                    Au f = (Au)((cstr)svalue + m->offset);
                    Au r = parse_object(scan, null, null, &scan, context);
                    verify(r && isa(r) == m->type, "type mismatch while parsing struct %s:%s (read: %s, expect: %s)",
                        type->ident, m->ident, !r ? "null" : (isa(r))->ident, m->type->ident);
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
                    scan++;
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
            ? schema : typeid(string), (Au)js, context);
    }
    else if (*scan == '{') { /// Type will make us convert to the Au, from map, and set is_map back to false; what could possibly get crossed up with this one
        if (sym) {
            verify(!schema || schema == typeid(Au) || eq(sym, schema->ident),
                "expected type: %s, found %o", schema->ident, sym);

            if (!schema) {
                schema = find_type(sym->chars, null);
                verify(schema, "type not found: %o", sym);
            }

            if (schema == typeid(Au)) {
                schema = find_type(sym->chars, null);
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
                    find_member(use_schema, name->chars, AU_MEMBER_VAR, 0, true);
            }

            if (!json_type && !mem && !is_map && !context) {
                // forward-compat: an unknown field is a warning, not a parse
                // abort (returning null here also left *remainder null from
                // entry -> the caller dereferenced it: SIGSEGV mid-file).
                // parse the value generically and discard it.
                printf("parse_object: skipping unknown field '%s' in type %s\n",
                    name ? name->chars : "?", use_schema ? use_schema->ident : "?");
                if (*scan == ':') {
                    scan = ws(&scan[1]);
                    parse_object(scan, null, null, &scan, context);
                    if (!scan) return null;
                    if (*scan == ',') { scan++; continue; }
                    else if (*scan != '}') return null;
                    continue;
                }
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
            Au value = parse_object(scan,
                (Au_t)(mem ? mem->type : (is_map ? meta_type : null)),
                mem ? mem->meta.a : null, &scan, context);
            // a failed inner parse leaves scan null (set at its entry, never
            // reset on early-return paths) — bail instead of dereferencing
            if (!scan)
                return null;

            if (set_ctx && value)
                set((map)context, (Au)name, (Au)value);

            if (json_type) {
                string type_name = (string)value;
                use_schema = find_type(type_name->chars, null);
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
        
        res = construct_with(use_schema, (Au)props, context); // makes a bit more sense to implement required here
        if (use_schema == typeid(map))
            hold(props);
    } else {
        res = (context && sym) ? get(context, (Au)sym) : null;
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

// ---- .agi : tab-indented object parser -------------------------------------
// parse the block of lines at exactly `indent` into an object of `schema` (or a
// generic map). a member whose next line is deeper is a nested typed block (the
// rest of its line names the Type, else the member's declared type / a map); a
// leaf parses its value via parse_object. advances *rem past the block.
static bool agi_type_is_array(Au_t t) {
    for (Au_t w = t; w; w = w->src) {
        if (w == typeid(array)) return true;
        if (w == w->src) break;
    }
    return false;
}

static Au parse_agi_block(cstr scan, int indent, Au_t schema, Au_t meta, cstr* rem, Au target) {
    // when `target` is set we SET members directly onto it (its type drives member
    // lookup); otherwise we build a props map and construct a fresh object/map.
    Au_t use_schema = target ? isa(target) : (schema ? schema : typeid(map));
    bool is_map     = use_schema == typeid(map);
    map  props      = target ? null : map(hsize, 16, assorted, true);

    while (*scan) {
        cstr ls = scan; int ind = 0;
        while (*ls == '\t') { ind++; ls++; }
        while (*ls == ' ')  ls++;                         // tolerate trailing spaces
        if (*ls == '\n') { scan = ls + 1; continue; }   // blank line
        if (*ls == 0)     break;
        if (*ls == '#') {                                 // comment line — to line break
            while (*ls && *ls != '\n') ls++;
            scan = (*ls == '\n') ? ls + 1 : ls;
            continue;
        }
        if (ind < indent) break;                          // dedent ends the block

        // key — unquoted symbol or quoted string
        cstr   ks  = ls;
        string key = (*ks == '"' || *ks == '\'')
            ? parse_json_string(ks, &ks, null)
            : parse_symbol(ks, &ks, null);
        verify(key, "agi: expected key");
        while (*ks == ' ' || *ks == '\t') ks++;
        verify(*ks == ':', "agi: expected ':' after key %o", key);
        ks++;                                             // past ':'
        while (*ks == ' ' || *ks == '\t') ks++;          // ks = start of value

        Au_t mem      = is_map ? null : find_member(use_schema, key->chars, AU_MEMBER_VAR, 0, true);
        Au_t mem_type = mem ? mem->type   : (is_map ? meta : null);
        Au_t mem_meta = mem ? mem->meta.a : null;

        // content end: stop at newline or an unquoted '#' (comment runs to line break)
        cstr re = ks; char q = 0;
        while (*re && *re != '\n') {
            if (q)      { if (*re == q) q = 0; }
            else if (*re == '\'' || *re == '"') q = *re;
            else if (*re == '#') break;
            re++;
        }
        cstr eol = re; while (*eol && *eol != '\n') eol++; // past any comment text
        cstr after = (*eol == '\n') ? eol + 1 : eol;       // start of next line
        int  nx    = agi_peek_indent(after);

        Au value = null;
        if (nx > indent) {
            bool arr_member = mem &&
                (agi_type_is_array(mem->type) || agi_type_is_array(mem->src));
            if (arr_member) {
                // keyed blocks -> array elements (string_agi emits items keyed
                // by their name/ident); insertion order is the array order
                Au_t elem = mem->meta.b ? (Au_t)mem->meta.b : mem->meta.a;
                cstr brem = after;
                map  mm   = (map)parse_agi_block(after, nx, typeid(map), elem, &brem, null);
                array arr = array(32);
                pairs(mm, ii) {
                    Au e = ii->value;
                    if (e && instanceof(ii->key, string)) {
                        // restore the key into name/ident when the block left it unset
                        Au_t nm = find_member(isa(e), "name", AU_MEMBER_VAR, 0, true);
                        if (!nm) nm = find_member(isa(e), "ident", AU_MEMBER_VAR, 0, true);
                        if (nm && nm->type == typeid(string) && !Au_get_property(e, nm->ident))
                            member_set(e, nm, ii->key);
                    }
                    push(arr, e);
                }
                value = (Au)arr;
                scan  = brem;
            } else {
                // nested block — the rest of the line (if any) names the Type. always
                // constructs fresh (target == null), then it's set onto the parent.
                Au_t btype = mem_type;
                cstr ts = ks;
                if (ts < re && *ts != ' ' && *ts != '\t') {
                    string tn = parse_symbol(ts, &ts, null);
                    if (tn) { Au_t ft = find_type(tn->chars, null); if (ft) btype = ft; }
                }
                cstr brem = after;
                value = parse_agi_block(after, nx, btype, mem_meta, &brem, null);
                scan  = brem;
            }
        } else {
            // leaf value. structured starters (quote / [ / { / number / -) and the
            // keywords true|false|null go through parse_object; a bare word is taken
            // as an unquoted string (json-in-value position can't, but .agi wants it).
            cstr v  = ks;
            cstr ve = re; while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t')) ve--;
            int  vlen = (int)(ve - v);
            char c0   = vlen ? *v : 0;
            bool structured = c0 == '"' || c0 == '\'' || c0 == '[' || c0 == '{' ||
                              c0 == '-' || (c0 >= '0' && c0 <= '9');
            bool keyword = (vlen == 4 && strncmp(v, "true",  4) == 0) ||
                           (vlen == 5 && strncmp(v, "false", 5) == 0) ||
                           (vlen == 4 && strncmp(v, "null",  4) == 0);
            // digit-leading text for an object-typed member (shape 32x32x1)
            // is a constructor string, not a number
            if ((c0 == '-' || (c0 >= '0' && c0 <= '9')) && mem_type &&
                mem_type != typeid(Au) && mem_type != typeid(string) &&
                !(mem_type->traits & AU_TRAIT_PRIMITIVE) &&
                !(mem_type->traits & AU_TRAIT_ENUM))
                structured = false;
            if (vlen == 0) {
                value = null;
            } else if (c0 == '[') {
                // agi arrays allow bare symbols: [ inL, inR ]
                Au_t elem = mem ? (mem->meta.b ? (Au_t)mem->meta.b : mem->meta.a) : meta;
                array arr = array(32);
                cstr  p   = v + 1;
                while (p < ve && *p != ']') {
                    while (p < ve && (*p == ' ' || *p == '\t' || *p == ',')) p++;
                    if (p >= ve || *p == ']') break;
                    Au item = null;
                    if (*p == '\'' || *p == '"')
                        item = (Au)parse_json_string(p, &p, null);
                    else {
                        cstr w = p;
                        while (p < ve && *p != ',' && *p != ']') p++;
                        cstr we = p; while (we > w && (we[-1] == ' ' || we[-1] == '\t')) we--;
                        int wl = (int)(we - w);
                        if (wl > 0) {
                            string ws2 = string(alloc, wl + 1);
                            memcpy((cstr)ws2->chars, w, wl);
                            ((cstr)ws2->chars)[wl] = 0;
                            ws2->count = wl;
                            item = (Au)ws2;
                        }
                    }
                    if (item)
                        push(arr, (elem && elem != typeid(none) && elem != typeid(Au) && elem != typeid(string))
                            ? (Au)construct_with(elem, item, null) : item);
                }
                value = (Au)arr;
            } else if (structured || keyword) {
                cstr vrem = v;
                value = parse_object(v, mem_type, mem_meta, &vrem, null);
            } else if (mem && mem->src == typeid(array)) {
                // array member with a bare value: split on WHITESPACE ONLY into an
                // array of the element type (the member's meta-b). commas are NOT
                // separators — a build-arg token may contain them (-Wl,-rpath,...).
                Au_t elem = (Au_t)mem->meta.b;
                array arr  = array();
                cstr  p    = v;
                while (p < ve) {
                    while (p < ve && (*p == ' ' || *p == '\t')) p++;
                    cstr w = p;
                    while (p < ve && *p != ' ' && *p != '\t') p++;
                    int wl = (int)(p - w);
                    if (wl <= 0) break;
                    string ws = string(alloc, wl + 1);
                    memcpy((cstr)ws->chars, w, wl);
                    ((cstr)ws->chars)[wl] = 0;
                    ws->count = wl;
                    push(arr, (elem && elem != typeid(none) && elem != typeid(Au) && elem != typeid(string))
                        ? (Au)construct_with(elem, (Au)ws, null) : (Au)ws);
                }
                value = (Au)arr;
            } else {
                string sv = string(alloc, vlen + 1);
                memcpy((cstr)sv->chars, v, vlen);
                ((cstr)sv->chars)[vlen] = 0;
                sv->count = vlen;
                value = (mem_type && mem_type != typeid(Au) && mem_type != typeid(string))
                    ? construct_with(mem_type, (Au)sv, null) : (Au)sv;
            }
            scan = after;
        }

        if (target) {
            if (is_map)          set((map)target, (Au)key, value);  // map target
            else if (mem)        member_set(target, mem, value);    // set member
        } else
            set(props, (Au)key, value);
    }

    if (rem) *rem = scan;
    if (target) return target;
    Au res = construct_with(use_schema, (Au)props, null);
    if (use_schema == typeid(map)) hold(props);
    return res;
}

// schema: type to construct when target is null. target: optional object to SET
// members onto (its existing type wins). returns the target (if given) or a fresh
// object/map. used so orbiter can read settings.agi straight onto itself.
Au parse_agi(Au_t schema, cstr s, Au target) {
    cstr rem = s;
    return parse_agi_block(s, 0, schema, null, &rem, target);
}

// ---- .agi : generic writer (inverse of parse_agi) ---------------------------
static bool agi_symbol_safe(cstr s) {
    if (!s || !isalpha((u8)*s)) return false;
    for (cstr p = s; *p; p++)
        if (!isalnum((u8)*p) && *p != '_' && *p != '-') return false;
    return true;
}

static none agi_indent(string res, int n) {
    for (int i = 0; i < n; i++)
        push(res, '\t');
}

// typed leaves (shape 32x32x1, path models/look.bin) may start with a digit
static bool agi_bare_safe(cstr s) {
    if (!s || !*s) return false;
    for (cstr p = s; *p; p++)
        if (!isalnum((u8)*p) && *p != '_' && *p != '-' && *p != '.' && *p != '/')
            return false;
    return true;
}

static cstr agi_enum_ident(Au_t et, i64 v) {
    for (num i = 0; i < et->members.count; i++) {
        Au_t m = (Au_t)et->members.origin[i];
        if (m->member_type != AU_MEMBER_ENUMV || !m->value) continue;
        i64 mv = (et->src && et->src->typesize == 8) ? *(i64*)m->value : (i64)*(i32*)m->value;
        if (mv == v) return m->ident;
    }
    return null;
}

static none agi_write_block(string res, Au a, int indent, int depth);

// leaf text for a value; false when the value needs a nested block
static bool agi_leaf(string res, Au v, int depth) {
    Au_t t = isa(v);
    if (instanceof(v, string)) {
        string s = (string)v;
        if (agi_symbol_safe(s->chars))
            concat(res, s);
        else {
            push(res, '\'');
            concat(res, escape(s));
            push(res, '\'');
        }
        return true;
    }
    if (t->traits & AU_TRAIT_PRIMITIVE) {
        // %g keeps small floats (epsilon 1e-8) from flattening to 0.000000
        if      (t == typeid(f32)) concat(res, f(string, "%g", (f64)*(f32*)v));
        else if (t == typeid(f64)) concat(res, f(string, "%g", *(f64*)v));
        else serialize(t, res, v);
        return true;
    }
    if (t->is_enum) {
        i64  ev = (t->src && t->src->typesize == 8) ? *(i64*)v : (i64)*(i32*)v;
        cstr id = agi_enum_ident(t, ev);
        if (id) append(res, (cstr)id);
        else    concat(res, f(string, "%lli", ev));
        return true;
    }
    if (instanceof(v, array)) {
        // arrays of class objects need keyed blocks — not a leaf
        each((collective)v, Au, e)
            if (e && isa(e)->is_class && !instanceof(e, string) && !instanceof(e, path))
                return false;
        push(res, '[');
        bool first = true;
        each((collective)v, Au, e) {
            append(res, first ? " " : ", ");
            if (!e)                        append(res, "null");
            else if (!agi_leaf(res, e, depth + 1))
                concat(res, json(e));
            first = false;
        }
        append(res, " ]");
        return true;
    }
    if (instanceof(v, map))
        return false;
    // string-castable leaves (path, shape); structs inline as json
    if (instanceof(v, path) || instanceof(v, shape) || t->is_struct) {
        string s = t->is_struct ? json(v) : cast(string, v);
        if (!s) return false;
        if (agi_bare_safe(s->chars) || t->is_struct)
            concat(res, s);
        else {
            push(res, '\'');
            concat(res, escape(s));
            push(res, '\'');
        }
        return true;
    }
    return false;
}

static none agi_write_key(string res, string k) {
    if (k && agi_symbol_safe(k->chars))
        concat(res, k);
    else {
        push(res, '\'');
        if (k) concat(res, escape(k));
        push(res, '\'');
    }
}

// key already written (no colon); writes ": value\n" or a typed nested block
static none agi_write_entry(string res, Au v, int indent, int depth) {
    if (depth > 12) {
        append(res, ": ");
        concat(res, json(v));
        push(res, '\n');
        return;
    }
    // arrays of class objects: keyed blocks (element's name/ident, else index)
    if (instanceof(v, array)) {
        bool objs = false;
        each((collective)v, Au, e)
            if (e && isa(e)->is_class && !instanceof(e, string) && !instanceof(e, path))
                objs = true;
        if (objs) {
            append(res, ":\n");
            int idx = 0;
            each((collective)v, Au, e) {
                if (!e) { idx++; continue; }
                agi_indent(res, indent + 1);
                string nm = (string)instanceof(Au_get_property(e, "name"),  string);
                if (!nm)  nm = (string)instanceof(Au_get_property(e, "ident"), string);
                agi_write_key(res, nm ? nm : f(string, "e%i", idx));
                append(res, ": ");
                append(res, (cstr)isa(e)->ident);
                push(res, '\n');
                agi_write_block(res, e, indent + 2, depth + 1);
                idx++;
            }
            return;
        }
    }
    if (instanceof(v, map)) {
        append(res, ":\n");
        agi_write_block(res, v, indent + 1, depth + 1);
        return;
    }
    string leaf = string(alloc, 64);
    if (agi_leaf(leaf, v, depth)) {
        append(res, ": ");
        concat(res, leaf);
        push(res, '\n');
        return;
    }
    // class object: type name in value position, members as a nested block
    append(res, ": ");
    append(res, (cstr)isa(v)->ident);
    push(res, '\n');
    agi_write_block(res, v, indent + 1, depth + 1);
}

static none agi_write_members(string res, Au a, Au_t type, int indent, int depth) {
    // base-first so inherited members (name, inputs) lead each block
    if (type->context && type->context->is_class && type->context != type &&
        type->context != typeid(Au))
        agi_write_members(res, a, type->context, indent, depth);
    for (num i = 0; i < type->members.count; i++) {
        Au_t m = (Au_t)type->members.origin[i];
        if (m->member_type != AU_MEMBER_VAR)          continue;
        if (m->is_static)                             continue;
        if (m->access_type == interface_intern)       continue;
        if (!m->type)                                 continue;
        // raw pointers (ref f32 buffers etc) have no text form
        if (m->type->is_pointer || (m->traits & AU_TRAIT_EXPLICIT_REF)) continue;
        Au v = Au_get_property(a, m->ident);
        if (!v) continue;
        agi_indent(res, indent);
        append(res, (cstr)m->ident);
        agi_write_entry(res, v, indent, depth);
    }
}

static none agi_write_block(string res, Au a, int indent, int depth) {
    if (instanceof(a, map)) {
        map ma = (map)a;
        pairs(ma, i) {
            agi_indent(res, indent);
            string k = instanceof(i->key, string) ? (string)i->key : cast(string, i->key);
            agi_write_key(res, k);
            agi_write_entry(res, i->value, indent, depth);
        }
        return;
    }
    agi_write_members(res, a, isa(a), indent, depth);
}

// generic object -> tab-indented .agi text via reflection (publics only)
string string_agi(Au a) {
    string res = string(alloc, 1024);
    if (a) agi_write_block(res, a, 0, 0);
    return res;
}

static array parse_array_objects(cstr* s, Au_t element_type, ctx context) {
    cstr scan = *s;
    array res = array(64);

    for (;;) {
        if (scan[0] == ']') {
            scan = ws(&scan[1]);
            break;
        }
        cstr before = scan;
        Au a = parse_object(scan, element_type, null, &scan, context);
        if (!scan) {
            break;
        }
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

static Au parse_array(cstr s, Au_t schema, Au_t meta_type, cstr* remainder, ctx context) { sequencer
    cstr scan = ws(s);
    verify(*scan == '[', "expected array '['");
    scan = ws(&scan[1]);
    Au res = null;
    if (!schema || (schema == typeid(array) || schema->src == typeid(array))) {
        // schema-less arrays (unknown/discarded fields, generic JSON) must not
        // force map elements — a ["string", ...] array would fault in
        // construct_with("expected map"). null lets parse_object infer.
        Au_t element_type = meta_type ? meta_type : (schema ? schema->meta.a : null);
        res = (Au)parse_array_objects(&scan, element_type, context);
    } else if (schema->meta.a == typeid(i64)) { // should support all vector types of i64 (needs type bounds check with vmember_count)
        array arb = parse_array_objects(&scan, typeid(i64), context);
        int vcount = len(arb);
        res = alloc2(schema, typeid(i64), new_shape(vcount, 0), __FILE__, __LINE__, seq);
        int n = 0;
        each(arb, Au, a) {
            verify(isa(a) == typeid(i64), "expected i64");
            ((i64*)res)[n++] = *(i64*)a;
        }
    } else if (schema->meta.a == typeid(f32)) { // should support all vector types of f32 (needs type bounds check with vmember_count)
        array arb = parse_array_objects(&scan, typeid(f32), context);
        int vcount = len(arb);
        res = alloc(typeid(f32), vcount, null, null, null, __FILE__, __LINE__, seq);
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
        res = construct_with(schema, (Au)arb, null);
    } else if (schema->src == typeid(vector)) {
        Au_t scalar_type = schema->meta.a;
        verify(scalar_type, "scalar type required when using vector (define a meta-type of vector with type)");
        
        array prelim = parse_array_objects(&scan, null, context);
        int count = len(prelim);
        // this should contain multiple arrays of scalar values; we want to convert each array to our 'scalar_type'
        // for instance, we may parse [[1,2,3,4,5...16],...] mat4x4's; we merely need to validate vmember_count and vmember_type and convert
        // if we have a vmember_count of 0 then we are dealing with a single primitive type
        vector vres = (vector)alloc(schema, 1, null, null, null, __FILE__, __LINE__, 0);
        vres->data_shape = new_shape(count, 0);
        Au_initialize((Au)vres);
        i8* data = (i8*)vres->origin;
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
        res = (Au)vres;
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
    return string(chars, (cstr)start, ref_length, len);
}

Au parse(Au_t schema, cstr s, ctx context) {
    if (context) {
        if (!ctx_checksums) ctx_checksums = hold(map(hsize, 32));
        string key = f(string, "%p", context);
        u64*   chk = (u64*)get(ctx_checksums, (Au)key);
        string ctx = extract_context(s, &s);
        u64 h = hash(ctx);
        if (!chk || *chk != h) {
            set(ctx_checksums, (Au)key, _u64(h));
            context->establishing = true;
            map ctx_update = (map)parse_object((cstr)ctx->chars, null, null, null, context);
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
        if (t->target)
            ((void(*)(Au,Au))t->work_fn)(t->target, thread->w);
        else
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
    t->global = hold(mutex(cond, true));
    for (int i = 0; i < n; i++) {
        thread_t* thread = &t->threads[i];
        thread->index = i;
        thread->w     = get(t->work, i);
        thread->next  = thread->w;
        thread->t     = t;
        thread->lock  = hold(mutex(cond, true));
        lock(thread->lock);
        pthread_create(&thread->obj, null, (void*)async_runner, thread);
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

typedef struct { callback fn; Au target; Au work; } au_spawn_t;
static void* au_spawn_runner(void* data) {
    au_spawn_t* s = (au_spawn_t*)data;
    s->fn(s->target, s->work);
    free(s);
    return null;
}
void au_spawn(callback fn, Au target, Au work) {
    au_spawn_t* s = (au_spawn_t*)malloc(sizeof(au_spawn_t));
    s->fn     = fn;
    s->target = target;
    s->work   = work;
    pthread_t tid;
    pthread_create(&tid, null, au_spawn_runner, s);
    pthread_detach(tid);
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
            cond_signal(thread->lock); // wake a parked runner or join hangs
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

#ifndef __APPLE__
// recursively register an inotify watch on dir and every subdirectory under it.
// inotify is per-directory (not recursive), so we walk the tree once at start.
// hidden entries (dotfiles, .git) are skipped; depth is bounded like the indexer.
static void watch_add_tree(int fd, const char* dir, int depth) {
    if (depth > 8) return;
    inotify_add_watch(fd, dir,
        IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) == 0 && S_ISDIR(st.st_mode))
            watch_add_tree(fd, p, depth + 1);
    }
    closedir(d);
}

// thread body: poll inotify for any change under res. on the first event of a
// batch, fire the callback (target = argument, work = null) so the listener can
// mark its index stale. heavy work stays on the listener's own thread.
// no hold/drop here — Au refs are non-atomic, so the object is kept alive by
// pause()/dealloc joining this thread before any free.
static void* watch_runner(void* arg) {
    watch a  = (watch)arg;
    int   fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) return null;
    a->fd = fd;
    if (a->res) watch_add_tree(fd, a->res->chars, 0);

    char buf[8192]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    while (a->running) {
        int len = read(fd, buf, sizeof(buf));
        if (len > 0) {
            if (a->running && a->callback)
                ((callback)a->callback)((Au)a->argument, null);
        }
        usleep(200000); // 200ms tick — pause() takes effect within this window
    }

    close(fd);
    a->fd = -1;
    return null;
}
#endif

none watch_init(watch a) {
    a->fd      = -1;
    a->running = false;
}

none watch_dealloc(watch a) {
    pause(a);
}

none watch_pause(watch a) {
#ifndef __APPLE__
    if (!a->running) return;
    a->running = false;       // runner observes this and exits its loop
    if (a->tid) {
        pthread_join((pthread_t)a->tid, null); // wait so it stops touching us
        a->tid = 0;
    }
#endif
}

none watch_start(watch a) {
#ifndef __APPLE__
    if (a->running || !a->res) return;
    a->running = true;
    pthread_t tid;
    if (pthread_create(&tid, null, watch_runner, a) != 0) {
        a->running = false;
        return;
    }
    a->tid = (i64)tid; // joined in pause()/dealloc; object outlives the thread
#endif
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
i64 shape_flat_index(shape data_shape, shape index_shape) {
    i64 flat = 0;
    i64 stride = 1;
    for (int d = data_shape->count - 1; d >= 0; d--) {
        flat += index_shape->data[d] * stride;
        stride *= data_shape->data[d];
    }
    return flat;
}

none shape_dealloc(shape a) {
    if (!a->is_global)
        free(a->data);
}

none shape_push(shape a, i64 i) {
    i64* prev = a->data;
    a->data = calloc(sizeof(i64), (a->count + 2));
    memcpy(a->data, prev, a->count * sizeof(i64));
    a->data[a->count++] = i;
    a->data[a->count]   = 0;
    if (!a->is_global)
        free(prev);
    a->is_global = false;
}

shape shape_with_i64(shape a, i64 i) {
    a->data         = &head(a)->count; // head wont mind if we redefine its count field for only shape.
    a->data[0]      = i;
    a->count        = 1;
    a->is_global    = true;
    return a;
}

// inverse of shape_cast_string ("32x32x1", also plain "5")
shape shape_with_string(shape a, string s) {
    a->data  = (i64*)calloc(sizeof(i64), 17);
    a->count = 0;
    cstr p = (cstr)s->chars;
    while (p && *p && a->count < 16) {
        char* e = null;
        i64   d = strtoll(p, &e, 10);
        if (e == p) break;
        a->data[a->count++] = d;
        p = (*e == 'x') ? e + 1 : null;
    }
    return a;
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

Au token_get_literal(token a, Au_t of_type) {
    if (!a) return null;
    if (a && a->literal && (!of_type || inherits(isa(a->literal), of_type))) {
        return a->literal;
    } else if (of_type == typeid(i64) && inherits(isa(a->literal), typeid(shape))) {
        shape vshape = (shape)a->literal;
        Au res = _i64(vshape->data[0]);
        return res;
    }
    return null;
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
    if (cs[0] == '.' && a->count == 1) return null;

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
    parser_target = (Au)a->target;
    parser_ident(a->target, a->input, (Au)a);
}

// constructors have ability to return whatever data they want, and
// when doing so, the buffer is kept around for the next user (max of +1)
tokens tokens_with_cstr(tokens a, cstr cs) {
    a->parser = parser_ident;
    a->target = (Au)parser_target;
    a->input  = (Au)string(cs);
    return a;
}

string tokens_cast_string(tokens a) {
    string result = string(alloc, 1024);
    int n    = len(a);
    int line = 0;
    for (int i = 0; i < n; i++) {
        token t = (token)a->origin[i];
        if (i == 0) {
            line = t->line;
            for (int j = 0; j < t->indent; j++)
                append(result, " ");
        } else if (t->line > line) {
            while (line < t->line) {
                append(result, "\n");
                line++;
            }
            for (int j = 0; j < t->indent; j++)
                append(result, " ");
        } else {
            append(result, " ");
        }
        append(result, t->chars);
    }
    return result;
}

void token_init(token a) {
    cstr prev = a->chars;
    sz length = a->count ? a->count : strlen(prev);
    a->chars  = (cstr)calloc((a->alloc ? a->alloc : length) + 1, 1);
    a->count    = length;

    memcpy(a->chars, prev, length);
    if (!a->literal) {
        if (a->chars[0] == '\"' || a->chars[0] == '\'') {
            string crop = string(chars, &a->chars[1], ref_length, length - 2);
            a->literal = (Au)read_string(crop->chars, a->chars[0] == '\"');
        } else
            a->literal = read_numeric(a);
    }
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

array read_arg_br(array tokens, int start, int* next_read, cstr open, cstr close) {
    int   level = 0;
    int   ln    = len(tokens);
    bool  count = ln - start;
    array res   = array(alloc, 32);
    int   next_level = 0;

    for (int i = start; i < ln; i++) {
        next_level = level;
        token t = (token)get(tokens, i);

        if (eq(t, open))
            next_level = level + 1;
        else if (eq(t, close) && level > 0)
            next_level = level - 1;

        if ((eq(t, ",") || eq(t, close)) && level == 0) {
            *next_read = i;
            return res;
        }

        push(res, (Au)t);
        level = next_level;
    }
    return count > 0 ? null : res;
}

array read_arg(array tokens, int start, int* next_read) {
    return read_arg_br(tokens, start, next_read, "(", ")");
}


none fdata_init(fdata f) {
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
                r   = f(string, "/tmp/f%p", (none*)h);
                src = (cstr)r->chars;
                p   = path(r);
            } while (exists(p));
        }
        f->id = fopen(src, f->read ? "rb" : "wb");
        if (!f->src)
             f->src = path(src);
    }
}

bool fdata_cast_bool(fdata f) {
    return f->id != null;
}


bool fdata_seek(fdata f, i64 value, bool from_end) {
    fseek(f->id, value, from_end ? SEEK_END : SEEK_SET);
    return true;
}

string fdata_gets(fdata f) {
    char buf[2048];
    if (fgets(buf, 2048, f->id) > 0)
        return string(buf);
    return null;
}

bool fdata_file_write(fdata f, Au o) {
    Au_t type = isa(o);
    if (type == typeid(string)) {
        u16 nbytes    = ((string)o)->count;
        u16 le_nbytes = htole16(nbytes);
        fwrite(&le_nbytes, 2, 1, f->id);
        f->fsize += (num)nbytes;
        return fwrite(((string)o)->chars, 1, nbytes, f->id) == nbytes;
    }
    sz size = isa(o)->typesize;
    f->fsize += (num)size;
    verify(type->traits & AU_TRAIT_PRIMITIVE, "not a primitive type");
    return fwrite(o, size, 1, f->id) == 1;
}

// its very french -- ffin should certainly be standard C
bool fdata_fin(fdata f) {
    long cur = ftell(f->id);
    if (cur < 0) return true;
    long end;
    long saved = cur;

    if (fseek(f->id, 0, SEEK_END) != 0)
        return true;

    end = ftell(f->id);
    fseek(f->id, saved, SEEK_SET);
    return cur >= end;
}

Au fdata_file_read(fdata f, Au_t type) {
    if (fin(f)) return null;
    if (type == typeid(string)) {
        char bytes[65536];
        u16  nbytes;
        if (f->text_mode) {
            verify(fgets(bytes, sizeof(bytes), f->id), "could not read text");
            return (Au)string(bytes); 
        }
        verify(fread(&nbytes, 2, 1, f->id) == 1, "failed to read byte count");
        nbytes = le16toh(nbytes);
        f->location += nbytes;
        verify(nbytes < 1024, "overflow");
        verify(fread(bytes, 1, nbytes, f->id) == nbytes, "read fail");
        bytes[nbytes] = 0;
        return (Au)string(bytes); 
    }
    Au o = alloc(type, 1, null, null, null, __FILE__, __LINE__, 0);
    sz size = isa(o)->typesize;
    f->location += size;
    verify(type->traits & AU_TRAIT_PRIMITIVE, "not a primitive type");
    bool success = fread(o, size, 1, f->id) == 1;
    return success ? o : null;
}

none fdata_file_close(fdata f) {
    if (f->id) {
        fclose(f->id);
        f->id = null;
    }
}

none fdata_dealloc(fdata f) {
    file_close(f);
}

i64 fdata_total_bytes(fdata a) {
    FILE* f = fopen(a->src->chars, "rb");
    fseek(f, 0, SEEK_END);
    sz flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);
    return (i64)flen;
}

u64 fdata_hash(fdata a) {
    const int sz = 2048;
    char buf[2048];
    u64 h = OFFSET_BASIS;

    //a->id = fopen(a->src->chars, "rb");
    //if (!a->handle)
    //    return 0;
    
    while (true) {
        int count = fread(buf, 1, sizeof(buf), a->id);
        if (count == 0)
            break;
        h = (u64)fnv1a_hash(buf, count, h);
    }
    //fclose(a->id);
    //a->id = null;
    return h;
}

/*

Au_t_f
*/

i32 app_run(app a) {
    return 0;
}

void live_app_run(live_app a)     { }
bool live_app_frame(live_app a)   { return false; }
void live_app_destroy(live_app a) { }

Au coverage_run(coverage a) {
    return null;
}

none app_init(app a) {
    puts("app init\n");
}


typedef struct _cov_module {
    uint64_t*  probes;
    uint32_t   probe_count;
    uint64_t*  timings;
    uint32_t   func_count;
    char**     func_names;
    struct _cov_module* next;
} cov_module;

static cov_module* __cov_modules = NULL;


void __coverage_report(void);
static void __coverage_sigint(int sig) {
    __coverage_report();
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}
void __coverage_register(uint64_t* probes, uint32_t probe_count,
                         uint64_t* timings, uint32_t func_count,
                         char** func_names) {
    cov_module* m = malloc(sizeof(cov_module));
    m->probes      = probes;
    m->probe_count = probe_count;
    m->timings     = timings;
    m->func_count  = func_count;
    m->func_names  = func_names;
    m->next        = __cov_modules;
    __cov_modules  = m;
    // print the timing/coverage report on normal exit AND on ctrl-C (the app is a
    // GUI loop, so SIGINT is the usual way out). registered once, only when a module
    // actually has timing/coverage data.
    static int hooked = 0;
    if (!hooked) {
        hooked = 1;
        atexit(__coverage_report);
        signal(SIGINT, __coverage_sigint);
    }
}

void __coverage_report(void) {
    if (!__cov_modules) return;
    uint32_t total_probes = 0, total_covered = 0;
    for (cov_module* m = __cov_modules; m; m = m->next) {
        for (uint32_t i = 0; i < m->probe_count; i++) {
            total_probes++;
            if (m->probes[i] > 0) total_covered++;
        }
    }
    float pct = total_probes > 0 ? (100.0f * total_covered / total_probes) : 100.0f;
    fprintf(stderr, "\n══════════════════════════════════════\n");
    fprintf(stderr, "  COVERAGE: %u/%u blocks (%.1f%%)\n",
            total_covered, total_probes, pct);
    
    // timing across all modules
    bool has_timing = false;
    for (cov_module* m = __cov_modules; m; m = m->next)
        if (m->timings && m->func_count > 0) { has_timing = true; break; }
    
    if (has_timing) {
        fprintf(stderr, "──────────────────────────────────────\n");
        fprintf(stderr, "  TIMING (top functions):\n");
        for (int shown = 0; shown < 40; shown++) {
            uint64_t max_ns = 0;
            cov_module* max_mod = NULL;
            uint32_t max_id = 0;
            for (cov_module* m = __cov_modules; m; m = m->next) {
                if (!m->timings) continue;
                for (uint32_t i = 0; i < m->func_count; i++) {
                    if (m->timings[i] > max_ns) {
                        max_ns = m->timings[i];
                        max_mod = m;
                        max_id = i;
                    }
                }
            }
            if (max_ns == 0) break;
            double ms = max_ns / 1000000.0;
            char* name = (max_mod->func_names && max_mod->func_names[max_id])
                ? max_mod->func_names[max_id] : "?";
            fprintf(stderr, "    %-22s %.3f ms\n", name, ms);
            max_mod->timings[max_id] = 0;
        }
    }
    fprintf(stderr, "══════════════════════════════════════\n\n");
}

__int64_t _epoch_millis();

__int64_t _epoch_millis() {
    struct timeval tv;
    gettimeofday((struct timeval*)&tv, 0L);
    return (__int64_t)(tv.tv_sec) * 1000 + (__int64_t)(tv.tv_usec) / 1000;
}

i64 epoch_millis() {
    return _epoch_millis();
}

i64 epoch_micros() {
    struct timeval tv;
    gettimeofday((struct timeval*)&tv, 0L);
    return (__int64_t)(tv.tv_sec) * 1000000 + (__int64_t)(tv.tv_usec);
}


define_arb(Au, Au, sizeof(struct _Au), AU_TRAIT_CLASS, null);

define_class(store, collective)

define_class(subscriber, Au)

define_class(fdata, Au)

define_class(subs, Au)

define_class(mutex, Au)

define_class(srcfile, Au);

define_class(app, Au)
define_class(live_app, Au)
define_class(ielement, Au)

define_class(coverage, Au)

define_class(watch,   Au)
define_class(msg,     Au)
define_class(async,   Au)
define_class(lambda,  Au, none, none, none, none, none, none, none, none)

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
define_primitive(unichar, numeric, 0) // stricter type matching with this
define_primitive(AFlag,  numeric, AU_TRAIT_INTEGRAL | AU_TRAIT_UNSIGNED)
define_primitive(cstr,   string_like, AU_TRAIT_POINTER, i8)
define_primitive(symbol, string_like, AU_TRAIT_CONST | AU_TRAIT_POINTER, i8)
define_primitive(cereal, raw, 0)
//define_primitive(micro,  raw, 0)
//define_primitive(meta_t, raw, AU_TRAIT_STRUCT)
define_primitive(none,   nil, AU_TRAIT_VOID)

//define_primitive(Au_t,  raw, 0)
define_struct(micro, ARef, i32, i32)
define_struct(meta_t, Au_t, Au)

define_primitive(handle, raw, AU_TRAIT_POINTER, u8)
define_primitive(ARef,   ref, AU_TRAIT_POINTER, Au)
define_primitive(Au_ts, ref, AU_TRAIT_POINTER, Au_t)
define_primitive(floats_t, raw, AU_TRAIT_POINTER, f32)

define_func_ptr(func,           typeid(none))
define_func_ptr(hook,           typeid(Au), Au)
define_func_ptr(callback,       typeid(Au), Au, Au)
define_func_ptr(callback_extra, typeid(Au), Au, Au, Au)

define_primitive(cstrs, raw, AU_TRAIT_POINTER, cstr)

define_class(line, Au)

define_enum(OPType)
define_enum(Exists)
define_enum(level)
define_enum(Syntax)

define_class(path, string)
//define_class(file)
define_class(string,  Au)
define_class(const_string, string)
define_class(ipart,   Au)
define_class(command, string)

define_class(shape, Au, i64)

define_class(token, string)

// this is defining tokens as array <token> .. the shape, is something the user gives (hard coded in Au)
define_class(tokens, array, token)

define_class(const_tokens, tokens, token)

define_class(fmt_token, Au)
define_class(fmt_file, Au)

define_class(item, Au)

define_class(collective, Au)
define_class(list,    collective, Au)
define_class(array,   collective, Au)
define_class(map,     collective, Au, Au)
define_class(ctx,            map)
define_class(subprocedure,    Au)

//define_class(Au_ts,           array, Au_t)
//define_class(array_map,        array, map)
//define_class(array_string,     array, string)

define_enum  (interface)
define_enum  (comparison)

#undef bind
