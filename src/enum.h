#ifndef _ENUM_
#define _ENUM_

#define _Enum(D,T,C) _Base(spr,T,C)        \
    method(D,T,C,C,find,(class,const char *)) \
    method(D,T,C,struct _object_Pairs *,enums,(class)) \
    override(D,T,C,void,class_preinit,(class)) \
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
    enum_object(D,T,C,Bool,     1)         \
    enum_object(D,T,C,Int8,     2)         \
    enum_object(D,T,C,UInt8,    3)         \
    enum_object(D,T,C,Int16,    4)         \
    enum_object(D,T,C,UInt16,   5)         \
    enum_object(D,T,C,Int32,    6)         \
    enum_object(D,T,C,UInt32,   7)         \
    enum_object(D,T,C,Int64,    8)         \
    enum_object(D,T,C,UInt64,   9)         \
    enum_object(D,T,C,Long,    10)         \
    enum_object(D,T,C,ULong,   11)         \
    enum_object(D,T,C,Float,   12)         \
    enum_object(D,T,C,Double,  13)
enum_declare(Type, Enum);

#define enum_find(C,N)  ((typeof(C))class_call(Enum, find, (class)class_object(C), N));
#define enums(C)        (Enum_enums((class)class_object(C)))

extern bool enum_init;
#endif