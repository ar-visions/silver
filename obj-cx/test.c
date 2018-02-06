#include <stdio.h>
#include <stdlib.h>

typedef struct _class_Super {
        struct _class_Super *parent;
        const char *class_name;
        unsigned int flags;
        int obj_size;
        void (*_init)(struct _object_Base *);   int *mcount;
        unsigned char *mtypes;
        const char **mnames;
        Method **m[1];
        void  (*super_only)(struct _object_Super  *, int );
        void  (*method)(struct _object_Super  *, int , int );
} class_Super Super_cl;


typedef struct _object_Super {
        struct _class_Super *cl;
        LItem *ar_node;
        int refs;
} object_Super;


typedef struct _class_Test {
        struct _class_Test *parent;
        const char *class_name;
        unsigned int flags;
        int obj_size;
        void (*_init)(struct _object_Super *);  int *mcount;
        unsigned char *mtypes;
        const char **mnames;
        Method **m[1];
        void  (*super_only)(struct _object_Test  *, int );
        Test (*Test)();
        Test (*i)(int );
        Test (*s)(String );
        int  (*get_test1)();
        void (*set_test1)(int );
        int  (*get_value)();
        void (*set_value)(int );
        void  (*method)(struct _object_Test  *, int , int );
        int  (*Main)();
} class_Test Test_cl;


typedef struct _object_Test {
        struct _class_Test *cl;
        LItem *ar_node;
        int refs;
int test1;
} object_Test;

void Super__init(Super self) {
}
void Super_super_only(struct _object_Super  *self, int test){
        printf("super only");
    }

void Super_method(struct _object_Super  *self, int arg, int arg2){
        printf("original function");
    }

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

int Test_get_value (Test self){
            return Test_cl.get_value(self);
        }

void Test_set_value (Test self, int value){
            return Test_cl.get_value(self);
        }

void Test_method(struct _object_Test  *self, int arg, int arg2){
        printf("value = %d\n", Test_cl.get_value(self));
        Test t = Test_cl.Test(&Test_cl, false,test1,test2);
    }

int Test_Main(){
        return 0;
    }

int main() {
        return Test_Main();
}
static void module_constructor(void) __attribute__(constructor) {
        Super_cl._init = Super__init;
        Super_cl.super_only = Super_super_only;
        Super_cl.method = Super_method;
        Super_cl.mtypes = (char *)malloc(2);
        Super_cl.mtypes[0] = 1;
        Super_cl.mtypes[1] = 1;
        Super_cl.mnames = (const char **)malloc(2 * sizeof(const char *));
        Super_cl.mnames[0] = "super_only";
        Super_cl.mnames[1] = "method";
        Super_cl.mcount = 2;
        Test_cl.parent = &Super_cl;
        Test_cl._init = Test__init;
        Test_cl.super_only = Super_super_only;
        Test_cl.Test = construct_Test_Test;
        Test_cl.i = construct_Test_i;
        Test_cl.s = construct_Test_s;
        Test_cl.get_test1 = Test_get_test1;
        Test_cl.set_test1 = Test_set_test1;
        Test_cl.get_value = Test_get_value;
        Test_cl.set_value = Test_set_value;
        Test_cl.method = Test_method;
        Test_cl.Main = Test_Main;
        Test_cl.mtypes = (char *)malloc(10);
        Test_cl.mtypes[0] = 1;
        Test_cl.mtypes[1] = 2;
        Test_cl.mtypes[2] = 2;
        Test_cl.mtypes[3] = 2;
        Test_cl.mtypes[4] = 0;
        Test_cl.mtypes[5] = 0;
        Test_cl.mtypes[6] = 0;
        Test_cl.mtypes[7] = 0;
        Test_cl.mtypes[8] = 1;
        Test_cl.mtypes[9] = 1;
        Test_cl.mnames = (const char **)malloc(10 * sizeof(const char *));
        Test_cl.mnames[0] = "super_only";
        Test_cl.mnames[1] = "Test";
        Test_cl.mnames[2] = "i";
        Test_cl.mnames[3] = "s";
        Test_cl.mnames[4] = "test1";
        Test_cl.mnames[5] = "test1";
        Test_cl.mnames[6] = "value";
        Test_cl.mnames[7] = "value";
        Test_cl.mnames[8] = "method";
        Test_cl.mnames[9] = "Main";
        Test_cl.mcount = 10;
}