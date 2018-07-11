#ifndef _STRING_
#define _STRING_

struct _object_String;

#define _String(D,T,C) _Base(spr,T,C)               \
    override(D,T,C,void,init,(C))                   \
    override(D,T,C,C,from_cstring,(Class, const char *)) \
    override(D,T,C,C,from_string,(Class,struct _object_String *)) \
    override(D,T,C,C,to_string,(C))                \
    override(D,T,C,void,free,(C))                  \
    override(D,T,C,C,copy,(C))                     \
    override(D,T,C,ulong,hash,(C))                 \
    override(D,T,C,int,compare,(C,C))              \
    method(D,T,C,int,char_index,(C,int))           \
    method(D,T,C,int,char_count,(C,int))           \
    method(D,T,C,int,str_index,(C,const char *))   \
    method(D,T,C,int,str_rindex,(C,const char *))  \
    method(D,T,C,C,upper,(C))                      \
    method(D,T,C,C,lower,(C))                      \
    method(D,T,C,C,new_from_cstring,(Class, const char *)) \
    method(D,T,C,int,cmp,(C,const char *))         \
    method(D,T,C,void,check_resize,(C, uint))      \
    method(D,T,C,int,concat_char,(C, const char)) \
    method(D,T,C,int,concat_chars,(C, const char *, int)) \
    method(D,T,C,int,concat_cstring,(C, const char *)) \
    method(D,T,C,int,concat_string,(C, C))        \
    method(D,T,C,int,concat_long,(C, long, const char *)) \
    method(D,T,C,int,concat_long_long,(C, uint64, const char *)) \
    method(D,T,C,int,concat_double,(C, double, const char *)) \
    method(D,T,C,int,concat_object,(C, Base))     \
    method(D,T,C,C,new_from_bytes,(Class,const uint8 *, size_t)) \
    method(D,T,C,C,from_bytes,(Class,const uint8 *, size_t)) \
    method(D,T,C,C,format,(Class,const char *,...))\
    method(D,T,C,bool,to_file,(C, const char *))   \
    method(D,T,C,uint *,decode_utf8,(C, uint *))   \
    method(D,T,C,C,from_file,(Class, const char *)) \
    method(D,T,C,Base,infer_object,(C))            \
    private_var(D,T,C,uint,flags)                   \
    private_var(D,T,C,uint,utf8_length)            \
    private_var(D,T,C,uint *,utf8_buffer)          \
    private_var(D,T,C,char *,buffer)               \
    private_var(D,T,C,size_t,buffer_size)          \
    private_var(D,T,C,size_t,length)
declare(String,Base)

#define string_eq(C,S)  (call(C,cmp,S) == 0)
#define string_cmp(C,S) (call(C,cmp,S))

#endif