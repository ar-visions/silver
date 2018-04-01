#include "module.h"
void Super__init(Super self) {
self->test_me = 1;
}
int  Super_get_test_me(Super self) { return self->test_me; }
int  Super_set_test_me(Super self, test_me value) { return self->test_me = value; }
void Super_super_only(Super self, int test){
        printf("super only");
    }
    
void Super_method(Super self, int arg, int arg2){
        printf("original function: %d", self->test_me);
    }

class_Super Super_cl;

static void module_constructor(void) __attribute__(constructor) {
	Super_cl.parent = &Base_cl;
	Super_cl._init = Super__init;
	Super_cl.get_cl = Base_get_cl;
	Super_cl.set_cl = Base_set_cl;
	Super_cl.get_refs = Base_get_refs;
	Super_cl.set_refs = Base_set_refs;
	Super_cl.init = Base_init;
	Super_cl.release = Base_release;
	Super_cl.retain = Base_retain;
	Super_cl.dealloc = Base_dealloc;
	Super_cl.init_object = Base_init_object;
	Super_cl.new_object = Base_new_object;
	Super_cl.free_object = Base_free_object;
	Super_cl.get_test_me = Super_get_test_me;
	Super_cl.set_test_me = Super_set_test_me;
	Super_cl.super_only = Super_super_only;
	Super_cl.method = Super_method;
	Super_cl.mtypes = (char *)malloc(15);
	Super_cl.mtypes[0] = 0;
	Super_cl.mtypes[1] = 0;
	Super_cl.mtypes[2] = 0;
	Super_cl.mtypes[3] = 0;
	Super_cl.mtypes[4] = 1;
	Super_cl.mtypes[5] = 1;
	Super_cl.mtypes[6] = 1;
	Super_cl.mtypes[7] = 1;
	Super_cl.mtypes[8] = 1;
	Super_cl.mtypes[9] = 1;
	Super_cl.mtypes[10] = 1;
	Super_cl.mtypes[11] = 0;
	Super_cl.mtypes[12] = 0;
	Super_cl.mtypes[13] = 1;
	Super_cl.mtypes[14] = 1;
	Super_cl.mnames = (const char **)malloc(15 * sizeof(const char *));
	Super_cl.mnames[0] = "cl";
	Super_cl.mnames[1] = "cl";
	Super_cl.mnames[2] = "refs";
	Super_cl.mnames[3] = "refs";
	Super_cl.mnames[4] = "init";
	Super_cl.mnames[5] = "release";
	Super_cl.mnames[6] = "retain";
	Super_cl.mnames[7] = "dealloc";
	Super_cl.mnames[8] = "init_object";
	Super_cl.mnames[9] = "new_object";
	Super_cl.mnames[10] = "free_object";
	Super_cl.mnames[11] = "test_me";
	Super_cl.mnames[12] = "test_me";
	Super_cl.mnames[13] = "super_only";
	Super_cl.mnames[14] = "method";
	Super_cl.mcount = 15;
}
