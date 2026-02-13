#ifndef _object_h
#define _object_h

#pragma pack(push, 1)

typedef struct _Au* Au;

typedef none(*func)    ();
typedef Au  (*hook)    (Au);
typedef Au  (*callback)(Au, Au);
typedef Au  (*callback_extra)(Au, Au, Au);

enum AU_MEMBER {
    AU_MEMBER_NONE      = 0,
    AU_MEMBER_MODULE    = 1,
    AU_MEMBER_TYPE      = 2,
    AU_MEMBER_CONSTRUCT = 3,
    AU_MEMBER_VAR       = 4,
    AU_MEMBER_FUNC      = 5,
    AU_MEMBER_OPERATOR  = 6,
    AU_MEMBER_CAST      = 7,
    AU_MEMBER_INDEX     = 8,
    AU_MEMBER_ENUMV     = 9,
    AU_MEMBER_OVERRIDE  = 10,
    AU_MEMBER_IS_ATTR   = 11,
    AU_MEMBER_NAMESPACE = 12,
    AU_MEMBER_DECL      = 13,
    AU_MEMBER_MACRO     = 14
};

typedef enum AU_MEMBER AFlag;

#define AU_TRAIT_PRIMITIVE   ((int64_t)1 << 0)
#define AU_TRAIT_INTEGRAL    ((int64_t) 1 << 1)
#define AU_TRAIT_REALISTIC   ((int64_t) 1 << 2)
#define AU_TRAIT_SIGNED      ((int64_t) 1 << 3)
#define AU_TRAIT_UNSIGNED    ((int64_t) 1 << 4)
#define AU_TRAIT_ENUM        ((int64_t) 1 << 5)
#define AU_TRAIT_ALIAS       ((int64_t) 1 << 6)
#define AU_TRAIT_ABSTRACT    ((int64_t) 1 << 7)
#define AU_TRAIT_STRUCT      ((int64_t) 1 << 8)
#define AU_TRAIT_UNION       ((int64_t) 1 << 9)
#define AU_TRAIT_USER_INIT   ((int64_t) 1 << 10)
#define AU_TRAIT_CLASS       ((int64_t) 1 << 11)
#define AU_TRAIT_POINTER     ((int64_t) 1 << 12)
#define AU_TRAIT_CONST       ((int64_t) 1 << 13)
#define AU_TRAIT_REQUIRED    ((int64_t) 1 << 14)
#define AU_TRAIT_SYSTEM      ((int64_t) 1 << 15)
#define AU_TRAIT_OVERRIDE    ((int64_t) 1 << 16) // im crash override 
#define AU_TRAIT_INLAY       ((int64_t) 1 << 17)
#define AU_TRAIT_VPROP       ((int64_t) 1 << 18)
#define AU_TRAIT_IMETHOD     ((int64_t) 1 << 19)
#define AU_TRAIT_SMETHOD     ((int64_t) 1 << 20)
#define AU_TRAIT_TMETHOD     ((int64_t) 1 << 21)
#define AU_TRAIT_IFINAL      ((int64_t) 1 << 22)
#define AU_TRAIT_FUNCPTR     ((int64_t) 1 << 23)
#define AU_TRAIT_SCHEMA      ((int64_t) 1 << 24)
#define AU_TRAIT_VARGS       ((int64_t) 1 << 25)
#define AU_TRAIT_VOID        ((int64_t) 1 << 26)
#define AU_TRAIT_STATIC      ((int64_t) 1 << 27)
#define AU_TRAIT_FORMATTER   ((int64_t) 1 << 28)
#define AU_TRAIT_NAMESPACE   ((int64_t) 1 << 29)
#define AU_TRAIT_TYPEID      ((int64_t) 1 << 30)
#define AU_TRAIT_MODINIT     ((int64_t) 1 << 31)
#define AU_TRAIT_NAMELESS    ((int64_t) 1 << 32)
#define AU_TRAIT_CLOSED      ((int64_t) 1 << 33)
#define AU_TRAIT_IS_RESOLVING ((int64_t) 1 << 34)
#define AU_TRAIT_HAS_RETURN  ((int64_t) 1 << 35)
#define AU_TRAIT_IS_IMPORTED ((int64_t) 1 << 36)
#define AU_TRAIT_IS_PREPARED ((int64_t) 1 << 37)
#define AU_TRAIT_IS_AU       ((int64_t) 1 << 38)
#define AU_TRAIT_IS_ASSIGNED ((int64_t) 1 << 39)
#define AU_TRAIT_IS_EXPORT   ((int64_t) 1 << 40)
#define AU_TRAIT_IPROP       ((int64_t) 1 << 41)
#define AU_TRAIT_META        ((int64_t) 1 << 42)
#define AU_TRAIT_LAMBDA      ((int64_t) 1 << 43)
#define AU_TRAIT_IS_TARGET   ((int64_t) 1 << 44)
#define AU_TRAIT_FUNCTIONAL  ((int64_t) 1 << 45)
#define AU_TRAIT_IS_HIDDEN   ((int64_t) 1 << 46)
#define AU_TRAIT_ALLOCATED   ((int64_t) 1 << 47)

typedef bool(*global_init_fn)();

typedef struct _Au_t *Au_t;

// auto-free recycler

typedef struct _method_t *method_t; 

typedef struct _object *object;

typedef struct _collective_abi {
    i32             count;
    i32             alloc;
    i32             hsize;
    ARef            origin;
    struct _item*   first, *last;
    struct _item**  hlist;
    bool            unmanaged;
    bool            assorted;
    struct _Au_t*   last_type;
} collective_abi;

typedef struct _Au_t *Au_t;

// this is an exact mock type of A's instance
typedef struct _object {
    Au_t            type;
    Au_t            scalar;
    i32             refs;
    i32             managed;
    struct _Au*     data;
    struct _shape*  shape;
    cstr            source;
    i64             line;
    i64             alloc;
    i64             count;
    bool            members_held;
    struct _object* meta[8];
    struct _object* f;
} *object;

typedef struct _ffi_method_t ffi_method_t;

#ifndef LLVM_VERSION_MAJOR
typedef void* LLVMMetadataRef;
typedef void* LLVMTypeRef;
typedef void* LLVMValueRef;
#endif

typedef struct _Au_t_user {
    struct _aether* mod;
    struct _etype*  etype;

} *Au_t_user;

// this is the standard _Au_t declaration
typedef struct _Au_t {
    Au_t            context;
    union { Au_t src, rtype, type; };
    Au_t            schema;
    Au_t_user       users;
    Au_t            module; // origin of its module
    Au_t            ptr; // a cache location for the type's pointer
    char*           ident;
    char*           alt;
    u32             abi_size;
    u32             align_bits;
    u32             record_alignment;
    i64             index; // index of type in module, or index of member in type
    object          value; // user-data value associated to type
    u8              member_type;
    u8              operator_type;
    u8              access_type;
    u8              reserved;
    union {
        struct {
            u64 is_primitive : 1; // AU_TRAIT_PRIMITIVE = 1 << 0,
            u64 is_integral  : 1; // AU_TRAIT_INTEGRAL  = 1 << 1,
            u64 is_realistic : 1; // AU_TRAIT_REALISTIC = 1 << 2,
            u64 is_signed    : 1; // AU_TRAIT_SIGNED    = 1 << 3,
            u64 is_unsigned  : 1; // AU_TRAIT_UNSIGNED  = 1 << 4,
            u64 is_enum      : 1; // AU_TRAIT_ENUM      = 1 << 5,
            u64 is_alias     : 1; // AU_TRAIT_ALIAS     = 1 << 6,
            u64 is_abstract  : 1; // AU_TRAIT_ABSTRACT  = 1 << 7,
            u64 is_struct    : 1; // AU_TRAIT_STRUCT    = 1 << 8,
            u64 is_union     : 1; // AU_TRAIT_UNION     = 1 << 9,
            u64 is_user_init : 1; // AU_TRAIT_USER_INIT = 1 << 10,
            u64 is_class     : 1; // AU_TRAIT_CLASS     = 1 << 11,
            u64 is_pointer   : 1; // AU_TRAIT_POINTER   = 1 << 12,
            u64 is_const     : 1; // AU_TRAIT_CONST     = 1 << 13,
            u64 is_required  : 1; // AU_TRAIT_REQUIRED  = 1 << 14,
            u64 is_system    : 1; // AU_TRAIT_SYSTEM    = 1 << 15,
            u64 is_override  : 1; // AU_TRAIT_OVERRIDE  = 1 << 16, // im crash override 
            u64 is_inlay     : 1; // AU_TRAIT_INLAY     = 1 << 17,
            u64 is_vprop     : 1; // AU_TRAIT_VPROP     = 1 << 18,
            u64 is_imethod   : 1; // AU_TRAIT_IMETHOD   = 1 << 19,
            u64 is_smethod   : 1; // AU_TRAIT_SMETHOD   = 1 << 20,
            u64 is_tmethod   : 1; // AU_TRAIT_TMETHOD   = 1 << 21,
            u64 is_ifinal    : 1; // AU_TRAIT_IFINAL    = 1 << 22,
            u64 is_funcptr   : 1;
            u64 is_schema    : 1;
            u64 is_vargs     : 1;
            u64 is_void      : 1;
            u64 is_static    : 1;
            u64 is_formatter : 1;
            u64 is_namespace : 1;
            u64 is_typeid    : 1;
            u64 is_mod_init  : 1;
            u64 is_nameless  : 1;
            u64 is_closed    : 1;
            u64 is_resolving : 1;
            u64 has_return   : 1;
            u64 is_imported  : 1;
            u64 is_prepared  : 1;
            u64 is_au        : 1;
            u64 is_assigned  : 1;
            u64 is_export    : 1;
            u64 is_iprop     : 1;
            u64 is_meta      : 1;
            u64 is_lambda    : 1;
            u64 is_target    : 1;
            u64 is_functional : 1;
            u64 is_hidden    : 1;
            u64 is_allocated : 1;
        };
        u64 traits;
    };
    int             global_count;
    int             offset;
    int             elements;
    int             typesize;
    int             isize;
    void*           fn;
    ffi_method_t*   ffi;
    struct _object  members_info;
    struct _collective_abi  members;
    struct _object  meta_info;
    union { struct _collective_abi meta, args; };
    struct _shape*  shape;
    union {
        u64             required_bits[2];
        struct _Au_t_f* __f[2];
    };
    struct {
        void* __none__;
    } ft;
} *Au_t;

#pragma pack(pop)

#endif