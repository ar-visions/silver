#ifndef _MODULE_LOADER_H_
#define _MODULE_LOADER_H_

#include "base.h"

ModuleLoadMethod *ml_list;
int ml_count;
int ml_alloc_size;

void module_loader_continue(ModuleLoadMethod ml_add) {
    if (ml_add) {
        ModuleLoadMethod *ml_prev = ml_list;
        if (ml_alloc_size == ml_count) {
            ml_alloc_size = max(16, ml_count << 1);
            ml_list = (ModuleLoadMethod *)malloc(sizeof(ModuleLoadMethod) * ml_alloc_size);
            if (ml_count > 0) {
                memcpy(ml_list, ml_prev, sizeof(ModuleLoadMethod) * ml_count);
                free(ml_prev);
            }
        }
        ml_list[ml_count++] = ml_add;
    }
    bool single_fail = false;
    bool single_success = false;
    int left = ml_count;
    do {
        single_success = false;
        single_fail = false;
        for (int i = 0; i < ml_count; i++) {
            ModuleLoadMethod ml = ml_list[i];
            if (!ml)
                continue;
            bool success = ml();
            if (success) {
                single_success = true;
                ml_list[i] = null;
                left--;
            } else
                single_fail = true;
        }
    } while (single_success && left > 0);

    if (left > 0) {
        int cur = 0;
        for (int i = 0; i < ml_count; i++) {
            ModuleLoadMethod ml = ml_list[i];
            if (ml && cur != i) {
                ml_list[cur++] = ml;
            }
        }
        ml_count = left;
    }
}

#endif