#ifndef _PAIRS_
#define _PAIRS_

#define _KeyValue(D,T,C) _Base(spr,T,C)              \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,ulong,hash,(C))                   \
    private_var(D,T,C,LItem *,ordered)               \
    private_var(D,T,C,Base,key)                      \
    private_var(D,T,C,Base,value)
declare(KeyValue, Base);

#define _Pairs(D,T,C) _Base(spr,T,C)                 \
    method(D,T,C,void,add,(C,Base,Base))             \
    method(D,T,C,bool,remove,(C,Base))               \
    method(D,T,C,void,clear,(C))                     \
    method(D,T,C,Base,value,(C,Base))                \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,C,copy,(C))                       \
    override(D,T,C,String,to_string,(C))             \
    override(D,T,C,C,from_string,(String))           \
    private_var(D,T,C,LList,ordered_list)            \
    private_var(D,T,C,LList *,lists)                 \
    private_var(D,T,C,size_t,list_count)
declare(Pairs, Base);

#define pairs_add(O,K,V)    (call(O, add, base(K), base(V)))
#define pairs_value(O,K,C)  (inherits(call(O, value, base(K)),C))
#define each_pair(O, ptr)   typeof(O) __pairs_each_##__LINE__ = O; ptr = (__pairs_each_##__LINE__ && __pairs_each_##__LINE__->ordered_list.first) ? (typeof(ptr))__pairs_each_##__LINE__->ordered_list.first->data : NULL; if (ptr) for (LItem *_i = __pairs_each_##__LINE__->ordered_list.first; _i; _i = _i->next, ptr = _i ? (typeof(ptr))_i->data : NULL)

#endif