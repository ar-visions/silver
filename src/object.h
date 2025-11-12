#ifndef _object_h
#define _object_h

typedef struct _Au* Au;

typedef none(*func)    ();
typedef Au  (*hook)    (Au);
typedef Au  (*callback)(Au, Au); // target and argument
typedef Au  (*callback_extra)(Au, Au, Au); // target, argument, argument2


/// our A-type classes have many types of methods
/// constructor, i[nstance]-method, s[tatic]-method, operator (these are enumeration!), and index.  we index by 1 argument only in C but we may allow for more in silver
enum AU_FLAG {
    AU_FLAG_NONE      = 0,
    AU_FLAG_CONSTRUCT = 1,
    AU_FLAG_PROP      = 2,
    AU_FLAG_INLAY     = 4,
    AU_FLAG_PRIV      = 8,
    AU_FLAG_INTERN    = 16,
    AU_FLAG_READ_ONLY = 32,
    AU_FLAG_IMETHOD   = 64,
    AU_FLAG_SMETHOD   = 128,
    AU_FLAG_OPERATOR  = 256,
    AU_FLAG_CAST      = 512,
    AU_FLAG_INDEX     = 1024,
    AU_FLAG_ENUMV     = 2048,
    AU_FLAG_OVERRIDE  = 4096,
    AU_FLAG_VPROP     = 8192,
    AU_FLAG_IS_ATTR   = 16384,
    AU_FLAG_OPAQUE    = 32768,
    AU_FLAG_IFINAL    = 32768 << 1,
    AU_FLAG_TMETHOD   = 32768 << 2
};

typedef enum AU_FLAG AFlag;

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
    AU_TRAIT_PTR_SIZE  = 1024,
    AU_TRAIT_PUBLIC    = 2048,
    AU_TRAIT_USER_INIT = 4096,
    AU_TRAIT_CLASS     = 8192,
    AU_TRAIT_BASE      = 8192 << 1,
    AU_TRAIT_POINTER   = 8192 << 2
};

typedef bool(*global_init_fn)();


_Pragma("pack(push, 1)")

typedef struct _Au_t *Au_t;

typedef struct _meta_t {
    long long       count;
    Au_t           meta_0, meta_1, meta_2, meta_3, 
                    meta_4, meta_5, meta_6, meta_7, meta_8, meta_9;
} _meta_t, *meta_t;

// auto-free recycler
typedef struct _af_recycler {
    struct _object** af4;
    i64     af_count;
    i64     af_alloc;
    struct _object** re;
    i64     re_count;
    i64     re_alloc;
} *af_recycler;

typedef struct method_t {
    struct _array*  atypes;
    Au_t           rtype;
    void*           address;
    void*           ffi_cif;  /// ffi-calling info
    void*           ffi_args; /// ffi-data types for args
} method_t;

/*
typedef struct _shape {
    i64 count;
    i64* data;
    bool is_global;
} _shape, *shape;
*/

// this is an exact mock type of A's type, minus the methods it holds
typedef struct _Au_t {
    struct _Au_t*  parent_type;
    char*           name;
    char*           module;
    Au_t*          sub_types;
    i16             sub_types_count;
    i16             sub_types_alloc;
    int             size;
    int             isize;
    af_recycler     af;
    int             magic;
    int             global_count;
    int             vmember_count;
    struct _Au_t*  vmember_type;
    int             member_count;
    struct _member* members;
    int             traits;
    void*           user;
    u64             required[2];
    struct _Au_t*  src;
    void*           arb;
    struct _shape*  shape;
    struct _meta_t  meta;
} *Au_t;

// this is an exact mock type of A's instance
typedef struct _object {
    Au_t           type;
    Au_t           scalar;
    i64             refs;
    struct _A*      data;
    struct _shape*  shape;
    cstr            source;
    i64             line;
    i64             alloc;
    i64             count;
    i64             recycle;
    i64             af_index;
} *object;

typedef struct _member {
    char*           name;
    struct _string* sname;
    Au_t           type;
    int             offset;
    int             count;
    int             member_type;
    int             operator_type;
    int             required;
    struct _meta_t  args;
    void*           ptr;
    void*           method;
    i64             id;
    i64             value;
} *member;

_Pragma("pack(pop)")

#endif