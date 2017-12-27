#ifndef _COLOR_
#define _COLOR_

#define _Mixable(D,T,C) _Base(spr,T,C)               \
    method(D,T,C,C,mix_with,(C,C,double))
declare(Mixable, Base);

#define _Color(D,T,C) _Mixable(spr,T,C)              \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,C,mix_with,(C,C,double))          \
    method(D,T,C,C,new_rgba,(double,double,double,double)) \
    var(D,T,C,double,r)                              \
    var(D,T,C,double,g)                              \
    var(D,T,C,double,b)                              \
    var(D,T,C,double,a)
declare(Color, Mixable);

#define _Fill(D,T,C) _Mixable(spr,T,C)               \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,C,mix_with,(C,C,double))          \
    object(D,T,C,Color,color)
declare(Fill, Mixable);

#define new_rgba(r,g,b,a)   (Color_cl->new_rgba(r,g,b,a))
#define rgba(r,g,b,a)       (autorelease(new_rgba(r,g,b,a)))
#endif
