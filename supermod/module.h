#ifndef __SUPERMOD_H__
#define __SUPERMOD_H__
#include <base/module.h>
#include <stdlib.h>

typedef struct _SuperClass {
	struct _Class *cl;
	int refs;
	struct _BaseClass *parent;
	const char *class_name;
	uint_t flags;
	uint_t object_size;
	uint_t *member_count;
	char *member_types;
	const char **member_names;
	Method **members[1];
	struct _Class * (*get_cl)(struct _Super *);
	void (*set_cl)(struct _Super *, struct _Class *);
	int  (*get_refs)(struct _Super *);
	void (*set_refs)(struct _Super *, int );
	void  (*init)(p_@›‚);
	struct _Base * (*release)(p_@›‚);
	struct _Base * (*retain)(p_@›‚);
	struct _Base * (*arg_test)(p_@›‚);
	void  (*dealloc)(p_@›‚);
	BaseMethod  (*init_object)(p_@›‚);
	struct _Base * (*new_object)(p_@›‚);
	struct _Base * (*free_object)(p_@›‚);
	int  (*get_test_me)(struct _Super *);
	void (*set_test_me)(struct _Super *, int );
	void  (*super_only)(p_@›‚);
	void  (*method)(p_@›‚);
} *SuperClass;

extern SuperClass Super_cl;


typedef struct _Super {
	SuperClass cl;
	int  refs;
	int  test_me;
} *Super;

#endif
