#include "module.h"
void Base__init(Base self) {
}
Class  Base_get_cl(Base self) { return self->cl; }
Class  Base_set_cl(Base self, cl value) { return self->cl = value; }
int  Base_get_refs(Base self) { return self->refs; }
int  Base_set_refs(Base self, refs value) { return self->refs = value; }
void Base_init(Base self){
    }
    
Base Base_release(Base self){
        if (--self->refs == 0) {
            Base_cl.free_object(self);
        }
    }
    
Base Base_retain(Base self){
        self->refs++;
        return ;
    }
    
void Base_dealloc(Base self){
        free();
    }
    
BaseMethod Base_init_object(Base obj, Class with_cl){
        BaseMethod i = null;
        if (with_cl->parent)
            i = Base_cl.init_object(obj, with_cl.parent);
        if (i != with_cl->init)
            Class_cl.init(with_cl,obj);
        return with_cl->init;
    }
    
Base Base_new_object(Class cl, size_t extra_size){
        Base obj = (Base)alloc_bytes(cl->object_size + extra_size);
        obj->cl = (BaseClass);
        Base_cl.init_object(obj, obj.cl);
    }
    
Base Base_free_object(Base obj){
        Class c_parent = obj->cl.parent;
        BaseMethod last_method = null;
        for (Class c = obj->cl; ; = c->parent) {
            if (c->dealloc != last_method) {
                Class_cl.dealloc(c,obj);
                last_method = (BaseMethod)c->dealloc;
            }
        }
    }

void Class__init(Class self) {
}
Class  Class_get_parent(Class self) { return self->parent; }
Class  Class_set_parent(Class self, parent value) { return self->parent = value; }
const char *  Class_get_class_name(Class self) { return self->class_name; }
const char *  Class_set_class_name(Class self, class_name value) { return self->class_name = value; }
uint_t  Class_get_flags(Class self) { return self->flags; }
uint_t  Class_set_flags(Class self, flags value) { return self->flags = value; }
uint_t  Class_get_object_size(Class self) { return self->object_size; }
uint_t  Class_set_object_size(Class self, object_size value) { return self->object_size = value; }
uint_t  Class_get_member_count(Class self) { return self->member_count; }
uint_t  Class_set_member_count(Class self, member_count value) { return self->member_count = value; }
const char *  Class_get_member_types(Class self) { return self->member_types; }
const char *  Class_set_member_types(Class self, member_types value) { return self->member_types = value; }
const char * *  Class_get_member_names(Class self) { return self->member_names; }
const char * *  Class_set_member_names(Class self, member_names value) { return self->member_names = value; }
Method * *  Class_get_members(Class self) { return self->members; }
Method * *  Class_set_members(Class self, members value) { return self->members = value; }
class_Base Base_cl;
class_Class Class_cl;

static void module_constructor(void) __attribute__(constructor) {
	Base_cl._init = Base__init;
	Base_cl.get_cl = Base_get_cl;
	Base_cl.set_cl = Base_set_cl;
	Base_cl.get_refs = Base_get_refs;
	Base_cl.set_refs = Base_set_refs;
	Base_cl.init = Base_init;
	Base_cl.release = Base_release;
	Base_cl.retain = Base_retain;
	Base_cl.dealloc = Base_dealloc;
	Base_cl.init_object = Base_init_object;
	Base_cl.new_object = Base_new_object;
	Base_cl.free_object = Base_free_object;
	Base_cl.mtypes = (char *)malloc(11);
	Base_cl.mtypes[0] = 0;
	Base_cl.mtypes[1] = 0;
	Base_cl.mtypes[2] = 0;
	Base_cl.mtypes[3] = 0;
	Base_cl.mtypes[4] = 1;
	Base_cl.mtypes[5] = 1;
	Base_cl.mtypes[6] = 1;
	Base_cl.mtypes[7] = 1;
	Base_cl.mtypes[8] = 1;
	Base_cl.mtypes[9] = 1;
	Base_cl.mtypes[10] = 1;
	Base_cl.mnames = (const char **)malloc(11 * sizeof(const char *));
	Base_cl.mnames[0] = "cl";
	Base_cl.mnames[1] = "cl";
	Base_cl.mnames[2] = "refs";
	Base_cl.mnames[3] = "refs";
	Base_cl.mnames[4] = "init";
	Base_cl.mnames[5] = "release";
	Base_cl.mnames[6] = "retain";
	Base_cl.mnames[7] = "dealloc";
	Base_cl.mnames[8] = "init_object";
	Base_cl.mnames[9] = "new_object";
	Base_cl.mnames[10] = "free_object";
	Base_cl.mcount = 11;
	Class_cl.parent = &Base_cl;
	Class_cl._init = Class__init;
	Class_cl.get_cl = Base_get_cl;
	Class_cl.set_cl = Base_set_cl;
	Class_cl.get_refs = Base_get_refs;
	Class_cl.set_refs = Base_set_refs;
	Class_cl.init = Base_init;
	Class_cl.release = Base_release;
	Class_cl.retain = Base_retain;
	Class_cl.dealloc = Base_dealloc;
	Class_cl.init_object = Base_init_object;
	Class_cl.new_object = Base_new_object;
	Class_cl.free_object = Base_free_object;
	Class_cl.get_parent = Class_get_parent;
	Class_cl.set_parent = Class_set_parent;
	Class_cl.get_class_name = Class_get_class_name;
	Class_cl.set_class_name = Class_set_class_name;
	Class_cl.get_flags = Class_get_flags;
	Class_cl.set_flags = Class_set_flags;
	Class_cl.get_object_size = Class_get_object_size;
	Class_cl.set_object_size = Class_set_object_size;
	Class_cl.get_member_count = Class_get_member_count;
	Class_cl.set_member_count = Class_set_member_count;
	Class_cl.get_member_types = Class_get_member_types;
	Class_cl.set_member_types = Class_set_member_types;
	Class_cl.get_member_names = Class_get_member_names;
	Class_cl.set_member_names = Class_set_member_names;
	Class_cl.get_members = Class_get_members;
	Class_cl.set_members = Class_set_members;
	Class_cl.mtypes = (char *)malloc(27);
	Class_cl.mtypes[0] = 0;
	Class_cl.mtypes[1] = 0;
	Class_cl.mtypes[2] = 0;
	Class_cl.mtypes[3] = 0;
	Class_cl.mtypes[4] = 1;
	Class_cl.mtypes[5] = 1;
	Class_cl.mtypes[6] = 1;
	Class_cl.mtypes[7] = 1;
	Class_cl.mtypes[8] = 1;
	Class_cl.mtypes[9] = 1;
	Class_cl.mtypes[10] = 1;
	Class_cl.mtypes[11] = 0;
	Class_cl.mtypes[12] = 0;
	Class_cl.mtypes[13] = 0;
	Class_cl.mtypes[14] = 0;
	Class_cl.mtypes[15] = 0;
	Class_cl.mtypes[16] = 0;
	Class_cl.mtypes[17] = 0;
	Class_cl.mtypes[18] = 0;
	Class_cl.mtypes[19] = 0;
	Class_cl.mtypes[20] = 0;
	Class_cl.mtypes[21] = 0;
	Class_cl.mtypes[22] = 0;
	Class_cl.mtypes[23] = 0;
	Class_cl.mtypes[24] = 0;
	Class_cl.mtypes[25] = 0;
	Class_cl.mtypes[26] = 0;
	Class_cl.mnames = (const char **)malloc(27 * sizeof(const char *));
	Class_cl.mnames[0] = "cl";
	Class_cl.mnames[1] = "cl";
	Class_cl.mnames[2] = "refs";
	Class_cl.mnames[3] = "refs";
	Class_cl.mnames[4] = "init";
	Class_cl.mnames[5] = "release";
	Class_cl.mnames[6] = "retain";
	Class_cl.mnames[7] = "dealloc";
	Class_cl.mnames[8] = "init_object";
	Class_cl.mnames[9] = "new_object";
	Class_cl.mnames[10] = "free_object";
	Class_cl.mnames[11] = "parent";
	Class_cl.mnames[12] = "parent";
	Class_cl.mnames[13] = "class_name";
	Class_cl.mnames[14] = "class_name";
	Class_cl.mnames[15] = "flags";
	Class_cl.mnames[16] = "flags";
	Class_cl.mnames[17] = "object_size";
	Class_cl.mnames[18] = "object_size";
	Class_cl.mnames[19] = "member_count";
	Class_cl.mnames[20] = "member_count";
	Class_cl.mnames[21] = "member_types";
	Class_cl.mnames[22] = "member_types";
	Class_cl.mnames[23] = "member_names";
	Class_cl.mnames[24] = "member_names";
	Class_cl.mnames[25] = "members";
	Class_cl.mnames[26] = "members";
	Class_cl.mcount = 27;
}
