#ifndef _APP_
#define _APP_

#define _AppDelegate(D,T,C) _Base(spr,T,C)           \
    method(D,T,C,void,loop,(C))
declare(AppDelegate, Base);

#define _Timer(D,T,C) _AppDelegate(spr,T,C)          \
    override(D,T,C,void,loop,(C))                    \
    method(D,T,C,void,start,(C))                     \
    method(D,T,C,void,stop,(C))                      \
    var(D,T,C,int,running)                           \
    var(D,T,C,int,interval)                          \
    var(D,T,C,int,wake)
declare(Timer, AppDelegate);

#define _App(D,T,C) _Base(spr,T,C)                   \
    override(D,T,C,void,class_init,(Class))          \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,void,free,(C))                    \
    method(D,T,C,void,push_delegate,(C,AppDelegate))          \
    method(D,T,C,void,remove_delegate,(C,AppDelegate))        \
    method(D,T,C,void,loop,(C))                      \
    private_var(D,T,C,List,delegates)
declare(App, Base);

EXPORT App app;

#endif
