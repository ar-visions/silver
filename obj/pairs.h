#ifndef _PAIRS_
#define _PAIRS_

#define _KeyValue(D,T,C) _Base(spr,T,C)              \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,ulong,hash,(C))                   \
    private_var(D,T,C,int,user_index)                \
    private_var(D,T,C,LItem *,hashed)                \
    private_var(D,T,C,LItem *,ordered)               \
    private_var(D,T,C,Base,key)                      \
    private_var(D,T,C,Base,value)
declare(KeyValue, Base);

#define _Pairs(D,T,C) _Base(spr,T,C)                 \
    method(D,T,C,void,add,(C,Base,Base))             \
    method(D,T,C,bool,remove,(C,Base))               \
    method(D,T,C,void,clear,(C))                     \
    method(D,T,C,Base,value,(C,Base))                \
    method(D,T,C,KeyValue,find,(C,Base))             \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,C,copy,(C))                       \
    override(D,T,C,String,to_string,(C))             \
    override(D,T,C,C,from_cstring,(Class, const char *)) \
    private_var(D,T,C,Base,user_data)                \
    private_var(D,T,C,int,user_flags)                \
    private_var(D,T,C,List,ordered_list)             \
    private_var(D,T,C,List *,lists)                  \
    private_var(D,T,C,size_t,list_count)
declare(Pairs, Base);

#define pairs_add(O,K,V)    (call(O, add, base(K), base(V)))
#define pairs_value(O,K,C)  ((O && K) ? inherits(call(O, value, base(K)),C) : NULL)
#define pairs_find(O,K)     ((O && K) ? call(O, find, base(K)) : NULL)
#define each_pair(O, KV)   if (O) each((O)->ordered_list, KV)

#endif
