#ifndef _LLIST_
#define _LLIST_

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>

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

typedef int (*LSort)(void *, void *);

void *llist_push(LList *list, void *data);
void *llist_pop(LList *list);
void *llist_first(LList *list);
void *llist_last(LList *list);
void  llist(LList *list, int item_size, int block_size);
void  llist_clear(LList *list, bool free_data);
void *llist_new_data(LList *list);
bool  llist_remove_data(LList *list, void *data);
void  llist_sort(LList *list, bool asc, LSort sortf);
int   llist_index_of_data(LList *list, void *data);

static inline void llist_add(LList *list, LItem *item) {
	if (!list->last) {
		list->first = list->last = item;
		item->prev = NULL;
	} else {
		list->last->next = item;
		item->prev = list->last;
		list->last = item;
	}
	item->next = NULL;
	list->count++;
}

static inline void llist_remove(LList *list, LItem *item) {
	if (item->prev)
		item->prev->next = item->next;
	else
		list->first = item->next;
	if (item->next)
		item->next->prev = item->prev;
	else
		list->last = item->prev;
	list->count--;
}

#define ll(list, size)  	 		 ( llist((list), 0, (size)) )
#define allocator(list, type, size)	 ( llist((list), sizeof(type), (size)) )
#define allocator_new(list)			 ( llist_new_data(list) )
#define ll_push(list, data)	 		 ( llist_push((list), (void *)(data)) )
#define ll_pop(list)		 		 ( llist_pop(list) )
#define ll_first(list)		 		 ( llist_first(list) )
#define ll_last(list)		 		 ( llist_last(list) )
#define ll_remove(list, data)		 ( llist_remove_data((list), (data)) )
#define ll_clear(list, free_data)	 ( llist_clear(list, free_data) )
#define ll_sort(list, asc, f)		 ( llist_sort((list), (asc), (LSort)(f)) );
#define ll_each(list, ptr) 	 		 ptr = (list)->first ? (__typeof__(ptr))(list)->first->data : NULL; if (ptr) for (LItem *_i = (list)->first; _i; _i = _i->next, ptr = _i ? (__typeof__(ptr))_i->data : NULL)

#endif