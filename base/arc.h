#ifndef _BASE_ARC_H_
#define _BASE_ARC_H_

typedef struct _ARC_Item {
    struct _base_Base o;
    bool retained;
} ARC_Item;

typedef struct _ARC_List {
    ARC_Item *items;
    int alloc;
    int cursor;
} ARC_List;

static __thread ARC_List arc_list;

struct _base_Base *arc_push(struct _base_Base o, bool retain) {
    if (!o)
        return NULL;
    if (arc_list->alloc >= arc_list->count) {
        ARC_Item *prev = arc_list->items;
        arc_list->alloc <<= 1;
        if (arc_list->alloc < 512)
            arc_list->alloc = 512;
        arc_list->items = (ARC_Item *)malloc(sizeof(ARC_Item *) * arc_list->alloc);
        if (prev)
            memcpy(arc_list->items, prev, sizeof(ARC_Item *) * arc_list->count);
    }
    ARC_Item *item = &arc_list->items[arc_list->count++];
    item->o = o;
    item->retained = retain;
    if (retained)
        o->refs++;
    return o;
}

void Base_free_object(struct _base_Base *);

void arc_release(int from) {
    for (int i = from; i < arc_list->count; ++i) {
        ARC_Item *item = &arc_list->items[i];
        if (item->retained)
            item->o->refs--;
        if (item->o->refs <= 0)
            Base_free_object(item->o);
    }
    arc_list->count = from;
}

struct _base_Base *arc_update(struct _base_Base **ptr, struct _base_Base *value) {
    struct _base_Base *before = *ptr;
    *ptr = value;
    if (value)
        ++value->refs;
    if (before && --before->refs <= 0)
        Base_free_object(item->o);
    return value;
}

#endif