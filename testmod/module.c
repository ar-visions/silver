#include "module.h"
void Test__init(Test self) {
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
    
void Test_method(struct _Test  *self, int arg, int arg2){
        printf("value = %d\n", Test_cl.get_value(self));
        Test t = Test_cl.Test(&Test_cl, false,test1, test2);
    }
    
int Test_Main(){
        Test t = Test_cl.Test(&Test_cl, false);
        Test_cl.method(t,1, 2);
        Test_cl.set_value (t,1);
        return 0;
    }

int main() {
	return Test_Main();
}
class_Test Test_cl;

static void module_constructor(void) __attribute__(constructor) {
	Test_cl._init = Test__init;
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
	Test_cl.mtypes = (char *)malloc(13);
	Test_cl.mtypes[0] = 2;
	Test_cl.mtypes[1] = 2;
	Test_cl.mtypes[2] = 2;
	Test_cl.mtypes[3] = 0;
	Test_cl.mtypes[4] = 0;
	Test_cl.mtypes[5] = 0;
	Test_cl.mtypes[6] = 0;
	Test_cl.mtypes[7] = 0;
	Test_cl.mtypes[8] = 0;
	Test_cl.mtypes[9] = 0;
	Test_cl.mtypes[10] = 0;
	Test_cl.mtypes[11] = 1;
	Test_cl.mtypes[12] = 1;
	Test_cl.mnames = (const char **)malloc(13 * sizeof(const char *));
	Test_cl.mnames[0] = "Test";
	Test_cl.mnames[1] = "i";
	Test_cl.mnames[2] = "s";
	Test_cl.mnames[3] = "test1";
	Test_cl.mnames[4] = "test1";
	Test_cl.mnames[5] = "value_intern";
	Test_cl.mnames[6] = "value_intern";
	Test_cl.mnames[7] = "value_test";
	Test_cl.mnames[8] = "value_test";
	Test_cl.mnames[9] = "value";
	Test_cl.mnames[10] = "value";
	Test_cl.mnames[11] = "method";
	Test_cl.mnames[12] = "Main";
	Test_cl.mcount = 13;
}
