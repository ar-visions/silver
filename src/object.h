#ifndef _object_h
#define _object_h

typedef struct _A* A;

typedef none(*fn)      ();
typedef A   (*hook)    (A);
typedef A   (*callback)(A, A); // target and argument


/// our A-type classes have many types of methods
/// constructor, i[nstance]-method, s[tatic]-method, operator (these are enumeration!), and index.  we index by 1 argument only in C but we may allow for more in silver
enum A_FLAG {
    A_FLAG_NONE      = 0,
    A_FLAG_CONSTRUCT = 1,
    A_FLAG_PROP      = 2,
    A_FLAG_INLAY     = 4,
    A_FLAG_PRIV      = 8,
    A_FLAG_INTERN    = 16,
    A_FLAG_READ_ONLY = 32,
    A_FLAG_IMETHOD   = 64,
    A_FLAG_SMETHOD   = 128,
    A_FLAG_OPERATOR  = 256,
    A_FLAG_CAST      = 512,
    A_FLAG_INDEX     = 1024,
    A_FLAG_ENUMV     = 2048,
    A_FLAG_OVERRIDE  = 4096,
    A_FLAG_VPROP     = 8192,
    A_FLAG_IS_ATTR   = 16384,
    A_FLAG_OPAQUE    = 32768,
    A_FLAG_IFINAL    = 32768 << 1
};

typedef enum A_FLAG AFlag;

enum A_TRAIT {
    A_TRAIT_PRIMITIVE = 1,
    A_TRAIT_INTEGRAL  = 2,
    A_TRAIT_REALISTIC = 4,
    A_TRAIT_SIGNED    = 8,
    A_TRAIT_UNSIGNED  = 16,
    A_TRAIT_ENUM      = 32,
    A_TRAIT_ALIAS     = 64,
    A_TRAIT_ABSTRACT  = 128,
    A_TRAIT_VECTOR    = 256,
    A_TRAIT_STRUCT    = 512,
    A_TRAIT_PTR_SIZE  = 1024,
    A_TRAIT_PUBLIC    = 2048,
    A_TRAIT_USER_INIT = 4096,
    A_TRAIT_CLASS     = 8192,
    A_TRAIT_BASE      = 8192 << 1,
    A_TRAIT_POINTER   = 8192 << 2
};

typedef bool(*global_init_fn)();


_Pragma("pack(push, 1)")

typedef struct _AType *AType;

typedef struct _meta_t {
    long long       count;
    AType           meta_0, meta_1, meta_2, meta_3, 
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
    AType           rtype;
    void*           address;
    void*           ffi_cif;  /// ffi-calling info
    void*           ffi_args; /// ffi-data types for args
} method_t;

// this is an exact mock type of A's type, minus the methods it holds
typedef struct _AType {
    struct _AType*  parent_type;
    char*           name;
    char*           module;
    AType*          sub_types;
    i16             sub_types_count;
    i16             sub_types_alloc;
    int             size;
    int             isize;
    af_recycler     af;
    int             magic;
    int             global_count;
    int             vmember_count;
    struct _AType*  vmember_type;
    int             member_count;
    struct _member* members;
    int             traits;
    void*           user;
    u64             required[2];
    struct _AType*  src;
    void*           arb;
    struct _meta_t  meta;
} *AType;


// this is an exact mock type of A's instance
typedef struct _object {
    AType           type;
    AType           scalar;
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
    AType           type;
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