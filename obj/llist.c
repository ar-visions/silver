#include <obj/llist.h>

static inline LBlock *llist_block_init(LList *list) {
	int struct_size = sizeof(LItem) + list->item_size;
	int size = sizeof(LBlock) + struct_size * list->block_size;
	LBlock *b = (LBlock *)malloc(size);
	b->first = b->last = NULL;
	b->space = b->data;
	b->struct_size = struct_size;
	b->available = list->block_size;
	b->size = list->block_size;
	b->floating = 0;
	b->count = 0;
	llist_add((LList *)&list->first_block, (LItem *)b);
	return b;
}

static inline LBlock *llist_block_find(LList *list) {
	for (LBlock *b = list->last_block; b; b = b->prev)
		if ((b->count + b->available) > 0)
			return b;
	return NULL;
}

void llist_add(LList *list, LItem *item) {
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

static inline void llist_block_remove(LList *list, LItem *item) {
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

void llist_remove(LList *list, LItem *item) {
	if (item->prev)
		item->prev->next = item->next;
	else
		list->first = item->next;
	if (item->next)
		item->next->prev = item->prev;
	else
		list->last = item->prev;
	list->count--;
	
	LBlock *b = item->block; 
	// add item back to block
	llist_add((LList *)&b->first, (LItem *)item);
	b->floating--;
	if (((b->available + b->floating) == 0 || ((b->count + b->available) == b->size))) {
		if (list->block_count > 1) {
			llist_block_remove((LList *)&list->first_block, (LItem *)b);
			free(b);
		}
	}
}

static inline LItem *llist_init_item(LList *list) {
	LBlock *b = llist_block_find(list);
	if (!b)
		b = llist_block_init(list);
	LItem *item;
	if (b->available) {
		item = (LItem *)b->space;
		b->space += b->struct_size;
		b->available--;
		item->block = b;
	} else {
		item = b->last;
		if (b->last != b->first) {
			b->last = b->last->prev;
			b->last->next = NULL;
		} else
			b->first = b->last = NULL;
		b->count--;
	}
	b->floating++;
	return item;
}

void *llist_new_data(LList *list) {
	LItem *item = llist_init_item(list);
	if (!item)
		return NULL;
	llist_add(list, item);
	return &item->data;
}

bool llist_remove_data(LList *list, void *data) {
	if (data == NULL)
		return false;
	LItem *item = NULL;
	if (list->item_size)
		item = &(((LItem *)data)[-4]);
	else
		for (LItem *i = list->first; i; i = i->next)
			if (i->data == data) {
				item = i;
				break;
			}
	if (item) {
        llist_remove(list, item);
        return true;
    }
    return false;
}

void *llist_push(LList *list, void *data) {
	LItem *item = llist_init_item(list);
	item->next = item->prev = NULL;
	void *ret = (void *)item;
	if (list->item_size) {
		if (data) {
			memcpy(item->data_bytes, data, list->item_size);
		} else {
			memset(item->data_bytes, 0, list->item_size);
		}
		item->data = item->data_bytes;
		ret = item->data_bytes;
	}
	else
        item->data = data;
	llist_add(list, item);
	return ret;
}

void *llist_pop(LList *list) {
	if (list->last) {
		LItem *last = list->last;
		LBlock *b = last->block;
		void *data = last->data;
		llist_remove(list, last);
		return data;
	}
	return NULL;
}

int llist_index_of_data(LList *list, void *data) {
	int index = 0;
	void *d = NULL;
    llist_each(list, d) {
        if (d == data)
            return index;
        index++;
    }
    return -1;
}

void *llist_first(LList *list) {
	if (list->first)
		return list->first->data;
	return NULL;
}

void *llist_last(LList *list) {
	if (list->last)
		return list->last->data;
	return NULL;
}

static LItem *llist_split(LItem *head) {
	LItem *fast = head, *slow = head;
	while (fast->next && fast->next->next) {
		fast = fast->next->next;
		slow = slow->next;
	}
	LItem *temp = slow->next;
	slow->next = NULL;
	return temp;
}

static LItem *llist_merge(LList *list, LItem *first, LItem *second, bool asc, SortMethod sortf) {
    if (!first)
        return second;
    if (!second)
        return first;
 
    if (asc ^ (sortf(first->data, second->data) > 0)) {
        first->next = llist_merge(list, first->next, second, asc, sortf);
        first->next->prev = first;
        first->prev = NULL;
        return first;
    } else {
        second->next = llist_merge(list, first, second->next, asc, sortf);
        second->next->prev = second;
        second->prev = NULL;
        return second;
    }
}

static LItem *llist_merge_sort(LList *list, LItem *first, bool asc, SortMethod sortf) {
    if (!first || !first->next)
        return first;
	LItem *second = llist_split(first);
    first = llist_merge_sort(list, first, asc, sortf);
    second = llist_merge_sort(list, second, asc, sortf);
    return llist_merge(list, first, second, asc, sortf);
}

void llist_sort(LList *list, bool asc, SortMethod sortf) {
	LItem *first = llist_merge_sort(list, list->first, asc, sortf);
	LItem *last = first;
	for (LItem *f = first; f; f = f->next)
		last = f;
	list->first = first;
	list->last = last;
}

void llist(LList *list, int item_size, int block_size) {
	memset(list, 0, sizeof(LList));
	list->block_size = block_size;
	list->item_size = item_size;
}

void llist_clear(LList *list, bool free_data) {
	if (free_data && list->item_size == 0) {
		void *data;
		while ((data = llist_pop(list))) {
			if (free_data)
				free(data);
		}
	} else {
        LBlock *n = NULL;
        for (LBlock *b = list->first_block; b; b = n) {
            n = b->next;
            free(b);
        }
        llist(list, list->item_size, list->block_size);
	}
}