include base;
private include module_loader;

class Base {
    Class cl;
    int refs;

    void init() {
    }
    Base release() {
        if (--self.refs == 0) {
            Base.free_object(self);
        }
        return self;
    }
    Base retain() {
        self.refs++;
        return self;
    }
    Base autorelease() {
        return self;
    }
    int arg_test(int arg) {
        return self.refs;
    }
    void dealloc() {
        free(self);
    }
    static void class_init() { }
    static Base instance(Base obj) {
        Class c = (Class)obj.cl;
        while (c) {
            if (c == (Class)class)
                return obj;
            if (c == Class.parent)
                break;
            c = c.parent;
        }
        return NULL;
    }
    static BaseMethod init_object(Base obj, Class with_cl, bool _init) {
        BaseMethod i = null;
        if (with_cl.parent)
            i = Base.init_object(obj, (Class)with_cl.parent, _init);
        BaseMethod next = _init ? (BaseMethod)with_cl._init : (BaseMethod)with_cl.init;
        if (next && i != next) {
            i = next;
            next(obj);
        }
        return i;
    }
    static Base new_object(Class cl, size_t extra_size, bool auto_release) {
        Base obj = (Base)alloc_bytes(cl.object_size + extra_size);
        obj.cl = (BaseClass)cl;
        Base.init_object(obj, (Class)obj.cl, true);
        Base.init_object(obj, (Class)obj.cl, false);
        return obj;
    }
    static void free_object(Base obj) {
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
    static bool load_module(const char *name) {
        return false;
    }
}

class Class {
    private Class parent;
    private const char *name;
    private BaseMethod _init;
    private uint32_t flags;
    private uint32_t object_size;
    private uint32_t member_count;
    private const char *member_types;
    private const char **member_names;
    private Method *members;
}

