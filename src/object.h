#ifndef _object_h
#define _object_h

typedef struct _Au* Au;

typedef none(*func)    ();
typedef Au  (*hook)    (Au);
typedef Au  (*callback)(Au, Au);
typedef Au  (*callback_extra)(Au, Au, Au);

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
    AU_MEMBER_ARG       = 11,
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
    AU_TRAIT_UNION     = 1 << 9,
    AU_TRAIT_USER_INIT = 1 << 10,
    AU_TRAIT_CLASS     = 1 << 11,
    AU_TRAIT_POINTER   = 1 << 12,
    AU_TRAIT_CONST     = 1 << 13,
    AU_TRAIT_REQUIRED  = 1 << 14,
    AU_TRAIT_SYSTEM    = 1 << 15,
    AU_TRAIT_OVERRIDE  = 1 << 16, // im crash override 
    AU_TRAIT_INLAY     = 1 << 17,
    AU_TRAIT_VPROP     = 1 << 18,
    AU_TRAIT_IMETHOD   = 1 << 19,
    AU_TRAIT_SMETHOD   = 1 << 20,
    AU_TRAIT_TMETHOD   = 1 << 21,
    AU_TRAIT_IFINAL    = 1 << 22,
    AU_TRAIT_FUNCPTR   = 1 << 23,
    AU_TRAIT_SCHEMA    = 1 << 24,
    AU_TRAIT_VARGS     = 1 << 25,
    AU_TRAIT_VOID      = 1 << 26,
    AU_TRAIT_STATIC    = 1 << 27,
    AU_TRAIT_FORMATTER = 1 << 28,
    AU_TRAIT_NAMESPACE = 1 << 29
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
    Au_t            schema;
    struct _etype*  user;
    Au_t            module; // origin of its module
    Au_t            ptr; // a cache location for the type's pointer
    char*           ident;
    i64             index; // index of type in module, or index of member in type
    object          value; // user-data value associated to type
    u8              member_type;
    u8              operator_type;
    u8              access_type;
    u8              reserved;
    union {
        struct {
            u32 is_primitive : 1; // AU_TRAIT_PRIMITIVE = 1 << 0,
            u32 is_integral  : 1; // AU_TRAIT_INTEGRAL  = 1 << 1,
            u32 is_realistic : 1; // AU_TRAIT_REALISTIC = 1 << 2,
            u32 is_signed    : 1; // AU_TRAIT_SIGNED    = 1 << 3,
            u32 is_unsigned  : 1; // AU_TRAIT_UNSIGNED  = 1 << 4,
            u32 is_enum      : 1; // AU_TRAIT_ENUM      = 1 << 5,
            u32 is_alias     : 1; // AU_TRAIT_ALIAS     = 1 << 6,
            u32 is_abstract  : 1; // AU_TRAIT_ABSTRACT  = 1 << 7,
            u32 is_struct    : 1; // AU_TRAIT_STRUCT    = 1 << 8,
            u32 is_union     : 1; // AU_TRAIT_UNION     = 1 << 9,
            u32 is_user_init : 1; // AU_TRAIT_USER_INIT = 1 << 10,
            u32 is_class     : 1; // AU_TRAIT_CLASS     = 1 << 11,
            u32 is_pointer   : 1; // AU_TRAIT_POINTER   = 1 << 12,
            u32 is_const     : 1; // AU_TRAIT_CONST     = 1 << 13,
            u32 is_required  : 1; // AU_TRAIT_REQUIRED  = 1 << 14,
            u32 is_system    : 1; // AU_TRAIT_SYSTEM    = 1 << 15,
            u32 is_override  : 1; // AU_TRAIT_OVERRIDE  = 1 << 16, // im crash override 
            u32 is_inlay     : 1; // AU_TRAIT_INLAY     = 1 << 17,
            u32 is_vprop     : 1; // AU_TRAIT_VPROP     = 1 << 18,
            u32 is_imethod   : 1; // AU_TRAIT_IMETHOD   = 1 << 19,
            u32 is_smethod   : 1; // AU_TRAIT_SMETHOD   = 1 << 20,
            u32 is_tmethod   : 1; // AU_TRAIT_TMETHOD   = 1 << 21,
            u32 is_ifinal    : 1; // AU_TRAIT_IFINAL    = 1 << 22,
            u32 is_funcptr   : 1;
            u32 is_schema    : 1;
            u32 is_vargs     : 1;
            u32 is_void      : 1;
            u32 is_static    : 1;
            u32 is_formatter : 1;
            u32 is_namespace : 1;
        };
        u32 traits;
    };
    int             global_count;
    int             offset;
    int             size;
    int             isize;
    void*           fn;
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

_Pragma("pack(pop)")

#endif