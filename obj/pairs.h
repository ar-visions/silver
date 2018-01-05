
class KeyValue {
    override void,free,(C);
    override ulong,hash,(C);
    private LItem * ordered;
    private Base key;
    private Base value;
};

class Pairs {
    void add(C,Base,Base);
    bool remove(C,Base);
    void clear(C);
    Base value(C,Base);
    override void init(C);
    override void free(C);
    override C copy(C);
    override String to_string(C);
    override C from_cstring(const char *);
    private LList ordered_list);
    private LList *lists);
    private size_t list_count);
};

#define pairs_add(O,K,V)    (call(O, add, base(K), base(V)))
#define pairs_value(O,K,C)  (inherits(call(O, value, base(K)),C))
#define each_pair(O, ptr)   typeof(O) __pairs_each_##__LINE__ = O; ptr = (__pairs_each_##__LINE__ && __pairs_each_##__LINE__->ordered_list.first) ? (typeof(ptr))__pairs_each_##__LINE__->ordered_list.first->data : NULL; if (ptr) for (LItem *_i = __pairs_each_##__LINE__->ordered_list.first; _i; _i = _i->next, ptr = _i ? (typeof(ptr))_i->data : NULL)
