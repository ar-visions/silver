#include "module.h"
void Super__init(Super self) {
}
void Super_super_only(struct _Super  *self, int test){
        printf("super only");
    }
    
void Super_method(struct _Super  *self, int arg, int arg2){
        printf("original function");
    }

class_Super Super_cl;

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
}
