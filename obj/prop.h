#ifndef _PROP_
#define _PROP_

#define _Prop(D,T,C) _Base(spr,T,C)                             \
    method(D,T,C,struct _object_List *,props_with_meta,(Class,Class,const char *)) \
    method(D,T,C,C,new_with,(Class,Class,char *, char *,Getter,Setter,char *)) \
    var(D,T,C,struct _object_String *,name)                     \
    var(D,T,C,Base,value)                                       \
    var(D,T,C,struct _object_Enum *,enum_type)                  \
    var(D,T,C,Class,class_type)                                 \
    var(D,T,C,struct _object_Pairs *,meta)                      \
    private_var(D,T,C,Class,prop_of)                            \
    private_var(D,T,C,Getter,getter)                            \
    private_var(D,T,C,Setter,setter)
declare(Prop, Base)

#endif
