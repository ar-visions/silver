#include "module.h"
void Super__init(Super self) {
self->test_me = 1;
}
int  Super_get_test_me(Super self) { return (int )self->test_me; }
int  Super_set_test_me(Super self, int  value) { return (int )(self->test_me = (typeof(self->test_me))value); }
void Super_super_only(Super self, int test){
        printf("super only");
    }
    
void Super_method(Super self, int arg, int arg2){
        printf("original function: %d", self->test_me);
    }

SuperClass Super_cl;

static void module_constructor(void) __attribute__(constructor) {
	Super_cl = (typeof(Super_cl))alloc_bytes(sizeof(*Super_cl));
	Super_cl->parent = &Base_cl;
	Super_cl->_init = Super__init;
	Super_cl->get_cl = Base_get_cl;
	Super_cl->set_cl = Base_set_cl;
	Super_cl->get_refs = Base_get_refs;
	Super_cl->set_refs = Base_set_refs;
	Super_cl->init = Base_init;
	Super_cl->release = Base_release;
	Super_cl->retain = Base_retain;
	Super_cl->dealloc = Base_dealloc;
	Super_cl->init_object = Base_init_object;
	Super_cl->new_object = Base_new_object;
	Super_cl->free_object = Base_free_object;
	Super_cl->get_test_me = Super_get_test_me;
	Super_cl->set_test_me = Super_set_test_me;
	Super_cl->super_only = Super_super_only;
	Super_cl->method = Super_method;
	Super_cl->member_types = (char *)malloc(15);
	Super_cl->member_types[0] = 0;
	Super_cl->member_types[1] = 0;
	Super_cl->member_types[2] = 0;
	Super_cl->member_types[3] = 0;
	Super_cl->member_types[4] = 1;
	Super_cl->member_types[5] = 1;
	Super_cl->member_types[6] = 1;
	Super_cl->member_types[7] = 1;
	Super_cl->member_types[8] = 1;
	Super_cl->member_types[9] = 1;
	Super_cl->member_types[10] = 1;
	Super_cl->member_types[11] = 0;
	Super_cl->member_types[12] = 0;
	Super_cl->member_types[13] = 1;
	Super_cl->member_types[14] = 1;
	Super_cl->member_names = (const char **)malloc(15 * sizeof(const char *));
	Super_cl->member_names[0] = "cl";
	Super_cl->member_names[1] = "cl";
	Super_cl->member_names[2] = "refs";
	Super_cl->member_names[3] = "refs";
	Super_cl->member_names[4] = "init";
	Super_cl->member_names[5] = "release";
	Super_cl->member_names[6] = "retain";
	Super_cl->member_names[7] = "dealloc";
	Super_cl->member_names[8] = "init_object";
	Super_cl->member_names[9] = "new_object";
	Super_cl->member_names[10] = "free_object";
	Super_cl->member_names[11] = "test_me";
	Super_cl->member_names[12] = "test_me";
	Super_cl->member_names[13] = "super_only";
	Super_cl->member_names[14] = "method";
	Super_cl->member_count = 15;
}
