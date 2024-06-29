#ifndef _A_
#define _A_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/// silver's A-type runtime

typedef void     none;
typedef  int8_t  i8;
typedef  int16_t i16;
typedef  int32_t i32;
typedef  int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef bool     boolean;

typedef float    f32;
typedef double   f64;
typedef long double   f128;
typedef double   real; /// real enough

#define null                            ((void*)0)

#define str(x)                          #x
#define str_args(...)                   str((__VA_ARGS__))

/// our public members and arguments must be A-based
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
    T ## _type.member_count++; \

#define   public_INIT2(T, B, R, N, ...)     
#define   public_PROTO(T, B, R, N, ...)  
#define   public_METHOD(T, B, R, N, ...)   

#define   method_INST(T, B, R, N, ...)
#define   method_TYPE(T, B, R, N, ...)     R (*N)(__VA_ARGS__);
#define   method_INIT(T, B, R, N, ...)     \
    T ## _type . N = & T ## _ ## N; \
    T ## _type.members[T ## _type.member_count].name = #N; \
    T ## _type.members[T ## _type.member_count].args = (struct args_t) { emit_types(__VA_ARGS__) }; \
    T ## _type.members[T ## _type.member_count].type = &R##_type; \
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

typedef uint64_t    sz_t;
typedef bool(*global_init_fn)();
#define A_TRAIT_PRIMITIVE       (1 << 0)

struct args_t {
    int              count;
    struct A_ftable* arg_0;
    struct A_ftable* arg_1;
    struct A_ftable* arg_2;
    struct A_ftable* arg_3;
    struct A_ftable* arg_4;
    struct A_ftable* arg_5;
    struct A_ftable* arg_6;
    struct A_ftable* arg_7;
};

/// methods and properties may use this
/// we may not want to expose members; they will be zero if that is the case
typedef struct member_t {
    char*           name;
    struct A_ftable*type;
    int             offset;
    struct args_t   args;
} member_t;

/// A type members with base-type
#define A_ftable_members(B) \
    struct B ## _ftable* parent; \
    char*           name; \
    sz_t            size; \
    int             member_count; \
    member_t*       members; \
    uint64_t        traits; \
    void*           arb;

#define declare_base( T ) \
    _Pragma("pack(push, 1)") \
    typedef struct T { \
        T ## _meta (T, A, INST) \
    } *T; \
    _Pragma("pack(pop)") \
    typedef struct T##_ftable { \
        A_ftable_members(A) \
        T ## _meta (T, A, TYPE) \
    } T ## _ftable; \
    extern T ## _ftable  T ## _type;

/// declare object and type with f-table from meta-def
#define declare_primitive( T ) \
    typedef struct T##_ftable { \
        A_ftable_members(A) \
        A_meta     (A, A, METHOD) \
    } T ## _ftable; \
    extern T ## _ftable  T ## _type;

/// declare object and type with f-table from meta-def
#define declare_class( T ) \
    _Pragma("pack(push, 1)") \
    typedef struct T { \
        T ## _meta (T, A, INST) \
    } *T; \
    _Pragma("pack(pop)") \
    \
    typedef struct T##_ftable { \
        A_ftable_members(A) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, A, TYPE) \
    } T ## _ftable; \
    extern T ## _ftable  T ## _type;

/// we mod, instead of [calling it] subclass
/// a class can still implement a protocol (initialization check)
#define declare_mod( T, B ) \
    typedef struct T { \
        T ## _meta (T, B, INST) \
    } *T; \
    typedef struct T##_ftable { \
        A_ftable_members(B) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, B, TYPE) \
    } T ## _ftable; \
    extern T ## _ftable  T ## _type;

#define declare_proto( T ) \
    typedef struct T { \
        struct T##_ftable* type; \
        T ## _meta (T, T, INST) \
    } *T; \
    typedef struct T##_ftable { \
        A_ftable_members(A) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, A, TYPE) \
    } T ## _ftable; \
    extern T ## _ftable  T ## _type;

void A_lazy_init(global_init_fn fn);

#define define_global(T, B, TYPE_SZ, TRAIT, META) \
    T ## _ftable T ## _type; \
    static __attribute__((constructor)) bool global_##T() { \
        T ## _ftable* type_ref = &T ## _type; \
        B ## _ftable* base_ref = &B ## _type; \
        if ((A_ftable*)type_ref != (A_ftable*)base_ref && base_ref->size == 0) { \
            A_lazy_init((global_init_fn)&global_##T); \
            return false; \
        } else { \
            memcpy(type_ref, base_ref, sizeof(B ## _ftable)); \
            static member_t members[sizeof(T ## _type) / sizeof(void*)]; \
            T ## _type.parent   = & B ## _type; \
            T ## _type.name     = #T;           \
            T ## _type.size     = TYPE_SZ;      \
            T ## _type.members  = members;      \
            T ## _type.traits   = TRAIT;        \
            META; \
        } \
    }

#define define_mod( T, B )    define_global(T, B, sizeof(struct T), 0, T ## _meta( T, T, INIT ))
#define define_proto( T )     define_global(T, A, sizeof(struct T), 0, T ## _meta( T, T, PROTO ))
#define define_class( T )     define_global(T, A, sizeof(struct T), 0, T ## _meta( T, T, INIT ))
#define define_primitive( T ) define_global(T, A, sizeof(T), A_TRAIT_PRIMITIVE, set_primitive_arb(&T ## _type))

#define A_meta(T, B, AR) \
    intern(T, B, AR, struct A_ftable*, type) \
    intern(T, B, AR, int64_t,          refs) \
    intern(T, B, AR, struct A*,        data) \
    intern(T, B, AR, sz_t,             alloc) \
    intern(T, B, AR, sz_t,             count) \
    intern(T, B, AR, struct A*,        origin) \
    intern(T, B, AR, struct A*,        object) \
    method(T, B, AR, none, init,       T) \
    method(T, B, AR, none, destructor, T) \
    method(T, B, AR, i32,  compare,    T, T)
declare_base(A)

#define string_meta(T, B, AR) \
    intern(T, B, AR, char*,   chars) \
    intern(T, B, AR, int32_t, alloc) \
    intern(T, B, AR, int32_t, len)
declare_class(string)

#define A_typeof(T) ((A_ftable*)&T##_type)

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
declare_primitive(boolean)
declare_primitive(none)
declare_primitive(sz_t)

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
    implement(T, B, AR, A,       get,    T, sz_t)
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
    method(T, B, AR, sz_t,      count, T)
declare_mod(list, collection)

/// array of elemental data
#define array_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, A*,         elements) \
    intern(T, B, AR, i32,        alloc) \
    intern(T, B, AR, i32,        len) \
    method(T, B, AR, T,          pop, T) \
    method(T, B, AR, T,          push,T, A) \
    method(T, B, AR, T,          get, T, i32) \
    method(T, B, AR, T,          count, T)
declare_mod(array, collection)

/// the object is allocated with a certain length, it has that on the object
#define vector_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, i32,        alloc) \
    intern(T, B, AR, i32,        len) \
    method(T, B, AR, A,          pop, T) \
    method(T, B, AR, void,       push,T, A) \
    method(T, B, AR, A,          get, T, i32) \
    method(T, B, AR, sz_t,       count, T)
declare_mod(vector, collection)

string string_new_reserve(int);

A        A_hold(A a); /// only  call this on objects
void     A_drop(A a); /// only  call this on objects
A          hold(A a); /// never call this on objects
void       drop(A a); /// never call this on objects
A         A_new(A_ftable* type, sz_t count);
A        object(A instance);
A          data(A instance);
//A_t*       type(A_alloc_t instance);

#define vnew(T, N) ((T)A_new(A_typeof(T), N))
#define  new(T)    ((T)A_new(A_typeof(T), 1))

#define ftable(TYPE, INSTANCE) ((TYPE##_ftable*)((A)INSTANCE)[-1].type)  
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
#endif