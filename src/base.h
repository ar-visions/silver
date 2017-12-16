#ifndef _BASE_

struct _class_Base;
struct _object_Enum;
struct _object_String;

#define _Base(D,T,C)                                            \
    method(D,T,C,void,class_preinit,(class))                    \
    method(D,T,C,void,class_init,(class))                       \
    method(D,T,C,void,init,(C))                                 \
    method(D,T,C,void,free,(C))                                 \
    method(D,T,C,void,log,(C,char *, ...))                      \
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
    method(D,T,C,int,compare,(C,C))                             \
    method(D,T,C,ulong,hash,(C))
declare(Base, Base)

#define set_prop(O,P,V) (call(O, set_property, P, base(V)))
#define get_prop(O,P,C) (inherits(call(O, get_property, P), C))
/*
#define log(C,S,...)                                            \
    if (C && C->is_logging())                                   \
        call(C, log, S, __VA_ARGS__)
*/
#endif
