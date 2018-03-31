#ifndef __SUPERMOD_H__
#define __SUPERMOD_H__
#include <stdlib.h>

typedef struct _class_Super {
	struct _class_Super *parent;
	const char *class_name;
	unsigned int flags;
	int obj_size;
	void (*_init)(struct _Base *);	int *mcount;
	unsigned char *mtypes;
	const char **mnames;
	Method **m[1];
	void  (*super_only)(struct _Super  *, int );
	void  (*method)(struct _Super  *, int , int );
} class_Super;

extern class_Super Super_cl;


typedef struct _Super {
	struct _class_Super *cl;
	LItem *ar_node;
	int refs;
} Super;

#endif
