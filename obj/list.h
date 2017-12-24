#ifndef _LIST_
#define _LIST_

#define _List(D,T,C) _Base(spr,T,C)                 \
    override(D,T,C,void,init,(C))                   \
    override(D,T,C,void,free,(C))                   \
    method(D,T,C,C,with_item_size,(int))            \
    method(D,T,C,void,push,(C,Base))                \
    method(D,T,C,Base,pop,(C))                      \
    method(D,T,C,void,sort,(C,bool,SortMethod))     \
    method(D,T,C,bool,remove,(C,Base))              \
    method(D,T,C,int,index_of,(C,Base))             \
    method(D,T,C,int,count,(C))                     \
    method(D,T,C,Base,first,(C))                    \
    method(D,T,C,Base,last,(C))                     \
    private_method(D,T,C,void,update_blocks,(C))    \
    var(D,T,C,int,min_block_size)                   \
    var(D,T,C,LList,list)
declare(List, Base);

#define list_with_item(S) (class_call(List, with_item_size, sizeof(S)))
#define instances(O,C,V)  llist_each(&O->list, V) if (inherits(V,C))
#define each(O,V)         llist_each(&O->list, V)
#define list_count(L)     ((L) ? ((L)->list.count) : 0)
#define list_push(L,O)    call((L), push, base(O))
#define list_remove(L,O)  call((L), remove, base(O))
#define list_pop(L,C)     inherits(call((L), pop), C)
#endif
