#ifndef _ENUM_
#define _ENUM_

#define _Enum(D,T,C) _Base(spr,T,C)        \
    method(D,T,C,C,find,(Class,const char *)) \
    method(D,T,C,C,from_ordinal,(Class,int)) \
    method(D,T,C,struct _object_Pairs *,enums,(Class)) \
    override(D,T,C,String,to_string,(C))    \
    override(D,T,C,void,class_preinit,(Class)) \
    override(D,T,C,void,free,(C))           \
    var(D,T,C,String,symbol)                \
    var(D,T,C,int,ordinal)
declare(Enum, Base);

#define enum_declare(C,S)                  \
    enum C##Enum {                         \
        _##C(cls,enum_def,C)               \
    };                                     \
    declare(C,S)

#define _Type(D,T,C) _Enum(spr,T,C)        \
    enum_object(D,T,C,Object,   0)         \
    enum_object(D,T,C,Enum,     1)         \
    enum_object(D,T,C,Boolean,  2)         \
    enum_object(D,T,C,Int8,     3)         \
    enum_object(D,T,C,UInt8,    4)         \
    enum_object(D,T,C,Int16,    5)         \
    enum_object(D,T,C,UInt16,   6)         \
    enum_object(D,T,C,Int32,    7)         \
    enum_object(D,T,C,UInt32,   8)         \
    enum_object(D,T,C,Int64,    9)         \
    enum_object(D,T,C,UInt64,  10)         \
    enum_object(D,T,C,Long,    11)         \
    enum_object(D,T,C,ULong,   12)         \
    enum_object(D,T,C,Float,   13)         \
    enum_object(D,T,C,Double,  14)
enum_declare(Type, Enum);

#define enum_find(C,N)  ((C)Enum_cl->find((Class)class_object(C), N))
#define enums(C)        (Enum_enums((Class)class_object(C)))

extern bool enum_init;
#endif