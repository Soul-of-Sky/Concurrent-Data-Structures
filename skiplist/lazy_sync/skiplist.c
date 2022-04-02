#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include "skiplist.h"

static struct sl_node* alloc_node(int levels, ukey_t k, uval_t v) {
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

static void free_node(struct sl_node* node) {
    free(node);
}

void sl_destroy(struct sl* sl) {
    struct sl_node *pred, *curr;
    
    pred = sl->head;
    while(pred) {
        curr = GET_NODE(pred->next);
        free_node(pred);
        pred = curr;
    }

    free(sl);
}

static int find(struct sl* sl, struct sl_node* preds, struct sl_node* succs) {
    struct sl_node *pred, *curr;
    int i;

    pred = sl->head;
    for (i = sl->max_levels - 1; i >= 0; i--) {
        curr = GET_NODE(pred->next);
        while()
    }
}

int sl_insert(struct sl* sl, ukey_t k, uval_t v) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];

retry:
    
}

int sl_lookup(struct sl* sl, ukey_t k, uval_t* v) {

}

int sl_remove(struct sl* sl, ukey_t k) {

}

int sl_range(struct sl* sl, ukey_t k, unsigned int len, uval_t* v_arr) {

}

void sl_range(struct sl* sl) {

}
