class List {
    override void init(C);
    override void free(C);
    C with_item_size(int);
    void *push(C,Base);
    Base pop(C);
    void sort(C,bool,SortMethod);
    bool remove(C,Base);
    int index_of(C,Base);
    Base object_at(C,int);
    int count(C);
    Base first(C);
    Base last(C);
    C new_list_of(Class,Class,...);
    void create_index(C);
    private void update_blocks(C);
    private Base *index;
    bool indexed;
    Class item_class;
    [test:true test2:false]
    int min_block_size;
    LList list;
};

#define list_with_item(S) (class_call(List, with_item_size, sizeof(S)))
#define instances(O,C,V)  llist_each(&O->list, V) if (inherits(V,C))
#define each(O,V)         llist_each(&O->list, V)
#define list_count(L)     ((L) ? ((L)->list.count) : 0)
#define list_push(L,O)    call((L), push, base(O))
#define list_remove(L,O)  call((L), remove, base(O))
#define list_pop(L,C)     inherits(call((L), pop), C)
#define new_list_of(C,I,...)  (class_call(C, new_list_of, class_object(C), class_object(I), ## __VA_ARGS__, NULL))
#define list_of(C,I,...)      (autorelease(class_call(C, new_list_of, class_object(C), class_object(I), ## __VA_ARGS__, NULL)))

