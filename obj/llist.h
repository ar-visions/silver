#ifndef _LLIST_
#define _LLIST_

#include <stdio.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <string.h>
#include <stdlib.h>

typedef struct _LItem {
	struct _LItem *next, *prev;
	struct _LBlock *block;
	void *data;
	char data_bytes[1];
} LItem;

typedef struct _LBlock {
	struct _LBlock *next, *prev;
	LItem *first, *last;
	int count;
	char *space;
	int struct_size;
	int available;
	int floating;
	int size;
	char data[1];
} LBlock;

typedef struct _LList {
	LItem *first;
	LItem *last;
	int count;
	LBlock *first_block;
	LBlock *last_block;
	int block_count;
	int block_size;
	int item_size;
} LList;

typedef int (*SortMethod)(void *, void *);

void *llist_push(LList *list, void *data);
void *llist_pop(LList *list);
void *llist_first(LList *list);
void *llist_last(LList *list);
void  llist(LList *list, int item_size, int block_size);
void  llist_clear(LList *list, bool free_data);
void *llist_new_data(LList *list);
bool  llist_remove_data(LList *list, void *data);
void  llist_sort(LList *list, bool asc, SortMethod sortf);
int   llist_index_of_data(LList *list, void *data);
void  llist_add(LList *list, LItem *item);
void  llist_remove(LList *list, LItem *item);

#ifndef typeof
#ifdef _MSC_VER
#define typeof decltype
#else
#define typeof __typeof__
#endif
#endif

#define llist_each(list, ptr) \
	ptr = (list)->first ? (typeof(ptr))(list)->first->data : NULL; \
	if (ptr) for (LItem *_i = (list)->first; _i; _i = _i->next, ptr = _i ? (typeof(ptr))_i->data : NULL)

#endif