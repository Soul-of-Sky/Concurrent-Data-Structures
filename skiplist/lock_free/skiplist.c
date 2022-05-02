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

struct sl* sl_init(int max_levels) {
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
                curr = GET_NODE(pred->next[i]);
                succ = GET_NODE(curr->next[i]);

                //TODO: free node
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
    int node_levels;
    int i;
    
    node_levels = get_rand_levels(sl);

retry:
    if (find(sl, k, preds, succs)) {
        return -EEXIST;
    } else {
        node = alloc_node(node_levels, k, v);
        for (i = 0; i < node_levels; i++) {
            node->next[i] = succs[i];
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
                find(sl, k, preds, succs);
                goto retry2;
            }
        }

        return 0;
    }
    
}

int sl_lookup(struct sl* sl, ukey_t k, uval_t* v) {
    struct sl_node *pred, *curr, *succ;
    int i;

    pred = sl->head;
    for (i = sl->max_levels - 1; i >= 0; i--) {
        curr = GET_NODE(pred->next[i]);
        while(1) {
            succ = GET_NODE(curr->next[i]);
            while(IS_MARKED(curr, i)) {
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
    }

    return k_cmp(curr->e.k, k) == 0 ? 0 : -ENOENT; 
}

int sl_remove(struct sl* sl, ukey_t k) {
   const int max_levels = sl->max_levels;
    struct sl_node* preds[max_levels];
    struct sl_node* succs[max_levels];
    struct sl_node *pred, *succ, *node;
    int i;

retry:
    if (!find(sl, k, preds, succs)) {
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
                find(sl, k, preds, succs);
                return 0;
            }
        }
        return -ENOENT;
    }
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
        if (!IS_MARKED(pred, 0)) {
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
