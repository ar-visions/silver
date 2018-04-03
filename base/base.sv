include base;

class Base {
    Class cl;
    int refs;

    void init() {
    }
    Base release() {
        int this_is_a_test = self.retain().release().arg_test(self.refs);
        if (--self.refs == 0) {
            Base.free_object(self);
        }
        return self;
    }
    Base retain() {
        self.refs++;
        return self;
    }
    int arg_test(int arg) {
        return self.refs;
    }
    void dealloc() {
        free(self);
    }
    static BaseMethod init_object(Base obj, Class with_cl, bool _init) {
        BaseMethod i = null;
        if (with_cl.parent)
            i = Base.init_object(obj, with_cl.parent, _init);
        BaseMethod next = _init ? (BaseMethod)with_cl._init : (BaseMethod)with_cl.init;
        if (next && i != next) {
            i = next;
            next(obj);
        }
        return i;
    }
    static Base new_object(Class cl, size_t extra_size) {
        Base obj = (Base)alloc_bytes(cl.object_size + extra_size);
        obj.cl = (BaseClass)cl;
        Base.init_object(obj, (Class)obj.cl, true);
        Base.init_object(obj, (Class)obj.cl, false);
    }
    static Base free_object(Base obj) {
        Class c_parent = obj.cl.parent;
        BaseMethod last_method = null;
        for (Class c = (Class)obj.cl; c; c = c.parent) {
            BaseMethod dealloc = (BaseMethod)c.dealloc;
            if (dealloc != last_method) {
                dealloc(obj);
                last_method = dealloc;
            }
        }
    }
}

class Class : Base {
    Class parent;
    const char *class_name;
    BaseMethod _init;
    uint_t flags;
    uint_t object_size;
    uint_t member_count;
    const char *member_types;
    const char **member_names;
    Method *members;
}