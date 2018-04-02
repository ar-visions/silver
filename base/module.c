#include "module.h"
void Base__init(Base self) {
}
Class  Base_get_cl(Base self) { return (Class )self->cl; }
Class  Base_set_cl(Base self, Class  value) { return (Class )(self->cl = (typeof(self->cl))value); }
int  Base_get_refs(Base self) { return (int )self->refs; }
int  Base_set_refs(Base self, int  value) { return (int )(self->refs = (typeof(self->refs))value); }
void Base_init(Base self){
    }
    
Base Base_release(Base self){
        
        int this_is_a_test = Base_cl->arg_test(Base_cl->release(Base_cl->retain(self)),self->refs));

        if (--self->refs == 0) {
            Base_cl->free_object(self));
        }
    }
    
Base Base_retain(Base self){
        self->refs++;
        return self;
    }
    
Base Base_arg_test(Base self, int arg){
        return self;
    }
    
void Base_dealloc(Base self){
        free(self);
    }
    
BaseMethod Base_init_object(Base obj, Class with_cl){
        BaseMethod i = null;
        if (with_cl->parent)
            i = Base_cl->init_object(obj, with_cl->parent));
        if (i != with_cl->init)
            Class_cl->init(with_clobj));
        Class_cl->_init(with_clobj));
        return with_cl->init;
    }
    
Base Base_new_object(Class cl, size_t extra_size){
        Base obj = (Base)alloc_bytes(cl->object_size + extra_size);
        obj->cl = (BaseClass)cl;
        Base_cl->init_object(obj, obj->cl));
    }
    
Base Base_free_object(Base obj){
        Class c_parent = obj->cl->parent;
        BaseMethod last_method = null;
        for (Class c = obj->cl; c; c = c->parent) {
            if (c->dealloc != last_method) {
                Class_cl->dealloc(cobj));
                last_method = (BaseMethod)c->dealloc;
            }
        }
    }

void Class__init(Class self) {
}
Class  Class_get_parent(Class self) { return (Class )self->parent; }
Class  Class_set_parent(Class self, Class  value) { return (Class )(self->parent = (typeof(self->parent))value); }
const char *  Class_get_class_name(Class self) { return (const char * )self->class_name; }
const char *  Class_set_class_name(Class self, const char *  value) { return (const char * )(self->class_name = (typeof(self->class_name))value); }
uint_t  Class_get_flags(Class self) { return (uint_t )self->flags; }
uint_t  Class_set_flags(Class self, uint_t  value) { return (uint_t )(self->flags = (typeof(self->flags))value); }
uint_t  Class_get_object_size(Class self) { return (uint_t )self->object_size; }
uint_t  Class_set_object_size(Class self, uint_t  value) { return (uint_t )(self->object_size = (typeof(self->object_size))value); }
uint_t  Class_get_member_count(Class self) { return (uint_t )self->member_count; }
uint_t  Class_set_member_count(Class self, uint_t  value) { return (uint_t )(self->member_count = (typeof(self->member_count))value); }
const char *  Class_get_member_types(Class self) { return (const char * )self->member_types; }
const char *  Class_set_member_types(Class self, const char *  value) { return (const char * )(self->member_types = (typeof(self->member_types))value); }
const char * *  Class_get_member_names(Class self) { return (const char * * )self->member_names; }
const char * *  Class_set_member_names(Class self, const char * *  value) { return (const char * * )(self->member_names = (typeof(self->member_names))value); }
Method * *  Class_get_members(Class self) { return (Method * * )self->members; }
Method * *  Class_set_members(Class self, Method * *  value) { return (Method * * )(self->members = (typeof(self->members))value); }
BaseClass Base_cl;
Class Class_cl;

static void module_constructor(void) __attribute__(constructor) {
	Base_cl = (typeof(Base_cl))alloc_bytes(sizeof(*Base_cl));
	Base_cl->_init = Base__init;
	Base_cl->get_cl = Base_get_cl;
	Base_cl->set_cl = Base_set_cl;
	Base_cl->get_refs = Base_get_refs;
	Base_cl->set_refs = Base_set_refs;
	Base_cl->init = Base_init;
	Base_cl->release = Base_release;
	Base_cl->retain = Base_retain;
	Base_cl->arg_test = Base_arg_test;
	Base_cl->dealloc = Base_dealloc;
	Base_cl->init_object = Base_init_object;
	Base_cl->new_object = Base_new_object;
	Base_cl->free_object = Base_free_object;
	Base_cl->member_types = (char *)malloc(12);
	Base_cl->member_types[0] = 0;
	Base_cl->member_types[1] = 0;
	Base_cl->member_types[2] = 0;
	Base_cl->member_types[3] = 0;
	Base_cl->member_types[4] = 1;
	Base_cl->member_types[5] = 1;
	Base_cl->member_types[6] = 1;
	Base_cl->member_types[7] = 1;
	Base_cl->member_types[8] = 1;
	Base_cl->member_types[9] = 1;
	Base_cl->member_types[10] = 1;
	Base_cl->member_types[11] = 1;
	Base_cl->member_names = (const char **)malloc(12 * sizeof(const char *));
	Base_cl->member_names[0] = "cl";
	Base_cl->member_names[1] = "cl";
	Base_cl->member_names[2] = "refs";
	Base_cl->member_names[3] = "refs";
	Base_cl->member_names[4] = "init";
	Base_cl->member_names[5] = "release";
	Base_cl->member_names[6] = "retain";
	Base_cl->member_names[7] = "arg_test";
	Base_cl->member_names[8] = "dealloc";
	Base_cl->member_names[9] = "init_object";
	Base_cl->member_names[10] = "new_object";
	Base_cl->member_names[11] = "free_object";
	Base_cl->member_count = 12;
	Class_cl = (typeof(Class_cl))alloc_bytes(sizeof(*Class_cl));
	Class_cl->parent = Base_cl;
	Class_cl->_init = Class__init;
	Class_cl->get_cl = Base_get_cl;
	Class_cl->set_cl = Base_set_cl;
	Class_cl->get_refs = Base_get_refs;
	Class_cl->set_refs = Base_set_refs;
	Class_cl->init = Base_init;
	Class_cl->release = Base_release;
	Class_cl->retain = Base_retain;
	Class_cl->arg_test = Base_arg_test;
	Class_cl->dealloc = Base_dealloc;
	Class_cl->init_object = Base_init_object;
	Class_cl->new_object = Base_new_object;
	Class_cl->free_object = Base_free_object;
	Class_cl->get_parent = Class_get_parent;
	Class_cl->set_parent = Class_set_parent;
	Class_cl->get_class_name = Class_get_class_name;
	Class_cl->set_class_name = Class_set_class_name;
	Class_cl->get_flags = Class_get_flags;
	Class_cl->set_flags = Class_set_flags;
	Class_cl->get_object_size = Class_get_object_size;
	Class_cl->set_object_size = Class_set_object_size;
	Class_cl->get_member_count = Class_get_member_count;
	Class_cl->set_member_count = Class_set_member_count;
	Class_cl->get_member_types = Class_get_member_types;
	Class_cl->set_member_types = Class_set_member_types;
	Class_cl->get_member_names = Class_get_member_names;
	Class_cl->set_member_names = Class_set_member_names;
	Class_cl->get_members = Class_get_members;
	Class_cl->set_members = Class_set_members;
	Class_cl->member_types = (char *)malloc(28);
	Class_cl->member_types[0] = 0;
	Class_cl->member_types[1] = 0;
	Class_cl->member_types[2] = 0;
	Class_cl->member_types[3] = 0;
	Class_cl->member_types[4] = 1;
	Class_cl->member_types[5] = 1;
	Class_cl->member_types[6] = 1;
	Class_cl->member_types[7] = 1;
	Class_cl->member_types[8] = 1;
	Class_cl->member_types[9] = 1;
	Class_cl->member_types[10] = 1;
	Class_cl->member_types[11] = 1;
	Class_cl->member_types[12] = 0;
	Class_cl->member_types[13] = 0;
	Class_cl->member_types[14] = 0;
	Class_cl->member_types[15] = 0;
	Class_cl->member_types[16] = 0;
	Class_cl->member_types[17] = 0;
	Class_cl->member_types[18] = 0;
	Class_cl->member_types[19] = 0;
	Class_cl->member_types[20] = 0;
	Class_cl->member_types[21] = 0;
	Class_cl->member_types[22] = 0;
	Class_cl->member_types[23] = 0;
	Class_cl->member_types[24] = 0;
	Class_cl->member_types[25] = 0;
	Class_cl->member_types[26] = 0;
	Class_cl->member_types[27] = 0;
	Class_cl->member_names = (const char **)malloc(28 * sizeof(const char *));
	Class_cl->member_names[0] = "cl";
	Class_cl->member_names[1] = "cl";
	Class_cl->member_names[2] = "refs";
	Class_cl->member_names[3] = "refs";
	Class_cl->member_names[4] = "init";
	Class_cl->member_names[5] = "release";
	Class_cl->member_names[6] = "retain";
	Class_cl->member_names[7] = "arg_test";
	Class_cl->member_names[8] = "dealloc";
	Class_cl->member_names[9] = "init_object";
	Class_cl->member_names[10] = "new_object";
	Class_cl->member_names[11] = "free_object";
	Class_cl->member_names[12] = "parent";
	Class_cl->member_names[13] = "parent";
	Class_cl->member_names[14] = "class_name";
	Class_cl->member_names[15] = "class_name";
	Class_cl->member_names[16] = "flags";
	Class_cl->member_names[17] = "flags";
	Class_cl->member_names[18] = "object_size";
	Class_cl->member_names[19] = "object_size";
	Class_cl->member_names[20] = "member_count";
	Class_cl->member_names[21] = "member_count";
	Class_cl->member_names[22] = "member_types";
	Class_cl->member_names[23] = "member_types";
	Class_cl->member_names[24] = "member_names";
	Class_cl->member_names[25] = "member_names";
	Class_cl->member_names[26] = "members";
	Class_cl->member_names[27] = "members";
	Class_cl->member_count = 28;
}
