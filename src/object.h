#ifndef _object_h
#define _object_h

typedef struct _Au* Au;

typedef none(*func)    ();
typedef Au  (*hook)    (Au);
typedef Au  (*callback)(Au, Au); // target and argument
typedef Au  (*callback_extra)(Au, Au, Au); // target, argument, argument2


/// our A-type classes have many types of methods
/// constructor, i[nstance]-method, s[tatic]-method, operator 
/// (these are enumeration!), and index.  we index by 1 argument 
/// only in C but we may allow for more in silver
// member enums span multiple use-case, from struct member types, to formatter targeting on arguments
enum AU_MEMBER {
    AU_MEMBER_NONE      = 0,
    AU_MEMBER_TYPE      = 1,
    AU_MEMBER_CONSTRUCT = 2,
    AU_MEMBER_PROP      = 3,
    AU_MEMBER_FUNC      = 4,
    AU_MEMBER_OPERATOR  = 5,
    AU_MEMBER_CAST      = 6,
    AU_MEMBER_INDEX     = 7,
    AU_MEMBER_ENUMV     = 8,
    AU_MEMBER_OVERRIDE  = 9,
    AU_MEMBER_IS_ATTR   = 10,
    AU_MEMBER_FORMATTER = 11,
};

typedef enum AU_MEMBER AFlag;

enum AU_TRAIT {
    AU_TRAIT_PRIMITIVE = 1 << 0,
    AU_TRAIT_INTEGRAL  = 1 << 1,
    AU_TRAIT_REALISTIC = 1 << 2,
    AU_TRAIT_SIGNED    = 1 << 3,
    AU_TRAIT_UNSIGNED  = 1 << 4,
    AU_TRAIT_ENUM      = 1 << 5,
    AU_TRAIT_ALIAS     = 1 << 6,
    AU_TRAIT_ABSTRACT  = 1 << 7,
    AU_TRAIT_STRUCT    = 1 << 8,
    AU_TRAIT_USER_INIT = 1 << 9,
    AU_TRAIT_CLASS     = 1 << 10,
    AU_TRAIT_POINTER   = 1 << 11,
    AU_TRAIT_CONST     = 1 << 12,
    AU_TRAIT_REQUIRED  = 1 << 13,
    AU_TRAIT_SYSTEM    = 1 << 14,
    AU_TRAIT_OVERRIDE  = 1 << 15, // im crash override 
    AU_TRAIT_INLAY     = 1 << 16,
    AU_TRAIT_VPROP     = 1 << 17,
    AU_TRAIT_IMETHOD   = 1 << 18,
    AU_TRAIT_SMETHOD   = 1 << 19,
    AU_TRAIT_TMETHOD   = 1 << 20,
    AU_TRAIT_IFINAL    = 1 << 21,
};

typedef bool(*global_init_fn)();


_Pragma("pack(push, 1)")

typedef struct _Au_t *Au_t;

// auto-free recycler
typedef struct _au_core *au_core;

typedef struct _method_t *method_t; 

typedef struct _object *object;

typedef struct _collective_abi {
    i32             count;
    i32             alloc;
    i32             hsize;
    ARef            origin;
    struct _item*   first, *last;
    struct _vector* hlist;
    bool            unmanaged;
    bool            assorted;
    struct _Au_t*   last_type;
} collective_abi;

typedef struct _Au_t *Au_t;

// this is an exact mock type of A's instance
typedef struct _object {
    Au_t            type;
    Au_t            scalar;
    i64             refs;
    struct _Au*     data;
    struct _shape*  shape;
    cstr            source;
    i64             line;
    i64             alloc;
    i64             count;
    i64             recycle;
    i64             af_index;
    struct _object* meta[8];
    struct _object* f;
} *object;

typedef struct _ffi_method_t ffi_method_t;

// this is the standard _Au_t declaration
typedef struct _Au_t {
    Au_t            context;
    union { Au_t src, rtype, type; };
    Au_t            user;
    Au_t            module; // origin of its module
    Au_t            ptr; // a cache location for the type's pointer
    char*           ident;
    i64             index; // index of type in module, or index of member in type
    object          value; // user-data value associated to type
    int             global_count;
    u8              member_type;
    u8              operator_type;
    u8              access_type;
    u8              reserved;
    union {
        struct {
            u32 is_primitive  : 1;  // AU_TRAIT_PRIMITIVE
            u32 is_integral   : 1;  // AU_TRAIT_INTEGRAL
            u32 is_realistic  : 1;  // AU_TRAIT_REALISTIC
            u32 is_signed     : 1;  // AU_TRAIT_SIGNED
            u32 is_unsigned   : 1;  // AU_TRAIT_UNSIGNED
            u32 is_enum       : 1;  // AU_TRAIT_ENUM
            u32 is_alias      : 1;  // AU_TRAIT_ALIAS
            u32 is_abstract   : 1;  // AU_TRAIT_ABSTRACT
            u32 is_struct     : 1;  // AU_TRAIT_STRUCT
            u32 is_user_init  : 1;  // AU_TRAIT_USER_INIT
            u32 is_class      : 1;  // AU_TRAIT_CLASS
            u32 is_pointer    : 1;  // AU_TRAIT_POINTER
            u32 is_const      : 1;  // AU_TRAIT_CONST
            u32 is_required   : 1;  // AU_TRAIT_REQUIRED
            u32 is_system     : 1;  // AU_TRAIT_SYSTEM
            u32 is_override   : 1;  // AU_TRAIT_OVERRIDE
        };
        u32 traits;
    };345 
    int             offset;
    int             size;
    int             isize;
    void*           fn; // used for function addresses
    ffi_method_t*   ffi;
    au_core         af; // Au-specific internal members on type
    struct _object  members_info;
    struct _collective_abi  members;
    struct _object  meta_info;
    union { struct _collective_abi meta, args; };
    struct _shape*  shape;
    u64             required_bits[2];
    struct {
        void* __none__;
    } ft;
} *Au_t;

#define mdl()

#define TC_mdl(MEMBER, VALUE) ({ /* AF_set((u64*)&instance->f, FIELD_ID(mdl, MEMBER)); */ VALUE; })
#define _ARG_COUNT_IMPL_mdl(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N
#define _ARG_COUNT_I_mdl(...) _ARG_COUNT_IMPL_mdl(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define _ARG_COUNT_mdl(...)   _ARG_COUNT_I_mdl("Au object model", ## __VA_ARGS__)
#define _COMBINE_mdl_(A, B)   A##B
#define _COMBINE_mdl(A, B)    _COMBINE_mdl_(A, B)
#define _N_ARGS_mdl_0( TYPE)
#define _N_ARGS_mdl_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, Au) Au_schema(Au, GENERICS, Au) const void *: (void)0)(instance, a)
#define _N_ARGS_mdl_2( TYPE, a,b) instance->a = TC_mdl(a,b);
#define _N_ARGS_mdl_4( TYPE, a,b, c,d) _N_ARGS_mdl_2(TYPE, a,b) instance->c = TC_mdl(c,d);
#define _N_ARGS_mdl_6( TYPE, a,b, c,d, e,f) _N_ARGS_mdl_4(TYPE, a,b, c,d) instance->e = TC_mdl(e,f);
#define _N_ARGS_mdl_8( TYPE, a,b, c,d, e,f, g,h) _N_ARGS_mdl_6(TYPE, a,b, c,d, e,f) instance->g = TC_mdl(g,h);
#define _N_ARGS_mdl_10( TYPE, a,b, c,d, e,f, g,h, i,j) _N_ARGS_mdl_8(TYPE, a,b, c,d, e,f, g,h) instance->i = TC_mdl(i,j);
#define _N_ARGS_mdl_12( TYPE, a,b, c,d, e,f, g,h, i,j, k,l) _N_ARGS_mdl_10(TYPE, a,b, c,d, e,f, g,h, i,j) instance->k = TC_mdl(k,l);
#define _N_ARGS_mdl_14( TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n) _N_ARGS_mdl_12(TYPE, a,b, c,d, e,f, g,h, i,j, k,l) instance->m = TC_mdl(m,n);
#define _N_ARGS_mdl_16( TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p) _N_ARGS_mdl_14(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n) instance->o = TC_mdl(o,p);
#define _N_ARGS_mdl_18( TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r) _N_ARGS_mdl_16(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p) instance->q = TC_mdl(q,r);
#define _N_ARGS_mdl_20( TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r, s,t) _N_ARGS_mdl_18(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r) instance->s = TC_mdl(s,t);
#define _N_ARGS_mdl_22( TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r, s,t, u,v) _N_ARGS_mdl_20(TYPE, a,b, c,d, e,f, g,h, i,j, k,l, m,n, o,p, q,r, s,t) instance->u = TC_mdl(u,v);
#define _N_ARGS_HELPER2_mdl(TYPE, N, ...)  _COMBINE_mdl(_N_ARGS_mdl_, N)(TYPE, ## __VA_ARGS__)
#define _N_ARGS_mdl(TYPE,...)    _N_ARGS_HELPER2_mdl(TYPE, _ARG_COUNT_mdl(__VA_ARGS__), ## __VA_ARGS__)
#define mdl2(context, ...) ({ \
    mdl instance = (mdl)Au_alloc_member(context); \
    _N_ARGS_mdl(mdl, ## __VA_ARGS__); \
    instance->context = context; \
    instance; \
})

_Pragma("pack(pop)")

#endif