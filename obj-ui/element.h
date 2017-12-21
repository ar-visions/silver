#ifndef _ELEMENT_
#define _ELEMENT_

#define _Overflow(D,T,C) _Enum(spr,T,C)              \
    enum_object(D,T,C,Show,0)                        \
    enum_object(D,T,C,Scroll,1)                      \
    enum_object(D,T,C,Hide,2)
enum_declare(Overflow, Enum);

struct _object_Element;

#define _ElementFlag(D,T,C) _Enum(spr,T,C)           \
    enum_object(D,T,C,LayoutChange,1)
enum_declare(ElementFlag, Enum)

#define _TouchEvent(D,T,C) _Base(spr,T,C)            \
    var(D,T,C,int,x)                                 \
    var(D,T,C,int,y)                                 \
    var(D,T,C,bool,down)                             \
    var(D,T,C,bool,move)                             \
    var(D,T,C,bool,up)
declare(TouchEvent, Base)

#define _KeyEvent(D,T,C) _Base(spr,T,C)              \
    var(D,T,C,int,key)                               \
    var(D,T,C,bool,down)                             \
    var(D,T,C,bool,repeat)                           \
    var(D,T,C,bool,up)
declare(KeyEvent, Base)

#define _Rect(D,T,C) _Base(spr,T,C)                  \
    var(D,T,C,int,x)                                 \
    var(D,T,C,int,y)                                 \
    var(D,T,C,int,w)                                 \
    var(D,T,C,int,h)
declare(Rect, Base)

#define _Font(D,T,C) _Base(spr,T,C)                  \
    var(D,T,C,String,name)
declare(Font, Base)

#define _Element(D,T,C) _List(spr,T,C)               \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,void,push,(C,Base))               \
    override(D,T,C,bool,remove,(C,Base))             \
    method(D,T,C,void,layout,(C))                    \
    method(D,T,C,void,render,(C))                    \
    method(D,T,C,void,touch,(C,TouchEvent))          \
    method(D,T,C,void,key,(C,KeyEvent))              \
    object(D,T,C,Pairs,relayout)                     \
    object(D,T,C,Pairs,state)                        \
    object(D,T,C,String,name)                        \
    object(D,T,C,Coord,top)                          \
    object(D,T,C,Coord,right)                        \
    object(D,T,C,Coord,bottom)                       \
    object(D,T,C,Coord,left)                         \
    object(D,T,C,Rect,rect)                          \
    object(D,T,C,struct _object_Element *,parent)    \
    object(D,T,C,Font,font)                          \
    object(D,T,C,Fill,background)                    \
    object(D,T,C,Fill,foreground)                    \
    object(D,T,C,Fill,border)                        \
    object(D,T,C,Vec2,scale)                         \
    object(D,T,C,Vec2,transform_origin)              \
    var(D,T,C,double,opacity)                        \
    var(D,T,C,double,blur)                           \
    var(D,T,C,double,rounded_tr)                     \
    var(D,T,C,double,rounded_br)                     \
    var(D,T,C,double,rounded_bl)                     \
    var(D,T,C,double,rounded_tl)                     \
    var(D,T,C,enum OverflowEnum,overflow_x)          \
    var(D,T,C,enum OverflowEnum,overflow_y)          \
    private_var(D,T,C,uint,flags)
declare(Element, List)

#endif
