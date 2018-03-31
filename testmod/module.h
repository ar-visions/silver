#ifndef __TESTMOD_H__
#define __TESTMOD_H__
#include <supermod/module.h>

typedef struct _class_Test {
	struct _class_Test *parent;
	const char *class_name;
	unsigned int flags;
	int obj_size;
	void (*_init)(struct _Base *);	int *mcount;
	unsigned char *mtypes;
	const char **mnames;
	Method **m[1];
	Test (*Test)();
	Test (*i)(int );
	Test (*s)(String );
	int  (*get_test1)();
	void (*set_test1)(int );
	int  (*get_value_intern)();
	void (*set_value_intern)(int );
	int  (*get_value_test)();
	void (*set_value_test)(int );
	int  (*get_value)();
	void (*set_value)(int );
	void  (*method)(struct _Test  *, int , int );
	int  (*Main)();
} class_Test;

extern class_Test Test_cl;


typedef struct _Test {
	struct _class_Test *cl;
	LItem *ar_node;
	int refs;
int test1;
int value_intern;
} Test;

#endif
