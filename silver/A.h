#ifndef _A_
#define _A_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ffi.h>

/// A-type runtime
typedef void                none;
typedef signed char         i8;
typedef short               i16;
typedef int                 i32;
typedef long long           i64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef long long           num;
typedef unsigned int        bool;
typedef float               f32;
typedef double              f64;
typedef long double         f128;
typedef double              real;    /// real enough
typedef void*               handle;
typedef char*               cstr;
typedef num                 sz;
typedef struct A_f*         A_t;
typedef struct A_f*         AType;
typedef struct args_t {
    num             count;
    A_t             arg_0, arg_1, arg_2, arg_3, 
                    arg_4, arg_5, arg_6, arg_7;
} args_t;

typedef bool(*global_init_fn)();

#define true                (bool)1
#define false               (bool)0
#define stringify(x)        #x
#define null                ((void*)0)
#define str(x)              #x
#define str_args(...)       str((__VA_ARGS__))
#define FNV_PRIME           0x100000001b3
#define OFFSET_BASIS        0xcbf29ce484222325

/// our A-type classes have many types of methods
/// constructor, i[nstance]-method, s[tatic]-method, operator (these are enumerable!), and index.  we index by 1 argument only in C but we may allow for more in silver
enum A_TYPE {
    A_TYPE_NONE      = 0,
    A_TYPE_PROP      = 1,
    A_TYPE_IMETHOD   = 2,
    A_TYPE_SMETHOD   = 4,
    A_TYPE_CONSTRUCT = 8,
    A_TYPE_OPERATOR  = 32,
    A_TYPE_CAST      = 64,
    A_TYPE_INDEX     = 128,
    A_TYPE_ENUMV     = 256
};

enum A_TRAIT {
    A_TRAIT_PRIMITIVE = 1,
    A_TRAIT_INTEGRAL  = 2,
    A_TRAIT_REALISTIC = 4,
    A_TRAIT_ENUM      = 8
};

#define   enum_value_DECL(E, N)             E##_##N,
#define   enum_value_IMPL(E, N) \
    E##_type.members[E## _type.member_count].name     = #N; \
    E##_type.members[E## _type.member_count].offset   = E##_##N;\
    E##_type.members[E## _type.member_count].type     = &i32_type; \
    E##_type.members[E## _type.member_count].member_type = A_TYPE_ENUMV; \
    E##_type.member_count++;
#define   enum_value(X,Y, N)                enum_value_##Y(X, N)

#define   i_intern_INST(X,Y, R, N, ...)     R N;
#define   i_intern_TYPE(X,Y, R, N, ...)  
#define   i_intern_INIT(X,Y, R, N, ...)
#define   i_intern_PROTO(X,Y, R, N, ...)    
#define   i_intern_METHOD(X,Y, R, N, ...)    
#define   i_intern(X,Y,Z, R, N, ...)        i_intern_##Z(X,Y,R,N, __VA_ARGS__)

#define   i_public_INST(X,Y, R, N, ...)     R N;
#define   i_public_TYPE(X,Y, R, N, ...)      
#define   i_public_CTR_MEMBER(X,Y, R, N, ...)
#define   i_public_INIT(X,Y, R, N, ...) \
    X##_type.members[X##_type.member_count].name     = #N; \
    X##_type.members[X##_type.member_count].offset   = offsetof(struct X, N); \
    X##_type.members[X##_type.member_count].type     = &R##_type; \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_PROP; \
    X##_type.member_count++;
#define   i_public_PROTO(X,Y, R, N, ...)  
#define   i_public_METHOD(X,Y, R, N, ...)   
#define   i_public(X,Y,Z, R, N, ...)        i_public_##Z(X,Y, R, N, __VA_ARGS__)

#define   s_method_INST(X,Y, R, N, ...)
#define   s_method_TYPE(X,Y, R, N, ...)     R (*N)(__VA_ARGS__);
#define   s_method_INIT(X,Y, R, N, ...)     \
    X##_type . N = & X## _ ## N; \
    X##_type.members[X##_type.member_count].name    = #N; \
    X##_type.members[X##_type.member_count].args    = (args_t) { emit_types(__VA_ARGS__) }; \
    X##_type.members[X##_type.member_count].type    = &R##_type; \
    X##_type.members[X##_type.member_count].offset  = offsetof(X##_f, N); \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_SMETHOD; \
    X##_type.member_count++;   
#define   s_method_PROTO(X,Y, R, N, ...)
#define   s_method_METHOD(X,Y, R, N, ...)   R (*N)(__VA_ARGS__);
#define   s_method(X,Y,Z, R, N, ...)        s_method_##Z(X,Y, R, N, ##__VA_ARGS__)

#define   i_method_INST(X,Y, R, N, ...)
#define   i_method_TYPE(X,Y, R, N, ...)     R (*N)(__VA_ARGS__);
#define   i_method_INIT(X,Y, R, N, ...)     \
    X##_type . N = & X## _ ## N; \
    X##_type.members[X##_type.member_count].name    = #N; \
    X##_type.members[X##_type.member_count].args    = (args_t) { emit_types(X, ##__VA_ARGS__) }; \
    X##_type.members[X##_type.member_count].type    = &R##_type; \
    X##_type.members[X##_type.member_count].offset  = offsetof(X##_f, N); \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_IMETHOD; \
    X##_type.member_count++; 
#define   i_method_PROTO(X,Y, R, N, ...)
#define   i_method_METHOD(X,Y, R, N, ...)   R (*N)(__VA_ARGS__);
#define   i_method(X,Y,Z, R, N, ...)        i_method_##Z(X,Y, R, N, X, ##__VA_ARGS__)

#define   i_operator_INST(X,Y, R, N, ARG)
#define   i_operator_TYPE(X,Y, R, N, ARG)   R (*operator_ ## N)(struct X*, ARG);
#define   i_operator_INIT(X,Y, R, N, ARG) \
    X##_type  . operator_##N = & X## _operator_ ## N; \
    assert(#X == #R); \
    X##_type.members[X##_type.member_count].name    = stringify(combine_tokens(operator_, N)); \
    X##_type.members[X##_type.member_count].args    = (args_t) { emit_types(X, ARG) }; \
    X##_type.members[X##_type.member_count].type    = &R##_type; \
    X##_type.members[X##_type.member_count].offset  = offsetof(X##_f, operator_##N); \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_OPERATOR; \
    X##_type.members[X##_type.member_count].operator_type = OPType_ ## N; \
    X##_type.member_count++; 
#define   i_operator_PROTO(X,Y, R, N, ARG)
#define   i_operator_METHOD(X,Y, R, N, ARG)
#define   i_operator(X,Y,Z, R, N, ARG)          i_operator_##Z(X,Y, R, N, ARG)

#define   i_cast_INST(X,Y, R)
#define   i_cast_TYPE(X,Y, R)                   R (*cast_##R)(struct X##_f*);
#define   i_cast_INIT(X,Y, R) \
    X##_type.cast_##R = & X##_cast_##R; \
    assert(#X == #R); \
    X##_type.members[X##_type.member_count].name    = stringify(combine_tokens(cast_, R)); \
    X##_type.members[X##_type.member_count].args    = (args_t) { emit_types(X) }; \
    X##_type.members[X##_type.member_count].type    = &R##_type; \
    X##_type.members[X##_type.member_count].offset  = offsetof(X##_f, cast_##R); \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_CAST; \
    X##_type.member_count++;  
#define   i_cast_PROTO(X,Y, R)
#define   i_cast_METHOD(X,Y, R)
#define   i_cast(X,Y,Z, R)                      i_cast_##Z(X,Y, R)


#define   i_index_INST(X,Y, R, ARG)
#define   i_index_TYPE(X,Y, R, ARG)             R (*index_##ARG)(struct X##_f*);
#define   i_index_INIT(X,Y, R, ARG) \
    X##_type  . index_##ARG = & X##_index_##ARG; \
    assert(#X == #R); \
    X##_type.members[X##_type.member_count].name    = stringify(combine_tokens(index_, ARG)); \
    X##_type.members[X##_type.member_count].args    = (args_t) { emit_types(X, ARG) }; \
    X##_type.members[X##_type.member_count].type    = &R##_type; \
    X##_type.members[X##_type.member_count].offset  = offsetof(X##_f, index_##ARG); \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_INDEX; \
    X##_type.member_count++; 
#define   i_index_PROTO(X,Y, R, ARG)
#define   i_index_METHOD(X,Y, R, ARG)
#define   i_index(X,Y,Z, R, ARG)                i_index_##Z(X,Y, R, ARG)

#define   i_construct_INST(X,Y,ARG, ...)
#define   i_construct_TYPE(X,Y,ARG, ...)        X (*with_##ARG)(struct X*, ARG, ##__VA_ARGS__);
#define   i_construct_INIT(X,Y,ARG, ...) \
    X##_type.with_##ARG = & X##_with_##ARG; \
    X##_type.members[X##_type.member_count].name        = #ARG; \
    X##_type.members[X##_type.member_count].args        = (args_t) { emit_types(X, ARG, ##__VA_ARGS__) }; \
    X##_type.members[X##_type.member_count].type        = &X##_type; \
    X##_type.members[X##_type.member_count].offset      = offsetof(X##_f, with_##ARG); \
    X##_type.members[X##_type.member_count].member_type = A_TYPE_CONSTRUCT; \
    X##_type.member_count++;  
#define i_construct_PROTO(X,Y, ARG, ...)
#define i_construct_METHOD(X,Y, ARG, ...)
#define i_construct(X,Y,Z, ARG, ...)            i_construct_##Z(X,Y, ARG, ##__VA_ARGS__)

#define i_method_vargs_INST(X,Y, R, N, ...)
#define i_method_vargs_TYPE(X,Y, R, N, ...)     R (*N)(X, __VA_ARGS__, ...);
#define i_method_vargs_INIT(X,Y, R, N, ...)     i_method_INIT(X,Y, R, N, __VA_ARGS__)  
#define i_method_vargs_PROTO(X,Y, R, N, ...)
#define i_method_vargs_METHOD(X,Y, R, N, ...)   R (*N)(__VA_ARGS__, ...);
#define i_method_vargs(X,Y,Z, R, N, ...)        i_method_vargs_##Z(X,Y, R, N, __VA_ARGS__)

#define s_method_vargs_INST(X,Y, R, N, ...)
#define s_method_vargs_TYPE(X,Y, R, N, ...)     R (*N)(__VA_ARGS__, ...);
#define s_method_vargs_INIT(X,Y, R, N, ...)     s_method_INIT(X,Y, R, N, ##__VA_ARGS__)  
#define s_method_vargs_PROTO(X,Y, R, N, ...)
#define s_method_vargs_METHOD(X,Y, R, N, ...)   R (*N)(__VA_ARGS__, ...);
#define s_method_vargs(X,Y,Z, R, N, ...)        s_method_vargs_##Z(X,Y, R, N, ##__VA_ARGS__)

#define implement_INST(X,Y, R, N, ...)
#define implement_TYPE(X,Y, R, N, ...)      
#define implement_INIT(X,Y, R, N, ...)
#define implement_PROTO(X,Y, R, N, ...)
#define implement_METHOD(X,Y, R, N, ...)
#define implement(X,Y,Z, R, N, ...)             implement_##Z(X,Y, R, N, __VA_ARGS__)

#define i_override_m_INST(X,Y, R, N)
#define i_override_m_TYPE(X,Y, R, N)        
#define i_override_m_INIT(X,Y, R, N)            X##_type . N = & X## _ ## N;
#define i_override_m_PROTO(X,Y, R, N)
#define i_override_m_METHOD(X,Y, R, N)
#define i_override_m(X,Y,Z, R, N)               i_override_m_##Z(X,Y, R, N)

#define i_override_ctr_INST(X,Y,   ARG)
#define i_override_ctr_TYPE(X,Y,   ARG)             
#define i_override_ctr_INIT(X,Y,   ARG)         X##_type.with_##ARG = & X##_with_##ARG;
#define i_override_ctr_PROTO(X,Y,  ARG)
#define i_override_ctr_METHOD(X,Y, ARG)
#define i_override_ctr(X,Y,Z, ARG)              i_override_ctr_##Z(X,Y, ARG)

typedef struct method_t {
    void*           address;
    void*           ffi_cif;  /// ffi-calling info
    void*           ffi_args; /// ffi-data types for args
} method_t;

typedef struct prop_t {
    void*           address;
} prop_t;

/// methods and properties may use this
/// we may not want to expose members; they will be zero if that is the case
typedef struct member_t {
    char*           name;
    A_t             type;
    num             offset;
    enum A_TYPE     member_type;
    int             operator_type;
    args_t          args;
    union {
        method_t*   method;
        prop_t*     prop;
    };
} member_t;

/// A type members with base-type
#define A_f_members(B) \
    struct B ## _f* parent; \
    char*           name; \
    num             size; \
    num             member_count; \
    member_t*       members; \
    u64             traits; \
    void*           arb;

#define declare_inst( X,Y ) \
    _Pragma("pack(push, 1)") \
    typedef struct X { \
        X##_meta (X,Y, INST) \
    } *X; \
    _Pragma("pack(pop)") \

#define declare_base( X ) \
    declare_inst(X, A) \
    typedef struct X##_f { \
        A_f_members(A) \
        X##_meta (X, A, TYPE) \
    } X##_f; \
    extern X##_f X##_type;

/// declare object and type with f-table from meta-def
#define declare_primitive( X ) \
    typedef struct X##_f { \
        A_f_members(A) \
        A_meta     (A, A, METHOD) \
    } X##_f, *X##_t; \
    extern X##_f X##_type;

#define declare_enum( E ) \
    typedef enum E { \
        E##_meta(E, DECL) \
        E##_ENUM_COUNT \
    } E; \
    typedef struct E##_f { \
        A_f_members(A) \
        A_meta     (E, A, METHOD) \
    } E##_f, *E##_t; \
    extern E##_f   E##_type;

void A_push_type(A_t type);

/// declare object and type with f-table from meta-def
#define declare_class(X) \
    declare_inst(X, A) \
    typedef struct X##_f { \
        A_f_members(A) \
        A_meta     (X, A, METHOD) \
        X##_meta (X, A, TYPE) \
    } X##_f, *X##_t; \
    extern X##_f  X##_type;

void        A_lazy_init(global_init_fn fn);

#define define_enum( E ) \
    E ## _f   E ## _type; \
    static __attribute__((constructor)) bool global_##E() { \
        E ## _f* type_ref = &E ## _type; \
        A_f* base_ref     = &A_type; \
        if ((A_t)type_ref != (A_t)base_ref && base_ref->size == 0) { \
            A_lazy_init((global_init_fn)&global_##E); \
            return false; \
        } else { \
            memcpy(type_ref, base_ref, sizeof(A_f)); \
            static member_t members[sizeof(E##_type) / sizeof(void*)]; \
            E##_type.parent   = & A_type; \
            E##_type.name     = #E; \
            E##_type.size     = sizeof(enum E); \
            E##_type.members  = members; \
            E##_type.traits   = A_TRAIT_ENUM; \
            E##_type.arb      = &ffi_type_sint32; \
            E##_meta( E, IMPL ); \
            A_push_type(&E##_type); \
            return true; \
        } \
    }
/// we mod, instead of [calling it] subclass
/// a class can still implement a protocol (initialization check)
#define declare_mod(X,Y) \
    declare_inst(X,Y) \
    typedef struct X##_f { \
        A_f_members(Y) \
        A_meta     (X,A, METHOD) \
        X##_meta (X,Y, TYPE) \
    } X##_f, *X##_t; \
    extern X##_f   X##_type;

#define declare_proto(X) \
    declare_inst(X,X) \
    typedef struct X##_f { \
        A_f_members(A) \
        A_meta     (X, A, METHOD) \
        X##_meta (X, A, TYPE) \
    } X##_f, *X##_t; \
    extern X##_f  X##_type;

#define define_global(X,Y, TYPE_SZ, TRAIT, META) \
    X##_f X##_type; \
    static __attribute__((constructor)) bool global_##X() { \
        X##_f* type_ref = &X##_type; \
        Y##_f* base_ref = &Y##_type; \
        if ((A_t)type_ref != (A_t)base_ref && base_ref->size == 0) { \
            A_lazy_init((global_init_fn)&global_##X); \
            return false; \
        } else { \
            memcpy(type_ref, base_ref, sizeof(Y##_f)); \
            static member_t members[sizeof(X##_type) / sizeof(void*)]; \
            X## _type.parent   = &Y##_type; \
            X## _type.name     = #X;        \
            X## _type.size     = TYPE_SZ;   \
            X## _type.members  = members;   \
            X## _type.traits   = TRAIT;     \
            META;                           \
            A_push_type(&X##_type);         \
            return true;                    \
        }                                   \
    }

#define define_primitive(X, ffi_type, traits) \
    define_global(X, A, sizeof(X), A_TRAIT_PRIMITIVE | traits, \
        X##_type.arb = &ffi_type; \
    )

#define define_class(X) \
    define_global(X, A, sizeof(struct X), 0, \
        X##_type.arb = &ffi_type_pointer; \
        X##_meta( X, A, INIT ) \
    )

/// its important that any additional struct be static, thus limiting the amount of data we are exporting
#define define_mod(X,Y) \
    define_global(X,Y, sizeof(struct X), 0, \
        X##_type.arb = &ffi_type_pointer; \
        X##_meta( X,Y, INIT ) \
    )

#define define_proto(X) \
    define_global(X, A, sizeof(struct X), 0, X##_meta(X,X, PROTO ))

/// constructors get a type forwarded from the construct macro
#define A_meta(X,Y,Z) \
    i_intern(X,Y,Z, A_t,       type) \
    i_intern(X,Y,Z, num,       refs) \
    i_intern(X,Y,Z, struct A*, data) \
    i_intern(X,Y,Z, num,       alloc) \
    i_intern(X,Y,Z, num,       count) \
    i_intern(X,Y,Z, struct A*, origin) \
    i_method(X,Y,Z, none,      init) \
    i_method(X,Y,Z, none,      destructor) \
    i_method(X,Y,Z, i32,       compare,    X) \
    i_method(X,Y,Z, u64,       hash) \
    i_method(X,Y,Z, bool,      boolean)
declare_base(A)

#define typeof(X) ((struct A_f*)&X##_type)

/// meta gives us access to one token we can override with (F)
declare_primitive( i8)
declare_primitive(i16)
declare_primitive(i32)
declare_primitive(i64)
declare_primitive( u8)
declare_primitive(u16)
declare_primitive(u32)
declare_primitive(u64)
declare_primitive(f32)
declare_primitive(f64)
declare_primitive(f128)
declare_primitive(cstr)
declare_primitive(bool)
declare_primitive(none)
declare_primitive(num)
declare_primitive(sz)

/// whatever we can 'name', we can handle as a type of any pointer primitive
declare_primitive(AType)

/// doubly-linked item type
#define item_meta(X,Y,Z) \
    i_intern(X,Y,Z, struct X*, next) \
    i_intern(X,Y,Z, struct X*, prev) \
    i_intern(X,Y,Z, A,         element)
declare_class(item)

/// why not put a key on A
#define field_meta(X,Y,Z) \
    i_intern(X,Y,Z, A, key) \
    i_intern(X,Y,Z, A, val) \
    i_override_m(X,Y,Z, u64, hash) \
    i_construct(X,Y,Z, cstr, A)
declare_class(field)

/// abstract collection type
/// proto needs an implement for each member type!
#define collection_meta(X,Y,Z) \
    implement(X,Y,Z, i64,     count) \
    implement(X,Y,Z, none,    push,   A) \
    implement(X,Y,Z, A,       pop) \
    implement(X,Y,Z, none,    remove, num) \
    implement(X,Y,Z, A,       get,    num)
declare_proto(collection)

/// linked-list of elemental data
#define list_meta(X,Y,Z) \
    collection_meta(X,Y,Z) \
    i_intern(X,Y,Z, item,      first) \
    i_intern(X,Y,Z, item,      last)  \
    i_intern(X,Y,Z, i64,       count) \
    i_public(X,Y,Z, i32,       public_integer) \
    i_method(X,Y,Z, A,         pop) \
    i_method(X,Y,Z, none,      push, A) \
    i_method(X,Y,Z, none,      remove, num) \
    i_method(X,Y,Z, A,         get,  i32) \
    i_method(X,Y,Z, num,       count)
declare_mod(list, collection)

/// array of elemental data
/// important to define the same operators here in silver definition of runtime
/// one has arguments (operator) and (cast) does not, thats always instance
/// we need different names for these.  they cannot both be considered 'operators'
/// we also need new data structure, so ops vs casts vs constructs vs methods
// += -= *= /= 
// ":", "+=", "-=", "*=", "/=", "|=",
// "&=", "^=", ">>=", "<<=", "%=", null);

#define OPType_meta(X,Y) \
    enum_value(X,Y, add) \
    enum_value(X,Y, sub) \
    enum_value(X,Y, mul) \
    enum_value(X,Y, div) \
    enum_value(X,Y, or) \
    enum_value(X,Y, and) \
    enum_value(X,Y, xor) \
    enum_value(X,Y, right) \
    enum_value(X,Y, left) \
    enum_value(X,Y, assign) \
    enum_value(X,Y, assign_add) \
    enum_value(X,Y, assign_sub) \
    enum_value(X,Y, assign_mul) \
    enum_value(X,Y, assign_div) \
    enum_value(X,Y, assign_or) \
    enum_value(X,Y, assign_and) \
    enum_value(X,Y, assign_xor) \
    enum_value(X,Y, assign_right) \
    enum_value(X,Y, assign_left) \
    enum_value(X,Y, mod_assign)
declare_enum(OPType)
define_enum(OPType)

#define path_meta(X,Y,Z) \
    i_intern    (X,Y,Z, cstr,   chars) \
    i_method    (X,Y,Z, bool,   exists) \
    i_method    (X,Y,Z, A,      read, AType) \
    i_override_m(X,Y,Z, u64,    hash) \
    i_construct (X,Y,Z, cstr)
declare_class(path)

#define array_meta(X,Y,Z) \
    collection_meta  (X,Y,Z) \
    i_intern         (X,Y,Z, A*,   elements) \
    i_intern         (X,Y,Z, i32,  alloc) \
    i_intern         (X,Y,Z, i32,  len) \
    s_method_vargs   (X,Y,Z, X,    of_objects, A) \
    i_method         (X,Y,Z, A,    first) \
    i_method         (X,Y,Z, A,    last) \
    i_method         (X,Y,Z, X,    pop) \
    i_method         (X,Y,Z, X,    remove, num) \
    i_method         (X,Y,Z, none, push,A) \
    i_method         (X,Y,Z, A,    get, i32) \
    i_method         (X,Y,Z, num,  count) \
    i_method         (X,Y,Z, num,  index_of, A) \
    i_operator       (X,Y,Z, none, assign_add, A) \
    i_operator       (X,Y,Z, none, assign_sub, num) \
    i_method_vargs   (X,Y,Z, none, push_symbols, X, cstr) \
    i_method_vargs   (X,Y,Z, none, push_objects, X, A) \
    i_override_m     (X,Y,Z, bool, boolean) \
    i_index          (X,Y,Z, A,    num) \
    i_cast           (X,Y,Z, bool)
declare_mod(array, collection)

#define string_meta(X,Y,Z) \
    i_intern(X,Y,Z,     cstr,    chars) \
    i_intern(X,Y,Z,     num,     alloc) \
    i_intern(X,Y,Z,     num,     len) \
    i_intern(X,Y,Z,     u64,     h) \
    i_method(X,Y,Z,     array,   split, A) \
    i_method(X,Y,Z,     num,     index_of, cstr) \
    i_construct(X,Y,Z,  sz) \
    i_construct(X,Y,Z,  cstr, num) \
    i_override_m(X,Y,Z, u64, hash)
declare_class(string)

#define vector_meta(X,Y,Z) \
    collection_meta(X,Y,Z) \
    i_method(X,Y,Z,     A,    pop) \
    i_method(X,Y,Z,     none, push,     A) \
    i_method(X,Y,Z,     A,    get,      i32) \
    i_method(X,Y,Z,     num,  count) \
    i_override_m(X,Y,Z, i32,  compare) \
    i_override_m(X,Y,Z, u64,  hash) \
    i_override_m(X,Y,Z, bool, boolean)
declare_mod(vector, collection)

#define EXPAND_ARGS_0()                         0
#define EXPAND_ARGS_1(a)                        1, &a##_type
#define EXPAND_ARGS_2(a, b)                     2, &a##_type, &b##_type
#define EXPAND_ARGS_3(a, b, c)                  3, &a##_type, &b##_type, &c##_type
#define EXPAND_ARGS_4(a, b, c, d)               4, &a##_type, &b##_type, &c##_type, &d##_type
#define EXPAND_ARGS_5(a, b, c, d, e)            5, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type
#define EXPAND_ARGS_6(a, b, c, d, e, f)         6, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type, &f##_type
#define EXPAND_ARGS_7(a, b, c, d, e, f, g)      7, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type, &f##_type, &g##_type
#define EXPAND_ARGS_8(a, b, c, d, e, f, g, h)   8, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type, &f##_type, &g##_type, &h##_type
#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N

#define COUNT_ARGS(...)             COUNT_ARGS_IMPL(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)
#define valloc(T, N)                ((T)A_alloc(typeof(T), N))
#define new(T)                      (valloc(T, 1))
#define ftable(TYPE, INSTANCE)      ((TYPE##_f*)((A)INSTANCE)[-1].type)
#define typeid(INSTANCE)            ((A_t)((A)INSTANCE)[-1].type) 
#define M(T,N,INSTANCE,...)         ftable(T, INSTANCE) -> N(INSTANCE, ##__VA_ARGS__)
#define construct(T,WITH,...)       T##_type.with_##WITH(valloc(T, 1), ## __VA_ARGS__)
#define operator(T,OP,I,...)        ftable(T, I) -> operator_##OP(I, ##__VA_ARGS__)
#define emit_types(...)             EXPAND_ARGS(__VA_ARGS__)
#define combine_tokens_(A, B)       A##B
#define combine_tokens(A, B)        combine_tokens_(A, B)
#define EXPAND_ARGS(...)            EXPAND_ARGS_HELPER(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define EXPAND_ARGS_HELPER(N, ...)  combine_tokens(EXPAND_ARGS_, N)(__VA_ARGS__)

/// we can iterate through a collection with this strange code
#define each(T, t, E, e) \
    if (M(T, count, t)) for (E e = 0; e == 0; e++) \
        for (num __i = 0, len = M(T, count, t); __i < len; __i++, e = M(T, get, t, __i)) \

/// possible to iterate safely through primitives
#define primitives(T, t, E, e) \
    if (M(T, count, t)) for (E e = 0; e == 0; e++) \
        for (num i = 0, len = M(T, count, t); i < len; i++, e = *(E*)M(T, get, t, i)) \

A           A_alloc(A_t type, num count);
A           A_hold(A a);
void        A_drop(A a);
A_f**       A_types(num* length);
member_t*   A_member(A_t type, enum A_TYPE member_type, char* name);
A           A_method(A_t type, char* method_name, array args);
A           A_primitive(A_t type, void* data);
A           A_enum(A_t enum_type, i32 value);
A           A_primitive_i8(i8);
A           A_primitive_u8(u8);
A           A_primitive_i16(i16);
A           A_primitive_u16(u16);
A           A_primitive_i32(i32);
A           A_primitive_u32(u32);
A           A_primitive_i64(i64);
A           A_primitive_u64(u64);
A           A_primitive_f32(f32);
A           A_primitive_f64(f64);
A           A_primitive_cstr(cstr);
A           A_primitive_none();
A           A_primitive_bool(bool);
A           A_realloc(A, num);
void        A_push(A, A);
void        A_finish_types();
A           hold(A a);
void        drop(A a);
A           object(A instance);
A           data(A instance);

#endif