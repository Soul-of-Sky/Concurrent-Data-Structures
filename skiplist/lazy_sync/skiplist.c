#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include "skiplist.h"

static struct sl_node* alloc_node(int levels, skey_t k, sval_t v) {
    unsigned int size = sizeof(struct sl_node) + levels * sizeof(markable_t);
    struct sl_node* node = aligned_alloc(8, size);

    node->e.k = k;
    node->e.v = v;
    spin_lock_init(&node->lock);
    memset(node->next, 0, sizeof(markable_t) * levels);

    return node;
}

struct sl* sl_init(int max_levels) {
    assert(max_levels > 0);

    struct sl* sl = (struct sl*) malloc(sizeof(struct sl));
    int i;

    sl->max_levels = max_levels;
    sl->levels = 1;

    sl->head = alloc_node(max_levels, 0, NULL);
    sl->tail = alloc_node(max_levels, UINT64_MAX, NULL);

    for (i = 0; i < max_levels; i++) {
        sl->head->next[i] = sl->tail;
    }

    sl->head->next[0] = FULLY_LINK(sl->head->next[0]);
    sl->tail->next[0] = FULLY_LINK(sl->tail->next[0]);

    return sl;
}

void sl_destroy(struct sl* sl) {
    struct sl_node* node = sl->head;
}

int sl_insert(struct sl* sl, skey_t k, sval_t v) {

}

int sl_lookup(struct sl* sl, skey_t k, sval_t* v) {

}

int sl_remove(struct sl* sl, skey_t k) {

}

int sl_range(struct sl* sl, skey_t k, unsigned int len, sval_t* v_arr) {

}

void sl_range(struct sl* sl) {

}
