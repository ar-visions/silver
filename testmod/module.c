#include "module.h"
void Test__init(Test self) {
self->test_me = 1;
self->test1 = 1;
self->cl->set_value(self, 2);
}
Test Test_construct_Test(struct _class_Test *cl, bool ar) {
Test self = object_alloc(cl, 0);
self->cl->_init(self);
printf("default constructor\n");
    return ar ? object_auto(self) : self;
}

Test Test_construct_i(struct _class_Test *cl, bool ar, int arg) {
Test self = object_alloc(cl, 0);
self->cl->_init(self);
Test_cl.set_value (self,arg);
    return ar ? object_auto(self) : self;
}

Test Test_construct_s(struct _class_Test *cl, bool ar, String arg) {
Test self = object_alloc(cl, 0);
self->cl->_init(self);
Test_cl.set_value (self,1);
    return ar ? object_auto(self) : self;
}

int  Test_get_test1(Test self) { return self->test1; }
int  Test_set_test1(Test self, test1 value) { return self->test1 = value; }
int  Test_get_value_intern(Test self) { return self->value_intern; }
int  Test_set_value_intern(Test self, value_intern value) { return self->value_intern = value; }
int Test_get_value_test (){
            return 1;
        }
        
int Test_set_value_test (int value){
            do_nothing;
        return Test_cl.get_value_test();}
    
int Test_get_value (Test self){
            return self->value_intern;
        }
        
int Test_set_value (Test self, int value){
            self->value_intern = value;
        return self->get_value(self);}
    
void Test_method(Test self, int arg, int arg2){
        printf("value = %d\n", Test_cl.get_value(self));
        Test t = Test(test1, test2);
    }
    
int Test_Main(){
        Test t = Test();
        t.method(1, 2);
        t.value = 1;
        return 0;
    }

int main() {
	return Test_Main();
}
class_Test Test_cl;

static void module_constructor(void) __attribute__(constructor) {
	Test_cl.parent = &Super_cl;
	Test_cl._init = Test__init;
	Test_cl.get_cl = Base_get_cl;
	Test_cl.set_cl = Base_set_cl;
	Test_cl.get_refs = Base_get_refs;
	Test_cl.set_refs = Base_set_refs;
	Test_cl.init = Base_init;
	Test_cl.release = Base_release;
	Test_cl.retain = Base_retain;
	Test_cl.dealloc = Base_dealloc;
	Test_cl.init_object = Base_init_object;
	Test_cl.new_object = Base_new_object;
	Test_cl.free_object = Base_free_object;
	Test_cl.get_test_me = Super_get_test_me;
	Test_cl.set_test_me = Super_set_test_me;
	Test_cl.super_only = Super_super_only;
	Test_cl.Test = construct_Test_Test;
	Test_cl.i = construct_Test_i;
	Test_cl.s = construct_Test_s;
	Test_cl.get_test1 = Test_get_test1;
	Test_cl.set_test1 = Test_set_test1;
	Test_cl.get_value_intern = Test_get_value_intern;
	Test_cl.set_value_intern = Test_set_value_intern;
	Test_cl.get_value_test = Test_get_value_test;
	Test_cl.set_value_test = Test_set_value_test;
	Test_cl.get_value = Test_get_value;
	Test_cl.set_value = Test_set_value;
	Test_cl.method = Test_method;
	Test_cl.Main = Test_Main;
	Test_cl.mtypes = (char *)malloc(27);
	Test_cl.mtypes[0] = 0;
	Test_cl.mtypes[1] = 0;
	Test_cl.mtypes[2] = 0;
	Test_cl.mtypes[3] = 0;
	Test_cl.mtypes[4] = 1;
	Test_cl.mtypes[5] = 1;
	Test_cl.mtypes[6] = 1;
	Test_cl.mtypes[7] = 1;
	Test_cl.mtypes[8] = 1;
	Test_cl.mtypes[9] = 1;
	Test_cl.mtypes[10] = 1;
	Test_cl.mtypes[11] = 0;
	Test_cl.mtypes[12] = 0;
	Test_cl.mtypes[13] = 1;
	Test_cl.mtypes[14] = 2;
	Test_cl.mtypes[15] = 2;
	Test_cl.mtypes[16] = 2;
	Test_cl.mtypes[17] = 0;
	Test_cl.mtypes[18] = 0;
	Test_cl.mtypes[19] = 0;
	Test_cl.mtypes[20] = 0;
	Test_cl.mtypes[21] = 0;
	Test_cl.mtypes[22] = 0;
	Test_cl.mtypes[23] = 0;
	Test_cl.mtypes[24] = 0;
	Test_cl.mtypes[25] = 1;
	Test_cl.mtypes[26] = 1;
	Test_cl.mnames = (const char **)malloc(27 * sizeof(const char *));
	Test_cl.mnames[0] = "cl";
	Test_cl.mnames[1] = "cl";
	Test_cl.mnames[2] = "refs";
	Test_cl.mnames[3] = "refs";
	Test_cl.mnames[4] = "init";
	Test_cl.mnames[5] = "release";
	Test_cl.mnames[6] = "retain";
	Test_cl.mnames[7] = "dealloc";
	Test_cl.mnames[8] = "init_object";
	Test_cl.mnames[9] = "new_object";
	Test_cl.mnames[10] = "free_object";
	Test_cl.mnames[11] = "test_me";
	Test_cl.mnames[12] = "test_me";
	Test_cl.mnames[13] = "super_only";
	Test_cl.mnames[14] = "Test";
	Test_cl.mnames[15] = "i";
	Test_cl.mnames[16] = "s";
	Test_cl.mnames[17] = "test1";
	Test_cl.mnames[18] = "test1";
	Test_cl.mnames[19] = "value_intern";
	Test_cl.mnames[20] = "value_intern";
	Test_cl.mnames[21] = "value_test";
	Test_cl.mnames[22] = "value_test";
	Test_cl.mnames[23] = "value";
	Test_cl.mnames[24] = "value";
	Test_cl.mnames[25] = "method";
	Test_cl.mnames[26] = "Main";
	Test_cl.mcount = 27;
}
