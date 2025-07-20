#ifndef _object_h
#define _object_h

_Pragma("pack(push, 1)")

typedef struct _AType *AType;

typedef struct meta_t {
    long long       count;
    AType           meta_0, meta_1, meta_2, meta_3, 
                    meta_4, meta_5, meta_6, meta_7, meta_8, meta_9;
} meta_t;

// auto-free recycler
typedef struct af_recycler {
    struct _object** af;
    i64     af_count;
    i64     af_alloc;
    struct _object** re;
    i64     re_count;
    i64     re_alloc;
} af_recycler;

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
    int             size;
    int             msize;
    af_recycler*    af;
    int             magic;
    int             global_count;
    int             vmember_count;
    struct _AType*  vmember_type;
    int             member_count;
    struct _member* members;
    int             traits;
    void*           user;
    u128            required;
    struct _AType*  src;
    void*           arb;
    meta_t          meta;
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


/// we mock string at the top, 
/// just so member can be treated
/// as such (we must be careful, 
/// because it has no header)
typedef struct _member {
    char*           name;
    struct _string* sname;
    AType           type;
    int             offset;
    int             count;
    int             member_type;
    int             operator_type;
    int             required;
    meta_t          args;
    void*           ptr;
    void*           method;
    i64             id;
    i64             value;
} *member;

_Pragma("pack(pop)")

#endif