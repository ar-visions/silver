#include <obj/obj.h>

static Class *classes;
static int classes_count;
static int classes_size;

static void class_set_methods(Class c) {
    for (int i = 0; i < c->mcount; i++) {
        if (!c->m[i]) {
            for (Class cc = c->parent; cc; cc = cc->parent) {
                if (cc->mcount < i)
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

Class class_find(const char *name) {
    Class c = NULL;
    for (int i = 0; i < classes_count; i++) {
        Class c = classes[i];
        if (strcmp(name, c->name) == 0)
            return c;
    }
    return NULL;
}

bool class_inherits(Class check, Class c) {
    while (check) {
        if (c == check)
            return true;
        if (check == check->parent)
            return false;
        check = check->parent;
    }
    return false;
}

Base object_inherits(Base o, Class c) {
    if (o && class_inherits((Class)o->cl, c)) {
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

typedef void (*free_call)(Base);
free_call call_frees(Base o, class_Base c, free_call *pf) {
    if (!c)
        return *pf;
    if (c->free != *pf) {
        free_call f = (c->parent != c) ? call_frees(o, c->parent, pf) : *pf;
        if (c->free != f) {
            c->free(o);
            *pf = c->free;
            return c->free;
        }
    }
    return *pf;
}

Base new_obj(class_Base c, size_t extra) {
    size_t alloc_size = c->obj_size + extra;
    Base self = (Base)class_alloc(c, alloc_size);
    self->refs = 1;
    self->alloc_size = alloc_size;
    self->cl = (class_Base const)c;
    self->super_object = self;
    if (c->init != Base_init) {
        init_call init = NULL;
        call_inits(self, (class_Base)self->cl, &init);
    }
    return self;
}

void free_obj(Base o) {
    class_Base c = o->cl;
    if (c->free != Base_free) {
        free_call f = NULL;
        call_frees(o, c, &f);
    }
    class_dealloc(c, o);
}

static bool _class_assemble(Class c) {
    Class cc = c;
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

void class_assemble(Class c) {
    if (!classes) {
        classes_count = 0;
        classes_size = 128;
        classes = (Class *)calloc(sizeof(Class), classes_size);
    }
    if (classes_size == classes_count) {
        size_t size_new = classes_size + classes_count;
        Class *classes_new = (Class *)calloc(sizeof(Class), size_new);
        memcpy(classes_new, classes, classes_count * sizeof(Class));
        free(classes);
        classes = classes_new;
        classes_size = size_new;
    }
    classes[classes_count++] = c;
    _class_assemble(c);
    for (;;) {
        bool change = false;
        for (int i = 0; i < classes_count; i++) {
            Class cc = classes[i];
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
        for (int i = 0; i < classes_count; i++) {
            Class c = classes[i];
            class_Base c_init = NULL;
            for (Class cc = c; cc; cc = cc->parent) {
                if ((cc->flags & CLASS_FLAG_PREINIT) == 0) {
                    c_init = (class_Base)cc;
                    found = true;
                }
                if (cc->parent == cc)
                    break;
            }
            if (c_init) {
                c_init->flags |= CLASS_FLAG_PREINIT;
                c_init->class_preinit((Class)c_init);
            }
        }
    } while (found);
    do {
        found = false;
        for (int i = 0; i < classes_count; i++) {
            Class c = classes[i];
            class_Base c_init = NULL;
            for (Class cc = c; cc; cc = cc->parent) {
                if ((cc->flags & CLASS_FLAG_INIT) == 0) {
                    c_init = (class_Base)cc;
                    found = true;
                }
                if (cc->parent == cc)
                    break;
            }
            if (c_init) {
                c_init->flags |= CLASS_FLAG_INIT;
                c_init->class_init((Class)c_init);
            }
        }
    } while (found);
}
