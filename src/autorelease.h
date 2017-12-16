#ifndef _AUTORELEASE_
#define _AUTORELEASE_

#define _AutoRelease(D,T,C) _Base(spr,T,C)         \
    override(D,T,C,void,class_init,(class))        \
    override(D,T,C,void,init,(C))                  \
    override(D,T,C,void,free,(C))                  \
    method(D,T,C,AutoRelease,current,())    \
    method(D,T,C,Base,add,(C,Base))         \
    method(D,T,C,void,remove,(C,Base))      \
    method(D,T,C,void,drain,(C))            \
    private_var(D,T,C,LList,list)
declare(AutoRelease, Base);

extern __thread AutoRelease *pool;
#endif
