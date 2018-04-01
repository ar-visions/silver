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
	void  (*init)(struct _Super *);
	struct _Base * (*release)(struct _Super *);
	struct _Base * (*retain)(struct _Super *);
	void  (*dealloc)(struct _Super *);
	BaseMethod  (*init_object)(struct _Base *, struct _Class *);
	struct _Base * (*new_object)(struct _Class *, size_t );
	struct _Base * (*free_object)(struct _Base *);
	int  (*get_test_me)(struct _Super *);
	void (*set_test_me)(struct _Super *, int );
	void  (*super_only)(struct _Super *, int );
	void  (*method)(struct _Super *, int , int );
} *SuperClass;

extern SuperClass Super_cl;


typedef struct _Super {
	SuperClass cl;
	int  refs;
	int  test_me;
} *Super;

#endif
