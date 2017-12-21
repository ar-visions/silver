#ifndef _COORD_
#define _COORD_

#define _Align(D,T,C) _Enum(spr,T,C)                 \
    enum_object(D,T,C,Start,0)                       \
    enum_object(D,T,C,End,1)                         \
    enum_object(D,T,C,Center,2)
enum_declare(Align, Enum);

#define _Coord(D,T,C) _Base(spr,T,C)                 \
    var(D,T,C,enum AlignEnum,align)                  \
    var(D,T,C,bool,relative)                         \
    var(D,T,C,double,value)                          \
    var(D,T,C,bool,scale)
declare(Coord, Base);

#endif