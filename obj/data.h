#ifndef _DATA
#define _DATA_

#define _Data(D,T,C) _Base(spr,T,C)        \
    override(D,T,C,void,free,(C))          \
    override(D,T,C,String,to_string,(C))   \
    override(D,T,C,C,from_string,(String)) \
    var(D,T,C,uint8 *,bytes)               \
    var(D,T,C,uint,length)
declare(Data, Base);

#endif
