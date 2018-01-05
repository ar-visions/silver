
class AutoRelease {
    override void class_init(Class);
    override void init(C);
    override void free(C);
    AutoRelease current();
    Base add(C,Base);
    void remove(C,Base);
    void drain(C);
    private LList list;
};

extern _thread_local_ AutoRelease *pool;
