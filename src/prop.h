#ifndef _PROP_
#define _PROP_

#define _Prop(D,T,C) _Base(spr,T,C)                             \
    method(D,T,C,void,set_for_object,(C,Base,Base))             \
    method(D,T,C,Base,get_for_object,(C,Base))                  \
    method(D,T,C,C,new_with,(char *, char *,Getter,Setter))     \
    var(D,T,C,struct _object_String *,name)                     \
    var(D,T,C,Base,value)                                       \
    var(D,T,C,struct _object_Enum *,enum_type)                  \
    var(D,T,C,class,class_type)                                 \
    private_var(D,T,C,Getter,getter)                            \
    private_var(D,T,C,Setter,setter)
declare(Prop, Base)

#endif
