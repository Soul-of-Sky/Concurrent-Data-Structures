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
    memset(node->next, 0, sizeof(markable_t) * levels);

    return node;
}

static void free_node(struct sl_node* node) {
    free(node);
}

struct sl* sl_create(int max_levels) {
    assert(max_levels > 0);
    srand(time(0));

    struct sl* sl = (struct sl*) malloc(sizeof(struct sl));
    int i;

    sl->max_levels = max_levels;
    sl->levels = 1;

    sl->head = alloc_node(max_levels, 0, 0);
    sl->tail = alloc_node(max_levels, UINT64_MAX, 0);

    for (i = 0; i < max_levels; i++) {
        sl->head->next[i] = (markable_t) sl->tail;
    }

    sl->ebr = ebr_create((free_fun_t) free_node);

    return sl;
}

void sl_destroy(struct sl* sl) {
    struct sl_node *pred, *curr;
    
    pred = sl->head;
    while(pred) {
        curr = GET_NODE(pred->next[0]);
        free_node(pred);
        pred = curr;
    }
    
    ebr_destroy(sl->ebr);

    free(sl);
}

static int find(struct sl* sl, ukey_t k, struct sl_node** preds, struct sl_node** succs, int tid) {
    struct sl_node *pred, *curr, *succ;
    int i;

retry:
    pred = sl->head;
    for (i = sl->max_levels - 1; i >= 0; i--) {
        curr = GET_NODE(pred->next[i]);
        while(1) {
            succ = GET_NODE(curr->next[i]);
            while(IS_MARKED(curr, i)) {
                if (!cmpxchg2(&pred->next[i], curr, succ)) {
                    goto retry;
                }
                if (i == 0) {
                    ebr_put(sl->ebr, curr, tid);
                }

                curr = GET_NODE(pred->next[i]);
                succ = GET_NODE(curr->next[i]);
            }
            if (k_cmp(curr->e.k, k) < 0) {
                pred = curr;
                curr = succ;
            } else {
                break;
            }
        }
        preds[i] = pred;
        succs[i] = curr;
    }

    return !k_cmp(curr->e.k, k);
}

static int get_rand_levels(struct sl* sl) {
    int levels = 1, old;

    while(rand() & 1) {
        levels++;
    }
    
    if (levels > sl->max_levels) {
        levels = sl->max_levels;
    }

    old = ACCESS_ONCE(sl->levels);
    if (levels > old) {
        cmpxchg(&sl->levels, old, old + 1);
        levels = old + 1;
    }

    return levels;
}

int sl_insert(struct sl* sl, ukey_t k, uval_t v, int tid) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *pred, *succ, *node;
    int node_levels;
    int i;

    ebr_enter(sl->ebr, tid);

    node_levels = get_rand_levels(sl);

retry:
    if (find(sl, k, preds, succs, tid)) {
        ebr_exit(sl->ebr, tid);
        return -EEXIST;
    } else {
        node = alloc_node(node_levels, k, v);
        for (i = 0; i < node_levels; i++) {
            node->next[i] = (markable_t) succs[i];
        }
        if (!(cmpxchg2(&preds[0]->next[0], succs[0], node))) {
            free_node(node);
            goto retry;
        }
        for (i = 1; i < node_levels; i++) {
retry2:
            pred = preds[i];
            succ = succs[i];
            if (!(cmpxchg2(&pred->next[i], succ, node))) {
                find(sl, k, preds, succs, tid);
                goto retry2;
            }
        }
        ebr_exit(sl->ebr, tid);
        return 0;
    }
    
}

int sl_lookup(struct sl* sl, ukey_t k, uval_t* v, int tid) {
    struct sl_node *pred, *curr, *succ;
    int ret, i;

    ebr_enter(sl->ebr, tid);

    pred = sl->head;
    for (i = sl->max_levels - 1; i >= 0; i--) {
        curr = GET_NODE(pred->next[i]);
        while(1) {
            succ = GET_NODE(curr->next[i]);
            while(IS_MARKED(curr, i)) {
                curr = curr;
                succ = GET_NODE(curr->next[i]);
            }
            if (k_cmp(curr->e.k, k) < 0) {
                pred = curr;
                curr = succ;
            } else {
                break;
            }
        }
    }

    *v = curr->e.v;
    ret = k_cmp(curr->e.k, k) == 0 ? 0 : -ENOENT; 
    
    ebr_exit(sl->ebr, tid);

    return ret;
}

int sl_remove(struct sl* sl, ukey_t k, int tid) {
   const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *pred, *succ, *node;
    int i;

    ebr_enter(sl->ebr, tid);

retry:
    if (!find(sl, k, preds, succs, tid)) {
        ebr_exit(sl->ebr, tid);
        return -ENOENT;
    } else {
        node = succs[0];
        for (i = node->levels - 1; i > 0; i--) {
            while(!IS_MARKED(node, i)) {
                succ = GET_NODE(node->next[i]);
                cmpxchg2(&node->next[i], succ, MARK_NODE2(succ));
            }
        }
        while(!IS_MARKED(node, 0)) {
            succ = GET_NODE(node->next[0]);
            if (cmpxchg2(&node->next[0], succ, MARK_NODE2(succ))) {
                /* try to remove physically */
                find(sl, k, preds, succs, tid);
                ebr_exit(sl->ebr, tid);
                return 0;
            }
        }
        ebr_exit(sl->ebr, tid);
        return -ENOENT;
    }
}

int sl_range(struct sl* sl, ukey_t k, unsigned int len, uval_t* v_arr, int tid) {
    const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *curr, *pred;
    int cnt = 0;

    ebr_enter(sl->ebr, tid);

    find(sl, k, preds, succs, tid);
    pred = succs[0];

    while(pred != sl->tail) {
        curr = GET_NODE(pred->next[0]);
        if (!IS_MARKED(pred, 0)) {
            v_arr[cnt++] = pred->e.v;
        }
        pred = curr;
    }

    ebr_exit(sl->ebr, tid);

    return cnt;
}

void sl_print(struct sl* sl) {
    struct sl_node* node;
    int i;

    for (i = ACCESS_ONCE(sl->levels); i >= 0; i--) {
        printf("level [%d]: ", i);
        node = GET_NODE(sl->head->next[i]);
        while(node != sl->tail) {
            printf("<%ld %ld> \n", node->e.k, node->e.v);
            node = GET_NODE(node->next[i]);
        }
        printf("\n");
    }
}
