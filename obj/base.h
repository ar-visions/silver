#ifndef _BASE_

struct _class_Base;
struct _object_Enum;
struct _object_Prop;
struct _object_String;
struct _object_Pairs;

#define _Base(D,T,C)                                            \
    method(D,T,C,void,class_preinit,(Class))                    \
    method(D,T,C,void,class_init,(Class))                       \
    method(D,T,C,void,init,(C))                                 \
    method(D,T,C,struct _object_String *,identity,(C))          \
    method(D,T,C,void,free,(C))                                 \
    method(D,T,C,void,print,(C, struct _object_String *))       \
    method(D,T,C,bool,is_logging,(C))                           \
    method(D,T,C,C,retain,(C))                                  \
    method(D,T,C,void,release,(C))                              \
    method(D,T,C,C,autorelease,(C))                             \
    method(D,T,C,C,copy,(C))                                    \
    method(D,T,C,const char *,to_cstring,(C))                   \
    method(D,T,C,C,from_cstring,(const char *))                 \
    method(D,T,C,struct _object_String *,to_string,(C))         \
    method(D,T,C,C,from_string,(struct _object_String *))       \
    method(D,T,C,void,set_property,(C,const char *,Base))       \
    method(D,T,C,Base,get_property,(C,const char *))            \
    method(D,T,C,Base,prop_value,(C, struct _object_Prop *))    \
    method(D,T,C,struct _object_Prop *,find_prop,(Class, const char *)) \
    method(D,T,C,int,compare,(C,C))                             \
    method(D,T,C,ulong,hash,(C))                                \
    method(D,T,C,void,serialize,(C,struct _object_Pairs *))     \
    method(D,T,C,struct _object_String *,to_json,(C))           \
    method(D,T,C,C,from_json,(Class, struct _object_String *))
declare(Base, Base)

#define set_prop(O,P,V) (call(O, set_property, P, base(V)))
#define get_prop(O,P,C) (inherits(call(O, get_property, P), C))

#define print(C,...)                                          \
    if (C && call(C, is_logging))                             \
        call(C, print, class_call(String, format, __VA_ARGS__));

#endif
