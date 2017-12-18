#ifndef _OBJ_
#define _OBJ_

#include <stdlib.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <llist.h>

#define typeof __typeof__
#define alloc(T) ((T *)alloc_bytes(sizeof(T)))

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
    int obj_size;
    int mcount;
    char **mnames;
    int pcount;
    Method m[0];
};

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif

// ------------------------ var ---------------------------
#define var_cls_implement(C, TYPE, NAME)                        \
    void C##_set_##NAME(C self, TYPE value) {                   \
        self->NAME = value;                                     \
    }                                                           \
    TYPE C##_get_##NAME(C self) {                               \
        return self->NAME;                                      \
    }
#define var_cls_class_def(C, TYPE, N)                           \
    c->set_##N = (typeof(c->set_##N))C##_set_##N;     \
    c->get_##N = (typeof(c->get_##N))C##_get_##N;
#define var_cls_class_dec(C, TYPE, NAME)                        \
    void (*set_##NAME)(C, TYPE);                                \
    TYPE (*get_##NAME)(C);
#define var_cls_enum_def(C, TYPE, NAME)
#define var_cls_forward_dec(C, TYPE, NAME)
#define var_cls_mname_dec(C, TYPE, NAME)                        \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define var_cls_mname_def(C, TYPE, NAME)                        \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ")"; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ")";
#define var_cls_object_dec(C, TYPE, NAME)          TYPE NAME;
#define var_cls_proto(C, TYPE, NAME)                            \
    void C##_set_##NAME(C self, TYPE value);                    \
    TYPE C##_get_##NAME(C self);
#define var_cls_override(C, TYPE, NAME)
#define var_spr_implement(C, TYPE, NAME)
#define var_spr_class_def(C, TYPE, N)
#define var_spr_class_dec(C, TYPE, NAME)                        \
    TYPE (*get_##NAME)(C);                                      \
    void (*set_##NAME)(C, TYPE);
#define var_spr_forward_dec(C, TYPE, NAME)
#define var_spr_mname_dec(C, TYPE, NAME)                        \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define var_spr_mname_def(C, TYPE, NAME)                        \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ")"; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ")";
#define var_spr_enum_def(C, TYPE, NAME)
#define var_spr_object_dec(C, TYPE, NAME)          TYPE NAME;
#define var_spr_proto(C, TYPE, NAME)
#define var_spr_override(C, TYPE, NAME)

#define private_var_cls_implement(C, TYPE, NAME)
#define private_var_cls_class_def(C, TYPE, N)
#define private_var_cls_class_dec(C, TYPE, NAME)
#define private_var_cls_enum_def(C, TYPE, NAME)
#define private_var_cls_forward_dec(C, TYPE, NAME)
#define private_var_cls_mname_dec(C, TYPE, NAME)
#define private_var_cls_mname_def(C, TYPE, NAME)
#define private_var_cls_object_dec(C, TYPE, NAME)         TYPE NAME;
#define private_var_cls_proto(C, TYPE, NAME)
#define private_var_cls_override(C, TYPE, NAME)
#define private_var_spr_implement(C, TYPE, NAME)
#define private_var_spr_class_def(C, TYPE, N)
#define private_var_spr_class_dec(C, TYPE, NAME)
#define private_var_spr_forward_dec(C, TYPE, NAME)
#define private_var_spr_mname_dec(C, TYPE, NAME)
#define private_var_spr_mname_def(C, TYPE, NAME)
#define private_var_spr_object_dec(C, TYPE, NAME)         TYPE ___##NAME;
#define private_var_spr_proto(C, TYPE, NAME)
#define private_var_spr_override(C, TYPE, NAME)

#define var(D, T, C, TYPE, NAME)                        var_##D##_##T(C, TYPE, NAME)
#define private_var(D, T, C, TYPE, NAME)                private_var_##D##_##T(C, TYPE, NAME)

// ------------------------- obj --------------------------
#define object_cls_implement(C, TYPE, NAME)                     \
    void C##_set_##NAME(C self, TYPE value) {                   \
        if (self->NAME != value) {                              \
            release(self->NAME);                                \
            self->NAME = retain(value);                         \
        }                                                       \
    }                                                           \
    TYPE C##_get_##NAME(C self) {                               \
        return self->NAME;                                      \
    }
#define object_cls_class_def(C, TYPE, N)                        \
    c->set_##N = (typeof(c->set_##N))C##_set_##N;               \
    c->get_##N = (typeof(c->get_##N))C##_get_##N;
#define object_cls_class_dec(C, TYPE, NAME)                     \
    void (*set_##NAME)(C, TYPE);                                \
    TYPE (*get_##NAME)(C);
#define object_cls_enum_def(C, TYPE, NAME)
#define object_cls_forward_dec(C, TYPE, NAME)
#define object_cls_mname_dec(C, TYPE, NAME)                     \
    const char *set_##NAME;                                     \
    const char *get_##NAME;
#define object_cls_mname_def(C, TYPE, NAME)                     \
    c->mnames->set_##NAME = "void set_" #NAME " (" #C "," #TYPE ")"; \
    c->mnames->get_##NAME = #TYPE " get_" #NAME " (" #C ")";
#define object_cls_object_dec(C, TYPE, NAME)          TYPE NAME;
#define object_cls_proto(C, TYPE, NAME)                         \
    void C##_set_##NAME(C self, TYPE value);                    \
    TYPE C##_get_##NAME(C self);
#define object_cls_override(C, TYPE, NAME)
#define object_spr_implement(C, TYPE, NAME)
#define object_spr_class_def(C, TYPE, N)
#define object_spr_class_dec(C, TYPE, NAME)                     \
    TYPE (*set_##NAME)(C);                                      \
    void (*get_##NAME)(C, TYPE);
#define object_spr_enum_def(C, TYPE, NAME)
#define object_spr_forward_dec(C, TYPE, NAME)
#define object_spr_mname_dec(C, TYPE, NAME)
#define object_spr_mname_def(C, TYPE, NAME)
#define object_spr_object_dec(C, TYPE, NAME)          TYPE NAME;
#define object_spr_proto(C, TYPE, NAME)
#define object_spr_override(C, TYPE, NAME)

#define private_object_cls_implement(C, TYPE, NAME)
#define private_object_cls_class_def(C, TYPE, N)
#define private_object_cls_class_dec(C, TYPE, NAME)
#define private_object_cls_enum_def(C, TYPE, NAME)
#define private_object_cls_forward_dec(C, TYPE, NAME)
#define private_object_cls_mname_dec(C, TYPE, NAME)
#define private_object_cls_mname_def(C, TYPE, NAME)
#define private_object_cls_object_dec(C, TYPE, NAME)         TYPE NAME;
#define private_object_cls_proto(C, TYPE, NAME)
#define private_object_cls_override(C, TYPE, NAME)
#define private_object_spr_implement(C, TYPE, NAME)
#define private_object_spr_class_def(C, TYPE, N)
#define private_object_spr_class_dec(C, TYPE, NAME)
#define private_object_spr_forward_dec(C, TYPE, NAME)
#define private_object_spr_mname_dec(C, TYPE, NAME)
#define private_object_spr_mname_def(C, TYPE, NAME)
#define private_object_spr_object_dec(C, TYPE, NAME)         TYPE ___##NAME;
#define private_object_spr_proto(C, TYPE, NAME)
#define private_object_spr_override(C, TYPE, NAME)

#define object(D, T, C, TYPE, NAME)                        object_##D##_##T(C, TYPE, NAME)
#define private_object(D, T, C, TYPE, NAME)                private_object_##D##_##T(C, TYPE, NAME)

// ------------------------- prop -------------------------------
#define prop_cls_implement(C, TYPE, NAME)
#define prop_cls_class_def(C, TYPE, NAME)
#define prop_cls_class_dec(C, TYPE, NAME)                       \
    TYPE (*set_NAME)(C *);                                      \
    TYPE (*get_NAME)(C *, TYPE);
#define prop_cls_proto(C, TYPE, NAME)                           \
    void C##_set_##NAME(C *self, TYPE *value)                   \
    TYPE C##_get_##NAME(C *self)
#define prop_cls_object_dec(C, TYPE, NAME)                TYPE NAME;

// ------------------------- method -----------------------------
#define method_cls_implement(C, R, N, A)
#define method_cls_class_def(C, R, N, A)           c->N = (typeof(c->N))C##_##N;
#define method_cls_class_dec(C, R, N, A)           R (*N)A;
#define method_cls_enum_def(C, R, N, A)
#define method_cls_forward_dec(C, R, N, A)
#define method_cls_mname_dec(C, R, N, A)           const char *N;
#define method_cls_mname_def(C, R, N, A)           c->mnames->N = (const char *)(#R " " #N " " #A);
#define method_cls_object_dec(C, R, N, A)
#define method_cls_override(C, R, N, A)
#define method_cls_proto(C, R, N, A)               EXPORT R C##_##N A;

#define method_spr_implement(C, R, N, A)
#define method_spr_class_def(C, R, N, A)
#define method_spr_class_dec(C, R, N, A)           R (*N)A;
#define method_spr_enum_def(C, R, N, A)
#define method_spr_forward_dec(C, R, N, A)
#define method_spr_mname_dec(C, R, N, A)           const char *N;
#define method_spr_mname_def(C, R, N, A)           c->mnames->N = (const char *)(#R " " #N " " #A);
#define method_spr_object_dec(C, R, N, A)
#define method_spr_override(C, R, N, A)
#define method_spr_proto(C, R, N, A)

#define private_method_cls_implement(C, R, N, A)          static R N A;
#define private_method_cls_class_def(C, R, N, A)
#define private_method_cls_class_dec(C, R, N, A)
#define private_method_cls_enum_def(C, R, N, A)
#define private_method_cls_forward_dec(C, R, N, A)
#define private_method_cls_mname_dec(C, R, N, A)
#define private_method_cls_mname_def(C, R, N, A)
#define private_method_cls_object_dec(C, R, N, A)
#define private_method_cls_override(C, R, N, A)
#define private_method_cls_proto(C, R, N, A)

#define private_method_spr_implement(C, R, N, A)
#define private_method_spr_class_def(C, R, N, A)
#define private_method_spr_class_dec(C, R, N, A)
#define private_method_spr_enum_def(C, R, N, A)
#define private_method_spr_forward_dec(C, R, N, A)
#define private_method_spr_mname_dec(C, R, N, A)
#define private_method_spr_mname_def(C, R, N, A)
#define private_method_spr_object_dec(C, R, N, A)
#define private_method_spr_override(C, R, N, A)
#define private_method_spr_proto(C, R, N, A)

#define method(D, T, C, R, N, A)                        method_##D##_##T(C, R, N, A)
#define private_method(D, T, C, R, N, A)                private_method_##D##_##T(C, R, N, A)

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

#define enum_object_cls_implement(C,E,O)                  inline enum C##Enum C##_enum_##E() { return (enum C##Enum)O; } 
#define enum_object_cls_class_def(C,E,O)                  c->enum_##E = C##_enum_##E;
#define enum_object_cls_class_dec(C,E,O)                  enum C##Enum (*enum_##E)();
#define enum_object_cls_enum_def(C,E,O)                   C##_##E = O,
#define enum_object_cls_forward_dec(C,E,O)
#define enum_object_cls_mname_dec(C,E,O)                  const char *enum_##E;
#define enum_object_cls_mname_def(C,E,O)                  c->mnames->enum_##E = "int enum_" #E " ()";
#define enum_object_cls_object_dec(C,E,O)
#define enum_object_cls_proto(C,E,O)                      extern inline enum C##Enum C##_enum_##E();
#define enum_object_cls_override(C,E,O)

#define enum_object_spr_implement(C,E,O)                  inline enum C##Enum C##_enum_##E() { return (enum C##Enum)O; } 
#define enum_object_spr_class_def(C,E,O)                  c->enum_##E = C##_enum_##E;
#define enum_object_spr_class_dec(C,E,O)                  enum C##Enum (*enum_##E)();
#define enum_object_spr_enum_def(C,E,O)                   C##_##E = O,
#define enum_object_spr_forward_dec(C,E,O)
#define enum_object_spr_mname_dec(C,E,O)                  const char *enum_##E;
#define enum_object_spr_mname_def(C,E,O)                  c->mnames->enum_##E = #E;
#define enum_object_spr_object_dec(C,E,O)
#define enum_object_spr_proto(C,E,O)                      extern inline enum C##Enum C##_enum_##E();
#define enum_object_spr_override(C,E,O)
#define enum_object(D,T,C,E,O)                            enum_object_##D##_##T(C,E,O)

enum ClassFlags {
    CLASS_FLAG_ASSEMBLED   = 1,
    CLASS_FLAG_PREINIT     = 2,
    CLASS_FLAG_INIT        = 4,
    CLASS_FLAG_NO_INIT     = 8
};

#define implement(C)                                            \
    struct _class_##C;                                          \
    class_##C C##_cl;                                           \
    static __attribute__ ((constructor))                        \
    void _##C##_def() {                                         \
        class_##C c = C##_cl = alloc(struct _class_##C);        \
        c->name = #C;                                           \
        c->obj_size = sizeof(struct _object_##C);               \
        c->super_name = C##_super_class;                        \
        c->mcount = (void **)(&c[1]) -                          \
                         (void **)((c->m));                     \
        c->mnames = (mnames_##C)alloc_bytes(                    \
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
    extern class_##C C##_cl;                                    \
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
        int obj_size;                                           \
        int mcount;                                             \
        mnames_##C mnames;                                      \
        int pcount;                                             \
        Method m[0];                                            \
        _##C(cls,class_dec,C)                                   \
    };                                                          \
    struct _object_##C {                                        \
        class_##C cl;                                        \
        struct _object_##S *super_object;                       \
        LItem *ar_node;                                         \
        int refs;                                               \
        int alloc_size;                                         \
        _##C(cls,object_dec,C)                                  \
    };                                                          \
    _##C(cls,proto,C)                                           \

#define list_add(T,V)                                           \
    if (!T##_first)                                             \
        T##_first = T##_last = (T)V;                            \
    else {                                                      \
        T##_last->next = (T)V;                                  \
        T##_last = (T)V;                                        \
    }

#define new(C)                  ((C)new_obj((class_Base)C##_cl, 0))
#define object_new(O)           ((typeof(O))((O) ? new_obj((class_Base)(O)->cl, 0) : NULL))
#define class_of(C,I)           (class_inherits((Class)C,(Class)I##_cl))
#define inherits(O,C)           ((C)object_inherits((Base)O,(Class)C##_cl))
#define super(M,A...)           (self->cl->parent->M(((typeof(self->super_object))self, ##A)))
#define call(C,M,A...)          ((C)->cl->M(C, ##A))
#define self(M,A...)            (self->cl->M(self, ##A))
#define class_call(C,M,A...)    (C##_##M(A))
#define class_object(C)         ((Class)C##_cl)
#define set(C,M,V)              ((C)->cl->set_##M(C, V))
#define get(C,M)                ((C)->cl->get_##M(C))
#define cp(C)                   (call((C), copy))
#define priv_call(M,A...)       (M(self, ##A))
#define priv_set(M,V)           (set_##M(self, V))
#define priv_get(M)             (get_##M(self))
#define base(O)                 ((Base)&(O->cl))
#define retain(o)               ((typeof(o))call(o, retain))
#define release(o)              (call(o, release))
#define autorelease(o)          ((typeof(o))call(o, autorelease))
#define auto(C)                 (autorelease(new(C)))
#define object_auto(O)          (autorelease(object_new(O)))
#define free_ptr(p)             ({ if (p) { free(p); p = NULL; } })
#define max(a,b)                ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b)                ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#define clamp(V,L,H)            (min(H,max(L,V)))
#define sqr(v)                  ((v) * (v))
#define cstring(O)              ({String _s = call((O), to_string); (_s ? _s->buffer : 0);})
#define string(cstring)         (class_call(String, from_cstring, cstring))
#define new_string(cstring)     (class_call(String, new_string, cstring))

#include <base.h>
#include <str.h>
#include <prop.h>
#include <autorelease.h>
#include <list.h>
#include <enum.h>
#include <pairs.h>
#include <vec.h>
#include <primitives.h>

EXPORT void *alloc_bytes(size_t);
EXPORT Base new_obj(class_Base, size_t);
EXPORT void free_obj(Base);
EXPORT void class_assemble(Class);
EXPORT void class_init();
EXPORT bool class_inherits(Class, Class);
EXPORT Class class_find(const char *name);
EXPORT Base object_inherits(Base o, Class c);

#endif
