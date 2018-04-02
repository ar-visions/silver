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
	void  (*init)(p_@›‚);
	struct _Base * (*release)(p_@›‚);
	struct _Base * (*retain)(p_@›‚);
	struct _Base * (*arg_test)(p_@›‚);
	void  (*dealloc)(p_@›‚);
	BaseMethod  (*init_object)(p_@›‚);
	struct _Base * (*new_object)(p_@›‚);
	struct _Base * (*free_object)(p_@›‚);
	int  (*get_test_me)(struct _Test *);
	void (*set_test_me)(struct _Test *, int );
	void  (*super_only)(p_@›‚);
	Test (*Test)(p_@›‚);
	Test (*i)(p_@›‚);
	Test (*s)(p_@›‚);
	int  (*get_test1)(struct _Test *);
	void (*set_test1)(struct _Test *, int );
	int  (*get_value_intern)(struct _Test *);
	void (*set_value_intern)(struct _Test *, int );
	int  (*get_value_test)();
	void (*set_value_test)(int );
	int  (*get_value)(struct _Test *);
	void (*set_value)(struct _Test *, int );
	void  (*method)(p_@›‚);
	int  (*Main)(p_@›‚);
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
