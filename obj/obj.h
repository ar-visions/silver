#ifndef _OBJ_
#define _OBJ_

#include <stdlib.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <strings.h>

#ifndef typeof
#ifdef _MSC_VER
#define typeof decltype
#else
#define typeof __typeof__
#endif
#endif

#ifdef _MSC_VER
#include <algorithm>
#define min std::min
#define max std::max
#define _thread_local_  __declspec(thread)
#else
#define _thread_local_  __thread
#ifndef max
#define max(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#endif
#ifndef min
#define min(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#endif
#endif

#define OBJ_VERSION "1.0.0"

#ifdef __cplusplus
    #define global_construct(f) \
        static void f(void); \
        struct f##_t_ { f##_t_(void) { f(); } }; static f##_t_ f##_; \
        static void f(void)
#elif defined(_MSC_VER)
    #pragma section(".CRT$XCU",read)
    #define global_construct2_(f,p) \
        static void f(void); \
        __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f; \
        __pragma(comment(linker,"/include:" p #f "_")) \
        static void f(void)
    #ifdef _WIN64
        #define global_construct(f) global_construct2_(f,"")
    #else
        #define global_construct(f) global_construct2_(f,"_")
    #endif
#else
    #define global_construct(f) \
        static void f(void) __attribute__((constructor)); \
        static void f(void)
#endif

struct _object_Base;

typedef struct _Class *Class;
typedef void *(*Method)();
typedef void (*Setter)(struct _object_Base *, void *);
typedef void *(*Getter)(struct _object_Base *);

typedef unsigned long long uint64;
typedef long long       int64;
typedef unsigned long   ulong;
typedef unsigned int    uint32;
typedef int             int32;
typedef unsigned int    uint;
typedef unsigned short  uint16;
typedef short           int16;
typedef unsigned char   uint8;
typedef signed char     int8;

struct _object_Prop;
struct _class_Pairs;

struct _Class {
    struct _Class *parent;
    const char *name;
    const char *super_name;
    unsigned int flags;
    struct _object_Pairs *meta;
    int object_count;
    int obj_size;
    int mcount;
    char **mnames;
    Method m[1];
};

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT extern
#endif

// ------------------------ var ---------------------------
#define var_cls_implement(C, TYPE, NAME, ...)                   \
    void C##_set_##NAME(C self, TYPE value) {                   \
        self->NAME = value;                                     \
    }                                                           \
    TYPE C##_get_##NAME(C self) {                               \
        return self->NAME;                                      \
    }
#define var_cls_class_def(C, TYPE, NAME, ...)                   \
    c->set_##NAME = (typeof(c->set_##NAME))C##_set_##NAME;      \
    c->get_##NAME = (typeof(c->get_##NAME))C##_get_##NAME;
#define var_cls_class_dec(C, TYPE, NAME, ...)                        \
    void (*set_##NAME)(C, TYPE);                                \
    TYPE (*get_##NAME)(C);
#define var_cls_enum_def(C, TYPE, NAME, ...)
#define var_cls_forward_dec(C, TYPE, NAME, ...)
#define var_cls_mname_dec(C, TYPE, NAME, ...)                        \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define var_cls_mname_def(C, TYPE, NAME, ...)                        \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ") " __VA_ARGS__; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ") " __VA_ARGS__;
#define var_cls_object_dec(C, TYPE, NAME, ...)          TYPE NAME;
#define var_cls_proto(C, TYPE, NAME, ...)                            \
    void C##_set_##NAME(C self, TYPE value);                    \
    TYPE C##_get_##NAME(C self);
#define var_cls_override(C, TYPE, NAME, ...)
#define var_spr_implement(C, TYPE, NAME, ...)
#define var_spr_class_def(C, TYPE, NAME, ...)
#define var_spr_class_dec(C, TYPE, NAME, ...)                        \
    TYPE (*get_##NAME)(C);                                      \
    void (*set_##NAME)(C, TYPE);
#define var_spr_forward_dec(C, TYPE, NAME, ...)
#define var_spr_mname_dec(C, TYPE, NAME, ...)                        \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define var_spr_mname_def(C, TYPE, NAME, ...)                   \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ") " __VA_ARGS__; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ") " __VA_ARGS__;
#define var_spr_enum_def(C, TYPE, NAME, ...)
#define var_spr_object_dec(C, TYPE, NAME, ...)          TYPE NAME;
#define var_spr_proto(C, TYPE, NAME, ...)
#define var_spr_override(C, TYPE, NAME, ...)

#define private_var_cls_implement(C, TYPE, NAME, ...)
#define private_var_cls_class_def(C, TYPE, NAME, ...)
#define private_var_cls_class_dec(C, TYPE, NAME, ...)
#define private_var_cls_enum_def(C, TYPE, NAME, ...)
#define private_var_cls_forward_dec(C, TYPE, NAME, ...)
#define private_var_cls_mname_dec(C, TYPE, NAME, ...)
#define private_var_cls_mname_def(C, TYPE, NAME, ...)
#define private_var_cls_object_dec(C, TYPE, NAME, ...)         TYPE NAME;
#define private_var_cls_proto(C, TYPE, NAME, ...)
#define private_var_cls_override(C, TYPE, NAME, ...)
#define private_var_spr_implement(C, TYPE, NAME, ...)
#define private_var_spr_class_def(C, TYPE, NAME, ...)
#define private_var_spr_class_dec(C, TYPE, NAME, ...)
#define private_var_spr_forward_dec(C, TYPE, NAME, ...)
#define private_var_spr_mname_dec(C, TYPE, NAME, ...)
#define private_var_spr_mname_def(C, TYPE, NAME, ...)
#define private_var_spr_object_dec(C, TYPE, NAME, ...)         TYPE NAME;
#define private_var_spr_proto(C, TYPE, NAME, ...)
#define private_var_spr_override(C, TYPE, NAME, ...)

#define var(D, T, C, TYPE, NAME, ...)                        var_##D##_##T(C, TYPE, NAME, __VA_ARGS__)
#define private_var(D, T, C, TYPE, NAME, ...)                private_var_##D##_##T(C, TYPE, NAME, __VA_ARGS__)

// ------------------------- obj --------------------------
#define object_cls_implement(C, TYPE, NAME, ...)                   \
    void C##_set_##NAME(C self, TYPE value) {                   \
        if (self->NAME != value) {                              \
            release(self->NAME);                                \
            self->NAME = retain(value);                         \
        }                                                       \
    }                                                           \
    TYPE C##_get_##NAME(C self) {                               \
        return self->NAME;                                      \
    }
#define object_cls_class_def(C, TYPE, NAME, ...)                   \
    c->set_##NAME = (typeof(c->set_##NAME))C##_set_##NAME;      \
    c->get_##NAME = (typeof(c->get_##NAME))C##_get_##NAME;
#define object_cls_class_dec(C, TYPE, NAME, ...)                        \
    void (*set_##NAME)(C, TYPE);                                \
    TYPE (*get_##NAME)(C);
#define object_cls_enum_def(C, TYPE, NAME, ...)
#define object_cls_forward_dec(C, TYPE, NAME, ...)
#define object_cls_mname_dec(C, TYPE, NAME, ...)                        \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define object_cls_mname_def(C, TYPE, NAME, ...)                        \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ") " __VA_ARGS__; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ") " __VA_ARGS__;
#define object_cls_object_dec(C, TYPE, NAME, ...)          TYPE NAME;
#define object_cls_proto(C, TYPE, NAME, ...)                            \
    void C##_set_##NAME(C self, TYPE value);                    \
    TYPE C##_get_##NAME(C self);
#define object_cls_override(C, TYPE, NAME, ...)
#define object_spr_implement(C, TYPE, NAME, ...)
#define object_spr_class_def(C, TYPE, NAME, ...)
#define object_spr_class_dec(C, TYPE, NAME, ...)                        \
    TYPE (*get_##NAME)(C);                                      \
    void (*set_##NAME)(C, TYPE);
#define object_spr_forward_dec(C, TYPE, NAME, ...)
#define object_spr_mname_dec(C, TYPE, NAME, ...)                        \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define object_spr_mname_def(C, TYPE, NAME, ...)                   \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ") " __VA_ARGS__; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ") " __VA_ARGS__;
#define object_spr_enum_def(C, TYPE, NAME, ...)
#define object_spr_object_dec(C, TYPE, NAME, ...)          TYPE NAME;
#define object_spr_proto(C, TYPE, NAME, ...)
#define object_spr_override(C, TYPE, NAME, ...)

#define object(D, T, C, TYPE, NAME, ...)                        object_##D##_##T(C, TYPE, NAME, __VA_ARGS__)

// ------------------------- prop -------------------------------
#define prop_cls_implement(C, TYPE, NAME, ...)
#define prop_cls_class_def(C, TYPE, NAME, ...)
#define prop_cls_class_dec(C, TYPE, NAME, ...)                       \
    TYPE (*set_NAME)(C *);                                      \
    TYPE (*get_NAME)(C *, TYPE);
#define prop_cls_proto(C, TYPE, NAME, ...)                           \
    void C##_set_##NAME(C *self, TYPE *value)                   \
    TYPE C##_get_##NAME(C *self)
#define prop_cls_object_dec(C, TYPE, NAME, ...)                TYPE NAME;

// ------------------------- method -----------------------------
#define method_cls_implement(C, R, N, A, ...)
#define method_cls_class_def(C, R, N, A, ...)           c->N = (typeof(c->N))C##_##N;
#define method_cls_class_dec(C, R, N, A, ...)           R (*N)A;
#define method_cls_enum_def(C, R, N, A, ...)
#define method_cls_forward_dec(C, R, N, A, ...)
#define method_cls_mname_dec(C, R, N, A, ...)           const char *N;
#define method_cls_mname_def(C, R, N, A, ...)           c->mnames->N = (const char *)(#R " " #N " " #A " "  __VA_ARGS__);
#define method_cls_object_dec(C, R, N, A, ...)
#define method_cls_override(C, R, N, A, ...)
#define method_cls_proto(C, R, N, A, ...)               EXPORT R C##_##N A;

#define method_spr_implement(C, R, N, A, ...)
#define method_spr_class_def(C, R, N, A, ...)
#define method_spr_class_dec(C, R, N, A, ...)           R (*N)A;
#define method_spr_enum_def(C, R, N, A, ...)
#define method_spr_forward_dec(C, R, N, A, ...)
#define method_spr_mname_dec(C, R, N, A, ...)           const char *N;
#define method_spr_mname_def(C, R, N, A, ...)           c->mnames->N = (const char *)(#R " " #N " " #A " "  __VA_ARGS__);
#define method_spr_object_dec(C, R, N, A, ...)
#define method_spr_override(C, R, N, A, ...)
#define method_spr_proto(C, R, N, A, ...)

#define private_method_cls_implement(C, R, N, A, ...)          static R N A;
#define private_method_cls_class_def(C, R, N, A, ...)
#define private_method_cls_class_dec(C, R, N, A, ...)
#define private_method_cls_enum_def(C, R, N, A, ...)
#define private_method_cls_forward_dec(C, R, N, A, ...)
#define private_method_cls_mname_dec(C, R, N, A, ...)
#define private_method_cls_mname_def(C, R, N, A, ...)
#define private_method_cls_object_dec(C, R, N, A, ...)
#define private_method_cls_override(C, R, N, A, ...)
#define private_method_cls_proto(C, R, N, A, ...)

#define private_method_spr_implement(C, R, N, A, ...)
#define private_method_spr_class_def(C, R, N, A, ...)
#define private_method_spr_class_dec(C, R, N, A, ...)
#define private_method_spr_enum_def(C, R, N, A, ...)
#define private_method_spr_forward_dec(C, R, N, A, ...)
#define private_method_spr_mname_dec(C, R, N, A, ...)
#define private_method_spr_mname_def(C, R, N, A, ...)
#define private_method_spr_object_dec(C, R, N, A, ...)
#define private_method_spr_override(C, R, N, A, ...)
#define private_method_spr_proto(C, R, N, A, ...)

#define method(D, T, C, R, N, A, ...)                        method_##D##_##T(C, R, N, A, ##  __VA_ARGS__)
#define private_method(D, T, C, R, N, A, ...)                private_method_##D##_##T(C, R, N, A, ##  __VA_ARGS__)

// ------------------------- override ---------------------------
#define override_cls_implement(C, R, N, A)
#define override_cls_class_def(C, R, N, A)
#define override_cls_class_dec(C, R, N, A)
#define override_cls_enum_def(C, R, N, A)
#define override_cls_forward_dec(C, R, N, A)
#define override_cls_mname_dec(C, R, N, A)
#define override_cls_mname_def(C, R, N, A)
#define override_cls_object_dec(C, R, N, A)
#define override_cls_proto(C, R, N, A)                    EXPORT R C##_##N A;
#define override_cls_override(C, R, N, A)                 c->N = C##_##N;

#define override_spr_implement(C, R, N, A)
#define override_spr_class_def(C, R, N, A)
#define override_spr_class_dec(C, R, N, A)
#define override_spr_enum_def(C, R, N, A)
#define override_spr_forward_dec(C, R, N, A)
#define override_spr_mname_dec(C, R, N, A)
#define override_spr_mname_def(C, R, N, A)
#define override_spr_object_dec(C, R, N, A)
#define override_spr_proto(C, R, N, A)
#define override_spr_override(C, R, N, A)
#define override(D, T, C, R, N, A)                      override_##D##_##T(C, R, N, A)

#define enum_object_cls_implement(C,E,O)                  enum C##Enum C##_enum_##E() { return (enum C##Enum)O; } 
#define enum_object_cls_class_def(C,E,O)                  c->enum_##E = C##_enum_##E;
#define enum_object_cls_class_dec(C,E,O)                  enum C##Enum (*enum_##E)();
#define enum_object_cls_enum_def(C,E,O)                   C##_##E = O,
#define enum_object_cls_forward_dec(C,E,O)
#define enum_object_cls_mname_dec(C,E,O)                  const char *enum_##E;
#define enum_object_cls_mname_def(C,E,O)                  c->mnames->enum_##E = "int enum_" #E " ()";
#define enum_object_cls_object_dec(C,E,O)
#define enum_object_cls_proto(C,E,O)                      enum C##Enum C##_enum_##E();
#define enum_object_cls_override(C,E,O)

#define enum_object_spr_implement(C,E,O)                  enum C##Enum C##_enum_##E() { return (enum C##Enum)O; } 
#define enum_object_spr_class_def(C,E,O)                  c->enum_##E = C##_enum_##E;
#define enum_object_spr_class_dec(C,E,O)                  enum C##Enum (*enum_##E)();
#define enum_object_spr_enum_def(C,E,O)                   C##_##E = O,
#define enum_object_spr_forward_dec(C,E,O)
#define enum_object_spr_mname_dec(C,E,O)                  const char *enum_##E;
#define enum_object_spr_mname_def(C,E,O)                  c->mnames->enum_##E = #E;
#define enum_object_spr_object_dec(C,E,O)
#define enum_object_spr_proto(C,E,O)                      enum C##Enum C##_enum_##E();
#define enum_object_spr_override(C,E,O)
#define enum_object(D,T,C,E,O)                            enum_object_##D##_##T(C,E,O)


#define CLASS_FLAG_ASSEMBLED   1
#define CLASS_FLAG_PREINIT     2
#define CLASS_FLAG_INIT        4
#define CLASS_FLAG_NO_INIT     8

#define implement(C)                                            \
    struct _class_##C;                                          \
    class_##C C##_cl;                                           \
    global_construct(_##C##_def) {                              \
        class_##C c = C##_cl = calloc(1,sizeof(struct _class_##C)); \
        c->name = #C;                                           \
        c->obj_size = sizeof(struct _object_##C);               \
        c->super_name = C##_super_class;                        \
        c->m = ((Method *)&c->m) + 1;                           \
        c->mcount = ((void **)(&c[1]) -                         \
                         (void **)(c->m));                      \
        c->mnames = (mnames_##C)calloc(1,                       \
                        sizeof(char *) * c->mcount);            \
        _##C(cls,override, C)                                   \
        _##C(cls,class_def, C)                                  \
        _##C(cls,mname_def,C)                                   \
        class_assemble((Class)c);                               \
    }                                                           \
    _##C(cls, implement, C)

#define declare(C,S)                                            \
    struct _class_##C;                                          \
    struct _mnames_##C;                                         \
    struct _object_##C;                                         \
    struct _object_##S;                                         \
    static const char *C##_super_class = #S;                    \
    typedef struct _class_##C * class_##C;                      \
    typedef struct _mnames_##C *mnames_##C;                     \
    typedef struct _object_##C * C;                             \
    EXPORT class_##C C##_cl;                                    \
    _##C(cls,forward_dec,C)                                     \
    struct _mnames_##C {                                        \
        _##C(cls,mname_dec,C)                                   \
    };                                                          \
    struct _class_##C {                                         \
        struct _class_##S *parent;                              \
        const char *name;                                       \
        const char *super_name;                                 \
        unsigned int flags;                                     \
        struct _object_Pairs *meta;                             \
        int object_count;                                       \
        int obj_size;                                           \
        int mcount;                                             \
        mnames_##C mnames;                                      \
        Method *m;                                              \
        _##C(cls,class_dec,C)                                   \
    };                                                          \
    struct _object_##C {                                        \
        class_##C cl;                                           \
        struct _object_##S *super_object;                       \
        int refs;                                               \
        int alloc_size;                                         \
        int testme;                                             \
        _##C(cls,object_dec,C)                                  \
    };                                                          \
    _##C(cls,proto,C)                                           \

#define new(C)                  ((C)new_obj((class_Base)C##_cl, 0))
#define auto(C)                 ((C)auto_obj((class_Base)C##_cl, 0))
#define alloc_struct(T)         ((T *)calloc(1, sizeof(T)))
#define array_struct(T,C)       ((T *)calloc(1, sizeof(T) * C))
#define object_new(O)           ((typeof(O))((O) ? new_obj((class_Base)(O)->cl, 0) : NULL))
#define class_of(I,C)           ((class_##I)class_inherits((Class)C,(Class)I##_cl))
#define instance(C,O)           ((C)object_inherits((Base)(O),(Class)C##_cl))
#ifdef _MSC_VER
    #define super(M,...)            (self->cl->parent->M(self->super_object, __VA_ARGS__))
    #define call(C,M,...)           ((C)->cl->M(C, __VA_ARGS__))
    #define self(M,...)             (self->cl->M(self, __VA_ARGS__))
    #define class_call(C,M,...)     (C##_cl->M((Class)C##_cl, __VA_ARGS__))
    #define priv_call(M,...)        (M(self, __VA_ARGS__))
    #define priv_set(M,V)           (set_##M(self, V))
#else
    #define super(M,A...)           (self->cl->parent->M(self->super_object, ##A))
    #define call(C,M,A...)          ((C)->cl->M(C, ##A))
    #define self(M,A...)            (self->cl->M(self, ##A))
    #define class_call(C,M,A...)    (C##_cl->M((Class)C##_cl, ##A))
    #define priv_call(M,A...)       (M(self, ##A))
    #define priv_set(M,V)           (set_##M(self, V))
#endif
#define priv_get(M)             (get_##M(this))
#define class_object(C)         ((Class)C##_cl)
#define set(C,M,V)              ((C)->cl->set_##M(C, V))
#define get(C,M)                ((C)->cl->get_##M(C))
#define cp(C)                   (call((C), copy))
#define base(O)                 ((Base)&(O->cl))
#define retain(o)               ({ typeof(o) _o = o; (_o) ? ((typeof(_o))call(_o, retain)) : NULL; })
#define release(o)              ({ typeof(o) _o = o; (_o) ? ((typeof(_o))call(_o, release)) : NULL; })
#define autorelease(o)          ({ typeof(o) _o = o; (_o) ? ((typeof(_o))call(_o, autorelease)) : NULL; })
#define object_auto(O)          (autorelease(object_new(O)))
#ifndef clamp
#define clamp(V,L,H)            (min((H),max((L),(V))))
#endif
#ifndef sqr
#define sqr(v)                  ((v) * (v))
#endif
#define string(cstring)         (String_cl->from_cstring((Class)&String_cl, cstring))
#define new_string(cstring)     (String_cl->new_from_cstring((Class)&String_cl, cstring))
#define mix(a,b,f)              (((double)(a) * (double)(f)) + ((double)(b) * (double)(1.0 - (double)(f))))
#define class_alloc(C,S)        ((C)->alloc((Class)(C), S))
#define object_alloc(C,S)       ((C)->cl->alloc((Class)(C)->cl, S))
#define class_dealloc(C,P)      ((C)->deallocx((Class)(C), P))
#define object_dealloc(C,P)     ((C)->cl->deallocx((Class)(C)->cl, P))
#include <obj/base.h>
#include <obj/string.h>
#include <obj/data.h>
#include <obj/prop.h>
#include <obj/list.h>
#include <obj/auto.h>
#include <obj/enum.h>
#include <obj/pairs.h>
#include <obj/app.h>
#include <obj/prim.h>
#include <obj/vec.h>

EXPORT Base new_obj(class_Base, size_t);
EXPORT Base auto_obj(class_Base, size_t);
EXPORT void free_obj(Base);
EXPORT void class_assemble(Class);
EXPORT void class_init();
EXPORT Class class_inherits(Class, Class);
EXPORT Class class_find(const char *name);
EXPORT Base object_inherits(Base o, Class c);

#endif
