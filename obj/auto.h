#ifndef _AUTORELEASE_
#define _AUTORELEASE_

#define _AutoRelease(D,T,C) _List(spr,T,C)  \
    override(D,T,C,void,class_init,(Class)) \
    override(D,T,C,void,init,(C))           \
    override(D,T,C,void,free,(C))           \
    method(D,T,C,C,current,())
declare(AutoRelease, List);

#endif
