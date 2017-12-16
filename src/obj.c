#include <obj.h>

static class class_first = NULL;
static class class_last  = NULL;

static void class_set_methods(class c) {
    for (int i = 0; i < c->mcount; i++) {
        if (!c->m[i]) {
            for (class cc = c->parent; cc; cc = cc->parent) {
                if (cc->mcount <= i)
                    break;
                if (cc->m[i]) {
                    c->m[i] = cc->m[i];
                    break;
                }
                if (cc->parent == cc)
                    break;
            }
        }
    }
}

class class_find(const char *name) {
    for (class c = class_first; c; c = c->next)
        if (strcmp(name, c->name) == 0)
            return c;
    return NULL;
}

bool class_inherits(class check, class c) {
    while (check) {
        if (c == check)
            return true;
        if (check == check->parent)
            return false;
        check = check->parent;
    }
    return false;
}

Base object_inherits(Base o, class c) {
    if (o && class_inherits((class)o->class, c)) {
        return o;
    }
    return NULL;
}

typedef void (*init_call)(Base);
init_call call_inits(Base o, class_Base c, init_call *pf) {
    if (!c)
        return *pf;
    if (c->init != *pf) {
        init_call init = (c->parent != c) ? call_inits(o, c->parent, pf) : *pf;
        if (c->init != init) {
            c->init(o);
            *pf = c->init;
            return c->init;
        }
    }
    return *pf;
}

void stack_obj(class_Base c, Base this) {
    memset(this, 0, c->obj_size);
    this->class = (class_Base const)c;
    this->super_object = this;
    if (c->init != Base_init) {
        init_call init = NULL;
        call_inits(this, (class_Base)this->class, &init);
    }
}

Base new_obj(class_Base c, size_t extra) {
    size_t alloc_size = c->obj_size + extra;
    Base this = (Base)alloc_bytes(alloc_size);
    this->refs = 1;
    this->alloc_size = alloc_size;
    this->class = (class_Base const)c;
    this->super_object = this;
    if (c->init != Base_init) {
        init_call init = NULL;
        call_inits(this, (class_Base)this->class, &init);
    }
    return this;
}

void free_obj(Base o) {
    call(o, free);
}

Base new_struct(int size, void **p) {
    Base this = new_obj(Base_cl, size);
    *p = (void*)(&this[1]);
    return this;
}

static bool _class_assemble(class c) {
    class cc = c;
    for (;;) {
        if (!cc->parent) {
            cc->parent = class_find(cc->super_name);
            if (!cc->parent)
                return false;
        }
        if (cc->parent == cc) {
            class_set_methods(c);
            c->flags |= CLASS_FLAG_ASSEMBLED;
            return true;
        }
        cc = cc->parent;
    }
}

void class_assemble(class c) {
    list_add(class, c);
    _class_assemble(c);
    for (;;) {
        bool change = false;
        for (class cc = class_first; cc; cc = cc->next) {
            if ((cc->flags & CLASS_FLAG_ASSEMBLED) == 0)
                change |= _class_assemble(cc);
        }
        if (!change)
            break;
    }
}

void class_init() {
    bool found;

    do {
        found = false;
        for (class_Base c = (class_Base)class_first; c; c = (class_Base)c->next) {
            class_Base c_init = NULL;
            for (class_Base cc = c; cc; cc = (class_Base)cc->parent) {
                if ((cc->flags & CLASS_FLAG_PREINIT) == 0) {
                    c_init = cc;
                    found = true;
                }
                if (cc->parent == cc)
                    break;
            }
            if (c_init) {
                c_init->flags |= CLASS_FLAG_PREINIT;
                c_init->class_preinit((class)c_init);
            }
        }
    } while (found);
    do {
        found = false;
        for (class_Base c = (class_Base)class_first; c; c = (class_Base)c->next) {
            class_Base c_init = NULL;
            for (class_Base cc = c; cc; cc = (class_Base)cc->parent) {
                if ((cc->flags & CLASS_FLAG_INIT) == 0) {
                    c_init = cc;
                    found = true;
                }
                if (cc->parent == cc)
                    break;
            }
            if (c_init) {
                c_init->flags |= CLASS_FLAG_INIT;
                c_init->class_init((class)c_init);
            }
        }
    } while (found);
}

void *alloc_bytes(size_t size) {
    void *m = malloc(size);
    if (m)
        memset(m, 0, size);
    return m;
}
