#ifndef _DATA
#define _DATA_

#define _Data(D,T,C) _Base(spr,T,C)        \
    override(D,T,C,String,to_string,(C))   \
    override(D,T,C,C,from_string,(String))
declare(Enum, Base);

#endif
