#ifndef _A_
#define _A_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ffi.h>

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

typedef struct A_f* AType;

enum A_TYPE {
    A_TYPE_NONE      = 0,
    A_TYPE_PROP      = 1,
    A_TYPE_IMETHOD   = 2,
    A_TYPE_SMETHOD   = 4,
    A_TYPE_CONSTRUCT = 8,
    A_TYPE_ENUMV     = 16
};

#define null                            ((void*)0)

#define str(x)                          #x
#define str_args(...)                   str((__VA_ARGS__))

#define   enum_value_DECL(E, N)    E##_##N,
#define   enum_value_IMPL(E, N) \
    E##_type.members[E## _type.member_count].name     = #N; \
    E##_type.members[E## _type.member_count].offset   = E##_##N;\
    E##_type.members[E## _type.member_count].type     = &i32_type; \
    E##_type.members[E## _type.member_count].member_type = A_TYPE_ENUMV; \
    E##_type.member_count++; \

#define   enum_value(E, AR, N) enum_value_##AR(E, N)

#define   intern_INST(T, B, R, N, ...)     R N;
#define   intern_TYPE(T, B, R, N, ...)
#define   intern_CTR_MEMBER(T, B, R, N, ...)   
#define   intern_INIT(T, B, R, N, ...)
#define   intern_INIT2(T, B, R, N, ...)    
#define   intern_PROTO(T, B, R, N, ...)    
#define   intern_METHOD(T, B, R, N, ...)    
#define   intern(T, B, AR, R, N, ...)  intern_    ##AR(T, B, R, N, __VA_ARGS__)

#define   public_INST(T, B, R, N, ...)     R N;
#define   public_TYPE(T, B, R, N, ...)      
#define   public_CTR_MEMBER(T, B, R, N, ...)
#define   public_INIT(T, B, R, N, ...) \
    T ## _type.members[T ## _type.member_count].name     = #N; \
    T ## _type.members[T ## _type.member_count].offset   = offsetof(struct T, N); \
    T ## _type.members[T ## _type.member_count].type     = &R##_type; \
    T ## _type.members[T ## _type.member_count].member_type = A_TYPE_PROP; \
    T ## _type.member_count++; \

#define   public_INIT2(T, B, R, N, ...)     
#define   public_PROTO(T, B, R, N, ...)  
#define   public_METHOD(T, B, R, N, ...)   
#define   public(T, B, AR, R, N, ...)  public_    ##AR(T, B, R, N, __VA_ARGS__)

#define   smethod_INST(T, B, R, N, ...)
#define   smethod_TYPE(T, B, R, N, ...)     R (*N)(__VA_ARGS__);
#define   smethod_CTR_MEMBER(T, B, R, N, ...)
#define   smethod_INIT(T, B, R, N, ...)     \
    T ## _type . N = & T ## _ ## N; \
    T ## _type.members[T ## _type.member_count].name = #N; \
    T ## _type.members[T ## _type.member_count].args = (args_t) { emit_types(__VA_ARGS__) }; \
    T ## _type.members[T ## _type.member_count].type = &R##_type; \
    T ## _type.members[T ## _type.member_count].offset = offsetof(T##_f, N); \
    T ## _type.members[T ## _type.member_count].member_type = A_TYPE_SMETHOD; \
    T ## _type.member_count++;
#define   smethod_INIT2(T, B, R, N, ...)    
#define   smethod_PROTO(T, B, R, N, ...)
#define   smethod_METHOD(T, B, R, N, ...)   R (*N)(__VA_ARGS__);
#define   smethod(T, B, AR, R, N, ...)      smethod_ ## AR(T, B, R, N, ##__VA_ARGS__)

#define   imethod_INST(T, B, R, N, ...)
#define   imethod_TYPE(T, B, R, N, ...)     R (*N)(__VA_ARGS__);
#define   imethod_CTR_MEMBER(T, B, R, N, ...)
#define   imethod_INIT(T, B, R, N, ...)     \
    T ## _type . N = & T ## _ ## N; \
    T ## _type.members[T ## _type.member_count].name = #N; \
    T ## _type.members[T ## _type.member_count].args = (args_t) { emit_types(__VA_ARGS__) }; \
    T ## _type.members[T ## _type.member_count].type = &R##_type; \
    T ## _type.members[T ## _type.member_count].offset = offsetof(T##_f, N); \
    T ## _type.members[T ## _type.member_count].member_type = A_TYPE_IMETHOD; \
    T ## _type.member_count++;
#define   imethod_INIT2(T, B, R, N, ...)    
#define   imethod_PROTO(T, B, R, N, ...)
#define   imethod_METHOD(T, B, R, N, ...)   R (*N)(__VA_ARGS__);
#define   imethod(T, B, AR, R, N, ...)      imethod_ ## AR(T, B, R, N, T, ##__VA_ARGS__)

/// the constructor has one more arg than we define explicitly; that is its type given to constructor
/// this is so we can generalize that (although, this is in parallel to A_alloc, 
/// effectively unless we override this one
#define   construct_INST(T, B, R, N, ...)
#define   construct_TYPE(T, B, R, N, ...)
#define   construct_CTR_MEMBER(T, B, R, N, ...)   R (*T##_##N)(struct T##_f*, ##__VA_ARGS__);
#define   construct_INIT(T, B, R, N, ...)     \
    T ## _new  . T##_##N = & T ## _ ## N; \
    assert(#T == #R); \
    T ## _type.members[T ## _type.member_count].name = #N; \
    T ## _type.members[T ## _type.member_count].args = (args_t) { emit_types(T, ##__VA_ARGS__) }; \
    T ## _type.members[T ## _type.member_count].type = &R##_type; \
    T ## _type.members[T ## _type.member_count].offset = offsetof(T##_ctr, T##_##N); \
    T ## _type.members[T ## _type.member_count].member_type = A_TYPE_CONSTRUCT; \
    T ## _type.member_count++;
#define   construct_INIT2(T, B, R, N, ...)    
#define   construct_PROTO(T, B, R, N, ...)
#define   construct_METHOD(T, B, R, N, ...)

#define   construct(T, B, AR, R, N, ...)      construct_ ##AR(T, B, R, N, ##__VA_ARGS__)

#define   imethod_vargs_INST(T, B, R, N, ...)
#define   imethod_vargs_TYPE(T, B, R, N, ...)     R (*N)(__VA_ARGS__, ...);
#define   imethod_vargs_CTR_MEMBER(T, B, R, N, ...)
#define   imethod_vargs_INIT(T, B, R, N, ...)     imethod_INIT(T, B, R, N, __VA_ARGS__)
#define   imethod_vargs_INIT2(T, B, R, N, ...)    
#define   imethod_vargs_PROTO(T, B, R, N, ...)
#define   imethod_vargs_METHOD(T, B, R, N, ...)   R (*N)(__VA_ARGS__, ...);
#define   imethod_vargs(T, B, AR, R, N, ...)      imethod_vargs_##AR(T, B, R, N, __VA_ARGS__)

#define   implement_INST(T, B, R, N, ...)
#define   implement_TYPE(T, B, R, N, ...)   
#define   implement_CTR_MEMBER(T, B, R, N, ...)  
#define   implement_INIT(T, B, R, N, ...)
#define   implement_PROTO(T, B, R, N, ...)
#define   implement_METHOD(T, B, R, N, ...)
#define   implement(T, B, AR, R, N, ...)         implement_##AR(T, B, R, N, __VA_ARGS__)

#define   method_override_INST(T, B, R, N, ...)
#define   method_override_TYPE(T, B, R, N, ...)
#define   method_override_CTR_MEMBER(T, B, R, N, ...)         
#define   method_override_INIT(T, B, R, N, ...)     T ## _type . N = & T ## _ ## N;
#define   method_override_PROTO(T, B, R, N, ...)
#define   method_override_METHOD(T, B, R, N, ...)
#define   method_override(T, B, AR, R, N, ...)  method_override_  ##AR(T, B, R, N, __VA_ARGS__)

#define   construct_override_INST(T, B, R, N, ...)
#define   construct_override_TYPE(T, B, R, N, ...)
#define   construct_override_CTR_MEMBER(T, B, R, N, ...)         
#define   construct_override_INIT(T, B, R, N, ...)     T ## _new . N = & T ## _ ## N;
#define   construct_override_PROTO(T, B, R, N, ...)
#define   construct_override_METHOD(T, B, R, N, ...)
#define   construct_override(T, B, AR, R, N, ...)  override_  ##AR(T, B, R, N, __VA_ARGS__)


typedef bool(*global_init_fn)();
#define A_TRAIT_PRIMITIVE       (1 << 0)
#define A_TRAIT_ENUM            (1 << 1)

#define FNV_PRIME    0x100000001b3
#define OFFSET_BASIS 0xcbf29ce484222325

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
    struct A_ctr*   factory;\
    void*           arb;

#define declare_inst( T, B ) \
    _Pragma("pack(push, 1)") \
    typedef struct T { \
        T ## _meta (T, B, INST) \
    } *T; \
    _Pragma("pack(pop)") \

#define declare_base( T ) \
    declare_inst(T, A) \
    typedef struct T##_f { \
        A_f_members(A) \
        T ## _meta (T, A, TYPE) \
    } T ## _f; \
    typedef struct T##_ctr { \
        A_meta (T, A, CTR_MEMBER) \
    } T ## _ctr; \
    extern T ## _f   T ## _type;

/// declare object and type with f-table from meta-def
#define declare_primitive( T ) \
    typedef struct T##_f { \
        A_f_members(A) \
        A_meta     (A, A, METHOD) \
    } T ## _f, *T##_t; \
    extern T ## _f   T ## _type;

#define declare_enum( E ) \
    typedef enum E { \
        E##_meta(E, DECL) \
    } E; \
    typedef struct E##_f { \
        A_f_members(A) \
        A_meta     (E, A, METHOD) \
    } E ## _f, *E##_t; \
    extern E##_f   E##_type;

/// we currently have no actual way to do poly with construct, 
/// less we give a type into constructor?
/// the idea of constructor registration is to have automatic type conversion, and then
/// the user may override construction
/// its a decent amount of registration
/// since the type is resolved prior to the call, we can construct initially there
/// is there a constructor that doesnt do a new() immediately?  lets be frank
/// it has its upsides to return a type instance based on data
/// that return instance may not be a 'new' at all

/// declare object and type with f-table from meta-def
#define declare_class( T ) \
    declare_inst(T, A) \
    typedef struct T##_f { \
        A_f_members(A) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, A, TYPE) \
    } T ## _f, *T##_t; \
    typedef struct T##_ctr { \
        A_meta (T, A, CTR_MEMBER) \
        T ## _meta (T, A, CTR_MEMBER) \
    } T ## _ctr; \
    extern T ## _ctr T ## _new; \
    extern T ## _f   T ## _type;

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
#define declare_mod( T, B ) \
    declare_inst(T, B) \
    typedef struct T##_f { \
        A_f_members(B) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, B, TYPE) \
    } T ## _f, *T##_t; \
    typedef struct T##_ctr { \
        T ## _meta (T, A, CTR_MEMBER) \
    } T ## _ctr; \
    extern T ## _ctr T ## _new; \
    extern T ## _f   T ## _type;

#define declare_proto( T ) \
    declare_inst(T, T) \
    typedef struct T##_f { \
        A_f_members(A) \
        A_meta     (T, A, METHOD) \
        T ## _meta (T, A, TYPE) \
    } T ## _f, *T##_t; \
    extern T ## _f  T ## _type;

void A_lazy_init(global_init_fn fn);

#define define_global(T, B, TYPE_SZ, TRAIT, META) \
    T##_f   T##_type; \
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

#define define_primitive( T, ffi_type ) \
    define_global(T, A, sizeof(T), A_TRAIT_PRIMITIVE, T ## _type.arb = &ffi_type; T##_type.factory = &A_new;)

#define define_class( T ) \
    T##_ctr T##_new; \
    define_global(T, A, sizeof(struct T), 0, T ## _type.arb = &ffi_type_pointer; T##_type.factory = &T##_new; T ## _meta( T, A, INIT ))

#define define_mod( T, B ) \
    T ## _ctr T ## _new; \
    define_global(T, B, sizeof(struct T), 0, T ## _type.arb = &ffi_type_pointer; T##_type.factory = &T##_new; T ## _meta( T, B, INIT ))

#define define_proto( T ) \
    define_global(T, A, sizeof(struct T), 0, T ## _meta( T, T, PROTO ))

/// constructors get a type forwarded from the construct macro
#define A_meta(T, B, AR) \
    intern( T, B, AR, A_t,              type) \
    intern( T, B, AR, num,              refs) \
    intern( T, B, AR, struct A*,        data) \
    intern( T, B, AR, num,              alloc) \
    intern( T, B, AR, num,              count) \
    intern( T, B, AR, struct A*,        origin) \
    construct(T, B, AR, T,              default) \
    imethod(T, B, AR, none, init) \
    imethod(T, B, AR, none, destructor) \
    imethod(T, B, AR, i32,  compare,    T) \
    imethod(T, B, AR, u64,  hash) \
    imethod(T, B, AR, bool, boolean)
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
declare_primitive(cstr)
declare_primitive(bool)
declare_primitive(none)
declare_primitive(num)

/// whatever we can 'name', we can handle as a type of any pointer primitive
declare_primitive(AType)

/// doubly-linked item type
#define item_meta(T, B, AR) \
    intern(T, B, AR, struct T*, next) \
    intern(T, B, AR, struct T*, prev) \
    intern(T, B, AR, A,         element)
declare_class(item)

/// abstract collection type
#define collection_meta(T, B, AR) \
    implement(T, B, AR, i64,     count) \
    implement(T, B, AR, none,    push,   A) \
    implement(T, B, AR, A,       pop) \
    implement(T, B, AR, A,       get,    num)
declare_proto(collection)

/// linked-list of elemental data
#define list_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, item,      first) \
    intern(T, B, AR, item,      last)  \
    intern(T, B, AR, i64,       count) \
    public(T, B, AR, i32,       public_integer) \
    imethod(T, B, AR, A,         pop) \
    imethod(T, B, AR, none,      push, A) \
    imethod(T, B, AR, A,         get,  i32) \
    imethod(T, B, AR, num,       count)
declare_mod(list, collection)

typedef char* cstr;

/// array of elemental data
#define array_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    intern(T, B, AR, A*,         elements) \
    intern(T, B, AR, i32,        alloc) \
    intern(T, B, AR, i32,        len) \
    imethod(T, B, AR, T,         pop) \
    imethod(T, B, AR, none,      push,A) \
    imethod(T, B, AR, A,         get, i32) \
    imethod(T, B, AR, T,         count) \
    imethod(T, B, AR, num,       index_of, A) \
    imethod_vargs(T, B, AR, none, push_symbols, T, cstr) \
    imethod_vargs(T, B, AR, none, push_objects, T, A) \
    method_override(T, B, AR, bool, boolean)
declare_mod(array, collection)

#define string_meta(T, B, AR) \
    intern(T, B, AR, char*,   chars) \
    intern(T, B, AR, num,     alloc) \
    intern(T, B, AR, num,     len) \
    intern(T, B, AR, u64,     h) \
    imethod(T, B, AR, array,   split, A) \
    imethod(T, B, AR, num,     index_of, cstr) \
    construct(T,B,AR, T, of_reserve, num) \
    construct(T,B,AR, T, of_cstr, cstr, num) \
    method_override(T, B, AR, u64, hash)
declare_class(string)

/// vector has no members, which means it can access any primitive
#define vector_meta(T, B, AR) \
    collection_meta(T, B, AR) \
    imethod(T, B, AR,  A,         pop) \
    imethod(T, B, AR,  none,      push,  A) \
    imethod(T, B, AR,  A,         get,   i32) \
    imethod(T, B, AR,  num,       count) \
    method_override(T, B, AR, i32,  compare,    T) \
    method_override(T, B, AR, u64,  hash) \
    method_override(T, B, AR, bool, boolean)
declare_mod(vector, collection)

A        A_hold(A a); /// only  call this on objects
void     A_drop(A a); /// only  call this on objects
A          hold(A a); /// never call this on objects
void       drop(A a); /// never call this on objects

/// its good to have a vector length in the constructor (cannot be interpreted as such in all cases though)
A         A_alloc(A_t type, num count);

A        object(A instance);
A          data(A instance);
//A_t*       type(A_alloc_t instance);




//#define   new(T)     ((T)A_alloc(typeof(T), 1))

// a useful new keyword to handle default construction, or additional processing with named ones
#define COUNT_ARGS(...) COUNT_ARGS_IMPL(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)
#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define valloc(T, N)            ((T)A_alloc(typeof(T), N))
#define NEW_1(T)                valloc(T, 1)
#define NEW_3(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_4(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_5(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_6(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_7(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_8(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_9(T, CTR, ...)      T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_10(T, CTR, ...)     T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_11(T, CTR, ...)     T##_new.T##_##CTR(valloc(T, 1), ## __VA_ARGS__)
#define NEW_IMPL2(N, T, ...)    NEW_##N(T, ##__VA_ARGS__)
#define NEW_IMPL(N, T, ...)     NEW_IMPL2(N, T, ##__VA_ARGS__)
#define new(T, ...)             NEW_IMPL(COUNT_ARGS(__VA_ARGS__), T, ##__VA_ARGS__)

#define ftable(TYPE, INSTANCE) ((TYPE##_f*)((A)INSTANCE)[-1].type)  
#define typeid(INSTANCE) ((A_t)((A)INSTANCE)[-1].type) 


#define M(TYPE,METHOD,INSTANCE,...) ftable(TYPE, INSTANCE) -> METHOD(INSTANCE, ##__VA_ARGS__)

/// emit (&A_type, &string_type) from (A, string)
/// for primitive types, these are translated -- this means a macro cannot use objects in primitive form
#define emit_types(...) EXPAND_ARGS(__VA_ARGS__)
#define combine_tokens_(A, B) A ## B
#define combine_tokens(A, B) combine_tokens_(A, B)
#define EXPAND_ARGS(...) \
    EXPAND_ARGS_HELPER(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)
#define EXPAND_ARGS_HELPER(N, ...) \
    combine_tokens(EXPAND_ARGS_, N)(__VA_ARGS__)

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

#define enums(ET, DEF, FIRST, ...) \

void A_push_type(A_t type);

A_f** A_types(num* length);
member_t* A_member(A_t type, enum A_TYPE member_type, char* name);
A A_method(A_t type, char* method_name, array args);
A A_primitive(A_t type, void* data);
A A_enum(A_t enum_type, i32 value);
A A_primitive_i8(i8);
A A_primitive_u8(u8);
A A_primitive_i16(i16);
A A_primitive_u16(u16);
A A_primitive_i32(i32);
A A_primitive_u32(u32);
A A_primitive_i64(i64);
A A_primitive_u64(u64);
A A_primitive_f32(f32);
A A_primitive_f64(f64);
A A_primitive_cstr(cstr);
A A_primitive_none();
A A_primitive_bool(bool);

A A_realloc(A, num);
void A_push(A, A);

void A_finish_types();

#endif