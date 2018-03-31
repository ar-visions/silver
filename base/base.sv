include base;

class Base {
    private static BaseMethod init_object(Base self, Class with_cl) {
        BaseMethod i = NULL;
        if (with_cl->parent)
            i = Base.init_object(self, (Class)with_cl->parent);
        if (i != with_cl->_init)
            with_cl->_init(self);
        return with_cl->init;
    }
    static Base new_object(Class cl, size_t extra_size) {
        Base self = (Base)alloc_bytes(cl->object_size + extra_size);
        self->cl = (class_Base)cl;
        Base.init_object(self, self->cl);
    }
    static Base free_object(Base self) {
        class_Base c_parent = self->cl->parent;
        BaseMethod last_method = NULL;
        for (class_Base c = self->cl; c; c = c->parent) {
            if (c->free != last_method) {
                c->free(self);
                last_method = (BaseMethod)c->free;
            }
        }
    }
    Base release() {
        if (--self->refs == 0) {
            Base.free_object(self);
        }
    }
    Base retain() {
        self->refs++;
        return self;
    }
    _thread_local_ Pool ar_pool;
    Base auto() {
        Base.pool
    }
}