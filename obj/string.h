#ifndef _STRING_
#define _STRING_

#define _String(D,T,C) _Base(spr,T,C)                \
    method(D,T,C,int,char_index,(C,int))             \
    method(D,T,C,int,str_index,(C,const char *))     \
    method(D,T,C,int,str_rindex,(C,const char *))    \
    method(D,T,C,C,upper,(C))                        \
    method(D,T,C,C,lower,(C))                        \
    method(D,T,C,C,new_string,(const char *))        \
    method(D,T,C,int,cmp,(C,const char *))           \
    method(D,T,C,void,check_resize,(C, uint))        \
    method(D,T,C,void,concat_char,(C, const char))   \
    method(D,T,C,void,concat_chars,(C, const char *, int)) \
    method(D,T,C,void,concat_string,(C, C))          \
    method(D,T,C,void,concat_long,(C, long, const char *)) \
    method(D,T,C,void,concat_long_long,(C, uint64, const char *)) \
    method(D,T,C,void,concat_double,(C, double, const char *)) \
    method(D,T,C,void,concat_object,(C, Base))       \
    method(D,T,C,C,from_bytes,(const char *, size_t)) \
    method(D,T,C,C,format,(const char *,...))        \
    override(D,T,C,C,from_cstring,(const char *))    \
    override(D,T,C,C,to_string,(C))                  \
    override(D,T,C,C,from_string,(C))                \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,C,copy,(C))                       \
    override(D,T,C,ulong,hash,(C))                   \
    override(D,T,C,int,compare,(C,C))                \
    private_var(D,T,C,char *,buffer)                 \
    private_var(D,T,C,size_t,buffer_size)            \
    private_var(D,T,C,size_t,length)
declare(String, Base);
#endif
