#include <A.h>
#include <ctype.h>
#include <stdarg.h>

static global_init_fn* call_after;
static num             call_after_alloc;
static num             call_after_count;
static array           keywords;

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

/// we dont call init across base vector.  there is no use-case for this that serves
/// init is for normal instances
/// vector is for primitive data
A A_alloc(A_f* type, num count) {
    A a = calloc(1, (type == typeof(A) ? 0 : sizeof(struct A)) + type->size * count);
    a->type   = type;
    a->origin = a;
    a->data   = &a[1];
    a->count  = count;
    a->alloc  = count;
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

/// with origin & data, we can reallocate.  one must 
A A_realloc(A a, num alloc) {
    A obj = object(a);
    assert(obj->type->traits == A_TRAIT_PRIMITIVE);
    A   re    = calloc(1, sizeof(struct A) + obj->type->size * alloc);
    num count = obj->count < alloc ? obj->count : alloc;
    memcpy(&re[1], obj->data, obj->type->size * count);
    if (obj->data != &obj[1])
        free(&obj->data[-1]);
    re->origin = obj; /// we do not want to keep the counts/alloc in sync between these
    obj->data  = &re[1];
    obj->count = count;
    obj->alloc = alloc;
    return obj->data;
}

void A_push(A a, A value) {
    A   obj = object(a);
    assert(obj->type->traits == A_TRAIT_PRIMITIVE);
    num sz  = obj->type->size;
    if (obj->count == obj->alloc)
        A_realloc(a, 32 + obj->alloc << 1);
    memcpy(&((u8*)obj->data)[obj->count++ * sz], value, sz);
}

void A_finish_types() {
    num         types_len;
    A_f**       types = A_types(&types_len);
    const num   max_args = 8;

    /// iterate through types
    for (num i = 0; i < types_len; i++) {
        A_f* type = types[i];

        /// for each member of type
        for (num m = 0; m < type->member_count; m++) {
            member_t* mem = &type->members[m];
            if (mem->member_type & (A_TYPE_IMETHOD | A_TYPE_SMETHOD | A_TYPE_CONSTRUCT)) {
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
                u8 *rel = (mem->member_type == A_TYPE_CONSTRUCT) ? 
                    (u8*)type->factory : (u8*)type;
                assert(rel);
                memcpy(&mem->method->address, &((u8*)rel)[mem->offset], sizeof(void*));
                assert(mem->method->address);
            }
        }
    }
}

member_t* A_member(A_f* type, enum A_TYPE member_type, char* name) {
    for (num i = 0; i < type->member_count; i++) {
        member_t* mem = &type->members[i];
        if (mem->member_type & member_type && strcmp(mem->name, name) == 0)
            return mem;
    }
    return 0;
}

A A_primitive(A_f* type, void* data) {
    assert(type->traits & A_TRAIT_PRIMITIVE);
    A copy = A_alloc(type, type->size);
    memcpy(copy, data, type->size);
    return copy;
}

A A_enum(A_f* type, i32 data) {
    assert(type->traits & A_TRAIT_ENUM);
    assert(type->size == sizeof(i32));
    A copy = A_alloc(type, type->size);
    memcpy(copy, &data, type->size);
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
A A_primitive_cstr(cstr data) { return A_primitive(&cstr_type, &data); }
A A_primitive_none()          { return A_primitive(&none_type, &data); }
A A_primitive_bool(bool data) { return A_primitive(&bool_type, &data); }

/// this calls type methods
A A_method(A_f* type, char* method_name, array args) {
    member_t* mem = A_member(type, A_TYPE_IMETHOD | A_TYPE_SMETHOD, method_name);
    assert(mem);
    const     num max_args = 8;
    void*     arg_values[max_args];

    assert(args->len <= max_args);
    assert(args->len == mem->args.count);
    for (num i = 0; i < args->len; i++) {
        A_f** method_args = &mem->args.arg_0;
        A_f*  arg_type    = method_args[i];
        arg_values[i] = (arg_type->traits & (A_TRAIT_PRIMITIVE | A_TRAIT_ENUM)) ? 
            (void*)args->elements[i] : (void*)&args->elements[i];
    }

    void* result[8]; /// enough space to handle all primitive data
    ffi_call((ffi_cif*)mem->method->ffi_cif, mem->method->address, result, arg_values);
    if (mem->type->traits & A_TRAIT_PRIMITIVE)
        return A_primitive(mem->type, result);
    else if (mem->type->traits & A_TRAIT_ENUM)
        return A_enum(mem->type, *(i32*)result);
    else
        return (A) result[0];
}

/// methods are ops
/// A -------------------------
A           A_hold      (A a) { ++a->refs; return a; }
static A   A_default    (A_t type, num count) {
    return A_alloc(type, count);
}
static void A_init      (A a) { }
static void A_destructor(A a) { }
static u64  A_hash      (A a) { return (u64)(size_t)a; }
static bool A_boolean   (A a) { return (bool)(size_t)a; }

/// these pointers are invalid for A since they are in who-knows land, but the differences would be the same
static i32 A_compare(A a, A b) {
    return (i32)(a - b);
}

static void  string_init(string a) { printf("init string\n"); }
static void  string_destructor(string a) { free(a->chars); }
static num   string_compare(string a, string b) { return strcmp(a->chars, b->chars); }
static array string_split(string a, A sp) {
    return null;
}
static num   string_index_of(string a, cstr cs) {
    char* f = strstr(a->chars, cs);
    return f ? (num)(f - a->chars) : (num)-1;
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

string string_of_reserve(string a, num sz) {
    a->alloc = sz;
    a->chars = (char*)calloc(1, a->alloc);
    return a;
}

/// better to call these in scope from the subclassed instance
/// i also think they should not perform their own new
/// its kind of useful for returning cached elements, but thats 
/// also a static method potential, not a constructor
/// and again its only polymorphism we're talking about
/// the design of calling your parent constructors is better
/// the base cannot construct the same as you can
string string_of_cstr(string a, cstr value, num len) {
    if (len == -1) len = strlen(value);
    a->alloc = len + 1;
    a->chars = (char*)calloc(1, a->alloc);
    memcpy(a->chars, value, len);
    a->chars[len] = 0;
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

A object(A instance) {
    return (instance - 1)->origin;
}

A data(A instance) {
    A obj = object(instance);
    return obj->data;
}

/// list -------------------------
static void list_push(list a, A e) {
    item n = new(item);
    n->element = e; /// held already by caller
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
    a->elements[a->len++] = b;
}

static void array_push_symbols(array a, char* f, ...) {
    va_list args;
    va_start(args, f);
    char* value;
    while ((value = va_arg(args, char*)) != null) {
        string s = new(string, of_cstr, value, strlen(value));
        M(array, push, a, s);
    }
    va_end(args);
}

static void array_push_objects(array a, A f, ...) {
    va_list args;
    va_start(args, f);
    A value;
    while ((value = va_arg(args, A)) != null)
        M(array, push, a, hold(value));
    va_end(args);
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

/// index of element that compares to 0 diff with item
static num array_index_of(array a, A b) {
    for (num i = 0; i < a->len; i++) {
        if (a -> elements[i] == b)
            return i;
    }
    return -1;
}

static bool array_boolean(array a) { return a && a->len > 0; }

static void vector_push(vector a, A b) {
    A obj = object(a);
    if (obj->alloc == obj->count)
        A_realloc(obj, 32 + (obj->alloc < 1));
    u8* dst = obj->data;
    num sz = obj->type->size;
    memcpy(&dst[sz * obj->count++], b, sz);
}

/// the only thing we would look out for here is a generic 
/// user of 'collection' calling object on this return result
static A vector_pop(vector a) {
    A obj = object(a);
    assert(obj->count > 0);
    u8* dst = obj->data;
    num sz = obj->type->size;
    return (A)&dst[sz * --obj->count];
}

static num vector_compare(vector a, vector b) {
    A a_object = object(a);
    A b_object = object(b);
    num diff = a_object->count - b_object->count;
    if (diff != 0)
        return diff;
    assert(a_object->type == b_object->type);
    u8* a_data = data(a);
    u8* b_data = data(b);
    num sz = a_object->type->size;
    for (num i = 0; i < a_object->count; i++) {
        num cmp = memcmp(&a_data[sz * i], &b_data[sz * i], sz);
        if (cmp != 0)
            return cmp;
    }
    return 0;
}

static A vector_get(vector a, num i) {
    A a_object = object(a);
    u8* a_data = data(a);
    num sz = a_object->type->size;
    return (A)&a_data[i * sz];
}

static num vector_count(vector a) {
    A a_object = object(a);
    return a_object->count;
}

static num vector_index_of(vector a, A b) {
    A a_object = object(a);
    u8* a_data = data(a);
    u8* b_data = data(b);
    num sz = a_object->type->size;
    for (num i = 0; i < a_object->count; i++) {
        if (memcmp(&a_data[sz * i], b_data, sz) == 0)
            return i;
    }
    return -1;
}

static bool vector_boolean(vector a) {
    A a_object = object(a);
    return a_object->count > 0;
}

u64 vector_hash(vector a) {
    A obj = object(a);
    return fnv1a_hash(obj->data, obj->type->size * obj->count, OFFSET_BASIS);
}


/// ordered init -------------------------
define_class(A)
define_class(string)
define_class(item)
define_proto(collection)
define_class(list)
define_class(array)
define_class(vector)

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
define_primitive(cstr, ffi_type_pointer)
define_primitive(bool, ffi_type_uint32)
define_primitive(num,  ffi_type_sint64)
define_primitive(none, ffi_type_void)
define_primitive(AType, ffi_type_pointer)

/// need to handle enum for arg in ffi
/// type-trait A_TRAIT_ENUM works

#define EType_meta(E, AR) \
    enum_value(E, AR, Undefined) \
    enum_value(E, AR, Statements) \
    enum_value(E, AR, Assign) \
    enum_value(E, AR, AssignAdd) \
    enum_value(E, AR, AssignSub) \
    enum_value(E, AR, AssignMul) \
    enum_value(E, AR, AssignDiv) \
    enum_value(E, AR, AssignOr) \
    enum_value(E, AR, AssignAnd) \
    enum_value(E, AR, AssignXor) \
    enum_value(E, AR, AssignShiftR) \
    enum_value(E, AR, AssignShiftL) \
    enum_value(E, AR, AssignMod) \
    enum_value(E, AR, If) \
    enum_value(E, AR, For) \
    enum_value(E, AR, While) \
    enum_value(E, AR, DoWhile) \
    enum_value(E, AR, Break) \
    enum_value(E, AR, LiteralReal) \
    enum_value(E, AR, LiteralInt) \
    enum_value(E, AR, LiteralStr) \
    enum_value(E, AR, LiteralStrInterp) \
    enum_value(E, AR, Array) \
    enum_value(E, AR, AlphaIdent) \
    enum_value(E, AR, Var) \
    enum_value(E, AR, Add) \
    enum_value(E, AR, Sub) \
    enum_value(E, AR, Mul) \
    enum_value(E, AR, Div) \
    enum_value(E, AR, Or) \
    enum_value(E, AR, And) \
    enum_value(E, AR, Xor) \
    enum_value(E, AR, MethodCall) \
    enum_value(E, AR, MethodReturn)
declare_enum(EType)

define_enum(EType)

#define ident_meta(T, B, AR) \
    intern(T, B, AR, string,    value) \
    intern(T, B, AR, string,    fname) \
    intern(T, B, AR, array,     members_cache) \
    intern(T, B, AR, int,       line_num) \
    intern(T, B, AR, u64,       h) \
    imethod(T, B, AR, array,     split_members) \
    imethod(T, B, AR, EType,     is_numeric) \
    imethod(T, B, AR, EType,     is_string) \
    construct(T, B, AR, T, of_token, string, string, num) \
    method_override(T, B, AR, u64, hash)
declare_class(ident)

EType ident_is_string(ident a) {
    char* t = a->value->chars;
    return t[0] == '"' ? EType_LiteralStr : t[0] == '\'' ? EType_LiteralStrInterp : EType_Undefined;
}

EType ident_is_numeric(ident a) {
    char* t = a->value->chars; /// even a null string can get valid pointer; a pointer to the length which is set to 0 is a null string
    return (t[0] >= '0' && t[0] <= '9') ? (strchr(t, '.') ? 
        EType_LiteralReal : EType_LiteralInt) : EType_Undefined;
}

u64 ident_hash(ident a) {
    if (!a->h) {
        u64 h = OFFSET_BASIS;
            h *= FNV_PRIME;
            h ^= M(string, hash, a->value);
        a->h = h;
    }
    return a->h;
}

array ident_split_members(ident a, A obj) {
    if (!a->members_cache)
        a->members_cache = M(string, split, a->value, obj);
    return a->members_cache;
}

void ident_of_token(ident a, string token, string fname, num line_num) {
    a->value = token;
    a->fname = fname;
    a->line_num = line_num;
}

define_class(ident)

/// static methods are a lure for arb data conversion
/// maybe thats ok

#define enode_meta(T, B, AR) \
    intern(T, B, AR, EType,      etype) \
    intern(T, B, AR, A,          value) \
    intern(T, B, AR, array,      operands) \
    intern(T, B, AR, array,      references) \
    smethod(T, B, AR, enode,     create_operation,   EType, array, array) \
    smethod(T, B, AR, enode,     create_value,       EType, A) \
    smethod(T, B, AR, enode,     method_call,        ident, array) \
    smethod(T, B, AR,  A,        lookup,             array, ident, bool) \
    smethod(T, B, AR,  string,   string_interpolate, A, array) \
    imethod(T, B, AR,  A,        exec,               array) \
    method_override(T, B, AR, bool, boolean)
declare_class(enode)

enode enode_create_operation(EType etype, array ops, array references) {
    return null;
}

enode enode_create_value(EType etype, A value) {
    return null;
}

enode enode_method_call(EType etype, ident method, array args) {
    return null;
}

A enode_lookup(array stack, ident id, bool top_only) {
    return null;
}

string enode_string_interpolate(A input, array stack) {
    return null;
}

A enode_exec(enode op, array stack) {
    return null;
}

bool enode_boolean(enode a) {
    return a->etype != EType_Undefined;
}

define_class(enode)

#define Parser_meta(T, B, AR) \
    intern( T, B, AR, array,  tokens) \
    intern( T, B, AR, string, fname) \
    intern( T, B, AR, num,    cur) \
    imethod(T, B, AR, ident,  token_at,          num) \
    imethod(T, B, AR, ident,  next) \
    imethod(T, B, AR, ident,  pop) \
    imethod(T, B, AR, num,    consume) \
    imethod(T, B, AR, EType,  expect,            ident, array) \
    imethod(T, B, AR, EType,  is_alpha_ident,    ident) \
    imethod(T, B, AR, ident,  relative,          num) \
    imethod(T, B, AR, EType,  is_assign,         ident) \
    imethod(T, B, AR, enode,  parse_statements) \
    imethod(T, B, AR, enode,  parse_expression) \
    imethod(T, B, AR, enode,  parse_statement) \
    imethod(T, B, AR, i64,    parse_numeric,     ident) \
    imethod(T, B, AR, EType,  is_var,            ident) \
    imethod(T, B, AR, enode,  parse_add) \
    imethod(T, B, AR, enode,  parse_mult) \
    imethod(T, B, AR, enode,  parse_primary)
declare_class(Parser)

static ident parse_token(char *start, num len, string fname, int line_num) {
    while (start[len - 1] == '\t' || start[len - 1] == ' ')
        len--;
    string all = new(string, of_cstr, start, len);
    char t = all->chars[0];
    bool is_number = (t == '-' || (t >= '0' && t <= '9'));
    return new(ident, of_token, all, fname, line_num); /// mr b: im a token!  line-num can be used for breakpoints (need the file path too)
}

static void ws(char **cur) {
    while (**cur == ' ' || **cur == '\t') {
        ++(**cur);
    }
}

/// parse tokens from string, referenced Parser in C++
static array parse_tokens(string input, string fname) {
    string        sp         = "$,<>()![]/+*:\"\'#"; /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
    char          until      = 0; /// either ) for $(script) ", ', f or i
    num           len        = input->len;
    char*         origin     = input->chars;
    char*         start      = 0;
    char*         cur        = origin - 1;
    int           line_num   = 1;
    bool          new_line   = true;
    bool          token_type = false;
    bool          found_null = false;
    bool          multi_comment = false;
    array         tokens     = new(array);
    ///
    while (*(++cur)) {
        bool is_ws = false;
        if (!until) {
            if (new_line)
                new_line = false;
            /// ws does not work with new lines
            if (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
                is_ws = true;
                ws(&cur);
            }
        }
        if (!*cur) break;
        bool add_str = false;
        char *rel = cur;
        if (*cur == '#') { // comment
            if (cur[1] == '#')
                multi_comment = !multi_comment;
            while (*cur && *cur != '\n')
                cur++;
            found_null = !*cur;
            new_line = true;
            until = 0; // requires further processing if not 0
        }
        if (until) {
            if (*cur == until && *(cur - 1) != '/') {
                add_str = true;
                until = 0;
                cur++;
                rel = cur;
            }
        }// else if (cur[0] == ':' && cur[1] == ':') { /// :: is a single token
        //    rel = ++cur;
        //}
        if (!until && !multi_comment) {
            char ch[2] = { cur[0], 0 };
            int type = M(string, index_of, sp, ch);
            new_line |= *cur == '\n';

            if (start && (is_ws || add_str || (token_type != (type >= 0) || token_type) || new_line)) {
                ident token = parse_token(start, (size_t)(rel - start), fname, line_num);
                M(array, push, tokens, token);
                if (!add_str) {
                    if (*cur == '$' && *(cur + 1) == '(') // shell
                        until = ')';
                    else if (*cur == '"') // double-quote
                        until = '"';
                    else if (*cur == '\'') // single-quote
                        until = '\'';
                }
                if (new_line) {
                    start = null;
                } else {
                    ws(&cur);
                    start = cur;
                    if (start[0] == ':' && start[1] == ':') /// double :: is a single token
                        cur++;
                    token_type = (type >= 0);
                }
            }
            else if (!start && !new_line) {
                start = cur;
                token_type = (type >= 0);
            }
        }
        if (new_line) {
            until = 0;
            line_num++;
            if (found_null)
                break;
        }
    }
    if (start && (cur - start)) {
        ident token = parse_token(start, cur - start, fname, line_num);
        M(array, push, tokens, token);
    }
    return tokens;
}

static ident  Parser_token_at(Parser a, num r) {
    return a->tokens->elements[a->cur + r];
}

static ident  Parser_next(Parser a) {
    return M(Parser, token_at, a, 0);
}

static ident  Parser_pop(Parser a) {
    if (a->cur < a->tokens->len)
        a->cur++;
    return a->tokens->elements[a->cur];
}

static num    Parser_consume(Parser a) {
    M(Parser, consume, a);
    return a->cur;
}

static EType  Parser_expect(Parser a, ident token, array tokens) {
    return M(array, index_of, tokens, token);
}

static EType  Parser_is_alpha_ident(Parser a, ident token) {
    char t = token ? token->value->chars[0] : 0;
    return (isalpha(t) && M(array, index_of, keywords, token) == -1) ?
        EType_AlphaIdent : EType_Undefined;
}

static ident  Parser_relative(Parser a, num pos) {
    return a->tokens->elements[a->cur + pos];
}

static EType  Parser_is_assign(Parser a, ident token) {
}

static enode  Parser_parse_statements(Parser parser) {
}

static enode  Parser_parse_expression(Parser parser) {
}

static enode  Parser_parse_statement(Parser parser) {
}

static i64    Parser_parse_numeric(Parser parser, ident token) {
}

static EType  Parser_is_var(Parser parser, ident) {
}

static enode  Parser_parse_add(Parser parser) {
}

static enode  Parser_parse_mult(Parser parser) {
}

static enode  Parser_parse_primary(Parser parser) {
}

/// parser we simply create with tokens
Parser parser_new(array tokens, string fname) {
    Parser parser = new(Parser);
    parser->tokens = hold(tokens);
    parser->fname  = hold(fname);
    return parser;
}

define_class(Parser)

int main(int argc, char **argv) {
    A_finish_types();

    /// lets begin porting of silver.cpp
    keywords = new(array);
    M(array, push_symbols, keywords, 
        "class",  "proto",  "struct",
        "import", "return", "asm", "if",
        "switch", "while",  "for", "do", null);

    ///
    return 0;
}