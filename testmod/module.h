#ifndef __TESTMOD_H__
#define __TESTMOD_H__
#include <base/module.h>
#include <supermod/module.h>

typedef struct _TestClass {
	struct _Class *cl;
	int refs;
	struct _SuperClass *parent;
	const char *class_name;
	uint_t flags;
	uint_t object_size;
	uint_t *member_count;
	char *member_types;
	const char **member_names;
	Method **members[1];
	struct _Class * (*get_cl)(struct _Test *);
	void (*set_cl)(struct _Test *, struct _Class *);
	int  (*get_refs)(struct _Test *);
	void (*set_refs)(struct _Test *, int );
	void  (*init)(struct _Test *);
	struct _Base * (*release)(struct _Test *);
	struct _Base * (*retain)(struct _Test *);
	void  (*dealloc)(struct _Test *);
	BaseMethod  (*init_object)(struct _Base *, struct _Class *);
	struct _Base * (*new_object)(struct _Class *, size_t );
	struct _Base * (*free_object)(struct _Base *);
	int  (*get_test_me)(struct _Test *);
	void (*set_test_me)(struct _Test *, int );
	void  (*super_only)(struct _Test *, int );
	Test (*Test)();
	Test (*i)(int );
	Test (*s)(String );
	int  (*get_test1)(struct _Test *);
	void (*set_test1)(struct _Test *, int );
	int  (*get_value_intern)(struct _Test *);
	void (*set_value_intern)(struct _Test *, int );
	int  (*get_value_test)();
	void (*set_value_test)(int );
	int  (*get_value)(struct _Test *);
	void (*set_value)(struct _Test *, int );
	void  (*method)(struct _Test *, int , int );
	int  (*Main)();
} *TestClass;

extern TestClass Test_cl;


typedef struct _Test {
	TestClass cl;
	int  refs;
	int  test_me;
	int  test1;
	int  value_intern;
} *Test;

#endif
