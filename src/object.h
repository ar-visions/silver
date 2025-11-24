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
    AU_MEMBER_INLAY     = 4,
    AU_MEMBER_PRIV      = 5,
    AU_MEMBER_INTERN    = 6,
    AU_MEMBER_READ_ONLY = 7,
    AU_MEMBER_IMETHOD   = 8,
    AU_MEMBER_SMETHOD   = 9,
    AU_MEMBER_OPERATOR  = 10,
    AU_MEMBER_CAST      = 11,
    AU_MEMBER_INDEX     = 12,
    AU_MEMBER_ENUMV     = 13,
    AU_MEMBER_OVERRIDE  = 14,
    AU_MEMBER_VPROP     = 15,
    AU_MEMBER_IS_ATTR   = 16,
    AU_MEMBER_OPAQUE    = 17,
    AU_MEMBER_IFINAL    = 18,
    AU_MEMBER_TMETHOD   = 19,
    AU_MEMBER_FORMATTER = 20
};

typedef enum AU_MEMBER AFlag;

enum AU_TRAIT {
    AU_TRAIT_PRIMITIVE = 1,
    AU_TRAIT_INTEGRAL  = 2,
    AU_TRAIT_REALISTIC = 4,
    AU_TRAIT_SIGNED    = 8,
    AU_TRAIT_UNSIGNED  = 16,
    AU_TRAIT_ENUM      = 32,
    AU_TRAIT_ALIAS     = 64,
    AU_TRAIT_ABSTRACT  = 128,
    AU_TRAIT_VECTOR    = 256,
    AU_TRAIT_STRUCT    = 512,
    AU_TRAIT_USER_INIT = 4096,
    AU_TRAIT_CLASS     = 8192,
    AU_TRAIT_POINTER   = 8192 << 2,
    AU_TRAIT_CONST     = 8192 << 3,
    AU_TRAIT_REQUIRED  = 8192 << 4
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
    i32             esize;
    ARef            origin;
    struct _item*   first, *last;
    struct _vector* hlist;
    bool            unmanaged;
    bool            assorted;
    struct _Au_t*   last_type;
} collective_abi;

typedef struct _Au_t* Au_t;

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
} *object;

// this is the standard _Au_t declaration
typedef struct _Au_t {
    Au_t            parent;
    union { Au_t src, rtype, type; };
    Au_t            user;
    Au_t            module; // origin of its module
    char*           ident;
    i64             index; // index of type in module, or index of member in type
    object          value; // user-data value associated to type
    int             global_count;
    int             member_type;
    int             operator_type;
    int             traits;
    int             offset;
    int             size;
    int             isize;
    void*           ptr; // used for function addresses
    au_core         af; // Au-specific internal members on type
    struct _object  members_info;
    struct _collective_abi  members;
    struct _object  meta_info;
    union { struct _collective_abi meta, args; };
    struct _shape*  shape;
    u64             required[2];
} *Au_t;

_Pragma("pack(pop)")

#endif