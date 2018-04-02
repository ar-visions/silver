#ifndef __BASE_H__
#define __BASE_H__
#include <base.h>

typedef struct _BaseClass {
	struct _Class *cl;
	int refs;
	struct _Class *parent;
	const char *class_name;
	uint_t flags;
	uint_t object_size;
	uint_t *member_count;
	char *member_types;
	const char **member_names;
	Method **members[1];
	struct _Class * (*get_cl)(struct _Base *);
	void (*set_cl)(struct _Base *, struct _Class *);
	int  (*get_refs)(struct _Base *);
	void (*set_refs)(struct _Base *, int );
	void  (*init)(p_@›‚);
	struct _Base * (*release)(p_@›‚);
	struct _Base * (*retain)(p_@›‚);
	struct _Base * (*arg_test)(p_@›‚);
	void  (*dealloc)(p_@›‚);
	BaseMethod  (*init_object)(p_@›‚);
	struct _Base * (*new_object)(p_@›‚);
	struct _Base * (*free_object)(p_@›‚);
} *BaseClass;

extern BaseClass Base_cl;


typedef struct _Base {
	BaseClass cl;
	int  refs;
} *Base;


typedef struct _Class {
	struct _BaseClass *cl;
	int refs;
	struct _Class *parent;
	const char *class_name;
	uint_t flags;
	uint_t object_size;
	uint_t *member_count;
	char *member_types;
	const char **member_names;
	Method **members[1];
	struct _Class * (*get_cl)(struct _Class *);
	void (*set_cl)(struct _Class *, struct _Class *);
	int  (*get_refs)(struct _Class *);
	void (*set_refs)(struct _Class *, int );
	void  (*init)(p_@›‚);
	struct _Base * (*release)(p_@›‚);
	struct _Base * (*retain)(p_@›‚);
	struct _Base * (*arg_test)(p_@›‚);
	void  (*dealloc)(p_@›‚);
	BaseMethod  (*init_object)(p_@›‚);
	struct _Base * (*new_object)(p_@›‚);
	struct _Base * (*free_object)(p_@›‚);
	struct _Class * (*get_parent)(struct _Class *);
	void (*set_parent)(struct _Class *, struct _Class *);
	const char * (*get_class_name)(struct _Class *);
	void (*set_class_name)(struct _Class *, const char *);
	uint_t  (*get_flags)(struct _Class *);
	void (*set_flags)(struct _Class *, uint_t );
	uint_t  (*get_object_size)(struct _Class *);
	void (*set_object_size)(struct _Class *, uint_t );
	uint_t  (*get_member_count)(struct _Class *);
	void (*set_member_count)(struct _Class *, uint_t );
	const char * (*get_member_types)(struct _Class *);
	void (*set_member_types)(struct _Class *, const char *);
	const char ** (*get_member_names)(struct _Class *);
	void (*set_member_names)(struct _Class *, const char **);
	Method ** (*get_members)(struct _Class *);
	void (*set_members)(struct _Class *, Method **);
} *Class;

extern Class Class_cl;

#endif
