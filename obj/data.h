#ifndef _DATA_
#define _DATA_

#define _Data(D,T,C) _Base(spr,T,C)        \
    override(D,T,C,void,free,(C))          \
    override(D,T,C,String,to_string,(C))   \
    override(D,T,C,C,from_string,(String)) \
    method(D,T,C,C,with_size,(uint))       \
    method(D,T,C,C,with_bytes,(uint8 *, uint)) \
    method(D,T,C,void,get_vector,(C, void **, size_t, uint *)) \
    var(D,T,C,uint8 *,bytes)               \
    var(D,T,C,uint,length)
declare(Data, Base);

#define data_vector(S,P,T,C) ((S ? call(S, get_vector, &(P), sizeof(T), &(C)) : NULL)

#endif
