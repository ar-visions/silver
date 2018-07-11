#ifndef _LIST_
#define _LIST_

#define _List(D,T,C) _Base(spr,T,C)                 \
    override(D,T,C,void,free,(C))                   \
    override(D,T,C,void *,alloc,(Class,size_t))     \
    override(D,T,C,void,deallocx,(Class,void *))    \
    override(D,T,C,String,to_string,(C))            \
    method(D,T,C,void,push,(C,Base))                \
    method(D,T,C,Base,pop,(C))                      \
    method(D,T,C,bool,remove,(C,Base))              \
    method(D,T,C,int,index_of,(C,Base))             \
    method(D,T,C,Base,object_at,(C,int))            \
    method(D,T,C,int,count,(C))                     \
    method(D,T,C,Base,first,(C))                    \
    method(D,T,C,Base,last,(C))                     \
    method(D,T,C,void,clear,(C))                    \
    method(D,T,C,C,new_list_of_objects,(Class,Class,...))   \
    method(D,T,C,C,new_prealloc,(Class,int))        \
    var(D,T,C,Class,item_class)                     \
    var(D,T,C,Base,user_data)                       \
    var(D,T,C,bool,weak_refs)                       \
    private_var(D,T,C,Base *,buffer)                \
    private_var(D,T,C,int,count)                    \
    private_var(D,T,C,int,remaining)
declare(List, Base);

#define list_var2(A,B)     A ## B
#define list_var(B)        list_var2(__list_each_,B)

extern Base List_placeholder;


#define each(O,V)         if ((O)->count > 0) for (int list_var(__LINE__) = 0; (list_var(__LINE__) < (O)->count) && ((void *)((V) = (typeof(V))(((O)->buffer[list_var(__LINE__)]))) != (void *)&List_placeholder); list_var(__LINE__)++)
#define instances(C,O,V)  each(O,V) if (instance(C,V))
#define list_count(L)     ((L) ? ((L)->list.count) : 0)
#define list_clear(L)     call((L), clear)
#define list_push(L,O)    call((L), push, base(O))
#define list_remove(L,O)  call((L), remove, base(O))
#define list_pop(L,C)     instance(C, call((L), pop))
#define list_get(C,L,I)   instance(C,call((L), object_at, (I)))
#define new_list_of(C,I,...)  (C##_cl->new_list_of_objects((Class)C##_cl, (Class)I##_cl, ## __VA_ARGS__, NULL))
#define list_of(C,I,...)      (autorelease(C##_cl->new_list_of_objects((Class)C##_cl, (Class)I##_cl, ## __VA_ARGS__, NULL)))
#endif
