#ifndef _A_
#define _A_

#include <stdio.h>
#include <stdlib.h>

/// A-type runtime
#define true                (bool)1
#define false               (bool)0

typedef void                none;
typedef signed char         i8;
typedef short               i16;
typedef int                 i32;
typedef long long           i64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef unsigned long long  num;
typedef unsigned int        bool;
typedef float               f32;
typedef double              f64;
typedef long double         f128;
typedef double              real;    /// real enough

enum A_TYPE {
    A_TYPE_NONE,
    A_TYPE_PROP,
    A_TYPE_METHOD
};

#define null                            ((void*)0)

#define str(x)                          #x
#define str_args(...)                   str((__VA_ARGS__))

#define   intern_INST(T, B, R, N, ...)     R N;
#define   intern_TYPE(T, B, R, N, ...)   
#define   intern_INIT(T, B, R, N, ...)
#define   intern_INIT2(T, B, R, N, ...)    
#define   intern_PROTO(T, B, R, N, ...)    
#define   intern_METHOD(T, B, R, N, ...)    

#define   public_INST(T, B, R, N, ...)     R N;
#define   public_TYPE(T, B, R, N, ...)      
#define   public_INIT(T, B, R, N, ...) \
    T ## _type.members[T ## _type.member_count].name     = #N; \
    T ## _type.members[T ## _type.member_count].offset   = offsetof(struct T, N); \
    T ## _type.members[T ## _type.member_count].type     = &R##_type; \
    T ## _type.members[T ## _type.member_count].member_type = A_TYPE_PROP; \
    T ## _type.member_count++; \

#define   public_INIT2(T, B, R, N, ...)     
#define   public_PROTO(T, B, R, N, ...)  
#define   public_METHOD(T, B, R, N, ...)   

#define   method_INST(T, B, R, N, ...)
#define   method_TYPE(T, B, R, N, ...)     R (*N)(__VA_ARGS__);
#define   method_INIT(T, B, R, N, ...)     \
    T ## _type . N = & T ## _ ## N; \
    T ## _type.members[T ## _type.member_count].name = #N; \
    T ## _type.members[T ## _type.member_count].args = (args_t) { emit_types(__VA_ARGS__) }; \
    T ## _type.members[T ## _type.member_count].type = &R##_type; \
    T ## _type.members[T ## _type.member_count].offset = offsetof(T##_f, N); \
    T ## _type.members[T ## _type.member_count].member_type = A_TYPE_METHOD; \
    T ## _type.member_count++; \

#define   method_INIT2(T, B, R, N, ...)    
#define   method_PROTO(T, B, R, N, ...)
#define   method_METHOD(T, B, R, N, ...)   R (*N)(__VA_ARGS__);

#define   implement_INST(T, B, R, N, ...)
#define   implement_TYPE(T, B, R, N, ...)     
#define   implement_INIT(T, B, R, N, ...)
#define   implement_PROTO(T, B, R, N, ...)
#define   implement_METHOD(T, B, R, N, ...)

#define   override_INST(T, B, R, N, ...)
#define   override_TYPE(T, B, R, N, ...)      
#define   override_INIT(T, B, R, N, ...)     T ## _type . N = & T ## _ ## N;
#define   override_PROTO(T, B, R, N, ...)
#define   override_METHOD(T, B, R, N, ...)

/// user macros
#define     implement(T, B, AR, R, N, ...)  implement_ ##AR(T, B, R, N, __VA_ARGS__)
#define        method(T, B, AR, R, N, ...)  method_    ##AR(T, B, R, N, __VA_ARGS__)
#define      override(T, B, AR, R, N, ...)  override_  ##AR(T, B, R, N, __VA_ARGS__)
#define        intern(T, B, AR, R, N, ...)  intern_    ##AR(T, B, R, N, __VA_ARGS__)
#define        public(T, B, AR, R, N, ...)  public_    ##AR(T, B, R, N, __VA_ARGS__)


typedef bool(*global_init_fn)();
#define A_TRAIT_PRIMITIVE       (1 << 0)

typedef struct A_f* A_t;
 
typedef struct args_t {
    num             count;
    A_t             arg_0, arg_1, arg_2, arg_3, 
                    arg_4, arg_5, arg_6, arg_7;
} args_t;

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

#define declare_base( T ) \
    _Pragma("pack(push, 1)") \
    typedef struct T { \
        T ## _meta (T, A, INST) \
    } *T; \
    _Pragma("pack(pop)") \
    typedef struct T##_f { \
        A_f_members(A) \
        T ## _meta (T, A, TYPE) \
    } T ## _f; \
    extern T ## _f  T ## _type;

/// declare object and type with f-table from meta-def
#define declare_primitive( T ) \
    typedef struct T##_f { \
        A_f_members(A) \
        A_meta     (A, A, METHOD) \
    } T ## _f; \
    extern T ## _f  T ## _type;

/// declare object and type with f-table from meta-def
#define declare_class( T ) \
    _Pragma("pack(push, 1)") \
    typedef struct T { \
        T ## _meta (T, A, INST) \
    } *T; \
    _Pragma("pack(pop)") \
    \
    typedef struct T##_f { \
        A_f_members(A) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, A, TYPE) \
    } T ## _f; \
    extern T ## _f  T ## _type;

/// we mod, instead of [calling it] subclass
/// a class can still implement a protocol (initialization check)
#define declare_mod( T, B ) \
    typedef struct T { \
        T ## _meta (T, B, INST) \
    } *T; \
    typedef struct T##_f { \
        A_f_members(B) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, B, TYPE) \
    } T ## _f; \
    extern T ## _f  T ## _type;

#define declare_proto( T ) \
    typedef struct T { \
        struct T##_f* type; \
        T ## _meta (T, T, INST) \
    } *T; \
    typedef struct T##_f { \
        A_f_members(A) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, A, TYPE) \
    } T ## _f; \
    extern T ## _f  T ## _type;

void A_lazy_init(global_init_fn fn);

#define define_global(T, B, TYPE_SZ, TRAIT, META) \
    T ## _f T ## _type; \
    static __attribute__((constructor)) bool global_##T() { \
        T ## _f* type_ref = &T ## _type; \
        B ## _f* base_ref = &B ## _type; \
        if ((A_t)type_ref != (A_t)base_ref && base_ref->size == 0) { \
            A_lazy_init((global_init_fn)&global_##T); \
            return false; \
        } else { \
            memcpy(type_ref, base_ref, sizeof(B ## _f)); \
            static member_t members[sizeof(T ## _type) / sizeof(void*)]; \
            T ## _type.parent   = & B ## _type; \
            T ## _type.name     = #T;           \
            T ## _type.size     = TYPE_SZ;      \
            T ## _type.members  = members;      \
            T ## _type.traits   = TRAIT;        \
            META;                               \
            A_push_type(&T##_type);             \
            return true;                        \
        }                                       \
    }

#define define_mod( T, B )    define_global(T, B, sizeof(struct T), 0, T ## _type.arb = &ffi_type_pointer; T ## _meta( T, B, INIT ))
#define define_proto( T )     define_global(T, A, sizeof(struct T), 0, T ## _meta( T, T, PROTO ))
#define define_class( T )     define_global(T, A, sizeof(struct T), 0, T ## _type.arb = &ffi_type_pointer; T ## _meta( T, A, INIT ))
#define define_primitive( T, ffi_type ) define_global(T, A, sizeof(T), A_TRAIT_PRIMITIVE, T ## _type.arb = &ffi_type)

#define A_meta(T, B, AR) \
    intern(T, B, AR, A_t,              type) \
    intern(T, B, AR, num,              refs) \
    intern(T, B, AR, struct A*,        data) \
    intern(T, B, AR, num,              alloc) \
    intern(T, B, AR, num,              count) \
    intern(T, B, AR, struct A*,        origin) \
    intern(T, B, AR, struct A*,        object) \
    method(T, B, AR, none, init,       T) \
    method(T, B, AR, none, destructor, T) \
    method(T, B, AR, i32,  compare,    T, T) \
    method(T, B, AR, u64,  hash,       T)
declare_base(A)

#define typeof(T) ((struct A_f*)&T##_type)

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
declare_primitive(bool)
declare_primitive(none)
declare_primitive(num)

/// doubly-linked item type
#define item_meta(T, B, AR) \
    intern(T, B, AR, struct T*, next) \
    intern(T, B, AR, struct T*, prev) \
    intern(T, B, AR, A,         element)
declare_class(item)

/// abstract collection type
#define collection_meta(T, B, AR) \
    implement(T, B, AR, i64,     count,  T) \
    implement(T, B, AR, none,    push,   T, A) \
    implement(T, B, AR, A,       pop,    T) \
    implement(T, B, AR, A,       get,    T, num)
declare_proto(collection)

/// linked-list of elemental data
#define list_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, item,      first) \
    intern(T, B, AR, item,      last)  \
    intern(T, B, AR, i64,       count) \
    public(T, B, AR, i32,       public_integer) \
    method(T, B, AR, A,         pop,  T) \
    method(T, B, AR, none,      push, T, A) \
    method(T, B, AR, A,         get,  T, i32) \
    method(T, B, AR, num,       count, T)
declare_mod(list, collection)

/// array of elemental data
#define array_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, A*,         elements) \
    intern(T, B, AR, i32,        alloc) \
    intern(T, B, AR, i32,        len) \
    method(T, B, AR, T,          pop, T) \
    method(T, B, AR, none,       push,T, A) \
    method(T, B, AR, A,          get, T, i32) \
    method(T, B, AR, T,          count, T)
declare_mod(array, collection)

#define string_meta(T, B, AR) \
    intern(T, B, AR, char*,   chars) \
    intern(T, B, AR, num,     alloc) \
    intern(T, B, AR, num,     len) \
    intern(T, B, AR, u64,     h) \
    method(T, B, AR, array,   split, T, A) \
    override(T, B, AR, u64, hash)
declare_class(string)

/// the object is allocated with a certain length, it has that on the object
#define vector_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, i32,        alloc) \
    intern(T, B, AR, i32,        len) \
    method(T, B, AR, A,          pop,   T) \
    method(T, B, AR, void,       push,  T, A) \
    method(T, B, AR, A,          get,   T, i32) \
    method(T, B, AR, num,        count, T)
declare_mod(vector, collection)

string string_new_reserve(num);

A        A_hold(A a); /// only  call this on objects
void     A_drop(A a); /// only  call this on objects
A          hold(A a); /// never call this on objects
void       drop(A a); /// never call this on objects
A         A_new(A_t type, num count);
A        object(A instance);
A          data(A instance);
//A_t*       type(A_alloc_t instance);

#define vnew(T, N) ((T)A_new(typeof(T), N))
#define  new(T)    ((T)A_new(typeof(T), 1))

#define ftable(TYPE, INSTANCE) ((TYPE##_f*)((A)INSTANCE)[-1].type)  
#define typeid(INSTANCE) ((A_t)((A)INSTANCE)[-1].type) 
#define M(TYPE,METHOD,INSTANCE,...) ftable(TYPE, INSTANCE) -> METHOD(INSTANCE, __VA_ARGS__)

/// emit (&A_type, &string_type) from (A, string)
/// for primitive types, these are translated -- this means a macro cannot use objects in primitive form
#define emit_types(...) EXPAND_ARGS(__VA_ARGS__)
#define combine_tokens_(A, B) A ## B
#define combine_tokens(A, B) combine_tokens_(A, B)
#define EXPAND_ARGS(...) \
    EXPAND_ARGS_HELPER(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define EXPAND_ARGS_HELPER(N, ...) \
    combine_tokens(EXPAND_ARGS_, N)(__VA_ARGS__)
#define COUNT_ARGS(...) \
    COUNT_ARGS_HELPER(__VA_ARGS__, 4, 3, 2, 1)

#define COUNT_ARGS_HELPER(_1, _2, _3, _4, N, ...) N

/// 8 is a good number of arguments
#define EXPAND_ARGS_0()                         0
#define EXPAND_ARGS_1(a)                        1, &a##_type
#define EXPAND_ARGS_2(a, b)                     2, &a##_type, &b##_type
#define EXPAND_ARGS_3(a, b, c)                  3, &a##_type, &b##_type, &c##_type
#define EXPAND_ARGS_4(a, b, c, d)               4, &a##_type, &b##_type, &c##_type, &d##_type
#define EXPAND_ARGS_5(a, b, c, d, e)            5, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type
#define EXPAND_ARGS_6(a, b, c, d, e, f)         6, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type, &f##_type
#define EXPAND_ARGS_7(a, b, c, d, e, f, g)      7, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type, &f##_type, &g##_type
#define EXPAND_ARGS_8(a, b, c, d, e, f, g, h)   8, &a##_type, &b##_type, &c##_type, &d##_type, &e##_type, &f##_type, &g##_type, &h##_type

void A_push_type(A_t type);

A_f** A_types(num* length);
member_t* A_find_member(A_t type, enum A_TYPE member_type, char* name);
A A_method(A_t type, char* method_name, array args);
A A_primitive(A_t type, void* data);
A A_primitive_i8(i8 data);
A A_primitive_u8(u8 data);
A A_primitive_i16(i16 data);
A A_primitive_u16(u16 data);
A A_primitive_i32(i32 data);
A A_primitive_u32(u32 data);
A A_primitive_i64(i64 data);
A A_primitive_u64(u64 data);
A A_primitive_f32(f32 data);
A A_primitive_f64(f64 data);
A A_primitive_none();
A A_primitive_bool(bool data);

void A_finish_types();

#endif