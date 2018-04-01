include base;

class Base {
    Class cl;
    int refs;

    void init() {
    }
    Base release() {
        if (--self.refs == 0) {
            Base.free_object(self);
        }
    }
    Base retain() {
        self.refs++;
        return self;
    }
    void dealloc() {
        free(self);
    }
    static BaseMethod init_object(Base obj, Class with_cl) {
        BaseMethod i = null;
        if (with_cl.parent)
            i = Base.init_object(obj, with_cl.parent);
        if (i != with_cl.init)
            with_cl.init(obj);
        return with_cl.init;
    }
    static Base new_object(Class cl, size_t extra_size) {
        Base obj = (Base)alloc_bytes(cl.object_size + extra_size);
        obj.cl = (BaseClass)cl;
        Base.init_object(obj, obj.cl);
    }
    static Base free_object(Base obj) {
        Class c_parent = obj.cl.parent;
        BaseMethod last_method = null;
        for (Class c = obj.cl; c; c = c.parent) {
            if (c.dealloc != last_method) {
                c.dealloc(obj);
                last_method = (BaseMethod)c.dealloc;
            }
        }
    }
}

class Class : Base {
    Class parent;
    const char *class_name;
    uint_t flags;
    uint_t object_size;
    uint_t member_count;
    const char *member_types;
    const char **member_names;
    Method **members[1];
}