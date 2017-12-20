#ifndef _LIST_
#define _LIST_

#define _List(D,T,C) _Base(spr,T,C)                 \
    override(D,T,C,void,init,(C))                   \
    override(D,T,C,void,free,(C))                   \
    method(D,T,C,void,push,(C,Base))                \
    method(D,T,C,Base,pop,(C))                      \
    method(D,T,C,void,sort,(C,bool,SortMethod))     \
    method(D,T,C,bool,remove,(C,Base))              \
    method(D,T,C,int,index_of,(C,Base))             \
    var(D,T,C,int,test_prop)                        \
    private_method(D,T,C,void,update_blocks,(C))    \
    private_var(D,T,C,LList,list)
declare(List, Base);

#define _List2(D,T,C) _List(spr,T,C)                \
    method(D,T,C,void,test1,(C))
declare(List2, List);

#define instances(O,C,V)  llist_each(&O->list, V) if (inherits(V,C))
#define each(O,V)         llist_each(&O->list, V)
#define push(L,O)         call((L), push, base(O))
#define remove(L,O)       call((L), remove, base(O))
#define pop(L)            call((L), pop)
#endif
