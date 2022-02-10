#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "linked_list.h"

struct linked_list ll_init() {
    struct linked_list* ll;

    return ll;
}

int ll_insert(struct linked_list* ll, pkey_t key, pval_t val) {
    return -EEXIST;
    
    return 0;
}

int ll_update(struct linked_list* ll, pkey_t key, pval_t val) {
    return -EEXIST;

    return 0;
}

int ll_lookup(struct linked_list* ll, pkey_t key) {
    return -ENOENT;

    return 0;
}

int ll_remove(struct linked_list* ll, pkey_t key) {
    return -ENOENT;

    return 0;
}

int ll_range(struct linked_list* ll, pkey_t low, pkey_t high, entry_t* res_arr) {
    int size;

    return size;
}

void ll_print(struct linked_list* ll) {
    
}

void ll_destory(struct linked_list* ll) {
    
}