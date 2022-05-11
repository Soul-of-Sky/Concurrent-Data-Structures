#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#include "atomic.h"
#include "skiplist.h"

static struct sl_node* alloc_node(int levels, ukey_t k, uval_t v) {
    unsigned int size = sizeof(struct sl_node) + levels * sizeof(markable_t);
    struct sl_node* node = aligned_alloc(8, size);

    node->e.k = k;
    node->e.v = v;
    node->levels = levels;
    spin_lock_init(&node->lock);
    memset(node->next, 0, sizeof(markable_t) * levels);

    return node;
}

struct sl* sl_create(int max_levels) {
    assert(max_levels > 0);
    srand(time(0));

    struct sl* sl = (struct sl*) malloc(sizeof(struct sl));
    int i;

    sl->max_levels = max_levels;
    sl->levels = 1;

    sl->head = alloc_node(max_levels, 0, NULL);
    sl->tail = alloc_node(max_levels, UINT64_MAX, NULL);

    for (i = 0; i < max_levels; i++) {
        sl->head->next[i] = sl->tail;
    }

    GET_SIGN(sl->head) = FULLY_LINK(sl->head);
    GET_SIGN(sl->tail) = FULLY_LINK(sl->tail);

    return sl;
}

static void free_node(struct sl_node* node) {
    free(node);
}

void sl_destroy(struct sl* sl) {
    struct sl_node *pred, *curr;
    
    pred = sl->head;
    while(pred) {
        curr = GET_NODE(pred->next[0]);
        free_node(pred);
        pred = curr;
    }

    free(sl);
}

static int find(struct sl* sl, ukey_t k, struct sl_node** preds, struct sl_node** succs) {
    struct sl_node *pred, *curr;
    int found_level, i;

    pred = sl->head;
    found_level = -1;
    for (i = sl->max_levels - 1; i >= 0; i--) {
        curr = GET_NODE(pred->next[i]);
        while(k_cmp(curr->e.k, k) < 0) {
            pred = curr;
            curr = GET_NODE(pred->next[i]);
        }
        if (k_cmp(curr->e.k, k) == 0 && found_level == -1) {
            found_level = i;
        }
        preds[i] = pred;
        succs[i] = curr;
    }

    return found_level;
}

static int get_rand_levels(struct sl* sl) {
    int levels = 1;

    while(rand() & 1) {
        levels++;
    }
    
    if (levels > sl->max_levels) {
        levels = sl->max_levels;
    }

    if (levels > sl->levels) {
        levels = xadd(&sl->levels, 1);
    }

    return levels;
}

int sl_insert(struct sl* sl, ukey_t k, uval_t v) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *pred, *succ, *node;
    int found_level, locked_level, all_locked;
    int node_levels;
    int i;
    
    node_levels = get_rand_levels(sl);

retry:
    found_level = find(sl, k, preds, succs);
    if (found_level != -1) {
        if (IS_MARKED(succs[found_level])) {
            goto retry;
        } else {
            while(!IS_FULLY_LINKED(succs[found_level]));
            return -EEXIST;
        }
    }

    locked_level = -1;
    all_locked = 1;
    for (i = 0; i < node_levels; i++) {
        pred = preds[i];
        succ = succs[i];
        if (i == 0 || preds[i - 1] != pred) {
            spin_lock(&pred->lock);
        }
        locked_level = i;
        if (IS_MARKED(pred) || IS_MARKED(succ) || GET_NODE(pred->next[i]) != succ) {
            all_locked = 0;
            break;
        }
    }
    
    if (!all_locked) {
        for (i = 0; i <= locked_level; i++) {
            pred = preds[i];
            if (i == 0 || preds[i - 1] != pred) {
                spin_unlock(&pred->lock);
            }
        }
        goto retry;
    }

    node = alloc_node(node_levels, k, v);
    
    node->next[0] = succs[0];
    preds[0]->next[0] = FULLY_LINK2(node);
    for (i = 1; i < node_levels; i++) {
        node->next[i] = succs[i];
        preds[i]->next[i] = node;
    }
    GET_SIGN(node) = FULLY_LINK(node);

    for (i = 0; i <= locked_level; i++) {
        pred = preds[i];
        if (i == 0 || preds[i - 1] != pred) {
            spin_unlock(&pred->lock);
        }
    }
    return 0;
}

int sl_lookup(struct sl* sl, ukey_t k, uval_t* v) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node* node;
    int found_level;
    
    found_level = find(sl, k, preds, succs);
    node = succs[found_level];

    if (found_level != -1 && IS_FULLY_LINKED(node) && !IS_MARKED(node)) {
        *v = node->e.v;
        return 0;
    }

    *v = 0;
    return -ENOENT;
}

int sl_remove(struct sl* sl, ukey_t k) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *pred, *succ, *node;
    int found_level, locked_level, node_levels;
    int is_marked = 0, all_locked, i;

retry:
    found_level = find(sl, k, preds, succs);
    node = succs[found_level];
    if (!is_marked && 
        !(found_level != -1 && IS_FULLY_LINKED(node) && !IS_MARKED(node))) {
        return -ENOENT;    
    }
    if (!is_marked) {
        spin_lock(&node->lock);
        if (IS_MARKED(node)) {
            spin_unlock(&node->lock);
            return -ENOENT;
        }
        node_levels = node->levels;
        GET_SIGN(node) = MARK_NODE(node);
        is_marked = 1;
    }

    locked_level = -1;
    all_locked = 1;
    for (i = 0; i < node_levels; i++) {
        pred = preds[i];
        succ = succs[i];
        if (i == 0 || preds[i - 1] != pred) {
            spin_lock(&pred->lock);
        }
        locked_level = i;
        if (IS_MARKED(pred) || GET_NODE(pred->next[i]) != succ) {
            all_locked = 0;
            break;
        }
    }
    
    if (!all_locked) {
        spin_unlock(&node->lock);
        for (i = 0; i <= locked_level; i++) {
            pred = preds[i];
            if (i == 0 || preds[i - 1] != pred) {
                spin_unlock(&pred->lock);
            }
        }
        goto retry;
    }

    preds[0]->next[0] = FULLY_LINK2(GET_NODE(node->next[0]));
    for (i = 1; i < node_levels; i++) {
        preds[i]->next[i] = node->next[i];
    }

    spin_unlock(&node->lock);
    for (i = 0; i <= locked_level; i++) {
        pred = preds[i];
        if (i == 0 || preds[i - 1] != pred) {
            spin_unlock(&pred->lock);
        }
    }

    return 0;
}

int sl_range(struct sl* sl, ukey_t k, unsigned int len, uval_t* v_arr) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *curr, *pred;
    int cnt = 0;

    find(sl, k, preds, succs);
    pred = succs[0];

    while(pred != sl->tail) {
        curr = GET_NODE(pred->next[0]);
        if (IS_FULLY_LINKED(pred) && !IS_MARKED(pred)) {
            v_arr[cnt++] = pred->e.v;
        }
        pred = curr;
    }

    return cnt;
}

void sl_print(struct sl* sl) {
    struct sl_node* node;
    int i;

    for (i = sl->levels; i >= 0; i--) {
        printf("level [%d]: ", i);
        node = GET_NODE(sl->head->next[i]);
        while(node != sl->tail) {
            printf("<%ld %ld> \n", node->e.k, node->e.v);
            node = GET_NODE(node->next[i]);
        }
        printf("\n");
    }
}
